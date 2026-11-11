// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_telemetry_logger.h"

#include <string>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_names.h"

namespace optimization_guide {

OnDeviceRequestTelemetryLogger::OnDeviceRequestTelemetryLogger(
    mojom::OnDeviceFeature feature)
    : feature_(feature) {}

OnDeviceRequestTelemetryLogger::OnDeviceRequestTelemetryLogger(
    mojom::OnDeviceFeature feature,
    OnDeviceBaseModel base_model)
    : feature_(feature), base_model_(base_model) {}

OnDeviceRequestTelemetryLogger::~OnDeviceRequestTelemetryLogger() {
  if (!base_model_.has_value()) {
    return;
  }
  if (result_ == Result::kUsedOnDevice) {
    base::UmaHistogramEnumeration(
        "OptimizationGuide.ModelExecution.OnDeviceModelCompleted",
        *base_model_);
    base::UmaHistogramEnumeration(
        base::StrCat(
            {"OptimizationGuide.ModelExecution.OnDeviceModelCompleted.",
             GetVariantName(feature_)}),
        *base_model_);
  }
  std::string feature_histogram = base::StrCat(
      {"OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.",
       GetVariantName(feature_)});
  base::UmaHistogramEnumeration(feature_histogram, result_);
  base::UmaHistogramEnumeration(
      base::StrCat({feature_histogram, ".",
                    ConvertBaseModelToUmaModelName(*base_model_)}),
      result_);
}

void OnDeviceRequestTelemetryLogger::StartTelemetryTimer() {
  CHECK(start_time_.is_null());
  start_time_ = base::TimeTicks::Now();
}

void OnDeviceRequestTelemetryLogger::RecordFirstResponse() {
  CHECK(!start_time_.is_null());
  if (!first_response_time_.is_null()) {
    return;  // Already recorded
  }
  first_response_time_ = base::TimeTicks::Now();
  base::TimeDelta time_to_first_response = first_response_time_ - start_time_;
  base::UmaHistogramMediumTimes(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceFirstResponseTime.",
           GetVariantName(feature_)}),
      time_to_first_response);
}

void OnDeviceRequestTelemetryLogger::RecordContextTime() {
  CHECK(!start_time_.is_null());
  if (!context_time_.is_null()) {
    return;
  }
  context_time_ = base::TimeTicks::Now();
}

void OnDeviceRequestTelemetryLogger::RecordCompletion(uint32_t num_tokens) {
  CHECK(!start_time_.is_null());
  completion_time_ = base::TimeTicks::Now();
  base::TimeDelta time_to_completion = completion_time_ - start_time_;

  base::UmaHistogramMediumTimes(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceResponseCompleteTime.",
           GetVariantName(feature_)}),
      time_to_completion);

  base::UmaHistogramCounts10000(
      base::StrCat(
          {"OptimizationGuide.ModelExecution.OnDeviceResponseCompleteTokens.",
           GetVariantName(feature_)}),
      num_tokens);

  if (num_tokens > 0 && !first_response_time_.is_null()) {
    base::UmaHistogramTimes(
        base::StrCat({"OptimizationGuide.ModelExecution."
                      "OnDeviceResponseTokensTimeToNextToken.",
                      GetVariantName(feature_)}),
        (completion_time_ - first_response_time_) / num_tokens);
  }
}

void OnDeviceRequestTelemetryLogger::RecordDestroyedWhileWaiting() {
  set_result(Result::kDestroyedWhileWaitingForResponse);
  if (!start_time_.is_null()) {
    base::UmaHistogramMediumTimes(
        base::StrCat({"OptimizationGuide.ModelExecution."
                      "OnDeviceDestroyedWhileWaitingForResponseTime.",
                      GetVariantName(feature_)}),
        base::TimeTicks::Now() - start_time_);
  }
}

base::TimeDelta OnDeviceRequestTelemetryLogger::GetTimeToFirstResponse() const {
  CHECK(!first_response_time_.is_null());
  return first_response_time_ - start_time_;
}

base::TimeDelta OnDeviceRequestTelemetryLogger::GetTimeToContextProcessing()
    const {
  CHECK(!context_time_.is_null());
  return context_time_ - start_time_;
}

base::TimeDelta OnDeviceRequestTelemetryLogger::GetTimeToCompletion() const {
  CHECK(!completion_time_.is_null());
  return completion_time_ - start_time_;
}

base::TimeTicks OnDeviceRequestTelemetryLogger::GetStartTime() const {
  return start_time_;
}

}  // namespace optimization_guide
