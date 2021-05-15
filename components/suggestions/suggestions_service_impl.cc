// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/suggestions/suggestions_service_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/google/core/common/google_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/suggestions/blocklist_store.h"
#include "components/suggestions/suggestions_store.h"
#include "components/variations/net/variations_http_headers.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

using base::TimeDelta;

namespace suggestions {

namespace {

// Used to UMA log the state of the last response from the server.
enum SuggestionsResponseState {
  RESPONSE_EMPTY,
  RESPONSE_INVALID,
  RESPONSE_VALID,
  RESPONSE_STATE_SIZE
};

// Will log the supplied response |state|.
void LogResponseState(SuggestionsResponseState state) {
  UMA_HISTOGRAM_ENUMERATION("Suggestions.ResponseState", state,
                            RESPONSE_STATE_SIZE);
}

const net::BackoffEntry::Policy kBlocklistBackoffPolicy = {
    /*num_errors_to_ignore=*/0,
    /*initial_delay_ms=*/1000,
    /*multiply_factor=*/2.0,
    /*jitter_factor=*/0.0,
    /*maximum_backoff_ms=*/2 * 60 * 60 * 1000,
    /*entry_lifetime_ms=*/-1,
    /*always_use_initial_delay=*/true};

const char kDefaultGoogleBaseURL[] = "https://www.google.com/";

GURL GetGoogleBaseURL() {
  GURL url(google_util::CommandLineGoogleBaseURL());
  if (url.is_valid())
    return url;
  return GURL(kDefaultGoogleBaseURL);
}

// Format strings for the various suggestions URLs. They all have two string
// params: The Google base URL and the device type.
// TODO(mathp): Put this in TemplateURL.
const char kSuggestionsURLFormat[] = "%schromesuggestions?%s";
const char kSuggestionsBlocklistURLPrefixFormat[] =
    "%schromesuggestions/blacklist?t=%s&url=";
const char kSuggestionsBlocklistClearURLFormat[] =
    "%schromesuggestions/blacklist/clear?t=%s";

const char kSuggestionsBlocklistURLParam[] = "url";
const char kSuggestionsDeviceParam[] = "t=%s";

#if defined(OS_ANDROID) || defined(OS_IOS)
const char kDeviceType[] = "2";
#else
const char kDeviceType[] = "1";
#endif

const char kFaviconURL[] =
    "https://s2.googleusercontent.com/s2/favicons?domain_url=%s&alt=s&sz=32";

// The default expiry timeout is 168 hours.
const int64_t kDefaultExpiryUsec = 168 * base::Time::kMicrosecondsPerHour;

}  // namespace

SuggestionsServiceImpl::SuggestionsServiceImpl(
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<SuggestionsStore> suggestions_store,
    std::unique_ptr<BlocklistStore> blocklist_store,
    const base::TickClock* tick_clock)
    : identity_manager_(identity_manager),
      sync_service_(sync_service),
      history_sync_state_(syncer::UploadState::INITIALIZING),
      url_loader_factory_(url_loader_factory),
      suggestions_store_(std::move(suggestions_store)),
      blocklist_store_(std::move(blocklist_store)),
      tick_clock_(tick_clock),
      blocklist_upload_backoff_(&kBlocklistBackoffPolicy, tick_clock_),
      blocklist_upload_timer_(tick_clock_) {
  // |sync_service_| is null if switches::kDisableSync is set (tests use that).
  if (sync_service_)
    sync_service_observation_.Observe(sync_service_);
  // Immediately get the current sync state, so we'll flush the cache if
  // necessary.
  OnStateChanged(sync_service_);
  // This makes sure the initial delay is actually respected.
  blocklist_upload_backoff_.InformOfRequest(/*succeeded=*/true);
}

SuggestionsServiceImpl::~SuggestionsServiceImpl() = default;

bool SuggestionsServiceImpl::FetchSuggestionsData() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // If sync state allows, issue a network request to refresh the suggestions.
  if (history_sync_state_ != syncer::UploadState::ACTIVE)
    return false;
  IssueRequestIfNoneOngoing(BuildSuggestionsURL());
  return true;
}

absl::optional<SuggestionsProfile>
SuggestionsServiceImpl::GetSuggestionsDataFromCache() const {
  SuggestionsProfile suggestions;
  // In case of empty cache or error, return empty.
  if (!suggestions_store_->LoadSuggestions(&suggestions))
    return absl::nullopt;
  blocklist_store_->FilterSuggestions(&suggestions);
  return suggestions;
}

base::CallbackListSubscription SuggestionsServiceImpl::AddCallback(
    const ResponseCallback& callback) {
  return callback_list_.Add(callback);
}

bool SuggestionsServiceImpl::BlocklistURL(const GURL& candidate_url) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(treib): Do we need to check |history_sync_state_| here?

  if (!blocklist_store_->BlocklistUrl(candidate_url))
    return false;

  callback_list_.Notify(
      GetSuggestionsDataFromCache().value_or(SuggestionsProfile()));

  // Blocklist uploads are scheduled on any request completion, so only schedule
  // an upload if there is no ongoing request.
  if (!pending_request_)
    ScheduleBlocklistUpload();

  return true;
}

bool SuggestionsServiceImpl::UndoBlocklistURL(const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(treib): Do we need to check |history_sync_state_| here?

  TimeDelta time_delta;
  if (blocklist_store_->GetTimeUntilURLReadyForUpload(url, &time_delta) &&
      time_delta > TimeDelta::FromSeconds(0) &&
      blocklist_store_->RemoveUrl(url)) {
    // The URL was not yet candidate for upload to the server and could be
    // removed from the blocklist.
    callback_list_.Notify(
        GetSuggestionsDataFromCache().value_or(SuggestionsProfile()));
    return true;
  }
  return false;
}

void SuggestionsServiceImpl::ClearBlocklist() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(treib): Do we need to check |history_sync_state_| here?

  blocklist_store_->ClearBlocklist();
  callback_list_.Notify(
      GetSuggestionsDataFromCache().value_or(SuggestionsProfile()));
  IssueRequestIfNoneOngoing(BuildSuggestionsBlocklistClearURL());
}

base::TimeDelta SuggestionsServiceImpl::BlocklistDelayForTesting() const {
  return blocklist_upload_backoff_.GetTimeUntilRelease();
}

bool SuggestionsServiceImpl::HasPendingRequestForTesting() const {
  return !!pending_request_;
}

// static
bool SuggestionsServiceImpl::GetBlocklistedUrl(const GURL& original_url,
                                               GURL* blocklisted_url) {
  bool is_blocklist_request = base::StartsWith(
      original_url.spec(), BuildSuggestionsBlocklistURLPrefix(),
      base::CompareCase::SENSITIVE);
  if (!is_blocklist_request)
    return false;

  // Extract the blocklisted URL from the blocklist request.
  std::string blocklisted;
  if (!net::GetValueForKeyInQuery(original_url, kSuggestionsBlocklistURLParam,
                                  &blocklisted)) {
    return false;
  }

  *blocklisted_url = GURL(blocklisted);
  return true;
}

// static
void SuggestionsServiceImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SuggestionsStore::RegisterProfilePrefs(registry);
  BlocklistStore::RegisterProfilePrefs(registry);
}

// static
GURL SuggestionsServiceImpl::BuildSuggestionsURL() {
  std::string query = base::StringPrintf(kSuggestionsDeviceParam, kDeviceType);
  return GURL(base::StringPrintf(
      kSuggestionsURLFormat, GetGoogleBaseURL().spec().c_str(), query.c_str()));
}

// static
std::string SuggestionsServiceImpl::BuildSuggestionsBlocklistURLPrefix() {
  return base::StringPrintf(kSuggestionsBlocklistURLPrefixFormat,
                            GetGoogleBaseURL().spec().c_str(), kDeviceType);
}

// static
GURL SuggestionsServiceImpl::BuildSuggestionsBlocklistURL(
    const GURL& candidate_url) {
  return GURL(BuildSuggestionsBlocklistURLPrefix() +
              net::EscapeQueryParamValue(candidate_url.spec(), true));
}

// static
GURL SuggestionsServiceImpl::BuildSuggestionsBlocklistClearURL() {
  return GURL(base::StringPrintf(kSuggestionsBlocklistClearURLFormat,
                                 GetGoogleBaseURL().spec().c_str(),
                                 kDeviceType));
}

SuggestionsServiceImpl::RefreshAction
SuggestionsServiceImpl::RefreshHistorySyncState() {
  syncer::UploadState new_sync_state = syncer::GetUploadToGoogleState(
      sync_service_, syncer::HISTORY_DELETE_DIRECTIVES);
  if (history_sync_state_ == new_sync_state)
    return NO_ACTION;

  syncer::UploadState old_sync_state = history_sync_state_;
  history_sync_state_ = new_sync_state;

  switch (new_sync_state) {
    case syncer::UploadState::INITIALIZING:
      // In this state, we do not issue server requests, but we will serve from
      // cache if available -> no action required.
      return NO_ACTION;
    case syncer::UploadState::ACTIVE:
      // If history sync was just enabled, immediately fetch suggestions, so
      // that hopefully the next NTP will already get them.
      if (old_sync_state == syncer::UploadState::NOT_ACTIVE)
        return FETCH_SUGGESTIONS;
      // Otherwise, this just means sync initialization finished.
      return NO_ACTION;
    case syncer::UploadState::NOT_ACTIVE:
      // If the user signed out (or disabled history sync), we have to clear
      // everything.
      return CLEAR_SUGGESTIONS;
  }
  NOTREACHED();
  return NO_ACTION;
}

void SuggestionsServiceImpl::OnStateChanged(syncer::SyncService* sync) {
  DCHECK(sync_service_ == sync);

  switch (RefreshHistorySyncState()) {
    case NO_ACTION:
      break;
    case CLEAR_SUGGESTIONS:
      // Cancel any ongoing request, to stop interacting with the server.
      pending_request_.reset(nullptr);
      suggestions_store_->ClearSuggestions();
      callback_list_.Notify(SuggestionsProfile());
      break;
    case FETCH_SUGGESTIONS:
      IssueRequestIfNoneOngoing(BuildSuggestionsURL());
      break;
  }
}

void SuggestionsServiceImpl::SetDefaultExpiryTimestamp(
    SuggestionsProfile* suggestions,
    int64_t default_timestamp_usec) {
  for (ChromeSuggestion& suggestion : *suggestions->mutable_suggestions()) {
    // Do not set expiry if the server has already provided a more specific
    // expiry time for this suggestion.
    if (!suggestion.has_expiry_ts())
      suggestion.set_expiry_ts(default_timestamp_usec);
  }
}

void SuggestionsServiceImpl::IssueRequestIfNoneOngoing(const GURL& url) {
  // If there is an ongoing request, let it complete.
  // This will silently swallow blocklist and clearblocklist requests if a
  // request happens to be ongoing.
  // TODO(treib): Queue such requests and send them after the current one
  // completes.
  if (pending_request_)
    return;
  // If there is an ongoing token request, also wait for that.
  if (token_fetcher_)
    return;

  signin::ScopeSet scopes{GaiaConstants::kChromeSyncOAuth2Scope};
  token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "suggestions_service", identity_manager_, scopes,
      base::BindOnce(&SuggestionsServiceImpl::AccessTokenAvailable,
                     base::Unretained(this), url),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);
}

void SuggestionsServiceImpl::AccessTokenAvailable(
    const GURL& url,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK(token_fetcher_);
  token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    blocklist_upload_backoff_.InformOfRequest(/*succeeded=*/false);
    ScheduleBlocklistUpload();
    return;
  }

  DCHECK(!access_token_info.token.empty());

  IssueSuggestionsRequest(url, access_token_info.token);
}

void SuggestionsServiceImpl::IssueSuggestionsRequest(
    const GURL& url,
    const std::string& access_token) {
  DCHECK(!access_token.empty());
  pending_request_ = CreateSuggestionsRequest(url, access_token);
  // Unretained is safe because the SimpleURLLoader in |pending_request_| will
  // not call the callback after it is deleted.
  auto callback = base::BindOnce(&SuggestionsServiceImpl::OnURLFetchComplete,
                                 base::Unretained(this), url);
  pending_request_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(), std::move(callback));
  last_request_started_time_ = tick_clock_->NowTicks();
}

std::unique_ptr<network::SimpleURLLoader>
SuggestionsServiceImpl::CreateSuggestionsRequest(
    const GURL& url,
    const std::string& access_token) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("suggestions_service", R"(
        semantics {
          sender: "Suggestions Service"
          description:
            "For signed-in users with History Sync enabled, the Suggestions "
            "Service fetches website suggestions, based on the user's browsing "
            "history, for display on the New Tab page."
          trigger: "Opening a New Tab page."
          data: "The user's OAuth2 credentials."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can disable this feature by signing out of Chromium, or "
            "disabling Sync or History Sync in Chromium settings under "
            "Advanced sync settings. The feature is enabled by default."
          chrome_policy {
            SyncDisabled {
              policy_options {mode: MANDATORY}
              SyncDisabled: true
            }
          }
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "GET";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  // Add Chrome experiment state to the request headers.
  // TODO: We should call AppendVariationHeaders with explicit
  // variations::SignedIn::kNo If the access_token is empty
  variations::AppendVariationsHeaderUnknownSignedIn(
      url, variations::InIncognito::kNo, resource_request.get());
  if (!access_token.empty()) {
    resource_request->headers.SetHeader(
        "Authorization", base::StrCat({"Bearer ", access_token}));
  }

  auto loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  // We use non-200 error codes as a signal to clear the cache in
  // OnURLFetchComplete.
  loader->SetAllowHttpErrorResults(true);
  return loader;
}

void SuggestionsServiceImpl::OnURLFetchComplete(
    const GURL& original_url,
    std::unique_ptr<std::string> suggestions_data) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // The SimpleURLLoader will be deleted when the request is handled.
  std::unique_ptr<const network::SimpleURLLoader> request =
      std::move(pending_request_);
  DCHECK(request);

  bool valid_request = suggestions_data && request->NetError() == net::OK;
  if (!valid_request) {
    // This represents network errors (i.e. the server did not provide a
    // response).
    base::UmaHistogramSparse("Suggestions.FailedRequestErrorCode",
                             -request->NetError());
    DVLOG(1) << "Suggestions server request failed with error: "
             << request->NetError() << ": "
             << net::ErrorToString(request->NetError());
    blocklist_upload_backoff_.InformOfRequest(/*succeeded=*/false);
    ScheduleBlocklistUpload();
    return;
  }

  int response_code = 0;
  if (request->ResponseInfo() && request->ResponseInfo()->headers)
    response_code = request->ResponseInfo()->headers->response_code();
  base::UmaHistogramSparse("Suggestions.FetchResponseCode", response_code);

  if (response_code != net::HTTP_OK) {
    // A non-200 response code means that server has no (longer) suggestions for
    // this user. Aggressively clear the cache.
    suggestions_store_->ClearSuggestions();
    blocklist_upload_backoff_.InformOfRequest(/*succeeded=*/false);
    ScheduleBlocklistUpload();
    return;
  }

  const TimeDelta latency =
      tick_clock_->NowTicks() - last_request_started_time_;
  UMA_HISTOGRAM_MEDIUM_TIMES("Suggestions.FetchSuccessLatency", latency);

  // Handle a successful blocklisting.
  GURL blocklisted_url;
  if (GetBlocklistedUrl(original_url, &blocklisted_url))
    blocklist_store_->RemoveUrl(blocklisted_url);

  // Parse the received suggestions and update the cache, or take proper action
  // in the case of invalid response.
  SuggestionsProfile suggestions;
  if (suggestions_data->empty()) {
    LogResponseState(RESPONSE_EMPTY);
    suggestions_store_->ClearSuggestions();
  } else if (suggestions.ParseFromString(*suggestions_data)) {
    LogResponseState(RESPONSE_VALID);
    int64_t now_usec =
        (base::Time::NowFromSystemTime() - base::Time::UnixEpoch())
            .ToInternalValue();
    SetDefaultExpiryTimestamp(&suggestions, now_usec + kDefaultExpiryUsec);
    PopulateExtraData(&suggestions);
    suggestions_store_->StoreSuggestions(suggestions);
  } else {
    LogResponseState(RESPONSE_INVALID);
  }

  callback_list_.Notify(
      GetSuggestionsDataFromCache().value_or(SuggestionsProfile()));

  blocklist_upload_backoff_.InformOfRequest(/*succeeded=*/true);
  ScheduleBlocklistUpload();
}

void SuggestionsServiceImpl::PopulateExtraData(
    SuggestionsProfile* suggestions) {
  for (ChromeSuggestion& suggestion : *suggestions->mutable_suggestions()) {
    if (!suggestion.has_favicon_url() || suggestion.favicon_url().empty()) {
      suggestion.set_favicon_url(
          base::StringPrintf(kFaviconURL, suggestion.url().c_str()));
    }
  }
}

void SuggestionsServiceImpl::Shutdown() {
  // Cancel pending request.
  pending_request_.reset(nullptr);

  sync_service_observation_.Reset();
}

void SuggestionsServiceImpl::ScheduleBlocklistUpload() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TimeDelta time_delta;
  if (blocklist_store_->GetTimeUntilReadyForUpload(&time_delta)) {
    // Blocklist cache is not empty: schedule.
    blocklist_upload_timer_.Start(
        FROM_HERE, time_delta + blocklist_upload_backoff_.GetTimeUntilRelease(),
        base::BindOnce(&SuggestionsServiceImpl::UploadOneFromBlocklist,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void SuggestionsServiceImpl::UploadOneFromBlocklist() {
  DCHECK(thread_checker_.CalledOnValidThread());

  GURL blocklisted_url;
  if (blocklist_store_->GetCandidateForUpload(&blocklisted_url)) {
    // Issue a blocklisting request. Even if this request ends up not being sent
    // because of an ongoing request, a blocklist request is later scheduled.
    IssueRequestIfNoneOngoing(BuildSuggestionsBlocklistURL(blocklisted_url));
    return;
  }

  // Even though there's no candidate for upload, the blocklist might not be
  // empty.
  ScheduleBlocklistUpload();
}

}  // namespace suggestions
