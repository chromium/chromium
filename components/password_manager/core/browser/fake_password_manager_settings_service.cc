// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/fake_password_manager_settings_service.h"

#include "base/containers/flat_set.h"
#include "components/password_manager/core/browser/password_manager_setting.h"

namespace password_manager {

FakePasswordManagerSettingsService::FakePasswordManagerSettingsService() {
  enabled_settings_.insert(PasswordManagerSetting::kOfferToSavePasswords);
  enabled_settings_.insert(PasswordManagerSetting::kAutoSignIn);
  // kBiometricReauthBeforePwdFilling is left out because it's off by default.

  // When adding new settings, assign a default value above that matches
  // production behavior. Then update the counter here.
  static_assert(static_cast<int>(PasswordManagerSetting::kMaxValue) == 2);
}

FakePasswordManagerSettingsService::~FakePasswordManagerSettingsService() =
    default;

void FakePasswordManagerSettingsService::SetSettingEnabled(
    PasswordManagerSetting setting,
    bool enabled) {
  if (enabled) {
    enabled_settings_.insert(setting);
  } else {
    enabled_settings_.erase(setting);
  }
}

bool FakePasswordManagerSettingsService::IsSettingEnabled(
    PasswordManagerSetting setting) const {
  return enabled_settings_.contains(setting);
}

void FakePasswordManagerSettingsService::RequestSettingsFromBackend() {}

void FakePasswordManagerSettingsService::TurnOffAutoSignIn() {
  SetSettingEnabled(PasswordManagerSetting::kAutoSignIn, false);
}

}  // namespace password_manager
