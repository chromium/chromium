// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "build/chromeos_buildflags.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_upsertion_result.h"
#include "google_apis/gaia/google_service_auth_error.h"

class OAuth2AccessTokenFetcher;
class OAuth2AccessTokenConsumer;

namespace account_manager {

// An interface to talk to |AccountManager|.
// Implementations of this interface hide the in-process / out-of-process nature
// of this communication.
// Instances of this class are singletons, and are independent of a |Profile|.
// Use |GetAccountManagerFacade()| to get an instance of this class.
class COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE) AccountManagerFacade {
 public:
  // UMA histogram name.
  static const char kAccountAdditionSource[];

  // Observer interface to get notifications about changes in the account list.
  class Observer : public base::CheckedObserver {
   public:
    Observer();
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override;

    // Invoked when an account is added or updated.
    virtual void OnAccountUpserted(const Account& account) = 0;
    // Invoked when an account is removed.
    virtual void OnAccountRemoved(const Account& account) = 0;
    // Invoked when the error state associated with an account changes.
    virtual void OnAuthErrorChanged(const AccountKey& account,
                                    const GoogleServiceAuthError& error) = 0;
    // Invoked when the account signin dialog is closed on the OS side. Check
    // `AccountManagerObserver::OnSigninDialogClosed()` Mojo API in
    // account_manager.mojom for details.
    virtual void OnSigninDialogClosed();
  };

  // The source UI surface used for launching the account addition /
  // re-authentication dialog. This should be as specific as possible.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // Note: Please update |AccountManagerAccountAdditionSource| in enums.xml
  // after adding new values.
  enum class AccountAdditionSource : int {
    // OS Settings > Add account button.
    kSettingsAddAccountButton = 0,
    // OS Settings > Sign in again button.
    kSettingsReauthAccountButton = 1,
    // Launched from an ARC application.
    kArc = 2,
    // Launched automatically from Chrome content area. As of now, this is
    // possible only when an account requires re-authentication.
    kContentAreaReauth = 3,
    // Print Preview dialog.
    kPrintPreviewDialogUnused = 4,
    // Account Manager migration welcome screen.
    kAccountManagerMigrationWelcomeScreen = 5,
    // Onboarding.
    kOnboarding = 6,
    // At profile creation, main account of secondary profile.
    kChromeProfileCreation = 7,
    // Account addition flow launched by the user from One Google Bar.
    kOgbAddAccount = 8,
    // Avatar bubble -> Sign in again button.
    kAvatarBubbleReauthAccountButton = 9,
    // A Chrome extension required account re-authentication.
    kChromeExtensionReauth = 10,
    // Sync promo with an account that requires re-authentication.
    kChromeSyncPromoReauth = 11,
    // Chrome Settings > Sign in again button.
    kChromeSettingsReauthAccountButton = 12,
    // Avatar bubble -> Turn on sync button.
    kAvatarBubbleTurnOnSyncAddAccount = 13,
    // A Chrome extension required a new account.
    kChromeExtensionAddAccount = 14,
    // Sync promo with a new account.
    kChromeSyncPromoAddAccount = 15,
    // Chrome Settings > Turn on Sync.
    kChromeSettingsTurnOnSyncButton = 16,
    // Launched from ChromeOS Projector App for re-authentication.
    kChromeOSProjectorAppReauth = 17,
    // Chrome Menu -> Turn on Sync
    kChromeMenuTurnOnSync = 18,
    // Sign-in promo with a new account.
    kChromeSigninPromoAddAccount = 19,

    kMaxValue = kChromeSigninPromoAddAccount
  };

  AccountManagerFacade();
  AccountManagerFacade(const AccountManagerFacade&) = delete;
  AccountManagerFacade& operator=(const AccountManagerFacade&) = delete;
  virtual ~AccountManagerFacade() = 0;

  // Registers an observer. Ensures the observer wasn't already registered.
  virtual void AddObserver(Observer* observer) = 0;
  // Unregisters an observer that was registered using AddObserver.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Gets the list of accounts in Account Manager. If the remote side doesn't
  // support this call, an empty list of accounts will be returned.
  virtual void GetAccounts(
      base::OnceCallback<void(const std::vector<Account>&)> callback) = 0;

  // If `account` is in an error state (for example, if the refresh token is
  // known to be invalid), `callback` will get the corresponding
  // GoogleServiceAuthError.  If there's no known persistent error for
  // `account`, `callback` will receive `GoogleServiceAuthError` with `NONE`
  // state (Note: fetching an access token might still fail in this case).
  virtual void GetPersistentErrorForAccount(
      const AccountKey& account,
      base::OnceCallback<void(const GoogleServiceAuthError&)> callback) = 0;

  // Launches account addition dialog.
  virtual void ShowAddAccountDialog(AccountAdditionSource source) = 0;

  // Launches account addition dialog and calls the `callback` with the result.
  // If `result` is `kSuccess`, the added account will be passed to the
  // callback. Otherwise `account` will be set to `std::nullopt`.
  virtual void ShowAddAccountDialog(
      AccountAdditionSource source,
      base::OnceCallback<void(const AccountUpsertionResult& result)>
          callback) = 0;

  // Launches account reauthentication dialog for provided `email`.
  // Note: the added/reauthenticated account may not match the account provided
  // in the `email` field if user decided to edit the email inside the dialog.
  virtual void ShowReauthAccountDialog(
      AccountAdditionSource source,
      const std::string& email,
      base::OnceCallback<void(const AccountUpsertionResult& result)>
          callback) = 0;

  // Launches OS Settings > Accounts.
  virtual void ShowManageAccountsSettings() = 0;

  // Creates an access token fetcher for `account`.
  // Currently, `account` must be a Gaia account.
  // The returned object should not outlive `AccountManagerFacade` itself.
  virtual std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const AccountKey& account,
      OAuth2AccessTokenConsumer* consumer) = 0;

  // Reports an `error` for `account`.
  // `account` must be a valid Gaia account known to Account Manager.
  // Setting the error `state` as `kNone` resets the error state for `account`.
  virtual void ReportAuthError(const AccountKey& account,
                               const GoogleServiceAuthError& error) = 0;

  // Adds or updates an account programmatically without user interaction.
  // Should only be used in tests.
  virtual void UpsertAccountForTesting(const Account& account,
                                       const std::string& token_value) = 0;

  // Removes an account programmatically without user interaction. Should only
  // be used in tests.
  virtual void RemoveAccountForTesting(const AccountKey& account) = 0;
};

}  // namespace account_manager

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_H_
