// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/breaking_news/subscription_manager_impl.h"

#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/stringprintf.h"
#include "components/ntp_snippets/breaking_news/breaking_news_metrics.h"
#include "components/ntp_snippets/breaking_news/subscription_json_request.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/service/variations_service.h"
#include "net/base/url_util.h"
#include "services/identity/public/cpp/primary_account_access_token_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ntp_snippets {

using internal::SubscriptionJsonRequest;

namespace {

const char kApiKeyParamName[] = "key";
const char kAuthorizationRequestHeaderFormat[] = "Bearer %s";

}  // namespace

SubscriptionManagerImpl::SubscriptionManagerImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service,
    variations::VariationsService* variations_service,
    identity::IdentityManager* identity_manager,
    const std::string& locale,
    const std::string& api_key,
    const GURL& subscribe_url,
    const GURL& unsubscribe_url)
    : url_loader_factory_(std::move(url_loader_factory)),
      pref_service_(pref_service),
      variations_service_(variations_service),
      identity_manager_(identity_manager),
      locale_(locale),
      api_key_(api_key),
      subscribe_url_(subscribe_url),
      unsubscribe_url_(unsubscribe_url) {
  identity_manager_->AddObserver(this);
}

SubscriptionManagerImpl::~SubscriptionManagerImpl() {
  identity_manager_->RemoveObserver(this);
}

void SubscriptionManagerImpl::Subscribe(const std::string& subscription_token) {
  // If there is a request in flight, cancel it.
  if (request_) {
    request_ = nullptr;
  }
  if (identity_manager_->HasPrimaryAccount()) {
    StartAccessTokenRequest(subscription_token);
  } else {
    SubscribeInternal(subscription_token, /*access_token=*/std::string());
  }
}

void SubscriptionManagerImpl::SubscribeInternal(
    const std::string& subscription_token,
    const std::string& access_token) {
  SubscriptionJsonRequest::Builder builder;
  builder.SetToken(subscription_token).SetUrlLoaderFactory(url_loader_factory_);

  if (!access_token.empty()) {
    builder.SetUrl(subscribe_url_);
    builder.SetAuthenticationHeader(base::StringPrintf(
        kAuthorizationRequestHeaderFormat, access_token.c_str()));
  } else {
    // When not providing OAuth token, we need to pass the Google API key.
    builder.SetUrl(
        net::AppendQueryParameter(subscribe_url_, kApiKeyParamName, api_key_));
  }

  builder.SetLocale(locale_);
  builder.SetCountryCode(variations_service_
                             ? variations_service_->GetStoredPermanentCountry()
                             : "");

  request_ = builder.Build();
  request_->Start(base::BindOnce(&SubscriptionManagerImpl::DidSubscribe,
                                 base::Unretained(this), subscription_token,
                                 /*is_authenticated=*/!access_token.empty()));
}

void SubscriptionManagerImpl::StartAccessTokenRequest(
    const std::string& subscription_token) {
  // If there is already an ongoing token request, destroy it.
  if (access_token_fetcher_) {
    access_token_fetcher_ = nullptr;
  }

  identity::ScopeSet scopes = {kContentSuggestionsApiScope};
  access_token_fetcher_ = std::make_unique<
      identity::PrimaryAccountAccessTokenFetcher>(
      "ntp_snippets", identity_manager_, scopes,
      base::BindOnce(&SubscriptionManagerImpl::AccessTokenFetchFinished,
                     base::Unretained(this), subscription_token),
      identity::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);
}

void SubscriptionManagerImpl::AccessTokenFetchFinished(
    const std::string& subscription_token,
    GoogleServiceAuthError error,
    identity::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    // In case of error, we will retry on next Chrome restart.
    return;
  }
  DCHECK(!access_token_info.token.empty());
  SubscribeInternal(subscription_token, access_token_info.token);
}

void SubscriptionManagerImpl::DidSubscribe(
    const std::string& subscription_token,
    bool is_authenticated,
    const Status& status) {
  metrics::OnSubscriptionRequestCompleted(status);

  // Delete the request only after we leave this method (which is called from
  // the request itself).
  std::unique_ptr<internal::SubscriptionJsonRequest> request_deleter(
      std::move(request_));

  switch (status.code) {
    case StatusCode::SUCCESS:
      // In case of successful subscription, store the same data used for
      // subscription in order to be able to resubscribe in case of data
      // change.
      // TODO(mamir): Store region and language.
      pref_service_->SetString(prefs::kBreakingNewsSubscriptionDataToken,
                               subscription_token);
      pref_service_->SetBoolean(
          prefs::kBreakingNewsSubscriptionDataIsAuthenticated,
          is_authenticated);
      break;
    default:
      // TODO(mamir): Handle failure.
      break;
  }
}

void SubscriptionManagerImpl::Unsubscribe() {
  std::string token =
      pref_service_->GetString(prefs::kBreakingNewsSubscriptionDataToken);
  ResubscribeInternal(/*old_token=*/token, /*new_token=*/std::string());
}

void SubscriptionManagerImpl::ResubscribeInternal(
    const std::string& old_token,
    const std::string& new_token) {
  // If there is an request in flight, cancel it.
  if (request_) {
    request_ = nullptr;
  }

  SubscriptionJsonRequest::Builder builder;
  builder.SetToken(old_token).SetUrlLoaderFactory(url_loader_factory_);
  builder.SetUrl(
      net::AppendQueryParameter(unsubscribe_url_, kApiKeyParamName, api_key_));

  request_ = builder.Build();
  request_->Start(base::BindOnce(&SubscriptionManagerImpl::DidUnsubscribe,
                                 base::Unretained(this), new_token));
}

bool SubscriptionManagerImpl::IsSubscribed() {
  std::string subscription_token =
      pref_service_->GetString(prefs::kBreakingNewsSubscriptionDataToken);
  return !subscription_token.empty();
}

bool SubscriptionManagerImpl::NeedsToResubscribe() {
  // Check if authentication state changed after subscription.
  bool is_auth_subscribe = pref_service_->GetBoolean(
      prefs::kBreakingNewsSubscriptionDataIsAuthenticated);
  bool is_authenticated = identity_manager_->HasPrimaryAccount();
  return is_auth_subscribe != is_authenticated;
}

void SubscriptionManagerImpl::Resubscribe(const std::string& new_token) {
  std::string old_token =
      pref_service_->GetString(prefs::kBreakingNewsSubscriptionDataToken);
  if (old_token == new_token) {
    // If the token didn't change, subscribe directly. The server handles the
    // unsubscription if previous subscriptions exists.
    Subscribe(old_token);
  } else {
    ResubscribeInternal(old_token, new_token);
  }
}

void SubscriptionManagerImpl::DidUnsubscribe(const std::string& new_token,
                                             const Status& status) {
  metrics::OnUnsubscriptionRequestCompleted(status);

  // Delete the request only after we leave this method (which is called from
  // the request itself).
  std::unique_ptr<internal::SubscriptionJsonRequest> request_deleter(
      std::move(request_));

  switch (status.code) {
    case StatusCode::SUCCESS:
      // In case of successful unsubscription, clear the previously stored data.
      // TODO(mamir): Clear stored region and language.
      pref_service_->ClearPref(prefs::kBreakingNewsSubscriptionDataToken);
      pref_service_->ClearPref(
          prefs::kBreakingNewsSubscriptionDataIsAuthenticated);
      if (!new_token.empty()) {
        Subscribe(new_token);
      }
      break;
    default:
      // TODO(mamir): Handle failure.
      break;
  }
}

void SubscriptionManagerImpl::OnPrimaryAccountSet(
    const AccountInfo& account_info) {
  SigninStatusChanged();
}

void SubscriptionManagerImpl::OnPrimaryAccountCleared(
    const AccountInfo& account_info) {
  SigninStatusChanged();
}

void SubscriptionManagerImpl::SigninStatusChanged() {
  // If subscribed already, resubscribe.
  if (IsSubscribed()) {
    if (request_) {
      request_ = nullptr;
    }
    std::string token =
        pref_service_->GetString(prefs::kBreakingNewsSubscriptionDataToken);
    Subscribe(token);
  }
}

// static
void SubscriptionManagerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kBreakingNewsSubscriptionDataToken,
                               std::string());
  registry->RegisterBooleanPref(
      prefs::kBreakingNewsSubscriptionDataIsAuthenticated, false);
}

// TODO(vitaliii): Add a test to ensure that this clears everything.
// static
void SubscriptionManagerImpl::ClearProfilePrefs(PrefService* pref_service) {
  pref_service->ClearPref(prefs::kBreakingNewsSubscriptionDataToken);
  pref_service->ClearPref(prefs::kBreakingNewsSubscriptionDataIsAuthenticated);
}

}  // namespace ntp_snippets
