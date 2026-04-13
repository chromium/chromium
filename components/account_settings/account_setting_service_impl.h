// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SERVICE_IMPL_H_
#define COMPONENTS_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/account_settings/account_setting_service.h"
#include "components/account_settings/account_setting_sync_bridge.h"

namespace account_settings {

class AccountSettingServiceImpl : public AccountSettingService,
                                  public AccountSettingSyncBridge::Observer {
 public:
  explicit AccountSettingServiceImpl(
      std::unique_ptr<AccountSettingSyncBridge> sync_bridge);
  AccountSettingServiceImpl(const AccountSettingServiceImpl&) = delete;
  AccountSettingServiceImpl& operator=(const AccountSettingServiceImpl&) =
      delete;
  ~AccountSettingServiceImpl() override;

  // AccountSettingService:
  void AddObserver(AccountSettingService::Observer* observer) override;
  void RemoveObserver(AccountSettingService::Observer* observer) override;
  std::optional<bool> GetBoolean(const AccountSetting& setting) const override;
  std::optional<int> GetInteger(const AccountSetting& setting) const override;
  std::optional<std::string> GetString(
      const AccountSetting& setting) const override;
  std::unique_ptr<syncer::DataTypeControllerDelegate>
  GetSyncControllerDelegate() override;

 private:
  // AccountSettingSyncBridge::Observer:
  void OnDataLoadedFromDisk() override;
  void OnDataUpdated(const std::string& setting_name) override;

  base::ObserverList<AccountSettingService::Observer> observers_;

  std::unique_ptr<AccountSettingSyncBridge> sync_bridge_;

  base::ScopedObservation<AccountSettingSyncBridge,
                          AccountSettingSyncBridge::Observer>
      scoped_observation_{this};
};

}  // namespace account_settings

#endif  // COMPONENTS_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SERVICE_IMPL_H_
