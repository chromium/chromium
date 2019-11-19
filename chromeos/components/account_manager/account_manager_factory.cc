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

  auto it = account_managers_.find(profile_path);
  if (it == account_managers_.end()) {
    it = account_managers_
             .emplace(profile_path, std::make_unique<AccountManager>())
             .first;
  }

  return it->second.get();
}

}  // namespace chromeos
