// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/memory/kstaled.h"

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"

namespace ash {

namespace {

constexpr char kMGLRUEnableFile[] = "/sys/kernel/mm/lru_gen/enabled";

// KernelSupportsKstaled will check if the kernel supports mg lru, this is as
// easy as looking for the presences of the enable file.
bool KernelSupportsKstaled() {
  static const bool supported_mglru =
      base::PathExists(base::FilePath(kMGLRUEnableFile));
  return supported_mglru;
}

void OnRatioSet(bool success) {
  if (!success) {
    LOG(ERROR) << "Unable to configure kstaled";
    return;
  }

  VLOG(1) << "Debugd configured kstaled with value: " << kKstaledRatio.Get();
}

}  // namespace

BASE_FEATURE(kKstaled, "KstaledSwap", base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kKstaledRatio = {&kKstaled, "KstaledRatio", 1};

// InitializeKstaled will attempt to configure kstaled with the experimental
// parameters for this user.
void InitializeKstaled() {
  bool feature_enabled = base::FeatureList::IsEnabled(kKstaled);
  if (!feature_enabled) {
    VLOG(1) << "Kstaled is disabled";
    return;
  }

  if (!KernelSupportsKstaled()) {
    // Only log an error when we're running on REAL CrOS without kernel
    // support.
    LOG_IF(ERROR, base::SysInfo::IsRunningOnChromeOS())
        << "Unable to configure kstaled: no kernel support";
    return;
  }

  int feature_ratio = kKstaledRatio.Get();
  if (feature_ratio < 0 || feature_ratio > 1) {
    LOG(ERROR) << "Invalid value set for feature ratio, it can be 0 or 1 only";
    return;
  }

  DebugDaemonClient* debugd_client = DebugDaemonClient::Get();
  DCHECK(debugd_client);

  debugd_client->SetKstaledRatio(static_cast<uint8_t>(feature_ratio),
                                 base::BindOnce(&OnRatioSet));
}

}  // namespace ash
