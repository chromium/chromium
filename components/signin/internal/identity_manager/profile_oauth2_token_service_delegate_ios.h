// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_IOS_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_IOS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"

class AccountTrackerService;
class DeviceAccountsProvider;
class SigninClient;

class ProfileOAuth2TokenServiceIOSDelegate
    : public ProfileOAuth2TokenServiceDelegate {
 public:
  ProfileOAuth2TokenServiceIOSDelegate(
      SigninClient* client,
      std::unique_ptr<DeviceAccountsProvider> provider,
      AccountTrackerService* account_tracker_service);
  ~ProfileOAuth2TokenServiceIOSDelegate() override;

  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer) override;

  // KeyedService
  void Shutdown() override;

  bool RefreshTokenIsAvailable(const CoreAccountId& account_id) const override;
  GoogleServiceAuthError GetAuthError(
      const CoreAccountId& account_id) const override;
  void UpdateAuthError(const CoreAccountId& account_id,
                       const GoogleServiceAuthError& error) override;

  void LoadCredentials(const CoreAccountId& primary_account_id) override;
  std::vector<CoreAccountId> GetAccounts() const override;

  // This method should not be called when using shared authentication.
  void UpdateCredentials(const CoreAccountId& account_id,
                         const std::string& refresh_token) override;

  // Removes all credentials from this instance of |ProfileOAuth2TokenService|,
  // however, it does not revoke the identities from the device.
  // Subsequent calls to |RefreshTokenIsAvailable| will return |false|.
  void RevokeAllCredentials() override;

  void ReloadAllAccountsFromSystem() override;

  void ReloadAccountFromSystem(const CoreAccountId& account_id) override;

  // Adds |account_id| to |accounts_| if it does not exist or udpates
  // the auth error state of |account_id| if it exists. Fires
  // |OnRefreshTokenAvailable| if the account info is updated.
  virtual void AddOrUpdateAccount(const CoreAccountId& account_id);

 protected:
  // Removes |account_id| from |accounts_|. Fires |OnRefreshTokenRevoked|
  // if the account info is removed.
  virtual void RemoveAccount(const CoreAccountId& account_id);

 private:
  friend class ProfileOAuth2TokenServiceIOSDelegateTest;
  FRIEND_TEST_ALL_PREFIXES(ProfileOAuth2TokenServiceIOSDelegateTest,
                           LoadRevokeCredentialsClearsExcludedAccounts);
  FRIEND_TEST_ALL_PREFIXES(ProfileOAuth2TokenServiceIOSDelegateTest,
                           ReloadCredentials);
  FRIEND_TEST_ALL_PREFIXES(ProfileOAuth2TokenServiceIOSDelegateTest,
                           ReloadCredentialsWithPrimaryAccountId);
  FRIEND_TEST_ALL_PREFIXES(ProfileOAuth2TokenServiceIOSDelegateTest,
                           UpdateAuthErrorAfterRevokeCredentials);
  FRIEND_TEST_ALL_PREFIXES(ProfileOAuth2TokenServiceIOSDelegateTest,
                           GetAuthError);

  struct AccountStatus {
    GoogleServiceAuthError last_auth_error;
  };

  // Maps the |account_id| of accounts known to ProfileOAuth2TokenService
  // to information about the account.
  typedef std::map<CoreAccountId, AccountStatus> AccountStatusMap;

  // Reloads accounts from the provider. Fires |OnRefreshTokenAvailable| for
  // each new account. Fires |OnRefreshTokenRevoked| for each account that was
  // removed.
  void ReloadCredentials();

  // Clears exclude secondary accounts preferences.
  void ClearExcludedSecondaryAccounts();

  // Info about the existing accounts.
  AccountStatusMap accounts_;

  // Calls to this class are expected to be made from the browser UI thread.
  // The purpose of this checker is to detect access to
  // ProfileOAuth2TokenService from multiple threads in upstream code.
  THREAD_CHECKER(thread_checker_);

  // The client with which this instance was initialied, or null.
  SigninClient* client_ = nullptr;
  std::unique_ptr<DeviceAccountsProvider> provider_;
  AccountTrackerService* account_tracker_service_;

  DISALLOW_COPY_AND_ASSIGN(ProfileOAuth2TokenServiceIOSDelegate);
};
#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_IOS_H_
