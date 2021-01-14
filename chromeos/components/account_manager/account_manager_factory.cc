// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/account_manager/account_manager_factory.h"

#include <string>
#include <utility>

#include "base/macros.h"
#include "chromeos/components/account_manager/account_manager.h"

namespace chromeos {

AccountManagerFactory::AccountManagerFactory() = default;
AccountManagerFactory::~AccountManagerFactory() = default;

AccountManager* AccountManagerFactory::GetAccountManager(
    const std::string& profile_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return GetAccountManagerHolder(profile_path).account_manager.get();
}

crosapi::AccountManagerAsh* AccountManagerFactory::GetAccountManagerAsh(
    const std::string& profile_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return GetAccountManagerHolder(profile_path).account_manager_ash.get();
}

AccountManagerFactory::AccountManagerHolder::AccountManagerHolder(
    std::unique_ptr<AccountManager> account_manager,
    std::unique_ptr<crosapi::AccountManagerAsh> account_manager_ash)
    : account_manager(std::move(account_manager)),
      account_manager_ash(std::move(account_manager_ash)) {}

AccountManagerFactory::AccountManagerHolder::~AccountManagerHolder() = default;

const AccountManagerFactory::AccountManagerHolder&
AccountManagerFactory::GetAccountManagerHolder(
    const std::string& profile_path) {
  auto it = account_managers_.find(profile_path);
  if (it == account_managers_.end()) {
    auto account_manager = std::make_unique<AccountManager>();
    auto account_manager_ash =
        std::make_unique<crosapi::AccountManagerAsh>(account_manager.get());
    it = account_managers_
             .emplace(std::piecewise_construct,
                      std::forward_as_tuple(profile_path),
                      std::forward_as_tuple(std::move(account_manager),
                                            std::move(account_manager_ash)))
             .first;
  }
  return it->second;
}

}  // namespace chromeos
