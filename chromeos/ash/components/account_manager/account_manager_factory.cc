// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/account_manager/account_manager_factory.h"

#include <string>
#include <utility>

#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"

namespace ash {

AccountManagerFactory::AccountManagerFactory() = default;
AccountManagerFactory::~AccountManagerFactory() = default;

account_manager::AccountManager* AccountManagerFactory::GetAccountManager(
    const std::string& profile_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return GetAccountManagerHolder(profile_path).account_manager.get();
}

crosapi::AccountManagerMojoService*
AccountManagerFactory::GetAccountManagerMojoService(
    const std::string& profile_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return GetAccountManagerHolder(profile_path)
      .account_manager_mojo_service.get();
}

AccountManagerFactory::AccountManagerHolder::AccountManagerHolder(
    std::unique_ptr<account_manager::AccountManager> account_manager,
    std::unique_ptr<crosapi::AccountManagerMojoService>
        account_manager_mojo_service)
    : account_manager(std::move(account_manager)),
      account_manager_mojo_service(std::move(account_manager_mojo_service)) {}

AccountManagerFactory::AccountManagerHolder::~AccountManagerHolder() = default;

const AccountManagerFactory::AccountManagerHolder&
AccountManagerFactory::GetAccountManagerHolder(
    const std::string& profile_path) {
  auto it = account_managers_.find(profile_path);
  if (it == account_managers_.end()) {
    auto account_manager = std::make_unique<account_manager::AccountManager>();
    auto account_manager_mojo_service =
        std::make_unique<crosapi::AccountManagerMojoService>(
            account_manager.get());
    it = account_managers_
             .emplace(
                 std::piecewise_construct, std::forward_as_tuple(profile_path),
                 std::forward_as_tuple(std::move(account_manager),
                                       std::move(account_manager_mojo_service)))
             .first;
  }
  return it->second;
}

}  // namespace ash
