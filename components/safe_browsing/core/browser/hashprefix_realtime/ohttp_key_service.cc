// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hashprefix_realtime/ohttp_key_service.h"

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/escape.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/utils/backoff_operator.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/utils.h"
#include "google_apis/google_api_keys.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

constexpr base::TimeDelta kKeyFetchTimeout = base::Seconds(3);

constexpr char kKeyFetchServerUrl[] =
    "https://safebrowsingohttpgateway.googleapis.com/v1/ohttp/hpkekeyconfig";
// Key older than 3 days is considered expired and should be refetched.
constexpr base::TimeDelta kKeyExpirationDuration = base::Days(3);
// For slower rotated keys (the old mechanism), key older than 7 days is
// considered expired and should be refetched.
constexpr base::TimeDelta kSlowerKeyExpirationDuration = base::Days(7);

// Async fetch will kick in if the key is close to the expiration threshold.
constexpr base::TimeDelta kKeyCloseToExpirationThreshold = base::Days(1);

// The interval that async workflow checks the status of the key.
constexpr base::TimeDelta kAsyncFetchCheckInterval = base::Hours(1);

// The minimum interval that async workflow checks the status of the key.
constexpr base::TimeDelta kAsyncFetchCheckMinInterval = base::Minutes(1);

// The error code represents that the server cannot successfully decrypt the
// request. Defined in
// https://www.ietf.org/archive/id/draft-ietf-ohai-ohttp-02.html#name-server-responsibilities
constexpr net::HttpStatusCode kKeyRelatedHttpErrorCode =
    net::HTTP_UNPROCESSABLE_CONTENT;

// The header that the server sets if the server is able to decrypt the request,
// but the key is outdated.
constexpr char kKeyRotatedHeader[] = "X-OhttpPublickey-Rotated";

// The maximum delayed time to fetch a new key if the key fetch is triggered
// by the server.
constexpr int kServerTriggeredFetchMaxDelayTimeSec = 60;

// Backoff constants
const size_t kNumFailuresToEnforceBackoff = 3;
const size_t kMinBackOffResetDurationInSeconds = 5 * 60;   //  5 minutes.
const size_t kMaxBackOffResetDurationInSeconds = 24 * 60 * 60;  // 1 day.

constexpr net::NetworkTrafficAnnotationTag kOhttpKeyTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("safe_browsing_ohttp_key_fetch",
                                        R"(
  semantics {
    sender: "Safe Browsing"
    description:
      "Get the Oblivious HTTP key for hash real time URL check."
    trigger:
      "Periodically fetching the key once every few hours or fetching the key "
      "during hash real time URL check if there is no key available."
    data:
        "A simple GET HTTP request. No user data is included."
    destination: GOOGLE_OWNED_SERVICE
    internal {
      contacts {
        email: "xinghuilu@chromium.org"
      }
      contacts {
        email: "chrome-counter-abuse-alerts@google.com"
      }
    }
    user_data {
      type: NONE
    }
    last_reviewed: "2023-03-06"
  }
  policy {
    cookies_allowed: NO
    setting:
      "Users can disable this feature by unselecting 'Standard protection' "
      "in Chromium settings under Security. The feature is enabled by default."
    chrome_policy {
      SafeBrowsingProtectionLevel {
        policy_options {mode: MANDATORY}
        SafeBrowsingProtectionLevel: 0
      }
    }
    chrome_policy {
      SafeBrowsingProxiedRealTimeChecksAllowed {
        policy_options {mode: MANDATORY}
        SafeBrowsingProxiedRealTimeChecksAllowed: false
      }
    }
  }
  comments:
      "SafeBrowsingProtectionLevel value of 0 or 2 disables fetching this "
      "OHTTP key. A value of 1 enables the feature. The feature is enabled by "
      "default."
  )");

bool IsEnabled(PrefService* pref_service, std::optional<std::string> country) {
  // If this class has been created, it is already known that the session is not
  // off-the-record, so |is_off_the_record| is passed through as false.
  return safe_browsing::hash_realtime_utils::DetermineHashRealTimeSelection(
             /*is_off_the_record=*/false, pref_service,
             /*latest_country=*/country) ==
         safe_browsing::hash_realtime_utils::HashRealTimeSelection::
             kHashRealTimeService;
}

GURL GetKeyFetchingUrl() {
  GURL url(kKeyFetchServerUrl);
  std::string api_key = google_apis::GetAPIKey();
  if (!api_key.empty()) {
    url = url.Resolve("?key=" + base::EscapeQueryParamValue(api_key, true));
  }
  return url;
}

}  // namespace

namespace safe_browsing {

OhttpKeyService::OhttpKeyService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service,
    PrefService* local_state,
    base::RepeatingCallback<std::optional<std::string>()> country_getter)
    : url_loader_factory_(url_loader_factory),
      pref_service_(pref_service),
      backoff_operator_(std::make_unique<BackoffOperator>(
          /*num_failures_to_enforce_backoff=*/kNumFailuresToEnforceBackoff,
          /*min_backoff_reset_duration_in_seconds=*/
          kMinBackOffResetDurationInSeconds,
          /*max_backoff_reset_duration_in_seconds=*/
          kMaxBackOffResetDurationInSeconds)),
      country_getter_(country_getter) {
  // |pref_service_| can be null in tests.
  if (!pref_service_) {
    return;
  }

  PopulateKeyFromPref();

  hash_realtime_utils::HashRealTimeSelectionConfiguringPrefs configuring_prefs =
      hash_realtime_utils::GetHashRealTimeSelectionConfiguringPrefs();
  // Set up listener for profile prefs.
  pref_change_registrar_.Init(pref_service_);
  for (const char* pref : configuring_prefs.profile_prefs) {
    pref_change_registrar_.Add(
        pref, base::BindRepeating(&OhttpKeyService::OnConfiguringPrefsChanged,
                                  weak_factory_.GetWeakPtr()));
  }
  // Set up listener for local state prefs.
  local_state_pref_change_registrar_.Init(local_state);
  for (const char* pref : configuring_prefs.local_state_prefs) {
    local_state_pref_change_registrar_.Add(
        pref, base::BindRepeating(&OhttpKeyService::OnConfiguringPrefsChanged,
                                  weak_factory_.GetWeakPtr()));
  }

  SetEnabled(IsEnabled(pref_service_, country_getter_.Run()));
}

OhttpKeyService::~OhttpKeyService() = default;

void OhttpKeyService::OnConfiguringPrefsChanged() {
  SetEnabled(IsEnabled(pref_service_, country_getter_.Run()));
}

void OhttpKeyService::SetEnabled(bool enable) {
  if (enabled_ == enable) {
    return;
  }
  enabled_ = enable;
  if (!enabled_) {
    url_loader_.reset();
    pending_callbacks_.Notify(std::nullopt);
    async_fetch_timer_.Stop();
    return;
  }
  base::UmaHistogramBoolean(
      "SafeBrowsing.HPRT.OhttpKeyService.IsFasterOhttpKeyRotationEnabled",
      base::FeatureList::IsEnabled(
          kHashPrefixRealTimeLookupsFasterOhttpKeyRotation));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&OhttpKeyService::MaybeStartOrRescheduleAsyncFetch,
                     weak_factory_.GetWeakPtr()));
}

void OhttpKeyService::GetOhttpKey(Callback callback) {
  base::UmaHistogramBoolean(
      "SafeBrowsing.HPRT.OhttpKeyService.IsEnabledFreshnessOnKeyFetch",
      enabled_ == IsEnabled(pref_service_, country_getter_.Run()));
  if (!enabled_) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // If there is a valid key in memory, use it directly.
  bool has_cache_key = ohttp_key_ && ohttp_key_->expiration > base::Time::Now();
  base::UmaHistogramBoolean("SafeBrowsing.HPRT.OhttpKeyService.HasCachedKey",
                            has_cache_key);
  if (has_cache_key) {
    std::move(callback).Run(ohttp_key_->key);
    return;
  }

  StartFetch(std::move(callback),
             FetchTriggerReason::kDuringHashRealTimeLookup);
}

void OhttpKeyService::NotifyLookupResponse(
    const std::string& key,
    int response_code,
    scoped_refptr<net::HttpResponseHeaders> headers) {
  // Skip server triggered fetch if:
  //   * The service is disabled. OR
  //   * The fetch is already scheduled. OR
  //   * |ohttp_key_| is already cleared up (this can happen if multiple
  //   requests are kicked off around the same time). OR
  //   * |ohttp_key_| and |key| are different, which means the notification is
  //   stale, since the key has changed since the lookup started.
  if (!enabled_ || server_triggered_fetch_scheduled_ || !ohttp_key_ ||
      ohttp_key_->key != key) {
    return;
  }

  if (!has_received_lookup_response_from_current_key_) {
    base::UmaHistogramSparse(
        "SafeBrowsing.HPRT.OhttpKeyService."
        "FirstLookupResponseCodeFromCurrentKey",
        response_code);
    has_received_lookup_response_from_current_key_ = true;
  }

  if (response_code == kKeyRelatedHttpErrorCode) {
    // The failure is caused by unrecognized key. This is a hard failure, so
    // clear the key immediately.
    ohttp_key_ = std::nullopt;
    server_triggered_fetch_scheduled_ = true;
    // Introduce an artificial delay so the server cannot correlate the key
    // fetch request with the original lookup request.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&OhttpKeyService::MaybeStartServerTriggeredFetch,
                       weak_factory_.GetWeakPtr(), key,
                       FetchTriggerReason::kKeyRelatedHttpErrorCode),
        base::Seconds(base::RandInt(0, kServerTriggeredFetchMaxDelayTimeSec)));
    return;
  }

  if (response_code == net::HTTP_OK && headers &&
      headers->HasHeader(kKeyRotatedHeader)) {
    server_triggered_fetch_scheduled_ = true;
    // The key is still valid, but it is close to expiration. It is a soft
    // failure, so do not clear the key immediately.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&OhttpKeyService::MaybeStartServerTriggeredFetch,
                       weak_factory_.GetWeakPtr(), key,
                       FetchTriggerReason::kKeyRotatedHeader),
        base::Seconds(base::RandInt(0, kServerTriggeredFetchMaxDelayTimeSec)));
    return;
  }
}

void OhttpKeyService::StartFetch(Callback callback,
                                 FetchTriggerReason trigger_reason) {
  base::UmaHistogramEnumeration(
      "SafeBrowsing.HPRT.OhttpKeyService.FetchKeyTriggerReason",
      trigger_reason);
  bool in_backoff = backoff_operator_->IsInBackoffMode();
  base::UmaHistogramBoolean("SafeBrowsing.HPRT.OhttpKeyService.BackoffState",
                            in_backoff);
  if (in_backoff) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  pending_callbacks_.AddUnsafe(std::move(callback));
  // If url_loader_ is not null, that means a request is already in progress.
  // Will notify the callback when it is completed.
  if (url_loader_) {
    return;
  }
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetKeyFetchingUrl();
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  if (base::FeatureList::IsEnabled(
          kHashPrefixRealTimeLookupsFasterOhttpKeyRotation)) {
    resource_request->headers.SetHeader("X-OhttpPublickey-Fst", "true");
  }
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 kOhttpKeyTrafficAnnotation);
  url_loader_->SetTimeoutDuration(kKeyFetchTimeout);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&OhttpKeyService::OnURLLoaderComplete,
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void OhttpKeyService::OnURLLoaderComplete(
    base::TimeTicks request_start_time,
    std::unique_ptr<std::string> response_body) {
  DCHECK(url_loader_);
  int net_error = url_loader_->NetError();
  int response_code = 0;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }

  base::UmaHistogramTimes("SafeBrowsing.HPRT.OhttpKeyService.Network.Time",
                          base::TimeTicks::Now() - request_start_time);
  RecordHttpResponseOrErrorCode(
      "SafeBrowsing.HPRT.OhttpKeyService.Network.Result", net_error,
      response_code);

  url_loader_.reset();
  bool is_key_fetch_successful =
      response_body && net_error == net::OK && response_code == net::HTTP_OK;
  if (is_key_fetch_successful) {
    ohttp_key_ = {*response_body,
                  base::Time::Now() +
                      (base::FeatureList::IsEnabled(
                           kHashPrefixRealTimeLookupsFasterOhttpKeyRotation)
                           ? kKeyExpirationDuration
                           : kSlowerKeyExpirationDuration)};
    StoreKeyToPref();
    has_received_lookup_response_from_current_key_ = false;
    backoff_operator_->ReportSuccess();
  } else {
    backoff_operator_->ReportError();
  }
  pending_callbacks_.Notify(is_key_fetch_successful
                                ? std::optional<std::string>(*response_body)
                                : std::nullopt);
}

void OhttpKeyService::MaybeStartOrRescheduleAsyncFetch() {
  if (!enabled_) {
    return;
  }

  if (ShouldStartAsyncFetch()) {
    StartFetch(base::BindOnce(&OhttpKeyService::OnAsyncFetchCompleted,
                              weak_factory_.GetWeakPtr()),
               FetchTriggerReason::kAsyncFetch);
  } else {
    async_fetch_timer_.Start(
        FROM_HERE, kAsyncFetchCheckInterval, this,
        &OhttpKeyService::MaybeStartOrRescheduleAsyncFetch);
  }
}

void OhttpKeyService::OnAsyncFetchCompleted(
    std::optional<std::string> ohttp_key) {
  if (!enabled_) {
    return;
  }

  base::TimeDelta next_fetch_time = kAsyncFetchCheckInterval;
  if (!ohttp_key) {
    // If the key fetch failed, retry earlier. If it is in backoff mode, retry
    // after the backoff ends. Otherwise, retry with minimum interval.
    next_fetch_time = backoff_operator_->IsInBackoffMode()
                          ? backoff_operator_->GetBackoffRemainingDuration()
                          : kAsyncFetchCheckMinInterval;
  }

  async_fetch_timer_.Start(FROM_HERE, next_fetch_time, this,
                           &OhttpKeyService::MaybeStartOrRescheduleAsyncFetch);
}

bool OhttpKeyService::ShouldStartAsyncFetch() {
  return !ohttp_key_ || ohttp_key_->expiration <=
                            base::Time::Now() + kKeyCloseToExpirationThreshold;
}

void OhttpKeyService::MaybeStartServerTriggeredFetch(
    std::string previous_key,
    FetchTriggerReason trigger_reason) {
  server_triggered_fetch_scheduled_ = false;
  if (ohttp_key_ && ohttp_key_->key != previous_key) {
    // The key has already been updated, no action needed.
    return;
  }

  StartFetch(base::DoNothing(), trigger_reason);
}

void OhttpKeyService::PopulateKeyFromPref() {
  std::string key =
      pref_service_->GetString(prefs::kSafeBrowsingHashRealTimeOhttpKey);
  base::Time expiration_time = pref_service_->GetTime(
      prefs::kSafeBrowsingHashRealTimeOhttpExpirationTime);
  if (!key.empty() && expiration_time > base::Time::Now()) {
    std::string decoded_key;
    base::Base64Decode(key, &decoded_key);
    ohttp_key_ = {decoded_key, expiration_time};
  }
}

void OhttpKeyService::StoreKeyToPref() {
  if (ohttp_key_ && ohttp_key_->expiration > base::Time::Now()) {
    std::string base64_encoded_key = base::Base64Encode(ohttp_key_->key);
    pref_service_->SetString(prefs::kSafeBrowsingHashRealTimeOhttpKey,
                             base64_encoded_key);
    pref_service_->SetTime(prefs::kSafeBrowsingHashRealTimeOhttpExpirationTime,
                           ohttp_key_->expiration);
  }
}

void OhttpKeyService::Shutdown() {
  url_loader_.reset();
  pending_callbacks_.Notify(std::nullopt);
  pref_change_registrar_.RemoveAll();
  local_state_pref_change_registrar_.RemoveAll();
  async_fetch_timer_.Stop();
}

void OhttpKeyService::set_ohttp_key_for_testing(
    OhttpKeyAndExpiration ohttp_key) {
  ohttp_key_ = ohttp_key;
}

std::optional<OhttpKeyService::OhttpKeyAndExpiration>
OhttpKeyService::get_ohttp_key_for_testing() {
  return ohttp_key_;
}

}  // namespace safe_browsing
