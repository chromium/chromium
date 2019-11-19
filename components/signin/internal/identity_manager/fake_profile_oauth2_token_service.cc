// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service.h"

#include <memory>

#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service_delegate.h"

FakeProfileOAuth2TokenService::FakeProfileOAuth2TokenService(
    PrefService* user_prefs)
    : ProfileOAuth2TokenService(
          user_prefs,
          std::make_unique<FakeProfileOAuth2TokenServiceDelegate>()) {
  OverrideAccessTokenManagerForTesting(
      std::make_unique<FakeOAuth2AccessTokenManager>(
          this /* OAuth2AccessTokenManager::Delegate* */));
}

FakeProfileOAuth2TokenService::~FakeProfileOAuth2TokenService() {}

void FakeProfileOAuth2TokenService::IssueAllTokensForAccount(
    const CoreAccountId& account_id,
    const std::string& access_token,
    const base::Time& expiration) {
  GetFakeAccessTokenManager()->IssueAllTokensForAccount(
      account_id, access_token, expiration);
}

void FakeProfileOAuth2TokenService::IssueAllTokensForAccount(
    const CoreAccountId& account_id,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  GetFakeAccessTokenManager()->IssueAllTokensForAccount(account_id,
                                                        token_response);
}

void FakeProfileOAuth2TokenService::IssueErrorForAllPendingRequestsForAccount(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& error) {
  GetFakeAccessTokenManager()->IssueErrorForAllPendingRequestsForAccount(
      account_id, error);
}

void FakeProfileOAuth2TokenService::IssueTokenForScope(
    const OAuth2AccessTokenManager::ScopeSet& scope,
    const std::string& access_token,
    const base::Time& expiration) {
  GetFakeAccessTokenManager()->IssueTokenForScope(scope, access_token,
                                                  expiration);
}

void FakeProfileOAuth2TokenService::IssueTokenForScope(
    const OAuth2AccessTokenManager::ScopeSet& scope,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  GetFakeAccessTokenManager()->IssueTokenForScope(scope, token_response);
}

void FakeProfileOAuth2TokenService::IssueErrorForScope(
    const OAuth2AccessTokenManager::ScopeSet& scope,
    const GoogleServiceAuthError& error) {
  GetFakeAccessTokenManager()->IssueErrorForScope(scope, error);
}

void FakeProfileOAuth2TokenService::IssueErrorForAllPendingRequests(
    const GoogleServiceAuthError& error) {
  GetFakeAccessTokenManager()->IssueErrorForAllPendingRequests(error);
}

void FakeProfileOAuth2TokenService::
    set_auto_post_fetch_response_on_message_loop(bool auto_post_response) {
  GetFakeAccessTokenManager()->set_auto_post_fetch_response_on_message_loop(
      auto_post_response);
}

void FakeProfileOAuth2TokenService::IssueTokenForAllPendingRequests(
    const std::string& access_token,
    const base::Time& expiration) {
  GetFakeAccessTokenManager()->IssueTokenForAllPendingRequests(access_token,
                                                               expiration);
}

void FakeProfileOAuth2TokenService::IssueTokenForAllPendingRequests(
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  GetFakeAccessTokenManager()->IssueTokenForAllPendingRequests(token_response);
}

std::vector<FakeOAuth2AccessTokenManager::PendingRequest>
FakeProfileOAuth2TokenService::GetPendingRequests() {
  return GetFakeAccessTokenManager()->GetPendingRequests();
}

FakeOAuth2AccessTokenManager*
FakeProfileOAuth2TokenService::GetFakeAccessTokenManager() {
  return static_cast<FakeOAuth2AccessTokenManager*>(GetAccessTokenManager());
}

bool FakeProfileOAuth2TokenService::IsFakeProfileOAuth2TokenServiceForTesting()
    const {
  return true;
}
