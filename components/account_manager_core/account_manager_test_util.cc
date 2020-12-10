// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/account_manager_test_util.h"

namespace account_manager {

account_manager::Account CreateTestGaiaAccount(const std::string& raw_email) {
  account_manager::Account account;
  account.key.account_type = account_manager::AccountType::kGaia;
  // TODO(https://crbug.com/1150770): Use signin::GetTestGaiaIdForEmail here.
  account.key.id = std::string("gaia_id_for_") + raw_email;
  account.raw_email = raw_email;
  return account;
}

}  // namespace account_manager
