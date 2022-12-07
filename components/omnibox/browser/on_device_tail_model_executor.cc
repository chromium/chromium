// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/on_device_tail_model_executor.h"

#include <cmath>
#include <cstdint>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
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

static constexpr size_t kPreQueryEncodingCacheSize = 10;
static constexpr size_t kRnnStepOutputCacheSize = 20;

}  // namespace

OnDeviceTailModelExecutor::RnnCellStates::RnnCellStates() = default;

OnDeviceTailModelExecutor::RnnCellStates::RnnCellStates(size_t num_layer,
                                                        size_t state_size) {
  c_i = std::vector<std::vector<float>>(num_layer,
                                        std::vector<float>(state_size));
  m_i = std::vector<std::vector<float>>(num_layer,
                                        std::vector<float>(state_size));
}

OnDeviceTailModelExecutor::RnnCellStates::RnnCellStates(
    const RnnCellStates& other) {
  c_i = other.c_i;
  m_i = other.m_i;
}

OnDeviceTailModelExecutor::RnnCellStates::~RnnCellStates() = default;

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

OnDeviceTailModelExecutor::OnDeviceTailModelExecutor()
    : prev_query_cache_(kPreQueryEncodingCacheSize),
      rnn_step_cache_(kRnnStepOutputCacheSize) {}

OnDeviceTailModelExecutor::~OnDeviceTailModelExecutor() = default;

bool OnDeviceTailModelExecutor::Init(const base::FilePath& model_filepath,
                                     const base::FilePath& vocab_filepath,
                                     size_t state_size,
                                     size_t num_layer,
                                     size_t embedding_dimension) {
  Reset();

  auto tokenizer = std::make_unique<OnDeviceTailTokenizer>();
  tokenizer->Init(vocab_filepath);
  if (!tokenizer->IsReady()) {
    DVLOG(1) << "Could not create tokenizer from file "
             << vocab_filepath.LossyDisplayName();
    return false;
  }
  tokenizer_ = std::move(tokenizer);

  if (!InitModelInterpreter(model_filepath)) {
    Reset();
    return false;
  }

  state_size_ = state_size;
  num_layer_ = num_layer;
  embedding_dimension_ = embedding_dimension;
  vocab_size_ = tokenizer_->vocab_size();

  return true;
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

  tflite::StderrReporter error_reporter;
  std::unique_ptr<tflite::FlatBufferModel> model =
      tflite::FlatBufferModel::BuildFromBuffer(
          reinterpret_cast<const char*>(model_fb_->data()), model_fb_->length(),
          &error_reporter);

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

  for (size_t i = 0; i < embedding_dimension_; i++) {
    prev_query_encoding->at(i) = output_tensor->data.f[i];
  }

  prev_query_cache_.Put(prev_query_token_ids, *prev_query_encoding);
  return true;
}

void OnDeviceTailModelExecutor::ResetCaches() {
  prev_query_cache_.Clear();
  rnn_step_cache_.Clear();
}

void OnDeviceTailModelExecutor::Reset() {
  ResetCaches();
  model_fb_ = nullptr;
  tokenizer_ = nullptr;
  prev_query_encoder_ = nullptr;
  rnn_step_ = nullptr;
  interpreter_ = nullptr;
}