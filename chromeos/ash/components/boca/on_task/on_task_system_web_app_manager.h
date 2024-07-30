// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_SYSTEM_WEB_APP_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_SYSTEM_WEB_APP_MANAGER_H_

#include "base/functional/callback_forward.h"

namespace ash {

// Responsible for managing all OnTask interactions with the Boca SWA. These
// interactions include launching the Boca SWA, closing the active SWA instance,
// pinning/unpinning the active SWA instance.
class OnTaskSystemWebAppManager {
 public:
  OnTaskSystemWebAppManager(const OnTaskSystemWebAppManager&) = delete;
  OnTaskSystemWebAppManager& operator=(const OnTaskSystemWebAppManager&) =
      delete;
  virtual ~OnTaskSystemWebAppManager() = default;

  // Launches the Boca SWA and triggers the specified callback to convey the
  // caller if the launch succeeded.
  virtual void LaunchSystemWebAppAsync(
      base::OnceCallback<void(bool)> callback) = 0;

  // Closes the active Boca SWA window.
  virtual void CloseActiveSystemWebAppWindow() = 0;

  // Returns true if there is an active Boca SWA window. False otherwise.
  virtual bool HasActiveSystemWebAppWindow() = 0;

  // Pins/unpins the active Boca SWA window based on the specified value.
  virtual void SetPinStateForActiveSystemWebAppWindow(bool pinned) = 0;

 protected:
  OnTaskSystemWebAppManager() = default;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_SYSTEM_WEB_APP_MANAGER_H_
