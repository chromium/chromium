// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/timer/elapsed_timer.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_value_utils.h"
#include "components/optimization_guide/core/model_execution/redactor.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_util.h"

namespace optimization_guide {

OnDeviceModelFeatureAdapter::OnDeviceModelFeatureAdapter(
    proto::OnDeviceModelExecutionFeatureConfig&& config)
    : config_(config) {
  if (config_.has_output_config() &&
      config_.output_config().has_redact_rules() &&
      config_.output_config().redact_rules().fields_to_check_size() &&
      !config_.output_config().redact_rules().rules().empty()) {
    redactor_ = Redactor::FromProto(config.output_config().redact_rules());
  }
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

std::optional<proto::Any> OnDeviceModelFeatureAdapter::ConstructOutputMetadata(
    const std::string& output) const {
  if (!config_.has_output_config()) {
    return std::nullopt;
  }
  auto output_config = config_.output_config();

  return SetProtoValue(output_config.proto_type(), output_config.proto_field(),
                       output);
}

RedactResult OnDeviceModelFeatureAdapter::Redact(
    const google::protobuf::MessageLite& last_message,
    std::string& current_response) const {
  if (!redactor_) {
    return RedactResult::kContinue;
  }
  auto redact_string_input = GetStringToCheckForRedacting(last_message);
  base::ElapsedTimer elapsed_timer;
  auto redact_result = redactor_->Redact(redact_string_input, current_response);
  base::UmaHistogramMicrosecondsTimes(
      base::StrCat({"OptimizationGuide.ModelExecution.TimeToProcessRedactions.",
                    GetStringNameForModelExecutionFeature(config_.feature())}),
      elapsed_timer.Elapsed());
  return redact_result;
}

}  // namespace optimization_guide
