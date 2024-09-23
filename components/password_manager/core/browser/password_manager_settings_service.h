// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_SETTINGS_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_SETTINGS_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_manager_setting.h"

namespace password_manager {

// Service used to access the password manager settings.
class PasswordManagerSettingsService : public KeyedService {
 public:
  // Checks if `setting` is enabled. It ensures that the correct pref is checked
  // on Android, which depends on the unified password manager status.
  virtual bool IsSettingEnabled(
      password_manager::PasswordManagerSetting setting) const = 0;

  // Asynchronously fetch password settings from backend.
  virtual void RequestSettingsFromBackend() = 0;

  // Sets the auto sign in setting to off. Used by the auto sign in first run
  // dialog.
  virtual void TurnOffAutoSignIn() = 0;

 protected:
  ~PasswordManagerSettingsService() override = default;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_SETTINGS_SERVICE_H_
