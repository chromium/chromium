// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/account_addition_result.h"

#include "base/check.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace account_manager {

// static
AccountAdditionResult AccountAdditionResult::FromStatus(Status status) {
  DCHECK_NE(status, Status::kSuccess);
  DCHECK_NE(status, Status::kNetworkError);
  return AccountAdditionResult(status, /*account=*/absl::nullopt,
                               GoogleServiceAuthError::AuthErrorNone());
}

// static
AccountAdditionResult AccountAdditionResult::FromAccount(
    const Account& account) {
  return AccountAdditionResult(Status::kSuccess, account,
                               GoogleServiceAuthError::AuthErrorNone());
}

// static
AccountAdditionResult AccountAdditionResult::FromError(
    const GoogleServiceAuthError& error) {
  DCHECK_NE(error.state(), GoogleServiceAuthError::NONE);
  return AccountAdditionResult(Status::kNetworkError, /*account=*/absl::nullopt,
                               error);
}

AccountAdditionResult::AccountAdditionResult(const AccountAdditionResult&) =
    default;

AccountAdditionResult::~AccountAdditionResult() = default;

AccountAdditionResult::AccountAdditionResult(
    Status status,
    const absl::optional<Account>& account,
    const GoogleServiceAuthError& error)
    : status_(status), account_(account), error_(error) {
  DCHECK_EQ(account.has_value(), status == Status::kSuccess);
  DCHECK_NE(error.state() == GoogleServiceAuthError::NONE,
            status == Status::kNetworkError);
}

}  // namespace account_manager
