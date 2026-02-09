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
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_value_utils.h"
#include "components/optimization_guide/core/model_execution/response_parser.h"
#include "components/optimization_guide/core/model_execution/response_parser_factory.h"
#include "components/optimization_guide/core/model_execution/simple_response_parser.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_util.h"

namespace optimization_guide {

OnDeviceModelFeatureAdapter::OnDeviceModelFeatureAdapter(
    proto::OnDeviceModelExecutionFeatureConfig config,
    ResponseParserFactory response_parser_factory)
    : config_(std::move(config)),
      parser_(response_parser_factory
                  ? response_parser_factory.Run(config_.output_config())
                  : CreateResponseParser(config_.output_config())) {
  // Set limits values in `token_limits_`.
  auto& input_config = config_.input_config();
  auto& output_config = config_.output_config();
  uint32_t max_tokens = kOnDeviceModelMaxTokens;
  token_limits_.max_tokens = max_tokens;
  token_limits_.min_context_tokens =
      input_config.has_min_context_tokens()
          ? std::min(input_config.min_context_tokens(), max_tokens)
          : 1024;
  token_limits_.max_context_tokens =
      input_config.has_max_context_tokens()
          ? std::min(input_config.max_context_tokens(), max_tokens)
          : 8192;
  token_limits_.max_execute_tokens =
      input_config.has_max_execute_tokens()
          ? std::min(input_config.max_execute_tokens(), max_tokens)
          : 1024;
  token_limits_.max_output_tokens =
      output_config.has_max_output_tokens()
          ? std::min(output_config.max_output_tokens(), max_tokens)
          : 1024;
}

OnDeviceModelFeatureAdapter::~OnDeviceModelFeatureAdapter() = default;

std::optional<SubstitutionResult>
OnDeviceModelFeatureAdapter::ConstructInputString(
    MultimodalMessageReadView request,
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



bool OnDeviceModelFeatureAdapter::ShouldParseResponse(
    ResponseCompleteness completeness) const {
  // Streaming responses are incompatible with redaction.
  return completeness == ResponseCompleteness::kComplete ||
         !parser_->SuppressParsingIncompleteResponse();
}

void OnDeviceModelFeatureAdapter::ParseResponse(
    const MultimodalMessage& request,
    const std::string& model_response,
    size_t previous_response_pos,
    ResponseParser::ResultCallback callback) const {
  if (!parser_) {
    std::move(callback).Run(base::unexpected(ResponseParsingError::kFailed));
    return;
  }

  parser_->ParseAsync(model_response.substr(previous_response_pos),
                      std::move(callback));
}

std::optional<proto::TextSafetyRequest>
OnDeviceModelFeatureAdapter::ConstructTextSafetyRequest(
    MultimodalMessageReadView request,
    const std::string& text) const {
  if (!config_.has_text_safety_fallback_config()) {
    return std::nullopt;
  }

  auto& text_safety_fallback_config = config_.text_safety_fallback_config();

  proto::TextSafetyRequest text_safety_request;
  text_safety_request.set_text(text);

  if (text_safety_fallback_config.has_input_url_proto_field()) {
    std::optional<proto::Value> input_url_value =
        request.GetValue(text_safety_fallback_config.input_url_proto_field());
    if (input_url_value) {
      const std::string string_value = GetStringFromValue(*input_url_value);
      text_safety_request.set_url(string_value);
    } else {
      return std::nullopt;
    }
  }

  return text_safety_request;
}

SamplingParamsConfig OnDeviceModelFeatureAdapter::GetSamplingParamsConfig()
    const {
  if (!config_.has_sampling_params()) {
    // Returns default value if the sampling params are not configured.
    return SamplingParamsConfig{
        .default_top_k = uint32_t(features::GetOnDeviceModelDefaultTopK()),
        .default_temperature =
            float(features::GetOnDeviceModelDefaultTemperature()),
    };
  }

  return SamplingParamsConfig{
      .default_top_k = config_.sampling_params().top_k(),
      .default_temperature = config_.sampling_params().temperature(),
  };
}

SamplingParams OnDeviceModelFeatureAdapter::GetDefaultSamplingParams() const {
  SamplingParamsConfig feature_params = GetSamplingParamsConfig();
  return SamplingParams{.top_k = feature_params.default_top_k,
                        .temperature = feature_params.default_temperature};
}

const proto::Any& OnDeviceModelFeatureAdapter::GetFeatureMetadata() const {
  return config_.feature_metadata();
}

const TokenLimits& OnDeviceModelFeatureAdapter::GetTokenLimits() const {
  return token_limits_;
}

on_device_model::mojom::ResponseConstraintPtr
OnDeviceModelFeatureAdapter::GetResponseConstraint() const {
  const auto& constraint = config_.output_config().response_constraint();
  switch (constraint.format_case()) {
    case proto::ResponseConstraint::kJsonSchema:
      return on_device_model::mojom::ResponseConstraint::NewJsonSchema(
          constraint.json_schema());
    case proto::ResponseConstraint::kRegex:
      return on_device_model::mojom::ResponseConstraint::NewRegex(
          constraint.regex());
    default:
      // Not configured, or not supported configuration.
      return nullptr;
  }
}

}  // namespace optimization_guide
