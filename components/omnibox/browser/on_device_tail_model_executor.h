// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_TAIL_MODEL_EXECUTOR_H_
#define COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_TAIL_MODEL_EXECUTOR_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/on_device_tail_tokenizer.h"
#include "third_party/tflite/src/tensorflow/lite/interpreter.h"
#include "third_party/tflite/src/tensorflow/lite/signature_runner.h"

// The on device tail model executor implements a beam search algorithm
// (https://en.wikipedia.org/wiki/Beam_search) to generate complete suggestions
// for the given prefix.
// At each search step, the executor feeds the token and cell states from the
// previous step into the model to generate the predictions for the next token.
// TODO(crbug.com/1372112): migrate to optimization_guide::TFLiteModelExecutor
// once it supports multi-subgraph model.
class OnDeviceTailModelExecutor {
 public:
  // The struct holds the prediction made by the model and its probability.
  struct Prediction {
    std::string suggestion;
    float probability;
  };

  OnDeviceTailModelExecutor();
  ~OnDeviceTailModelExecutor();

  // Initializes the model executor.
  bool Init(const base::FilePath& model_filepath,
            const base::FilePath& vocab_filepath,
            size_t state_size,
            size_t num_layer,
            size_t embedding_dimension);

  // Returns whether the executor is initialized.
  bool IsReady() const { return interpreter_ != nullptr; }

  // Resets the model executor.
  void Reset();

  // Returns at most `max_num_suggestions` suggestions and their probabilities,
  // with minimum probability `probability_threshold` for the given `prefix` and
  // `previous_query`. The given prefix will only be extended at most
  // `max_rnn_steps` times.
  // TODO(crbug/1372112): implement this function.
  std::vector<Prediction> GenerateSuggestionsForPrefix(
      const std::string& prefix,
      const std::string& previous_query,
      size_t max_num_suggestions,
      size_t max_rnn_steps,
      float probability_threshold);

 private:
  friend class OnDeviceTailModelExecutorTest;
  FRIEND_TEST_ALL_PREFIXES(OnDeviceTailModelExecutorTest,
                           TestEncodePreviousQuery);

  struct RnnCellStates {
    RnnCellStates();
    RnnCellStates(size_t num_layer, size_t state_size);
    RnnCellStates(const RnnCellStates& other);
    ~RnnCellStates();

    bool operator==(const RnnCellStates& other) const {
      return c_i == other.c_i && m_i == other.m_i;
    }
    bool operator!=(const RnnCellStates& other) const {
      return !(*this == other);
    }

    // Cell states, see definitions at
    // https://github.com/tensorflow/lingvo/blob/master/lingvo/core/rnn_cell.py#L221.
    std::vector<std::vector<float>> c_i, m_i;
  };

  // The struct which holds the output from subgraph |rnn_step_|.
  struct RnnStepOutput {
    RnnStepOutput(size_t num_layer, size_t state_size, size_t vocab_size);
    RnnStepOutput(const RnnStepOutput& other);
    ~RnnStepOutput();

    // The output RNN cell states.
    RnnCellStates states;

    // The probability vector; `probs[i]` corresponds to the probability of the
    // i-th token in the vocabulary.
    std::vector<float> probs;
  };

  // Helper function to initialize TFlite model interpreter.
  bool InitModelInterpreter(const base::FilePath& model_filepath);

  // Gets the encoding for previous query token IDs.
  bool EncodePreviousQuery(
      const OnDeviceTailTokenizer::TokenIds& prev_query_token_ids,
      std::vector<float>* prev_query_encoding);

  // Resets LRU caches.
  void ResetCaches();

  // The tokenizer and tensorflow lite model & interpreter instances.
  std::unique_ptr<OnDeviceTailTokenizer> tokenizer_;
  std::unique_ptr<base::MemoryMappedFile> model_fb_;
  std::unique_ptr<tflite::Interpreter> interpreter_;

  // The pointers to subgraphs in the model.
  raw_ptr<tflite::SignatureRunner> prev_query_encoder_;
  raw_ptr<tflite::SignatureRunner> rnn_step_;

  // We use LRU caches to keep track of most recent outputs of subgraphs, such
  // that we will not need to run the interpreter if a cache hit is found for a
  // specific input.
  base::LRUCache<OnDeviceTailTokenizer::TokenIds, std::vector<float>>
      prev_query_cache_;
  base::LRUCache<OnDeviceTailTokenizer::TokenIds, RnnStepOutput>
      rnn_step_cache_;

  // Parameters needed to run the executor.
  size_t state_size_;
  size_t num_layer_;
  size_t embedding_dimension_;
  size_t vocab_size_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_TAIL_MODEL_EXECUTOR_H_
