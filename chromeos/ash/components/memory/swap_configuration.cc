// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/memory/swap_configuration.h"

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "chromeos/ash/components/dbus/resourced/resourced_client.h"

namespace ash {

// There are going to be 2 separate experiments for memory pressure signal, one
// for ARC enabled users and one for ARC disabled users.
BASE_FEATURE(kCrOSMemoryPressureSignalStudyNonArc,
             "ChromeOSMemoryPressureSignalStudyNonArc",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int> kCrOSMemoryPressureSignalStudyNonArcModerateBps{
    &kCrOSMemoryPressureSignalStudyNonArc, "moderate_threshold_percentage",
    4000};

const base::FeatureParam<int> kCrOSMemoryPressureSignalStudyNonArcCriticalBps{
    &kCrOSMemoryPressureSignalStudyNonArc, "critical_threshold_percentage",
    1500};

const base::FeatureParam<int>
    kCrOSMemoryPressureSignalStudyNonArcCriticalProtectedBps{
        &kCrOSMemoryPressureSignalStudyNonArc,
        "critical_protected_threshold_percentage", 750};

BASE_FEATURE(kCrOSMemoryPressureSignalStudyArc,
             "ChromeOSMemoryPressureSignalStudyArc",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int> kCrOSMemoryPressureSignalStudyArcModerateBps{
    &kCrOSMemoryPressureSignalStudyArc, "moderate_threshold_percentage", 4000};

const base::FeatureParam<int> kCrOSMemoryPressureSignalStudyArcCriticalBps{
    &kCrOSMemoryPressureSignalStudyArc, "critical_threshold_percentage", 800};

const base::FeatureParam<int>
    kCrOSMemoryPressureSignalStudyArcCriticalProtectedBps{
        &kCrOSMemoryPressureSignalStudyArc,
        "critical_protected_threshold_percentage", 800};

namespace {

void ConfigureResourcedPressureThreshold(bool arc_enabled) {
  if (!ResourcedClient::Get()) {
    return;
  }

  bool experiment_enabled = false;
  uint32_t moderate_bps = 0;
  uint32_t critical_bps = 0;
  uint32_t critical_protected_bps = 0;
  if (arc_enabled) {
    experiment_enabled =
        base::FeatureList::IsEnabled(kCrOSMemoryPressureSignalStudyArc);
    if (experiment_enabled) {
      moderate_bps = kCrOSMemoryPressureSignalStudyArcModerateBps.Get();
      critical_bps = kCrOSMemoryPressureSignalStudyArcCriticalBps.Get();
      critical_protected_bps =
          kCrOSMemoryPressureSignalStudyArcCriticalProtectedBps.Get();
    }
  } else {
    experiment_enabled =
        base::FeatureList::IsEnabled(kCrOSMemoryPressureSignalStudyNonArc);
    if (experiment_enabled) {
      moderate_bps = kCrOSMemoryPressureSignalStudyNonArcModerateBps.Get();
      critical_bps = kCrOSMemoryPressureSignalStudyNonArcCriticalBps.Get();
      critical_protected_bps =
          kCrOSMemoryPressureSignalStudyNonArcCriticalProtectedBps.Get();
    }
  }

  if (experiment_enabled) {
    // We need to send a debus message to resourced with the critical threshold
    // value (in bps).
    if (critical_bps < 520 || critical_bps > 2500 || moderate_bps > 7500 ||
        moderate_bps < 2000 || critical_bps >= moderate_bps) {
      // To avoid a potentially catastrophic misconfiguration we
      // only allow critical values between 5.2% and 25%, moderate between 20%
      // and 75%, and moderate must be greater than critical.
      LOG(ERROR) << "Invalid values specified for memory thresholds: "
                 << critical_bps << " and " << moderate_bps;
      return;
    }

    LOG(WARNING) << "Overriding memory thresholds with values "
                 << (critical_bps / 100.0) << "% and " << (moderate_bps / 100.0)
                 << "%";
    ResourcedClient::MemoryMargins margins = {
        .moderate_bps = moderate_bps,
        .critical_bps = critical_bps,
        .critical_protected_bps = critical_protected_bps};
    ResourcedClient::Get()->SetMemoryMargins(margins);
  }
}

}  // namespace

void ConfigureSwap(bool arc_enabled) {
  ConfigureResourcedPressureThreshold(arc_enabled);
}

}  // namespace ash
