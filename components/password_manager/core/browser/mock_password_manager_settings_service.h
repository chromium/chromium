// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_MANAGER_SETTINGS_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_MANAGER_SETTINGS_SERVICE_H_

#include "components/password_manager/core/browser/password_manager_setting.h"
#include "components/password_manager/core/browser/password_manager_settings_service.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockPasswordManagerSettingsService
    : public PasswordManagerSettingsService {
 public:
  MockPasswordManagerSettingsService();
  ~MockPasswordManagerSettingsService() override;

  MOCK_METHOD(bool,
              IsSettingEnabled,
              (password_manager::PasswordManagerSetting),
              (override));
  MOCK_METHOD(void, RequestSettingsFromBackend, (), (override));

  MOCK_METHOD(void, TurnOffAutoSignIn, (), (override));
};

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_MANAGER_SETTINGS_SERVICE_H_
