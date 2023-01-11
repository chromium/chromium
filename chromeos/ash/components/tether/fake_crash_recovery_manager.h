// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_CRASH_RECOVERY_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_CRASH_RECOVERY_MANAGER_H_

#include "base/functional/callback.h"
#include "chromeos/ash/components/tether/crash_recovery_manager.h"

namespace ash {

namespace tether {

// Test double for CrashRecoveryManager.
class FakeCrashRecoveryManager : public CrashRecoveryManager {
 public:
  FakeCrashRecoveryManager();

  FakeCrashRecoveryManager(const FakeCrashRecoveryManager&) = delete;
  FakeCrashRecoveryManager& operator=(const FakeCrashRecoveryManager&) = delete;

  ~FakeCrashRecoveryManager() override;

  base::OnceClosure TakeOnRestorationFinishedCallback() {
    return std::move(on_restoration_finished_callback_);
  }

  // CrashRecoveryManager:
  void RestorePreCrashStateIfNecessary(
      base::OnceClosure on_restoration_finished) override;

 private:
  base::OnceClosure on_restoration_finished_callback_;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_CRASH_RECOVERY_MANAGER_H_
