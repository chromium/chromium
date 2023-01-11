// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_CRASH_RECOVERY_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_CRASH_RECOVERY_MANAGER_H_

#include "base/functional/callback.h"

namespace ash {

namespace tether {

// Restores Tether state after a browser crash.
class CrashRecoveryManager {
 public:
  CrashRecoveryManager() {}

  CrashRecoveryManager(const CrashRecoveryManager&) = delete;
  CrashRecoveryManager& operator=(const CrashRecoveryManager&) = delete;

  virtual ~CrashRecoveryManager() {}

  // Restores state which was lost by a browser crash. If a crash did not occur
  // the last time that TetherComponent was active, this function is a no-op.
  // If there was an active Tether connection and the browser crashed, this
  // function restores the Tether connection.
  //
  // This function should only be called during the initialization of
  // TetherComponent.
  virtual void RestorePreCrashStateIfNecessary(
      base::OnceClosure on_restoration_finished) = 0;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_CRASH_RECOVERY_MANAGER_H_
