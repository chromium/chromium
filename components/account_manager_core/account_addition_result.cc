// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/account_addition_result.h"

namespace account_manager {

AccountAdditionResult::AccountAdditionResult(Status status) : status(status) {}

AccountAdditionResult::AccountAdditionResult(Status status, Account account)
    : status(status), account(account) {
  DCHECK_EQ(status, Status::kSuccess);
}

AccountAdditionResult::AccountAdditionResult(Status status,
                                             GoogleServiceAuthError error)
    : status(status), error(error) {
  DCHECK_NE(status, Status::kSuccess);
}

AccountAdditionResult::AccountAdditionResult(const AccountAdditionResult&) =
    default;

AccountAdditionResult::~AccountAdditionResult() = default;

}  // namespace account_manager
