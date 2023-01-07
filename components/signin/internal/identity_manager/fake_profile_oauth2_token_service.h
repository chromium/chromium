// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_FAKE_PROFILE_OAUTH2_TOKEN_SERVICE_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_FAKE_PROFILE_OAUTH2_TOKEN_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "google_apis/gaia/fake_oauth2_access_token_manager.h"

// Helper class to simplify writing unittests that depend on an instance of
// ProfileOAuth2TokenService.
//
// Tests would typically do something like the following:
//
// FakeProfileOAuth2TokenService service;
// ...
// service.IssueRefreshToken("token");  // Issue refresh token/notify observers
// ...
// // Confirm that there is at least one active request.
// EXPECT_GT(0U, service.GetPendingRequests().size());
// ...
// // Make any pending token fetches for a given scope succeed.
// OAuth2AccessTokenManager::ScopeSet scopes;
// scopes.insert(GaiaConstants::kYourServiceScope);
// IssueTokenForScope(scopes, "access_token", base::Time()::Max());
// ...
// // ...or make them fail...
// IssueErrorForScope(scopes, GoogleServiceAuthError(INVALID_GAIA_CREDENTIALS));
//
class FakeProfileOAuth2TokenService : public ProfileOAuth2TokenService {
 public:
  explicit FakeProfileOAuth2TokenService(PrefService* user_prefs);
  FakeProfileOAuth2TokenService(
      PrefService* user_prefs,
      std::unique_ptr<ProfileOAuth2TokenServiceDelegate> delegate);

  FakeProfileOAuth2TokenService(const FakeProfileOAuth2TokenService&) = delete;
  FakeProfileOAuth2TokenService& operator=(
      const FakeProfileOAuth2TokenService&) = delete;

  ~FakeProfileOAuth2TokenService() override;

  // Gets a list of active requests (can be used by tests to validate that the
  // correct request has been issued).
  std::vector<FakeOAuth2AccessTokenManager::PendingRequest>
  GetPendingRequests();

  // Helper routines to issue tokens for pending requests.
  void IssueAllTokensForAccount(const CoreAccountId& account_id,
                                const std::string& access_token,
                                const base::Time& expiration);

  // Helper routines to issue token for pending requests based on TokenResponse.
  void IssueAllTokensForAccount(
      const CoreAccountId& account_id,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response);

  void IssueErrorForAllPendingRequestsForAccount(
      const CoreAccountId& account_id,
      const GoogleServiceAuthError& error);

  void IssueTokenForScope(const OAuth2AccessTokenManager::ScopeSet& scopes,
                          const std::string& access_token,
                          const base::Time& expiration);

  void IssueTokenForScope(
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response);

  void IssueErrorForScope(const OAuth2AccessTokenManager::ScopeSet& scopes,
                          const GoogleServiceAuthError& error);

  void IssueTokenForAllPendingRequests(const std::string& access_token,
                                       const base::Time& expiration);

  void IssueTokenForAllPendingRequests(
      const OAuth2AccessTokenConsumer::TokenResponse& token_response);

  void IssueErrorForAllPendingRequests(const GoogleServiceAuthError& error);

  void set_auto_post_fetch_response_on_message_loop(bool auto_post_response);

  bool IsFakeProfileOAuth2TokenServiceForTesting() const override;

 private:
  FakeOAuth2AccessTokenManager* GetFakeAccessTokenManager();
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_FAKE_PROFILE_OAUTH2_TOKEN_SERVICE_H_
