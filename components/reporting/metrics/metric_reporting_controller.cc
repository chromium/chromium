// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/metric_reporting_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "components/reporting/metrics/reporting_settings.h"

namespace reporting {

MetricReportingController::MetricReportingController(
    ReportingSettings* reporting_settings,
    const std::string& setting_path,
    bool setting_enabled_default_value)
    : reporting_settings_(reporting_settings),
      setting_path_(setting_path),
      setting_enabled_default_value_(setting_enabled_default_value) {
  UpdateSetting();

  subscription_ = reporting_settings_->AddSettingsObserver(
      setting_path_,
      base::BindRepeating(&MetricReportingController::UpdateSetting,
                          base::Unretained(this)));
}

MetricReportingController::~MetricReportingController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool MetricReportingController::IsEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return setting_enabled_;
}

void MetricReportingController::SetSettingUpdateCb(
    base::RepeatingClosure on_setting_enabled,
    base::RepeatingClosure on_setting_disabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  on_setting_enabled_ = std::move(on_setting_enabled);
  on_setting_disabled_ = std::move(on_setting_disabled);
  if (setting_enabled_) {
    on_setting_enabled_.Run();
  }
}

void MetricReportingController::UpdateSetting() {
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::OnceClosure update_setting_cb = base::BindOnce(
      &MetricReportingController::UpdateSetting, weak_factory_.GetWeakPtr());
  bool trusted = reporting_settings_->PrepareTrustedValues(
      base::BindPostTaskToCurrentDefault(std::move(update_setting_cb)));
  if (!trusted) {
    return;
  }

  bool new_setting_enabled = setting_enabled_default_value_;
  reporting_settings_->GetReportingEnabled(setting_path_, &new_setting_enabled);

  if (setting_enabled_ != new_setting_enabled) {
    setting_enabled_ = new_setting_enabled;
    if (setting_enabled_) {
      on_setting_enabled_.Run();
    } else {
      on_setting_disabled_.Run();
    }
  }
}
}  // namespace reporting
