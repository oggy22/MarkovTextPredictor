#include <iostream>
#include <fstream>
#include <string>
#include <optional>
#include <vector>
#include <unordered_map>
#include <random>
#include <future>

// Random generator variables
std::random_device rd;  // Non-deterministic seed
std::mt19937 gen(rd()); // Mersenne Twister engine

// For atomic console output
static std::mutex coutMutex;
void printAtomicPredictorStart(int size) {
    std::lock_guard<std::mutex> lock(coutMutex);
    std::cout << "Predictor(size=" << size << "): Initializing..." << std::endl;
}
void printAtomicPredictorDone(int size) {
    std::lock_guard<std::mutex> lock(coutMutex);
    std::cout << "Predictor(size=" << size << "): Done." << std::endl;
}

class Predictor {
	std::unordered_map<std::string, std::vector<char>> model_;
	int size_;
	int hits_ = 0;
public:
    Predictor(const std::string& text, int size) : size_(size), hits_(0) {
        auto buildModel = [this, &text, size]() {
            printAtomicPredictorStart(size);
            for (size_t i = 0; i + size < text.size(); ++i) {
                std::string key = text.substr(i, size);
                char next_char = text[i + size];
                this->model_[key].push_back(next_char);
            }
			printAtomicPredictorDone(size);
            };
        fut = std::async(std::launch::async, buildModel);
	}

    std::future<void> fut;

    std::optional<char> predictNextChar(const std::string& context) {
		fut.wait(); // Ensure the model is built

		// If the context is shorter than the model size, return nullopt
        if (size_ > context.size()) return std::nullopt;

		// Find the context in the model
		std::string key = context.substr(context.size() - size_);
		auto it = model_.find(key);
        if (it == model_.end())
            return std::nullopt;
	
		// Get a random character from the vector
        std::uniform_int_distribution<size_t> dist(0, it->second.size()-1);
        size_t index = dist(gen);
		hits_++;
		return it->second[index];
	}

	int getHits() const { return hits_; }
};

class MarkovTextPredictor {
	int max_length_;
	std::vector<Predictor> predictors_; // Assuming Predictor is a defined class
public:
    MarkovTextPredictor(const std::string& text, int max_length=15)
    {
		predictors_.reserve(max_length + 1);
        for (int size = 0; size <= max_length; ++size) {
            // Initialize each predictor with the text and size
            predictors_.emplace_back(text, size);
		}

		// Wait for all predictors to finish building their models  
        for (auto& predictor : predictors_) {
            predictor.fut.wait();
		}
    }

    void printStats() const
    {
        for (int size = 0; size < predictors_.size(); ++size) {
			std::cout << "Predictor size " << size << " hits: " << predictors_[size].getHits() << std::endl;
        }
    }

    char predictNextChar(const std::string& context)
    {
		// Iterate through predictors from largest to smallest
        for (int size = predictors_.size()-1; size >= 0; --size) {
            auto prediction = predictors_[size].predictNextChar(context);
            if (prediction.has_value()) {
                return prediction.value();
            }
		}

		// This should never happen since size 0 predictor always returns a character
        return 'a';
	}
};

int main(int argc, char *argv[]) // Fix parameter names and types
{
    if (argc < 2) {
        std::cout << "Usage: MarkovTextPredictor <input_file>\n";
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file) { // Check if file opened successfully
        std::cout << "Error: Could not open file " << argv[1] << "\n";
        return 1;
    }

    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	MarkovTextPredictor predictor(text);

	std::string prompt = argc > 2 ? argv[2] : "";

	std::string output = prompt;
    for (int i= 0; i < 1000; ++i) {
		output += predictor.predictNextChar(output);
	}

    std::replace(output.begin(), output.end(), '\n', ' ');

	std::cout << output << std::endl;

	predictor.printStats();
    return 0;
}