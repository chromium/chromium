// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_DEVICE_ACCOUNTS_SYNCHRONIZER_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_DEVICE_ACCOUNTS_SYNCHRONIZER_H_

#include "base/optional.h"
#include "build/build_config.h"
#include "google_apis/gaia/core_account_id.h"

namespace signin {

// DeviceAccountsSynchronizer is the interface to support seeding the accounts
// information from a device-level store.
class DeviceAccountsSynchronizer {
 public:
  DeviceAccountsSynchronizer() = default;
  virtual ~DeviceAccountsSynchronizer() = default;

#if defined(OS_ANDROID)
  // Reloads the information of all device-level accounts. All device-level
  // accounts will be visible in IdentityManager::GetAccountsWithRefreshTokens()
  // with any persistent errors cleared after this method is called.
  virtual void ReloadAllAccountsFromSystemWithPrimaryAccount(
      const base::Optional<CoreAccountId>& primary_account_id) = 0;
#endif

#if defined(OS_IOS)
  // Reloads the information of all device-level accounts. All device-level
  // accounts will be visible in IdentityManager::GetAccountsWithRefreshTokens()
  // with any persistent errors cleared after this method is called.
  virtual void ReloadAllAccountsFromSystem() = 0;

  // Reloads the information of the device-level account with |account_id|. The
  // account will be visible in IdentityManager::GetAccountsWithRefreshTokens()
  // with any persistent error cleared after this method is called.
  virtual void ReloadAccountFromSystem(const CoreAccountId& account_id) = 0;
#endif

  // Class is non-copyable, non-moveable.
  DeviceAccountsSynchronizer(const DeviceAccountsSynchronizer&) = delete;
  DeviceAccountsSynchronizer& operator=(const DeviceAccountsSynchronizer&) =
      delete;

  DeviceAccountsSynchronizer(DeviceAccountsSynchronizer&&) noexcept = delete;
  DeviceAccountsSynchronizer& operator=(DeviceAccountsSynchronizer&&) noexcept =
      delete;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_DEVICE_ACCOUNTS_SYNCHRONIZER_H_
