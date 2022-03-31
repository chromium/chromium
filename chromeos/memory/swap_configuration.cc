// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/memory/swap_configuration.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/dbus/resourced/resourced_client.h"

namespace chromeos {

const base::Feature kCrOSTuneMinFilelist{"CrOSTuneMinFilelist",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int> kCrOSMinFilelistMb{&kCrOSTuneMinFilelist,
                                                 "CrOSMinFilelistMb", -1};

const base::Feature kCrOSTuneRamVsSwapWeight{"CrOSTuneRamVsSwapWeight",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int> kCrOSRamVsSwapWeight{&kCrOSTuneRamVsSwapWeight,
                                                   "CrOSRamVsSwapWeight", -1};

const base::Feature kCrOSTuneExtraFree{"CrOSTuneExtraFree",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int> kCrOSExtraFreeMb{&kCrOSTuneExtraFree,
                                               "CrOSExtraFreeMb", -1};

const base::Feature kCrOSMemoryPressureSignalStudy{
    "ChromeOSMemoryPressureSignalStudy", base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int>
    kCrOSMemoryPressureSignalStudyCriticalThresholdPrecentageBps{
        &kCrOSMemoryPressureSignalStudy, "critical_threshold_percentage", 520};

const base::FeatureParam<int>
    kCrOSMemoryPressureSignalStudyModerateThresholdPrecentageBps{
        &kCrOSMemoryPressureSignalStudy, "moderate_threshold_percentage", 4000};

namespace {

constexpr const char kMinFilelist[] = "min_filelist";
constexpr const char kRamVsSwapWeight[] = "ram_vs_swap_weight";
constexpr const char kExtraFree[] = "extra_free";

void OnSwapParameterSet(std::string parameter,
                        absl::optional<std::string> res) {
  LOG_IF(ERROR, !res.has_value())
      << "Setting swap paramter " << parameter << " failed.";
  LOG_IF(ERROR, res.has_value() && !res.value().empty())
      << "Setting swap parameter " << parameter
      << " returned error: " << res.value();
}

void ConfigureMinFilelistIfEnabled() {
  if (!base::FeatureList::IsEnabled(kCrOSTuneMinFilelist)) {
    return;
  }

  chromeos::DebugDaemonClient* debugd_client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  CHECK(debugd_client);

  int min_mb = kCrOSMinFilelistMb.Get();
  if (min_mb < 0) {
    LOG(ERROR) << "Min Filelist MB is enabled with an invalid value: "
               << min_mb;
    return;
  }

  VLOG(1) << "Setting min filelist to " << min_mb << "MB";
  debugd_client->SetSwapParameter(
      kMinFilelist, min_mb, base::BindOnce(&OnSwapParameterSet, kMinFilelist));
}

void ConfigureRamVsSwapWeightIfEnabled() {
  if (!base::FeatureList::IsEnabled(kCrOSTuneRamVsSwapWeight))
    return;

  chromeos::DebugDaemonClient* debugd_client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  CHECK(debugd_client);

  int swap_weight = kCrOSRamVsSwapWeight.Get();
  if (swap_weight < 0) {
    LOG(ERROR) << "Ram vs Swap weight must be greater than or equal to 0, "
                  "invalid value: "
               << swap_weight;
    return;
  }

  VLOG(1) << "Setting ram vs swap weight to: " << swap_weight;
  debugd_client->SetSwapParameter(
      kRamVsSwapWeight, swap_weight,
      base::BindOnce(&OnSwapParameterSet, kRamVsSwapWeight));
}

void ConfigureExtraFreeIfEnabled() {
  if (!base::FeatureList::IsEnabled(kCrOSTuneExtraFree))
    return;

  chromeos::DebugDaemonClient* debugd_client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  CHECK(debugd_client);

  int extra_free = kCrOSExtraFreeMb.Get();
  if (extra_free < 0) {
    LOG(ERROR)
        << "Extra free must be greater than or equal to 0, invalid value: "
        << extra_free;
    return;
  }

  VLOG(1) << "Setting extra_free mb to: " << extra_free;
  debugd_client->SetSwapParameter(
      kExtraFree, extra_free, base::BindOnce(&OnSwapParameterSet, kExtraFree));
}

void OnMemoryMarginsSet(bool result, uint64_t critical, uint64_t moderate) {
  if (!result) {
    LOG(ERROR) << "Unable to set critical memory margins via resourced";
    return;
  }

  LOG(WARNING) << "Set memory margins via resourced to: " << critical
               << "KB and " << moderate << "KB";
}

void ConfigureResourcedPressureThreshold() {
  if (!chromeos::ResourcedClient::Get()) {
    return;
  }

  if (base::FeatureList::IsEnabled(kCrOSMemoryPressureSignalStudy)) {
    // We need to send a debus message to resourced with the critical threshold
    // value (in bps).
    int critical_bps =
        kCrOSMemoryPressureSignalStudyCriticalThresholdPrecentageBps.Get();
    int moderate_bps =
        kCrOSMemoryPressureSignalStudyModerateThresholdPrecentageBps.Get();

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
    chromeos::ResourcedClient::Get()->SetMemoryMarginsBps(
        critical_bps, moderate_bps, base::BindOnce(&OnMemoryMarginsSet));
  }
}

}  // namespace

void ConfigureSwap() {
  ConfigureResourcedPressureThreshold();
  ConfigureExtraFreeIfEnabled();
  ConfigureRamVsSwapWeightIfEnabled();
  ConfigureMinFilelistIfEnabled();
}

}  // namespace chromeos
