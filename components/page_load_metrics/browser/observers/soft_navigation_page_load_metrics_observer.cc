// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/soft_navigation_page_load_metrics_observer.h"

#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/browser/page_load_type.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

using page_load_metrics::CalculateLCPEntropyBucket;
using page_load_metrics::ContentfulPaintTimingInfo;
using page_load_metrics::InteractionToNextPaintCalculator;
using page_load_metrics::LayoutShiftUkmValue;
using page_load_metrics::LayoutShiftUmaValue10000;
using page_load_metrics::NormalizedCLSData;
using page_load_metrics::PageLoadMetricsObserver;
using page_load_metrics::PageLoadMetricsObserverDelegate;
using page_load_metrics::PageLoadType;
using page_load_metrics::mojom::EventTiming;
using page_load_metrics::mojom::PageLoadTiming;

namespace {
std::string DebugString(SoftNavigationPageLoadMetricsObserver::State state) {
  switch (state) {
    case SoftNavigationPageLoadMetricsObserver::State::kInitial:
      return "initial";
    case SoftNavigationPageLoadMetricsObserver::State::kStarted:
      return "started";
    case SoftNavigationPageLoadMetricsObserver::State::kPrerenderStarted:
      return "prerenderStarted";
    case SoftNavigationPageLoadMetricsObserver::State::kPrerenderActivated:
      return "prerenderActivated";
    case SoftNavigationPageLoadMetricsObserver::State::kInBackForwardCache:
      return "inBackForwardCache";
    case SoftNavigationPageLoadMetricsObserver::State::
        kRestoredFromBackForwardCache:
      return "restoredFromBackForwardCache";
    case SoftNavigationPageLoadMetricsObserver::State::kComplete:
      return "complete";
  }
}

PageLoadType StateToPageLoadType(
    SoftNavigationPageLoadMetricsObserver::State state) {
  switch (state) {
    case SoftNavigationPageLoadMetricsObserver::State::kStarted:
      return PageLoadType::kPageLoad;
    case SoftNavigationPageLoadMetricsObserver::State::kPrerenderActivated:
      return PageLoadType::kPrerenderPageLoad;
    case SoftNavigationPageLoadMetricsObserver::State::
        kRestoredFromBackForwardCache:
      return PageLoadType::kHistoryNavigation;
    default:
      NOTREACHED() << "unexpected state: " << DebugString(state);
  }
}
}  // namespace

SoftNavigationPageLoadMetricsObserver::SoftNavigationPageLoadMetricsObserver() =
    default;

SoftNavigationPageLoadMetricsObserver::
    ~SoftNavigationPageLoadMetricsObserver() = default;

const char* SoftNavigationPageLoadMetricsObserver::GetObserverName() const {
  static constexpr std::string_view kName =
      "SoftNavigationPageLoadMetricsObserver";
  return kName.data();
}

PageLoadMetricsObserver::ObservePolicy
SoftNavigationPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // We only care about the outermost main frame events.
  return STOP_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
SoftNavigationPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  CHECK_EQ(state_, State::kInitial);
  state_ = State::kPrerenderStarted;
  return CONTINUE_OBSERVING;
}

void SoftNavigationPageLoadMetricsObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  CHECK_EQ(state_, State::kPrerenderStarted);
  if (GetDelegate().WasPrerenderedThenActivatedInForeground()) {
    should_record_soft_cls_ = true;
  }
  state_ = State::kPrerenderActivated;
}

PageLoadMetricsObserver::ObservePolicy
SoftNavigationPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  CHECK_EQ(state_, State::kInitial);
  if (started_in_foreground) {
    should_record_soft_cls_ = true;
  }
  state_ = State::kStarted;
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
SoftNavigationPageLoadMetricsObserver::OnEnterBackForwardCache(
    const PageLoadTiming& timing) {
  RecordSoftNavigationEventIfPending();
  should_record_soft_cls_ = false;
  state_ = State::kInBackForwardCache;
  return CONTINUE_OBSERVING;
}

void SoftNavigationPageLoadMetricsObserver::OnRestoreFromBackForwardCache(
    const PageLoadTiming& timing,
    content::NavigationHandle* navigation_handle) {
  state_ = State::kRestoredFromBackForwardCache;
  content::WebContents* web_contents = GetDelegate().GetWebContents();
  if (web_contents &&
      web_contents->GetVisibility() == content::Visibility::VISIBLE) {
    should_record_soft_cls_ = true;
  }
  pending_soft_navigation_ = false;
}

void SoftNavigationPageLoadMetricsObserver::OnComplete(
    const PageLoadTiming& timing) {
  RecordSoftNavigationEventIfPending();
  state_ = State::kComplete;
}

PageLoadMetricsObserver::ObservePolicy
SoftNavigationPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const PageLoadTiming& timing) {
  RecordSoftNavigationEventIfPending();
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
SoftNavigationPageLoadMetricsObserver::OnHidden(const PageLoadTiming& timing) {
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
SoftNavigationPageLoadMetricsObserver::OnShown() {
  should_record_soft_cls_ = true;
  return CONTINUE_OBSERVING;
}

void SoftNavigationPageLoadMetricsObserver::OnSoftNavigation() {
  // It's possible that the OnSoftNavigation event arrives late - after a page
  // lifecycle event (esp. OnEnterBackForwardCache) that would have ended soft
  // navigation recording, because detected navigations are sent with page load
  // metrics with buffering from the renderer. At that point we're no longer in
  // a good position to record the soft navigation, so we ignore it, as flipping
  // pending_soft_navigation_ to true could cause crashes when the page (later)
  // gets evicted from the back-forward cache or the app moves to the
  // background.  See also crbug.com/513856242 and crbug.com/513789479.
  if (state_ == State::kStarted || state_ == State::kPrerenderActivated ||
      state_ == State::kRestoredFromBackForwardCache) {
    // Emit the previous soft navigation, and note the next one as pending.
    RecordSoftNavigationEventIfPending();
    pending_soft_navigation_ = true;
  }
}

bool SoftNavigationPageLoadMetricsObserver::
    FromForegroundOptionalEventInForeground(
        const std::optional<base::TimeDelta>& event) {
  // TODO(crbug.com/7817946): We may want to revise this logic so that soft LCP
  // for soft navs can be counted even when the (hard) navigation was started in
  // the background.
  const PageLoadMetricsObserverDelegate& delegate = GetDelegate();
  if (state_ == State::kStarted) {
    return WasStartedInForegroundOptionalEventInForeground(event, delegate);
  } else if (state_ == State::kPrerenderActivated) {
    return WasActivatedInForegroundOptionalEventInForeground(event, delegate);
  } else if (state_ == State::kRestoredFromBackForwardCache) {
    // The index for the most recent bfcache restore is # bfcache restores - 1.
    size_t num_bfcache_restores = delegate.GetNumBackForwardCacheRestores();
    CHECK_NE(0u, num_bfcache_restores);
    return WasStartedInForegroundOptionalEventInForegroundAfterBackForwardCacheRestore(
        event, delegate, num_bfcache_restores - 1);
  }
  return false;
}

void SoftNavigationPageLoadMetricsObserver::
    RecordSoftNavigationEventIfPending() {
  if (!pending_soft_navigation_) {
    return;
  }
  pending_soft_navigation_ = false;
  const auto& soft_navigation_metrics =
      GetDelegate().GetSoftNavigationMetrics();
  ukm::SourceId ukm_source_id =
      GetDelegate().GetUkmSourceIdForSameDocumentNavigation(
          soft_navigation_metrics.same_document_metrics_token);
  if (ukm_source_id == ukm::kInvalidSourceId) {
    return;
  }
  ukm::builders::SoftNavigation builder(ukm_source_id);

  builder.SetStartTime(soft_navigation_metrics.start_time.InMillisecondsF());
  PAGE_LOAD_HISTOGRAM("PageLoad.SoftNavigation.StartTime",
                      soft_navigation_metrics.start_time);
  builder.SetNavigationType(
      static_cast<int>(soft_navigation_metrics.navigation_type));
  builder.SetPageLoadType(static_cast<int>(StateToPageLoadType(state_)));

  RecordSoftLcp(builder);
  RecordSoftInp(builder);
  RecordSoftCls(builder);
  builder.Record(ukm::UkmRecorder::Get());
}

void SoftNavigationPageLoadMetricsObserver::RecordSoftLcp(
    ukm::builders::SoftNavigation& builder) {
  // All loading performance timings within the soft LCP object are relative to
  // the (hard) navigation start. Therefore, when we record the metric values
  // for the soft navigation's LCP below, we need to subtract the soft
  // navigation's start time (which is also relative to the (hard) navigation
  // start) from these values.
  const auto& largest_contentful_paint =
      GetDelegate().GetSoftNavigationLargestContentfulPaint();
  const auto& soft_navigation_metrics =
      GetDelegate().GetSoftNavigationMetrics();
  if (largest_contentful_paint.ContainsValidTime() &&
      FromForegroundOptionalEventInForeground(
          largest_contentful_paint.Time())) {
    base::TimeDelta soft_lcp = (largest_contentful_paint.Time().value() -
                                soft_navigation_metrics.start_time);
    builder.SetPaintTiming_LargestContentfulPaint(soft_lcp.InMilliseconds());
    PAGE_LOAD_HISTOGRAM("PageLoad.SoftNavigation.LargestContentfulPaint",
                        soft_lcp);

    builder.SetPaintTiming_LargestContentfulPaintType(
        LargestContentfulPaintTypeToUKMFlags(largest_contentful_paint.Type()));

    if (largest_contentful_paint.TextOrImage() ==
        ContentfulPaintTimingInfo::LargestContentTextOrImage::kImage) {
      builder.SetPaintTiming_LargestContentfulPaintBPP(
          CalculateLCPEntropyBucket(largest_contentful_paint.ImageBPP()));

      auto priority = largest_contentful_paint.ImageRequestPriority();
      if (priority.has_value()) {
        builder.SetPaintTiming_LargestContentfulPaintRequestPriority(
            priority.value());
      }

      if (largest_contentful_paint.ImageDiscoveryTime().has_value()) {
        builder.SetPaintTiming_LargestContentfulPaintImageDiscoveryTime(
            (largest_contentful_paint.ImageDiscoveryTime().value() -
             soft_navigation_metrics.start_time)
                .InMilliseconds());
      }

      if (largest_contentful_paint.ImageLoadStart().has_value()) {
        builder.SetPaintTiming_LargestContentfulPaintImageLoadStart(
            (largest_contentful_paint.ImageLoadStart().value() -
             soft_navigation_metrics.start_time)
                .InMilliseconds());
      }

      if (largest_contentful_paint.ImageLoadEnd().has_value()) {
        builder.SetPaintTiming_LargestContentfulPaintImageLoadEnd(
            (largest_contentful_paint.ImageLoadEnd().value() -
             soft_navigation_metrics.start_time)
                .InMilliseconds());
      }
    }
  }
}

void SoftNavigationPageLoadMetricsObserver::RecordSoftInp(
    ukm::builders::SoftNavigation& builder) {
  const InteractionToNextPaintCalculator&
      soft_nav_interaction_to_next_paint_calculator =
          GetDelegate()
              .GetSoftNavigationIntervalInteractionToNextPaintCalculator();
  std::optional<InteractionToNextPaintCalculator::InteractionData> inp_data =
      soft_nav_interaction_to_next_paint_calculator.ApproximateHighPercentile();
  if (inp_data.has_value()) {
    const EventTiming& inp = inp_data->max_event;
    builder
        .SetInteractiveTiming_UserInteractionLatency_HighPercentile2_MaxEventDuration(
            inp.duration.InMilliseconds());

    UmaHistogramCustomTimes("PageLoad.SoftNavigation.InteractionToNextPaint",
                            inp.duration, base::Milliseconds(1),
                            base::Seconds(60), 50);

    // For soft navigations, the interaction offset is the offset _after_ the
    // soft navigation occurred.
    builder.SetInteractiveTiming_INPOffset(inp_data->interaction_offset);
    // For soft navigations, the interaction time should be reported as the
    // TimeDelta between the interaction and the soft navigation start. Since
    // the interaction time is a TimeTicks and the soft navigation start_time is
    // a TimeDelta from navigation_start, we need to add the navigation start
    // TimeTicks to the soft_navigation start_time TimeDelta and then subtract
    // that from the interaction_time TimeTicks.
    base::TimeDelta interaction_time =
        inp.start_time - (GetDelegate().GetNavigationStart() +
                          GetDelegate().GetSoftNavigationMetrics().start_time);
    builder.SetInteractiveTiming_INPTime(interaction_time.InMilliseconds());
    builder.SetInteractiveTiming_NumInteractions(
        ukm::GetExponentialBucketMinForCounts1000(
            soft_nav_interaction_to_next_paint_calculator
                .num_user_interactions()));
  }
}

void SoftNavigationPageLoadMetricsObserver::RecordSoftCls(
    ukm::builders::SoftNavigation& builder) {
  // Don't report CLS if we were never in the foreground.
  if (!should_record_soft_cls_) {
    return;
  }
  const NormalizedCLSData& normalized_cls =
      GetDelegate().GetSoftNavigationIntervalNormalizedCLSData();
  if (normalized_cls.data_tainted) {
    return;
  }
  const float cls = normalized_cls.session_windows_gap1000ms_max5000ms_max_cls;
  builder
      .SetLayoutInstability_MaxCumulativeShiftScore_SessionWindow_Gap1000ms_Max5000ms(
          LayoutShiftUkmValue(cls));
  // Report UMA using same binning as all WebVitals.CumulativeLayoutShift
  // histograms; the binning ensures changes close to zero can accurately
  // be measured.
  base::UmaHistogramCustomCounts(
      "PageLoad.SoftNavigation.CumulativeLayoutShift",
      LayoutShiftUmaValue10000(cls), 1, 24000, 50);
}
