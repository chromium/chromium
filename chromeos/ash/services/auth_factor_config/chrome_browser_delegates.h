// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_CHROME_BROWSER_DELEGATES_H_
#define CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_CHROME_BROWSER_DELEGATES_H_

#include <memory>
#include <string>
#include "base/functional/callback.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"

namespace ash::auth {

class QuickUnlockStorageDelegate {
 public:
  QuickUnlockStorageDelegate() = default;
  QuickUnlockStorageDelegate(const QuickUnlockStorageDelegate&) = delete;
  QuickUnlockStorageDelegate& operator=(const QuickUnlockStorageDelegate&) =
      delete;
  virtual ~QuickUnlockStorageDelegate() = default;

  virtual UserContext* GetUserContext(const ::user_manager::User* user,
                                      const std::string& token) = 0;
  virtual void SetUserContext(const ::user_manager::User* user,
                              std::unique_ptr<UserContext> user_context) = 0;

  virtual PrefService* GetPrefService(const ::user_manager::User& user) = 0;
};

class PinBackendDelegate {
 public:
  using BoolCallback = base::OnceCallback<void(bool)>;

  PinBackendDelegate() = default;
  PinBackendDelegate(const PinBackendDelegate&) = delete;
  PinBackendDelegate& operator=(const PinBackendDelegate&) = delete;
  virtual ~PinBackendDelegate() = default;

  virtual void Set(const AccountId& account_id,
                   const std::string& auth_token,
                   const std::string& pin,
                   BoolCallback did_set) = 0;

  virtual void Remove(const AccountId& account_id,
                      const std::string& auth_token,
                      BoolCallback did_remove) = 0;
};

}  // namespace ash::auth

#endif  // CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_CHROME_BROWSER_DELEGATES_H_
