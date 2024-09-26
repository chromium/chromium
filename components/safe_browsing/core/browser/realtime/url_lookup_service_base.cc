// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"

#include <memory>

#include "base/base64url.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/common/utils.h"
#include "net/base/ip_address.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace safe_browsing {

namespace {

const size_t kNumFailuresToEnforceBackoff = 3;

const size_t kMinBackOffResetDurationInSeconds = 5 * 60;   //  5 minutes.
const size_t kMaxBackOffResetDurationInSeconds = 30 * 60;  // 30 minutes.

const size_t kURLLookupTimeoutDurationInSeconds = 3;

// Represents the value stored in the |version| field of |RTLookupRequest|.
const int kRTLookupRequestVersion = 3;

// UMA helper functions.
void RecordBooleanWithAndWithoutSuffix(const std::string& metric,
                                       const std::string& suffix,
                                       bool value) {
  base::UmaHistogramBoolean(metric, value);
  base::UmaHistogramBoolean(metric + suffix, value);
}

void RecordSparseWithAndWithoutSuffix(const std::string& metric,
                                      const std::string& suffix,
                                      int32_t value) {
  base::UmaHistogramSparse(metric, value);
  base::UmaHistogramSparse(metric + suffix, value);
}

void RecordTimesWithAndWithoutSuffix(const std::string& metric,
                                     const std::string& suffix,
                                     base::TimeDelta value) {
  base::UmaHistogramTimes(metric, value);
  base::UmaHistogramTimes(metric + suffix, value);
}

void RecordCount100WithAndWithoutSuffix(const std::string& metric,
                                        const std::string& suffix,
                                        int value) {
  base::UmaHistogramCounts100(metric, value);
  base::UmaHistogramCounts100(metric + suffix, value);
}

void RecordCount1MWithAndWithoutSuffix(const std::string& metric,
                                       const std::string& suffix,
                                       int value) {
  base::UmaHistogramCounts1M(metric, value);
  base::UmaHistogramCounts1M(metric + suffix, value);
}

void RecordRequestPopulationWithAndWithoutSuffix(
    const std::string& metric,
    const std::string& suffix,
    ChromeUserPopulation::UserPopulation population) {
  base::UmaHistogramExactLinear(metric, population,
                                ChromeUserPopulation::UserPopulation_MAX + 1);
  base::UmaHistogramExactLinear(metric + suffix, population,
                                ChromeUserPopulation::UserPopulation_MAX + 1);
}

void RecordNetworkResultWithAndWithoutSuffix(const std::string& metric,
                                             const std::string& suffix,
                                             int net_error,
                                             int response_code) {
  RecordHttpResponseOrErrorCode(metric.c_str(), net_error, response_code);
  RecordHttpResponseOrErrorCode((metric + suffix).c_str(), net_error,
                                response_code);
}

RTLookupRequest::OSType GetRTLookupRequestOSType() {
#if BUILDFLAG(IS_ANDROID)
  return RTLookupRequest::OS_TYPE_ANDROID;
#elif BUILDFLAG(IS_CHROMEOS)
  return RTLookupRequest::OS_TYPE_CHROME_OS;
#elif BUILDFLAG(IS_IOS)
  return RTLookupRequest::OS_TYPE_IOS;
#elif BUILDFLAG(IS_LINUX)
  return RTLookupRequest::OS_TYPE_LINUX;
#elif BUILDFLAG(IS_MAC)
  return RTLookupRequest::OS_TYPE_MAC;
#elif BUILDFLAG(IS_WIN)
  return RTLookupRequest::OS_TYPE_WINDOWS;
#else
  return RTLookupRequest::OS_TYPE_UNSPECIFIED;
#endif
}

void InvokeLookupResponseCallbacks(
    std::vector<RTLookupResponseCallback> callbacks,
    bool is_rt_lookup_successful,
    bool is_cached_response,
    std::unique_ptr<RTLookupResponse> response) {
  for (auto& callback : callbacks) {
    auto response2 = std::make_unique<RTLookupResponse>(*response);
    std::move(callback).Run(is_rt_lookup_successful, is_cached_response,
                            std::move(response2));
  }
}

}  // namespace

RealTimeUrlLookupServiceBase::PendingRTLookupRequestData::
    PendingRTLookupRequestData(std::unique_ptr<network::SimpleURLLoader> loader)
    : loader_(std::move(loader)) {}
RealTimeUrlLookupServiceBase::PendingRTLookupRequestData::
    PendingRTLookupRequestData(PendingRTLookupRequestData&&) = default;
RealTimeUrlLookupServiceBase::PendingRTLookupRequestData&
RealTimeUrlLookupServiceBase::PendingRTLookupRequestData::operator=(
    PendingRTLookupRequestData&&) = default;
RealTimeUrlLookupServiceBase::PendingRTLookupRequestData::
    ~PendingRTLookupRequestData() = default;

void RealTimeUrlLookupServiceBase::PendingRTLookupRequestData::AddCallback(
    RTLookupResponseCallback callback) {
  if (!callback.is_null()) {
    callbacks_.emplace_back(std::move(callback));
  }
}

RealTimeUrlLookupServiceBase::RealTimeUrlLookupServiceBase(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    VerdictCacheManager* cache_manager,
    base::RepeatingCallback<ChromeUserPopulation()>
        get_user_population_callback,
    ReferrerChainProvider* referrer_chain_provider,
    PrefService* pref_service,
    WebUIDelegate* delegate)
    : url_loader_factory_(url_loader_factory),
      cache_manager_(cache_manager),
      pref_service_(pref_service),
      get_user_population_callback_(get_user_population_callback),
      referrer_chain_provider_(referrer_chain_provider),
      backoff_operator_(std::make_unique<BackoffOperator>(
          /*num_failures_to_enforce_backoff=*/kNumFailuresToEnforceBackoff,
          /*min_backoff_reset_duration_in_seconds=*/
          kMinBackOffResetDurationInSeconds,
          /*max_backoff_reset_duration_in_seconds=*/
          kMaxBackOffResetDurationInSeconds)),
      webui_delegate_(delegate) {}

RealTimeUrlLookupServiceBase::~RealTimeUrlLookupServiceBase() = default;

// static
SBThreatType RealTimeUrlLookupServiceBase::GetSBThreatTypeForRTThreatType(
    RTLookupResponse::ThreatInfo::ThreatType rt_threat_type,
    RTLookupResponse::ThreatInfo::VerdictType rt_verdict_type) {
  using enum SBThreatType;

  if (rt_threat_type == RTLookupResponse::ThreatInfo::MANAGED_POLICY) {
    switch (rt_verdict_type) {
      case RTLookupResponse::ThreatInfo::DANGEROUS:
        return SB_THREAT_TYPE_MANAGED_POLICY_BLOCK;
      case RTLookupResponse::ThreatInfo::WARN:
        return SB_THREAT_TYPE_MANAGED_POLICY_WARN;
      case RTLookupResponse::ThreatInfo::SAFE:
        return SB_THREAT_TYPE_SAFE;
      default:
        NOTREACHED_IN_MIGRATION();
        return SB_THREAT_TYPE_SAFE;
    }
  }

  if (rt_verdict_type != RTLookupResponse::ThreatInfo::DANGEROUS) {
    return SB_THREAT_TYPE_SAFE;
  }

  switch (rt_threat_type) {
    case RTLookupResponse::ThreatInfo::WEB_MALWARE:
      return SB_THREAT_TYPE_URL_MALWARE;
    case RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING:
      return SB_THREAT_TYPE_URL_PHISHING;
    case RTLookupResponse::ThreatInfo::UNWANTED_SOFTWARE:
      return SB_THREAT_TYPE_URL_UNWANTED;
    case RTLookupResponse::ThreatInfo::UNCLEAR_BILLING:
      return SB_THREAT_TYPE_BILLING;
    case RTLookupResponse::ThreatInfo::MANAGED_POLICY:
    case RTLookupResponse::ThreatInfo::THREAT_TYPE_UNSPECIFIED:
      NOTREACHED_IN_MIGRATION()
          << "Unexpected RTLookupResponse::ThreatType encountered";
      return SB_THREAT_TYPE_SAFE;
  }
}

// static
GURL RealTimeUrlLookupServiceBase::SanitizeURL(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  return url.ReplaceComponents(replacements);
}

// static
void RealTimeUrlLookupServiceBase::SanitizeReferrerChainEntries(
    ReferrerChain* referrer_chain,
    std::optional<base::Time> min_allowed_timestamp,
    bool should_remove_subresource_url) {
  for (ReferrerChainEntry& entry : *referrer_chain) {
    // Remove URLs in the entry if the referrer chain is collected
    // before the min_timestamp.
    if (min_allowed_timestamp.has_value() &&
        entry.navigation_time_msec() <
            min_allowed_timestamp->InMillisecondsSinceUnixEpoch()) {
      entry.clear_url();
      entry.clear_main_frame_url();
      entry.clear_referrer_url();
      entry.clear_referrer_main_frame_url();
      for (ReferrerChainEntry::ServerRedirect& server_redirect_entry :
           *entry.mutable_server_redirect_chain()) {
        server_redirect_entry.clear_url();
      }
      entry.set_is_url_removed_by_policy(true);
    }
    if (!should_remove_subresource_url) {
      continue;
    }
    // is_subframe_url_removed is added in the proto.
    // If the entry sets main_frame_url, that means the url is triggered in a
    // subframe. Thus replace the url with the main_frame_url and clear
    // the main_frame_url field.
    if (entry.has_main_frame_url()) {
      entry.set_url(entry.main_frame_url());
      entry.clear_main_frame_url();
      entry.set_is_subframe_url_removed(true);
    }
    // If the entry sets referrer_main_frame_url, that means the referrer_url is
    // triggered in a subframe. Thus replace the referrer_url with the
    // referrer_main_frame_url and clear the referrer_main_frame_url field.
    if (entry.has_referrer_main_frame_url()) {
      entry.set_referrer_url(entry.referrer_main_frame_url());
      entry.clear_referrer_main_frame_url();
      entry.set_is_subframe_referrer_url_removed(true);
    }
  }
}

base::WeakPtr<RealTimeUrlLookupServiceBase>
RealTimeUrlLookupServiceBase::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool RealTimeUrlLookupServiceBase::IsInBackoffMode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool in_backoff = backoff_operator_->IsInBackoffMode();
  RecordBooleanWithAndWithoutSuffix("SafeBrowsing.RT.BackoffState",
                                    GetMetricSuffix(), in_backoff);
  return in_backoff;
}

std::unique_ptr<RTLookupResponse>
RealTimeUrlLookupServiceBase::GetCachedRealTimeUrlVerdict(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<RTLookupResponse::ThreatInfo> cached_threat_info =
      std::make_unique<RTLookupResponse::ThreatInfo>();

  base::TimeTicks get_cache_start_time = base::TimeTicks::Now();

  RTLookupResponse::ThreatInfo::VerdictType verdict_type =
      cache_manager_ ? cache_manager_->GetCachedRealTimeUrlVerdict(
                           url, cached_threat_info.get())
                     : RTLookupResponse::ThreatInfo::VERDICT_TYPE_UNSPECIFIED;

  RecordSparseWithAndWithoutSuffix("SafeBrowsing.RT.GetCacheResult",
                                   GetMetricSuffix(), verdict_type);
  RecordTimesWithAndWithoutSuffix(
      "SafeBrowsing.RT.GetCache.Time", GetMetricSuffix(),
      base::TimeTicks::Now() - get_cache_start_time);

  if (verdict_type == RTLookupResponse::ThreatInfo::SAFE ||
      verdict_type == RTLookupResponse::ThreatInfo::DANGEROUS) {
    auto cache_response = std::make_unique<RTLookupResponse>();
    RTLookupResponse::ThreatInfo* new_threat_info =
        cache_response->add_threat_info();
    *new_threat_info = *cached_threat_info;
    return cache_response;
  }
  return nullptr;
}

void RealTimeUrlLookupServiceBase::MayBeCacheRealTimeUrlVerdict(
    RTLookupResponse response) {
  if (cache_manager_ && response.threat_info_size() > 0) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&VerdictCacheManager::CacheRealTimeUrlVerdict,
                                  cache_manager_->GetWeakPtr(), response,
                                  base::Time::Now()));
  }
}

void RealTimeUrlLookupServiceBase::SendSampledRequest(
    const GURL& url,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    SessionID tab_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(url.is_valid());

  MaybeSendRequest(url,
                   /* access_token_string */ std::string(),
                   /* response_callback */ base::NullCallback(),
                   std::move(callback_task_runner),
                   /* is_sampled_report */ true, tab_id);
}

void RealTimeUrlLookupServiceBase::StartLookup(
    const GURL& url,
    RTLookupResponseCallback response_callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    SessionID tab_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(url.is_valid());

  // Check cache.
  std::unique_ptr<RTLookupResponse> cache_response =
      GetCachedRealTimeUrlVerdict(url);
  if (cache_response) {
    callback_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(response_callback),
                                  /* is_rt_lookup_successful */ true,
                                  /* is_cached_response */ true,
                                  std::move(cache_response)));
    return;
  }

  if (IsInBackoffMode()) {
    callback_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(response_callback),
                                  /* is_rt_lookup_successful */ false,
                                  /* is_cached_response */ false,
                                  /* response */ nullptr));
    return;
  }

  if (CanPerformFullURLLookupWithToken()) {
    GetAccessToken(url, std::move(response_callback),
                   std::move(callback_task_runner), tab_id);
  } else {
    MaybeSendRequest(url,
                     /* access_token_string */ std::string(),
                     std::move(response_callback),
                     std::move(callback_task_runner),
                     /* is_sampled_report */ false, tab_id);
  }
}

void RealTimeUrlLookupServiceBase::MaybeSendRequest(
    const GURL& url,
    const std::string& access_token_string,
    RTLookupResponseCallback response_callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    bool is_sampled_report,
    SessionID tab_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GURL sanitized_url = SanitizeURL(url);

  bool request_is_already_pending = pending_requests_.count(sanitized_url) > 0;
  RecordBooleanWithAndWithoutSuffix("SafeBrowsing.RT.Request.Concurrent",
                                    GetMetricSuffix(),
                                    request_is_already_pending);

  // If a request for this URL is already pending, queue up the callback.
  // This is done to prevent duplicating network requests to the backend
  // service and prevent unneeded QPS increase.
  if (request_is_already_pending) {
    pending_requests_.at(sanitized_url)
        .AddCallback(std::move(response_callback));
    return;
  }

  std::unique_ptr<RTLookupRequest> request =
      FillRequestProto(sanitized_url, is_sampled_report, tab_id);
  RecordRequestPopulationWithAndWithoutSuffix(
      "SafeBrowsing.RT.Request.UserPopulation", GetMetricSuffix(),
      request->population().user_population());
  RecordCount100WithAndWithoutSuffix(
      "SafeBrowsing.RT.Request.ReferrerChainLength", GetMetricSuffix(),
      request->referrer_chain().size());
  // Track sampled and full report
  base::UmaHistogramBoolean("SafeBrowsing.RT.SampledRequestSent",
                            is_sampled_report);

  std::string req_data;
  request->SerializeToString(&req_data);

  auto resource_request = GetResourceRequest();
  if (!access_token_string.empty()) {
    LogAuthenticatedCookieResets(
        *resource_request,
        SafeBrowsingAuthenticatedEndpoint::kRealtimeUrlLookup);
    SetAccessTokenAndClearCookieInResourceRequest(resource_request.get(),
                                                  access_token_string);
  }
  RecordBooleanWithAndWithoutSuffix("SafeBrowsing.RT.HasTokenInRequest",
                                    GetMetricSuffix(),
                                    !access_token_string.empty());

  MaybeLogLastProtegoPingTimeToPrefs(!access_token_string.empty());
  std::optional<int> webui_token =
      LogLookupRequest(*request, access_token_string);

  // NOTE: Pass |callback_task_runner| by copying it here as it's also needed
  // just below.
  SendRequestInternal(
      sanitized_url, std::move(resource_request), req_data, access_token_string,
      std::move(response_callback), callback_task_runner,
      request->population().user_population(), is_sampled_report, webui_token);
}

void RealTimeUrlLookupServiceBase::SendRequestInternal(
    const GURL& url,
    std::unique_ptr<network::ResourceRequest> resource_request,
    const std::string& req_data,
    std::optional<std::string> access_token_string,
    RTLookupResponseCallback response_callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    ChromeUserPopulation::UserPopulation user_population,
    bool is_sampled_report,
    std::optional<int> webui_token) {
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       GetTrafficAnnotationTag());
  RecordCount1MWithAndWithoutSuffix("SafeBrowsing.RT.Request.Size",
                                    GetMetricSuffix(), req_data.size());
  base::TimeTicks start_time = base::TimeTicks::Now();
  if (!first_request_start_time_) {
    first_request_start_time_ = start_time;
  }
  loader->AttachStringForUpload(req_data, "application/octet-stream");
  loader->SetTimeoutDuration(base::Seconds(kURLLookupTimeoutDurationInSeconds));
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&RealTimeUrlLookupServiceBase::OnURLLoaderComplete,
                     GetWeakPtr(), url, access_token_string, user_population,
                     start_time, is_sampled_report,
                     std::move(callback_task_runner), webui_token));

  DCHECK_EQ(pending_requests_.count(url), 0u);
  PendingRTLookupRequestData data(std::move(loader));
  data.AddCallback(std::move(response_callback));
  pending_requests_.emplace(url, std::move(data));
}

void RealTimeUrlLookupServiceBase::OnURLLoaderComplete(
    const GURL& url,
    std::optional<std::string> access_token_string,
    ChromeUserPopulation::UserPopulation user_population,
    base::TimeTicks request_start_time,
    bool is_sampled_report,
    scoped_refptr<base::SequencedTaskRunner> response_callback_task_runner,
    std::optional<int> webui_token,
    std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(first_request_start_time_);

  auto it = pending_requests_.find(url);
  CHECK(it != pending_requests_.end(), base::NotFatalUntil::M130)
      << "Request not found";

  RecordTimesWithAndWithoutSuffix("SafeBrowsing.RT.Network.Time",
                                  GetMetricSuffix(),
                                  base::TimeTicks::Now() - request_start_time);

  network::SimpleURLLoader* url_loader = it->second.loader();
  int net_error = url_loader->NetError();
  int response_code = 0;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers) {
    response_code = url_loader->ResponseInfo()->headers->response_code();
  }
  std::string report_type_suffix =
      is_sampled_report ? ".SampledPing" : ".NormalPing";
  RecordNetworkResultWithAndWithoutSuffix("SafeBrowsing.RT.Network.Result",
                                          GetMetricSuffix(), net_error,
                                          response_code);
  RecordHttpResponseOrErrorCode(
      ("SafeBrowsing.RT.Network.Result" + report_type_suffix).c_str(),
      net_error, response_code);

  bool was_first_request = *first_request_start_time_ == request_start_time;
  bool request_had_cookie = url_loader->ResponseInfo() &&
                            url_loader->ResponseInfo()->was_cookie_in_request;
  bool sent_with_token = access_token_string && !access_token_string->empty();
  MaybeLogProtegoPingCookieHistograms(request_had_cookie, was_first_request,
                                      sent_with_token);

  if (response_code == net::HTTP_UNAUTHORIZED &&
      access_token_string.has_value()) {
    OnResponseUnauthorized(access_token_string.value());
  }

  auto response = std::make_unique<RTLookupResponse>();
  bool is_rt_lookup_successful = false;
  if (net_error == net::OK && response_code == net::HTTP_OK) {
    if (response->ParseFromString(*response_body)) {
      is_rt_lookup_successful = true;
      backoff_operator_->ReportSuccess();
    } else {
      backoff_operator_->ReportError();
    }
  } else if (!ErrorIsRetriable(net_error, response_code)) {
    backoff_operator_->ReportError();
  }

  RecordBooleanWithAndWithoutSuffix("SafeBrowsing.RT.IsLookupSuccessful",
                                    GetMetricSuffix(), is_rt_lookup_successful);
  base::UmaHistogramBoolean(
      "SafeBrowsing.RT.IsLookupSuccessful" + report_type_suffix,
      is_rt_lookup_successful);

  MayBeCacheRealTimeUrlVerdict(*response);

  if (is_rt_lookup_successful) {
    LogLookupResponseForToken(webui_token, *response);
  }

  RecordCount100WithAndWithoutSuffix("SafeBrowsing.RT.ThreatInfoSize",
                                     GetMetricSuffix(),
                                     response->threat_info_size());
  if (response && response->threat_info_size() > 0) {
    RecordSparseWithAndWithoutSuffix("SafeBrowsing.RT.Response.VerdictType",
                                     GetMetricSuffix(),
                                     response->threat_info(0).verdict_type());

    std::string enhanced_protection_suffix =
        user_population == ChromeUserPopulation::ENHANCED_PROTECTION
            ? "EnhancedProtection"
            : "NotEnhancedProtection";

    // Log histograms with suffix and avoid using
    // RecordSparseWithAndWithoutSuffix as it (without) has been logged.
    base::UmaHistogramSparse(
        "SafeBrowsing.RT.Response.VerdictType." + enhanced_protection_suffix,
        response->threat_info(0).verdict_type());
  }
  // The response callback list could be empty in the case of sampled reports.
  if (it->second.has_callbacks()) {
    response_callback_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&InvokeLookupResponseCallbacks,
                       it->second.take_callbacks(), is_rt_lookup_successful,
                       /* is_cached_response */ false, std::move(response)));
  }

  pending_requests_.erase(it);
}

std::unique_ptr<network::ResourceRequest>
RealTimeUrlLookupServiceBase::GetResourceRequest() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetRealTimeLookupUrl();
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->method = "POST";
  if (!ShouldIncludeCredentials())
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  return resource_request;
}

std::unique_ptr<RTLookupRequest> RealTimeUrlLookupServiceBase::FillRequestProto(
    const GURL& url,
    bool is_sampled_report,
    SessionID tab_id) {
  auto request = std::make_unique<RTLookupRequest>();
  request->set_url(url.spec());
  request->set_lookup_type(RTLookupRequest::NAVIGATION);
  request->set_version(kRTLookupRequestVersion);
  request->set_os_type(GetRTLookupRequestOSType());
  request->set_report_type(is_sampled_report ? RTLookupRequest::SAMPLED_REPORT
                                             : RTLookupRequest::FULL_REPORT);
  request->set_frame_type(RTLookupRequest::MAIN_FRAME);
  std::optional<std::string> dm_token_string = GetDMTokenString();
  if (dm_token_string.has_value()) {
    request->set_dm_token(dm_token_string.value());

    std::string email = GetUserEmail();
    if (!email.empty()) {
      request->set_email(std::move(email));
    }

    // Check for the profile token here because we want to avoid cases where the
    // value is populated in the non-enterprise case.
    std::string profile_dm_token = GetProfileDMTokenString();
    if (!profile_dm_token.empty()) {
      request->set_profile_dm_token(std::move(profile_dm_token));
    }
  }

  std::string browser_dm_token = GetBrowserDMTokenString();
  if (!browser_dm_token.empty()) {
    request->set_browser_dm_token(std::move(browser_dm_token));
  }

  std::unique_ptr<enterprise_connectors::ClientMetadata> client_metadata =
      GetClientMetadata();

  if (client_metadata) {
    *request->mutable_client_reporting_metadata() = std::move(*client_metadata);
  }

  *request->mutable_population() = get_user_population_callback_.Run();
  if (referrer_chain_provider_) {
    ReferrerChainProvider::AttributionResult attribution_result =
        referrer_chain_provider_->IdentifyReferrerChainByPendingEventURL(
            SanitizeURL(url), GetReferrerUserGestureLimit(),
            request->mutable_referrer_chain());
    // The navigation event may not be found for various reasons. One
    // possibility is that with async checks, the event URL may no longer be
    // pending if the page has already loaded. If the navigation event is not
    // found, try to fetch the referrer chain as a regular event URL rather than
    // a pending one.
    if (attribution_result == ReferrerChainProvider::AttributionResult::
                                  NAVIGATION_EVENT_NOT_FOUND &&
        base::FeatureList::IsEnabled(
            safe_browsing::kSafeBrowsingAsyncRealTimeCheck)) {
      CHECK(request->referrer_chain().empty());
      referrer_chain_provider_->IdentifyReferrerChainByEventURL(
          SanitizeURL(url), tab_id, GetReferrerUserGestureLimit(),
          request->mutable_referrer_chain());

      RecordBooleanWithAndWithoutSuffix(
          "SafeBrowsing.RT.EventUrlReferrerChainFetchSucceeded",
          GetMetricSuffix(), !request->referrer_chain().empty());
    }
    SanitizeReferrerChainEntries(request->mutable_referrer_chain(),
                                 GetMinAllowedTimestampForReferrerChains(),
                                 /*should_remove_subresource_url=*/
                                 !CanIncludeSubframeUrlInReferrerChain());
  }

  if (CanSendPageLoadToken() && cache_manager_) {
    ChromeUserPopulation::PageLoadToken token;
    token = cache_manager_->CreatePageLoadToken(url);
    request->mutable_population()->mutable_page_load_tokens()->Add()->Swap(
        &token);
  }

  return request;
}

std::optional<int> RealTimeUrlLookupServiceBase::LogLookupRequest(
    const RTLookupRequest& request,
    const std::string& oauth_token) {
  if (!webui_delegate_) {
    return std::nullopt;
  }

  return webui_delegate_->AddToURTLookupPings(request, oauth_token);
}

void RealTimeUrlLookupServiceBase::LogLookupResponseForToken(
    std::optional<int> token,
    const RTLookupResponse& response) {
  if (!webui_delegate_) {
    return;
  }

  if (!token.has_value()) {
    return;
  }

  webui_delegate_->AddToURTLookupResponses(token.value(), response);
}

void RealTimeUrlLookupServiceBase::OnResponseUnauthorized(
    const std::string& invalid_access_token) {}

void RealTimeUrlLookupServiceBase::Shutdown() {
  pending_requests_.clear();

  // Clear references to other KeyedServices.
  cache_manager_ = nullptr;
  referrer_chain_provider_ = nullptr;
}

}  // namespace safe_browsing
