// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_FAKE_CRASH_RECOVERY_MANAGER_H_
#define CHROMEOS_COMPONENTS_TETHER_FAKE_CRASH_RECOVERY_MANAGER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chromeos/components/tether/crash_recovery_manager.h"

namespace chromeos {

namespace tether {

// Test double for CrashRecoveryManager.
class FakeCrashRecoveryManager : public CrashRecoveryManager {
 public:
  FakeCrashRecoveryManager();
  ~FakeCrashRecoveryManager() override;

  base::OnceClosure TakeOnRestorationFinishedCallback() {
    return std::move(on_restoration_finished_callback_);
  }

  // CrashRecoveryManager:
  void RestorePreCrashStateIfNecessary(
      base::OnceClosure on_restoration_finished) override;

 private:
  base::OnceClosure on_restoration_finished_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeCrashRecoveryManager);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_FAKE_CRASH_RECOVERY_MANAGER_H_
