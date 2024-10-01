// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"

#include <optional>
#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_value_utils.h"
#include "components/optimization_guide/core/model_execution/redactor.h"
#include "components/optimization_guide/core/model_execution/response_parser.h"
#include "components/optimization_guide/core/model_execution/response_parser_registry.h"
#include "components/optimization_guide/core/model_execution/simple_response_parser.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_util.h"

namespace optimization_guide {

OnDeviceModelFeatureAdapter::OnDeviceModelFeatureAdapter(
    proto::OnDeviceModelExecutionFeatureConfig&& config)
    : config_(config),
      redactor_(Redactor::FromProto(config.output_config().redact_rules())),
      parser_(
          ResponseParserRegistry::Get().CreateParser(config_.output_config())) {
}

OnDeviceModelFeatureAdapter::~OnDeviceModelFeatureAdapter() = default;

std::string OnDeviceModelFeatureAdapter::GetStringToCheckForRedacting(
    const google::protobuf::MessageLite& message) const {
  for (const auto& proto_field :
       config_.output_config().redact_rules().fields_to_check()) {
    std::optional<proto::Value> value = GetProtoValue(message, proto_field);
    if (value) {
      const std::string string_value = GetStringFromValue(*value);
      if (!string_value.empty()) {
        return string_value;
      }
    }
  }
  return std::string();
}

std::optional<SubstitutionResult>
OnDeviceModelFeatureAdapter::ConstructInputString(
    const google::protobuf::MessageLite& request,
    bool want_input_context) const {
  if (!config_.has_input_config()) {
    return std::nullopt;
  }
  const auto input_config = config_.input_config();
  if (input_config.request_base_name() != request.GetTypeName()) {
    return std::nullopt;
  }
  return CreateSubstitutions(
      request, want_input_context ? input_config.input_context_substitutions()
                                  : input_config.execute_substitutions());
}

RedactResult OnDeviceModelFeatureAdapter::Redact(
    const google::protobuf::MessageLite& last_message,
    std::string& current_response) const {
  auto redact_string_input = GetStringToCheckForRedacting(last_message);
  base::ElapsedTimer elapsed_timer;
  auto redact_result = redactor_.Redact(redact_string_input, current_response);
  base::UmaHistogramMicrosecondsTimes(
      base::StrCat({"OptimizationGuide.ModelExecution.TimeToProcessRedactions.",
                    GetStringNameForModelExecutionFeature(config_.feature())}),
      elapsed_timer.Elapsed());
  return redact_result;
}

bool OnDeviceModelFeatureAdapter::ShouldParseResponse(bool is_complete) const {
  return is_complete || !parser_->SuppressParsingIncompleteResponse();
}

void OnDeviceModelFeatureAdapter::ParseResponse(
    const google::protobuf::MessageLite& request,
    const std::string& model_response,
    ResponseParser::ResultCallback callback) const {
  std::string redacted_response = model_response;
  auto redact_result = Redact(request, redacted_response);
  if (redact_result != RedactResult::kContinue) {
    std::move(callback).Run(
        base::unexpected(ResponseParsingError::kRejectedPii));
    return;
  }
  if (!parser_) {
    std::move(callback).Run(base::unexpected(ResponseParsingError::kFailed));
    return;
  }
  parser_->ParseAsync(redacted_response, std::move(callback));
}

std::optional<proto::TextSafetyRequest>
OnDeviceModelFeatureAdapter::ConstructTextSafetyRequest(
    const google::protobuf::MessageLite& request,
    const std::string& text) const {
  if (!config_.has_text_safety_fallback_config()) {
    return std::nullopt;
  }

  auto& text_safety_fallback_config = config_.text_safety_fallback_config();

  proto::TextSafetyRequest text_safety_request;
  text_safety_request.set_text(text);

  if (text_safety_fallback_config.has_input_url_proto_field()) {
    std::optional<proto::Value> input_url_value = GetProtoValue(
        request, text_safety_fallback_config.input_url_proto_field());
    if (input_url_value) {
      const std::string string_value = GetStringFromValue(*input_url_value);
      text_safety_request.set_url(string_value);
    } else {
      return std::nullopt;
    }
  }

  return text_safety_request;
}

std::optional<SamplingParams> OnDeviceModelFeatureAdapter::MaybeSamplingParams()
    const {
  if (!config_.has_sampling_params()) {
    return std::nullopt;
  }
  return SamplingParams{
      .top_k = config_.sampling_params().top_k(),
      .temperature = config_.sampling_params().temperature(),
  };
}

const proto::Any& OnDeviceModelFeatureAdapter::GetFeatureMetadata() const {
  return config_.feature_metadata();
}

}  // namespace optimization_guide
