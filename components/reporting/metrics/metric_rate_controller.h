// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_METRIC_RATE_CONTROLLER_H_
#define COMPONENTS_REPORTING_METRICS_METRIC_RATE_CONTROLLER_H_

#include <string>

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace reporting {

class ReportingSettings;

BASE_DECLARE_FEATURE(kEnableTelemetryTestingRates);

// Control reporting rate based on the reporting setting specified by the
// setting path. Rate refers to the time between two consecutive periodic metric
// collections, i.e., the period of the repeated metric collections.
class MetricRateController {
 public:
  // `rate_unit_to_ms` multiplied by the rate in the settings results in the
  // rate in milliseconds. It is used as a conversion helper for converting rate
  // setting value to milliseconds if the setting value is not represented in
  // milliseconds.
  MetricRateController(base::RepeatingClosure task,
                       ReportingSettings* reporting_settings,
                       const std::string& rate_setting_path,
                       base::TimeDelta default_rate,
                       int rate_unit_to_ms = 1);

  MetricRateController(const MetricRateController& other) = delete;
  MetricRateController& operator=(const MetricRateController& other) = delete;

  ~MetricRateController();

  void Start();

  void Stop();

 private:
  void Run();

  const base::RepeatingClosure task_;
  const raw_ptr<ReportingSettings> reporting_settings_;
  const std::string rate_setting_path_;
  base::TimeDelta rate_;
  const base::TimeDelta default_rate_;
  const int rate_unit_to_ms_;

  base::OneShotTimer timer_;
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_METRICS_METRIC_RATE_CONTROLLER_H_
