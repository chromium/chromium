// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/child_account_info_fetcher_impl.h"

#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/signin/core/browser/account_fetcher_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "google/cacheinvalidation/types.pb.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// TODO(maroun): Remove this file.

namespace {

const char kFetcherId[] = "ChildAccountInfoFetcherImpl";

// Exponential backoff policy on service flag fetching failure.
const net::BackoffEntry::Policy kBackoffPolicy = {
  0,  // Number of initial errors to ignore without backoff.
  2000,  // Initial delay for backoff in ms.
  2,  // Factor to multiply waiting time by.
  0.2,  // Fuzzing percentage. 20% will spread requests randomly between
        // 80-100% of the calculated time.
  1000 * 60 * 60* 4,  // Maximum time to delay requests by (4 hours).
  -1,  // Don't discard entry even if unused.
  false,  // Don't use the initial delay unless the last request was an error.
};

// The invalidation object ID used for child account graduation event.
// The syntax is:
// 'U' -> This is a user specific invalidation.
// 'CA' -> Namespace used for all ChildAccount invalidations.
// 'GRAD' -> Indicates the actual event i.e. child account graduation.
const char kChildAccountGraduationId[] = "UCAGRAD";

}  // namespace

ChildAccountInfoFetcherImpl::ChildAccountInfoFetcherImpl(
    const std::string& account_id,
    AccountFetcherService* fetcher_service,
    OAuth2TokenService* token_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    invalidation::InvalidationService* invalidation_service)
    : OAuth2TokenService::Consumer(kFetcherId),
      token_service_(token_service),
      url_loader_factory_(url_loader_factory),
      fetcher_service_(fetcher_service),
      invalidation_service_(invalidation_service),
      account_id_(account_id),
      backoff_(&kBackoffPolicy),
      fetch_in_progress_(false) {
  TRACE_EVENT_ASYNC_BEGIN1("AccountFetcherService", kFetcherId, this,
                           "account_id", account_id);
  // Invalidation service may not be available in tests.
  if (invalidation_service_) {
    invalidation_service_->RegisterInvalidationHandler(this);
    syncer::ObjectIdSet ids;
    ids.insert(invalidation::ObjectId(
        ipc::invalidation::ObjectSource::CHROME_COMPONENTS,
        kChildAccountGraduationId));
    bool insert_success =
        invalidation_service_->UpdateRegisteredInvalidationIds(this, ids);
    DCHECK(insert_success);
  }
  FetchIfNotInProgress();
}

ChildAccountInfoFetcherImpl::~ChildAccountInfoFetcherImpl() {
  TRACE_EVENT_ASYNC_END0("AccountFetcherService", kFetcherId, this);
  if (invalidation_service_)
    UnregisterInvalidationHandler();
}

void ChildAccountInfoFetcherImpl::FetchIfNotInProgress() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (fetch_in_progress_)
    return;
  fetch_in_progress_ = true;
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kOAuth1LoginScope);
  login_token_request_ =
      token_service_->StartRequest(account_id_, scopes, this);
}

void ChildAccountInfoFetcherImpl::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  TRACE_EVENT_ASYNC_STEP_PAST0("AccountFetcherService", kFetcherId, this,
                               "OnGetTokenSuccess");
  DCHECK_EQ(request, login_token_request_.get());

  gaia_auth_fetcher_ = fetcher_service_->signin_client_->CreateGaiaAuthFetcher(
      this, GaiaConstants::kChromeSource, url_loader_factory_);
  gaia_auth_fetcher_->StartOAuthLogin(token_response.access_token,
                                      GaiaConstants::kGaiaService);
}

void ChildAccountInfoFetcherImpl::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  HandleFailure();
}

void ChildAccountInfoFetcherImpl::OnClientLoginSuccess(
    const ClientLoginResult& result) {
  gaia_auth_fetcher_->StartGetUserInfo(result.lsid);
}

void ChildAccountInfoFetcherImpl::OnClientLoginFailure(
    const GoogleServiceAuthError& error) {
  HandleFailure();
}

void ChildAccountInfoFetcherImpl::OnGetUserInfoSuccess(
    const UserInfoMap& data) {
  auto services_iter = data.find("allServices");
  if (services_iter != data.end()) {
    std::vector<std::string> service_flags = base::SplitString(
        services_iter->second, ",", base::TRIM_WHITESPACE,
        base::SPLIT_WANT_ALL);
    bool is_child_account = base::ContainsValue(
        service_flags, AccountTrackerService::kChildAccountServiceFlag);
    if (!is_child_account && invalidation_service_) {
      // Don't bother listening for invalidations as a non-child account can't
      // become a child account.
      bool insert_success =
          invalidation_service_->UpdateRegisteredInvalidationIds(
              this, syncer::ObjectIdSet());
      DCHECK(insert_success);
      UnregisterInvalidationHandler();
    }
    fetcher_service_->SetIsChildAccount(account_id_, is_child_account);
  } else {
    DLOG(ERROR) << "ChildAccountInfoFetcherImpl::OnGetUserInfoSuccess: "
                << "GetUserInfo response didn't include allServices field.";
  }
  fetch_in_progress_ = false;
}

void ChildAccountInfoFetcherImpl::OnGetUserInfoFailure(
    const GoogleServiceAuthError& error) {
  HandleFailure();
}

void ChildAccountInfoFetcherImpl::HandleFailure() {
  fetch_in_progress_ = false;
  backoff_.InformOfRequest(false);
  timer_.Start(FROM_HERE, backoff_.GetTimeUntilRelease(), this,
               &ChildAccountInfoFetcherImpl::FetchIfNotInProgress);
}

void ChildAccountInfoFetcherImpl::UnregisterInvalidationHandler() {
  invalidation_service_->UnregisterInvalidationHandler(this);
  invalidation_service_ = nullptr;
}

void ChildAccountInfoFetcherImpl::OnInvalidatorStateChange(
    syncer::InvalidatorState state) {
  if (state == syncer::INVALIDATOR_SHUTTING_DOWN)
    UnregisterInvalidationHandler();
}

void ChildAccountInfoFetcherImpl::OnIncomingInvalidation(
    const syncer::ObjectIdInvalidationMap& invalidation_map) {
  FetchIfNotInProgress();
  invalidation_map.AcknowledgeAll();
}

std::string ChildAccountInfoFetcherImpl::GetOwnerName() const {
  return std::string(kFetcherId);
}
