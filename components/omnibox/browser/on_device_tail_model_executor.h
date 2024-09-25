// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_TAIL_MODEL_EXECUTOR_H_
#define COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_TAIL_MODEL_EXECUTOR_H_

#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/omnibox/browser/on_device_tail_tokenizer.h"
#include "components/optimization_guide/proto/on_device_tail_suggest_model_metadata.pb.h"
#include "third_party/tflite/src/tensorflow/lite/interpreter.h"
#include "third_party/tflite/src/tensorflow/lite/signature_runner.h"

// The on device tail model executor implements a beam search algorithm
// (https://en.wikipedia.org/wiki/Beam_search) to generate complete suggestions
// for the given prefix.
// At each search step, the executor feeds the token and cell states from the
// previous step into the model to generate the predictions for the next token.
// TODO(crbug.com/40241602): migrate to optimization_guide::TFLiteModelExecutor
// once it supports multi-subgraph model.
class OnDeviceTailModelExecutor {
 public:
  // The struct holds the prediction made by the model and its probability.
  struct Prediction {
    std::string suggestion;
    float probability;
  };

  // The struct holds the input parameters needed to generate predictions from
  // the model.
  struct ModelInput {
    ModelInput();
    ModelInput(std::string prefix,
               std::string previous_query,
               size_t max_num_suggestions);

    std::string prefix;
    std::string previous_query;
    size_t max_num_suggestions;
  };

  using ModelMetadata =
      optimization_guide::proto::OnDeviceTailSuggestModelMetadata;

  OnDeviceTailModelExecutor();
  ~OnDeviceTailModelExecutor();

  // Initializes the model executor.
  bool Init();
  bool Init(const base::FilePath& model_filepath,
            const base::flat_set<base::FilePath>& additional_files,
            const ModelMetadata& metadata);

  // Returns whether the executor is initialized.
  bool IsReady() const { return interpreter_ != nullptr; }

  // Resets the model executor.
  void Reset();

  // Returns at most `max_num_suggestions` suggestions and their probabilities,
  // with minimum probability `probability_threshold` for the given `prefix` and
  // `previous_query`. The given prefix will only be extended at most
  // `max_rnn_steps` times.
  std::vector<Prediction> GenerateSuggestionsForPrefix(const ModelInput& input);

  // Returns the time when the executor is last called.
  base::TimeTicks GetExecutorLastCalledTime() const {
    return executor_last_called_time_;
  }

 private:
  friend class OnDeviceTailModelExecutorPublic;

  struct RnnCellStates {
    RnnCellStates();
    RnnCellStates(size_t num_layer, size_t state_size);
    RnnCellStates(const RnnCellStates& other);
    RnnCellStates(RnnCellStates&& other) noexcept;
    RnnCellStates& operator=(const RnnCellStates& other);
    RnnCellStates& operator=(RnnCellStates&& other) noexcept;
    ~RnnCellStates();

    bool operator==(const RnnCellStates& other) const {
      return c_i == other.c_i && m_i == other.m_i;
    }
    bool operator!=(const RnnCellStates& other) const {
      return !(*this == other);
    }

    // Cell states, see definitions at
    // https://github.com/tensorflow/lingvo/blob/master/lingvo/core/rnn_cell.py#L221.
    std::vector<std::vector<float>> c_i;
    std::vector<std::vector<float>> m_i;
  };

  // The struct which holds the output from subgraph `rnn_step_`.
  struct RnnStepOutput {
    RnnStepOutput();
    RnnStepOutput(size_t num_layer, size_t state_size, size_t vocab_size);
    RnnStepOutput(const RnnStepOutput& other);
    ~RnnStepOutput();

    bool operator==(const RnnStepOutput& other) const {
      return states == other.states && probs == other.probs;
    }

    bool operator!=(const RnnStepOutput& other) const {
      return !(*this == other);
    }

    // The output RNN cell states.
    RnnCellStates states;

    // The probability vector; `probs[i]` corresponds to the probability of the
    // i-th token in the vocabulary.
    std::vector<float> probs;
  };

  // The node struct which holds all information needed to run the beam search.
  struct BeamNode {
    BeamNode();
    BeamNode(int num_layer, int state_size);
    BeamNode(const BeamNode& other);
    BeamNode(BeamNode&& other) noexcept;
    BeamNode& operator=(const BeamNode& other);
    BeamNode& operator=(BeamNode&& other) noexcept;
    ~BeamNode();

    bool operator>(const BeamNode& other) const {
      return this->log_prob > other.log_prob;
    }

    // The suggestion token IDs which the beam node is representing.
    OnDeviceTailTokenizer::TokenIds token_ids;

    // The cache key for `rnn_step_cache_` which is the vector of the previous
    // query token IDs plus suggestion token IDs.
    OnDeviceTailTokenizer::TokenIds rnn_step_cache_key;

    // The prefix which has to be met in next expansion.
    std::string constraint_prefix;

    // The output RNN cell states from the last `rnn_step_` invocation.
    RnnCellStates states;

    // The accumulated log probability for the node.
    float log_prob = 0.0;
  };

  // A min priority queue to store beam nodes such that we can conveniently
  // discard nodes with low probability when there are too many candidates.
  using CandidateQueue =
      std::priority_queue<BeamNode, std::vector<BeamNode>, std::greater<>>;

  using TokenIdAndProb = std::pair<OnDeviceTailTokenizer::TokenId, float>;

  // Helper function to initialize TFlite model interpreter.
  bool InitModelInterpreter(const base::FilePath& model_filepath);

  // Gets the encoding for previous query token IDs.
  bool EncodePreviousQuery(
      const OnDeviceTailTokenizer::TokenIds& prev_query_token_ids,
      std::vector<float>* prev_query_encoding);

  // Invokes subgraph `rnn_step_` to get the prediction for the next token.
  bool RunRnnStep(const OnDeviceTailTokenizer::TokenIds& rnn_step_cache_key,
                  const OnDeviceTailTokenizer::TokenId& input_id,
                  const std::vector<float>& prev_query_encoding,
                  const RnnCellStates& previous_states,
                  RnnStepOutput* rnn_step_output);

  // Creates new beams from the current beam and the RNN step output, and pushes
  // them into related candidate queues.
  void CreateNewBeams(const RnnStepOutput& rnn_step_output,
                      const BeamNode& current_beam,
                      size_t max_num_suggestions,
                      float log_prob_threshold,
                      CandidateQueue* partial_candidates,
                      CandidateQueue* completed_candidates);

  // Builds and maybe insert new beam nodes from the given token ID &
  // probability pair into the candidate queue and drop low probability node
  // from the queue if needed.
  void InsertBeamNodeToCandidateQueue(const TokenIdAndProb& token_id_and_prob,
                                      const RnnCellStates& states,
                                      const BeamNode& current_beam,
                                      float log_prob_threshold,
                                      size_t max_num_suggestions,
                                      CandidateQueue* queue);

  // Gets the root beam node by feeding all unambiguous token IDs (except the
  // last token) into the model.
  bool GetRootBeamNode(
      const OnDeviceTailTokenizer::Tokenization& input_tokenization,
      const OnDeviceTailTokenizer::TokenIds& prev_query_token_ids,
      std::vector<float>* prev_query_encoding,
      BeamNode* root_beam);

  // Resets LRU caches.
  void ResetCaches();

  // Helper to calculate log probability.
  static float GetLogProbability(float probability);

  // Loads bad suggestion filter lists from filepaths.
  void LoadBadSubstringSet();
  void LoadBadwordHashSet();

  // Determines if the given suggestion is bad and should be discarded, by
  // checking if the suggestion contain words specified by `badword_hashes_`.
  // Note currently this function might not support CJK language properly as it
  // uses whitespace to split the suggestion.
  // We use this on device filter since this model is an ML model and we do not
  // have a good way to force the model to drop a given result in any
  // circumstance during training.
  bool IsSuggestionBad(const std::string suggestion);

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
  size_t max_num_steps_;
  float log_probability_threshold_;

  // The time when the executor is last called.
  base::TimeTicks executor_last_called_time_;

  // Files and metadata needed to initialize the model executor;
  base::FilePath model_filepath_;
  base::FilePath vocab_filepath_;
  base::FilePath badword_hashes_filepath_;
  base::FilePath bad_substrings_filepath_;
  optimization_guide::proto::OnDeviceTailSuggestModelMetadata metadata_;

  // The hashes (calculated by base::PersistentHash) of badword and the bad
  // substrings which are encoded by BASE64 used to filter bad suggestions.
  std::set<uint32_t> badword_hashes_;
  std::set<std::string> bad_substrings_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_TAIL_MODEL_EXECUTOR_H_
