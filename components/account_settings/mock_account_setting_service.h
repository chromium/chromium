// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_SETTINGS_MOCK_ACCOUNT_SETTING_SERVICE_H_
#define COMPONENTS_ACCOUNT_SETTINGS_MOCK_ACCOUNT_SETTING_SERVICE_H_

#include "components/account_settings/account_setting_service.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace account_settings {

class MockAccountSettingService : public AccountSettingService {
 public:
  MockAccountSettingService();
  ~MockAccountSettingService() override;

  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  MOCK_METHOD(std::optional<bool>,
              GetBoolean,
              (const AccountSetting&),
              (const, override));
  MOCK_METHOD(std::optional<int>,
              GetInteger,
              (const AccountSetting&),
              (const, override));
  MOCK_METHOD(std::optional<std::string>,
              GetString,
              (const AccountSetting&),
              (const, override));
  MOCK_METHOD(std::unique_ptr<syncer::DataTypeControllerDelegate>,
              GetSyncControllerDelegate,
              (),
              (override));
};

}  // namespace account_settings

#endif  // COMPONENTS_ACCOUNT_SETTINGS_MOCK_ACCOUNT_SETTING_SERVICE_H_
