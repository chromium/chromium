// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/memory/mglru.h"

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "chromeos/ash/components/dbus/swap_management/swap_management_client.h"

namespace ash {

namespace {

constexpr char kMGLRUEnableFile[] = "/sys/kernel/mm/lru_gen/enabled";

// KernelSupportsMGLRU will check if the kernel supports mg lru, this is as
// easy as looking for the presences of the enable file.
bool KernelSupportsMGLRU() {
  static const bool supported_mglru =
      base::PathExists(base::FilePath(kMGLRUEnableFile));
  return supported_mglru;
}

void OnMGLRUSetEnable(bool success) {
  if (!success) {
    LOG(ERROR) << "Unable to configure MGLRU.";
    return;
  }

  VLOG(1) << "swap_management configured MGLRU with value: "
          << kMGLRUEnableValue.Get();
}

}  // namespace

BASE_FEATURE(kMGLRUEnable, "MGLRUEnable", base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kMGLRUEnableValue = {&kMGLRUEnable,
                                                   "MGLRUEnableValue", 0};

// InitializeMGLRU will attempt to configure MGLRU with the experimental
// parameters for this user.
void InitializeMGLRU() {
  bool feature_enabled = base::FeatureList::IsEnabled(kMGLRUEnable);
  if (!feature_enabled) {
    VLOG(1) << "MGLRU feature is disabled";
    return;
  }

  if (!KernelSupportsMGLRU()) {
    // Only log an error when we're running on REAL CrOS without kernel
    // support.
    LOG_IF(ERROR, base::SysInfo::IsRunningOnChromeOS())
        << "Unable to configure MGLRU: no kernel support";
    return;
  }

  int feature_value = kMGLRUEnableValue.Get();
  if (feature_value < 0 ||
      feature_value > std::numeric_limits<uint8_t>::max()) {
    LOG(ERROR) << "Invalid value set for feature value.";
    return;
  }

  SwapManagementClient* swap_management_client = SwapManagementClient::Get();
  CHECK(swap_management_client);

  swap_management_client->MGLRUSetEnable(static_cast<uint8_t>(feature_value),
                                         base::BindOnce(&OnMGLRUSetEnable));
}

}  // namespace ash
