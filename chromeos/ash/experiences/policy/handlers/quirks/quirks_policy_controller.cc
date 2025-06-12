// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/policy/handlers/quirks/quirks_policy_controller.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/quirks/quirks_manager.h"

namespace policy {

QuirksPolicyController::QuirksPolicyController(
    quirks::QuirksManager* quirks_manager,
    ash::CrosSettings* cros_settings)
    : quirks_manager_(CHECK_DEREF(quirks_manager)),
      cros_settings_(CHECK_DEREF(cros_settings)) {
  subscription_ = cros_settings_->AddSettingsObserver(
      ash::kDeviceQuirksDownloadEnabled,
      base::BindRepeating(&QuirksPolicyController::OnUpdated,
                          weak_factory_.GetWeakPtr()));

  // Call manually once to sync the enabled state to settings.
  OnUpdated();
}

QuirksPolicyController::~QuirksPolicyController() = default;

void QuirksPolicyController::OnUpdated() {
  bool value = true;
  ash::CrosSettings::Get()->GetBoolean(ash::kDeviceQuirksDownloadEnabled,
                                       &value);
  quirks_manager_->SetEnabled(value);
}

}  // namespace policy
