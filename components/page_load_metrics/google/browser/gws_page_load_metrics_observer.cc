// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/google/browser/gws_page_load_metrics_observer.h"

#include <string>
#include <unordered_map>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/trace_event/named_trigger.h"
#include "base/trace_event/trace_event.h"
#include "components/crash/core/common/crash_key.h"
#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/page_load_metrics/google/browser/google_url_util.h"
#include "components/page_load_metrics/google/browser/gws_abandoned_page_load_metrics_observer.h"
#include "components/page_load_metrics/google/browser/gws_session_state.h"
#include "components/page_load_metrics/google/browser/histogram_suffixes.h"
#include "components/page_load_metrics/google/browser/prerender_prewarm_navigation_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_connection_info.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

using page_load_metrics::PageAbortReason;

namespace internal {

#define HISTOGRAM_PREFIX "PageLoad.Clients.GoogleSearch."

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
const char kHistogramGWSFirstFetchStartToFirstRequestStart[] =
    HISTOGRAM_PREFIX "NavigationTiming.FirstFetchStartToFirstRequestStart";
const char kHistogramGWSCreateStreamDelay[] =
    HISTOGRAM_PREFIX "NavigationTiming.CreateStreamDelay2";
const char kHistogramGWSConnectedCallbackDelay[] =
    HISTOGRAM_PREFIX "NavigationTiming.ConnectedCallbackDelay2";
const char kHistogramGWSInitializeStreamDelay[] =
    HISTOGRAM_PREFIX "NavigationTiming.InitializeStreamDelay";

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

const char kHistogramGWSAFTEnd[] = HISTOGRAM_PREFIX "PaintTiming.AFTEnd2";
const char kHistogramGWSAFTEndWithPreNavigationLatency[] =
    HISTOGRAM_PREFIX "PaintTiming.AFTEndWithPreNavigationLatency";
const char kHistogramGWSAFTStart[] = HISTOGRAM_PREFIX "PaintTiming.AFTStart2";
const char kHistogramGWSHeadChunkStart[] =
    HISTOGRAM_PREFIX "PaintTiming.HeadChunkStart";
const char kHistogramGWSHeadChunkEnd[] =
    HISTOGRAM_PREFIX "PaintTiming.HeadChunkEnd";
const char kHistogramGWSBodyChunkStart[] =
    HISTOGRAM_PREFIX "PaintTiming.BodyChunkStart2";
const char kHistogramGWSBodyChunkEnd[] =
    HISTOGRAM_PREFIX "PaintTiming.BodyChunkEnd2";
const char kHistogramGWSFirstContentfulPaint[] =
    HISTOGRAM_PREFIX "PaintTiming.NavigationToFirstContentfulPaint";
const char kHistogramGWSLargestContentfulPaint[] =
    HISTOGRAM_PREFIX "PaintTiming.NavigationToLargestContentfulPaint";
const char kHistogramGWSParseStart[] =
    HISTOGRAM_PREFIX "ParseTiming.NavigationToParseStart";
const char kHistogramGWSConnectStart[] =
    HISTOGRAM_PREFIX "NavigationTiming.NavigationToConnectStart2";
const char kHistogramGWSDomainLookupStart[] =
    HISTOGRAM_PREFIX "DomainLookupTiming.NavigationToDomainLookupStart2";
const char kHistogramGWSDomainLookupEnd[] =
    HISTOGRAM_PREFIX "DomainLookupTiming.NavigationToDomainLookupEnd2";

const char kHistogramGWSHST[] = HISTOGRAM_PREFIX "CSI.HST";
const char kHistogramGWSHCT[] = HISTOGRAM_PREFIX "CSI.HCT";
const char kHistogramGWSSCT[] = HISTOGRAM_PREFIX "CSI.SCT";
const char kHistogramGWSSRT[] = HISTOGRAM_PREFIX "CSI.SRT";
const char kHistogramGWSSGL[] = HISTOGRAM_PREFIX "CSI.SGL";
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
const char kHistogramIncognitoSuffix[] = ".Incognito";
const char kHistogramSyntheticResponseSuffix[] = ".SyntheticResponse";

const char kHistogramGWSSessionSource[] = HISTOGRAM_PREFIX "SessionSource";
const char kHistogramGWSAdvertisedAltSvcState[] =
    HISTOGRAM_PREFIX "AdvertisedAltSvcState";
const char kHistogramGWSHttpNetworkSessionQuicEnabled[] =
    HISTOGRAM_PREFIX "HttpNetworkSessionQuicEnabled";

// Prerender related histograms.
const char kHistogramPrerenderHostReused[] =
    HISTOGRAM_PREFIX "Prerender.HostReused";
const char kHistogramPrerenderPrewarmNavigationStatus[] =
    HISTOGRAM_PREFIX "Prerender.PrewarmNavigationStatus";
const char kHistogramGWSPrerenderNavigationToActivation[] =
    HISTOGRAM_PREFIX "Prerender.NavigationToActivation";
const char kHistogramGWSActivationToFirstContentfulPaint[] =
    HISTOGRAM_PREFIX "Prerender.ActivationToFirstContentfulPaint";
const char kHistogramGWSActivationToLargestContentfulPaint[] =
    HISTOGRAM_PREFIX "Prerender.ActivationToLargestContentfulPaint";

const char kHistogramGWSWarmUpType[] = HISTOGRAM_PREFIX "WarmUpType";

const char kHistogramPrerenderSuffix[] = ".Prerender";
const char kHistogramNonPrerenderSuffix[] = ".NonPrerender";
// ServiceWorker related histograms.
const char kHistogramServiceWorkerParseStartSearch[] =
    "PageLoad.Clients.ServiceWorker2.ParseTiming.NavigationToParseStart.search";
const char kHistogramServiceWorkerFirstContentfulPaintSearch[] =
    "PageLoad.Clients.ServiceWorker2.PaintTiming."
    "NavigationToFirstContentfulPaint.search";
const char kHistogramServiceWorkerParseStartToFirstContentfulPaintSearch[] =
    "PageLoad.Clients.ServiceWorker2.PaintTiming."
    "ParseStartToFirstContentfulPaint.search";
const char kHistogramServiceWorkerDomContentLoadedSearch[] =
    "PageLoad.Clients.ServiceWorker2.DocumentTiming."
    "NavigationToDOMContentLoadedEventFired.search";
const char kHistogramServiceWorkerLoadSearch[] =
    "PageLoad.Clients.ServiceWorker2.DocumentTiming.NavigationToLoadEventFired."
    "search";
const char kHistogramNoServiceWorkerFirstContentfulPaintSearch[] =
    "PageLoad.Clients.NoServiceWorker2.PaintTiming."
    "NavigationToFirstContentfulPaint.search";
const char kHistogramNoServiceWorkerParseStartToFirstContentfulPaintSearch[] =
    "PageLoad.Clients.NoServiceWorker2.PaintTiming."
    "ParseStartToFirstContentfulPaint.search";
const char kHistogramNoServiceWorkerDomContentLoadedSearch[] =
    "PageLoad.Clients.NoServiceWorker2.DocumentTiming."
    "NavigationToDOMContentLoadedEventFired.search";
const char kHistogramNoServiceWorkerLoadSearch[] =
    "PageLoad.Clients.NoServiceWorker2.DocumentTiming."
    "NavigationToLoadEventFired.search";
}  // namespace internal

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(WarmUpType)
enum class WarmUpType {
  kRegularSignedIn = 0,
  kRegularPrewarmed = 1,
  kReegularCold = 2,
  kOffTheRecordSignedIn = 3,
  kOffTheRecordPrewarmed = 4,
  kOffTheRecordColdButRegularSignedIn = 5,
  kOffTheRecordColdButRegularPrewarmed = 6,
  kOffTheRecordAndRegularCold = 7,
  kMaxValue = kOffTheRecordAndRegularCold,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/page/enums.xml:GwsWarmUpType)

WarmUpType ClassifyIntoWarmUpType(content::BrowserContext* current_context,
                                  content::BrowserContext* original_context) {
  CHECK(current_context);
  page_load_metrics::GWSSessionState* current_session_state =
      page_load_metrics::GWSSessionState::GetOrCreateForBrowserContext(
          current_context);
  if (!original_context) {
    if (current_session_state->IsSignedIn()) {
      return WarmUpType::kRegularSignedIn;
    } else if (current_session_state->IsPrewarmed()) {
      return WarmUpType::kRegularPrewarmed;
    } else {
      return WarmUpType::kReegularCold;
    }
  } else {
    if (current_session_state->IsSignedIn()) {
      return WarmUpType::kOffTheRecordSignedIn;
    } else if (current_session_state->IsPrewarmed()) {
      return WarmUpType::kOffTheRecordPrewarmed;
    } else {
      page_load_metrics::GWSSessionState* original_session_state =
          page_load_metrics::GWSSessionState::GetOrCreateForBrowserContext(
              original_context);
      if (original_session_state->IsSignedIn()) {
        return WarmUpType::kOffTheRecordColdButRegularSignedIn;
      } else if (original_session_state->IsPrewarmed()) {
        return WarmUpType::kOffTheRecordColdButRegularPrewarmed;
      } else {
        return WarmUpType::kOffTheRecordAndRegularCold;
      }
    }
  }
}

BASE_FEATURE(kRecordPrenavigationLatency, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsPrenavigationLatencyEnabled() {
  return base::FeatureList::IsEnabled(kRecordPrenavigationLatency);
}

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

std::string GetProtocolSuffix(
    const net::HttpConnectionInfoCoarse http_connection_info) {
  return base::StrCat(
      {".", net::HttpConnectionInfoCoarseToString(http_connection_info)});
}

}  // namespace

GWSPageLoadMetricsObserver::GWSPageLoadMetricsObserver() {
  static bool is_first_navigation = true;
  is_first_navigation_ = is_first_navigation;
  is_first_navigation = false;
}

GWSPageLoadMetricsObserver::~GWSPageLoadMetricsObserver() = default;

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
  if (!is_prerendered_ && is_first_navigation_) {
    base::UmaHistogramBoolean(internal::kHistogramGWSIsFirstNavigationForGWS,
                              is_gws_url);
  }
  if (!is_gws_url) {
    return STOP_OBSERVING;
  }
  navigation_handle_timing_ = navigation_handle->GetNavigationHandleTiming();
  was_cached_ = navigation_handle->WasResponseCached();
  network_accessed_ = navigation_handle->NetworkAccessed();
  http_connection_info_ =
      net::HttpConnectionInfoToCoarse(navigation_handle->GetConnectionInfo());
  if (!is_prerendered_) {
    RecordPreCommitHistograms();
  }

  // Record the prerender prewarm navigation status for the navigation here.
  // This is because once the navigation is activated, the `NavigationHandle`
  // will change.
  if (auto* prerender_prewarm_navigation_data =
          page_load_metrics::PrerenderPrewarmNavigationData::
              GetForNavigationHandle(*navigation_handle)) {
    prerender_prewarm_navigation_status_ =
        prerender_prewarm_navigation_data->GetNavigationStatus();
  }

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  is_prerendered_ = true;
  // TODO(crbug.com/40222513): Currently, we do not record most metrics for
  // prerendered pages. Consider and enable metrics for prerender as well.
  return CONTINUE_OBSERVING;
}

void GWSPageLoadMetricsObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  CHECK(is_prerendered_);
  // We record the prerender host reuse status.
  base::UmaHistogramBoolean(internal::kHistogramPrerenderHostReused,
                            navigation_handle->IsPrerenderHostReused());
  if (IsIncognitoProfile()) {
    auto histogram_name = base::StrCat({internal::kHistogramPrerenderHostReused,
                                        internal::kHistogramIncognitoSuffix});
    base::UmaHistogramBoolean(histogram_name,
                              navigation_handle->IsPrerenderHostReused());
  }

  // |navigation_handle| here is for the activation navigation, while
  // |GetDelegate().GetNavigationStart()| is the start time of initial prerender
  // navigation.
  auto navigation_to_activation_time =
      navigation_handle->NavigationStart() - GetDelegate().GetNavigationStart();
  base::UmaHistogramCustomTimes(
      internal::kHistogramGWSPrerenderNavigationToActivation,
      navigation_to_activation_time, base::Milliseconds(10), base::Minutes(10),
      100);
  if (IsIncognitoProfile()) {
    auto histogram_name =
        base::StrCat({internal::kHistogramGWSPrerenderNavigationToActivation,
                      internal::kHistogramIncognitoSuffix});
    base::UmaHistogramCustomTimes(histogram_name, navigation_to_activation_time,
                                  base::Milliseconds(10), base::Minutes(10),
                                  100);
  }

  if (prerender_prewarm_navigation_status_.has_value()) {
    base::UmaHistogramEnumeration(
        internal::kHistogramPrerenderPrewarmNavigationStatus,
        prerender_prewarm_navigation_status_.value());
  }
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
  if (WasActivatedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_paint, GetDelegate())) {
    CHECK(is_prerendered_);
    base::TimeDelta activation_to_fcp =
        page_load_metrics::CorrectEventAsNavigationOrActivationOrigined(
            GetDelegate(), timing.paint_timing->first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSActivationToFirstContentfulPaint,
                        activation_to_fcp);
    if (IsIncognitoProfile()) {
      auto histogram_name =
          base::StrCat({internal::kHistogramGWSActivationToFirstContentfulPaint,
                        internal::kHistogramIncognitoSuffix});
      PAGE_LOAD_HISTOGRAM(histogram_name, activation_to_fcp);
    }
    return;
  }

  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    return;
  }
  CHECK(!is_prerendered_);
  if (page_load_metrics::IsServiceWorkerControlled(GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramServiceWorkerFirstContentfulPaintSearch,
        timing.paint_timing->first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramServiceWorkerParseStartToFirstContentfulPaintSearch,
        timing.paint_timing->first_contentful_paint.value() -
            timing.parse_timing->parse_start.value());
  } else {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramNoServiceWorkerFirstContentfulPaintSearch,
        timing.paint_timing->first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(
        internal::
            kHistogramNoServiceWorkerParseStartToFirstContentfulPaintSearch,
        timing.paint_timing->first_contentful_paint.value() -
            timing.parse_timing->parse_start.value());
  }
  PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSFirstContentfulPaint,
                      timing.paint_timing->first_contentful_paint.value());
  if (is_header_from_synthetic_response_) {
    PAGE_LOAD_HISTOGRAM(
        base::StrCat({internal::kHistogramGWSFirstContentfulPaint,
                      internal::kHistogramSyntheticResponseSuffix}),
        timing.paint_timing->first_contentful_paint.value());
  }
}

void GWSPageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->dom_content_loaded_event_start,
          GetDelegate())) {
    return;
  }

  if (page_load_metrics::IsServiceWorkerControlled(GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramServiceWorkerDomContentLoadedSearch,
        timing.document_timing->dom_content_loaded_event_start.value());
  } else {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramNoServiceWorkerDomContentLoadedSearch,
        timing.document_timing->dom_content_loaded_event_start.value());
  }
}

void GWSPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->load_event_start, GetDelegate())) {
    return;
  }

  if (page_load_metrics::IsServiceWorkerControlled(GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramServiceWorkerLoadSearch,
                        timing.document_timing->load_event_start.value());
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramNoServiceWorkerLoadSearch,
                        timing.document_timing->load_event_start.value());
  }
}

void GWSPageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_start, GetDelegate())) {
    return;
  }
  CHECK(!is_prerendered_);
  PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSParseStart,
                      timing.parse_timing->parse_start.value());
  if (page_load_metrics::IsServiceWorkerControlled(GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramServiceWorkerParseStartSearch,
                        timing.parse_timing->parse_start.value());
  }
  if (page_load_metrics::IsServiceWorkerSyntheticResponseEnabled(
          GetDelegate())) {
    is_header_from_synthetic_response_ = true;
    PAGE_LOAD_HISTOGRAM(
        base::StrCat({internal::kHistogramGWSParseStart,
                      internal::kHistogramSyntheticResponseSuffix}),
        timing.parse_timing->parse_start.value());
  }
}

void GWSPageLoadMetricsObserver::OnConnectStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.connect_start, GetDelegate())) {
    return;
  }
  CHECK(!is_prerendered_);
  PAGE_LOAD_HISTOGRAM(AddHistogramSuffix(internal::kHistogramGWSConnectStart),
                      timing.connect_start.value());
}

void GWSPageLoadMetricsObserver::OnDomainLookupStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.domain_lookup_timing->domain_lookup_start, GetDelegate())) {
    return;
  }
  CHECK(!is_prerendered_);
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
  CHECK(!is_prerendered_);
  PAGE_LOAD_HISTOGRAM(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupEnd),
      timing.domain_lookup_timing->domain_lookup_end.value());
}

void GWSPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!is_prerendered_) {
    const base::TimeTicks navigation_start = GetDelegate().GetNavigationStart();
    if (!navigation_start.is_null()) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSNavigationStartToOnComplete,
                          base::TimeTicks::Now() - navigation_start);
    }
  }
  LogMetricsOnComplete();
}

void GWSPageLoadMetricsObserver::OnCustomUserTimingMarkObserved(
    const std::vector<page_load_metrics::mojom::CustomUserTimingMarkPtr>&
        timings) {
  auto record_histogram = [this](const std::string& histogram_name,
                                 const base::TimeDelta& timing) {
    auto histogram_with_suffix = base::StrCat(
        {histogram_name,
         is_prerendered_ ? internal::kHistogramPrerenderSuffix
                         : internal::kHistogramNonPrerenderSuffix,
         IsIncognitoProfile() ? internal::kHistogramIncognitoSuffix : "",
         is_header_from_synthetic_response_
             ? internal::kHistogramSyntheticResponseSuffix
             : ""});
    PAGE_LOAD_HISTOGRAM(histogram_name, timing);
    PAGE_LOAD_HISTOGRAM(histogram_with_suffix, timing);
  };
  for (const auto& mark : timings) {
    // TODO(crbug.com/436345871): Update the logic to align with the server
    // behavior.
    auto timing =
        is_prerendered_
            ? page_load_metrics::CorrectEventAsNavigationOrActivationOrigined(
                  GetDelegate(), mark->start_time)
            : mark->start_time;
    if (mark->mark_name == internal::kGwsAFTStartMarkName) {
      record_histogram(internal::kHistogramGWSAFTStart, timing);
      aft_start_time_ = mark->start_time;
    } else if (mark->mark_name == internal::kGwsAFTEndMarkName) {
      record_histogram(internal::kHistogramGWSAFTEnd, timing);
      aft_end_time_ = mark->start_time;
    } else if (mark->mark_name == internal::kGwsHeadChunkStartMarkName) {
      record_histogram(internal::kHistogramGWSHeadChunkStart, timing);
      head_chunk_start_time_ = mark->start_time;
    } else if (mark->mark_name == internal::kGwsHeadChunkEndMarkName) {
      record_histogram(internal::kHistogramGWSHeadChunkEnd, timing);
      head_chunk_end_time_ = mark->start_time;
    } else if (mark->mark_name == internal::kGwsBodyChunkStartMarkName) {
      record_histogram(internal::kHistogramGWSBodyChunkStart, timing);
      body_chunk_start_time_ = mark->start_time;
    } else if (mark->mark_name == internal::kGwsBodyChunkEndMarkName) {
      record_histogram(internal::kHistogramGWSBodyChunkEnd, timing);
    } else if (IsPrenavigationLatencyEnabled() &&
               mark->mark_name == internal::kGwsSGLMarkName) {
      // Because this is a performance mark for previous navigation, we should
      // not correct the timing for prerender activation, or else we would get
      // inconsistent timing.
      // So, we use `mark->start_time` directly here rather than using the
      // pre-adjusted `timing`.
      record_histogram(internal::kHistogramGWSSGL, mark->start_time);
      sgl_time_ = mark->start_time;
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
  RecordGWSSessionStateHistograms();

  const page_load_metrics::ContentfulPaintTimingInfo&
      all_frames_largest_contentful_paint =
          GetDelegate()
              .GetLargestContentfulPaintHandler()
              .MergeMainFrameAndSubframes();
  if (!all_frames_largest_contentful_paint.ContainsValidTime()) {
    return;
  }

  if (WasActivatedInForegroundOptionalEventInForeground(
          all_frames_largest_contentful_paint.Time(), GetDelegate())) {
    CHECK(is_prerendered_);
    base::TimeDelta activation_to_lcp =
        page_load_metrics::CorrectEventAsNavigationOrActivationOrigined(
            GetDelegate(), all_frames_largest_contentful_paint.Time().value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramGWSActivationToLargestContentfulPaint,
        activation_to_lcp);

    if (IsIncognitoProfile()) {
      PAGE_LOAD_HISTOGRAM(
          base::StrCat(
              {internal::kHistogramGWSActivationToLargestContentfulPaint,
               internal::kHistogramIncognitoSuffix}),
          activation_to_lcp);
    }
    return;
  }

  if (IsPrenavigationLatencyEnabled() && aft_end_time_.has_value()) {
    // There are multiple patterns to record the prenavigation latency events:
    // - For non prerendering cases: If we have prenavigation latency, we should
    // add them to the AFT performance mark to get the total latency.
    // - For prerendering cases:
    //   - If the activation happens during this navigation, we should update
    //     the base time to start from the activation time. This means that any
    //     prenavigation event that happens before activation should be ignored
    //     and be set to 0.
    //   - If the activation happens before this navigation, the
    //     current navigation would start as a normal (non-prerendered
    //     navigation), since it should not start from `OnPrerenderStart`, and
    //     start from `OnStart`. Hence, we would not need to correct the base
    //     time, and the prenavigation latency should be recorded as it is to
    //     be consistent with what is recorded in the server side.
    auto base_time = aft_end_time_.value();
    std::optional<base::TimeDelta> prenavigation_time = sgl_time_;
    if (is_prerendered_) {
      // If we are in prerendering, we need to correct the AFTEnd to start from
      // the activation timing.
      base_time =
          page_load_metrics::CorrectEventAsNavigationOrActivationOrigined(
              GetDelegate(), aft_end_time_.value());
      prenavigation_time = std::nullopt;
    }

    // We record pre navigation time here as well.
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramGWSAFTEndWithPreNavigationLatency,
        base_time + prenavigation_time.value_or(base::TimeDelta()));
  }

  if (!WasStartedInForegroundOptionalEventInForeground(
          all_frames_largest_contentful_paint.Time(), GetDelegate())) {
    return;
  }

  CHECK(!is_prerendered_);
  RecordNavigationTimingHistograms();
  PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSLargestContentfulPaint,
                      all_frames_largest_contentful_paint.Time().value());
}

void GWSPageLoadMetricsObserver::RecordNavigationTimingHistograms() {
  CHECK(!is_prerendered_);
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

  // To avoid affecting other metrics, check `first_fetch_start_time`
  // separately.
  if (timing.first_fetch_start_time.has_value()) {
    PAGE_LOAD_SHORT_HISTOGRAM(
        internal::kHistogramGWSFirstFetchStartToFirstRequestStart,
        timing.first_request_start_time - *timing.first_fetch_start_time);
  }

  auto protocol = GetProtocolSuffix(http_connection_info_);
  auto record_histogram_with_suffix =
      [&protocol](const std::string& histogram_name, base::TimeDelta timing) {
        auto histogram_with_suffix = base::StrCat({histogram_name, protocol});
        PAGE_LOAD_SHORT_HISTOGRAM(histogram_name, timing);
        PAGE_LOAD_SHORT_HISTOGRAM(histogram_with_suffix, timing);
      };

  record_histogram_with_suffix(
      internal::kHistogramGWSConnectTimingFirstRequestDomainLookupDelay,
      timing.first_request_domain_lookup_delay);
  record_histogram_with_suffix(
      internal::kHistogramGWSConnectTimingFirstRequestConnectDelay,
      timing.first_request_connect_delay);
  record_histogram_with_suffix(
      internal::kHistogramGWSConnectTimingFirstRequestSslDelay,
      timing.first_request_ssl_delay);
  record_histogram_with_suffix(
      internal::kHistogramGWSConnectTimingFinalRequestDomainLookupDelay,
      timing.final_request_domain_lookup_delay);
  record_histogram_with_suffix(
      internal::kHistogramGWSConnectTimingFinalRequestConnectDelay,
      timing.final_request_connect_delay);
  record_histogram_with_suffix(
      internal::kHistogramGWSConnectTimingFinalRequestSslDelay,
      timing.final_request_ssl_delay);

  PAGE_LOAD_SHORT_HISTOGRAM(internal::kHistogramGWSCreateStreamDelay,
                            timing.create_stream_delay);
  PAGE_LOAD_SHORT_HISTOGRAM(internal::kHistogramGWSConnectedCallbackDelay,
                            timing.connected_callback_delay);
  PAGE_LOAD_SHORT_HISTOGRAM(internal::kHistogramGWSInitializeStreamDelay,
                            timing.initialize_stream_delay);

  // Record latency trace events.
  RecordLatencyHistograms(timing.non_redirect_response_start_time);

  if (network_accessed_) {
    if (timing.session_details.has_value()) {
      RecordSessionDetails(*timing.session_details, protocol);
    } else {
      // `session_details` is expected to be present. Collect a
      // DumpWithoutCrashing report.
      base::debug::DumpWithoutCrashing();
    }
  }

  // Record trace events according to the navigation milestone.
  TRACE_EVENT_BEGIN("loading", "GWSNavigationStartToFirstRequestStart",
                    perfetto::Track::FromPointer(this), navigation_start_time);
  TRACE_EVENT_END("loading", /* GWSNavigationStartToFirstRequestStart */
                  perfetto::Track::FromPointer(this),
                  timing.first_request_start_time);

  TRACE_EVENT_BEGIN("loading", "GWSFirstRequestStartToFirstResponseStart",
                    perfetto::Track::FromPointer(this),
                    timing.first_request_start_time);
  TRACE_EVENT_END("loading", /* GWSFirstRequestStartToFirstResponseStart */
                  perfetto::Track::FromPointer(this),
                  timing.first_response_start_time);

  TRACE_EVENT_BEGIN("loading", "GWSFirstResponseStartToFirstLoaderCallback",
                    perfetto::Track::FromPointer(this),
                    timing.first_response_start_time);
  TRACE_EVENT_END("loading", /* GWSFirstResponseStartToFirstLoaderCallback */
                  perfetto::Track::FromPointer(this),
                  timing.first_loader_callback_time);

  TRACE_EVENT_BEGIN("loading", "GWSFirstLoadCallbackToFinalResponseStart",
                    perfetto::Track::FromPointer(this),
                    timing.first_loader_callback_time);
  TRACE_EVENT_END("loading", /* GWSFirstLoadCallbackToFinalResponseStart */
                  perfetto::Track::FromPointer(this),
                  timing.final_response_start_time);

  TRACE_EVENT_BEGIN("loading", "GWSFinalResponseStartToFinalLoaderCallback",
                    perfetto::Track::FromPointer(this),
                    timing.final_response_start_time);
  TRACE_EVENT_END("loading", /* GWSFinalResponseStartToFinalLoaderCallback */
                  perfetto::Track::FromPointer(this),
                  timing.final_loader_callback_time);
}

void GWSPageLoadMetricsObserver::RecordPreCommitHistograms() {
  CHECK(!is_prerendered_);
  base::UmaHistogramEnumeration(internal::kHistogramGWSNavigationSourceType,
                                source_type_);
  if (!was_cached_) {
    RecordConnectionReuseHistograms();
  }
}

void GWSPageLoadMetricsObserver::RecordConnectionReuseHistograms() {
  DCHECK(!was_cached_);
  CHECK(!is_prerendered_);

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

  auto protocol = GetProtocolSuffix(http_connection_info_);
  auto total_histogram_name =
      base::StrCat({internal::kHistogramGWSConnectionReuseStatus, protocol});
  base::UmaHistogramEnumeration(total_histogram_name, status);

  if (IsIncognitoProfile()) {
    auto histogram_name_with_incognito_suffix =
        base::StrCat({internal::kHistogramGWSConnectionReuseStatus,
                      internal::kHistogramIncognitoSuffix});
    base::UmaHistogramEnumeration(histogram_name_with_incognito_suffix, status);

    // Record the total histogram with protocol suffix as well.
    total_histogram_name = base::StrCat({total_histogram_name, protocol});
    base::UmaHistogramEnumeration(total_histogram_name, status);
  }

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

void GWSPageLoadMetricsObserver::RecordLatencyHistograms(
    base::TimeTicks response_start_time) {
  CHECK(!is_prerendered_);
  const auto track = perfetto::NamedTrack("GWSLatencyEvent", navigation_id_);
  // TODO(crbug.com/364278026): SRT starts from the time when the user submits
  // a query. Using the navigation start time may not perfect to measure SRT.
  base::TimeDelta srt =
      response_start_time - GetDelegate().GetNavigationStart();
  TRACE_EVENT_BEGIN("navigation", "GWSLatency:SRT", track,
                    GetDelegate().GetNavigationStart());
  TRACE_EVENT_END("navigation", /* GWSLatency:SRT */
                  track, response_start_time);
  PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSSRT, srt);

  // Log some important CSI metrics only when related submetrics are recorded.
  std::optional<base::TimeDelta> hct;
  std::optional<base::TimeDelta> sct;

  if (aft_end_time_.has_value()) {
    // Currently `aft_start_time_` has the value of the server response time,
    // but in theory AFT starts at the end of SRT, the time when the client
    // receives the first byte of the header chunk.
    TRACE_EVENT_BEGIN("navigation", "GWSLatency:AFT", track,
                      response_start_time);
    TRACE_EVENT_END("navigation", /* GWSLatency:AFT */
                    track,
                    GetDelegate().GetNavigationStart() + aft_end_time_.value());
  }
  if (body_chunk_start_time_.has_value()) {
    TRACE_EVENT_BEGIN("navigation", "GWSLatency:SCT", track,
                      response_start_time);
    TRACE_EVENT_END(
        "navigation", /* GWSLatency:SCT */
        track,
        GetDelegate().GetNavigationStart() + body_chunk_start_time_.value());
    // `body_chunk_start_time_` is `base::TimeDelta` from the navigation start
    // time. On the other hand, `response_start_time` is `base::TimeTicks`.
    // SCT is the delta from 1) received the response header to 2) started
    // executing body chunk, this calculates the new delta between them for SCT.
    sct = body_chunk_start_time_.value() - srt;
    PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSSCT, sct.value());
  }
  if (head_chunk_end_time_.has_value()) {
    TRACE_EVENT_BEGIN("navigation", "GWSLatency:HCT", track,
                      response_start_time);
    TRACE_EVENT_END(
        "navigation", /* GWSLatency:HCT */
        track,
        GetDelegate().GetNavigationStart() + head_chunk_end_time_.value());
    // HCT is the delta from 1) received the response header to 2) the end of
    // the head chunk. This calculates the new delta between them for HCT.
    hct = head_chunk_end_time_.value() - srt;
    PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSHCT, hct.value());
  }
  if (head_chunk_start_time_.has_value()) {
    TRACE_EVENT_BEGIN("navigation", "GWSLatency:HST", track,
                      response_start_time);
    TRACE_EVENT_END(
        "navigation", /* GWSLatency:HST */
        track,
        GetDelegate().GetNavigationStart() + head_chunk_start_time_.value());
    // HST is the delta from 1) received the response header to 2) the start
    // time of processing head chunk. This calculates the new delta between
    // them for HST.
    PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSHST,
                        head_chunk_start_time_.value() - srt);
  }
  if (sct.has_value() && hct.has_value()) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramGWSTimeBetweenHCTAndSCT,
                        sct.value() - hct.value());
  }
}

void GWSPageLoadMetricsObserver::RecordSessionDetails(
    const content::NavigationHandleTiming::SessionDetails& session_details,
    std::string_view protocol) {
  if (http_connection_info_ == net::HttpConnectionInfoCoarse::kHTTP2 ||
      http_connection_info_ == net::HttpConnectionInfoCoarse::kQUIC) {
    if (session_details.session_source.has_value()) {
      base::UmaHistogramEnumeration(
          base::StrCat({internal::kHistogramGWSSessionSource, protocol}),
          *session_details.session_source);
    } else {
      // `session_source` is expected to be present. Collect a
      // DumpWithoutCrashing report.
      base::debug::DumpWithoutCrashing();
    }
  }

  std::string advertized_alt_svc_state_histgram_name = base::StrCat(
      {internal::kHistogramGWSAdvertisedAltSvcState,
       session_details.http_network_session_quic_enabled ? ".QuicEnabled"
                                                         : ".QuicDisabled"});
  base::UmaHistogramEnumeration(advertized_alt_svc_state_histgram_name,
                                session_details.advertised_alt_svc_state);
  if (IsIncognitoProfile()) {
    auto histogram_name = base::StrCat({advertized_alt_svc_state_histgram_name,
                                        internal::kHistogramIncognitoSuffix});
    base::UmaHistogramEnumeration(histogram_name,
                                  session_details.advertised_alt_svc_state);
  }

  base::UmaHistogramBoolean(
      internal::kHistogramGWSHttpNetworkSessionQuicEnabled,
      session_details.http_network_session_quic_enabled);
}

void GWSPageLoadMetricsObserver::RecordGWSSessionStateHistograms() {
  auto* browser_context = GetDelegate().GetWebContents()->GetBrowserContext();
  CHECK(browser_context);

  auto* gws_session_state =
      page_load_metrics::GWSSessionState::GetOrCreateForBrowserContext(
          browser_context);
  gws_session_state->IncreasePageLoadCount();

  if (!gws_session_state->IsSignedIn() && IsSignedIn(browser_context)) {
    gws_session_state->SetSignedIn();
  }

  content::BrowserContext* original_browser_context =
      browser_context->IsOffTheRecord() ? GetOriginalBrowserContext() : nullptr;
  if (original_browser_context) {
    auto* original_gws_session_state =
        page_load_metrics::GWSSessionState::GetOrCreateForBrowserContext(
            original_browser_context);
    if (!original_gws_session_state->IsSignedIn() &&
        IsSignedIn(original_browser_context)) {
      original_gws_session_state->SetSignedIn();
    }
  }
  WarmUpType type =
      ClassifyIntoWarmUpType(browser_context, original_browser_context);
  base::UmaHistogramEnumeration(internal::kHistogramGWSWarmUpType, type);

  if (!gws_session_state->IsSignedIn() && aft_end_time_.has_value()) {
    gws_session_state->SetPrewarmed();
  }
}
