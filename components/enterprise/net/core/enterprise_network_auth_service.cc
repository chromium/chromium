// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/enterprise_network_auth_service.h"

#include <map>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/net/core/auth_scope_metadata.h"
#include "components/enterprise/net/core/utils.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "url/gurl.h"

namespace enterprise_net {

namespace {

// Maps a GoogleServiceAuthError to the corresponding TokenFetchError.
// Retriable cases are marked as kTransientError, while permanent failures
// return their specific enum state.
TokenFetchError GoogleServiceAuthErrorToTokenFetchError(
    const GoogleServiceAuthError& error) {
  switch (error.state()) {
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      return TokenFetchError::kTransientError;
    case GoogleServiceAuthError::ACCOUNT_NOT_FOUND:
      return TokenFetchError::kNoPrimaryAccount;
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
      return TokenFetchError::kInvalidCredentials;
    case GoogleServiceAuthError::REQUEST_CANCELED:
      return TokenFetchError::kCanceled;
    default:
      return TokenFetchError::kAuthError;
  }
}

// Helper to record Enterprise.NetworkAuth.TokenFetch* histogram metrics and run
// the result callback from a single centralized location.
void RecordResultAndRunCallback(
    EnterpriseNetworkAuthService::AccessTokenCallback callback,
    AccessTokenResult result,
    base::TimeTicks start_time) {
  TokenFetchError error_for_metrics =
      result.has_value() ? kNoErrorForMetrics : result.error();
  base::UmaHistogramEnumeration("Enterprise.NetworkAuth.TokenFetchError",
                                error_for_metrics);
  base::UmaHistogramMediumTimes(
      result.has_value() ? "Enterprise.NetworkAuth.TokenFetchLatency.Success"
                         : "Enterprise.NetworkAuth.TokenFetchLatency.Failure",
      base::TimeTicks::Now() - start_time);
  std::move(callback).Run(std::move(result));
}

}  // namespace

std::string_view TokenFetchErrorToString(TokenFetchError error) {
  switch (error) {
    case TokenFetchError::kNoPrimaryAccount:
      return "no_primary_account";
    case TokenFetchError::kUnmanagedUser:
      return "unmanaged_user";
    case TokenFetchError::kUnsupportedScope:
      return "unsupported_scope";
    case TokenFetchError::kInvalidCredentials:
      return "invalid_credentials";
    case TokenFetchError::kTransientError:
      return "transient_error";
    case TokenFetchError::kAuthError:
      return "auth_error";
    case TokenFetchError::kCanceled:
      return "canceled";
    case TokenFetchError::kInapplicableServer:
      return "inapplicable_server";
  }
}

EnterpriseNetworkAuthService::PendingManagedStatusCheck::
    PendingManagedStatusCheck() = default;
EnterpriseNetworkAuthService::PendingManagedStatusCheck::
    PendingManagedStatusCheck(
        std::unique_ptr<signin::AccountManagedStatusFinder> finder,
        signin::OAuthConsumerId consumer_id,
        AccessTokenCallback callback,
        base::TimeTicks start_time)
    : finder(std::move(finder)),
      consumer_id(consumer_id),
      callback(std::move(callback)),
      start_time(start_time) {}
EnterpriseNetworkAuthService::PendingManagedStatusCheck::
    PendingManagedStatusCheck(PendingManagedStatusCheck&&) noexcept = default;
EnterpriseNetworkAuthService::PendingManagedStatusCheck&
EnterpriseNetworkAuthService::PendingManagedStatusCheck::operator=(
    PendingManagedStatusCheck&&) noexcept = default;
EnterpriseNetworkAuthService::PendingManagedStatusCheck::
    ~PendingManagedStatusCheck() = default;

EnterpriseNetworkAuthService::PendingTokenFetch::PendingTokenFetch() = default;
EnterpriseNetworkAuthService::PendingTokenFetch::PendingTokenFetch(
    std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> fetcher,
    AccessTokenCallback callback,
    base::TimeTicks start_time)
    : fetcher(std::move(fetcher)),
      callback(std::move(callback)),
      start_time(start_time) {}
EnterpriseNetworkAuthService::PendingTokenFetch::PendingTokenFetch(
    PendingTokenFetch&&) noexcept = default;
EnterpriseNetworkAuthService::PendingTokenFetch&
EnterpriseNetworkAuthService::PendingTokenFetch::operator=(
    PendingTokenFetch&&) noexcept = default;
EnterpriseNetworkAuthService::PendingTokenFetch::~PendingTokenFetch() = default;

EnterpriseNetworkAuthService::EnterpriseNetworkAuthService()
    : channel_(version_info::Channel::UNKNOWN) {}

EnterpriseNetworkAuthService::EnterpriseNetworkAuthService(
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    enterprise::ProfileIdService* profile_id_service,
    version_info::Channel channel)
    : identity_manager_(identity_manager),
      pref_service_(pref_service),
      profile_id_service_(profile_id_service),
      channel_(channel) {
  CHECK(identity_manager_);
  CHECK(pref_service_);
  CHECK(profile_id_service_);
  identity_manager_observation_.Observe(identity_manager_);
}

EnterpriseNetworkAuthService::~EnterpriseNetworkAuthService() = default;

void EnterpriseNetworkAuthService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void EnterpriseNetworkAuthService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void EnterpriseNetworkAuthService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  ClearPendingTokenFetches();
  for (auto& observer : observers_) {
    observer.OnAccountStateChanged();
  }
}

void EnterpriseNetworkAuthService::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (identity_manager_ &&
      account_info == identity_manager_->GetPrimaryAccountInfo(
                          signin::ConsentLevel::kSignin)) {
    for (auto& observer : observers_) {
      observer.OnAccountStateChanged();
    }
  }
}

void EnterpriseNetworkAuthService::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  if (identity_manager_ &&
      account_info == identity_manager_->GetPrimaryAccountInfo(
                          signin::ConsentLevel::kSignin) &&
      error.state() == GoogleServiceAuthError::NONE) {
    for (auto& observer : observers_) {
      observer.OnAccountStateChanged();
    }
  }
}

void EnterpriseNetworkAuthService::OnRefreshTokensLoaded() {
  for (auto& observer : observers_) {
    observer.OnAccountStateChanged();
  }
}

void EnterpriseNetworkAuthService::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  identity_manager_observation_.Reset();
}

void EnterpriseNetworkAuthService::Shutdown() {
  identity_manager_observation_.Reset();
  ClearPendingTokenFetches();
  observers_.Clear();
}

void EnterpriseNetworkAuthService::FetchAccessToken(
    AuthScope scope,
    const GURL& destination_url,
    AccessTokenCallback callback) {
  const base::TimeTicks start_time = base::TimeTicks::Now();
  std::optional<signin::OAuthConsumerId> consumer_id =
      GetOAuthConsumerIdForScope(scope);
  if (!consumer_id.has_value()) {
    RecordResultAndRunCallback(
        std::move(callback),
        base::unexpected(TokenFetchError::kUnsupportedScope), start_time);
    return;
  }

  if (!IsDestinationAllowedForScope(scope, destination_url, channel_)) {
    RecordResultAndRunCallback(
        std::move(callback),
        base::unexpected(TokenFetchError::kInapplicableServer), start_time);
    return;
  }

  CoreAccountInfo primary_account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (primary_account.IsEmpty()) {
    RecordResultAndRunCallback(
        std::move(callback),
        base::unexpected(TokenFetchError::kNoPrimaryAccount), start_time);
    return;
  }

  if (identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account.account_id)) {
    RecordResultAndRunCallback(
        std::move(callback),
        base::unexpected(TokenFetchError::kInvalidCredentials), start_time);
    return;
  }

  auto pending_check = std::make_unique<PendingManagedStatusCheck>();
  pending_check->consumer_id = *consumer_id;
  pending_check->callback = std::move(callback);
  pending_check->start_time = start_time;

  int check_id = pending_status_checks_.Add(std::move(pending_check));

  auto finder = std::make_unique<signin::AccountManagedStatusFinder>(
      identity_manager_, primary_account,
      base::BindOnce(&EnterpriseNetworkAuthService::OnManagedStatusChecked,
                     weak_factory_.GetWeakPtr(), check_id));

  pending_status_checks_.Lookup(check_id)->finder = std::move(finder);

  // Synchronous case.
  if (pending_status_checks_.Lookup(check_id)->finder->GetOutcome() !=
      signin::AccountManagedStatusFinderOutcome::kPending) {
    OnManagedStatusChecked(check_id);
  }
}

net::HttpRequestHeaders EnterpriseNetworkAuthService::ResolveExtraHeaders(
    const std::vector<ProxyExtraHeader>& extra_headers) const {
  std::string profile_id;
  if (profile_id_service_) {
    std::optional<std::string> pid = profile_id_service_->GetProfileId();
    if (pid.has_value()) {
      profile_id = *pid;
    }
  }
  std::string accept_languages;
  // "intl.accept_languages" is Chrome's standard pref containing HTTP
  // Accept-Language formatted string (e.g. "en-US,en;q=0.9").
  if (pref_service_ && pref_service_->FindPreference("intl.accept_languages")) {
    accept_languages = pref_service_->GetString("intl.accept_languages");
  }
  return enterprise_net::ResolveExtraHeadersWithValues(
      extra_headers, profile_id, accept_languages);
}

void EnterpriseNetworkAuthService::ClearPendingTokenFetches() {
  // Clear requests waiting for managed status check.
  for (base::IDMap<std::unique_ptr<PendingManagedStatusCheck>>::iterator it(
           &pending_status_checks_);
       !it.IsAtEnd(); it.Advance()) {
    RecordResultAndRunCallback(std::move(it.GetCurrentValue()->callback),
                               base::unexpected(TokenFetchError::kCanceled),
                               it.GetCurrentValue()->start_time);
  }
  pending_status_checks_.Clear();

  // Clear requests waiting for access token.
  for (base::IDMap<std::unique_ptr<PendingTokenFetch>>::iterator it(
           &access_token_fetchers_);
       !it.IsAtEnd(); it.Advance()) {
    RecordResultAndRunCallback(std::move(it.GetCurrentValue()->callback),
                               base::unexpected(TokenFetchError::kCanceled),
                               it.GetCurrentValue()->start_time);
  }
  access_token_fetchers_.Clear();
}

void EnterpriseNetworkAuthService::StartAccessTokenFetch(
    signin::OAuthConsumerId consumer_id,
    AccessTokenCallback callback,
    base::TimeTicks start_time) {
  auto token_fetcher =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          consumer_id, identity_manager_,
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
          signin::ConsentLevel::kSignin);

  auto pending_fetch = std::make_unique<PendingTokenFetch>(
      std::move(token_fetcher), std::move(callback), start_time);
  auto* token_fetcher_ptr = pending_fetch->fetcher.get();

  int fetch_id = access_token_fetchers_.Add(std::move(pending_fetch));

  token_fetcher_ptr->Start(
      base::BindOnce(&EnterpriseNetworkAuthService::OnAccessTokenFetched,
                     weak_factory_.GetWeakPtr(), fetch_id));
}

void EnterpriseNetworkAuthService::OnManagedStatusChecked(int check_id) {
  auto* pending_check = pending_status_checks_.Lookup(check_id);
  if (!pending_check) {
    return;
  }

  signin::OAuthConsumerId consumer_id = pending_check->consumer_id;
  AccessTokenCallback callback = std::move(pending_check->callback);
  base::TimeTicks start_time = pending_check->start_time;
  signin::AccountManagedStatusFinderOutcome outcome =
      pending_check->finder->GetOutcome();

  pending_status_checks_.Remove(check_id);

  if (outcome != signin::AccountManagedStatusFinderOutcome::kEnterprise &&
      outcome !=
          signin::AccountManagedStatusFinderOutcome::kEnterpriseGoogleDotCom) {
    RecordResultAndRunCallback(
        std::move(callback), base::unexpected(TokenFetchError::kUnmanagedUser),
        start_time);
    return;
  }

  StartAccessTokenFetch(consumer_id, std::move(callback), start_time);
}

void EnterpriseNetworkAuthService::OnAccessTokenFetched(
    int fetch_id,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  auto* pending_fetch = access_token_fetchers_.Lookup(fetch_id);
  if (!pending_fetch) {
    return;
  }

  AccessTokenCallback callback = std::move(pending_fetch->callback);
  base::TimeTicks start_time = pending_fetch->start_time;
  access_token_fetchers_.Remove(fetch_id);

  if (error.state() != GoogleServiceAuthError::NONE) {
    TokenFetchError fetch_error =
        GoogleServiceAuthErrorToTokenFetchError(error);
    RecordResultAndRunCallback(std::move(callback),
                               base::unexpected(fetch_error), start_time);
    return;
  }

  RecordResultAndRunCallback(std::move(callback), access_token_info.token,
                             start_time);
}

}  // namespace enterprise_net
