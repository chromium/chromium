// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_FACTORY_H_
#define CHROMEOS_ASH_COMPONENTS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_FACTORY_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/component_export.h"
#include "base/sequence_checker.h"

namespace account_manager {
class AccountManager;
}  // namespace account_manager

namespace crosapi {
class AccountManagerMojoService;
}  // namespace crosapi

namespace ash {

// This factory is needed because of multi signin on Chrome OS. Device Accounts,
// which are simultaneously logged into Chrome OS, should see different
// instances of |AccountManager| and hence |AccountManager| cannot be a part of
// a global like |g_browser_process| (otherwise Device Accounts will start
// sharing |AccountManager| and by extension, their Secondary
// Accounts/Identities, which is undesirable).
// Once multi signin has been removed and multi profile on ChromeOS takes its
// place, remove this class and make |AccountManager| a part of
// |g_browser_process|.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_ACCOUNT_MANAGER)
    AccountManagerFactory {
 public:
  AccountManagerFactory();
  AccountManagerFactory(const AccountManagerFactory&) = delete;
  AccountManagerFactory& operator=(const AccountManagerFactory&) = delete;
  ~AccountManagerFactory();

  // Returns the |AccountManager| corresponding to the given |profile_path|.
  account_manager::AccountManager* GetAccountManager(
      const std::string& profile_path);

  // Returns the |AccountManagerMojoService| corresponding to the given
  // |profile_path|.
  crosapi::AccountManagerMojoService* GetAccountManagerMojoService(
      const std::string& profile_path);

 private:
  struct AccountManagerHolder {
    AccountManagerHolder(
        std::unique_ptr<account_manager::AccountManager> account_manager,
        std::unique_ptr<crosapi::AccountManagerMojoService>
            account_manager_mojo_service);
    AccountManagerHolder(const AccountManagerHolder&) = delete;
    AccountManagerHolder& operator=(const AccountManagerHolder&) = delete;
    ~AccountManagerHolder();

    const std::unique_ptr<account_manager::AccountManager> account_manager;
    const std::unique_ptr<crosapi::AccountManagerMojoService>
        account_manager_mojo_service;
  };

  const AccountManagerHolder& GetAccountManagerHolder(
      const std::string& profile_path);

  // A mapping from Profile path to an |AccountManagerHolder|. Acts a cache of
  // Account Managers and AccountManagerMojoService objects.
  std::unordered_map<std::string, AccountManagerHolder> account_managers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_FACTORY_H_
