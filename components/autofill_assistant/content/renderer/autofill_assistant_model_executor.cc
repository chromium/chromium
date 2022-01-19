// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/content/renderer/autofill_assistant_model_executor.h"

#include "base/i18n/case_conversion.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "components/optimization_guide/core/execution_status.h"
#include "components/optimization_guide/core/tflite_op_resolver.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/metadata/cc/metadata_extractor.h"

namespace autofill_assistant {

AutofillAssistantModelExecutor::AutofillAssistantModelExecutor() = default;
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

absl::optional<std::string>
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
  DCHECK_GE(input_tensors.size(), 5u);
  std::vector<std::vector<float>> inputs;
  for (const auto* input_tensor : input_tensors) {
    if (!input_tensor->dims || input_tensor->dims->size < 1) {
      return false;
    }
    inputs.emplace_back(std::vector(input_tensor->dims->data[1], 0.0f));
  }

  DCHECK(tags_tokenizer_);
  Tokenize(node_signals.node_features.html_tag.Utf16(), tags_tokenizer_.get(),
           &inputs[0]);
  DCHECK(types_tokenizer_);
  Tokenize(node_signals.node_features.type.Utf16(), types_tokenizer_.get(),
           &inputs[1]);
  DCHECK(text_tokenizer_);
  Tokenize(node_signals.node_features.invisible_attributes.Utf16(),
           text_tokenizer_.get(), &inputs[2]);
  for (const auto& text : node_signals.node_features.text) {
    Tokenize(text.Utf16(), text_tokenizer_.get(), &inputs[2]);
  }
  for (const auto& text : node_signals.label_features.text) {
    Tokenize(text.Utf16(), text_tokenizer_.get(), &inputs[3]);
  }
  for (const auto& text : node_signals.context_features.header_text) {
    Tokenize(text.Utf16(), text_tokenizer_.get(), &inputs[4]);
  }

  for (size_t i = 0; i < inputs.size(); ++i) {
    absl::Status tensor_status =
        tflite::task::core::PopulateTensor<float>(inputs[i], input_tensors[i]);
    if (!tensor_status.ok()) {
      return false;
    }
  }
  return true;
}

// TODO(b/204841212): Implement this with use of ModelMetadata.
absl::optional<std::string> AutofillAssistantModelExecutor::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors) {
  static const base::NoDestructor<std::vector<std::string>> output_roles(
      {"UNKNOWN_ROLE", "NAME_FIRST", "NAME_LAST", "NAME_FULL", "ADDRESS_LINE1",
       "ADDRESS_LINE2", "CITY", "STATE", "COUNTRY", "POSTAL_CODE",
       "CREDIT_CARD_NUMBER", "CREDIT_CARD_EXP_MONTH",
       "CREDIT_CARD_VERIFICATION_CODE", "ORGANIZATION",
       "CREDIT_CARD_EXPIRATION", "PHONE_NUMBER", "USERNAME_OR_EMAIL",
       "CREDIT_CARD_EXP_YEAR"});

  DCHECK_GE(output_tensors.size(), 1u);
  std::vector<float> data;
  absl::Status vector_status =
      tflite::task::core::PopulateVector<float>(output_tensors[0], &data);
  if (!vector_status.ok()) {
    return absl::nullopt;
  }

  // TODO(b/204841212): The output is 24 floats, but we only care about the
  // the first [0-17].
  DCHECK_GE(data.size(), output_roles->size());
  int index = std::distance(
      data.begin(),
      std::max_element(data.begin(), data.begin() + output_roles->size()));
  return output_roles->at(index);
}

void AutofillAssistantModelExecutor::Tokenize(
    const std::u16string& input,
    tflite::support::text::tokenizer::RegexTokenizer* tokenizer,
    std::vector<float>* output) {
  auto result =
      tokenizer->Tokenize(base::UTF16ToUTF8(base::i18n::ToUpper(input)));
  for (const auto& token : result.subwords) {
    int index;
    if (tokenizer->LookupId(token, &index)) {
      if (static_cast<size_t>(index) >= output->size()) {
        NOTREACHED();
        continue;
      }
      ++output->at(index);
    }
  }
}

}  // namespace autofill_assistant
