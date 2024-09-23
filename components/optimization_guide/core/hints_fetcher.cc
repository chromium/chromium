// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/hints_fetcher.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/time/default_clock.h"
#include "components/optimization_guide/core/hints_processing_util.h"
#include "components/optimization_guide/core/optimization_guide_common.mojom.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace optimization_guide {

namespace {

// Returns the string that can be used to record histograms for the request
// context.
//
// Keep in sync with RequestContext variant list in
// //tools/metrics/histograms/metadata/optimization/histograms.xml.
std::string GetStringNameForRequestContext(
    proto::RequestContext request_context) {
  switch (request_context) {
    case proto::RequestContext::CONTEXT_UNSPECIFIED:
    case proto::RequestContext::CONTEXT_BATCH_UPDATE_MODELS:
      NOTREACHED_IN_MIGRATION();
      return "Unknown";
    case proto::RequestContext::CONTEXT_PAGE_NAVIGATION:
      return "PageNavigation";
    case proto::RequestContext::CONTEXT_BATCH_UPDATE_GOOGLE_SRP:
      return "BatchUpdateGoogleSRP";
    case proto::RequestContext::CONTEXT_BATCH_UPDATE_ACTIVE_TABS:
      return "BatchUpdateActiveTabs";
    case proto::RequestContext::CONTEXT_BOOKMARKS:
      return "Bookmarks";
    case proto::RequestContext::CONTEXT_JOURNEYS:
      return "Journeys";
    case proto::RequestContext::CONTEXT_NEW_TAB_PAGE:
      return "NewTabPage";
    case proto::RequestContext::CONTEXT_PAGE_INSIGHTS_HUB:
      return "PageInsightsHub";
    case proto::RequestContext::CONTEXT_NON_PERSONALIZED_PAGE_INSIGHTS_HUB:
      return "NonPersonalizedPageInsightsHub";
    case proto::RequestContext::CONTEXT_SHOPPING:
      return "Shopping";
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

void RecordRequestStatusHistogram(proto::RequestContext request_context,
                                  FetcherRequestStatus status) {
  DCHECK_NE(status, FetcherRequestStatus::kDeprecatedNetworkOffline);
  base::UmaHistogramEnumeration(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.RequestStatus." +
          GetStringNameForRequestContext(request_context),
      status);
}

// Appends override headers as specified by the command line arguments.
void AppendOverrideHeadersIfNeeded(network::ResourceRequest& request) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOptimizationGuideLanguageOverride)) {
    return;
  }
  request.headers.SetHeaderIfMissing(
      kOptimizationGuideLanguageOverrideHeaderKey,
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kOptimizationGuideLanguageOverride));
}

}  // namespace

HintsFetcher::HintsFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& optimization_guide_service_url,
    PrefService* pref_service,
    OptimizationGuideLogger* optimization_guide_logger)
    : optimization_guide_service_url_(
          net::AppendOrReplaceQueryParameter(optimization_guide_service_url,
                                             "key",
                                             std::nullopt)),
      optimization_guide_service_api_key_(
          features::GetOptimizationGuideServiceAPIKey()),
      pref_service_(pref_service),
      time_clock_(base::DefaultClock::GetInstance()),
      optimization_guide_logger_(optimization_guide_logger) {
  url_loader_factory_ = std::move(url_loader_factory);
  // Allow non-https scheme only when it is overridden in command line. This is
  // needed for iOS EG2 tests which don't support HTTPS embedded test servers
  // due to ssl certificate validation. So, the EG2 tests use HTTP hints
  // servers.
  CHECK(optimization_guide_service_url_.SchemeIs(url::kHttpsScheme) ||
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kOptimizationGuideServiceGetHintsURL));
  DCHECK(features::IsRemoteFetchingEnabled());
}

HintsFetcher::~HintsFetcher() {
  if (active_url_loader_) {
    if (hints_fetched_callback_)
      std::move(hints_fetched_callback_).Run(std::nullopt);
    RecordRequestStatusHistogram(request_context_,
                                 FetcherRequestStatus::kRequestCanceled);
  }
}

// static
void HintsFetcher::ClearHostsSuccessfullyFetched(PrefService* pref_service) {
  pref_service->SetDict(prefs::kHintsFetcherHostsSuccessfullyFetched,
                        base::Value::Dict());
}

void HintsFetcher::SetTimeClockForTesting(const base::Clock* time_clock) {
  time_clock_ = time_clock;
}

// static
bool HintsFetcher::WasHostCoveredByFetch(PrefService* pref_service,
                                         const std::string& host) {
  return WasHostCoveredByFetch(pref_service, host,
                               base::DefaultClock::GetInstance());
}

// static
bool HintsFetcher::WasHostCoveredByFetch(PrefService* pref_service,
                                         const std::string& host,
                                         const base::Clock* time_clock) {
  if (!optimization_guide::features::ShouldPersistHintsToDisk()) {
    // Don't consult the pref if we aren't even persisting hints to disk.
    return false;
  }

  ScopedDictPrefUpdate hosts_fetched(
      pref_service, prefs::kHintsFetcherHostsSuccessfullyFetched);
  std::optional<double> value =
      hosts_fetched->FindDouble(HashHostForDictionary(host));
  if (!value)
    return false;

  base::Time host_valid_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(*value));
  return host_valid_time > time_clock->Now();
}

// static
void HintsFetcher::ClearSingleFetchedHost(PrefService* pref_service,
                                          const std::string& host) {
  ScopedDictPrefUpdate hosts_fetched_list(
      pref_service, prefs::kHintsFetcherHostsSuccessfullyFetched);
  hosts_fetched_list->RemoveByDottedPath(HashHostForDictionary(host));
}

// static
void HintsFetcher::AddFetchedHostForTesting(PrefService* pref_service,
                                            const std::string& host,
                                            base::Time time) {
  ScopedDictPrefUpdate hosts_fetched_list(
      pref_service, prefs::kHintsFetcherHostsSuccessfullyFetched);
  hosts_fetched_list->Set(HashHostForDictionary(host),
                          time.ToDeltaSinceWindowsEpoch().InSecondsF());
}

bool HintsFetcher::FetchOptimizationGuideServiceHints(
    const std::vector<std::string>& hosts,
    const std::vector<GURL>& urls,
    const base::flat_set<optimization_guide::proto::OptimizationType>&
        optimization_types,
    optimization_guide::proto::RequestContext request_context,
    const std::string& locale,
    const std::string& access_token,
    bool skip_cache,
    HintsFetchedCallback hints_fetched_callback,
    std::optional<proto::RequestContextMetadata> request_context_metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(optimization_types.size(), 0u);
  request_context_ = request_context;

  if (active_url_loader_) {
    OPTIMIZATION_GUIDE_LOG(
        optimization_guide_common::mojom::LogSource::HINTS,
        optimization_guide_logger_,
        "No hints fetched: HintsFetcher busy in another fetch");
    RecordRequestStatusHistogram(request_context_,
                                 FetcherRequestStatus::kFetcherBusy);
    std::move(hints_fetched_callback).Run(std::nullopt);
    return false;
  }

  std::vector<std::string> filtered_hosts =
      GetSizeLimitedHostsDueForHintsRefresh(hosts, skip_cache);
  std::vector<GURL> valid_urls = GetSizeLimitedURLsForFetching(urls);
  if (filtered_hosts.empty() && valid_urls.empty()) {
    OPTIMIZATION_GUIDE_LOG(optimization_guide_common::mojom::LogSource::HINTS,
                           optimization_guide_logger_,
                           "No hints fetched: No hosts/URLs");
    RecordRequestStatusHistogram(
        request_context_, FetcherRequestStatus::kNoHostsOrURLsToFetchHints);
    std::move(hints_fetched_callback).Run(std::nullopt);
    return false;
  }

  DCHECK_GE(features::MaxHostsForOptimizationGuideServiceHintsFetch(),
            filtered_hosts.size());
  DCHECK_GE(features::MaxUrlsForOptimizationGuideServiceHintsFetch(),
            valid_urls.size());

  if (optimization_types.empty()) {
    OPTIMIZATION_GUIDE_LOG(optimization_guide_common::mojom::LogSource::HINTS,
                           optimization_guide_logger_,
                           "No hints fetched: No supported optimization types");
    RecordRequestStatusHistogram(
        request_context_,
        FetcherRequestStatus::kNoSupportedOptimizationTypesToFetchHints);
    std::move(hints_fetched_callback).Run(std::nullopt);
    return false;
  }

  hints_fetch_start_time_ = base::TimeTicks::Now();
  proto::GetHintsRequest get_hints_request;
  get_hints_request.add_supported_key_representations(proto::HOST);
  get_hints_request.add_supported_key_representations(proto::FULL_URL);

  for (const auto& optimization_type : optimization_types)
    get_hints_request.add_supported_optimizations(optimization_type);

  get_hints_request.set_context(request_context_);

  get_hints_request.set_locale(locale);

  if (request_context_metadata) {
    *get_hints_request.mutable_context_metadata() = *request_context_metadata;
  }

  *get_hints_request.mutable_origin_info() =
      optimization_guide::GetClientOriginInfo();

  for (const auto& url : valid_urls)
    get_hints_request.add_urls()->set_url(url.spec());

  for (const auto& host : filtered_hosts) {
    proto::HostInfo* host_info = get_hints_request.add_hosts();
    host_info->set_host(host);
  }

  std::string serialized_request;
  get_hints_request.SerializeToString(&serialized_request);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("hintsfetcher_gethintsrequest", R"(
        semantics {
          sender: "HintsFetcher"
          description:
            "Requests Hints from the Optimization Guide Service for use in "
            "providing metadata about page loads for Chrome for features such "
            "as price tracking and trust information about the websites users "
            "visit."
          trigger:
            "Requested once per page load if user has consented to sending "
            "URLs to Google to make browsing better and the browser does not "
            "have a valid hint for the current page load."
          data: "The URL and host of the current page load."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this via the Make searches and browsing better' "
            "under Settings > Sync and Google services > Make searches and "
            "browsing better."
          chrome_policy {
            UrlKeyedAnonymizedDataCollectionEnabled {
              policy_options {mode: MANDATORY}
              UrlKeyedAnonymizedDataCollectionEnabled: false
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = optimization_guide_service_url_;
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  // Fill in the appropriate authentication header based on presence of the auth
  // token.
  if (access_token.empty()) {
    PopulateApiKeyRequestHeader(resource_request.get(),
                                optimization_guide_service_api_key_);
  } else {
    PopulateAuthorizationRequestHeader(resource_request.get(), access_token);
  }

  AppendOverrideHeadersIfNeeded(*resource_request);
  active_url_loader_ = variations::CreateSimpleURLLoaderWithVariationsHeader(
      std::move(resource_request),
      // This is always InIncognito::kNo as the OptimizationGuideKeyedService
      // is not enabled on incognito sessions and is rechecked before each
      // fetch.
      variations::InIncognito::kNo, variations::SignedIn::kNo,
      traffic_annotation);

  active_url_loader_->AttachStringForUpload(serialized_request,
                                            "application/x-protobuf");

  UMA_HISTOGRAM_COUNTS_100(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount",
      filtered_hosts.size());
  UMA_HISTOGRAM_COUNTS_100(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount",
      valid_urls.size());

  // Record histogram variants based on request context.
  // Histogram macro doesn't allow dynamic string. Use function.
  base::UmaHistogramCounts100(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount." +
          GetStringNameForRequestContext(request_context_),
      filtered_hosts.size());

  base::UmaHistogramCounts100(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount." +
          GetStringNameForRequestContext(request_context_),
      valid_urls.size());

  // It's safe to use |base::Unretained(this)| here because |this| owns
  // |active_url_loader_| and the callback will be canceled if
  // |active_url_loader_| is destroyed.
  active_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&HintsFetcher::OnURLLoadComplete, base::Unretained(this),
                     skip_cache));

  hints_fetched_callback_ = std::move(hints_fetched_callback);
  hosts_fetched_ = filtered_hosts;
  return true;
}

void HintsFetcher::HandleResponse(const std::string& get_hints_response_data,
                                  int net_status,
                                  int response_code,
                                  bool skip_cache) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  if (response_code >= 0) {
    base::UmaHistogramSparse(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", response_code);
  }
  // Net error codes are negative but histogram enums must be positive.
  base::UmaHistogramSparse(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.NetErrorCode",
      -net_status);

  if (net_status == net::OK && response_code == net::HTTP_OK &&
      get_hints_response->ParseFromString(get_hints_response_data)) {
    UMA_HISTOGRAM_COUNTS_100(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.HintCount",
        get_hints_response->hints_size());
    base::TimeDelta fetch_latency =
        base::TimeTicks::Now() - hints_fetch_start_time_;
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.FetchLatency",
        fetch_latency);
    base::UmaHistogramMediumTimes(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.FetchLatency." +
            GetStringNameForRequestContext(request_context_),
        fetch_latency);
    if (skip_cache) {
      RecordRequestStatusHistogram(request_context_,
                                   FetcherRequestStatus::kSuccess);
      std::move(hints_fetched_callback_).Run(std::move(get_hints_response));

      return;
    }

    // Check cache duration and update.
    base::TimeDelta valid_duration =
        features::StoredFetchedHintsFreshnessDuration();
    if (get_hints_response->has_max_cache_duration()) {
      valid_duration =
          base::Seconds(get_hints_response->max_cache_duration().seconds());
    }
    UpdateHostsSuccessfullyFetched(valid_duration);
    RecordRequestStatusHistogram(request_context_,
                                 FetcherRequestStatus::kSuccess);
    std::move(hints_fetched_callback_).Run(std::move(get_hints_response));
  } else {
    hosts_fetched_.clear();
    RecordRequestStatusHistogram(request_context_,
                                 FetcherRequestStatus::kResponseError);
    std::move(hints_fetched_callback_).Run(std::nullopt);
  }
}

void HintsFetcher::UpdateHostsSuccessfullyFetched(
    base::TimeDelta valid_duration) {
  if (!optimization_guide::features::ShouldPersistHintsToDisk()) {
    // Do not persist any state if we aren't persisting hints to disk.
    return;
  }

  ScopedDictPrefUpdate hosts_fetched_list(
      pref_service_, prefs::kHintsFetcherHostsSuccessfullyFetched);

  // Remove any expired hosts.
  std::vector<std::string> entries_to_remove;
  for (auto it : *hosts_fetched_list) {
    auto seconds = it.second.GetIfDouble();
    if (!seconds || base::Time::FromDeltaSinceWindowsEpoch(
                        base::Seconds(*seconds)) < time_clock_->Now()) {
      entries_to_remove.emplace_back(it.first);
    }
  }
  for (const auto& host : entries_to_remove) {
    hosts_fetched_list->RemoveByDottedPath(host);
  }

  if (hosts_fetched_.empty())
    return;

  // Ensure there is enough space in the dictionary pref for the
  // most recent set of hosts to be stored.
  if (hosts_fetched_list->size() + hosts_fetched_.size() >
      features::MaxHostsForRecordingSuccessfullyCovered()) {
    entries_to_remove.clear();
    size_t num_entries_to_remove =
        hosts_fetched_list->size() + hosts_fetched_.size() -
        features::MaxHostsForRecordingSuccessfullyCovered();
    for (auto it : *hosts_fetched_list) {
      if (entries_to_remove.size() >= num_entries_to_remove)
        break;
      entries_to_remove.emplace_back(it.first);
    }
    for (const auto& host : entries_to_remove) {
      hosts_fetched_list->RemoveByDottedPath(host);
    }
  }

  // Add the covered hosts in |hosts_fetched_| to the dictionary pref.
  base::Time host_invalid_time = time_clock_->Now() + valid_duration;
  for (const std::string& host : hosts_fetched_) {
    hosts_fetched_list->Set(
        HashHostForDictionary(host),
        host_invalid_time.ToDeltaSinceWindowsEpoch().InSecondsF());
  }
  DCHECK_LE(hosts_fetched_list->size(),
            features::MaxHostsForRecordingSuccessfullyCovered());
  hosts_fetched_.clear();
}

// Callback is only invoked if |active_url_loader_| is bound and still alive.
void HintsFetcher::OnURLLoadComplete(
    bool skip_cache,
    std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int response_code = -1;
  if (active_url_loader_->ResponseInfo() &&
      active_url_loader_->ResponseInfo()->headers) {
    response_code =
        active_url_loader_->ResponseInfo()->headers->response_code();
  }
  auto net_error = active_url_loader_->NetError();
  // Reset the active URL loader here since actions happening during response
  // handling may destroy |this|.
  active_url_loader_.reset();

  HandleResponse(response_body ? *response_body : "", net_error, response_code,
                 skip_cache);
}

// Returns the subset of URLs from |urls| for which the URL is considered
// valid and can be included in a hints fetch.
std::vector<GURL> HintsFetcher::GetSizeLimitedURLsForFetching(
    const std::vector<GURL>& urls) const {
  std::vector<GURL> valid_urls;
  for (size_t i = 0; i < urls.size(); i++) {
    if (valid_urls.size() >=
        features::MaxUrlsForOptimizationGuideServiceHintsFetch()) {
      base::UmaHistogramCounts100(
          "OptimizationGuide.HintsFetcher.GetHintsRequest.DroppedUrls." +
              GetStringNameForRequestContext(request_context_),
          urls.size() - i);
      OPTIMIZATION_GUIDE_LOG(
          optimization_guide_common::mojom::LogSource::HINTS,
          optimization_guide_logger_,
          base::StrCat({"Skipped adding URL due to limit, context:",
                        GetStringNameForRequestContext(request_context_),
                        " URL:", urls[i].possibly_invalid_spec()}));
      break;
    }
    if (IsValidURLForURLKeyedHint(urls[i])) {
      valid_urls.push_back(urls[i]);
    } else {
      OPTIMIZATION_GUIDE_LOG(
          optimization_guide_common::mojom::LogSource::HINTS,
          optimization_guide_logger_,
          base::StrCat({"Skipped adding invalid URL, context:",
                        GetStringNameForRequestContext(request_context_),
                        " URL:", urls[i].possibly_invalid_spec()}));
    }
  }
  return valid_urls;
}

std::vector<std::string> HintsFetcher::GetSizeLimitedHostsDueForHintsRefresh(
    const std::vector<std::string>& hosts,
    bool skip_cache) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Value::Dict& hosts_fetched =
      pref_service_->GetDict(prefs::kHintsFetcherHostsSuccessfullyFetched);

  std::vector<std::string> target_hosts;
  target_hosts.reserve(hosts.size());

  for (size_t i = 0; i < hosts.size(); i++) {
    if (target_hosts.size() >=
        features::MaxHostsForOptimizationGuideServiceHintsFetch()) {
      base::UmaHistogramCounts100(
          "OptimizationGuide.HintsFetcher.GetHintsRequest.DroppedHosts." +
              GetStringNameForRequestContext(request_context_),
          hosts.size() - i);
      break;
    }

    std::string host = hosts[i];
    // Skip over localhosts, IP addresses, and invalid hosts.
    if (net::HostStringIsLocalhost(host))
      continue;
    url::CanonHostInfo host_info;
    std::string canonicalized_host(net::CanonicalizeHost(host, &host_info));
    if (host_info.IsIPAddress() ||
        !net::IsCanonicalizedHostCompliant(canonicalized_host)) {
      OPTIMIZATION_GUIDE_LOG(
          optimization_guide_common::mojom::LogSource::HINTS,
          optimization_guide_logger_,
          base::StrCat({"Skipped adding invalid host:", host}));
      continue;
    }
    if (skip_cache) {
      target_hosts.push_back(host);
      continue;
    }

    bool host_hints_due_for_refresh = true;

    std::optional<double> value =
        hosts_fetched.FindDouble(HashHostForDictionary(host));
    if (value && optimization_guide::features::ShouldPersistHintsToDisk()) {
      base::Time host_valid_time =
          base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(*value));
      host_hints_due_for_refresh =
          (host_valid_time - features::GetHostHintsFetchRefreshDuration() <=
           time_clock_->Now());
    }
    if (host_hints_due_for_refresh) {
      target_hosts.push_back(host);
    } else {
      OPTIMIZATION_GUIDE_LOG(
          optimization_guide_common::mojom::LogSource::HINTS,
          optimization_guide_logger_,
          base::StrCat({"Skipped refreshing hints for host:", host}));
    }
  }
  DCHECK_GE(features::MaxHostsForOptimizationGuideServiceHintsFetch(),
            target_hosts.size());
  return target_hosts;
}

}  // namespace optimization_guide
