// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/metric_rate_controller.h"

#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "components/reporting/metrics/reporting_settings.h"

namespace reporting {

// static
BASE_FEATURE(kEnableTelemetryTestingRates,
             "EnableTelemetryTestingRates",
             base::FEATURE_DISABLED_BY_DEFAULT);

MetricRateController::MetricRateController(
    base::RepeatingClosure task,
    ReportingSettings* reporting_settings,
    const std::string& rate_setting_path,
    base::TimeDelta default_rate,
    int rate_unit_to_ms)
    : task_(std::move(task)),
      reporting_settings_(reporting_settings),
      rate_setting_path_(rate_setting_path),
      rate_(default_rate),
      default_rate_(default_rate),
      rate_unit_to_ms_(rate_unit_to_ms) {}

MetricRateController::~MetricRateController() = default;

void MetricRateController::Start() {
  bool trusted = reporting_settings_->PrepareTrustedValues(base::DoNothing());
  if (trusted) {
    int current_rate;
    if (reporting_settings_->GetInteger(rate_setting_path_, &current_rate)) {
      rate_ = base::Milliseconds(current_rate * rate_unit_to_ms_);
    }
  }
  // Use default rate if setting rate is too high or if testing flag is enabled
  // to have a higher collection and reporting rate set using the default rate.
  if (rate_ < base::Milliseconds(1) ||
      base::FeatureList::IsEnabled(kEnableTelemetryTestingRates)) {
    if (default_rate_ < base::Milliseconds(1)) {
      return;
    }
    rate_ = default_rate_;
  }
  timer_.Start(FROM_HERE, rate_, this, &MetricRateController::Run);
}

void MetricRateController::Stop() {
  timer_.Stop();
}

void MetricRateController::Run() {
  task_.Run();
  Start();
}
}  // namespace reporting
