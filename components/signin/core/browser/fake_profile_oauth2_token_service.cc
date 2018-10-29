// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "google_apis/gaia/fake_oauth2_token_service_delegate.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

FakeProfileOAuth2TokenService::PendingRequest::PendingRequest() {}

FakeProfileOAuth2TokenService::PendingRequest::PendingRequest(
    const PendingRequest& other) = default;

FakeProfileOAuth2TokenService::PendingRequest::~PendingRequest() {}

FakeProfileOAuth2TokenService::FakeProfileOAuth2TokenService(
    PrefService* user_prefs)
    : FakeProfileOAuth2TokenService(
          user_prefs,
          std::make_unique<FakeOAuth2TokenServiceDelegate>()) {}

FakeProfileOAuth2TokenService::FakeProfileOAuth2TokenService(
    PrefService* user_prefs,
    std::unique_ptr<OAuth2TokenServiceDelegate> delegate)
    : ProfileOAuth2TokenService(user_prefs, std::move(delegate)),
      auto_post_fetch_response_on_message_loop_(false),
      weak_ptr_factory_(this) {}

FakeProfileOAuth2TokenService::~FakeProfileOAuth2TokenService() {}

void FakeProfileOAuth2TokenService::IssueAllTokensForAccount(
    const std::string& account_id,
    const std::string& access_token,
    const base::Time& expiration) {
  DCHECK(!auto_post_fetch_response_on_message_loop_);
  CompleteRequests(account_id, true, ScopeSet(),
                   GoogleServiceAuthError::AuthErrorNone(),
                   OAuth2AccessTokenConsumer::TokenResponse(
                       access_token, expiration, std::string() /* id_token */));
}

void FakeProfileOAuth2TokenService::IssueAllTokensForAccount(
    const std::string& account_id,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  DCHECK(!auto_post_fetch_response_on_message_loop_);
  CompleteRequests(account_id, true, ScopeSet(),
                   GoogleServiceAuthError::AuthErrorNone(), token_response);
}

void FakeProfileOAuth2TokenService::IssueErrorForAllPendingRequestsForAccount(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  DCHECK(!auto_post_fetch_response_on_message_loop_);
  CompleteRequests(account_id, true, ScopeSet(), error,
                   OAuth2AccessTokenConsumer::TokenResponse());
}

void FakeProfileOAuth2TokenService::IssueTokenForScope(
    const ScopeSet& scope,
    const std::string& access_token,
    const base::Time& expiration) {
  DCHECK(!auto_post_fetch_response_on_message_loop_);
  CompleteRequests("", false, scope, GoogleServiceAuthError::AuthErrorNone(),
                   OAuth2AccessTokenConsumer::TokenResponse(
                       access_token, expiration, std::string() /* id_token */));
}

void FakeProfileOAuth2TokenService::IssueTokenForScope(
    const ScopeSet& scope,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  DCHECK(!auto_post_fetch_response_on_message_loop_);
  CompleteRequests("", false, scope, GoogleServiceAuthError::AuthErrorNone(),
                   token_response);
}

void FakeProfileOAuth2TokenService::IssueErrorForScope(
    const ScopeSet& scope,
    const GoogleServiceAuthError& error) {
  DCHECK(!auto_post_fetch_response_on_message_loop_);
  CompleteRequests("", false, scope, error,
                   OAuth2AccessTokenConsumer::TokenResponse());
}

void FakeProfileOAuth2TokenService::IssueErrorForAllPendingRequests(
    const GoogleServiceAuthError& error) {
  DCHECK(!auto_post_fetch_response_on_message_loop_);
  CompleteRequests("", true, ScopeSet(), error,
                   OAuth2AccessTokenConsumer::TokenResponse());
}

void FakeProfileOAuth2TokenService::IssueTokenForAllPendingRequests(
    const std::string& access_token,
    const base::Time& expiration) {
  DCHECK(!auto_post_fetch_response_on_message_loop_);
  CompleteRequests("", true, ScopeSet(),
                   GoogleServiceAuthError::AuthErrorNone(),
                   OAuth2AccessTokenConsumer::TokenResponse(
                       access_token, expiration, std::string() /* id_token */));
}

void FakeProfileOAuth2TokenService::IssueTokenForAllPendingRequests(
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  DCHECK(!auto_post_fetch_response_on_message_loop_);
  CompleteRequests("", true, ScopeSet(),
                   GoogleServiceAuthError::AuthErrorNone(), token_response);
}

void FakeProfileOAuth2TokenService::UpdateAuthErrorForTesting(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  ProfileOAuth2TokenService::UpdateAuthError(account_id, error);
}

void FakeProfileOAuth2TokenService::CompleteRequests(
    const std::string& account_id,
    bool all_scopes,
    const ScopeSet& scope,
    const GoogleServiceAuthError& error,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  std::vector<FakeProfileOAuth2TokenService::PendingRequest> requests =
      GetPendingRequests();

  // Walk the requests and notify the callbacks.
  for (auto it = requests.begin(); it != requests.end(); ++it) {
    DCHECK(it->request);

    bool scope_matches = all_scopes || it->scopes == scope;
    bool account_matches = account_id.empty() || account_id == it->account_id;
    if (account_matches && scope_matches) {
      it->request->InformConsumer(
          error, OAuth2AccessTokenConsumer::TokenResponse(
                     token_response.access_token,
                     token_response.expiration_time, token_response.id_token));
    }
  }
}

std::vector<FakeProfileOAuth2TokenService::PendingRequest>
FakeProfileOAuth2TokenService::GetPendingRequests() {
  std::vector<PendingRequest> valid_requests;
  for (auto it = pending_requests_.begin(); it != pending_requests_.end();
       ++it) {
    if (it->request)
      valid_requests.push_back(*it);
  }
  return valid_requests;
}

void FakeProfileOAuth2TokenService::CancelAllRequests() {
  CompleteRequests(
      "", true, ScopeSet(),
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED),
      OAuth2AccessTokenConsumer::TokenResponse());
}

void FakeProfileOAuth2TokenService::CancelRequestsForAccount(
    const std::string& account_id) {
  CompleteRequests(
      account_id, true, ScopeSet(),
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED),
      OAuth2AccessTokenConsumer::TokenResponse());
}

void FakeProfileOAuth2TokenService::FetchOAuth2Token(
    RequestImpl* request,
    const std::string& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& client_id,
    const std::string& client_secret,
    const ScopeSet& scopes) {
  PendingRequest pending_request;
  pending_request.account_id = account_id;
  pending_request.client_id = client_id;
  pending_request.client_secret = client_secret;
  pending_request.url_loader_factory = url_loader_factory;
  pending_request.scopes = scopes;
  pending_request.request = request->AsWeakPtr();
  pending_requests_.push_back(pending_request);

  if (auto_post_fetch_response_on_message_loop_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeProfileOAuth2TokenService::CompleteRequests,
                       weak_ptr_factory_.GetWeakPtr(), account_id,
                       /*all_scoped=*/true, ScopeSet(),
                       GoogleServiceAuthError::AuthErrorNone(),
                       OAuth2AccessTokenConsumer::TokenResponse(
                           "access_token", base::Time::Max(), std::string())));
  }
}

void FakeProfileOAuth2TokenService::InvalidateAccessTokenImpl(
    const std::string& account_id,
    const std::string& client_id,
    const ScopeSet& scopes,
    const std::string& access_token) {
  // Do nothing, as we don't have a cache from which to remove the token.
}
