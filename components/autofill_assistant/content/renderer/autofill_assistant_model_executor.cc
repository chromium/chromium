// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/content/renderer/autofill_assistant_model_executor.h"

#include <ostream>

#include "base/command_line.h"
#include "base/i18n/case_conversion.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill_assistant/content/common/switches.h"
#include "components/optimization_guide/core/execution_status.h"
#include "components/optimization_guide/core/tflite_op_resolver.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/tflite/src/tensorflow/lite/kernels/internal/runtime_shape.h"
#include "third_party/tflite/src/tensorflow/lite/kernels/internal/tensor_ctypes.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/metadata/cc/metadata_extractor.h"

namespace autofill_assistant {
namespace {

std::string SparseVectorToDebugString(
    AutofillAssistantModelExecutor::SparseVector sparse_vector) {
  std::ostringstream out;
  out << "Sparse vector representation:\n";
  for (const auto& entry : sparse_vector) {
    out << "  [idx: [" << entry.first.first << ", " << entry.first.second
        << "], count: " << entry.second << "]";
  }
  return out.str();
}

void DenseEncode(
    const AutofillAssistantModelExecutor::SparseVector& sparse_vector,
    std::vector<std::vector<float>>& inputs) {
  for (const auto& entry : sparse_vector) {
    const auto& coordinates = entry.first;
    if (static_cast<size_t>(coordinates.first) >= inputs.size()) {
      NOTREACHED();
      continue;
    }
    if (static_cast<size_t>(coordinates.second) >=
        inputs[coordinates.first].size()) {
      NOTREACHED();
      continue;
    }
    inputs[coordinates.first][coordinates.second] = entry.second;
  }
}

}  // namespace

AutofillAssistantModelExecutor::AutofillAssistantModelExecutor(
    absl::optional<OverridesMap> overrides)
    : overrides_(std::move(overrides)) {}

AutofillAssistantModelExecutor::~AutofillAssistantModelExecutor() = default;

bool AutofillAssistantModelExecutor::InitializeModelFromFile(
    base::File model_file) {
  if (!model_file.IsValid() || !model_file_.Initialize(std::move(model_file))) {
    return false;
  }

  std::unique_ptr<tflite::task::core::TfLiteEngine> tflite_engine =
      std::make_unique<tflite::task::core::TfLiteEngine>(
          std::make_unique<optimization_guide::TFLiteOpResolver>());
  absl::Status model_load_status = tflite_engine->BuildModelFromFlatBuffer(
      reinterpret_cast<const char*>(model_file_.data()), model_file_.length());
  if (!model_load_status.ok()) {
    DLOG(ERROR) << "Failed to load model: " << model_load_status.ToString();
    return false;
  }
  absl::Status interpreter_status = tflite_engine->InitInterpreter(
      tflite::proto::ComputeSettings(), /* num_threads= */ 1);
  if (!interpreter_status.ok()) {
    DLOG(ERROR) << "Failed to initialize model interpreter: "
                << interpreter_status.ToString();
    return false;
  }

  auto metadata_or =
      tflite_engine->metadata_extractor()->GetAssociatedFile("metadata.pb");
  if (!metadata_or.ok()) {
    DLOG(ERROR) << "Could not read metadata: "
                << metadata_or.status().ToString();
    return false;
  }
  if (!model_metadata_.ParseFromArray(metadata_or->data(),
                                      metadata_or->size())) {
    DLOG(ERROR) << "Could not parse metadata.";
    return false;
  }

  InitializeTagsTokenizer("\\s+", model_metadata_.input().tag().vocabulary());
  InitializeTypesTokenizer("\\s+", model_metadata_.input().type().vocabulary());
  InitializeTextTokenizer(model_metadata_.input().text().regex(),
                          model_metadata_.input().text().vocabulary());

  BuildExecutionTask(std::move(tflite_engine));

  return true;
}

void AutofillAssistantModelExecutor::BuildExecutionTask(
    std::unique_ptr<tflite::task::core::TfLiteEngine> tflite_engine) {
  execution_task_ =
      std::make_unique<ExecutionTask>(std::move(tflite_engine), this);
}

absl::optional<std::pair<int, int>>
AutofillAssistantModelExecutor::ExecuteModelWithInput(
    const blink::AutofillAssistantNodeSignals& node_signals) {
  if (!execution_task_) {
    NOTREACHED() << "No available task";
    return absl::nullopt;
  }
  optimization_guide::ExecutionStatus out_status;
  return Execute(execution_task_.get(), &out_status, node_signals);
}

void AutofillAssistantModelExecutor::InitializeTagsTokenizer(
    const std::string& pattern,
    const std::string& vocabulary) {
  DCHECK(!pattern.empty());
  DCHECK(!vocabulary.empty());
  tags_tokenizer_ =
      std::make_unique<tflite::support::text::tokenizer::RegexTokenizer>(
          pattern, vocabulary.data(), vocabulary.size());
}

void AutofillAssistantModelExecutor::InitializeTypesTokenizer(
    const std::string& pattern,
    const std::string& vocabulary) {
  DCHECK(!pattern.empty());
  DCHECK(!vocabulary.empty());
  types_tokenizer_ =
      std::make_unique<tflite::support::text::tokenizer::RegexTokenizer>(
          pattern, vocabulary.data(), vocabulary.size());
}

void AutofillAssistantModelExecutor::InitializeTextTokenizer(
    const std::string& pattern,
    const std::string& vocabulary) {
  DCHECK(!pattern.empty());
  DCHECK(!vocabulary.empty());
  text_tokenizer_ =
      std::make_unique<tflite::support::text::tokenizer::RegexTokenizer>(
          pattern, vocabulary.data(), vocabulary.size());
}

bool AutofillAssistantModelExecutor::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const blink::AutofillAssistantNodeSignals& node_signals) {
  if (input_tensors.size() < 5u) {
    NOTREACHED() << "Input tensors mismatch.";
    return false;
  }

  SparseVector sparse_vector = TokenizeSignalsToSparseVector(node_signals);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAutofillAssistantDebugAnnotateDom)) {
    VLOG(3) << SparseVectorToDebugString(sparse_vector);
  }

  if (overrides_ && overrides_->contains(sparse_vector)) {
    overrides_result_ = (*overrides_)[sparse_vector];
    return true;
  }

  std::vector<std::vector<float>> inputs;
  for (const auto* input_tensor : input_tensors) {
    tflite::RuntimeShape shape = tflite::GetTensorShape(input_tensor);
    if (shape.DimensionsCount() < 2) {
      return false;
    }
    inputs.emplace_back(std::vector(shape.Dims(1), 0.0f));
  }
  DenseEncode(sparse_vector, inputs);

  for (size_t i = 0; i < inputs.size(); ++i) {
    absl::Status tensor_status =
        tflite::task::core::PopulateTensor<float>(inputs[i], input_tensors[i]);
    if (!tensor_status.ok()) {
      return false;
    }
  }
  return true;
}

bool AutofillAssistantModelExecutor::GetIndexOfBestRole(
    const std::vector<float>& output_role,
    size_t* index_of_best_role) const {
  if (output_role.size() <
      static_cast<size_t>(
          model_metadata_.output().semantic_role().classes_size())) {
    NOTREACHED();
    return false;
  }
  *index_of_best_role = std::distance(
      output_role.begin(),
      std::max_element(
          output_role.begin(),
          output_role.begin() +
              model_metadata_.output().semantic_role().classes_size()));
  return true;
}

bool AutofillAssistantModelExecutor::GetBlockIndex(
    const std::vector<float>& output_role,
    size_t index_of_best_role,
    int* block_index) const {
  if (index_of_best_role >=
      static_cast<size_t>(model_metadata_.output()
                              .semantic_role()
                              .objective_block_index_size())) {
    NOTREACHED();
    return false;
  }
  *block_index = model_metadata_.output().semantic_role().objective_block_index(
      index_of_best_role);
  return true;
}

bool AutofillAssistantModelExecutor::GetObjective(
    const std::vector<float>& output_objective,
    int block_index,
    int* objective) const {
  if (block_index + 1 >= model_metadata_.output().objective().blocks_size()) {
    NOTREACHED();
    return false;
  }
  auto block_start = output_objective.begin() +
                     model_metadata_.output().objective().blocks(block_index);
  auto block_end = output_objective.begin() +
                   model_metadata_.output().objective().blocks(block_index + 1);
  size_t index_of_best_objective = std::distance(
      output_objective.begin(), std::max_element(block_start, block_end));
  if (index_of_best_objective >=
      static_cast<size_t>(
          model_metadata_.output().objective().classes_size())) {
    NOTREACHED();
    return false;
  }

  *objective =
      model_metadata_.output().objective().classes(index_of_best_objective);
  return true;
}

absl::optional<std::pair<int, int>> AutofillAssistantModelExecutor::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors) {
  // Check if we have an override for this execution and return that instead.
  if (overrides_result_) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kAutofillAssistantDebugAnnotateDom)) {
      VLOG(3) << "Found override, using (role: " << overrides_result_->first
              << ", objective: " << overrides_result_->second << ")";
    }
    // Cleanup the result in case this executor is reused.
    std::pair<int, int> result = *overrides_result_;
    overrides_result_.reset();
    return result;
  }
  if (output_tensors.size() < 2u) {
    NOTREACHED() << "Output Tensors mismatch.";
    return absl::nullopt;
  }
  std::vector<float> output_role;
  absl::Status role_status = tflite::task::core::PopulateVector<float>(
      output_tensors[0], &output_role);
  if (!role_status.ok()) {
    return absl::nullopt;
  }
  std::vector<float> output_objective;
  absl::Status objective_status = tflite::task::core::PopulateVector<float>(
      output_tensors[1], &output_objective);
  if (!objective_status.ok()) {
    return absl::nullopt;
  }

  size_t index_of_best_role;
  if (!GetIndexOfBestRole(output_role, &index_of_best_role)) {
    return absl::nullopt;
  }
  if (index_of_best_role >=
      static_cast<size_t>(
          model_metadata_.output().semantic_role().classes_size())) {
    NOTREACHED();
    return absl::nullopt;
  }
  int semantic_role =
      model_metadata_.output().semantic_role().classes(index_of_best_role);
  if (semantic_role == 0) {
    return std::pair<int, int>(semantic_role, 0);
  }

  int block_index;
  if (!GetBlockIndex(output_role, index_of_best_role, &block_index)) {
    return absl::nullopt;
  }
  int objective;
  if (!GetObjective(output_objective, block_index, &objective)) {
    return absl::nullopt;
  }

  return std::pair<int, int>(semantic_role, objective);
}

void AutofillAssistantModelExecutor::Tokenize(
    const std::u16string& input,
    tflite::support::text::tokenizer::RegexTokenizer* tokenizer,
    const int feature_index,
    SparseMap& output_map) {
  auto result =
      tokenizer->Tokenize(base::UTF16ToUTF8(base::i18n::ToUpper(input)));
  for (const auto& token : result.subwords) {
    int index;
    if (tokenizer->LookupId(token, &index)) {
      output_map[std::make_pair(feature_index, index)]++;
    }
  }
}

AutofillAssistantModelExecutor::SparseVector
AutofillAssistantModelExecutor::TokenizeSignalsToSparseVector(
    const blink::AutofillAssistantNodeSignals& node_signals) {
  SparseMap sparse_map;

  DCHECK(tags_tokenizer_);
  Tokenize(node_signals.node_features.html_tag.Utf16(), tags_tokenizer_.get(),
           /* feature_index= */ 0, sparse_map);
  DCHECK(types_tokenizer_);
  Tokenize(node_signals.node_features.type.Utf16(), types_tokenizer_.get(),
           /* feature_index= */ 1, sparse_map);
  DCHECK(text_tokenizer_);
  Tokenize(node_signals.node_features.invisible_attributes.Utf16(),
           text_tokenizer_.get(), /* feature_index= */ 2, sparse_map);
  for (const auto& text : node_signals.node_features.text) {
    Tokenize(text.Utf16(), text_tokenizer_.get(), /* feature_index= */ 2,
             sparse_map);
  }
  for (const auto& text : node_signals.label_features.text) {
    Tokenize(text.Utf16(), text_tokenizer_.get(), /* feature_index= */ 3,
             sparse_map);
  }
  for (const auto& text : node_signals.context_features.header_text) {
    Tokenize(text.Utf16(), text_tokenizer_.get(), /* feature_index= */ 4,
             sparse_map);
  }
  Tokenize(node_signals.context_features.form_type.Utf16(),
           text_tokenizer_.get(), /* feature_index= */ 4, sparse_map);

  SparseVector sparse_vector;
  for (const auto& entry : sparse_map) {
    sparse_vector.emplace_back(entry);
  }
  return sparse_vector;
}

}  // namespace autofill_assistant
