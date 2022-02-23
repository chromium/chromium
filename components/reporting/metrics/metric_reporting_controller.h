// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_METRIC_REPORTING_CONTROLLER_H_
#define COMPONENTS_REPORTING_METRICS_METRIC_REPORTING_CONTROLLER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

namespace reporting {

class ReportingSettings;

// Control reporter intilaization and cleanup based on the reporting setting
// specified by the setting path.
class MetricReportingController {
 public:
  MetricReportingController(
      ReportingSettings* reporting_settings,
      const std::string& setting_path,
      bool setting_enabled_default_value,
      base::RepeatingClosure on_setting_enabled,
      base::RepeatingClosure on_setting_disabled = base::DoNothing());

  MetricReportingController(const MetricReportingController& other) = delete;
  MetricReportingController& operator=(const MetricReportingController& other) =
      delete;

  ~MetricReportingController();

 private:
  void UpdateSetting();

  const raw_ptr<ReportingSettings> reporting_settings_;
  const std::string setting_path_;
  const bool setting_enabled_default_value_;
  const base::RepeatingClosure on_setting_enabled_;
  const base::RepeatingClosure on_setting_disabled_;

  bool setting_enabled_ = false;

  base::CallbackListSubscription subscription_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<MetricReportingController> weak_factory_{this};
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_METRICS_METRIC_REPORTING_CONTROLLER_H_
