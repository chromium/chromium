// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/omnibox/browser/on_device_tail_model_executor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAreArray;

namespace {

constexpr int kNumLayer = 1;
constexpr int kStateSize = 512;
constexpr int kEmbeddingDim = 64;
constexpr int kMaxNumSteps = 20;
constexpr float kProbabilityThreshold = 0.05;

base::FilePath GetTestFilePath(const std::string& filename) {
  base::FilePath file_path;
  if (!filename.empty()) {
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path);
    std::string fullname = "components/test/data/omnibox/" + filename;
    file_path = file_path.AppendASCII(fullname);
  }
  return file_path;
}

}  // namespace

class OnDeviceTailModelExecutorPublic : public OnDeviceTailModelExecutor {
 public:
  using OnDeviceTailModelExecutor::BeamNode;
  using OnDeviceTailModelExecutor::CandidateQueue;
  using OnDeviceTailModelExecutor::RnnCellStates;
  using OnDeviceTailModelExecutor::RnnStepOutput;
  using OnDeviceTailModelExecutor::RunRnnStep;
  using OnDeviceTailModelExecutor::TokenIdAndProb;

  using OnDeviceTailModelExecutor::CreateNewBeams;
  using OnDeviceTailModelExecutor::EncodePreviousQuery;
  using OnDeviceTailModelExecutor::GetLogProbability;
  using OnDeviceTailModelExecutor::GetRootBeamNode;
  using OnDeviceTailModelExecutor::InsertBeamNodeToCandidateQueue;

  using OnDeviceTailModelExecutor::prev_query_cache_;
  using OnDeviceTailModelExecutor::rnn_step_cache_;
  using OnDeviceTailModelExecutor::tokenizer_;
  using OnDeviceTailModelExecutor::vocab_size_;
};

// Note: suggestions which contain unprintable tokens (see definition at
// OnDeviceTailTokenizer::IsTokenPrintable) will not be outputted by the
// executor and be dropped in the private member function
// OnDeviceTailModelExecutor::InsertBeamNodeToCandidateQueue.
class OnDeviceTailModelExecutorTest : public ::testing::Test {
 public:
  OnDeviceTailModelExecutorTest() {
    InitExecutor(base::flat_set<std::string>());
  }

 protected:
  void TearDown() override { executor_.reset(); }

  std::vector<float> GetPrevQueryCache(
      const OnDeviceTailTokenizer::TokenIds& token_ids) {
    std::vector<float> result;
    auto iter = executor_->prev_query_cache_.Get(token_ids);
    if (iter != executor_->prev_query_cache_.end()) {
      result = iter->second;
    }
    return result;
  }

  OnDeviceTailModelExecutorPublic::RnnStepOutput GetRnnStepOutputCache(
      const OnDeviceTailTokenizer::TokenIds& token_ids) {
    OnDeviceTailModelExecutorPublic::RnnStepOutput result;
    auto iter = executor_->rnn_step_cache_.Get(token_ids);
    if (iter != executor_->rnn_step_cache_.end()) {
      result = iter->second;
    }
    return result;
  }

  static void AreBeamNodesEqual(
      const OnDeviceTailModelExecutorPublic::BeamNode& b1,
      const OnDeviceTailModelExecutorPublic::BeamNode& b2) {
    EXPECT_EQ(b1.token_ids, b2.token_ids);
    EXPECT_EQ(b1.rnn_step_cache_key, b2.rnn_step_cache_key);
    EXPECT_EQ(b1.constraint_prefix, b2.constraint_prefix);
    EXPECT_EQ(b1.states, b2.states);
    EXPECT_NEAR(b1.log_prob, b2.log_prob, 0.01f);
  }

  void InitExecutor(const base::flat_set<std::string>& denylist_filenames) {
    executor_ = std::make_unique<OnDeviceTailModelExecutorPublic>();
    OnDeviceTailModelExecutor::ModelMetadata metadata;
    metadata.mutable_lstm_model_params()->set_num_layer(kNumLayer);
    metadata.mutable_lstm_model_params()->set_state_size(kStateSize);
    metadata.mutable_lstm_model_params()->set_embedding_dimension(
        kEmbeddingDim);
    metadata.mutable_lstm_model_params()->set_max_num_steps(kMaxNumSteps);
    metadata.mutable_lstm_model_params()->set_probability_threshold(
        kProbabilityThreshold);

    base::flat_set<base::FilePath> additional_files;
    additional_files.insert(GetTestFilePath("vocab_test.txt"));

    for (const auto& filename : denylist_filenames) {
      additional_files.insert(GetTestFilePath(filename));
    }

    EXPECT_TRUE(executor_->Init(GetTestFilePath("test_tail_model.tflite"),
                                additional_files, metadata));
  }

  std::unique_ptr<OnDeviceTailModelExecutorPublic> executor_;
};

TEST_F(OnDeviceTailModelExecutorTest, TestEncodePreviousQuery) {
  OnDeviceTailTokenizer::TokenIds ids1({66}), ids2({66, 67});
  std::vector<float> encoding1, encoding2, cached_encoding;

  EXPECT_TRUE(executor_->EncodePreviousQuery(ids1, &encoding1));
  EXPECT_TRUE(executor_->EncodePreviousQuery(ids2, &encoding2));
  EXPECT_TRUE(executor_->EncodePreviousQuery(ids1, &encoding1));

  EXPECT_NE(encoding1, encoding2);
  EXPECT_EQ(GetPrevQueryCache(ids1), encoding1);
  EXPECT_EQ(GetPrevQueryCache(ids2), encoding2);
}

TEST_F(OnDeviceTailModelExecutorTest, TestRunRnnStep) {
  OnDeviceTailTokenizer::TokenIds cache_key1({66, 67, 68}),
      cache_key2({66, 67, 69}), prev_query_ids({66, 67});
  OnDeviceTailTokenizer::TokenId input_id1 = 68, input_id2 = 69;
  OnDeviceTailModelExecutorPublic::RnnCellStates previous_states(kNumLayer,
                                                                 kStateSize);
  std::vector<float> prev_query_encoding(kEmbeddingDim);
  for (size_t i = 0; i < prev_query_encoding.size(); i++) {
    prev_query_encoding[i] = (i + 1) * 0.001;
  }

  OnDeviceTailModelExecutorPublic::RnnStepOutput output1, output2;
  EXPECT_TRUE(executor_->RunRnnStep(cache_key1, input_id1, prev_query_encoding,
                                    previous_states, &output1));
  EXPECT_TRUE(executor_->RunRnnStep(cache_key2, input_id2, prev_query_encoding,
                                    previous_states, &output2));
  EXPECT_NE(output1.probs, output2.probs);
  EXPECT_NE(output1.states.c_i, output2.states.c_i);
  EXPECT_NE(output1.states.m_i, output2.states.m_i);
  EXPECT_EQ(GetRnnStepOutputCache(cache_key1), output1);
  EXPECT_EQ(GetRnnStepOutputCache(cache_key2), output2);
}

TEST_F(OnDeviceTailModelExecutorTest, TestCreateNewBeams) {
  OnDeviceTailModelExecutorPublic::RnnStepOutput output(kNumLayer, kStateSize,
                                                        executor_->vocab_size_);
  OnDeviceTailModelExecutorPublic::BeamNode current_beam(kNumLayer, kStateSize),
      expected_beam(kNumLayer, kStateSize);

  output.states.c_i[0][10] = -0.77;
  output.states.m_i[0][11] = 0.88;
  current_beam.token_ids = {66, 67};
  current_beam.rnn_step_cache_key = {64, 65, 66, 67};
  current_beam.log_prob =
      OnDeviceTailModelExecutorPublic::GetLogProbability(0.9);

  expected_beam.states = output.states;
  expected_beam.constraint_prefix = "";

  size_t max_num_suggestions = 3;
  float log_prob_threshold =
      OnDeviceTailModelExecutorPublic::GetLogProbability(0.3);

  {
    OnDeviceTailModelExecutorPublic::CandidateQueue partial_candidates,
        completed_candidates;

    output.probs[69] = 0.8;
    output.probs[70] = 0.7;
    output.probs[71] = 0.6;
    output.probs[72] = 0.3;
    output.probs[executor_->tokenizer_->GetEndQueryTokenId()] = 0.7;
    executor_->CreateNewBeams(output, current_beam, max_num_suggestions,
                              log_prob_threshold, &partial_candidates,
                              &completed_candidates);
    EXPECT_EQ(3U, partial_candidates.size());
    EXPECT_EQ(1U, completed_candidates.size());

    expected_beam.token_ids = {66, 67,
                               executor_->tokenizer_->GetEndQueryTokenId()};
    expected_beam.rnn_step_cache_key = {
        64, 65, 66, 67, executor_->tokenizer_->GetEndQueryTokenId()};
    expected_beam.log_prob =
        OnDeviceTailModelExecutorPublic::GetLogProbability(0.9 * 0.7);
    AreBeamNodesEqual(expected_beam, completed_candidates.top());

    expected_beam.token_ids = {66, 67, 71};
    expected_beam.rnn_step_cache_key = {64, 65, 66, 67, 71};
    expected_beam.log_prob =
        OnDeviceTailModelExecutorPublic::GetLogProbability(0.9 * 0.6);
    AreBeamNodesEqual(expected_beam, partial_candidates.top());

    partial_candidates.pop();
    expected_beam.token_ids = {66, 67, 70};
    expected_beam.rnn_step_cache_key = {64, 65, 66, 67, 70};
    expected_beam.log_prob =
        OnDeviceTailModelExecutorPublic::GetLogProbability(0.9 * 0.7);
    AreBeamNodesEqual(expected_beam, partial_candidates.top());

    partial_candidates.pop();
    expected_beam.token_ids = {66, 67, 69};
    expected_beam.rnn_step_cache_key = {64, 65, 66, 67, 69};
    expected_beam.log_prob =
        OnDeviceTailModelExecutorPublic::GetLogProbability(0.9 * 0.8);
    AreBeamNodesEqual(expected_beam, partial_candidates.top());
  }

  // With constraint prefix set.
  {
    OnDeviceTailModelExecutorPublic::CandidateQueue partial_candidates,
        completed_candidates;
    current_beam.constraint_prefix = "a";
    output.probs[69] = 0.9;
    output.probs[70] = 0.8;
    output.probs[71] = 0.7;
    output.probs[261] = 0.08;  // token#261: "ab"
    output.probs[262] = 0.07;  // token#262: "ac"
    output.probs[263] = 0.01;  // token#262: "ad"

    executor_->CreateNewBeams(output, current_beam, max_num_suggestions,
                              log_prob_threshold, &partial_candidates,
                              &completed_candidates);

    expected_beam.token_ids = {66, 67, 262};
    expected_beam.rnn_step_cache_key = {64, 65, 66, 67, 262};
    expected_beam.log_prob =
        OnDeviceTailModelExecutorPublic::GetLogProbability(0.9 * (0.07 / 0.16));

    EXPECT_EQ(2U, partial_candidates.size());
    EXPECT_EQ(0U, completed_candidates.size());

    AreBeamNodesEqual(expected_beam, partial_candidates.top());
    partial_candidates.pop();

    expected_beam.token_ids = {66, 67, 261};
    expected_beam.rnn_step_cache_key = {64, 65, 66, 67, 261};
    expected_beam.log_prob =
        OnDeviceTailModelExecutorPublic::GetLogProbability(0.9 * (0.08 / 0.16));
    AreBeamNodesEqual(expected_beam, partial_candidates.top());
  }
}

TEST_F(OnDeviceTailModelExecutorTest, TestInsertBeamNodeToCandidateQueue) {
  OnDeviceTailModelExecutorPublic::RnnCellStates states(kNumLayer, kStateSize);
  states.c_i[0][10] = -0.77;
  states.m_i[0][11] = 0.88;

  OnDeviceTailModelExecutorPublic::BeamNode current_beam(kNumLayer, kStateSize),
      expected_beam(kNumLayer, kStateSize);
  current_beam.token_ids = {66, 67};
  current_beam.rnn_step_cache_key = {64, 65, 66, 67};
  current_beam.log_prob =
      OnDeviceTailModelExecutorPublic::GetLogProbability(0.9);

  size_t max_num_suggestions = 2;
  float log_prob_threshold =
      OnDeviceTailModelExecutorPublic::GetLogProbability(0.5);

  // Empty candidate queue.
  {
    OnDeviceTailModelExecutorPublic::CandidateQueue queue;
    OnDeviceTailModelExecutorPublic::TokenIdAndProb token_id_and_prob{68, 0.8};
    expected_beam.token_ids = {66, 67, 68};
    expected_beam.rnn_step_cache_key = {64, 65, 66, 67, 68};
    expected_beam.states = states;
    expected_beam.log_prob =
        OnDeviceTailModelExecutorPublic::GetLogProbability(0.9 * 0.8);

    executor_->InsertBeamNodeToCandidateQueue(token_id_and_prob, states,
                                              current_beam, log_prob_threshold,
                                              max_num_suggestions, &queue);
    EXPECT_EQ(1U, queue.size());
    AreBeamNodesEqual(expected_beam, queue.top());
  }

  // Candidate queue with already 2 candidates inside.
  {
    OnDeviceTailModelExecutorPublic::CandidateQueue queue;
    OnDeviceTailModelExecutorPublic::TokenIdAndProb token_id_and_prob{68, 0.8};
    expected_beam.token_ids = {66, 67, 68};
    expected_beam.rnn_step_cache_key = {64, 65, 66, 67, 68};
    expected_beam.states = states;
    expected_beam.log_prob =
        OnDeviceTailModelExecutorPublic::GetLogProbability(0.9 * 0.8);

    OnDeviceTailModelExecutorPublic::BeamNode beam1(kNumLayer, kStateSize),
        beam2(kNumLayer, kStateSize);

    beam1.token_ids = {69};
    beam1.log_prob = OnDeviceTailModelExecutorPublic::GetLogProbability(0.5);
    beam2.token_ids = {70};
    beam2.log_prob = OnDeviceTailModelExecutorPublic::GetLogProbability(0.4);
    queue.emplace(beam1);
    queue.emplace(beam2);

    executor_->InsertBeamNodeToCandidateQueue(token_id_and_prob, states,
                                              current_beam, log_prob_threshold,
                                              max_num_suggestions, &queue);
    EXPECT_EQ(2U, queue.size());
    EXPECT_EQ(beam1.token_ids, queue.top().token_ids);
    queue.pop();
    AreBeamNodesEqual(expected_beam, queue.top());
  }
}

TEST_F(OnDeviceTailModelExecutorTest, TestGetRootBeamNode) {
  OnDeviceTailTokenizer::Tokenization tokenization;
  tokenization.unambiguous_ids = {257, 468, 469, 470};
  tokenization.unambiguous_prefix = "ackageail";
  tokenization.constraint_prefix = "a";

  OnDeviceTailTokenizer::TokenIds prev_query_token_ids = {474, 475};
  std::vector<float> expected_prev_query_encoding(kEmbeddingDim);
  expected_prev_query_encoding[1] = 0.66;
  expected_prev_query_encoding[11] = 0.77;
  expected_prev_query_encoding[31] = 0.88;
  executor_->prev_query_cache_.Put(prev_query_token_ids,
                                   expected_prev_query_encoding);

  OnDeviceTailModelExecutorPublic::RnnStepOutput rnn_output(
      kNumLayer, kStateSize, executor_->vocab_size_);
  OnDeviceTailModelExecutorPublic::RnnCellStates states(kNumLayer, kStateSize);
  states.c_i[0][25] = 25;
  states.m_i[0][35] = 35;
  rnn_output.states = states;
  rnn_output.probs[60] = 0.7;
  rnn_output.probs[70] = 0.6;
  OnDeviceTailTokenizer::TokenIds rnn_step_cache_key = {474, 475, 257, 468,
                                                        469};
  executor_->rnn_step_cache_.Put(rnn_step_cache_key, rnn_output);

  std::vector<float> prev_query_encoding;
  OnDeviceTailModelExecutorPublic::BeamNode root_beam, expected_beam;

  expected_beam.token_ids = tokenization.unambiguous_ids;
  rnn_step_cache_key.emplace_back(470);
  expected_beam.rnn_step_cache_key = rnn_step_cache_key;
  expected_beam.states = states;
  expected_beam.constraint_prefix = "a";

  executor_->GetRootBeamNode(tokenization, prev_query_token_ids,
                             &prev_query_encoding, &root_beam);
  EXPECT_EQ(expected_prev_query_encoding, prev_query_encoding);
  AreBeamNodesEqual(expected_beam, root_beam);
}

TEST_F(OnDeviceTailModelExecutorTest, TestGenerateSuggestionsForPrefix) {
  std::vector<OnDeviceTailModelExecutor::Prediction> predictions;

  {
    OnDeviceTailModelExecutor::ModelInput input("faceb", "", 5);
    predictions = executor_->GenerateSuggestionsForPrefix(input);
    EXPECT_FALSE(predictions.empty());
    EXPECT_TRUE(base::StartsWith(predictions[0].suggestion, "facebook",
                                 base::CompareCase::SENSITIVE));
  }

  {
    OnDeviceTailModelExecutor::ModelInput input("", "snapchat", 5);
    predictions = executor_->GenerateSuggestionsForPrefix(input);
    EXPECT_TRUE(predictions.empty());
  }

  {
    OnDeviceTailModelExecutor::ModelInput input("faceb", "snapchat", 5);
    predictions = executor_->GenerateSuggestionsForPrefix(input);
    EXPECT_FALSE(predictions.empty());
    EXPECT_TRUE(base::StartsWith(predictions[0].suggestion, "facebook",
                                 base::CompareCase::SENSITIVE));
  }
}

TEST_F(OnDeviceTailModelExecutorTest,
       TestGenerateSuggestionsWithBadwordFilter) {
  std::vector<OnDeviceTailModelExecutor::Prediction> predictions;

  {
    // Make sure the test model can predict "login" for prefix "logi".
    InitExecutor(base::flat_set<std::string>());
    OnDeviceTailModelExecutor::ModelInput input("logi", "", 5);
    predictions = executor_->GenerateSuggestionsForPrefix(input);
    EXPECT_EQ(predictions.size(), 1U);
    EXPECT_TRUE(base::StartsWith(predictions[0].suggestion, "login",
                                 base::CompareCase::SENSITIVE));
  }

  {
    // The test badwords file contains hash for word "login", so |predictions|
    // should not contain results with word "login".
    InitExecutor(base::flat_set<std::string>({"badword_hashes_test.txt"}));
    OnDeviceTailModelExecutor::ModelInput input("logi", "", 5);
    predictions = executor_->GenerateSuggestionsForPrefix(input);
    for (auto& prediction : predictions) {
      auto words =
          base::SplitString(prediction.suggestion, base::kWhitespaceASCII,
                            base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      EXPECT_FALSE(base::Contains(words, "login"));
    }
  }

  {
    // The denylist contains substrings "ogi" & "abc", so |prediction| should
    // not contain these strings neither.
    InitExecutor(base::flat_set<std::string>({"denylist_test.txt"}));
    OnDeviceTailModelExecutor::ModelInput input("logi", "", 5);
    predictions = executor_->GenerateSuggestionsForPrefix(input);
    for (auto& prediction : predictions) {
      EXPECT_FALSE(base::Contains(prediction.suggestion, "abc"));
      EXPECT_FALSE(base::Contains(prediction.suggestion, "ogi"));
    }
  }
}
