// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/memory/swap_configuration.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"

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

namespace {

constexpr const char kMinFilelist[] = "min_filelist";
constexpr const char kRamVsSwapWeight[] = "ram_vs_swap_weight";
constexpr const char kExtraFree[] = "extra_free";

void OnSwapParameterSet(std::string parameter,
                        base::Optional<std::string> res) {
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

}  // namespace

void ConfigureSwap() {
  ConfigureExtraFreeIfEnabled();
  ConfigureRamVsSwapWeightIfEnabled();
  ConfigureMinFilelistIfEnabled();
}

}  // namespace chromeos
