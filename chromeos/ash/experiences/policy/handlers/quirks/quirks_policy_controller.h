// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_POLICY_HANDLERS_QUIRKS_QUIRKS_POLICY_CONTROLLER_H_
#define CHROMEOS_ASH_EXPERIENCES_POLICY_HANDLERS_QUIRKS_QUIRKS_POLICY_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"

namespace ash {
class CrosSettings;
}  // namespace ash

namespace quirks {
class QuirksManager;
}  // namespace quirks

namespace policy {

// Observes ash::kDeviceQuirksDownloadEnabled setting, and enables/disabled
// QuirksManager.
class QuirksPolicyController {
 public:
  // Passed `quirks_manager` and `cros_settings` must outlive this instance.
  QuirksPolicyController(quirks::QuirksManager* quirks_manager,
                         ash::CrosSettings* cros_settings);
  QuirksPolicyController(const QuirksPolicyController&) = delete;
  QuirksPolicyController& operator=(const QuirksPolicyController&) = delete;
  ~QuirksPolicyController();

  // Called when the device setting is updated.
  void OnUpdated();

 private:
  const raw_ref<quirks::QuirksManager> quirks_manager_;
  const raw_ref<ash::CrosSettings> cros_settings_;

  base::CallbackListSubscription subscription_;
  base::WeakPtrFactory<QuirksPolicyController> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROMEOS_ASH_EXPERIENCES_POLICY_HANDLERS_QUIRKS_QUIRKS_POLICY_CONTROLLER_H_
