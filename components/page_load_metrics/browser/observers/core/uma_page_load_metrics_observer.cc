// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/core/uma_page_load_metrics_observer.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/trace_event/named_trigger.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "build/chromeos_buildflags.h"
#include "components/metrics/metrics_data_validation.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_memory_tracker.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/common/process_type.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/request_destination.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "ui/base/page_transition_types.h"
#include "ui/events/blink/blink_features.h"

namespace {

static constexpr uint64_t kInstantPageLoadEventsTraceTrackId = 13839844603789;

// The threshold to emit a trace event is the 99th percentile
// of the histogram on Windows Stable as of Feb 26th, 2020.
constexpr base::TimeDelta kFirstContentfulPaintTraceThreshold =
    base::Milliseconds(12388);

// TODO(bmcquade): If other observers want to log histograms based on load type,
// promote this enum to page_load_metrics_observer.h.
enum PageLoadType {
  LOAD_TYPE_NONE = 0,
  LOAD_TYPE_RELOAD,
  LOAD_TYPE_FORWARD_BACK,
  LOAD_TYPE_NEW_NAVIGATION
};

PageLoadType GetPageLoadType(ui::PageTransition transition) {
  if (transition & ui::PAGE_TRANSITION_FORWARD_BACK) {
    return LOAD_TYPE_FORWARD_BACK;
  }
  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD)) {
    return LOAD_TYPE_RELOAD;
  }
  if (ui::PageTransitionIsNewNavigation(transition)) {
    return LOAD_TYPE_NEW_NAVIGATION;
  }
  NOTREACHED_IN_MIGRATION()
      << "Received PageTransition with no matching PageLoadType.";
  return LOAD_TYPE_NONE;
}

std::unique_ptr<base::trace_event::TracedValue> FirstInputDelayTraceData(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  std::unique_ptr<base::trace_event::TracedValue> data =
      std::make_unique<base::trace_event::TracedValue>();
  data->SetDouble(
      "firstInputDelayInMilliseconds",
      timing.interactive_timing->first_input_delay->InMillisecondsF());
  data->SetDouble(
      "navStartToFirstInputTimestampInMilliseconds",
      timing.interactive_timing->first_input_timestamp->InMillisecondsF());
  return data;
}

#define TRACE_WITH_TIMESTAMP0(category_group, name, trace_id, begin_time,   \
                              end_time)                                     \
  do {                                                                      \
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(category_group, name,  \
                                                     trace_id, begin_time); \
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(category_group, name,    \
                                                   trace_id, end_time);     \
  } while (0)

}  // namespace

namespace internal {

const char kHistogramDomContentLoaded[] =
    "PageLoad.DocumentTiming.NavigationToDOMContentLoadedEventFired";
const char kBackgroundHistogramDomContentLoaded[] =
    "PageLoad.DocumentTiming.NavigationToDOMContentLoadedEventFired.Background";
const char kHistogramLoad[] =
    "PageLoad.DocumentTiming.NavigationToLoadEventFired";
const char kBackgroundHistogramLoad[] =
    "PageLoad.DocumentTiming.NavigationToLoadEventFired.Background";
const char kHistogramFirstPaint[] =
    "PageLoad.PaintTiming.NavigationToFirstPaint";
const char kBackgroundHistogramFirstPaint[] =
    "PageLoad.PaintTiming.NavigationToFirstPaint.Background";
const char kHistogramFirstImagePaint[] =
    "PageLoad.PaintTiming.NavigationToFirstImagePaint";
const char kBackgroundHistogramFirstImagePaint[] =
    "PageLoad.PaintTiming.NavigationToFirstImagePaint.Background";
const char kBackgroundHttpsOrDataOrFileSchemeHistogramFirstContentfulPaint[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.Background."
    "HttpsOrDataOrFileScheme";
const char kHistogramFirstContentfulPaint[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint";
const char kBackgroundHistogramFirstContentfulPaint[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.Background";
const char kHistogramFirstContentfulPaintInitiatingProcess[] =
    "PageLoad.Internal.PaintTiming.NavigationToFirstContentfulPaint."
    "InitiatingProcess";
const char kHistogramLargestContentfulPaint[] =
    "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2";
const char kBackgroundHttpsOrDataOrFileSchemeHistogramLargestContentfulPaint[] =
    "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.Background."
    "HttpsOrDataOrFileScheme";
const char kHistogramLargestContentfulPaintContentType[] =
    "PageLoad.Internal.PaintTiming.LargestContentfulPaint.ContentType";
const char kHistogramLargestContentfulPaintMainFrame[] =
    "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.MainFrame";
const char kHistogramLargestContentfulPaintMainFrameContentType[] =
    "PageLoad.Internal.PaintTiming.LargestContentfulPaint.MainFrame."
    "ContentType";
const char kHistogramLargestContentfulPaintCrossSiteSubFrame[] =
    "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2."
    "CrossSiteSubFrame";
const char kHistogramLargestContentfulPaintSetSpeculationRulesPrerender[] =
    "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2."
    "SetSpeculationRulesPrerender";
const char kHistogramNumInteractions[] =
    "PageLoad.InteractiveTiming.NumInteractions";
const char kHistogramUserInteractionLatencyHighPercentile2MaxEventDuration[] =
    "PageLoad.InteractiveTiming.UserInteractionLatency.HighPercentile2."
    "MaxEventDuration";
const char kHistogramInpOffset[] = "PageLoad.InteractiveTiming.INPOffset";
const char kHistogramInpTime[] = "PageLoad.InteractiveTiming.INPTime";
const char kHistogramWorstUserInteractionLatencyMaxEventDuration[] =
    "PageLoad.InteractiveTiming.WorstUserInteractionLatency.MaxEventDuration";

const char kHistogramFirstInputDelay[] =
    "PageLoad.InteractiveTiming.FirstInputDelay4";
const char kHistogramFirstInputTimestamp[] =
    "PageLoad.InteractiveTiming.FirstInputTimestamp4";
const char kHistogramParseStartToFirstContentfulPaint[] =
    "PageLoad.PaintTiming.ParseStartToFirstContentfulPaint";
const char kBackgroundHistogramParseStartToFirstContentfulPaint[] =
    "PageLoad.PaintTiming.ParseStartToFirstContentfulPaint.Background";
const char kHistogramParseStart[] =
    "PageLoad.ParseTiming.NavigationToParseStart";
const char kBackgroundHistogramParseStart[] =
    "PageLoad.ParseTiming.NavigationToParseStart.Background";
const char kHistogramParseBlockedOnScriptLoad[] =
    "PageLoad.ParseTiming.ParseBlockedOnScriptLoad";
const char kBackgroundHistogramParseBlockedOnScriptLoad[] =
    "PageLoad.ParseTiming.ParseBlockedOnScriptLoad.Background";
const char kHistogramParseBlockedOnScriptExecution[] =
    "PageLoad.ParseTiming.ParseBlockedOnScriptExecution";
const char kHistogramParseBlockedOnScriptExecutionDocumentWrite[] =
    "PageLoad.ParseTiming.ParseBlockedOnScriptExecutionFromDocumentWrite";

const char kHistogramFirstContentfulPaintNoStore[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.NoStore";

const char kHistogramFirstContentfulPaintHiddenWhileFlushing[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.HiddenWhileFlushing";

const char kHistogramLoadTypeFirstContentfulPaintReload[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.LoadType."
    "Reload";
const char kHistogramLoadTypeFirstContentfulPaintReloadByGesture[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.LoadType."
    "Reload.UserGesture";
const char kHistogramLoadTypeFirstContentfulPaintForwardBack[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.LoadType."
    "ForwardBackNavigation";
const char kHistogramLoadTypeFirstContentfulPaintForwardBackNoStore[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.LoadType."
    "ForwardBackNavigation.NoStore";
const char kHistogramLoadTypeFirstContentfulPaintNewNavigation[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.LoadType."
    "NewNavigation";

const char kHistogramPageTimingForegroundDuration[] =
    "PageLoad.PageTiming.ForegroundDuration";
const char kHistogramPageTimingForegroundDurationAfterPaint[] =
    "PageLoad.PageTiming.ForegroundDuration.AfterPaint";
const char kHistogramPageTimingForegroundDurationNoCommit[] =
    "PageLoad.PageTiming.ForegroundDuration.NoCommit";
const char kHistogramPageTimingForegroundDurationWithPaint[] =
    "PageLoad.PageTiming.ForegroundDuration.WithPaint";
const char kHistogramPageTimingForegroundDurationWithoutPaint[] =
    "PageLoad.PageTiming.ForegroundDuration.WithoutPaint";

const char kHistogramLoadTypeParseStartReload[] =
    "PageLoad.ParseTiming.NavigationToParseStart.LoadType.Reload";
const char kHistogramLoadTypeParseStartForwardBack[] =
    "PageLoad.ParseTiming.NavigationToParseStart.LoadType."
    "ForwardBackNavigation";
const char kHistogramLoadTypeParseStartForwardBackNoStore[] =
    "PageLoad.ParseTiming.NavigationToParseStart.LoadType."
    "ForwardBackNavigation.NoStore";
const char kHistogramLoadTypeParseStartNewNavigation[] =
    "PageLoad.ParseTiming.NavigationToParseStart.LoadType.NewNavigation";

const char kHistogramFirstForeground[] =
    "PageLoad.PageTiming.NavigationToFirstForeground";

const char kHistogramUserGestureNavigationToForwardBack[] =
    "PageLoad.PageTiming.ForegroundDuration.PageEndReason."
    "ForwardBackNavigation.UserGesture";

const char kHistogramForegroundToFirstContentfulPaint[] =
    "PageLoad.PaintTiming.ForegroundToFirstContentfulPaint";

const char kHistogramFirstContentfulPaintUserInitiated[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.UserInitiated";

const char kHistogramCachedResourceLoadTimePrefix[] =
    "PageLoad.Experimental.PageTiming.CachedResourceLoadTime.";
const char kHistogramCommitSentToFirstSubresourceLoadStart[] =
    "PageLoad.Experimental.PageTiming.CommitSentToFirstSubresourceLoadStart";
const char kHistogramNavigationToFirstSubresourceLoadStart[] =
    "PageLoad.Experimental.PageTiming.NavigationToFirstSubresourceLoadStart";
const char kHistogramResourceLoadTimePrefix[] =
    "PageLoad.Experimental.PageTiming.ResourceLoadTime.";
const char kHistogramTotalSubresourceLoadTimeAtFirstContentfulPaint[] =
    "PageLoad.Experimental.PageTiming."
    "TotalSubresourceLoadTimeAtFirstContentfulPaint";
const char kHistogramFirstEligibleToPaintToFirstPaint[] =
    "PageLoad.Experimental.PaintTiming.FirstEligibleToPaintToFirstPaint";

const char kHistogramPageLoadCpuTotalUsage[] = "PageLoad.Cpu.TotalUsage";
const char kHistogramPageLoadCpuTotalUsageForegrounded[] =
    "PageLoad.Cpu.TotalUsageForegrounded";

const char kHistogramInputToNavigation[] =
    "PageLoad.Experimental.InputTiming.InputToNavigationStart";
const char kBackgroundHistogramInputToNavigation[] =
    "PageLoad.Experimental.InputTiming.InputToNavigationStart.Background";
const char kHistogramInputToNavigationLinkClick[] =
    "PageLoad.Experimental.InputTiming.InputToNavigationStart.FromLinkClick";
const char kHistogramInputToNavigationOmnibox[] =
    "PageLoad.Experimental.InputTiming.InputToNavigationStart.FromOmnibox";
const char kHistogramInputToFirstContentfulPaint[] =
    "PageLoad.Experimental.PaintTiming.InputToFirstContentfulPaint";

const char kHistogramBackForwardCacheEvent[] =
    "PageLoad.BackForwardCache.Event";

// Navigation metrics from the navigation start.
const char kHistogramNavigationTimingNavigationStartToFirstRequestStart[] =
    "PageLoad.Experimental.NavigationTiming.NavigationStartToFirstRequestStart";
const char kHistogramNavigationTimingNavigationStartToFirstResponseStart[] =
    "PageLoad.Experimental.NavigationTiming."
    "NavigationStartToFirstResponseStart";
const char kHistogramNavigationTimingNavigationStartToFirstLoaderCallback[] =
    "PageLoad.Experimental.NavigationTiming."
    "NavigationStartToFirstLoaderCallback";
const char kHistogramNavigationTimingNavigationStartToFinalRequestStart[] =
    "PageLoad.Experimental.NavigationTiming.NavigationStartToFinalRequestStart";
const char kHistogramNavigationTimingNavigationStartToFinalResponseStart[] =
    "PageLoad.Experimental.NavigationTiming."
    "NavigationStartToFinalResponseStart";
const char kHistogramNavigationTimingNavigationStartToFinalLoaderCallback[] =
    "PageLoad.Experimental.NavigationTiming."
    "NavigationStartToFinalLoaderCallback";
const char kHistogramNavigationTimingNavigationStartToNavigationCommitSent[] =
    "PageLoad.Experimental.NavigationTiming."
    "NavigationStartToNavigationCommitSent";

// Navigation metrics between milestones.
const char kHistogramNavigationTimingFirstRequestStartToFirstResponseStart[] =
    "PageLoad.Experimental.NavigationTiming."
    "FirstRequestStartToFirstResponseStart";
const char kHistogramNavigationTimingFirstResponseStartToFirstLoaderCallback[] =
    "PageLoad.Experimental.NavigationTiming."
    "FirstResponseStartToFirstLoaderCallback";
const char kHistogramNavigationTimingFinalRequestStartToFinalResponseStart[] =
    "PageLoad.Experimental.NavigationTiming."
    "FinalRequestStartToFinalResponseStart";
const char kHistogramNavigationTimingFinalResponseStartToFinalLoaderCallback[] =
    "PageLoad.Experimental.NavigationTiming."
    "FinalResponseStartToFinalLoaderCallback";
const char
    kHistogramNavigationTimingFinalLoaderCallbackToNavigationCommitSent[] =
        "PageLoad.Experimental.NavigationTiming."
        "FinalLoaderCallbackToNavigationCommitSent";

// V8 memory usage metrics.
const char kHistogramMemoryMainframe[] =
    "PageLoad.Experimental.Memory.Core.MainFrame.Max";
const char kHistogramMemorySubframeAggregate[] =
    "PageLoad.Experimental.Memory.Core.Subframe.Aggregate.Max";
const char kHistogramMemoryTotal[] =
    "PageLoad.Experimental.Memory.Core.Total.Max";

}  // namespace internal

UmaPageLoadMetricsObserver::UmaPageLoadMetricsObserver()
    : transition_(ui::PAGE_TRANSITION_LINK),
      was_no_store_main_resource_(false),
      cache_bytes_(0),
      network_bytes_(0),
      network_bytes_including_headers_(0) {
  // Emit a trigger to allow trace collection tied to navigations. For
  // simplicity, this signal happens during `WillStartRequest`, which is a bit
  // later than the `navigation_start` timestamp used in
  // `PageLoad.PaintTiming.NavigationToFirstContentfulPaint`.
  base::trace_event::EmitNamedTrigger("navigation-start");
}

UmaPageLoadMetricsObserver::~UmaPageLoadMetricsObserver() = default;

const char* UmaPageLoadMetricsObserver::GetObserverName() const {
  static const char kName[] = "UmaPageLoadMetricsObserver";
  return kName;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UmaPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class needs forwarding for the events OnLoadedResource and
  // OnV8MemoryChanged.
  return FORWARD_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UmaPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // PrerenderPageLoadMetricsObserver records prerendering version of metrics
  // and this PLMO can stop on prerendering.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UmaPageLoadMetricsObserver::OnRedirect(
    content::NavigationHandle* navigation_handle) {
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UmaPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  transition_ = navigation_handle->GetPageTransition();
  const net::HttpResponseHeaders* headers =
      navigation_handle->GetResponseHeaders();
  if (headers) {
    was_no_store_main_resource_ =
        headers->HasHeaderValue("cache-control", "no-store");
  }
  navigation_handle_timing_ = navigation_handle->GetNavigationHandleTiming();
  return CONTINUE_OBSERVING;
}

void UmaPageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->dom_content_loaded_event_start,
          GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDomContentLoaded,
        timing.document_timing->dom_content_loaded_event_start.value());
  } else {
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramDomContentLoaded,
        timing.document_timing->dom_content_loaded_event_start.value());
  }

  EmitInstantTraceEvent(
      timing.document_timing->dom_content_loaded_event_start.value(),
      "PageLoadMetrics.NavigationToDOMContentLoadedEventFired");
}

void UmaPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->load_event_start, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramLoad,
                        timing.document_timing->load_event_start.value());
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramLoad,
                        timing.document_timing->load_event_start.value());
  }

  EmitInstantTraceEvent(timing.document_timing->load_event_start.value(),
                        "PageLoadMetrics.NavigationToMainFrameOnLoad");
}

void UmaPageLoadMetricsObserver::OnFirstPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  first_paint_ = GetDelegate().GetNavigationStart() +
                 timing.paint_timing->first_paint.value();
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_paint, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstPaint,
                        timing.paint_timing->first_paint.value());
    if (timing.paint_timing->first_eligible_to_paint) {
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFirstEligibleToPaintToFirstPaint,
          timing.paint_timing->first_paint.value() -
              timing.paint_timing->first_eligible_to_paint.value());
    }
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramFirstPaint,
                        timing.paint_timing->first_paint.value());
  }
}

void UmaPageLoadMetricsObserver::OnFirstImagePaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_image_paint, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstImagePaint,
                        timing.paint_timing->first_image_paint.value());
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramFirstImagePaint,
                        timing.paint_timing->first_image_paint.value());
  }
}

void UmaPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK(timing.paint_timing->first_contentful_paint);
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    EmitFCPTraceEvent(timing.paint_timing->first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstContentfulPaint,
                        timing.paint_timing->first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(internal::kHistogramParseStartToFirstContentfulPaint,
                        timing.paint_timing->first_contentful_paint.value() -
                            timing.parse_timing->parse_start.value());

    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramTotalSubresourceLoadTimeAtFirstContentfulPaint,
        total_subresource_load_time_);

    // Emit a trace event to highlight a long navigation to first contentful
    // paint.
    if (timing.paint_timing->first_contentful_paint.value() >
        kFirstContentfulPaintTraceThreshold) {
      auto trace_id = TRACE_ID_WITH_SCOPE(
          "UmaPageLoadMetricsObserver::OnFirstContentfulPaintInPage_for_"
          "LongNavigation",
          TRACE_ID_LOCAL(this));
      base::TimeTicks navigation_start = GetDelegate().GetNavigationStart();
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
          "latency", "Long Navigation to First Contentful Paint", trace_id,
          navigation_start);
      TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
          "latency", "Long Navigation to First Contentful Paint", trace_id,
          navigation_start +
              timing.paint_timing->first_contentful_paint.value());
    }

    UMA_HISTOGRAM_ENUMERATION(
        internal::kHistogramFirstContentfulPaintInitiatingProcess,
        GetDelegate().GetUserInitiatedInfo().browser_initiated
            ? content::PROCESS_TYPE_BROWSER
            : content::PROCESS_TYPE_RENDERER,
        content::PROCESS_TYPE_CONTENT_END);

    if (was_no_store_main_resource_) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstContentfulPaintNoStore,
                          timing.paint_timing->first_contentful_paint.value());
    }

    // TODO(bmcquade): consider adding a histogram that uses
    // UserInputInfo.user_input_event.
    if (GetDelegate().GetUserInitiatedInfo().browser_initiated ||
        GetDelegate().GetUserInitiatedInfo().user_gesture) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstContentfulPaintUserInitiated,
                          timing.paint_timing->first_contentful_paint.value());
    }

    if (timing.input_to_navigation_start) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramInputToNavigation,
                          timing.input_to_navigation_start.value());
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramInputToFirstContentfulPaint,
          timing.input_to_navigation_start.value() +
              timing.paint_timing->first_contentful_paint.value());

      if (ui::PageTransitionCoreTypeIs(transition_, ui::PAGE_TRANSITION_LINK)) {
        PAGE_LOAD_HISTOGRAM(internal::kHistogramInputToNavigationLinkClick,
                            timing.input_to_navigation_start.value());
      } else if (ui::PageTransitionCoreTypeIs(transition_,
                                              ui::PAGE_TRANSITION_GENERATED) ||
                 ui::PageTransitionCoreTypeIs(transition_,
                                              ui::PAGE_TRANSITION_TYPED)) {
        PAGE_LOAD_HISTOGRAM(internal::kHistogramInputToNavigationOmnibox,
                            timing.input_to_navigation_start.value());
      }
    }

    if (GetDelegate().GetTimeToFirstBackground()) {
      // We were started in the foreground, and got FCP while in foreground,
      // but became hidden while propagating the FCP value from Blink into the
      // PLM observer. In this case, we will have missed the FCP UKM value,
      // since it is logged in UkmPageLoadMetricsObserver::OnHidden.
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFirstContentfulPaintHiddenWhileFlushing,
          timing.paint_timing->first_contentful_paint.value());
    }

    switch (GetPageLoadType(transition_)) {
      case LOAD_TYPE_RELOAD:
        PAGE_LOAD_HISTOGRAM(
            internal::kHistogramLoadTypeFirstContentfulPaintReload,
            timing.paint_timing->first_contentful_paint.value());
        // TODO(bmcquade): consider adding a histogram that uses
        // UserInputInfo.user_input_event.
        if (GetDelegate().GetUserInitiatedInfo().browser_initiated ||
            GetDelegate().GetUserInitiatedInfo().user_gesture) {
          PAGE_LOAD_HISTOGRAM(
              internal::kHistogramLoadTypeFirstContentfulPaintReloadByGesture,
              timing.paint_timing->first_contentful_paint.value());
        }
        break;
      case LOAD_TYPE_FORWARD_BACK:
        PAGE_LOAD_HISTOGRAM(
            internal::kHistogramLoadTypeFirstContentfulPaintForwardBack,
            timing.paint_timing->first_contentful_paint.value());
        if (was_no_store_main_resource_) {
          PAGE_LOAD_HISTOGRAM(
              internal::
                  kHistogramLoadTypeFirstContentfulPaintForwardBackNoStore,
              timing.paint_timing->first_contentful_paint.value());
        }
        break;
      case LOAD_TYPE_NEW_NAVIGATION:
        PAGE_LOAD_HISTOGRAM(
            internal::kHistogramLoadTypeFirstContentfulPaintNewNavigation,
            timing.paint_timing->first_contentful_paint.value());
        break;
      case LOAD_TYPE_NONE:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramFirstContentfulPaint,
                        timing.paint_timing->first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramParseStartToFirstContentfulPaint,
        timing.paint_timing->first_contentful_paint.value() -
            timing.parse_timing->parse_start.value());
    if (timing.input_to_navigation_start) {
      PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramInputToNavigation,
                          timing.input_to_navigation_start.value());
    }

    PAGE_LOAD_HISTOGRAM(
        internal::
            kBackgroundHttpsOrDataOrFileSchemeHistogramFirstContentfulPaint,
        timing.paint_timing->first_contentful_paint.value());
  }

  if (page_load_metrics::WasStartedInBackgroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramForegroundToFirstContentfulPaint,
                        timing.paint_timing->first_contentful_paint.value() -
                            GetDelegate().GetTimeToFirstForeground().value());
  }
}

void UmaPageLoadMetricsObserver::OnFirstInputInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.interactive_timing->first_input_timestamp, GetDelegate())) {
    return;
  }
  UMA_HISTOGRAM_CUSTOM_TIMES(
      internal::kHistogramFirstInputDelay,
      timing.interactive_timing->first_input_delay.value(),
      base::Milliseconds(1), base::Seconds(60), 50);
  // The pseudo metric of |kHistogramFirstInputDelay|. Only used to assess field
  // trial data quality.
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "UMA.Pseudo.PageLoad.InteractiveTiming.FirstInputDelay4",
      metrics::GetPseudoMetricsSample(
          timing.interactive_timing->first_input_delay.value()),
      base::Milliseconds(1), base::Seconds(60), 50);
  PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstInputTimestamp,
                      timing.interactive_timing->first_input_timestamp.value());
  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "loading", "FirstInputDelay::AllFrames::UMA",
      GetDelegate().GetNavigationStart() +
          timing.interactive_timing->first_input_timestamp.value(),
      "data", FirstInputDelayTraceData(timing));
}

void UmaPageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_start, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramParseStart,
                        timing.parse_timing->parse_start.value());

    switch (GetPageLoadType(transition_)) {
      case LOAD_TYPE_RELOAD:
        PAGE_LOAD_HISTOGRAM(internal::kHistogramLoadTypeParseStartReload,
                            timing.parse_timing->parse_start.value());
        break;
      case LOAD_TYPE_FORWARD_BACK:
        PAGE_LOAD_HISTOGRAM(internal::kHistogramLoadTypeParseStartForwardBack,
                            timing.parse_timing->parse_start.value());
        if (was_no_store_main_resource_) {
          PAGE_LOAD_HISTOGRAM(
              internal::kHistogramLoadTypeParseStartForwardBackNoStore,
              timing.parse_timing->parse_start.value());
        }
        break;
      case LOAD_TYPE_NEW_NAVIGATION:
        PAGE_LOAD_HISTOGRAM(internal::kHistogramLoadTypeParseStartNewNavigation,
                            timing.parse_timing->parse_start.value());
        break;
      case LOAD_TYPE_NONE:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramParseStart,
                        timing.parse_timing->parse_start.value());
  }
}

void UmaPageLoadMetricsObserver::OnParseStop(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_stop, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramParseBlockedOnScriptLoad,
        timing.parse_timing->parse_blocked_on_script_load_duration.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramParseBlockedOnScriptExecution,
        timing.parse_timing->parse_blocked_on_script_execution_duration
            .value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramParseBlockedOnScriptExecutionDocumentWrite,
        timing.parse_timing
            ->parse_blocked_on_script_execution_from_document_write_duration
            .value());
  } else {
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramParseBlockedOnScriptLoad,
        timing.parse_timing->parse_blocked_on_script_load_duration.value());
  }
}

void UmaPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordNavigationTimingHistograms();
  RecordTimingHistograms(timing);
  RecordByteAndResourceHistograms(timing);
  RecordCpuUsageHistograms();
  RecordForegroundDurationHistograms(timing, base::TimeTicks());
  RecordV8MemoryHistograms();
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UmaPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // FlushMetricsOnAppEnterBackground is invoked on Android in cases where the
  // app is about to be backgrounded, as part of the Activity.onPause()
  // flow. After this method is invoked, Chrome may be killed without further
  // notification, so we record final metrics collected up to this point.
  if (GetDelegate().DidCommit()) {
    RecordNavigationTimingHistograms();
    RecordTimingHistograms(timing);
    RecordByteAndResourceHistograms(timing);
    RecordCpuUsageHistograms();
    RecordV8MemoryHistograms();
  }
  RecordForegroundDurationHistograms(timing, base::TimeTicks::Now());
  return STOP_OBSERVING;
}

void UmaPageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info) {
  // Provide an empty PageLoadTiming, since we don't have any timing metrics
  // for failed provisional loads.
  RecordForegroundDurationHistograms(page_load_metrics::mojom::PageLoadTiming(),
                                     base::TimeTicks());
}

void UmaPageLoadMetricsObserver::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo&
        extra_request_complete_info) {
  const net::LoadTimingInfo& timing_info =
      *extra_request_complete_info.load_timing_info;
  if (timing_info.receive_headers_end.is_null())
    return;

  std::string_view destination =
      network::RequestDestinationToStringForHistogram(
          extra_request_complete_info.request_destination);

  base::TimeDelta delta =
      timing_info.receive_headers_end - timing_info.request_start;
  if (extra_request_complete_info.was_cached) {
    base::UmaHistogramMediumTimes(
        base::StrCat(
            {internal::kHistogramCachedResourceLoadTimePrefix, destination}),
        delta);
  } else {
    base::UmaHistogramMediumTimes(
        base::StrCat({internal::kHistogramResourceLoadTimePrefix, destination}),
        delta);
  }

  // Rest of the method only operates on subresource loads.
  if (extra_request_complete_info.request_destination ==
      network::mojom::RequestDestination::kDocument) {
    return;
  }

  total_subresource_load_time_ += delta;

  // Rest of the method only logs metrics for the first subresource load.
  if (received_first_subresource_load_)
    return;

  received_first_subresource_load_ = true;
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramNavigationToFirstSubresourceLoadStart,
      timing_info.request_start - GetDelegate().GetNavigationStart());

  base::TimeTicks commit_sent_time =
      navigation_handle_timing_.navigation_commit_sent_time;
  if (!commit_sent_time.is_null()) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramCommitSentToFirstSubresourceLoadStart,
        timing_info.request_start - commit_sent_time);

    TRACE_WITH_TIMESTAMP0(
        "loading", "CommitSentToFirstSubresourceLoadStart",
        TRACE_ID_WITH_SCOPE("CommitSentToFirstSubresourceLoadStart",
                            TRACE_ID_LOCAL(this)),
        commit_sent_time, timing_info.request_start);
  }
}

void UmaPageLoadMetricsObserver::OnUserInput(
    const blink::WebInputEvent& event,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (first_paint_.is_null())
    return;

  // Track clicks after first paint for possible click burst.
  click_tracker_.OnUserInput(event);
}

void UmaPageLoadMetricsObserver::OnResourceDataUseObserved(
    content::RenderFrameHost* rfh,
    const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
        resources) {
  for (auto const& resource : resources) {
    if (resource->is_complete) {
      if (resource->cache_type ==
          page_load_metrics::mojom::CacheType::kNotCached) {
        network_bytes_ += resource->encoded_body_length;
      } else {
        cache_bytes_ += resource->encoded_body_length;
      }
    }
    network_bytes_including_headers_ += resource->delta_bytes;
  }
}

void UmaPageLoadMetricsObserver::RecordNavigationTimingHistograms() {
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
  // TODO(crbug.com/40688345): Change these early-returns to DCHECKs
  // after the issue 1076710 is fixed.
  if (navigation_start_time > timing.first_request_start_time ||
      timing.first_request_start_time > timing.first_response_start_time ||
      timing.first_response_start_time > timing.first_loader_callback_time ||
      timing.first_loader_callback_time > timing.navigation_commit_sent_time) {
    return;
  }
  if (navigation_start_time > timing.final_request_start_time ||
      timing.final_request_start_time > timing.final_response_start_time ||
      timing.final_response_start_time > timing.final_loader_callback_time ||
      timing.final_loader_callback_time > timing.navigation_commit_sent_time) {
    return;
  }
  DCHECK_LE(timing.first_request_start_time, timing.final_request_start_time);
  DCHECK_LE(timing.first_response_start_time, timing.final_response_start_time);
  DCHECK_LE(timing.first_loader_callback_time,
            timing.final_loader_callback_time);

  // Record the elapsed time from the navigation start milestone.
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramNavigationTimingNavigationStartToFirstRequestStart,
      timing.first_request_start_time - navigation_start_time);
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramNavigationTimingNavigationStartToFirstResponseStart,
      timing.first_response_start_time - navigation_start_time);
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramNavigationTimingNavigationStartToFirstLoaderCallback,
      timing.first_loader_callback_time - navigation_start_time);

  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramNavigationTimingNavigationStartToFinalRequestStart,
      timing.final_request_start_time - navigation_start_time);
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramNavigationTimingNavigationStartToFinalResponseStart,
      timing.final_response_start_time - navigation_start_time);
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramNavigationTimingNavigationStartToFinalLoaderCallback,
      timing.final_loader_callback_time - navigation_start_time);

  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramNavigationTimingNavigationStartToNavigationCommitSent,
      timing.navigation_commit_sent_time - navigation_start_time);

  // Record the intervals between milestones.
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramNavigationTimingFirstRequestStartToFirstResponseStart,
      timing.first_response_start_time - timing.first_request_start_time);
  PAGE_LOAD_HISTOGRAM(
      internal::
          kHistogramNavigationTimingFirstResponseStartToFirstLoaderCallback,
      timing.first_loader_callback_time - timing.first_response_start_time);

  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramNavigationTimingFinalRequestStartToFinalResponseStart,
      timing.final_response_start_time - timing.final_request_start_time);
  PAGE_LOAD_HISTOGRAM(
      internal::
          kHistogramNavigationTimingFinalResponseStartToFinalLoaderCallback,
      timing.final_loader_callback_time - timing.final_response_start_time);

  PAGE_LOAD_HISTOGRAM(
      internal::
          kHistogramNavigationTimingFinalLoaderCallbackToNavigationCommitSent,
      timing.navigation_commit_sent_time - timing.final_loader_callback_time);
}

// This method records values for metrics that were not recorded during any
// other event, or records failure status for metrics that have not been
// collected yet. This is meant to be called at the end of a page lifetime, for
// example, when the user is navigating away from the page.
void UmaPageLoadMetricsObserver::RecordTimingHistograms(
    const page_load_metrics::mojom::PageLoadTiming& main_frame_timing) {
  // Log time to first foreground / time to first background. Log counts that we
  // started a relevant page load in the foreground / background.
  if (!GetDelegate().StartedInForeground() &&
      GetDelegate().GetTimeToFirstForeground()) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstForeground,
                        GetDelegate().GetTimeToFirstForeground().value());
  }

  const page_load_metrics::ContentfulPaintTimingInfo&
      main_frame_largest_contentful_paint =
          GetDelegate()
              .GetLargestContentfulPaintHandler()
              .MainFrameLargestContentfulPaint();
  if (main_frame_largest_contentful_paint.ContainsValidTime() &&
      WasStartedInForegroundOptionalEventInForeground(
          main_frame_largest_contentful_paint.Time(), GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramLargestContentfulPaintMainFrame,
                        main_frame_largest_contentful_paint.Time().value());
    UMA_HISTOGRAM_ENUMERATION(
        internal::kHistogramLargestContentfulPaintMainFrameContentType,
        main_frame_largest_contentful_paint.TextOrImage());
  }

  const page_load_metrics::ContentfulPaintTimingInfo&
      cross_site_sub_frame_contentful_paint =
          GetDelegate()
              .GetLargestContentfulPaintHandler()
              .CrossSiteSubframesLargestContentfulPaint();
  if (cross_site_sub_frame_contentful_paint.ContainsValidTime() &&
      WasStartedInForegroundOptionalEventInForeground(
          cross_site_sub_frame_contentful_paint.Time(), GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramLargestContentfulPaintCrossSiteSubFrame,
        cross_site_sub_frame_contentful_paint.Time().value());
  }

  const page_load_metrics::ContentfulPaintTimingInfo&
      all_frames_largest_contentful_paint =
          GetDelegate()
              .GetLargestContentfulPaintHandler()
              .MergeMainFrameAndSubframes();
  if (all_frames_largest_contentful_paint.ContainsValidTime()) {
    const base::TimeDelta lcp_time =
        all_frames_largest_contentful_paint.Time().value();
    if (WasStartedInForegroundOptionalEventInForeground(
            all_frames_largest_contentful_paint.Time(), GetDelegate())) {
      EmitLCPTraceEvent(lcp_time);
      PAGE_LOAD_HISTOGRAM(internal::kHistogramLargestContentfulPaint, lcp_time);

      if (content::WebContents* web_contents = GetDelegate().GetWebContents()) {
        if (content::PreloadingData* preloading_data =
                content::PreloadingData::GetForWebContents(web_contents)) {
          if (preloading_data->HasSpeculationRulesPrerender()) {
            PAGE_LOAD_HISTOGRAM(
                internal::
                    kHistogramLargestContentfulPaintSetSpeculationRulesPrerender,
                lcp_time);
          }
        }
      }
      // The pseudo metric of |kHistogramLargestContentfulPaint|. Only used to
      // assess field trial data quality.
      PAGE_LOAD_HISTOGRAM(
          "UMA.Pseudo.PageLoad.PaintTiming.NavigationToLargestContentfulPaint2",
          metrics::GetPseudoMetricsSample(lcp_time));
      UMA_HISTOGRAM_ENUMERATION(
          internal::kHistogramLargestContentfulPaintContentType,
          all_frames_largest_contentful_paint.TextOrImage());
      TRACE_EVENT_MARK_WITH_TIMESTAMP1(
          "loading", "NavStartToLargestContentfulPaint::AllFrames::UMA",
          GetDelegate().GetNavigationStart() + lcp_time, "data",
          all_frames_largest_contentful_paint.DataAsTraceValue());
    } else {
      PAGE_LOAD_HISTOGRAM(
          internal::
              kBackgroundHttpsOrDataOrFileSchemeHistogramLargestContentfulPaint,
          lcp_time);
    }
  }

  RecordNormalizedResponsivenessMetrics();
}

void UmaPageLoadMetricsObserver::RecordNormalizedResponsivenessMetrics() {
  const page_load_metrics::ResponsivenessMetricsNormalization&
      responsiveness_metrics_normalization =
          GetDelegate().GetResponsivenessMetricsNormalization();
  std::optional<page_load_metrics::mojom::UserInteractionLatency> inp =
      responsiveness_metrics_normalization.ApproximateHighPercentile();
  if (!inp.has_value()) {
    return;
  }

  UmaHistogramCustomTimes(
      internal::kHistogramWorstUserInteractionLatencyMaxEventDuration,
      responsiveness_metrics_normalization.worst_latency()
          .value()
          .interaction_latency,
      base::Milliseconds(1), base::Seconds(60), 50);
  UmaHistogramCustomTimes(
      internal::kHistogramUserInteractionLatencyHighPercentile2MaxEventDuration,
      inp->interaction_latency, base::Milliseconds(1), base::Seconds(60), 50);
  base::TimeDelta interaction_time =
      inp->interaction_time - GetDelegate().GetNavigationStart();
  UmaHistogramCustomTimes(internal::kHistogramInpTime, interaction_time,
                          base::Milliseconds(1), base::Seconds(3600), 100);
  base::UmaHistogramCounts1000(internal::kHistogramInpOffset,
                               inp->interaction_offset);
  base::UmaHistogramCounts1000(
      internal::kHistogramNumInteractions,
      responsiveness_metrics_normalization.num_user_interactions());
}

void UmaPageLoadMetricsObserver::RecordForegroundDurationHistograms(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    base::TimeTicks app_background_time) {
  std::optional<base::TimeDelta> foreground_duration =
      page_load_metrics::GetInitialForegroundDuration(GetDelegate(),
                                                      app_background_time);
  if (!foreground_duration)
    return;

  if (GetDelegate().DidCommit()) {
    PAGE_LOAD_LONG_HISTOGRAM(internal::kHistogramPageTimingForegroundDuration,
                             foreground_duration.value());
    if (timing.paint_timing->first_paint &&
        timing.paint_timing->first_paint < foreground_duration) {
      PAGE_LOAD_LONG_HISTOGRAM(
          internal::kHistogramPageTimingForegroundDurationAfterPaint,
          foreground_duration.value() -
              timing.paint_timing->first_paint.value());
      PAGE_LOAD_LONG_HISTOGRAM(
          internal::kHistogramPageTimingForegroundDurationWithPaint,
          foreground_duration.value());
    } else {
      PAGE_LOAD_LONG_HISTOGRAM(
          internal::kHistogramPageTimingForegroundDurationWithoutPaint,
          foreground_duration.value());
    }
  } else {
    PAGE_LOAD_LONG_HISTOGRAM(
        internal::kHistogramPageTimingForegroundDurationNoCommit,
        foreground_duration.value());
  }

  if (GetDelegate().GetPageEndReason() == page_load_metrics::END_FORWARD_BACK &&
      GetDelegate().GetUserInitiatedInfo().user_gesture &&
      !GetDelegate().GetUserInitiatedInfo().browser_initiated &&
      GetDelegate().GetTimeToPageEnd() <= foreground_duration) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramUserGestureNavigationToForwardBack,
                        GetDelegate().GetTimeToPageEnd().value());
  }
}

void UmaPageLoadMetricsObserver::OnCpuTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::CpuTiming& timing) {
  total_cpu_usage_ += timing.task_time;

  if (GetDelegate().GetVisibilityTracker().currently_in_foreground()) {
    foreground_cpu_usage_ += timing.task_time;
  }
}

void UmaPageLoadMetricsObserver::RecordByteAndResourceHistograms(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_GE(network_bytes_, 0);
  DCHECK_GE(cache_bytes_, 0);
  click_tracker_.RecordClickBurst(GetDelegate().GetPageUkmSourceId());
}

void UmaPageLoadMetricsObserver::RecordCpuUsageHistograms() {
  PAGE_LOAD_HISTOGRAM(internal::kHistogramPageLoadCpuTotalUsage,
                      total_cpu_usage_);
  PAGE_LOAD_HISTOGRAM(internal::kHistogramPageLoadCpuTotalUsageForegrounded,
                      foreground_cpu_usage_);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UmaPageLoadMetricsObserver::OnEnterBackForwardCache(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  UMA_HISTOGRAM_ENUMERATION(
      internal::kHistogramBackForwardCacheEvent,
      internal::PageLoadBackForwardCacheEvent::kEnterBackForwardCache);
  return PageLoadMetricsObserver::OnEnterBackForwardCache(timing);
}

void UmaPageLoadMetricsObserver::OnRestoreFromBackForwardCache(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    content::NavigationHandle* navigation_handle) {
  // This never reaches yet because OnEnterBackForwardCache returns
  // STOP_OBSERVING.
  // TODO(hajimehoshi): After changing OnEnterBackForwardCache to continue
  // observation, remove the above comment.
  UMA_HISTOGRAM_ENUMERATION(
      internal::kHistogramBackForwardCacheEvent,
      internal::PageLoadBackForwardCacheEvent::kRestoreFromBackForwardCache);
}

void UmaPageLoadMetricsObserver::OnV8MemoryChanged(
    const std::vector<page_load_metrics::MemoryUpdate>& memory_updates) {
  DCHECK(base::FeatureList::IsEnabled(features::kV8PerFrameMemoryMonitoring));

  for (const auto& update : memory_updates) {
    memory_update_received_ = true;

    content::RenderFrameHost* render_frame_host =
        content::RenderFrameHost::FromID(update.routing_id);

    if (!render_frame_host)
      continue;

    if (!render_frame_host->GetParentOrOuterDocument()) {
      // |render_frame_host| is the outermost main frame.
      main_frame_memory_usage_.UpdateUsage(update.delta_bytes);
    } else {
      aggregate_subframe_memory_usage_.UpdateUsage(update.delta_bytes);
    }

    aggregate_total_memory_usage_.UpdateUsage(update.delta_bytes);
  }
}

void UmaPageLoadMetricsObserver::RecordV8MemoryHistograms() {
  if (base::FeatureList::IsEnabled(features::kV8PerFrameMemoryMonitoring)) {
    PAGE_BYTES_HISTOGRAM(internal::kHistogramMemoryMainframe,
                         main_frame_memory_usage_.max_bytes_used());
    PAGE_BYTES_HISTOGRAM(internal::kHistogramMemorySubframeAggregate,
                         aggregate_subframe_memory_usage_.max_bytes_used());
    PAGE_BYTES_HISTOGRAM(internal::kHistogramMemoryTotal,
                         aggregate_total_memory_usage_.max_bytes_used());
  }
}

void UmaPageLoadMetricsObserver::MemoryUsage::UpdateUsage(int64_t delta_bytes) {
  current_bytes_used_ += delta_bytes;
  max_bytes_used_ = std::max(max_bytes_used_, current_bytes_used_);
}

// Perfetto trace events for page load events need to be in sync with UMA
// histogram data for metric accuracy; they are recorded together accordingly.
// Navigation ID is used to join all related trace events.
void UmaPageLoadMetricsObserver::EmitFCPTraceEvent(
    base::TimeDelta first_contentful_paint_timing) {
  const base::TimeTicks navigation_start = GetDelegate().GetNavigationStart();
  const perfetto::Track track(base::trace_event::GetNextGlobalTraceId(),
                              perfetto::ProcessTrack::Current());

  TRACE_EVENT_BEGIN(
      "loading,interactions",
      "PageLoadMetrics.NavigationToFirstContentfulPaint", track,
      navigation_start, [&](perfetto::EventContext& ctx) {
        auto* page_load_proto =
            ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                ->set_page_load();
        page_load_proto->set_url(
            GetDelegate().GetUrl().possibly_invalid_spec());
        page_load_proto->set_navigation_id(GetDelegate().GetNavigationId());
      });

  TRACE_EVENT_END("loading,interactions", track,
                  navigation_start + first_contentful_paint_timing);
}

void UmaPageLoadMetricsObserver::EmitLCPTraceEvent(
    base::TimeDelta largest_contentful_paint_timing) {
  const base::TimeTicks navigation_start = GetDelegate().GetNavigationStart();
  const perfetto::Track track(base::trace_event::GetNextGlobalTraceId(),
                              perfetto::ProcessTrack::Current());
  TRACE_EVENT_BEGIN(
      "loading,interactions",
      "PageLoadMetrics.NavigationToLargestContentfulPaint", track,
      navigation_start, [&](perfetto::EventContext& ctx) {
        auto* page_load_proto =
            ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                ->set_page_load();
        page_load_proto->set_navigation_id(GetDelegate().GetNavigationId());

        // URL is not needed here, as it will already be recorded in the FCP
        // trace event. We can join the events using the navigation id.
      });

  TRACE_EVENT_END("loading,interactions", track,
                  navigation_start + largest_contentful_paint_timing);
}

void UmaPageLoadMetricsObserver::EmitInstantTraceEvent(
    base::TimeDelta duration,
    const char event_name[]) {
  const base::TimeTicks navigation_start = GetDelegate().GetNavigationStart();
  const perfetto::Track track(kInstantPageLoadEventsTraceTrackId,
                              perfetto::ProcessTrack::Current());
  TRACE_EVENT_INSTANT(
      "loading,interactions", perfetto::StaticString{event_name}, track,
      navigation_start + duration, [&](perfetto::EventContext ctx) {
        auto* page_load_proto =
            ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                ->set_page_load();
        page_load_proto->set_navigation_id(GetDelegate().GetNavigationId());
      });
}
