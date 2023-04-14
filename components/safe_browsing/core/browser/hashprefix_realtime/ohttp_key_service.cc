// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hashprefix_realtime/ohttp_key_service.h"

#include "base/rand_util.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/utils/backoff_operator.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

constexpr base::TimeDelta kKeyFetchTimeout = base::Seconds(3);
// TODO(crbug.com/1407283): Update the endpoint when it is finalized.
constexpr char kKeyFetchServerUrl[] =
    "https://safebrowsingohttpgateway.googleapis.com/key";
// Key older than 7 days is considered expired and should be refetched.
constexpr base::TimeDelta kKeyExpirationDuration = base::Days(7);

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
const size_t kMaxBackOffResetDurationInSeconds = 30 * 60;  // 30 minutes.

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
  }
  comments:
      "SafeBrowsingProtectionLevel value of 0 or 2 disables fetching this "
      "OHTTP key. A value of 1 enables the feature. The feature is enabled by "
      "default."
  )");

}  // namespace

namespace safe_browsing {

OhttpKeyService::OhttpKeyService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service)
    : url_loader_factory_(url_loader_factory),
      pref_service_(pref_service),
      backoff_operator_(std::make_unique<BackoffOperator>(
          /*num_failures_to_enforce_backoff=*/kNumFailuresToEnforceBackoff,
          /*min_backoff_reset_duration_in_seconds=*/
          kMinBackOffResetDurationInSeconds,
          /*max_backoff_reset_duration_in_seconds=*/
          kMaxBackOffResetDurationInSeconds)) {
  // |pref_service_| can be null in tests.
  if (!pref_service_) {
    return;
  }

  PopulateKeyFromPref();

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kSafeBrowsingEnabled,
      base::BindRepeating(&OhttpKeyService::OnSafeBrowsingStateChanged,
                          weak_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      prefs::kSafeBrowsingEnhanced,
      base::BindRepeating(&OhttpKeyService::OnSafeBrowsingStateChanged,
                          weak_factory_.GetWeakPtr()));

  SetEnabled(GetSafeBrowsingState(*pref_service_) ==
             SafeBrowsingState::STANDARD_PROTECTION);
}

OhttpKeyService::~OhttpKeyService() = default;

void OhttpKeyService::OnSafeBrowsingStateChanged() {
  SetEnabled(GetSafeBrowsingState(*pref_service_) ==
             SafeBrowsingState::STANDARD_PROTECTION);
}

void OhttpKeyService::SetEnabled(bool enable) {
  if (enabled_ == enable) {
    return;
  }
  enabled_ = enable;
  if (!enabled_) {
    url_loader_.reset();
    pending_callbacks_.Notify(absl::nullopt);
    async_fetch_timer_.Stop();
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&OhttpKeyService::MaybeStartOrRescheduleAsyncFetch,
                     weak_factory_.GetWeakPtr()));
}

void OhttpKeyService::GetOhttpKey(Callback callback) {
  if (!enabled_) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  // If there is a valid key in memory, use it directly.
  if (ohttp_key_ && ohttp_key_->expiration > base::Time::Now()) {
    std::move(callback).Run(ohttp_key_->key);
    return;
  }

  StartFetch(std::move(callback));
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

  if (response_code == kKeyRelatedHttpErrorCode) {
    // The failure is caused by unrecognized key. This is a hard failure, so
    // clear the key immediately.
    ohttp_key_ = absl::nullopt;
    server_triggered_fetch_scheduled_ = true;
    // Introduce an artificial delay so the server cannot correlate the key
    // fetch request with the original lookup request.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&OhttpKeyService::MaybeStartServerTriggeredFetch,
                       weak_factory_.GetWeakPtr(), key),
        base::Seconds(base::RandInt(0, kServerTriggeredFetchMaxDelayTimeSec)));
    return;
  }

  if (response_code == net::HTTP_OK && headers->HasHeader(kKeyRotatedHeader)) {
    server_triggered_fetch_scheduled_ = true;
    // The key is still valid, but it is close to expiration. It is a soft
    // failure, so do not clear the key immediately.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&OhttpKeyService::MaybeStartServerTriggeredFetch,
                       weak_factory_.GetWeakPtr(), key),
        base::Seconds(base::RandInt(0, kServerTriggeredFetchMaxDelayTimeSec)));
    return;
  }
}

void OhttpKeyService::StartFetch(Callback callback) {
  if (backoff_operator_->IsInBackoffMode()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  if (callback) {
    pending_callbacks_.AddUnsafe(std::move(callback));
  }
  // If url_loader_ is not null, that means a request is already in progress.
  // Will notify the callback when it is completed.
  if (url_loader_) {
    return;
  }
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kKeyFetchServerUrl);
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 kOhttpKeyTrafficAnnotation);
  url_loader_->SetTimeoutDuration(kKeyFetchTimeout);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&OhttpKeyService::OnURLLoaderComplete,
                     weak_factory_.GetWeakPtr()));
}

void OhttpKeyService::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  // TODO(crbug.com/1407283): Log net error and response code.
  DCHECK(url_loader_);
  int response_code = 0;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }
  bool is_key_fetch_successful = response_body &&
                                 url_loader_->NetError() == net::OK &&
                                 response_code == net::HTTP_OK;

  url_loader_.reset();
  if (is_key_fetch_successful) {
    ohttp_key_ = {*response_body, base::Time::Now() + kKeyExpirationDuration};
    StoreKeyToPref();
    backoff_operator_->ReportSuccess();
  } else {
    backoff_operator_->ReportError();
  }
  pending_callbacks_.Notify(is_key_fetch_successful
                                ? absl::optional<std::string>(*response_body)
                                : absl::nullopt);
}

void OhttpKeyService::MaybeStartOrRescheduleAsyncFetch() {
  if (!enabled_) {
    return;
  }

  if (ShouldStartAsyncFetch()) {
    StartFetch(base::BindOnce(&OhttpKeyService::OnAsyncFetchCompleted,
                              weak_factory_.GetWeakPtr()));
  } else {
    async_fetch_timer_.Start(
        FROM_HERE, kAsyncFetchCheckInterval, this,
        &OhttpKeyService::MaybeStartOrRescheduleAsyncFetch);
  }
}

void OhttpKeyService::OnAsyncFetchCompleted(
    absl::optional<std::string> ohttp_key) {
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

void OhttpKeyService::MaybeStartServerTriggeredFetch(std::string previous_key) {
  server_triggered_fetch_scheduled_ = false;
  if (ohttp_key_ && ohttp_key_->key != previous_key) {
    // The key has already been updated, no action needed.
    return;
  }

  StartFetch(base::NullCallback());
}

void OhttpKeyService::PopulateKeyFromPref() {
  std::string key =
      pref_service_->GetString(prefs::kSafeBrowsingHashRealTimeOhttpKey);
  base::Time expiration_time = pref_service_->GetTime(
      prefs::kSafeBrowsingHashRealTimeOhttpExpirationTime);
  if (!key.empty() && expiration_time > base::Time::Now()) {
    ohttp_key_ = {key, expiration_time};
  }
}

void OhttpKeyService::StoreKeyToPref() {
  if (ohttp_key_ && ohttp_key_->expiration > base::Time::Now()) {
    pref_service_->SetString(prefs::kSafeBrowsingHashRealTimeOhttpKey,
                             ohttp_key_->key);
    pref_service_->SetTime(prefs::kSafeBrowsingHashRealTimeOhttpExpirationTime,
                           ohttp_key_->expiration);
  }
}

void OhttpKeyService::Shutdown() {
  url_loader_.reset();
  pending_callbacks_.Notify(absl::nullopt);
  pref_change_registrar_.RemoveAll();
  async_fetch_timer_.Stop();
}

void OhttpKeyService::set_ohttp_key_for_testing(
    OhttpKeyAndExpiration ohttp_key) {
  ohttp_key_ = ohttp_key;
}

absl::optional<OhttpKeyService::OhttpKeyAndExpiration>
OhttpKeyService::get_ohttp_key_for_testing() {
  return ohttp_key_;
}

}  // namespace safe_browsing
