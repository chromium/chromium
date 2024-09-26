// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/realtime/url_lookup_service.h"

#include "base/base64url.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/realtime/policy_engine.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/unified_consent/pref_names.h"
#include "net/base/ip_address.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

constexpr int kDefaultRealTimeUrlLookupReferrerLength = 2;

// Probability for sending protego requests for urls on the allowlist
const float kProbabilityForSendingSampledRequests = 0.01;

constexpr char kCookieHistogramPrefix[] = "SafeBrowsing.RT.Request.HadCookie";

}  // namespace

namespace safe_browsing {

RealTimeUrlLookupService::RealTimeUrlLookupService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    VerdictCacheManager* cache_manager,
    base::RepeatingCallback<ChromeUserPopulation()>
        get_user_population_callback,
    PrefService* pref_service,
    std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
    const ClientConfiguredForTokenFetchesCallback& client_token_config_callback,
    bool is_off_the_record,
    variations::VariationsService* variations_service,
    ReferrerChainProvider* referrer_chain_provider,
    WebUIDelegate* delegate)
    : RealTimeUrlLookupServiceBase(url_loader_factory,
                                   cache_manager,
                                   get_user_population_callback,
                                   referrer_chain_provider,
                                   pref_service,
                                   delegate),
      pref_service_(pref_service),
      token_fetcher_(std::move(token_fetcher)),
      client_token_config_callback_(client_token_config_callback),
      is_off_the_record_(is_off_the_record),
      variations_(variations_service) {
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kSafeBrowsingEnhanced,
      base::BindRepeating(&RealTimeUrlLookupService::OnPrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      base::BindRepeating(&RealTimeUrlLookupService::OnPrefChanged,
                          base::Unretained(this)));
}

void RealTimeUrlLookupService::GetAccessToken(
    const GURL& url,
    RTLookupResponseCallback response_callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    SessionID tab_id) {
  token_fetcher_->Start(base::BindOnce(
      &RealTimeUrlLookupService::OnGetAccessToken, weak_factory_.GetWeakPtr(),
      url, std::move(response_callback), std::move(callback_task_runner),
      base::TimeTicks::Now(), tab_id));
}

void RealTimeUrlLookupService::OnPrefChanged() {
  if (CanPerformFullURLLookup()) {
    url_lookup_enabled_timestamp_ = base::Time::Now();
  }
}

void RealTimeUrlLookupService::OnGetAccessToken(
    const GURL& url,
    RTLookupResponseCallback response_callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::TimeTicks get_token_start_time,
    SessionID tab_id,
    const std::string& access_token) {
  if (shutting_down_)
    return;

  base::UmaHistogramTimes("SafeBrowsing.RT.GetToken.Time",
                          base::TimeTicks::Now() - get_token_start_time);
  base::UmaHistogramBoolean("SafeBrowsing.RT.HasTokenFromFetcher",
                            !access_token.empty());
  MaybeSendRequest(url, access_token, std::move(response_callback),
                   std::move(callback_task_runner),
                   /* is_sampled_report */ false, tab_id);
}

void RealTimeUrlLookupService::OnResponseUnauthorized(
    const std::string& invalid_access_token) {
  token_fetcher_->OnInvalidAccessToken(invalid_access_token);
}

RealTimeUrlLookupService::~RealTimeUrlLookupService() = default;

bool RealTimeUrlLookupService::CanPerformFullURLLookup() const {
  return RealTimePolicyEngine::CanPerformFullURLLookup(
      pref_service_, is_off_the_record_, variations_);
}

bool RealTimeUrlLookupService::CanPerformFullURLLookupWithToken() const {
  return RealTimePolicyEngine::CanPerformFullURLLookupWithToken(
      pref_service_, is_off_the_record_, client_token_config_callback_,
      variations_);
}

int RealTimeUrlLookupService::GetReferrerUserGestureLimit() const {
  return kDefaultRealTimeUrlLookupReferrerLength;
}

bool RealTimeUrlLookupService::CanSendPageLoadToken() const {
  return true;
}

bool RealTimeUrlLookupService::CanIncludeSubframeUrlInReferrerChain() const {
  return IsEnhancedProtectionEnabled(*pref_service_) &&
         CanPerformFullURLLookup();
}

bool RealTimeUrlLookupService::CanCheckSafeBrowsingDb() const {
  // Always return true, because consumer real time URL check only works when
  // safe browsing is enabled.
  return true;
}

bool RealTimeUrlLookupService::CanCheckSafeBrowsingHighConfidenceAllowlist()
    const {
  // Always return true, because consumer real time URL check always checks
  // high confidence allowlist.
  return true;
}

bool RealTimeUrlLookupService::CanSendRTSampleRequest() const {
  return IsExtendedReportingEnabled(*pref_service_) &&
         (bypass_protego_probability_for_tests_ ||
          base::RandDouble() <= kProbabilityForSendingSampledRequests);
}

std::string RealTimeUrlLookupService::GetUserEmail() const {
  return "";
}

std::string RealTimeUrlLookupService::GetBrowserDMTokenString() const {
  return "";
}

std::string RealTimeUrlLookupService::GetProfileDMTokenString() const {
  return "";
}

std::unique_ptr<enterprise_connectors::ClientMetadata>
RealTimeUrlLookupService::GetClientMetadata() const {
  return nullptr;
}

void RealTimeUrlLookupService::Shutdown() {
  shutting_down_ = true;

  // Clear state that was potentially bound to the lifetime of other
  // KeyedServices by the embedder.
  token_fetcher_.reset();
  client_token_config_callback_ = ClientConfiguredForTokenFetchesCallback();

  pref_change_registrar_.RemoveAll();

  RealTimeUrlLookupServiceBase::Shutdown();
}

GURL RealTimeUrlLookupService::GetRealTimeLookupUrl() const {
  return GURL(
      "https://safebrowsing.google.com/safebrowsing/clientreport/realtime");
}

net::NetworkTrafficAnnotationTag
RealTimeUrlLookupService::GetTrafficAnnotationTag() const {
  return net::DefineNetworkTrafficAnnotation(
      "safe_browsing_realtime_url_lookup",
      R"(
        semantics {
          sender: "Safe Browsing"
          description:
            "When Safe Browsing can't detect that a URL is safe based on its "
            "local database, it sends the top-level URL to Google to verify it "
            "before showing a warning to the user."
          trigger:
            "When a main frame URL fails to match the local hash-prefix "
            "database of known safe URLs and a valid result from a prior "
            "lookup is not already cached, this will be sent."
          data: "The main frame URL that did not match the local safelist."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "Safe Browsing cookie store"
          setting:
            "Users can disable Safe Browsing real time URL checks by "
            "unchecking 'Protect you and your device from dangerous sites' in "
            "Chromium settings under Privacy, or by unchecking 'Make searches "
            "and browsing better (Sends URLs of pages you visit to Google)' in "
            "Chromium settings under Privacy."
          chrome_policy {
            UrlKeyedAnonymizedDataCollectionEnabled {
              policy_options {mode: MANDATORY}
              UrlKeyedAnonymizedDataCollectionEnabled: false
            }
          }
        })");
}

std::optional<std::string> RealTimeUrlLookupService::GetDMTokenString() const {
  // DM token should only be set for enterprise requests.
  return std::nullopt;
}

std::string RealTimeUrlLookupService::GetMetricSuffix() const {
  return ".Consumer";
}

bool RealTimeUrlLookupService::CanCheckUrl(const GURL& url) {
  if (VerdictCacheManager::has_artificial_cached_url()) {
    return true;
  }
  return CanGetReputationOfUrl(url);
}

bool RealTimeUrlLookupService::ShouldIncludeCredentials() const {
  return true;
}

std::optional<base::Time>
RealTimeUrlLookupService::GetMinAllowedTimestampForReferrerChains() const {
  return url_lookup_enabled_timestamp_;
}

void RealTimeUrlLookupService::MaybeLogLastProtegoPingTimeToPrefs(
    bool sent_with_token) {
  // `pref_service_` can be null in tests.
  if (pref_service_ && IsEnhancedProtectionEnabled(*pref_service_)) {
    pref_service_->SetTime(
        sent_with_token
            ? prefs::kSafeBrowsingEsbProtegoPingWithTokenLastLogTime
            : prefs::kSafeBrowsingEsbProtegoPingWithoutTokenLastLogTime,
        base::Time::Now());
  }
}

void RealTimeUrlLookupService::MaybeLogProtegoPingCookieHistograms(
    bool request_had_cookie,
    bool was_first_request,
    bool sent_with_token) {
  std::string histogram_name = kCookieHistogramPrefix;
  base::StrAppend(&histogram_name,
                  {was_first_request ? ".FirstRequest" : ".SubsequentRequest"});
  base::UmaHistogramBoolean(histogram_name, request_had_cookie);
  // `pref_service_` can be null in tests.
  // This histogram variant is only logged for signed-out ESB users.
  if (!sent_with_token && pref_service_ &&
      IsEnhancedProtectionEnabled(*pref_service_)) {
    base::StrAppend(&histogram_name, {".SignedOutEsbUser"});
    base::UmaHistogramBoolean(histogram_name, request_had_cookie);
  }
}

}  // namespace safe_browsing
