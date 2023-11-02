// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/metric_reporting_controller.h"

#include "base/bind.h"
#include "base/task/bind_post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/reporting/metrics/reporting_settings.h"

namespace reporting {

MetricReportingController::MetricReportingController(
    ReportingSettings* reporting_settings,
    const std::string& setting_path,
    bool setting_enabled_default_value,
    base::RepeatingClosure on_setting_enabled,
    base::RepeatingClosure on_setting_disabled)
    : reporting_settings_(reporting_settings),
      setting_path_(setting_path),
      setting_enabled_default_value_(setting_enabled_default_value),
      on_setting_enabled_(std::move(on_setting_enabled)),
      on_setting_disabled_(std::move(on_setting_disabled)) {
  UpdateSetting();

  subscription_ = reporting_settings_->AddSettingsObserver(
      setting_path_,
      base::BindRepeating(&MetricReportingController::UpdateSetting,
                          base::Unretained(this)));
}

MetricReportingController::~MetricReportingController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MetricReportingController::UpdateSetting() {
  CHECK(base::SequencedTaskRunnerHandle::IsSet());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::OnceClosure update_setting_cb = base::BindOnce(
      &MetricReportingController::UpdateSetting, weak_factory_.GetWeakPtr());
  bool trusted = reporting_settings_->PrepareTrustedValues(base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(), std::move(update_setting_cb)));
  if (!trusted) {
    return;
  }

  bool new_setting_enabled = setting_enabled_default_value_;
  reporting_settings_->GetBoolean(setting_path_, &new_setting_enabled);

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
