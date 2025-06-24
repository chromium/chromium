// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FAKE_PASSWORD_MANAGER_SETTINGS_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FAKE_PASSWORD_MANAGER_SETTINGS_SERVICE_H_

#include "base/containers/flat_set.h"
#include "components/password_manager/core/browser/password_manager_settings_service.h"

namespace password_manager {

enum class PasswordManagerSetting : int;

// Fake in-memory implementation of `PasswordManagerSettingsService`.
class FakePasswordManagerSettingsService
    : public PasswordManagerSettingsService {
 public:
  FakePasswordManagerSettingsService();

  FakePasswordManagerSettingsService(
      const FakePasswordManagerSettingsService&) = delete;
  FakePasswordManagerSettingsService& operator=(
      const FakePasswordManagerSettingsService&) = delete;

  ~FakePasswordManagerSettingsService() override;

  void SetSettingEnabled(PasswordManagerSetting setting, bool enabled);

  // PasswordManagerSettingsService implementation.
  bool IsSettingEnabled(PasswordManagerSetting setting) const override;
  void RequestSettingsFromBackend() override;
  void TurnOffAutoSignIn() override;

 private:
  base::flat_set<password_manager::PasswordManagerSetting> enabled_settings_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FAKE_PASSWORD_MANAGER_SETTINGS_SERVICE_H_
