// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_METRICS_PROVIDER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace performance_manager {

// A metrics provider to add some performance manager related metrics to the UMA
// protos on each upload.
class MetricsProvider : public metrics::MetricsProvider {
 public:
  enum class EfficiencyMode {
    // No efficiency mode for the entire upload window
    kNormal = 0,
    // In high efficiency mode for the entire upload window
    kHighEfficiency = 1,
    // In battery saver mode for the entire upload window
    kBatterySaver = 2,
    // Both modes enabled for the entire upload window
    kBoth = 3,
    // The modes were changed during the upload window
    kMixed = 4,
    // Max value, used in UMA histograms macros
    kMaxValue = kMixed
  };

  explicit MetricsProvider(PrefService* local_state);
  ~MetricsProvider() override;

  // metrics::MetricsProvider:
  // This is only called from UMA code but is public for testing.
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  void OnEfficiencyModeChanged();
  EfficiencyMode ComputeCurrentMode() const;

  PrefChangeRegistrar pref_change_registrar_;
  PrefService* const local_state_;
  EfficiencyMode current_mode_ = EfficiencyMode::kNormal;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_METRICS_PROVIDER_H_
