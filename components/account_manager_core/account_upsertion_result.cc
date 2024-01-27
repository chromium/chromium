// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/account_upsertion_result.h"

#include "base/check.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace account_manager {

// static
AccountUpsertionResult AccountUpsertionResult::FromStatus(Status status) {
  DCHECK_NE(status, Status::kSuccess);
  DCHECK_NE(status, Status::kNetworkError);
  return AccountUpsertionResult(status, /*account=*/std::nullopt,
                                GoogleServiceAuthError::AuthErrorNone());
}

// static
AccountUpsertionResult AccountUpsertionResult::FromAccount(
    const Account& account) {
  return AccountUpsertionResult(Status::kSuccess, account,
                                GoogleServiceAuthError::AuthErrorNone());
}

// static
AccountUpsertionResult AccountUpsertionResult::FromError(
    const GoogleServiceAuthError& error) {
  DCHECK_NE(error.state(), GoogleServiceAuthError::NONE);
  return AccountUpsertionResult(Status::kNetworkError,
                                /*account=*/std::nullopt, error);
}

AccountUpsertionResult::AccountUpsertionResult(const AccountUpsertionResult&) =
    default;

AccountUpsertionResult& AccountUpsertionResult::operator=(
    const AccountUpsertionResult&) = default;

AccountUpsertionResult::~AccountUpsertionResult() = default;

AccountUpsertionResult::AccountUpsertionResult(
    Status status,
    const std::optional<Account>& account,
    const GoogleServiceAuthError& error)
    : status_(status), account_(account), error_(error) {
  DCHECK_EQ(account.has_value(), status == Status::kSuccess);
  DCHECK_NE(error.state() == GoogleServiceAuthError::NONE,
            status == Status::kNetworkError);
}

}  // namespace account_manager
