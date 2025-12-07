// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/settings/cros_settings_waiter.h"

#include "base/callback_list.h"
#include "chromeos/ash/components/settings/cros_settings.h"

namespace ash {

CrosSettingsWaiter::CrosSettingsWaiter(
    base::span<const std::string_view> settings) {
  auto* cros_settings = ash::CrosSettings::Get();
  for (auto setting : settings) {
    subscriptions_.push_back(
        cros_settings->AddSettingsObserver(setting, run_loop_.QuitClosure()));
  }
}

CrosSettingsWaiter::~CrosSettingsWaiter() = default;

void CrosSettingsWaiter::Wait() {
  run_loop_.Run();
}

}  // namespace ash
