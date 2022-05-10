// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/metrics/metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"

namespace performance_manager {

MetricsProvider::MetricsProvider(PrefService* local_state)
    : local_state_(local_state) {
  pref_change_registrar_.Init(local_state_);
  pref_change_registrar_.Add(
      performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled,
      base::BindRepeating(&MetricsProvider::OnEfficiencyModeChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      performance_manager::user_tuning::prefs::kBatterySaverModeEnabled,
      base::BindRepeating(&MetricsProvider::OnEfficiencyModeChanged,
                          base::Unretained(this)));

  current_mode_ = ComputeCurrentMode();
}

MetricsProvider::~MetricsProvider() = default;

void MetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  base::UmaHistogramEnumeration("PerformanceManager.UserTuning.EfficiencyMode",
                                current_mode_);

  // Set `current_mode_` to represent the state of the modes as they are now, so
  // that this mode is what is adequately reported at the next report, unless it
  // changes in the meantime.
  current_mode_ = ComputeCurrentMode();
}

void MetricsProvider::OnEfficiencyModeChanged() {
  EfficiencyMode new_mode = ComputeCurrentMode();

  // If the mode changes between UMA reports, mark it as Mixed for this
  // interval.
  if (current_mode_ != new_mode) {
    current_mode_ = EfficiencyMode::kMixed;
  }
}

MetricsProvider::EfficiencyMode MetricsProvider::ComputeCurrentMode() const {
  bool high_efficiency_enabled = local_state_->GetBoolean(
      performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled);
  bool battery_saver_enabled = local_state_->GetBoolean(
      performance_manager::user_tuning::prefs::kBatterySaverModeEnabled);

  if (high_efficiency_enabled && battery_saver_enabled) {
    return EfficiencyMode::kBoth;
  }

  if (high_efficiency_enabled) {
    return EfficiencyMode::kHighEfficiency;
  }

  if (battery_saver_enabled) {
    return EfficiencyMode::kBatterySaver;
  }

  return EfficiencyMode::kNormal;
}

}  // namespace performance_manager