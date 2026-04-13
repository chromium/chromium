// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SERVICE_H_
#define COMPONENTS_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SERVICE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/observer_list_types.h"
#include "components/account_settings/account_settings.h"
#include "components/keyed_service/core/keyed_service.h"

namespace syncer {
class DataTypeControllerDelegate;
}

namespace account_settings {

// Manages settings stored in your Google account. These settings differ from
// regular prefs, since they originate from the user's account and are available
// beyond Chrome.
class AccountSettingService : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the value of a specific account setting changes.
    virtual void OnAccountSettingDataUpdated(
        const std::string& setting_name) = 0;
  };

  ~AccountSettingService() override = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns a value for the specified `setting` and type if exists, otherwise
  // returns `nullopt`.
  virtual std::optional<bool> GetBoolean(
      const AccountSetting& setting) const = 0;
  virtual std::optional<int> GetInteger(
      const AccountSetting& setting) const = 0;
  virtual std::optional<std::string> GetString(
      const AccountSetting& setting) const = 0;

  // Returns a controller delegate for the sync bridge owned by this service.
  virtual std::unique_ptr<syncer::DataTypeControllerDelegate>
  GetSyncControllerDelegate() = 0;
};

}  // namespace account_settings

#endif  // COMPONENTS_ACCOUNT_SETTINGS_ACCOUNT_SETTING_SERVICE_H_
