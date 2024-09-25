// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/omnibox/browser/on_device_tail_model_executor.h"

#include <cmath>
#include <cstdint>
#include <sstream>
#include <string_view>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/tflite_op_resolver.h"
#include "third_party/tflite/src/tensorflow/lite/c/c_api_types.h"
#include "third_party/tflite/src/tensorflow/lite/kernels/register.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

namespace {
// The names of the subgraphs.
static constexpr char kPreviousQueryEncoder[] = "context_encoder";
static constexpr char kRnnStep[] = "rnn_step";

// The names of input & output node.
static constexpr char kPrevQueryTokenIdsNodeName[] = "prev_query_token_ids";
static constexpr char kPrevQueryEncodingOutputNodeName[] =
    "prev_query_encoding";

static constexpr char kRnnStepInputIdsNodeName[] = "input_ids";
static constexpr char kRnnStepPrevQueryEncodingInputNodeName[] =
    "prev_query_encoding";

static constexpr std::string_view kRnnStepCStateInputNamePrefix = "c_in_";
static constexpr std::string_view kRnnStepMStateInputNamePrefix = "m_in_";

static constexpr std::string_view kRnnStepCStateOutputNamePrefix = "c_out_";
static constexpr std::string_view kRnnStepMStateOutputNamePrefix = "m_out_";

static constexpr char kRnnStepOutputProbsNodeName[] = "probs";

// Some default values of params needed to run the model.
static constexpr size_t kDefaultMaxNumSteps = 20;
static constexpr float kDefaultProbabilityThreshold = 0.01;

// The sizes of the caches.
static constexpr size_t kPreQueryEncodingCacheSize = 10;
static constexpr size_t kRnnStepOutputCacheSize = 20;

// Maximum file size that will be loaded in bytes.
static constexpr size_t kFileSizeLimit = 128 * 1024;

// Keywords to identify additional files needed by the executor.
static constexpr char kVocabFileNameKeyword[] = "vocab";
static constexpr char kBadwordHashesFileNameKeyword[] = "hashes";
static constexpr char kBadSubstringDenyListFileNameKeyword[] = "denylist";

std::ostream& operator<<(std::ostream& os,
                         const OnDeviceTailTokenizer::TokenIds& ids) {
  if (ids.empty()) {
    return os;
  }

  auto iter = ids.begin();
  os << base::NumberToString(*iter);
  ++iter;

  for (; iter != ids.end(); ++iter) {
    os << ", " << base::NumberToString(*iter);
  }
  return os;
}

std::string LoadFileContent(const base::FilePath file_path) {
  std::string content;
  if (file_path.empty()) {
    return content;
  }
  if (!base::ReadFileToStringWithMaxSize(file_path, &content, kFileSizeLimit)) {
    DVLOG(1) << "Failed to read file: " << file_path.LossyDisplayName();
    content.clear();
  }
  return content;
}

}  // namespace

OnDeviceTailModelExecutor::ModelInput::ModelInput() = default;

OnDeviceTailModelExecutor::ModelInput::ModelInput(std::string prefix,
                                                  std::string previous_query,
                                                  size_t max_num_suggestions)
    : prefix(std::move(prefix)),
      previous_query(std::move(previous_query)),
      max_num_suggestions(max_num_suggestions) {}

OnDeviceTailModelExecutor::RnnCellStates::RnnCellStates() = default;

OnDeviceTailModelExecutor::RnnCellStates::RnnCellStates(size_t num_layer,
                                                        size_t state_size) {
  c_i = std::vector<std::vector<float>>(num_layer,
                                        std::vector<float>(state_size));
  m_i = std::vector<std::vector<float>>(num_layer,
                                        std::vector<float>(state_size));
}

OnDeviceTailModelExecutor::RnnCellStates::RnnCellStates(
    const RnnCellStates& other) = default;

OnDeviceTailModelExecutor::RnnCellStates::RnnCellStates(
    RnnCellStates&& other) noexcept = default;

OnDeviceTailModelExecutor::RnnCellStates&
OnDeviceTailModelExecutor::RnnCellStates::operator=(
    const RnnCellStates& other) = default;

OnDeviceTailModelExecutor::RnnCellStates&
OnDeviceTailModelExecutor::RnnCellStates::operator=(
    RnnCellStates&& other) noexcept = default;

OnDeviceTailModelExecutor::RnnCellStates::~RnnCellStates() = default;

OnDeviceTailModelExecutor::RnnStepOutput::RnnStepOutput() = default;

OnDeviceTailModelExecutor::RnnStepOutput::RnnStepOutput(size_t num_layer,
                                                        size_t state_size,
                                                        size_t vocab_size)
    : states(num_layer, state_size) {
  probs = std::vector<float>(vocab_size, std::numeric_limits<float>::min());
}

OnDeviceTailModelExecutor::RnnStepOutput::RnnStepOutput(
    const RnnStepOutput& other) {
  probs = other.probs;
  states = other.states;
}

OnDeviceTailModelExecutor::RnnStepOutput::~RnnStepOutput() = default;

OnDeviceTailModelExecutor::BeamNode::BeamNode() = default;

OnDeviceTailModelExecutor::BeamNode::BeamNode(int num_layer, int state_size)
    : states(num_layer, state_size) {}

OnDeviceTailModelExecutor::BeamNode::BeamNode(const BeamNode& other) = default;

OnDeviceTailModelExecutor::BeamNode::BeamNode(BeamNode&& other) noexcept =
    default;

OnDeviceTailModelExecutor::BeamNode&
OnDeviceTailModelExecutor::BeamNode::operator=(const BeamNode& other) = default;

OnDeviceTailModelExecutor::BeamNode&
OnDeviceTailModelExecutor::BeamNode::operator=(BeamNode&& other) noexcept =
    default;

OnDeviceTailModelExecutor::BeamNode::~BeamNode() = default;

OnDeviceTailModelExecutor::OnDeviceTailModelExecutor()
    : prev_query_cache_(kPreQueryEncodingCacheSize),
      rnn_step_cache_(kRnnStepOutputCacheSize) {}

OnDeviceTailModelExecutor::~OnDeviceTailModelExecutor() = default;

bool OnDeviceTailModelExecutor::Init() {
  executor_last_called_time_ = base::TimeTicks::Now();
  Reset();
  if (model_filepath_.empty() || vocab_filepath_.empty()) {
    return false;
  }
  auto tokenizer = std::make_unique<OnDeviceTailTokenizer>();
  tokenizer->Init(vocab_filepath_);
  if (!tokenizer->IsReady()) {
    DVLOG(1) << "Could not create tokenizer from file "
             << vocab_filepath_.LossyDisplayName();
    vocab_filepath_.clear();
    return false;
  }
  tokenizer_ = std::move(tokenizer);

  if (!InitModelInterpreter(model_filepath_)) {
    Reset();
    model_filepath_.clear();
    return false;
  }

  state_size_ = metadata_.lstm_model_params().state_size();
  num_layer_ = metadata_.lstm_model_params().num_layer();
  embedding_dimension_ = metadata_.lstm_model_params().embedding_dimension();

  if (metadata_.lstm_model_params().max_num_steps() > 0) {
    max_num_steps_ = metadata_.lstm_model_params().max_num_steps();
  } else {
    max_num_steps_ = kDefaultMaxNumSteps;
  }

  if (metadata_.lstm_model_params().probability_threshold() > 0) {
    log_probability_threshold_ = GetLogProbability(
        metadata_.lstm_model_params().probability_threshold());
  } else {
    log_probability_threshold_ =
        GetLogProbability(kDefaultProbabilityThreshold);
  }

  vocab_size_ = tokenizer_->vocab_size();
  LoadBadSubstringSet();
  LoadBadwordHashSet();

  return true;
}

bool OnDeviceTailModelExecutor::Init(
    const base::FilePath& model_filepath,
    const base::flat_set<base::FilePath>& additional_files,
    const ModelMetadata& metadata) {
  base::FilePath vocab_filepath, badword_hashes_filepath,
      bad_substrings_filepath;
  for (const base::FilePath& file_path : additional_files) {
    if (!file_path.empty()) {
      std::string file_path_str =
          optimization_guide::FilePathToString(file_path);
      if (base::Contains(file_path_str, kVocabFileNameKeyword)) {
        vocab_filepath = file_path;
      } else if (base::Contains(file_path_str, kBadwordHashesFileNameKeyword)) {
        badword_hashes_filepath = file_path;
      } else if (base::Contains(file_path_str,
                                kBadSubstringDenyListFileNameKeyword)) {
        bad_substrings_filepath = file_path;
      }
    }
  }

  if (model_filepath.empty() || vocab_filepath.empty()) {
    return false;
  }

  model_filepath_ = model_filepath;
  vocab_filepath_ = vocab_filepath;
  badword_hashes_filepath_ = badword_hashes_filepath;
  bad_substrings_filepath_ = bad_substrings_filepath;
  metadata_ = metadata;

  if (Init()) {
    return true;
  }

  model_filepath_.clear();
  vocab_filepath_.clear();
  badword_hashes_filepath_.clear();
  bad_substrings_filepath_.clear();
  return false;
}

bool OnDeviceTailModelExecutor::InitModelInterpreter(
    const base::FilePath& model_filepath) {
  auto model_fb = std::make_unique<base::MemoryMappedFile>();
  if (!model_fb->Initialize(model_filepath)) {
    DVLOG(1) << "Could not load model into memory from path "
             << model_filepath.LossyDisplayName();
    return false;
  }
  model_fb_ = std::move(model_fb);

  std::unique_ptr<tflite::FlatBufferModel> model =
      tflite::FlatBufferModel::VerifyAndBuildFromBuffer(
          reinterpret_cast<const char*>(model_fb_->data()),
          model_fb_->length());

  if (model == nullptr) {
    DVLOG(1) << "Could not create flat buffer model for file "
             << model_filepath.LossyDisplayName();
    return false;
  }

  optimization_guide::TFLiteOpResolver resolver;
  if (tflite::InterpreterBuilder(*model, resolver)(&interpreter_) !=
      kTfLiteOk) {
    DVLOG(1) << "Could not create on device tail model interpreter!";
    return false;
  }

  prev_query_encoder_ = interpreter_->GetSignatureRunner(kPreviousQueryEncoder);
  if (prev_query_encoder_ == nullptr) {
    DVLOG(1) << "Could not create signature runner context_encoder";
    return false;
  }
  if (prev_query_encoder_->AllocateTensors() != kTfLiteOk) {
    DVLOG(1) << "Could not allocate tensors for previous query encoder";
    return false;
  }

  rnn_step_ = interpreter_->GetSignatureRunner(kRnnStep);
  if (rnn_step_ == nullptr) {
    DVLOG(1) << "Could not create signature runner rnn_step";
    return false;
  }
  if (rnn_step_->AllocateTensors() != kTfLiteOk) {
    DVLOG(1) << "Could not allocate tenors for rnn step";
    return false;
  }

  return true;
}

bool OnDeviceTailModelExecutor::EncodePreviousQuery(
    const OnDeviceTailTokenizer::TokenIds& prev_query_token_ids,
    std::vector<float>* prev_query_encoding) {
  auto iter = prev_query_cache_.Get(prev_query_token_ids);
  if (iter != prev_query_cache_.end()) {
    *prev_query_encoding = iter->second;
    return true;
  }

  DCHECK(prev_query_encoder_);
  DCHECK(prev_query_encoding);

  // Resizes the input tensor for previous query encoder as the input size is
  // not fixed.
  if (kTfLiteOk != prev_query_encoder_->ResizeInputTensor(
                       kPrevQueryTokenIdsNodeName,
                       {1, static_cast<int>(prev_query_token_ids.size())})) {
    DVLOG(1)
        << "Could not resize input tensor for prev query encoder to length "
        << prev_query_token_ids.size();
    return false;
  }
  if (kTfLiteOk != prev_query_encoder_->AllocateTensors()) {
    DVLOG(1) << "Could not allocate tensors for prev query encoder after "
             << "resizing";
    return false;
  }

  // Input: type INT32, shape [1, previous query length]
  TfLiteTensor* input_tensor =
      prev_query_encoder_->input_tensor(kPrevQueryTokenIdsNodeName);
  for (size_t i = 0; i < prev_query_token_ids.size(); ++i) {
    input_tensor->data.i32[i] = prev_query_token_ids[i];
  }
  if (prev_query_encoder_->Invoke() != kTfLiteOk) {
    DVLOG(1) << "Could not invoke prev query encoder";
    return false;
  }

  // Output: type FLOAT32, shape [1, embedding_dimension_]
  auto* output_tensor =
      prev_query_encoder_->output_tensor(kPrevQueryEncodingOutputNodeName);
  TfLiteIntArray* dims = output_tensor->dims;
  if (dims->size != 2 || dims->data[0] != 1 ||
      dims->data[1] != static_cast<int>(embedding_dimension_)) {
    DVLOG(1) << "Wrong embedding dimension for previous query encoder";
    return false;
  }

  if (prev_query_encoding->size() != embedding_dimension_) {
    prev_query_encoding->resize(embedding_dimension_);
  }

  for (size_t i = 0; i < embedding_dimension_; ++i) {
    prev_query_encoding->at(i) = output_tensor->data.f[i];
  }

  prev_query_cache_.Put(prev_query_token_ids, *prev_query_encoding);
  return true;
}

void OnDeviceTailModelExecutor::ResetCaches() {
  prev_query_cache_.Clear();
  rnn_step_cache_.Clear();
}

void OnDeviceTailModelExecutor::LoadBadSubstringSet() {
  bad_substrings_.clear();

  std::string content = LoadFileContent(bad_substrings_filepath_);
  if (content.empty()) {
    return;
  }

  std::string bad_substring, line;
  std::stringstream file_content(content);
  while (std::getline(file_content, line)) {
    if (line.empty()) {
      break;
    }
    if (base::Base64Decode(line, &bad_substring)) {
      bad_substrings_.insert(bad_substring);
    } else {
      DVLOG(1) << "Could not decode line: " << line;
    }
  }
}

void OnDeviceTailModelExecutor::LoadBadwordHashSet() {
  badword_hashes_.clear();

  std::string content = LoadFileContent(badword_hashes_filepath_);
  if (content.empty()) {
    return;
  }

  std::string hash_string;
  std::stringstream badword_hash_strings(content);
  while (std::getline(badword_hash_strings, hash_string)) {
    if (hash_string.empty()) {
      break;
    }
    uint32_t hash_int;
    if (base::StringToUint(hash_string, &hash_int)) {
      badword_hashes_.insert(hash_int);
    }
  }
}

bool OnDeviceTailModelExecutor::IsSuggestionBad(const std::string suggestion) {
  if (suggestion.empty()) {
    return false;
  }

  for (const std::string& substring : bad_substrings_) {
    if (base::Contains(suggestion, substring)) {
      return true;
    }
  }

  if (!badword_hashes_.empty()) {
    std::vector<std::string> words =
        base::SplitString(suggestion, base::kWhitespaceASCII,
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    for (const std::string& word : words) {
      auto hash_value = base::PersistentHash(word);
      if (base::Contains(badword_hashes_, hash_value)) {
        return true;
      }
    }
  }

  return false;
}

void OnDeviceTailModelExecutor::Reset() {
  ResetCaches();
  model_fb_ = nullptr;
  tokenizer_ = nullptr;
  prev_query_encoder_ = nullptr;
  rnn_step_ = nullptr;
  interpreter_ = nullptr;
}

bool OnDeviceTailModelExecutor::RunRnnStep(
    const OnDeviceTailTokenizer::TokenIds& rnn_step_cache_key,
    const OnDeviceTailTokenizer::TokenId& input_id,
    const std::vector<float>& prev_query_encoding,
    const RnnCellStates& previous_states,
    RnnStepOutput* rnn_step_output) {
  const auto iter = rnn_step_cache_.Get(rnn_step_cache_key);
  if (iter != rnn_step_cache_.end()) {
    *rnn_step_output = iter->second;
    return true;
  }

  DCHECK(rnn_step_);

  TfLiteTensor* input_tensor;

  // Feed current token ID.
  input_tensor = rnn_step_->input_tensor(kRnnStepInputIdsNodeName);
  input_tensor->data.i32[0] = input_id;

  // Feed previous query encoding.
  input_tensor =
      rnn_step_->input_tensor(kRnnStepPrevQueryEncodingInputNodeName);
  for (size_t i = 0; i < prev_query_encoding.size(); ++i) {
    input_tensor->data.f[i] = prev_query_encoding[i];
  }

  // Feed c states.
  for (size_t i = 0; i < num_layer_; ++i) {
    std::string node_name =
        base::StrCat({kRnnStepCStateInputNamePrefix, base::NumberToString(i)});
    input_tensor = rnn_step_->input_tensor(node_name.c_str());
    for (size_t j = 0; j < state_size_; ++j) {
      input_tensor->data.f[j] = previous_states.c_i[i][j];
    }
  }

  // Feed m states.
  for (size_t i = 0; i < num_layer_; ++i) {
    std::string node_name =
        base::StrCat({kRnnStepMStateInputNamePrefix, base::NumberToString(i)});
    input_tensor = rnn_step_->input_tensor(node_name.c_str());
    for (size_t j = 0; j < state_size_; ++j) {
      input_tensor->data.f[j] = previous_states.m_i[i][j];
    }
  }

  if (kTfLiteOk != rnn_step_->Invoke()) {
    DVLOG(1) << "Could not invoke RNN step runner";
    return false;
  }

  RnnStepOutput output(num_layer_, state_size_, vocab_size_);
  const TfLiteTensor* output_tensor;
  output_tensor = rnn_step_->output_tensor(kRnnStepOutputProbsNodeName);

  // Fetch output probabilities.
  for (size_t i = 0; i < vocab_size_; ++i) {
    output.probs[i] = output_tensor->data.f[i];
  }

  // Fetch c states.
  for (size_t i = 0; i < num_layer_; ++i) {
    std::string node_name =
        base::StrCat({kRnnStepCStateOutputNamePrefix, base::NumberToString(i)});
    output_tensor = rnn_step_->output_tensor(node_name.c_str());
    for (size_t j = 0; j < state_size_; ++j) {
      output.states.c_i[i][j] = output_tensor->data.f[j];
    }
  }

  // Fetch m states.
  for (size_t i = 0; i < num_layer_; ++i) {
    std::string node_name =
        base::StrCat({kRnnStepMStateOutputNamePrefix, base::NumberToString(i)});
    output_tensor = rnn_step_->output_tensor(node_name.c_str());
    for (size_t j = 0; j < state_size_; ++j) {
      output.states.m_i[i][j] = output_tensor->data.f[j];
    }
  }

  rnn_step_cache_.Put(rnn_step_cache_key, output);
  *rnn_step_output = std::move(output);
  return true;
}

void OnDeviceTailModelExecutor::CreateNewBeams(
    const RnnStepOutput& rnn_step_output,
    const BeamNode& current_beam,
    size_t max_num_suggestions,
    float log_prob_threshold,
    CandidateQueue* partial_candidates,
    CandidateQueue* completed_candidates) {
  DCHECK(partial_candidates && completed_candidates);
  if (current_beam.log_prob < log_prob_threshold) {
    return;
  }

  if (current_beam.constraint_prefix.empty()) {
    for (OnDeviceTailTokenizer::TokenId token_id = 0;
         static_cast<size_t>(token_id) < rnn_step_output.probs.size();
         ++token_id) {
      CandidateQueue* queue = tokenizer_->IsEndQueryTokenId(token_id)
                                  ? completed_candidates
                                  : partial_candidates;
      InsertBeamNodeToCandidateQueue(
          {token_id, rnn_step_output.probs[token_id]}, rnn_step_output.states,
          current_beam, log_prob_threshold, max_num_suggestions, queue);
    }
    return;
  }

  // If constraint prefix is set, normalize the probabilities of the matching
  // tokens.
  // Given the sum of the probability for tokens matching constraint prefix, the
  // normalized probability is:
  // prob[i]_normalized = prob[i] / sum_constraint_prob, where
  // sum_constraint_prob = sum(prob[i]) for i-th token which matches the
  // constraint prefix.
  float sum_constraint_prob = 0;
  std::vector<TokenIdAndProb> candidates;

  for (OnDeviceTailTokenizer::TokenId token_id = 0;
       static_cast<size_t>(token_id) < rnn_step_output.probs.size();
       ++token_id) {
    if (!base::StartsWith(tokenizer_->IdToToken(token_id),
                          current_beam.constraint_prefix,
                          base::CompareCase::SENSITIVE)) {
      continue;
    }
    sum_constraint_prob += rnn_step_output.probs[token_id];
    candidates.emplace_back(token_id, rnn_step_output.probs[token_id]);
  }

  for (const auto& token_id_and_prob : candidates) {
    InsertBeamNodeToCandidateQueue(
        {token_id_and_prob.first,
         token_id_and_prob.second / sum_constraint_prob},
        rnn_step_output.states, current_beam, log_prob_threshold,
        max_num_suggestions, partial_candidates);
  }

  return;
}

void OnDeviceTailModelExecutor::InsertBeamNodeToCandidateQueue(
    const TokenIdAndProb& token_id_and_prob,
    const RnnCellStates& states,
    const BeamNode& current_beam,
    float log_prob_threshold,
    size_t max_num_suggestions,
    CandidateQueue* queue) {
  DCHECK(queue);
  BeamNode node;

  node.log_prob =
      current_beam.log_prob + GetLogProbability(token_id_and_prob.second);
  if (node.log_prob < log_prob_threshold) {
    return;
  }

  const OnDeviceTailTokenizer::TokenId& new_token_id = token_id_and_prob.first;
  // Drop the candidate if the given token cannot be properly displayed to
  // users, unless it is the end query token.
  if (!(tokenizer_->IsEndQueryTokenId(new_token_id) ||
        tokenizer_->IsTokenPrintable(new_token_id))) {
    return;
  }

  // Check if there are enough candidates in the queue and drop the lowest
  // probability candidate from the queue if needed.
  if (queue->size() >= max_num_suggestions) {
    if (node.log_prob <= queue->top().log_prob) {
      return;
    }
    queue->pop();
  }

  node.token_ids = current_beam.token_ids;
  node.token_ids.emplace_back(new_token_id);
  node.rnn_step_cache_key = current_beam.rnn_step_cache_key;
  node.rnn_step_cache_key.emplace_back(new_token_id);
  node.states = states;

  queue->emplace(std::move(node));
}

bool OnDeviceTailModelExecutor::GetRootBeamNode(
    const OnDeviceTailTokenizer::Tokenization& input_tokenization,
    const OnDeviceTailTokenizer::TokenIds& prev_query_token_ids,
    std::vector<float>* prev_query_encoding,
    BeamNode* root_beam) {
  DCHECK(prev_query_encoding);
  if (!EncodePreviousQuery(prev_query_token_ids, prev_query_encoding)) {
    return false;
  }

  DCHECK(root_beam);
  root_beam->rnn_step_cache_key = prev_query_token_ids;
  root_beam->token_ids.clear();
  RnnStepOutput rnn_step_output(num_layer_, state_size_, vocab_size_);

  for (size_t i = 0; i < input_tokenization.unambiguous_ids.size() - 1; ++i) {
    const OnDeviceTailTokenizer::TokenId& token_id =
        input_tokenization.unambiguous_ids[i];

    root_beam->rnn_step_cache_key.emplace_back(token_id);
    root_beam->token_ids.emplace_back(token_id);
    if (!RunRnnStep(root_beam->rnn_step_cache_key, token_id,
                    *prev_query_encoding, rnn_step_output.states,
                    &rnn_step_output)) {
      return false;
    }
  }

  // Force the input id of the next RNN step invocation to be the last
  // unambiguous token of the given prefix.
  root_beam->rnn_step_cache_key.emplace_back(
      input_tokenization.unambiguous_ids.back());
  root_beam->token_ids.emplace_back(input_tokenization.unambiguous_ids.back());
  root_beam->constraint_prefix = input_tokenization.constraint_prefix;
  root_beam->states = std::move(rnn_step_output.states);
  root_beam->log_prob = 0.0;
  return true;
}

// static
float OnDeviceTailModelExecutor::GetLogProbability(float probability) {
  return probability > 0 ? std::log(probability)
                         : std::numeric_limits<float>::min();
}

std::vector<OnDeviceTailModelExecutor::Prediction>
OnDeviceTailModelExecutor::GenerateSuggestionsForPrefix(
    const ModelInput& input) {
  executor_last_called_time_ = base::TimeTicks::Now();
  DCHECK(IsReady());
  std::vector<Prediction> predictions;

  // Only trigger for prefixed suggest requests.
  if (input.prefix.empty()) {
    return predictions;
  }

  // Return early if the prefix contains bad words.
  // TODO(crbug.com/40241602): maybe add a unit test for this.
  if (IsSuggestionBad(input.prefix)) {
    return predictions;
  }

  OnDeviceTailTokenizer::Tokenization input_tokenization;
  tokenizer_->CreatePrefixTokenization(input.prefix, &input_tokenization);

  OnDeviceTailTokenizer::TokenIds prev_query_token_ids;
  tokenizer_->TokenizePrevQuery(input.previous_query, &prev_query_token_ids);

  std::vector<float> prev_query_encoding;
  BeamNode root_beam;
  if (!GetRootBeamNode(input_tokenization, prev_query_token_ids,
                       &prev_query_encoding, &root_beam)) {
    DVLOG(1) << "Failed to get root beam node for prefix [" << input.prefix
             << "][" << input.previous_query << "]";
    return predictions;
  }

  OnDeviceTailModelExecutor::CandidateQueue partial_candidates,
      completed_candidates;
  partial_candidates.emplace(std::move(root_beam));

  for (size_t i = 0; i < max_num_steps_; ++i) {
    if (partial_candidates.empty()) {
      break;
    }

    std::vector<BeamNode> beam_nodes;
    while (!partial_candidates.empty()) {
      beam_nodes.emplace_back(std::move(partial_candidates.top()));
      partial_candidates.pop();
    }

    for (const auto& beam : beam_nodes) {
      RnnStepOutput rnn_step_output;
      if (RunRnnStep(beam.rnn_step_cache_key, beam.token_ids.back(),
                     prev_query_encoding, beam.states, &rnn_step_output)) {
        CreateNewBeams(rnn_step_output, beam, input.max_num_suggestions,
                       log_probability_threshold_, &partial_candidates,
                       &completed_candidates);

      } else {
        DVLOG(1) << "Failed to run RNN step for cache key: "
                 << beam.rnn_step_cache_key;
      }
    }
  }

  // Construct predictions from the beam node stored in the completed queue.
  for (; !completed_candidates.empty(); completed_candidates.pop()) {
    const BeamNode& beam = completed_candidates.top();
    if (beam.token_ids.size() < 3 ||
        !tokenizer_->IsBeginQueryTokenId(beam.token_ids.front()) ||
        !tokenizer_->IsEndQueryTokenId(beam.token_ids.back())) {
      DVLOG(1) << "Illegal prediction: " << beam.token_ids;
      continue;
    }

    std::string suggestion;
    // Skip the first leading space (i.e. the second token) if it is explicitly
    // added during encoding. Note the first token is always the begin query
    // token.
    size_t index;
    if (OmniboxFieldTrial::ShouldEncodeLeadingSpaceForOnDeviceTailSuggest()) {
      index = 2;
    } else {
      index = 1;
    }

    for (; index < beam.token_ids.size() - 1; ++index) {
      suggestion += tokenizer_->IdToToken(beam.token_ids[index]);
    }

    // Remove echo suggestion.
    if (suggestion == input.prefix) {
      continue;
    }

    if (IsSuggestionBad(suggestion)) {
      continue;
    }

    Prediction prediction;
    prediction.suggestion = suggestion;
    prediction.probability = std::exp(beam.log_prob);
    predictions.emplace_back(std::move(prediction));
  }

  // Reverse the predictions vector as it shall be returned in the descending
  // order of probability.
  std::reverse(predictions.begin(), predictions.end());
  return predictions;
}
