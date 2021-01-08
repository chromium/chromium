// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_H_

#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "components/account_manager_core/account.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace account_manager {

// An interface to talk to |AccountManager|.
// Implementations of this interface hide the in-process / out-of-process nature
// of this communication.
// Instances of this class are singletons, and are independent of a |Profile|.
// Use |GetAccountManagerFacade()| to get an instance of this class.
class COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE) AccountManagerFacade {
 public:
  // The source UI surface used for launching the account addition /
  // re-authentication dialog. This should be as specific as possible.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // Note: Please update |AccountManagerAccountAdditionSource| in enums.xml
  // after adding new values.
  enum class AccountAdditionSource : int {
    // Settings > Add account button.
    kSettingsAddAccountButton = 0,
    // Settings > Sign in again button.
    kSettingsReauthAccountButton = 1,
    // Launched from an ARC application.
    kArc = 2,
    // Launched automatically from Chrome content area. As of now, this is
    // possible only when an account requires re-authentication.
    kContentArea = 3,
    // Print Preview dialog.
    kPrintPreviewDialog = 4,
    // Account Manager migration welcome screen.
    kAccountManagerMigrationWelcomeScreen = 5,
    // Onboarding.
    kOnboarding = 6,

    kMaxValue = kOnboarding
  };

  // The result of account addition request.
  struct AccountAdditionResult {
    enum class Status : int {
      // The account was added successfully.
      kSuccess = 0,
      // The dialog is already open.
      kAlreadyInProgress = 1,
      // User closed the dialog.
      kCancelledByUser = 2,
      // Network error.
      kNetworkError = 3,
    };

    Status status;
    // The account that was added.
    base::Optional<AccountKey> account;
    // The error is set only if `status` is set to `kNetworkError`.
    base::Optional<GoogleServiceAuthError> error;

    AccountAdditionResult();
    AccountAdditionResult(Status status, AccountKey account);
    AccountAdditionResult(Status status, GoogleServiceAuthError error);
    ~AccountAdditionResult();
  };

  AccountManagerFacade();
  AccountManagerFacade(const AccountManagerFacade&) = delete;
  AccountManagerFacade& operator=(const AccountManagerFacade&) = delete;
  virtual ~AccountManagerFacade() = 0;

  // Returns |true| if |AccountManager| is connected and has been fully
  // initialized.
  // Note: For out-of-process implementations, it returns |false| if the IPC
  // pipe to |AccountManager| is disconnected.
  virtual bool IsInitialized() = 0;

  // Launches account addition dialog and calls the `callback` with the result.
  // If `result` is `kSuccess`, the added account will be passed to the
  // callback. Otherwise `account` will be set to `base::nullopt`.
  virtual void ShowAddAccountDialog(
      const AccountAdditionSource& source,
      base::OnceCallback<void(const AccountAdditionResult& result)>
          callback) = 0;

  // Launches account reauthentication dialog for provided `email`.
  virtual void ShowReauthAccountDialog(const AccountAdditionSource& source,
                                       const std::string& email) = 0;
};

}  // namespace account_manager

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_H_
