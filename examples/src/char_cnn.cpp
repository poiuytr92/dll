//=======================================================================
// Copyright (c) 2014-2017 Baptiste Wicht
// Distributed under the terms of the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#include <unordered_map>

#include "cpp_utils/tuple_utils.hpp"

#include "dll/neural/embedding_layer.hpp"
#include "dll/neural/conv_layer.hpp"
#include "dll/pooling/mp_layer.hpp"
#include "dll/neural/dense_layer.hpp"
#include "dll/utility/group_layer.hpp"
#include "dll/utility/merge_layer.hpp"
#include "dll/network.hpp"
#include "dll/datasets.hpp"

void generate(std::vector<std::string>& words, std::vector<size_t>& labels, const std::string& base_word, size_t label){
    std::random_device rd;
    std::mt19937_64 engine(rd());

    std::uniform_int_distribution<int> letters_dist(0, 25);

    for(size_t i = 0; i < 250; ++i){
        std::string word = base_word;

        for(size_t i = 0; i < 10; ++i){
            word += ('A' + letters_dist(engine));
        }

        words.emplace_back(std::move(word));

        labels.push_back(label);
    }
}

int main(int /*argc*/, char* /*argv*/ []) {
    std::vector<std::string> words;
    std::vector<size_t> labels;

    generate(words, labels, "ZEROX", 0);
    generate(words, labels, "XONEX", 1);
    generate(words, labels, "XTWOX", 2);
    generate(words, labels, "THREE", 3);
    generate(words, labels, "FOURX", 4);

    std::random_device rd;
    std::mt19937_64 engine(rd());
    cpp::parallel_shuffle(words.begin(), words.end(), labels.begin(), labels.end(), engine);

    std::vector<etl::fast_dyn_matrix<float, 15>> samples;
    std::unordered_map<char, size_t> chars;

    for (auto& word : words) {
        samples.emplace_back();

        for (size_t i = 0; i < word.size(); ++i) {
            auto c = word[i];

            if (!chars.count(c)) {
                auto s = chars.size();
                // Done before because operator[] will create a new value
                chars[c] = s;
            }

            samples.back()[i] = chars[c];
        }
    }

    constexpr size_t embedding = 16;
    constexpr size_t length = 15;

    using embedding_network_t = dll::network_desc<
        dll::network_layers<
            // The embedding layer
            dll::embedding_layer<26, length, embedding>

            // The convolutional layers
            , dll::merge_layer<
                0
                , dll::group_layer<
                      dll::conv_layer<1, length, embedding, 16, 3, embedding>
                    , dll::mp_2d_layer<16, length - 3 + 1, 1, length - 3 + 1, 1>
                >
                , dll::group_layer<
                      dll::conv_layer<1, length, embedding, 16, 4, embedding>
                    , dll::mp_2d_layer<16, length - 4 + 1, 1, length - 4 + 1, 1>
                >
                , dll::group_layer<
                      dll::conv_layer<1, length, embedding, 16, 5, embedding>
                    , dll::mp_2d_layer<16, length - 5 + 1, 1, length - 5 + 1, 1>
                >
            >

            // The final softmax layer
            , dll::dense_layer<48, 10, dll::softmax>
        >
        , dll::updater<dll::updater_type::NADAM>     // Nesterov Adam (NADAM)
        , dll::batch_size<50>                        // The mini-batch size
        , dll::shuffle                               // Shuffle before each epoch
    >::network_t;

    auto net = std::make_unique<embedding_network_t>();

    // Display the network and dataset
    net->display();

    // Train the network for performance sake
    net->fine_tune(samples, labels, 50);

    // Test the network on train set
    net->evaluate(samples, labels);


    return 0;
}