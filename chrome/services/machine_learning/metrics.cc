// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/machine_learning/metrics.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"

namespace machine_learning {
namespace metrics {

const char kServiceRequested[] = "ChromeMLService.ServiceStatusWhenRequested";
const char kServiceLaunch[] = "ChromeMLService.ServiceLaunch";
const char kServiceNormalTermination[] =
    "ChromeMLService.ServiceNormalTermination";
const char kServiceCrash[] = "ChromeMLService.ServiceCrash";
const char kServiceAliveDuration[] = "ChromeMLService.ServiceAliveDuration";
const char kDecisionTreeModelLoadResult[] =
    "ChromeMLService.LoadModelResult.DecisionTreeModel";
const char kDecisionTreeModelPredictionResult[] =
    "ChromeMLService.PredictionResult.DecisionTreeModel";
const char kDecisionTreeModelValidationLatency[] =
    "ChromeMLService.ValidationLatency.DecisionTreeModel";
const char kDecisionTreeModelEvaluationLatency[] =
    "ChromeMLService.EvaluationLatency.DecisionTreeModel";

void LogServiceRequested(MLServiceRequestStatus status) {
  UMA_HISTOGRAM_ENUMERATION(kServiceRequested, status);
}

void LogServiceLaunch() {
  UMA_HISTOGRAM_BOOLEAN(kServiceLaunch, true);
}

void LogServiceNormalTermination() {
  UMA_HISTOGRAM_BOOLEAN(kServiceNormalTermination, true);
}

void LogServiceCrash() {
  UMA_HISTOGRAM_BOOLEAN(kServiceCrash, true);
}

void LogServiceAliveDuration(base::TimeDelta time) {
  UMA_HISTOGRAM_CUSTOM_TIMES(kServiceAliveDuration, time,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromSeconds(30), 50);
}

ScopedLatencyRecorder::ScopedLatencyRecorder(const std::string& metric_name)
    : metric_name_(metric_name), start_time_(base::TimeTicks::Now()) {}

ScopedLatencyRecorder::~ScopedLatencyRecorder() {
  RecordTimeElapsed();
}

void ScopedLatencyRecorder::RecordTimeElapsed() {
  if (recorded_)
    return;

  // Cannot use macros here since |metrics_name_| may be different.
  base::UmaHistogramCustomTimes(metric_name_,
                                base::TimeTicks::Now() - start_time_,
                                base::TimeDelta::FromMilliseconds(1),
                                base::TimeDelta::FromMilliseconds(2000), 50);
  recorded_ = true;
}

}  // namespace metrics
}  // namespace machine_learning
