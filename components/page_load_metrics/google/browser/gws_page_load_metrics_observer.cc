// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/google/browser/gws_page_load_metrics_observer.h"

#include <string>
#include <unordered_map>
#include <vector>

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "base/trace_event/named_trigger.h"
#include "components/crash/core/common/crash_key.h"
#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/page_load_metrics/google/browser/google_url_util.h"
#include "components/page_load_metrics/google/browser/gws_abandoned_page_load_metrics_observer.h"
#include "components/page_load_metrics/google/browser/histogram_suffixes.h"
#include "components/policy/content/policy_blocklist_metrics.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_instance.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

using page_load_metrics::PageAbortReason;

namespace internal {

#define HISTOGRAM_PREFIX "PageLoad.Clients.GoogleSearch."
#define FINEGRAINED_HISTOGRAM_PREFIX \
  "PageLoad.Clients.GoogleSearch.FineGrained."

const char kHistogramGWSNavigationStartToFinalRequestStart[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationStartToFinalRequestStart";
const char kHistogramGWSNavigationStartToFinalResponseStart[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationStartToFinalResponseStart";
const char kHistogramGWSNavigationStartToFinalLoaderCallback[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationStartToFinalLoaderCallback";
const char kHistogramGWSNavigationStartToFirstRequestStart[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationStartToFirstRequestStart";
const char kHistogramGWSNavigationStartToFirstResponseStart[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationStartToFirstResponseStart";
const char kHistogramGWSNavigationStartToFirstLoaderCallback[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationStartToFirstLoaderCallback";
const char kHistogramGWSNavigationStartToOnComplete[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationStartToOnComplete";

const char kHistogramGWSConnectTimingFirstRequestDomainLookupDelay[] =
    HISTOGRAM_PREFIX "ConnectTiming.FirstRequestDomainLookupDelay";
const char kHistogramGWSConnectTimingFirstRequestConnectDelay[] =
    HISTOGRAM_PREFIX "ConnectTiming.FirstRequestConnectDelay";
const char kHistogramGWSConnectTimingFirstRequestSslDelay[] =
    HISTOGRAM_PREFIX "ConnectTiming.FirstRequestSslDelay";
const char kHistogramGWSConnectTimingFinalRequestDomainLookupDelay[] =
    HISTOGRAM_PREFIX "ConnectTiming.FinalRequestDomainLookupDelay";
const char kHistogramGWSConnectTimingFinalRequestConnectDelay[] =
    HISTOGRAM_PREFIX "ConnectTiming.FinalRequestConnectDelay";
const char kHistogramGWSConnectTimingFinalRequestSslDelay[] =
    HISTOGRAM_PREFIX "ConnectTiming.FinalRequestSslDelay";

const char kHistogramGWSAFTEnd[] = HISTOGRAM_PREFIX "PaintTiming.AFTEnd";
const char kHistogramGWSAFTStart[] = HISTOGRAM_PREFIX "PaintTiming.AFTStart";
const char kHistogramGWSHeaderChunkStart[] =
    HISTOGRAM_PREFIX "PaintTiming.HeaderChunkStart";
const char kHistogramGWSHeaderChunkEnd[] =
    HISTOGRAM_PREFIX "PaintTiming.HeaderChunkEnd";
const char kHistogramGWSBodyChunkStart[] =
    HISTOGRAM_PREFIX "PaintTiming.BodyChunkStart";
const char kHistogramGWSBodyChunkEnd[] =
    HISTOGRAM_PREFIX "PaintTiming.BodyChunkEnd";
const char kHistogramGWSFirstContentfulPaint[] =
    HISTOGRAM_PREFIX "PaintTiming.NavigationToFirstContentfulPaint";
const char kHistogramGWSLargestContentfulPaint[] =
    HISTOGRAM_PREFIX "PaintTiming.NavigationToLargestContentfulPaint";
const char kFineGrainedHistogramGWSLargestContentfulPaint[] =
    FINEGRAINED_HISTOGRAM_PREFIX
    "PaintTiming.NavigationToLargestContentfulPaint";
const char kHistogramGWSParseStart[] =
    HISTOGRAM_PREFIX "ParseTiming.NavigationToParseStart";
const char kHistogramGWSConnectStart[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationToConnectStart2";
const char kHistogramGWSDomainLookupStart[] =
    HISTOGRAM_PREFIX "DomainLookupTiming.NavigationToDomainLookupStart2";
const char kHistogramGWSDomainLookupEnd[] =
    HISTOGRAM_PREFIX "DomainLookupTiming.NavigationToDomainLookupEnd2";

const char kHistogramGWSHST[] = HISTOGRAM_PREFIX "CSI.HeadChunkStartTime";
const char kHistogramGWSHCT[] = HISTOGRAM_PREFIX "CSI.HeadChunkContentTime";
const char kHistogramGWSSCT[] = HISTOGRAM_PREFIX "CSI.SearchContentTime";
const char kHistogramGWSSRT[] = HISTOGRAM_PREFIX "CSI.ServerResponseTime";
const char kHistogramGWSTimeBetweenHCTAndSCT[] =
    HISTOGRAM_PREFIX "CSI.TimeBetweenHCTAndSCT";

const char kHistogramGWSNavigationSourceType[] =
    HISTOGRAM_PREFIX "NavigationSourceType";
const char kHistogramGWSNavigationSourceTypeReuse[] =
    HISTOGRAM_PREFIX "NavigationSourceType.ConnectionReuse";
const char kHistogramGWSNavigationSourceTypeDNSReuse[] =
    HISTOGRAM_PREFIX "NavigationSourceType.DNSReuse";
const char kHistogramGWSNavigationSourceTypeNonReuse[] =
    HISTOGRAM_PREFIX "NavigationSourceType.NonConnectionReuse";

const char kHistogramGWSIsFirstNavigationForGWS[] =
    HISTOGRAM_PREFIX "IsFirstNavigationForGWS";

const char kHistogramGWSConnectionReuseStatus[] =
    HISTOGRAM_PREFIX "ConnectionReuseStatus";

const char kHistogramGWSAllHeadersExpected[] =
    HISTOGRAM_PREFIX "SyntheticResponse.AllHeadersExpected";
const char kHistogramGWSHeaderMismatchType[] =
    HISTOGRAM_PREFIX "SyntheticResponse.HeaderMismatchType";

}  // namespace internal

namespace {

constexpr char kSafeSitesFilterEnabledSuffix[] = ".SafeSitesFilterEnabled";
constexpr char kSafeSitesFilterDisabledSuffix[] = ".SafeSitesFilterDisabled";

// TODO(crbug.com/352578800): When this is enabled, the browser will log
// response headers if those're unexpected to be in the navigation response.
BASE_FEATURE(kSyntheticResponseReportUnexpectedHeader,
             "SyntheticResponseReportUnexpectedHeader",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsNavigationFromNewTabPage(
    GWSPageLoadMetricsObserver::NavigationSourceType type) {
  switch (type) {
    case GWSPageLoadMetricsObserver::kFromNewTabPage:
    case GWSPageLoadMetricsObserver::kStartedInBackgroundFromNewTabPage:
      return true;
    case GWSPageLoadMetricsObserver::kFromGWSPage:
    case GWSPageLoadMetricsObserver::kUnknown:
    case GWSPageLoadMetricsObserver::kStartedInBackgroundFromGWSPage:
    case GWSPageLoadMetricsObserver::kStartedInBackground:
      return false;
  }
}

GWSPageLoadMetricsObserver::NavigationSourceType GetBackgroundedState(
    GWSPageLoadMetricsObserver::NavigationSourceType type) {
  switch (type) {
    case GWSPageLoadMetricsObserver::kFromNewTabPage:
      return GWSPageLoadMetricsObserver::kStartedInBackgroundFromNewTabPage;
    case GWSPageLoadMetricsObserver::kFromGWSPage:
      return GWSPageLoadMetricsObserver::kStartedInBackgroundFromGWSPage;
    case GWSPageLoadMetricsObserver::kUnknown:
      return GWSPageLoadMetricsObserver::kStartedInBackground;
    case GWSPageLoadMetricsObserver::kStartedInBackgroundFromGWSPage:
    case GWSPageLoadMetricsObserver::kStartedInBackgroundFromNewTabPage:
    case GWSPageLoadMetricsObserver::kStartedInBackground:
      // Types that already have backgrounded types
      return type;
  }
}

void RecordPageLoadHistogramWithVariants(bool is_safesites_filter_enabled,
                                         std::string_view name,
                                         base::TimeDelta sample) {
  PAGE_LOAD_HISTOGRAM(name, sample);
  PAGE_LOAD_HISTOGRAM(
      base::StrCat({name, is_safesites_filter_enabled
                              ? kSafeSitesFilterEnabledSuffix
                              : kSafeSitesFilterDisabledSuffix}),
      sample);
}

void RecordFineGrainedPageLoadHistogramWithVariants(
    bool is_safesites_filter_enabled,
    std::string_view name,
    base::TimeDelta sample) {
  // Record variant metrics in a range from 10ms to 10s with 100 buckets.
  // Current PAGE_LOAD_HISTOGRAM macro does it from 10ms to 10 minutes with 100
  // buckets, but it would not be suitable to monitor much faster pages living
  // in the real world today, as the bucket size for median value is about 50ms
  // in the current config.
  base::UmaHistogramCustomTimes(name, sample, base::Milliseconds(10),
                                base::Seconds(10), 100);
  base::UmaHistogramCustomTimes(
      base::StrCat({name, is_safesites_filter_enabled
                              ? kSafeSitesFilterEnabledSuffix
                              : kSafeSitesFilterDisabledSuffix}),
      sample, base::Milliseconds(10), base::Seconds(10), 100);
}

struct ExpectedHeaderInfo {
  std::unordered_set<std::string> values;
  bool allow_value_mismatch = false;
  bool found_in_actual_headers = false;
};

std::unordered_map<std::string, ExpectedHeaderInfo> GetExpectedHeaderInfo() {
  std::unordered_map<std::string, ExpectedHeaderInfo> expected_headers;
  expected_headers.emplace(
      "accept-ch",
      ExpectedHeaderInfo(
          {"Sec-CH-Prefers-Color-Scheme", "Sec-CH-UA-Form-Factors",
           "Sec-CH-UA-Platform", "Sec-CH-UA-Platform-Version", "Sec-CH-UA-Arch",
           "Sec-CH-UA-Model", "Sec-CH-UA-Bitness",
           "Sec-CH-UA-Full-Version-List", "Sec-CH-UA-WoW64"},
          true));
  expected_headers.emplace(
      "alt-svc",
      ExpectedHeaderInfo({"h3=\":443\"; ma=2592000,h3-29=\":443\"; ma=2592000"},
                         false));
  expected_headers.emplace("cache-control",
                           ExpectedHeaderInfo({"private, max-age=0"}, false));
  expected_headers.emplace("content-encoding",
                           ExpectedHeaderInfo({"br"}, false));
  // CSP value will be checked via
  // `CheckContentSecurityPolicyHeaderConsistency()`.
  expected_headers.emplace("content-security-policy",
                           ExpectedHeaderInfo({}, true));
  expected_headers.emplace(
      "content-type", ExpectedHeaderInfo({"text/html; charset=UTF-8"}, false));
  expected_headers.emplace(
      "date", ExpectedHeaderInfo({"Wed, 09 Oct 2024 18:56:06 GMT"}, true));
  expected_headers.emplace("expires", ExpectedHeaderInfo({"-1"}, false));
  expected_headers.emplace("permissions-policy",
                           ExpectedHeaderInfo({"unload=()"}, false));
  expected_headers.emplace("server", ExpectedHeaderInfo({"gws"}, false));
  expected_headers.emplace("strict-transport-security",
                           ExpectedHeaderInfo({"max-age=31536000"}, false));
  expected_headers.emplace("x-frame-options",
                           ExpectedHeaderInfo({"SAMEORIGIN"}, false));
  expected_headers.emplace("x-xss-protection",
                           ExpectedHeaderInfo({"0"}, false));
// At the OnCommit() phase, headers in `navigation_handle->GetResponseHeaders()`
// don't have "set-cookie" headers, so it's excluded from the expected header
// list.

// TODO(crbug.com/376572257): Better platform detection aligning with GWS
// response.
#if BUILDFLAG(IS_ANDROID)
#else
  expected_headers.emplace(
      "cross-origin-opener-policy",
      ExpectedHeaderInfo({"same-origin-allow-popups; report-to=\"gws\""},
                         false));
  expected_headers.emplace(
      "report-to",
      ExpectedHeaderInfo(
          {"{\"group\":\"gws\",\"max_age\":2592000,\"endpoints\":[{\"url\":"
           "\"https://csp.withgoogle.com/csp/report-to/gws/cdt1\"}]}"},
          false));
#endif  // BUDILDFLAG(IS_ANDROID)

  return expected_headers;
}

// Check the Content-Security-Policy header is expected, except for the `nonce`.
bool CheckContentSecurityPolicyHeaderConsistency(
    const std::string header_value) {
  const std::string first_half =
      "object-src 'none';base-uri 'self';script-src 'nonce-";
  const std::string second_half =
      "' 'strict-dynamic' 'report-sample' 'unsafe-eval' 'unsafe-inline' https: "
      "http:;report-uri https://csp.withgoogle.com/csp/gws/";
  if (header_value.find(first_half) == std::string::npos) {
    return false;
  }
  if (header_value.find(second_half) == std::string::npos) {
    return false;
  }
  return true;
}

using ArrayItemKey = crash_reporter::CrashKeyString<256>;
ArrayItemKey g_header_not_expected_keys_for_header_name[] = {
    {"GWSHeaderNotExpected-Header-1", ArrayItemKey::Tag::kArray},
    {"GWSHeaderNotExpected-Header-2", ArrayItemKey::Tag::kArray},
    {"GWSHeaderNotExpected-Header-3", ArrayItemKey::Tag::kArray},
    {"GWSHeaderNotExpected-Header-4", ArrayItemKey::Tag::kArray},
    {"GWSHeaderNotExpected-Header-5", ArrayItemKey::Tag::kArray},
};
ArrayItemKey g_header_not_expected_keys_for_value[] = {
    {"GWSHeaderNotExpected-Value-1", ArrayItemKey::Tag::kArray},
    {"GWSHeaderNotExpected-Value-2", ArrayItemKey::Tag::kArray},
    {"GWSHeaderNotExpected-Value-3", ArrayItemKey::Tag::kArray},
    {"GWSHeaderNotExpected-Value-4", ArrayItemKey::Tag::kArray},
    {"GWSHeaderNotExpected-Value-5", ArrayItemKey::Tag::kArray},
};
ArrayItemKey g_header_value_mismatched_keys_for_header_name[] = {
    {"GWSHeaderValueMismatched-Header-1", ArrayItemKey::Tag::kArray},
    {"GWSHeaderValueMismatched-Header-2", ArrayItemKey::Tag::kArray},
    {"GWSHeaderValueMismatched-Header-3", ArrayItemKey::Tag::kArray},
    {"GWSHeaderValueMismatched-Header-4", ArrayItemKey::Tag::kArray},
    {"GWSHeaderValueMismatched-Header-5", ArrayItemKey::Tag::kArray},
};
ArrayItemKey g_header_value_mismatched_keys_for_value[] = {
    {"GWSHeaderValueMismatched-Value-1", ArrayItemKey::Tag::kArray},
    {"GWSHeaderValueMismatched-Value-2", ArrayItemKey::Tag::kArray},
    {"GWSHeaderValueMismatched-Value-3", ArrayItemKey::Tag::kArray},
    {"GWSHeaderValueMismatched-Value-4", ArrayItemKey::Tag::kArray},
    {"GWSHeaderValueMismatched-Value-5", ArrayItemKey::Tag::kArray},
};
ArrayItemKey g_header_not_exist_keys_for_header_name[] = {
    {"GWSHeaderNotActuallyExist-Header-1", ArrayItemKey::Tag::kArray},
    {"GWSHeaderNotActuallyExist-Header-2", ArrayItemKey::Tag::kArray},
    {"GWSHeaderNotActuallyExist-Header-3", ArrayItemKey::Tag::kArray},
    {"GWSHeaderNotActuallyExist-Header-4", ArrayItemKey::Tag::kArray},
    {"GWSHeaderNotActuallyExist-Header-5", ArrayItemKey::Tag::kArray},
};

struct HeaderInfo {
  std::string header_name;
  std::string value;
};

using ReportedHeaders = std::vector<HeaderInfo>;

enum class HeaderMismatchType {
  kHeaderNotExpected = 1 << 0,
  kValueMismatched = 1 << 1,
  kHeaderNotActuallyExist = 1 << 2,
  kMaxValue = kHeaderNotActuallyExist,
};

void SetHeaderCrashKeys(const ReportedHeaders& reported_headers,
                        HeaderMismatchType mismatch_type) {
  auto it = reported_headers.begin();

#define SetCrashKeyForUnexpectedHeader(headers, keys, is_header_name) \
  it = headers.begin();                                               \
  for (ArrayItemKey & key : keys) {                                   \
    if (it == headers.end()) {                                        \
      key.Clear();                                                    \
    } else {                                                          \
      key.Set(is_header_name ? it->header_name : it->value);          \
      ++it;                                                           \
    }                                                                 \
  }

  switch (mismatch_type) {
    case HeaderMismatchType::kHeaderNotExpected:
      SetCrashKeyForUnexpectedHeader(reported_headers,
                                     g_header_not_expected_keys_for_header_name,
                                     /*is_header_name=*/true);
      SetCrashKeyForUnexpectedHeader(reported_headers,
                                     g_header_not_expected_keys_for_value,
                                     /*is_header_name=*/false);
      break;
    case HeaderMismatchType::kValueMismatched:
      SetCrashKeyForUnexpectedHeader(
          reported_headers, g_header_value_mismatched_keys_for_header_name,
          /*is_header_name=*/true);
      SetCrashKeyForUnexpectedHeader(reported_headers,
                                     g_header_value_mismatched_keys_for_value,
                                     /*is_header_name=*/false);
      break;
    case HeaderMismatchType::kHeaderNotActuallyExist:
      SetCrashKeyForUnexpectedHeader(reported_headers,
                                     g_header_not_exist_keys_for_header_name,
                                     /*is_header_name=*/true);
      break;
  }
#undef SetCrashKeyForUnexpectedHeader
}
}  // namespace

GWSPageLoadMetricsObserver::GWSPageLoadMetricsObserver() {
  static bool is_first_navigation = true;
  is_first_navigation_ = is_first_navigation;
  is_first_navigation = false;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  navigation_id_ = navigation_handle->GetNavigationId();
  if (page_load_metrics::IsGoogleSearchResultUrl(navigation_handle->GetURL())) {
    // Emit a trigger to allow trace collection tied to gws navigations.
    base::trace_event::EmitNamedTrigger("gws-navigation-start");
  }

  // Determine the source of the navigation.
  if (IsFromNewTabPage(navigation_handle)) {
    source_type_ = kFromNewTabPage;
  } else if (page_load_metrics::IsGoogleSearchResultUrl(
                 currently_committed_url)) {
    source_type_ = kFromGWSPage;
  }

  // Since `kFromNewTabPage` / `kFromGWSPage` and `kStartedInBackground` may
  // not be mutual exclusive, we also consider the case where both cases may
  // be satisfied (i.e. check if the navigation comes from background and was
  // from NTP/ GWS).
  if (!started_in_foreground) {
    source_type_ = GetBackgroundedState(source_type_);
  }

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  const bool is_gws_url =
      page_load_metrics::IsGoogleSearchResultUrl(navigation_handle->GetURL());
  if (is_first_navigation_) {
    base::UmaHistogramBoolean(internal::kHistogramGWSIsFirstNavigationForGWS,
                              is_gws_url);
  }
  if (!is_gws_url) {
    return STOP_OBSERVING;
  }

  if (const PolicyBlocklistMetrics* const metrics =
      PolicyBlocklistMetrics::Get(*navigation_handle)) {
    is_safesites_filter_enabled_ = true;
    base::UmaHistogramCounts100(
        "Navigation.Throttles.PolicyBlocklist.RedirectCount.GoogleSearch."
        "SafeSitesFilterEnabled",
        metrics->redirect_count);
    base::UmaHistogramTimes(
        "Navigation.Throttles.PolicyBlocklist.RequestToResponseTime2."
        "GoogleSearch.SafeSitesFilterEnabled",
        metrics->request_to_response_time);
    base::UmaHistogramTimes(
        "Navigation.Throttles.PolicyBlocklist.ResponseDeferDuration."
        "GoogleSearch.SafeSitesFilterEnabled",
        metrics->response_defer_duration);
    if (metrics->cache_hit.has_value()) {
      base::UmaHistogramBoolean(
          "Navigation.Throttles.PolicyBlocklist.CacheHit.GoogleSearch."
          "SafeSitesFilterEnabled",
          *metrics->cache_hit);
    }
  }

  navigation_handle_timing_ = navigation_handle->GetNavigationHandleTiming();
  was_cached_ = navigation_handle->WasResponseCached();
  RecordPreCommitHistograms();
  MaybeRecordUnexpectedHeaders(navigation_handle->GetResponseHeaders());

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // TODO(crbug.com/40222513): Handle Prerendering cases.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class is interested only in events that are preprocessed and
  // dispatched also to the outermost page at PageLoadTracker. So, this class
  // doesn't need to forward events for FencedFrames.
  return STOP_OBSERVING;
}

void GWSPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    return;
  }

  RecordPageLoadHistogramWithVariants(
      is_safesites_filter_enabled_, internal::kHistogramGWSFirstContentfulPaint,
      timing.paint_timing->first_contentful_paint.value());
}

void GWSPageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_start, GetDelegate())) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSParseStart,
                      timing.parse_timing->parse_start.value());
}

void GWSPageLoadMetricsObserver::OnConnectStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.connect_start, GetDelegate())) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(AddHistogramSuffix(internal::kHistogramGWSConnectStart),
                      timing.connect_start.value());
}

void GWSPageLoadMetricsObserver::OnDomainLookupStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.domain_lookup_timing->domain_lookup_start, GetDelegate())) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupStart),
      timing.domain_lookup_timing->domain_lookup_start.value());
}

void GWSPageLoadMetricsObserver::OnDomainLookupEnd(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.domain_lookup_timing->domain_lookup_end, GetDelegate())) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupEnd),
      timing.domain_lookup_timing->domain_lookup_end.value());
}

void GWSPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  const base::TimeTicks navigation_start = GetDelegate().GetNavigationStart();
  if (!navigation_start.is_null()) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSNavigationStartToOnComplete,
                        base::TimeTicks::Now() - navigation_start);
  }
  LogMetricsOnComplete();
}

void GWSPageLoadMetricsObserver::OnCustomUserTimingMarkObserved(
    const std::vector<page_load_metrics::mojom::CustomUserTimingMarkPtr>&
        timings) {
  for (const auto& mark : timings) {
    if (mark->mark_name == internal::kGwsAFTStartMarkName) {
      RecordPageLoadHistogramWithVariants(is_safesites_filter_enabled_,
                                          internal::kHistogramGWSAFTStart,
                                          mark->start_time);
      aft_start_time_ = mark->start_time;
    } else if (mark->mark_name == internal::kGwsAFTEndMarkName) {
      RecordPageLoadHistogramWithVariants(is_safesites_filter_enabled_,
                                          internal::kHistogramGWSAFTEnd,
                                          mark->start_time);
      aft_end_time_ = mark->start_time;
    } else if (mark->mark_name == internal::kGwsHeaderChunkStartMarkName) {
      RecordPageLoadHistogramWithVariants(
          is_safesites_filter_enabled_, internal::kHistogramGWSHeaderChunkStart,
          mark->start_time);
      header_chunk_start_time_ = mark->start_time;
    } else if (mark->mark_name == internal::kGwsHeaderChunkEndMarkName) {
      RecordPageLoadHistogramWithVariants(is_safesites_filter_enabled_,
                                          internal::kHistogramGWSHeaderChunkEnd,
                                          mark->start_time);
      header_chunk_end_time_ = mark->start_time;
    } else if (mark->mark_name == internal::kGwsBodyChunkStartMarkName) {
      RecordPageLoadHistogramWithVariants(is_safesites_filter_enabled_,
                                          internal::kHistogramGWSBodyChunkStart,
                                          mark->start_time);
      body_chunk_start_time_ = mark->start_time;
    } else if (mark->mark_name == internal::kGwsBodyChunkEndMarkName) {
      RecordPageLoadHistogramWithVariants(is_safesites_filter_enabled_,
                                          internal::kHistogramGWSBodyChunkEnd,
                                          mark->start_time);
    }
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  LogMetricsOnComplete();
  return STOP_OBSERVING;
}

void GWSPageLoadMetricsObserver::LogMetricsOnComplete() {
  const page_load_metrics::ContentfulPaintTimingInfo&
      all_frames_largest_contentful_paint =
          GetDelegate()
              .GetLargestContentfulPaintHandler()
              .MergeMainFrameAndSubframes();
  if (!all_frames_largest_contentful_paint.ContainsValidTime() ||
      !WasStartedInForegroundOptionalEventInForeground(
          all_frames_largest_contentful_paint.Time(), GetDelegate())) {
    return;
  }
  RecordNavigationTimingHistograms();
  RecordPageLoadHistogramWithVariants(
      is_safesites_filter_enabled_,
      internal::kHistogramGWSLargestContentfulPaint,
      all_frames_largest_contentful_paint.Time().value());
  RecordFineGrainedPageLoadHistogramWithVariants(
      is_safesites_filter_enabled_,
      internal::kFineGrainedHistogramGWSLargestContentfulPaint,
      all_frames_largest_contentful_paint.Time().value());
}

void GWSPageLoadMetricsObserver::RecordNavigationTimingHistograms() {
  const base::TimeTicks navigation_start_time =
      GetDelegate().GetNavigationStart();
  const content::NavigationHandleTiming& timing = navigation_handle_timing_;

  // Record metrics for navigation only when all relevant milestones are
  // recorded and in the expected order. It is allowed that they have the same
  // value for some cases (e.g., internal redirection for HSTS).
  if (navigation_start_time.is_null() ||
      timing.first_request_start_time.is_null() ||
      timing.first_response_start_time.is_null() ||
      timing.first_loader_callback_time.is_null() ||
      timing.final_request_start_time.is_null() ||
      timing.final_response_start_time.is_null() ||
      timing.final_loader_callback_time.is_null() ||
      timing.navigation_commit_sent_time.is_null()) {
    return;
  }

  // Record the elapsed time from the navigation start milestone.
  PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSNavigationStartToFirstRequestStart,
                      timing.first_request_start_time - navigation_start_time);
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramGWSNavigationStartToFirstResponseStart,
      timing.first_response_start_time - navigation_start_time);
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramGWSNavigationStartToFirstLoaderCallback,
      timing.first_loader_callback_time - navigation_start_time);
  PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSNavigationStartToFinalRequestStart,
                      timing.final_request_start_time - navigation_start_time);
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramGWSNavigationStartToFinalResponseStart,
      timing.final_response_start_time - navigation_start_time);
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramGWSNavigationStartToFinalLoaderCallback,
      timing.final_loader_callback_time - navigation_start_time);

  PAGE_LOAD_SHORT_HISTOGRAM(
      internal::kHistogramGWSConnectTimingFirstRequestDomainLookupDelay,
      timing.first_request_domain_lookup_delay);
  PAGE_LOAD_SHORT_HISTOGRAM(
      internal::kHistogramGWSConnectTimingFirstRequestConnectDelay,
      timing.first_request_connect_delay);
  PAGE_LOAD_SHORT_HISTOGRAM(
      internal::kHistogramGWSConnectTimingFirstRequestSslDelay,
      timing.first_request_ssl_delay);
  PAGE_LOAD_SHORT_HISTOGRAM(
      internal::kHistogramGWSConnectTimingFinalRequestDomainLookupDelay,
      timing.final_request_domain_lookup_delay);
  PAGE_LOAD_SHORT_HISTOGRAM(
      internal::kHistogramGWSConnectTimingFinalRequestConnectDelay,
      timing.final_request_connect_delay);
  PAGE_LOAD_SHORT_HISTOGRAM(
      internal::kHistogramGWSConnectTimingFinalRequestSslDelay,
      timing.final_request_ssl_delay);

  // Record latency trace events.
  RecordLatencyHitograms(timing.non_redirect_response_start_time);

  // Record trace events according to the navigation milestone.
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "loading", "GWSNavigationStartToFirstRequestStart", TRACE_ID_LOCAL(this),
      navigation_start_time);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "loading", "GWSNavigationStartToFirstRequestStart", TRACE_ID_LOCAL(this),
      timing.first_request_start_time);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "loading", "GWSFirstRequestStartToFirstResponseStart",
      TRACE_ID_LOCAL(this), timing.first_request_start_time);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "loading", "GWSFirstRequestStartToFirstResponseStart",
      TRACE_ID_LOCAL(this), timing.first_response_start_time);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "loading", "GWSFirstResponseStartToFirstLoaderCallback",
      TRACE_ID_LOCAL(this), timing.first_response_start_time);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "loading", "GWSFirstResponseStartToFirstLoaderCallback",
      TRACE_ID_LOCAL(this), timing.first_loader_callback_time);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "loading", "GWSFirstLoadCallbackToFinalResponseStart",
      TRACE_ID_LOCAL(this), timing.first_loader_callback_time);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "loading", "GWSFirstLoadCallbackToFinalResponseStart",
      TRACE_ID_LOCAL(this), timing.final_response_start_time);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "loading", "GWSFinalResponseStartToFinalLoaderCallback",
      TRACE_ID_LOCAL(this), timing.final_response_start_time);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "loading", "GWSFinalResponseStartToFinalLoaderCallback",
      TRACE_ID_LOCAL(this), timing.final_loader_callback_time);
}

void GWSPageLoadMetricsObserver::RecordPreCommitHistograms() {
  base::UmaHistogramEnumeration(internal::kHistogramGWSNavigationSourceType,
                                source_type_);
  if (!was_cached_) {
    RecordConnectionReuseHistograms();
  }
}

void GWSPageLoadMetricsObserver::RecordConnectionReuseHistograms() {
  DCHECK(!was_cached_);

  const content::NavigationHandleTiming& timing = navigation_handle_timing_;
  ConnectionReuseStatus status = ConnectionReuseStatus::kNonReuse;
  // If domain lookup duration is zero and connect duration is also zero,
  // this is most-likely a connection reuse.
  if (timing.first_request_domain_lookup_delay.is_zero()) {
    status = ConnectionReuseStatus::kDNSReused;
    if (timing.first_request_connect_delay.is_zero()) {
      status = ConnectionReuseStatus::kReused;
    }
  }
  base::UmaHistogramEnumeration(internal::kHistogramGWSConnectionReuseStatus,
                                status);

  switch (status) {
    case ConnectionReuseStatus::kNonReuse:
      base::UmaHistogramEnumeration(
          internal::kHistogramGWSNavigationSourceTypeNonReuse, source_type_);
      break;
    case ConnectionReuseStatus::kDNSReused:
      base::UmaHistogramEnumeration(
          internal::kHistogramGWSNavigationSourceTypeDNSReuse, source_type_);
      break;
    case ConnectionReuseStatus::kReused:
      base::UmaHistogramEnumeration(
          internal::kHistogramGWSNavigationSourceTypeReuse, source_type_);
      break;
  }
}

std::string GWSPageLoadMetricsObserver::AddHistogramSuffix(
    const std::string& histogram_name) {
  std::string suffix =
      (is_first_navigation_ ? internal::kSuffixFirstNavigation
                            : internal::kSuffixSubsequentNavigation);
  if (!IsBrowserStartupComplete()) {
    suffix += internal::kSuffixIsBrowserStarting;
  }

  if (IsNavigationFromNewTabPage(source_type_)) {
    suffix += internal::kSuffixFromNewTabPage;
  }

  return histogram_name + suffix;
}

void GWSPageLoadMetricsObserver::RecordLatencyHitograms(
    base::TimeTicks response_start_time) {
  const auto trace_id =
      TRACE_ID_WITH_SCOPE("GWSLatencyEvent", TRACE_ID_LOCAL(navigation_id_));
  // TODO(crbug.com/364278026): SRT starts from the time when the user submits
  // a query. Using the navigation start time may not perfect to measure SRT.
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "navigation", "GWSLatency:SRT", trace_id,
      GetDelegate().GetNavigationStart());
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0("navigation", "GWSLatency:SRT",
                                                 trace_id, response_start_time);
  PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSSRT,
                      response_start_time - GetDelegate().GetNavigationStart());

  // Log some important CSI metrics only when related submetrics are recorded.
  std::optional<base::TimeDelta> hct_time;
  std::optional<base::TimeDelta> sct_time;

  if (aft_end_time_.has_value()) {
    // Currently `aft_start_time_` has the value of the server response time,
    // but in theory AFT starts at the end of SRT, the time when the client
    // receives the first byte of the header chunk.
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
        "navigation", "GWSLatency:AFT", trace_id, response_start_time);
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        "navigation", "GWSLatency:AFT", trace_id,
        GetDelegate().GetNavigationStart() + aft_end_time_.value());
  }
  if (body_chunk_start_time_.has_value()) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
        "navigation", "GWSLatency:SCT", trace_id, response_start_time);
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        "navigation", "GWSLatency:SCT", trace_id,
        GetDelegate().GetNavigationStart() + body_chunk_start_time_.value());
    sct_time = GetDelegate().GetNavigationStart() +
               body_chunk_start_time_.value() - response_start_time;
    PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSSCT, sct_time.value());
  }
  if (header_chunk_end_time_.has_value()) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
        "navigation", "GWSLatency:HCT", trace_id, response_start_time);
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        "navigation", "GWSLatency:HCT", trace_id,
        GetDelegate().GetNavigationStart() + header_chunk_end_time_.value());
    hct_time = GetDelegate().GetNavigationStart() +
               header_chunk_end_time_.value() - response_start_time;
    PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSHCT, hct_time.value());
  }
  if (header_chunk_start_time_.has_value()) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
        "navigation", "GWSLatency:HST", trace_id, response_start_time);
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        "navigation", "GWSLatency:HST", trace_id,
        GetDelegate().GetNavigationStart() + header_chunk_start_time_.value());
    PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSHST,
                        GetDelegate().GetNavigationStart() +
                            header_chunk_start_time_.value() -
                            response_start_time);
  }
  if (sct_time.has_value() && hct_time.has_value()) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSTimeBetweenHCTAndSCT,
                        sct_time.value() - hct_time.value());
  }
}

void GWSPageLoadMetricsObserver::MaybeRecordUnexpectedHeaders(
    const net::HttpResponseHeaders* response_headers) {
  ReportedHeaders not_expected_headers;
  ReportedHeaders value_mismatched_headers;
  ReportedHeaders not_exist_headers;

  std::unordered_map<std::string, ExpectedHeaderInfo> expected_headers =
      GetExpectedHeaderInfo();

  size_t iter = 0;
  std::string name, value;
  while (response_headers->EnumerateHeaderLines(&iter, &name, &value)) {
    if (!expected_headers.contains(name)) {
      // GWSHeaderNotExpected: The header is not in the expected header list.
      not_expected_headers.emplace_back(name, value);
      continue;
    }
    if (name == "content-security-policy") {
      // Check content-security-policy separately. The CSP value should be
      // consistent except for the `nonce` value.
      if (!CheckContentSecurityPolicyHeaderConsistency(value)) {
        value_mismatched_headers.emplace_back(name, value);
      }
    }
    auto* expected = &expected_headers[name];
    expected->found_in_actual_headers = true;
    if (!expected->allow_value_mismatch && !expected->values.contains(value)) {
      // GWSHeaderValueMismatched: The header is in the expected header list,
      // but the value is different or an inconsistent value is not allowed.
      value_mismatched_headers.emplace_back(name, value);
    }
  }

  for (auto header : expected_headers) {
    if (header.second.found_in_actual_headers) {
      continue;
    }
    // GWSHeaderNotActuallyExist: The expected header does not exist in the
    // actual headers.
    not_exist_headers.emplace_back(header.first, "");
  }

  bool all_headers_expected = not_expected_headers.empty() &&
                              value_mismatched_headers.empty() &&
                              not_exist_headers.empty();
  bool set_crash_key =
      !all_headers_expected &&
      base::FeatureList::IsEnabled(kSyntheticResponseReportUnexpectedHeader);

  // Potential hit rate of the synthetic response.
  base::UmaHistogramBoolean(internal::kHistogramGWSAllHeadersExpected,
                            all_headers_expected);

  size_t mismatch_type = 0;
  if (!not_expected_headers.empty()) {
    mismatch_type |= static_cast<int>(HeaderMismatchType::kHeaderNotExpected);
  }
  if (!value_mismatched_headers.empty()) {
    mismatch_type |= static_cast<int>(HeaderMismatchType::kValueMismatched);
  }
  if (!not_exist_headers.empty()) {
    mismatch_type |=
        static_cast<int>(HeaderMismatchType::kHeaderNotActuallyExist);
  }
  UMA_HISTOGRAM_COUNTS_100(internal::kHistogramGWSHeaderMismatchType,
                           mismatch_type);

  if (set_crash_key) {
    if (!not_expected_headers.empty()) {
      SetHeaderCrashKeys(not_expected_headers,
                         HeaderMismatchType::kHeaderNotExpected);
    }
    if (!value_mismatched_headers.empty()) {
      SetHeaderCrashKeys(value_mismatched_headers,
                         HeaderMismatchType::kValueMismatched);
    }
    if (!not_exist_headers.empty()) {
      SetHeaderCrashKeys(not_exist_headers,
                         HeaderMismatchType::kHeaderNotActuallyExist);
    }
    base::debug::DumpWithoutCrashing();
  }
}
