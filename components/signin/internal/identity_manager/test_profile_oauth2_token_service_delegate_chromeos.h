// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_TEST_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_CHROMEOS_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_TEST_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_CHROMEOS_H_

#include "base/scoped_observation.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate_chromeos.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_observer.h"
#include "services/network/test/test_network_connection_tracker.h"

class AccountTrackerService;
class SigninClient;

namespace crosapi {
class AccountManagerMojoService;
}

namespace signin {

// Creates and owns instance of TestNetworkConnectionTracker, if it wasn't
// created yet. This is a wrapper around
// ProfileOAuth2TokenServiceDelegateChromeOS that can be used for testing. It
// lazily instantiates a TestNetworkConnectionTracker if it wasn't created yet.
class TestProfileOAuth2TokenServiceDelegateChromeOS
    : public ProfileOAuth2TokenServiceDelegate,
      public ProfileOAuth2TokenServiceObserver {
 public:
  TestProfileOAuth2TokenServiceDelegateChromeOS(
      SigninClient* client,
      AccountTrackerService* account_tracker_service,
      crosapi::AccountManagerMojoService* account_manager_mojo_service,
      bool is_regular_profile);
  ~TestProfileOAuth2TokenServiceDelegateChromeOS() override;
  TestProfileOAuth2TokenServiceDelegateChromeOS(
      const TestProfileOAuth2TokenServiceDelegateChromeOS&) = delete;
  TestProfileOAuth2TokenServiceDelegateChromeOS& operator=(
      const TestProfileOAuth2TokenServiceDelegateChromeOS&) = delete;

  // ProfileOAuth2TokenServiceDelegate overrides.
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer,
      const std::string& token_binding_challenge) override;
  bool RefreshTokenIsAvailable(const CoreAccountId& account_id) const override;
  void UpdateAuthError(const CoreAccountId& account_id,
                       const GoogleServiceAuthError& error,
                       bool fire_auth_error_changed) override;
  GoogleServiceAuthError GetAuthError(
      const CoreAccountId& account_id) const override;
  std::vector<CoreAccountId> GetAccounts() const override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      const override;
  const net::BackoffEntry* BackoffEntry() const override;
  void ClearAuthError(const std::optional<CoreAccountId>& account_id) override;
  GoogleServiceAuthError BackOffError() const override;
  void ResetBackOffEntry() override;

  // |ProfileOAuth2TokenServiceObserver| implementation.
  void OnRefreshTokenAvailable(const CoreAccountId& account_id) override;
  void OnRefreshTokenRevoked(const CoreAccountId& account_id) override;
  void OnEndBatchChanges() override;
  void OnRefreshTokensLoaded() override;
  void OnAuthErrorChanged(
      const CoreAccountId& account_id,
      const GoogleServiceAuthError& auth_error,
      signin_metrics::SourceForRefreshTokenOperation source) override;

 private:
  // ProfileOAuth2TokenServiceDelegate implementation:
  void LoadCredentialsInternal(const CoreAccountId& primary_account_id,
                               bool is_syncing) override;
  void UpdateCredentialsInternal(const CoreAccountId& account_id,
                                 const std::string& refresh_token) override;
  void RevokeCredentialsInternal(const CoreAccountId& account_id) override;
  void RevokeAllCredentialsInternal(
      signin_metrics::SourceForRefreshTokenOperation source) override;

  // Owning pointer to TestNetworkConnectionTracker. Set only if it wasn't
  // created before initialization of this class.
  std::unique_ptr<network::TestNetworkConnectionTracker> owned_tracker_;
  std::unique_ptr<account_manager::AccountManagerFacade>
      account_manager_facade_;
  std::unique_ptr<ProfileOAuth2TokenServiceDelegateChromeOS> delegate_;
  base::ScopedObservation<ProfileOAuth2TokenServiceDelegateChromeOS,
                          ProfileOAuth2TokenServiceObserver>
      token_service_observation_{this};
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_TEST_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_CHROMEOS_H_
