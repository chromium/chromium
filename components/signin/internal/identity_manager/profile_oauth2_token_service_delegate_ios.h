// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_IOS_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_IOS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/scoped_observation.h"
#include "base/threading/thread_checker.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/ios/device_accounts_provider.h"

class AccountTrackerService;
class SigninClient;

class ProfileOAuth2TokenServiceIOSDelegate
    : public ProfileOAuth2TokenServiceDelegate,
      public DeviceAccountsProvider::Observer {
 public:
  ProfileOAuth2TokenServiceIOSDelegate(
      SigninClient* client,
      std::unique_ptr<DeviceAccountsProvider> provider,
      AccountTrackerService* account_tracker_service);

  ProfileOAuth2TokenServiceIOSDelegate(
      const ProfileOAuth2TokenServiceIOSDelegate&) = delete;
  ProfileOAuth2TokenServiceIOSDelegate& operator=(
      const ProfileOAuth2TokenServiceIOSDelegate&) = delete;

  ~ProfileOAuth2TokenServiceIOSDelegate() override;

  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer,
      const std::string& token_binding_challenge) override;

#if BUILDFLAG(IS_IOS)
  void GetRefreshTokenFromDevice(
      const CoreAccountId& account_id,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      signin::AccessTokenFetcher::TokenCallback callback) override;
#endif

  // KeyedService
  void Shutdown() override;

  bool RefreshTokenIsAvailable(const CoreAccountId& account_id) const override;
  bool RefreshTokenIsAvailableOnDevice(
      const CoreAccountId& account_id) const override;

  std::vector<CoreAccountId> GetAccounts() const override;

  std::vector<AccountInfo> GetAccountsOnDevice() const override;

  void ReloadAllAccountsFromSystemWithPrimaryAccount(
      const std::optional<CoreAccountId>& primary_account_id) override;
  void ReloadAccountFromSystem(const CoreAccountId& account_id) override;

  // DeviceAccountsProvider::Observer:
  void OnAccountsOnDeviceChanged() override;
  void OnAccountOnDeviceUpdated(
      const DeviceAccountsProvider::AccountInfo& device_account) override;

 protected:
  // Removes |account_id| from |accounts_|. Fires |OnRefreshTokenRevoked|
  // if the account info is removed.
  virtual void RemoveAccount(const CoreAccountId& account_id);

 private:
  FRIEND_TEST_ALL_PREFIXES(ProfileOAuth2TokenServiceIOSDelegateTest,
                           OnAuthErrorChangedAfterUpdatingCredentials);

  // ProfileOAuth2TokenServiceDelegate implementation:
  void LoadCredentialsInternal(
      const CoreAccountId& primary_account_id) override;
  // This method should not be called when using shared authentication.
  void UpdateCredentialsInternal(
      const CoreAccountId& account_id,
      const std::string& refresh_token,
      const std::vector<uint8_t>& wrapped_binding_key) override;
  // Removes all credentials from this instance of |ProfileOAuth2TokenService|,
  // however, it does not revoke the identities from the device.
  // Subsequent calls to |RefreshTokenIsAvailable| will return |false|.
  void RevokeAllCredentialsInternal(
      signin_metrics::SourceForRefreshTokenOperation source) override;

  // Reloads accounts from the provider. Fires |OnRefreshTokenAvailable| for
  // each new account. Fires |OnRefreshTokenRevoked| for each account that was
  // removed.
  void ReloadCredentials(const CoreAccountId& primary_account_id);

  // Adds `account_id` to `accounts_` if it does not exist or updates the auth
  // error state of `account_id` to match `error` if it exists. Fires
  // `OnRefreshTokenAvailable` if the account info is updated or
  // `OnAuthErrorChanged` if the auth error changed.
  void AddOrUpdateAccount(const CoreAccountId& account_id,
                          GoogleServiceAuthError error);

  // Info about the existing accounts.
  std::set<CoreAccountId> accounts_;

  // Calls to this class are expected to be made from the browser UI thread.
  // The purpose of this checker is to detect access to
  // ProfileOAuth2TokenService from multiple threads in upstream code.
  THREAD_CHECKER(thread_checker_);

  // The client with which this instance was initialied, or null.
  SigninClient* client_ = nullptr;
  std::unique_ptr<DeviceAccountsProvider> provider_;
  AccountTrackerService* account_tracker_service_;
  base::ScopedObservation<DeviceAccountsProvider,
                          ProfileOAuth2TokenServiceIOSDelegate>
      device_accounts_provider_observation_{this};
};
#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_IOS_H_
