// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_TEST_IDENTITY_MANAGER_OBSERVER_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_TEST_IDENTITY_MANAGER_OBSERVER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin {

// Class that observes events from IdentityManager. It allows setting
// |OnceClosure| callbacks to be executed for the observed events and retrieving
// the potential results and/or errors returned after such events have occurred.
class TestIdentityManagerObserver : IdentityManager::Observer {
 public:
  using PrimaryAccountChangedCallback =
      base::OnceCallback<void(PrimaryAccountChangeEvent)>;

  explicit TestIdentityManagerObserver(IdentityManager* identity_manager);

  TestIdentityManagerObserver(const TestIdentityManagerObserver&) = delete;
  TestIdentityManagerObserver& operator=(const TestIdentityManagerObserver&) =
      delete;

  ~TestIdentityManagerObserver() override;

  void SetOnPrimaryAccountChangedCallback(
      PrimaryAccountChangedCallback callback);
  const PrimaryAccountChangeEvent& GetPrimaryAccountChangedEvent();

  void SetOnRefreshTokenUpdatedCallback(base::OnceClosure callback);
  const CoreAccountInfo& AccountFromRefreshTokenUpdatedCallback();

  void SetOnErrorStateOfRefreshTokenUpdatedCallback(base::OnceClosure callback);
  const CoreAccountInfo& AccountFromErrorStateOfRefreshTokenUpdatedCallback();
  const GoogleServiceAuthError&
  ErrorFromErrorStateOfRefreshTokenUpdatedCallback() const;

  void SetOnRefreshTokenRemovedCallback(base::OnceClosure callback);
  const CoreAccountId& AccountIdFromRefreshTokenRemovedCallback();

  void SetOnRefreshTokensLoadedCallback(base::OnceClosure callback);

  void SetOnAccountsInCookieUpdatedCallback(base::OnceClosure callback);
  const AccountsInCookieJarInfo&
  AccountsInfoFromAccountsInCookieUpdatedCallback();
  const GoogleServiceAuthError& ErrorFromAccountsInCookieUpdatedCallback()
      const;

  void SetOnCookieDeletedByUserCallback(base::OnceClosure callback);

  const AccountInfo& AccountFromAccountUpdatedCallback();
  const AccountInfo& AccountFromAccountRemovedWithInfoCallback();
  bool WasCalledAccountRemovedWithInfoCallback();

  // Each element represents all the changes from an individual batch that has
  // occurred, with the elements ordered from oldest to newest batch occurrence.
  const std::vector<std::vector<CoreAccountId>>& BatchChangeRecords() const;

 private:
  // IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const PrimaryAccountChangeEvent& event_details) override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;
  void OnRefreshTokensLoaded() override;

  void OnAccountsInCookieUpdated(
      const AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;
  void OnAccountsCookieDeletedByUserAction() override;

  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnExtendedAccountInfoRemoved(const AccountInfo& info) override;

  void StartBatchOfRefreshTokenStateChanges();
  void OnEndBatchOfRefreshTokenStateChanges() override;

  raw_ptr<IdentityManager> identity_manager_;

  PrimaryAccountChangedCallback on_primary_account_changed_callback_;
  PrimaryAccountChangeEvent on_primary_account_changed_event_;

  base::OnceClosure on_refresh_token_updated_callback_;
  CoreAccountInfo account_from_refresh_token_updated_callback_;

  base::OnceClosure on_error_state_of_refresh_token_updated_callback_;
  CoreAccountInfo account_from_error_state_of_refresh_token_updated_callback_;
  GoogleServiceAuthError
      error_from_error_state_of_refresh_token_updated_callback_;

  base::OnceClosure on_refresh_token_removed_callback_;
  CoreAccountId account_from_refresh_token_removed_callback_;

  base::OnceClosure on_refresh_tokens_loaded_callback_;

  base::OnceClosure on_accounts_in_cookie_updated_callback_;
  AccountsInCookieJarInfo accounts_info_from_cookie_change_callback_;
  GoogleServiceAuthError error_from_cookie_change_callback_;

  base::OnceClosure on_cookie_deleted_by_user_callback_;

  AccountInfo account_from_account_updated_callback_;
  AccountInfo account_from_account_removed_with_info_callback_;

  bool is_inside_batch_ = false;
  bool was_called_account_removed_with_info_callback_ = false;
  std::vector<std::vector<CoreAccountId>> batch_change_records_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_TEST_IDENTITY_MANAGER_OBSERVER_H_
