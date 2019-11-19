// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_FACTORY_H_
#define CHROMEOS_COMPONENTS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_FACTORY_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "chromeos/components/account_manager/account_manager.h"

namespace chromeos {

// This factory is needed because of multi signin on Chrome OS. Device Accounts,
// which are simultaneously logged into Chrome OS, should see different
// instances of |AccountManager| and hence |AccountManager| cannot be a part of
// a global like |g_browser_process| (otherwise Device Accounts will start
// sharing |AccountManager| and by extension, their Secondary
// Accounts/Identities, which is undesirable).
// Once multi signin has been removed and multi profile on ChromeOS takes its
// place, remove this class and make |AccountManager| a part of
// |g_browser_process|.
class COMPONENT_EXPORT(ACCOUNT_MANAGER) AccountManagerFactory {
 public:
  AccountManagerFactory();
  ~AccountManagerFactory();

  // Returns the |AccountManager| corresponding to the given |profile_path|.
  AccountManager* GetAccountManager(const std::string& profile_path);

 private:
  // A mapping from Profile path to an |AccountManager|. Acts a cache of
  // Account Managers.
  std::unordered_map<std::string, std::unique_ptr<AccountManager>>
      account_managers_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(AccountManagerFactory);
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_FACTORY_H_
