// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_EXTENSIONS_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_EXTENSIONS_MANAGER_H_

namespace ash::boca {

// Responsible for managing extensions in the context of OnTask. Especially
// useful as the app window transitions between locked and unlocked settings.
class OnTaskExtensionsManager {
 public:
  OnTaskExtensionsManager(const OnTaskExtensionsManager&) = delete;
  OnTaskExtensionsManager& operator=(const OnTaskExtensionsManager&) = delete;
  virtual ~OnTaskExtensionsManager() = default;

  // Disables extensions for OnTask. Primarily used for disabling some
  // extensions (notably user installed ones) to facilitate the locked setting.
  virtual void DisableExtensions() = 0;

  // Re-enables extensions for OnTask. Primarily used for re-enabling extensions
  // that were previously disabled to facilitate the locked setting.
  virtual void ReEnableExtensions() = 0;

 protected:
  OnTaskExtensionsManager() = default;
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_EXTENSIONS_MANAGER_H_
