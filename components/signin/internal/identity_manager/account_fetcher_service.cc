// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_fetcher_service.h"

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/internal/identity_manager/account_capabilities_fetcher.h"
#include "components/signin/internal/identity_manager/account_capabilities_fetcher_factory.h"
#include "components/signin/internal/identity_manager/account_info_fetcher.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "components/signin/internal/identity_manager/child_account_info_fetcher_android.h"
#include "components/signin/public/identity_manager/tribool.h"
#endif

namespace {

const base::TimeDelta kRefreshFromTokenServiceDelay = base::Hours(24);

}  // namespace

const char kImageFetcherUmaClient[] = "AccountFetcherService";

// This pref used to be in the AccountTrackerService, hence its string value.
const char AccountFetcherService::kLastUpdatePref[] =
    "account_tracker_service_last_update";

// AccountFetcherService implementation
AccountFetcherService::AccountFetcherService() = default;

AccountFetcherService::~AccountFetcherService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_ANDROID)
  // child_info_request_ is an invalidation handler and needs to be
  // unregistered during the lifetime of the invalidation service.
  child_info_request_.reset();
#endif
}

// static
void AccountFetcherService::RegisterPrefs(PrefRegistrySimple* user_prefs) {
  user_prefs->RegisterTimePref(AccountFetcherService::kLastUpdatePref,
                               base::Time());
}

void AccountFetcherService::Initialize(
    SigninClient* signin_client,
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service,
    std::unique_ptr<image_fetcher::ImageDecoder> image_decoder,
    std::unique_ptr<AccountCapabilitiesFetcherFactory>
        account_capabilities_fetcher_factory) {
  DCHECK(signin_client);
  DCHECK(!signin_client_);
  signin_client_ = signin_client;
  DCHECK(account_tracker_service);
  DCHECK(!account_tracker_service_);
  account_tracker_service_ = account_tracker_service;
  DCHECK(token_service);
  DCHECK(!token_service_);
  token_service_ = token_service;
  token_service_observation_.Observe(token_service_);

  DCHECK(image_decoder);
  DCHECK(!image_decoder_);
  image_decoder_ = std::move(image_decoder);
  DCHECK(!account_capabilities_fetcher_factory_);
  DCHECK(account_capabilities_fetcher_factory);
  account_capabilities_fetcher_factory_ =
      std::move(account_capabilities_fetcher_factory);

  repeating_timer_ = std::make_unique<signin::PersistentRepeatingTimer>(
      signin_client_->GetPrefs(), AccountFetcherService::kLastUpdatePref,
      kRefreshFromTokenServiceDelay,
      base::BindRepeating(&AccountFetcherService::RefreshAllAccountInfo,
                          base::Unretained(this),
                          /*only_fetch_if_invalid=*/false));

  // Tokens may have already been loaded and we will not receive a
  // notification-on-registration for |token_service_->AddObserver(this)| few
  // lines above.
  if (token_service_->AreAllCredentialsLoaded())
    OnRefreshTokensLoaded();
}

bool AccountFetcherService::IsAllUserInfoFetched() const {
  return user_info_requests_.empty();
}

bool AccountFetcherService::AreAllAccountCapabilitiesFetched() const {
  return account_capabilities_requests_.empty();
}

void AccountFetcherService::OnNetworkInitialized() {
  DCHECK(!network_initialized_);
  DCHECK(!network_fetches_enabled_);
#if BUILDFLAG(IS_ANDROID)
  DCHECK(!child_info_request_);
#endif
  network_initialized_ = true;
  MaybeEnableNetworkFetches();
}

void AccountFetcherService::EnableNetworkFetchesForTest() {
  if (!network_initialized_)
    OnNetworkInitialized();

  if (!refresh_tokens_loaded_)
    OnRefreshTokensLoaded();
}

void AccountFetcherService::EnableAccountRemovalForTest() {
  enable_account_removal_for_test_ = true;
}

AccountCapabilitiesFetcherFactory*
AccountFetcherService::GetAccountCapabilitiesFetcherFactoryForTest() {
  return account_capabilities_fetcher_factory_.get();
}

void AccountFetcherService::RefreshAllAccountInfo(bool only_fetch_if_invalid) {
  for (const auto& account : token_service_->GetAccounts()) {
    RefreshAccountInfo(account, only_fetch_if_invalid);
  }
}

// Child account status is refreshed through invalidations which are only
// available for the primary account. Finding the primary account requires a
// dependency on PrimaryAccountManager which we get around by only allowing a
// single account. This is possible since we only support a single account to be
// a child anyway.
#if BUILDFLAG(IS_ANDROID)
void AccountFetcherService::RefreshAccountInfoIfStale(
    const CoreAccountId& account_id) {
  DCHECK(network_fetches_enabled_);
  RefreshAccountInfo(account_id, /*only_fetch_if_invalid=*/true);
}

void AccountFetcherService::UpdateChildInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<CoreAccountId> accounts = token_service_->GetAccounts();
  if (accounts.size() >= 1) {
    // If a child account is present then there can be only one child account,
    // and it must be the first account on the device.
    //
    // TODO(crbug.com/40803816): consider removing this assumption.
    const CoreAccountId& candidate = accounts[0];
    if (candidate == child_request_account_id_)
      return;
    if (!child_request_account_id_.empty())
      ResetChildInfo();
    child_request_account_id_ = candidate;
    StartFetchingChildInfo(candidate);
  } else {
    ResetChildInfo();
  }
}
#endif

void AccountFetcherService::MaybeEnableNetworkFetches() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!network_initialized_ || !refresh_tokens_loaded_)
    return;
  if (!network_fetches_enabled_) {
    network_fetches_enabled_ = true;
    repeating_timer_->Start();
  }
  RefreshAllAccountInfo(/*only_fetch_if_invalid=*/true);
#if BUILDFLAG(IS_ANDROID)
  UpdateChildInfo();
#endif
}

// Starts fetching user information. This is called periodically to refresh.
void AccountFetcherService::StartFetchingUserInfo(
    const CoreAccountId& account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(network_fetches_enabled_);

  std::unique_ptr<AccountInfoFetcher>& request =
      user_info_requests_[account_id];
  if (!request) {
    DVLOG(1) << "StartFetching " << account_id;
    std::unique_ptr<AccountInfoFetcher> fetcher =
        std::make_unique<AccountInfoFetcher>(
            token_service_, signin_client_->GetURLLoaderFactory(), this,
            account_id);
    request = std::move(fetcher);
    user_info_fetch_start_times_[account_id] = base::TimeTicks::Now();
    request->Start();
  }
}

#if BUILDFLAG(IS_ANDROID)
// Starts fetching whether this is a child account. Handles refresh internally.
void AccountFetcherService::StartFetchingChildInfo(
    const CoreAccountId& account_id) {
  child_info_request_ =
      ChildAccountInfoFetcherAndroid::Create(this, child_request_account_id_);
}

void AccountFetcherService::ResetChildInfo() {
  if (!child_request_account_id_.empty()) {
    AccountInfo account_info =
        account_tracker_service_->GetAccountInfo(child_request_account_id_);
    // TODO(crbug.com/40776452): Reset the status to kUnknown, rather
    // than kFalse.
    if (account_info.is_child_account != signin::Tribool::kUnknown)
      SetIsChildAccount(child_request_account_id_, false);
  }
  child_request_account_id_ = CoreAccountId();
  child_info_request_.reset();
}

void AccountFetcherService::SetIsChildAccount(const CoreAccountId& account_id,
                                              bool is_child_account) {
  if (child_request_account_id_ == account_id)
    account_tracker_service_->SetIsChildAccount(account_id, is_child_account);
}
#endif

void AccountFetcherService::DestroyFetchers(const CoreAccountId& account_id) {
  user_info_requests_.erase(account_id);
  account_capabilities_requests_.erase(account_id);
}

void AccountFetcherService::PrepareForFetchingAccountCapabilities() {
  account_capabilities_fetcher_factory_
      ->PrepareForFetchingAccountCapabilities();
}

void AccountFetcherService::StartFetchingAccountCapabilities(
    const CoreAccountInfo& core_account_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(network_fetches_enabled_);

  std::unique_ptr<AccountCapabilitiesFetcher>& request =
      account_capabilities_requests_[core_account_info.account_id];
  if (!request) {
    AccountInfo account_info =
        account_tracker_service_->GetAccountInfo(core_account_info.account_id);

    request =
        account_capabilities_fetcher_factory_->CreateAccountCapabilitiesFetcher(
            core_account_info,
            account_info.capabilities.AreAnyCapabilitiesKnown()
                ? AccountCapabilitiesFetcher::FetchPriority::kBackground
                : AccountCapabilitiesFetcher::FetchPriority::kForeground,
            base::BindOnce(
                &AccountFetcherService::OnAccountCapabilitiesFetchComplete,
                base::Unretained(this)));
    request->Start();
  }
}

void AccountFetcherService::RefreshAccountInfo(const CoreAccountId& account_id,
                                               bool only_fetch_if_invalid) {
  DCHECK(network_fetches_enabled_);

  // TODO(crbug.com/40283608): It seems quite suspect account tracker needs to
  // start tracking the account when refreshing the account info. Understand why
  // this is needed and ideally remove this call (it may have been added just
  // for tests).
  base::UmaHistogramBoolean(
      "Signin.AccountTracker.RefreshAccountInfo.IsAlreadyTrackingAccount",
      account_tracker_service_->IsTrackingAccount(account_id));
  account_tracker_service_->StartTrackingAccount(account_id);

  const AccountInfo& info =
      account_tracker_service_->GetAccountInfo(account_id);

  if (!only_fetch_if_invalid || !info.capabilities.AreAllCapabilitiesKnown()) {
    StartFetchingAccountCapabilities(info);
  }

  // |only_fetch_if_invalid| is false when the service is due for a timed
  // update.
  if (!only_fetch_if_invalid || !info.IsValid()) {
    // Fetching the user info will also fetch the account image.
    StartFetchingUserInfo(account_id);
    return;
  }

  // User info is already valid and does not need to be downloaded again.
  // Fetch the account image in case it was not fetched previously.
  //
  // Note: |FetchAccountImage()| does not fetch the account image if the
  // account image was already downloaded. So it is fine to call this method
  // even when |only_fetch_if_invalid| is true.
  FetchAccountImage(account_id);
}

void AccountFetcherService::OnUserInfoFetchSuccess(
    const CoreAccountId& account_id,
    const base::Value::Dict& user_info) {
  account_tracker_service_->SetAccountInfoFromUserInfo(account_id, user_info);
  auto it = user_info_fetch_start_times_.find(account_id);
  if (it != user_info_fetch_start_times_.end()) {
    base::UmaHistogramMediumTimes(
        "Signin.AccountFetcher.AccountUserInfoFetchTime",
        base::TimeTicks::Now() - it->second);
    user_info_fetch_start_times_.erase(it);
  }
  FetchAccountImage(account_id);
  user_info_requests_.erase(account_id);
}

image_fetcher::ImageFetcherImpl*
AccountFetcherService::GetOrCreateImageFetcher() {
  // Lazy initialization of |image_fetcher_| because the request context might
  // not be available yet when |Initialize| is called.
  if (!image_fetcher_) {
    image_fetcher_ = std::make_unique<image_fetcher::ImageFetcherImpl>(
        std::move(image_decoder_), signin_client_->GetURLLoaderFactory());
  }
  return image_fetcher_.get();
}

void AccountFetcherService::FetchAccountImage(const CoreAccountId& account_id) {
  DCHECK(signin_client_);
  AccountInfo account_info =
      account_tracker_service_->GetAccountInfo(account_id);
  std::string picture_url_string = account_info.picture_url;

  GURL picture_url(picture_url_string);
  if (!picture_url.is_valid()) {
    DVLOG(1) << "Invalid avatar picture URL: \"" + picture_url_string + "\"";
    return;
  }
  GURL image_url_with_size(signin::GetAvatarImageURLWithOptions(
      picture_url, signin::kAccountInfoImageSize, true /* no_silhouette */));

  if (image_url_with_size.spec() ==
      account_info.last_downloaded_image_url_with_size) {
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("accounts_image_fetcher", R"(
        semantics {
          sender: "Image fetcher for GAIA accounts"
          description:
            "To use a GAIA web account to log into Chrome in the user menu, the"
            "account images of the signed-in GAIA accounts are displayed."
          trigger: "At startup."
          data: "Account picture URL of signed-in GAIA accounts."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "This feature cannot be disabled by settings, "
                   "however, it will only be requested if the user "
                   "has signed into the web."
          policy_exception_justification:
            "Not implemented, considered not useful as no content is being "
            "uploaded or saved; this request merely downloads the web account"
            "profile image."
        })");

  auto callback = base::BindOnce(&AccountFetcherService::OnImageFetched,
                                 base::Unretained(this), account_id,
                                 image_url_with_size.spec());
  image_fetcher::ImageFetcherParams params(traffic_annotation,
                                           kImageFetcherUmaClient);
  user_avatar_fetch_start_times_[account_id] = base::TimeTicks::Now();
  GetOrCreateImageFetcher()->FetchImage(image_url_with_size,
                                        std::move(callback), std::move(params));
}

void AccountFetcherService::OnUserInfoFetchFailure(
    const CoreAccountId& account_id) {
  LOG(WARNING) << "Failed to get UserInfo for " << account_id;
  user_info_fetch_start_times_.erase(account_id);
  // |account_id| is owned by the request. Cannot be used after this line.
  user_info_requests_.erase(account_id);
}

void AccountFetcherService::OnAccountCapabilitiesFetchComplete(
    const CoreAccountId& account_id,
    const std::optional<AccountCapabilities>& account_capabilities) {
  if (account_capabilities.has_value()) {
    account_tracker_service_->SetAccountCapabilities(account_id,
                                                     *account_capabilities);
  }
  // |account_id| is owned by the request. Cannot be used after this line.
  account_capabilities_requests_.erase(account_id);
}

void AccountFetcherService::OnRefreshTokenAvailable(
    const CoreAccountId& account_id) {
  TRACE_EVENT1("AccountFetcherService",
               "AccountFetcherService::OnRefreshTokenAvailable", "account_id",
               account_id.ToString());
  DVLOG(1) << "AVAILABLE " << account_id;

  // The SigninClient needs a "final init" in order to perform some actions
  // (such as fetching the signin token "handle" in order to look for password
  // changes) once everything is initialized and the refresh token is present.
  signin_client_->DoFinalInit();

  if (!network_fetches_enabled_)
    return;
  RefreshAccountInfo(account_id, /*only_fetch_if_invalid=*/true);
#if BUILDFLAG(IS_ANDROID)
  UpdateChildInfo();
#endif
}

void AccountFetcherService::OnRefreshTokenRevoked(
    const CoreAccountId& account_id) {
  TRACE_EVENT1("AccountFetcherService",
               "AccountFetcherService::OnRefreshTokenRevoked", "account_id",
               account_id.ToString());
  DVLOG(1) << "REVOKED " << account_id;

  // Short-circuit out if network fetches are not enabled.
  if (!network_fetches_enabled_) {
    if (enable_account_removal_for_test_) {
      account_tracker_service_->StopTrackingAccount(account_id);
    }
    return;
  }

  DestroyFetchers(account_id);
#if BUILDFLAG(IS_ANDROID)
  UpdateChildInfo();
#endif
  account_tracker_service_->StopTrackingAccount(account_id);
}

void AccountFetcherService::OnRefreshTokensLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  refresh_tokens_loaded_ = true;
  MaybeEnableNetworkFetches();
}

void AccountFetcherService::OnImageFetched(
    const CoreAccountId& account_id,
    const std::string& image_url_with_size,
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& metadata) {
  if (metadata.http_response_code != net::HTTP_OK) {
    DCHECK(image.IsEmpty());
    return;
  }
  account_tracker_service_->SetAccountImage(account_id, image_url_with_size,
                                            image);
  auto it = user_avatar_fetch_start_times_.find(account_id);
  if (it != user_avatar_fetch_start_times_.end()) {
    base::UmaHistogramMediumTimes(
        "Signin.AccountFetcher.AccountAvatarFetchTime",
        base::TimeTicks::Now() - it->second);
    user_avatar_fetch_start_times_.erase(it);
  }
}
