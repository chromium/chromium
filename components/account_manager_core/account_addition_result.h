// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_ADDITION_RESULT_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_ADDITION_RESULT_H_

#include "base/optional.h"
#include "components/account_manager_core/account.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace account_manager {

// The result of account addition request.
struct COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE) AccountAdditionResult {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Status {
    // The account was added successfully.
    kSuccess = 0,
    // The dialog is already open.
    kAlreadyInProgress = 1,
    // User closed the dialog.
    kCancelledByUser = 2,
    // Network error.
    kNetworkError = 3,
    // Unexpected response (couldn't parse mojo struct).
    kUnexpectedResponse = 4,
    kMaxValue = kUnexpectedResponse,
  };

  Status status;
  // The account that was added.
  base::Optional<Account> account;
  // The error is set only if `status` is set to `kNetworkError`.
  base::Optional<GoogleServiceAuthError> error;

  explicit AccountAdditionResult(Status status);
  AccountAdditionResult(Status status, Account account);
  AccountAdditionResult(Status status, GoogleServiceAuthError error);
  AccountAdditionResult(const AccountAdditionResult&);
  ~AccountAdditionResult();
};

}  // namespace account_manager

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_ADDITION_RESULT_H_
