// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/cryptohome/cryptohome_util.h"

#include "base/notreached.h"

namespace cryptohome {

const std::string GetCryptohomeId(const AccountId& account_id) {
  switch (account_id.GetAccountType()) {
    case AccountType::GOOGLE: {
      return account_id.GetUserEmail();
    }
    case AccountType::ACTIVE_DIRECTORY: {
      // Always use the account id key, authpolicyd relies on it!
      return account_id.GetAccountIdKey();
    }
    case AccountType::UNKNOWN: {
      return account_id.GetUserEmail();
    }
  }

  NOTREACHED_IN_MIGRATION();
  return account_id.GetUserEmail();
}

}  // namespace cryptohome
