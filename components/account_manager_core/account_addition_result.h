// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_ADDITION_RESULT_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_ADDITION_RESULT_H_

#include "components/account_manager_core/account.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace account_manager {

// The result of account addition request.
class COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE) AccountAdditionResult {
 public:
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
    // The sign-in was blocked by policy for this user.
    kBlockedByPolicy = 5,
    // Mojo remote to Account Manager is disconnected.
    kMojoRemoteDisconnected = 6,
    // Remote and receiver have incompatible Mojo versions.
    kIncompatibleMojoVersions = 7,

    kMaxValue = kIncompatibleMojoVersions,
  };

  // Creates result with `status` different from `kSuccess` and `kNetworkError`.
  // `account` is nullopt and `error` is NONE. To create a result with
  // `kSuccess` or `kNetworkError`, use the other constructors.
  static AccountAdditionResult FromStatus(Status status);

  // Creates result with `status` set to `kSuccess`. `error` is NONE.
  static AccountAdditionResult FromAccount(const Account& account);

  // Creates result with `status` set to `kNetworkError`. `account` is nullopt,
  // error state must not be NONE.
  static AccountAdditionResult FromError(const GoogleServiceAuthError& error);

  Status status() const { return status_; }

  // The account that was added. Set iff `status` is set to `kSuccess`.
  const absl::optional<Account>& account() const { return account_; }

  // The error state is NONE unless `status` is set to `kNetworkError`.
  const GoogleServiceAuthError& error() const { return error_; }

  AccountAdditionResult(const AccountAdditionResult&);
  ~AccountAdditionResult();

 private:
  AccountAdditionResult(Status status,
                        const absl::optional<Account>& account,
                        const GoogleServiceAuthError& error);

  const Status status_;
  const absl::optional<Account> account_;
  const GoogleServiceAuthError error_;
};

}  // namespace account_manager

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_ADDITION_RESULT_H_
