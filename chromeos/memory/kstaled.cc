// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/memory/kstaled.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"

namespace chromeos {

namespace {

constexpr char kKstaledRatioFile[] = "/sys/kernel/mm/kstaled/ratio";

// KernelSupportsKstaled will check if the kernel supports kstaled this is as
// easy as checking for the kstaled sysfs node.
bool KernelSupportsKstaled() {
  static const bool supported =
      base::PathExists(base::FilePath(kKstaledRatioFile));
  return supported;
}

void OnRatioSet(bool success) {
  if (!success) {
    LOG(ERROR) << "Unable to configure kstaled";
    return;
  }

  VLOG(1) << "Debugd configured kstaled with value: " << kKstaledRatio.Get();
}

}  // namespace

const base::Feature kKstaled{"KstaledSwap", base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int> kKstaledRatio = {&kKstaled, "KstaledRatio", 4};

// InitializeKstaled will attempt to configure kstaled with the experimental
// parameters for this user.
void InitializeKstaled() {
  bool feature_enabled = base::FeatureList::IsEnabled(kKstaled);
  if (!feature_enabled) {
    VLOG(1) << "Kstaled is disabled";
    return;
  }

  if (!KernelSupportsKstaled()) {
    LOG(ERROR) << "Unable to configure kstaled: no kernel support";
    return;
  }

  int feature_ratio = kKstaledRatio.Get();
  if (feature_ratio <= 0 || feature_ratio > 255) {
    LOG(ERROR) << "Configuring kstaled with a ratio of 0 disables the "
                  "feature, the valid range is 1-255.";
    return;
  }

  chromeos::DebugDaemonClient* debugd_client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  DCHECK(debugd_client);

  debugd_client->SetKstaledRatio(static_cast<uint8_t>(feature_ratio),
                                 base::Bind(&OnRatioSet));
}

}  // namespace chromeos
