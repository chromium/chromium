// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_tracker.h"

#include <algorithm>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "components/page_load_metrics/browser/features.h"
#include "components/page_load_metrics/browser/observers/assert_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_embedder_interface.h"
#include "components/page_load_metrics/browser/page_load_metrics_forward_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_memory_tracker.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_discard_reason.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "page_load_metrics_observer_delegate.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/input/input_event.mojom-shared.h"

namespace page_load_metrics {

namespace internal {

const char kErrorEvents[] = "PageLoad.Internal.ErrorCode";
const char kPageLoadPrerender2Event[] = "PageLoad.Internal.Prerender2.Event";
const char kPageLoadTrackerPageType[] = "PageLoad.Internal.PageType";
}  // namespace internal

void RecordInternalError(InternalErrorLoadEvent event) {
  base::UmaHistogramEnumeration(internal::kErrorEvents, event, ERR_LAST_ENTRY);
}

void RecordPageType(internal::PageLoadTrackerPageType type) {
  base::UmaHistogramEnumeration(internal::kPageLoadTrackerPageType, type);
}

// TODO(csharrison): Add a case for client side redirects, which is what JS
// initiated window.location / window.history navigations get set to.
PageEndReason EndReasonForPageTransition(ui::PageTransition transition) {
  if (transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT) {
    return END_CLIENT_REDIRECT;
  }
  // Check for forward/back navigations first since there are forward/back
  // navigations that haved PAGE_TRANSITION_RELOAD but are not user reloads
  // (pull-to-refresh or preview opt-out).
  if (transition & ui::PAGE_TRANSITION_FORWARD_BACK) {
    return END_FORWARD_BACK;
  }
  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD)) {
    return END_RELOAD;
  }
  if (ui::PageTransitionIsNewNavigation(transition)) {
    return END_NEW_NAVIGATION;
  }
  NOTREACHED_IN_MIGRATION()
      << "EndReasonForPageTransition received unexpected ui::PageTransition: "
      << transition;
  return END_OTHER;
}

bool IsNavigationUserInitiated(content::NavigationHandle* handle) {
  // TODO(crbug.com/41257523): Browser initiated navigations should have
  // HasUserGesture() set to true. In the meantime, we consider all
  // browser-initiated navigations to be user initiated.
  //
  // TODO(crbug.com/40480474): Some browser-initiated navigations incorrectly
  // report that they are renderer-initiated. We will currently report that
  // these navigations are not user initiated, when in fact they are user
  // initiated.
  return handle->HasUserGesture() || !handle->IsRendererInitiated();
}

namespace {

void DispatchEventsAfterBackForwardCacheRestore(
    PageLoadMetricsObserverInterface* observer,
    const std::vector<mojo::StructPtr<mojom::BackForwardCacheTiming>>&
        last_timings,
    const std::vector<mojo::StructPtr<mojom::BackForwardCacheTiming>>&
        new_timings) {
  if (new_timings.size() < last_timings.size()) {
    mojo::ReportBadMessage(base::StringPrintf(
        "`new_timings.size()` (%zu) must be equal to or greater than "
        "`last_timings.size()` (%zu) but is not",
        new_timings.size(), last_timings.size()));
    return;
  }

  for (size_t i = 0; i < new_timings.size(); i++) {
    auto first_paint =
        new_timings[i]->first_paint_after_back_forward_cache_restore;
    if (!first_paint.is_zero() &&
        (i >= last_timings.size() ||
         last_timings[i]
             ->first_paint_after_back_forward_cache_restore.is_zero())) {
      observer->OnFirstPaintAfterBackForwardCacheRestoreInPage(*new_timings[i],
                                                               i);
    }

    auto request_animation_frames =
        new_timings[i]
            ->request_animation_frames_after_back_forward_cache_restore;
    if (request_animation_frames.size() == 3 &&
        (i >= last_timings.size() ||
         last_timings[i]
             ->request_animation_frames_after_back_forward_cache_restore
             .empty())) {
      observer->OnRequestAnimationFramesAfterBackForwardCacheRestoreInPage(
          *new_timings[i], i);
    }

    auto first_input_delay =
        new_timings[i]->first_input_delay_after_back_forward_cache_restore;
    if (first_input_delay.has_value() &&
        (i >= last_timings.size() ||
         !last_timings[i]
              ->first_input_delay_after_back_forward_cache_restore
              .has_value())) {
      observer->OnFirstInputAfterBackForwardCacheRestoreInPage(*new_timings[i],
                                                               i);
    }
  }
}

void DispatchObserverTimingCallbacks(PageLoadMetricsObserverInterface* observer,
                                     const mojom::PageLoadTiming& last_timing,
                                     const mojom::PageLoadTiming& new_timing) {
  if (!last_timing.Equals(new_timing)) {
    observer->OnTimingUpdate(nullptr, new_timing);
  }
  if (new_timing.document_timing->dom_content_loaded_event_start &&
      !last_timing.document_timing->dom_content_loaded_event_start) {
    observer->OnDomContentLoadedEventStart(new_timing);
  }
  if (new_timing.document_timing->load_event_start &&
      !last_timing.document_timing->load_event_start) {
    observer->OnLoadEventStart(new_timing);
  }
  if (new_timing.interactive_timing->first_input_delay &&
      !last_timing.interactive_timing->first_input_delay) {
    observer->OnFirstInputInPage(new_timing);
  }
  if (new_timing.paint_timing->first_paint &&
      !last_timing.paint_timing->first_paint) {
    observer->OnFirstPaintInPage(new_timing);
  }
  DispatchEventsAfterBackForwardCacheRestore(
      observer, last_timing.back_forward_cache_timings,
      new_timing.back_forward_cache_timings);
  if (new_timing.paint_timing->first_image_paint &&
      !last_timing.paint_timing->first_image_paint) {
    observer->OnFirstImagePaintInPage(new_timing);
  }
  if (new_timing.paint_timing->first_contentful_paint &&
      !last_timing.paint_timing->first_contentful_paint) {
    observer->OnFirstContentfulPaintInPage(new_timing);
  }
  if (new_timing.paint_timing->first_meaningful_paint &&
      !last_timing.paint_timing->first_meaningful_paint) {
    observer->OnFirstMeaningfulPaintInMainFrameDocument(new_timing);
  }
  if (new_timing.parse_timing->parse_start &&
      !last_timing.parse_timing->parse_start) {
    observer->OnParseStart(new_timing);
  }
  if (new_timing.parse_timing->parse_stop &&
      !last_timing.parse_timing->parse_stop) {
    observer->OnParseStop(new_timing);
  }
  if (new_timing.domain_lookup_timing->domain_lookup_start &&
      !last_timing.domain_lookup_timing->domain_lookup_start) {
    observer->OnDomainLookupStart(new_timing);
  }
  if (new_timing.domain_lookup_timing->domain_lookup_end &&
      !last_timing.domain_lookup_timing->domain_lookup_end) {
    observer->OnDomainLookupEnd(new_timing);
  }
  if (new_timing.connect_start && !last_timing.connect_start) {
    observer->OnConnectStart(new_timing);
  }
}

internal::PageLoadTrackerPageType CalculatePageType(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrerenderedMainFrame()) {
    return internal::PageLoadTrackerPageType::kPrerenderPage;
  } else if (navigation_handle->GetNavigatingFrameType() ==
             content::FrameType::kFencedFrameRoot) {
    return internal::PageLoadTrackerPageType::kFencedFramesPage;
  }
  return navigation_handle->GetWebContents()->IsInPreviewMode()
             ? internal::PageLoadTrackerPageType::kPreviewPrimaryPage
             : internal::PageLoadTrackerPageType::kPrimaryPage;
}

bool CalculateIsOriginVisit(bool is_first_navigation,
                            ui::PageTransition transition) {
  if (is_first_navigation) {
    return true;
  }
  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_LINK) ||
      ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD)) {
    return false;
  }
  return true;
}

void RegisterObservers(PageLoadTracker* tracker,
                       PageLoadMetricsEmbedderInterface* embedder,
                       content::NavigationHandle* navigation_handle) {
#if DCHECK_IS_ON()
  // Link Preview doesn't emit activation event yet and assertion of event
  // orders fail.
  //
  // TODO(b:302999778): Reenable it.
  if (!tracker->GetWebContents()->IsInPreviewMode()) {
    tracker->AddObserver(std::make_unique<AssertPageLoadMetricsObserver>());
  }
#endif
  embedder->RegisterObservers(tracker, navigation_handle);
}

}  // namespace

PageLoadTracker::PageLoadTracker(
    bool in_foreground,
    PageLoadMetricsEmbedderInterface* embedder_interface,
    const GURL& currently_committed_url,
    bool is_first_navigation_in_web_contents,
    content::NavigationHandle* navigation_handle,
    UserInitiatedInfo user_initiated_info,
    ukm::SourceId source_id,
    base::WeakPtr<PageLoadTracker> parent_tracker)
    : did_stop_tracking_(false),
      navigation_id_(navigation_handle->GetNavigationId()),
      navigation_start_(navigation_handle->NavigationStart()),
      url_(navigation_handle->GetURL()),
      start_url_(navigation_handle->GetURL()),
      visibility_tracker_(base::DefaultTickClock::GetInstance(), in_foreground),
      did_commit_(false),
      page_end_reason_(END_NONE),
      page_end_user_initiated_info_(UserInitiatedInfo::NotUserInitiated()),
      started_in_foreground_(in_foreground),
      last_dispatched_merged_page_timing_(CreatePageLoadTiming()),
      user_initiated_info_(user_initiated_info),
      embedder_interface_(embedder_interface),
      metrics_update_dispatcher_(this, navigation_handle, embedder_interface),
      source_id_(source_id),
      web_contents_(navigation_handle->GetWebContents()),
      is_first_navigation_in_web_contents_(is_first_navigation_in_web_contents),
      is_origin_visit_(
          CalculateIsOriginVisit(is_first_navigation_in_web_contents,
                                 navigation_handle->GetPageTransition())),
      soft_navigation_metrics_(CreateSoftNavigationMetrics()),
      page_type_(CalculatePageType(navigation_handle)),
      parent_tracker_(std::move(parent_tracker)) {
  DCHECK(!navigation_handle->HasCommitted());
  RegisterObservers(this, embedder_interface, navigation_handle);
  switch (page_type_) {
    case internal::PageLoadTrackerPageType::kPrimaryPage:
      CHECK_NE(ukm::kInvalidSourceId, source_id_);
      InvokeAndPruneObservers(
          "PageLoadMetricsObserver::OnStart",
          base::BindRepeating(
              [](content::NavigationHandle* navigation_handle,
                 const GURL& currently_committed_url,
                 bool started_in_foreground,
                 PageLoadMetricsObserverInterface* observer) {
                return observer->OnStart(navigation_handle,
                                         currently_committed_url,
                                         started_in_foreground);
              },
              navigation_handle, currently_committed_url,
              started_in_foreground_),
          /*permit_forwarding=*/false);
      break;
    case internal::PageLoadTrackerPageType::kPrerenderPage:
      CHECK(!started_in_foreground_);
      CHECK_EQ(ukm::kInvalidSourceId, source_id_);
      prerendering_state_ = PrerenderingState::kInPrerendering;
      InvokeAndPruneObservers(
          "PageLoadMetricsObserver::OnPrerenderStart",
          base::BindRepeating(
              [](content::NavigationHandle* navigation_handle,
                 const GURL& currently_committed_url,
                 PageLoadMetricsObserverInterface* observer) {
                return observer->OnPrerenderStart(navigation_handle,
                                                  currently_committed_url);
              },
              navigation_handle, currently_committed_url),
          /*permit_forwarding=*/false);
      base::UmaHistogramEnumeration(
          internal::kPageLoadPrerender2Event,
          internal::PageLoadPrerenderEvent::kNavigationInPrerenderedMainFrame);
      break;
    case internal::PageLoadTrackerPageType::kFencedFramesPage:
      CHECK_NE(ukm::kInvalidSourceId, source_id_);
      InvokeAndPruneObservers(
          "PageLoadMetricsObserver::OnFencedFramesStart",
          base::BindRepeating(
              [](content::NavigationHandle* navigation_handle,
                 const GURL& currently_committed_url,
                 PageLoadMetricsObserverInterface* observer) {
                return observer->OnFencedFramesStart(navigation_handle,
                                                     currently_committed_url);
              },
              navigation_handle, currently_committed_url),
          /*permit_forwarding=*/true);
      break;
    case internal::PageLoadTrackerPageType::kPreviewPrimaryPage:
      CHECK_NE(ukm::kInvalidSourceId, source_id_);
      prerendering_state_ = PrerenderingState::kInPreview;
      InvokeAndPruneObservers(
          "PageLoadMetricsObserver::OnPreviewStart",
          base::BindRepeating(
              [](content::NavigationHandle* navigation_handle,
                 const GURL& currently_committed_url,
                 PageLoadMetricsObserverInterface* observer) {
                return observer->OnPreviewStart(navigation_handle,
                                                currently_committed_url);
              },
              navigation_handle, currently_committed_url),
          /*permit_forwarding=*/false);
      break;
  }
  RecordPageType(page_type_);
}

PageLoadTracker::~PageLoadTracker() {
  if (did_stop_tracking_) {
    return;
  }

  metrics_update_dispatcher_.ShutDown();

  if (page_end_time_.is_null()) {
    // page_end_time_ can be unset in some cases, such as when a navigation is
    // aborted by a navigation that started before it. In these cases, set the
    // end time to the current time.
    RecordInternalError(ERR_NO_PAGE_LOAD_END_TIME);
    NotifyPageEnd(END_OTHER, UserInitiatedInfo::NotUserInitiated(),
                  base::TimeTicks::Now(), true);
  }

  if (!did_commit_) {
    if (!failed_provisional_load_info_) {
      RecordInternalError(ERR_NO_COMMIT_OR_FAILED_PROVISIONAL_LOAD);
    }
  } else if (page_load_metrics::IsEmpty(metrics_update_dispatcher_.timing())) {
    RecordInternalError(ERR_NO_IPCS_RECEIVED);
  }

  for (const auto& observer : observers_) {
    if (failed_provisional_load_info_) {
      observer->OnFailedProvisionalLoad(*failed_provisional_load_info_);
    } else {
      DCHECK(did_commit_);
      observer->OnComplete(metrics_update_dispatcher_.timing());
    }
  }
}

void PageLoadTracker::PageHidden() {
  // Only log the first time we background in a given page load.
  if (!first_background_time_.has_value() ||
      (!back_forward_cache_restores_.empty() &&
       !back_forward_cache_restores_.back()
            .first_background_time.has_value())) {
    // The possible visibility state transitions and events are:
    //
    // A. navigation_start (fg) -> first background (bg)
    //    -> first foreground (fg)
    // B. navigation_start (bg) -> first foreground (fg)
    //    -> first background (bg)
    // C. navigation_start (bg) -> activation_start (fg)
    //    -> first background (bg) -> first foreground (fg)
    //
    // where fg = foreground, bg = background. A and B are non prerendered and C
    // is prerendered.
    //
    // PageShown and PgaeHidden are not called for navigation_start and
    // actiivation_start; called when the visibility is changed after
    // navigation_start (resp. activation_start) for non prerendered (resp.
    // prerendered) pages.
    //
    // Here we check that the first background follows some event in foreground.
    if (!first_background_time_.has_value()) {
      if (prerendering_state_ == PrerenderingState::kNoPrerendering ||
          prerendering_state_ == PrerenderingState::kInPreview) {
        DCHECK_EQ(!started_in_foreground_, first_foreground_time_.has_value());
      } else {
        DCHECK(!first_foreground_time_.has_value());
      }
    }

    base::TimeTicks background_time = base::TimeTicks::Now();
    ClampBrowserTimestampIfInterProcessTimeTickSkew(&background_time);
    DCHECK_GE(background_time, navigation_start_);

    if (!first_background_time_.has_value()) {
      first_background_time_ = background_time;
    }

    if (!back_forward_cache_restores_.empty() &&
        !back_forward_cache_restores_.back()
             .first_background_time.has_value()) {
      back_forward_cache_restores_.back().first_background_time =
          background_time -
          back_forward_cache_restores_.back().navigation_start_time;
    }
  }
  visibility_tracker_.OnHidden();
  InvokeAndPruneObservers("PageLoadMetricsObserver::OnHidden",
                          base::BindRepeating(
                              [](const mojom::PageLoadTiming* timing,
                                 PageLoadMetricsObserverInterface* observer) {
                                return observer->OnHidden(*timing);
                              },
                              &metrics_update_dispatcher_.timing()),
                          /*permit_forwarding=*/false);
}

void PageLoadTracker::PageShown() {
  // Only log the first time we foreground in a given page load.
  if (!first_foreground_time_.has_value()) {
    // See comment about visibility state transitions in PageHidden.
    //
    // Here we check that the first foreground follows some event in background.
    if (prerendering_state_ == PrerenderingState::kNoPrerendering ||
        prerendering_state_ == PrerenderingState::kInPreview) {
      DCHECK_EQ(started_in_foreground_, first_background_time_.has_value());
    } else {
      DCHECK(first_background_time_.has_value());
    }

    base::TimeTicks foreground_time = base::TimeTicks::Now();
    ClampBrowserTimestampIfInterProcessTimeTickSkew(&foreground_time);
    DCHECK_GE(foreground_time, navigation_start_);
    first_foreground_time_ = foreground_time;
  }

  visibility_tracker_.OnShown();
  InvokeAndPruneObservers(
      "PageLoadMetricsObserver::OnShown",
      base::BindRepeating([](PageLoadMetricsObserverInterface* observer) {
        return observer->OnShown();
      }),
      /*permit_forwarding=*/false);
}

void PageLoadTracker::RenderFrameDeleted(content::RenderFrameHost* rfh) {
  if (parent_tracker_) {
    // Notify the parent of a deletion of RenderFrameHost of a subframe.
    parent_tracker_->RenderFrameDeleted(rfh);
  }

  for (const auto& observer : observers_) {
    observer->OnRenderFrameDeleted(rfh);
  }
}

void PageLoadTracker::FrameTreeNodeDeleted(
    content::FrameTreeNodeId frame_tree_node_id) {
  if (parent_tracker_) {
    // Notify the parent of a deletion of FrameTreeNode of a subframe.
    //
    // Note that deletion of main frames are forwarded by
    // MetrcisWebContentsObserver.
    parent_tracker_->FrameTreeNodeDeleted(frame_tree_node_id);
  }

  metrics_update_dispatcher_.OnSubFrameDeleted(frame_tree_node_id);
  largest_contentful_paint_handler_.OnSubFrameDeleted(frame_tree_node_id);
  for (const auto& observer : observers_) {
    observer->OnSubFrameDeleted(frame_tree_node_id);
  }
}

void PageLoadTracker::WillProcessNavigationResponse(
    content::NavigationHandle* navigation_handle) {
  DCHECK(!navigation_request_id_.has_value());
  navigation_request_id_ = navigation_handle->GetGlobalRequestID();
}

void PageLoadTracker::Commit(content::NavigationHandle* navigation_handle) {
  // We don't deliver OnCommit() for activation. Prerendered pages will see
  // DidActivatePrerenderedPage() instead.
  // Event records below are also not needed as we did them for the initial
  // navigation on starting prerendering.
  DCHECK(!navigation_handle->IsPrerenderedPageActivation());

  if (parent_tracker_) {
    // Notify the parent of the inner main frame navigation as a sub-frame
    // navigation.
    parent_tracker_->DidFinishSubFrameNavigation(navigation_handle);
  } else if (navigation_handle->IsPrerenderedPageActivation()) {
    NOTREACHED_IN_MIGRATION();
    // We don't deliver OnCommit() for activation. Prerendered pages will see
    // DidActivatePrerenderedPage() instead.
    // Event records below are also not needed as we did them for the initial
    // navigation on starting prerendering.
    return;
  }

  did_commit_ = true;
  url_ = navigation_handle->GetURL();
  // Some transitions (like CLIENT_REDIRECT) are only known at commit time.
  user_initiated_info_.user_gesture = navigation_handle->HasUserGesture();

  if (navigation_handle->IsInMainFrame()) {
    largest_contentful_paint_handler_.RecordMainFrameTreeNodeId(
        navigation_handle->GetFrameTreeNodeId());
    experimental_largest_contentful_paint_handler_.RecordMainFrameTreeNodeId(
        navigation_handle->GetFrameTreeNodeId());
  }
  InvokeAndPruneObservers(
      "PageLoadMetricsObserver::ShouldObserveMimeType",
      base::BindRepeating(
          [](const std::string& mime_type,
             PageLoadMetricsObserverInterface* observer) {
            return observer->ShouldObserveMimeType(mime_type);
          },
          // Query with the outermost page's MIME type so that we can ask each
          // observer with information for the page they are interested in.
          navigation_handle->GetRenderFrameHost()
              ->GetOutermostMainFrameOrEmbedder()
              ->GetPage()
              .GetContentsMimeType()),
      /*permit_forwarding=*/false);
  InvokeAndPruneObservers(
      "PageLoadMetricsObserver::ShouldObserveScheme",
      base::BindRepeating(
          [](const GURL& url, PageLoadMetricsObserverInterface* observer) {
            return observer->ShouldObserveScheme(url);
          },
          navigation_handle->GetURL()),
      /*permit_forwarding=*/false);
  InvokeAndPruneObservers("PageLoadMetricsObserver::OnCommit",
                          base::BindRepeating(
                              [](content::NavigationHandle* navigation_handle,
                                 PageLoadMetricsObserverInterface* observer) {
                                return observer->OnCommit(navigation_handle);
                              },
                              navigation_handle),
                          /*permit_forwarding=*/false);
}

void PageLoadTracker::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  DCHECK_EQ(prerendering_state_, PrerenderingState::kInPrerendering);

  prerendering_state_ = PrerenderingState::kActivatedNoActivationStart;
  source_id_ = ukm::ConvertToSourceId(navigation_handle->GetNavigationId(),
                                      ukm::SourceIdType::NAVIGATION_ID);

  switch (GetWebContents()->GetVisibility()) {
    case content::Visibility::HIDDEN:
    case content::Visibility::OCCLUDED:
      visibility_at_activation_ = PageVisibility::kBackground;
      break;
    case content::Visibility::VISIBLE:
      visibility_at_activation_ = PageVisibility::kForeground;
      break;
  }

  for (const auto& observer : observers_) {
    observer->DidActivatePrerenderedPage(navigation_handle);
  }

  base::UmaHistogramEnumeration(
      internal::kPageLoadPrerender2Event,
      internal::PageLoadPrerenderEvent::kPrerenderActivationNavigation);
}

void PageLoadTracker::DidActivatePreviewedPage(
    base::TimeTicks activation_time) {
  CHECK_EQ(prerendering_state_, PrerenderingState::kInPreview);
  prerendering_state_ = PrerenderingState::kNoPrerendering;

  // We don't keep `activation_time` as `activation_start_` because we measure
  // preview mode performance as navigation originated rather than activation.

  for (const auto& observer : observers_) {
    observer->DidActivatePreviewedPage(activation_time);
  }
}

void PageLoadTracker::DidCommitSameDocumentNavigation(
    content::NavigationHandle* navigation_handle) {
  if (parent_tracker_) {
    // Notify the parent of the inner main frame navigation as a sub-frame
    // navigation.
    parent_tracker_->DidFinishSubFrameNavigation(navigation_handle);
  }

  // Update soft navigation URL and UKM source id;
  // A same-document navigation may not be a soft navigation. But when a soft
  // navigation updates comes in later, the URL and source id updated here would
  // correspond to that soft navigation.
  if (navigation_handle->IsInMainFrame()) {
    previous_soft_navigation_source_id_ = potential_soft_navigation_source_id_;
    potential_soft_navigation_source_id_ =
        ukm::ConvertToSourceId(navigation_handle->GetNavigationId(),
                               ukm::SourceIdObj::Type::NAVIGATION_ID);
  }

  for (const auto& observer : observers_) {
    observer->OnCommitSameDocumentNavigation(navigation_handle);
  }
}

void PageLoadTracker::DidInternalNavigationAbort(
    content::NavigationHandle* navigation_handle) {
  if (parent_tracker_) {
    // Notify the parent of the inner main frame navigation as a sub-frame
    // navigation.
    parent_tracker_->DidFinishSubFrameNavigation(navigation_handle);
  }

  for (const auto& observer : observers_) {
    observer->OnDidInternalNavigationAbort(navigation_handle);
  }
}

void PageLoadTracker::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  // Don't notify the parent as inner main frame's events are converted to
  // sub-frames events for the parent, but this event is only for the main
  // frame.

  for (const auto& observer : observers_) {
    observer->ReadyToCommitNextNavigation(navigation_handle);
  }
}

void PageLoadTracker::DidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  if (parent_tracker_) {
    // Notify the parent of inner frame navigations.
    parent_tracker_->DidFinishSubFrameNavigation(navigation_handle);
  }
  metrics_update_dispatcher_.DidFinishSubFrameNavigation(navigation_handle);
  largest_contentful_paint_handler_.OnDidFinishSubFrameNavigation(
      navigation_handle, navigation_start_);
  experimental_largest_contentful_paint_handler_.OnDidFinishSubFrameNavigation(
      navigation_handle, navigation_start_);
  for (const auto& observer : observers_) {
    observer->OnDidFinishSubFrameNavigation(navigation_handle);
  }
}

void PageLoadTracker::FailedProvisionalLoad(
    content::NavigationHandle* navigation_handle,
    base::TimeTicks failed_load_time) {
  DCHECK(!failed_provisional_load_info_);
  if (parent_tracker_) {
    // Notify the parent of the inner main frame navigation as a sub-frame
    // navigation.
    parent_tracker_->DidFinishSubFrameNavigation(navigation_handle);
  }
  CHECK(navigation_handle->GetNavigationDiscardReason().has_value());
  failed_provisional_load_info_ = std::make_unique<FailedProvisionalLoadInfo>(
      failed_load_time - navigation_handle->NavigationStart(),
      navigation_handle->GetNetErrorCode(),
      navigation_handle->GetNavigationDiscardReason().value());
}

void PageLoadTracker::DidUpdateNavigationHandleTiming(
    content::NavigationHandle* navigation_handle) {
  InvokeAndPruneObservers(
      "PageLoadMetricsObserver::OnNavigationHandleTimingUpdated",
      base::BindRepeating(
          [](content::NavigationHandle* navigation_handle,
             PageLoadMetricsObserverInterface* observer) {
            return observer->OnNavigationHandleTimingUpdated(navigation_handle);
          },
          navigation_handle),
      /*permit_forwarding=*/false);
}

void PageLoadTracker::Redirect(content::NavigationHandle* navigation_handle) {
  url_ = navigation_handle->GetURL();
  InvokeAndPruneObservers("PageLoadMetricsObserver::Redirect",
                          base::BindRepeating(
                              [](content::NavigationHandle* navigation_handle,
                                 PageLoadMetricsObserverInterface* observer) {
                                return observer->OnRedirect(navigation_handle);
                              },
                              navigation_handle),
                          /*permit_forwarding=*/false);
}

void PageLoadTracker::OnInputEvent(const blink::WebInputEvent& event) {
  static const bool do_not_send_continuous_events =
      !base::FeatureList::IsEnabled(
          features::kSendContinuousInputEventsToObservers);
  using Type = blink::mojom::EventType;
  const bool is_continuous_event =
      (event.GetType() == Type::kTouchMove ||
       event.GetType() == Type::kGestureScrollUpdate ||
       event.GetType() == Type::kGesturePinchUpdate);
  if (do_not_send_continuous_events && is_continuous_event) {
    return;
  }

  // TODO(b/328601354): Confirm continuous input events are not required for
  // page load tracker observers and rename the API to reflect the same.
  for (const auto& observer : observers_) {
    observer->OnUserInput(event, metrics_update_dispatcher_.timing());
  }
}

void PageLoadTracker::FlushMetricsOnAppEnterBackground() {
  metrics_update_dispatcher()->FlushPendingTimingUpdates();

  InvokeAndPruneObservers(
      "PageLoadMetricsObserver::FlushMetricsOnAppEnterBackground",
      base::BindRepeating(
          [](const mojom::PageLoadTiming* timing,
             PageLoadMetricsObserverInterface* observer) {
            return observer->FlushMetricsOnAppEnterBackground(*timing);
          },
          &metrics_update_dispatcher_.timing()),
      /*permit_forwarding=*/false);
}

void PageLoadTracker::OnLoadedResource(
    const ExtraRequestCompleteInfo& extra_request_complete_info) {
  for (const auto& observer : observers_) {
    observer->OnLoadedResource(extra_request_complete_info);
  }
}

void PageLoadTracker::FrameReceivedUserActivation(
    content::RenderFrameHost* rfh) {
  for (const auto& observer : observers_) {
    observer->FrameReceivedUserActivation(rfh);
  }
}

void PageLoadTracker::FrameDisplayStateChanged(
    content::RenderFrameHost* render_frame_host,
    bool is_display_none) {
  for (const auto& observer : observers_) {
    observer->FrameDisplayStateChanged(render_frame_host, is_display_none);
  }
}

void PageLoadTracker::FrameSizeChanged(
    content::RenderFrameHost* render_frame_host,
    const gfx::Size& frame_size) {
  for (const auto& observer : observers_) {
    observer->FrameSizeChanged(render_frame_host, frame_size);
  }
}

void PageLoadTracker::OnCookiesRead(
    const GURL& url,
    const GURL& first_party_url,
    bool blocked_by_policy,
    bool is_ad_tagged,
    const net::CookieSettingOverrides& cookie_setting_overrides,
    bool is_partitioned_access) {
  for (const auto& observer : observers_) {
    observer->OnCookiesRead(url, first_party_url, blocked_by_policy,
                            is_ad_tagged, cookie_setting_overrides,
                            is_partitioned_access);
  }
}

void PageLoadTracker::OnCookieChange(
    const GURL& url,
    const GURL& first_party_url,
    const net::CanonicalCookie& cookie,
    bool blocked_by_policy,
    bool is_ad_tagged,
    const net::CookieSettingOverrides& cookie_setting_overrides,
    bool is_partitioned_access) {
  for (const auto& observer : observers_) {
    observer->OnCookieChange(url, first_party_url, cookie, blocked_by_policy,
                             is_ad_tagged, cookie_setting_overrides,
                             is_partitioned_access);
  }
}

void PageLoadTracker::OnStorageAccessed(const GURL& url,
                                        const GURL& first_party_url,
                                        bool blocked_by_policy,
                                        StorageType access_type) {
  for (const auto& observer : observers_) {
    observer->OnStorageAccessed(url, first_party_url, blocked_by_policy,
                                access_type);
  }
}

void PageLoadTracker::StopTracking() {
  did_stop_tracking_ = true;
  observers_map_.clear();
  observers_.clear();
}

void PageLoadTracker::AddObserver(
    std::unique_ptr<PageLoadMetricsObserverInterface> observer) {
  observer->SetDelegate(this);

  if (observer->GetObserverName()) {
    DCHECK(observers_map_.find(observer->GetObserverName()) ==
           observers_map_.end())
        << "We expect that observer's class and name is unique in trackers. "
           "Note that observer's class can be non-unique in test, e.g. "
           "PageLoadMetricsTestWaiter. In that case, use a unique name in "
           "the test. See also constructor of PageLoadMetricsTestWaiter.";
    observers_map_.emplace(observer->GetObserverName(), observer.get());
  }

  observers_.push_back(std::move(observer));
}

base::WeakPtr<PageLoadMetricsObserverInterface> PageLoadTracker::FindObserver(
    const char* name) {
  auto it = observers_map_.find(name);
  if (it != observers_map_.end()) {
    return it->second->GetWeakPtr();
  }
  return nullptr;
}

void PageLoadTracker::ClampBrowserTimestampIfInterProcessTimeTickSkew(
    base::TimeTicks* event_time) {
  DCHECK(event_time != nullptr);
  // Windows 10 GCE bot non-deterministically failed because TimeTicks::Now()
  // called in the browser process e.g. commit_time was less than
  // navigation_start_ that was populated in the renderer process because the
  // clock was not system-wide monotonic.
  // Note that navigation_start_ can also be set in the browser process in
  // some cases and in those cases event_time should never be <
  // navigation_start_. If it is due to a code error and it gets clamped in this
  // function, on high resolution systems it should lead to a dcheck failure.

  // TODO(shivanisha): Currently IsHighResolution is the best way to check
  // if the clock is system-wide monotonic. However IsHighResolution
  // does a broader check to see if the clock in use is high resolution
  // which also implies it is system-wide monotonic (on Windows).
  if (base::TimeTicks::IsHighResolution()) {
    DCHECK(event_time->is_null() || *event_time >= navigation_start_);
    return;
  }

  if (!event_time->is_null() && *event_time < navigation_start_) {
    RecordInternalError(ERR_INTER_PROCESS_TIME_TICK_SKEW);
    *event_time = navigation_start_;
  }
}

bool PageLoadTracker::HasMatchingNavigationRequestID(
    const content::GlobalRequestID& request_id) const {
  DCHECK(request_id != content::GlobalRequestID());
  return navigation_request_id_.has_value() &&
         navigation_request_id_.value() == request_id;
}

void PageLoadTracker::NotifyPageEnd(PageEndReason page_end_reason,
                                    UserInitiatedInfo user_initiated_info,
                                    base::TimeTicks timestamp,
                                    bool is_certainly_browser_timestamp) {
  DCHECK_NE(page_end_reason, END_NONE);
  // Use UpdatePageEnd to update an already notified PageLoadTracker.
  if (page_end_reason_ != END_NONE) {
    return;
  }

  UpdatePageEndInternal(page_end_reason, user_initiated_info, timestamp,
                        is_certainly_browser_timestamp);
}

void PageLoadTracker::UpdatePageEnd(PageEndReason page_end_reason,
                                    UserInitiatedInfo user_initiated_info,
                                    base::TimeTicks timestamp,
                                    bool is_certainly_browser_timestamp) {
  DCHECK_NE(page_end_reason, END_NONE);
  DCHECK_NE(page_end_reason, END_OTHER);
  DCHECK_EQ(page_end_reason_, END_OTHER);
  DCHECK(!page_end_time_.is_null());
  if (page_end_time_.is_null() || page_end_reason_ != END_OTHER) {
    return;
  }

  // For some aborts (e.g. navigations), the initiated timestamp can be earlier
  // than the timestamp that aborted the load. Taking the minimum gives the
  // closest user initiated time known.
  UpdatePageEndInternal(page_end_reason, user_initiated_info,
                        std::min(page_end_time_, timestamp),
                        is_certainly_browser_timestamp);
}

bool PageLoadTracker::IsLikelyProvisionalAbort(
    base::TimeTicks abort_cause_time) const {
  // Note that |abort_cause_time - page_end_time_| can be negative.
  return page_end_reason_ == END_OTHER &&
         (abort_cause_time - page_end_time_).InMilliseconds() < 100;
}

void PageLoadTracker::UpdatePageEndInternal(
    PageEndReason page_end_reason,
    UserInitiatedInfo user_initiated_info,
    base::TimeTicks timestamp,
    bool is_certainly_browser_timestamp) {
  // When a provisional navigation commits, that navigation's start time is
  // interpreted as the abort time for other provisional loads in the tab.
  // However, this only makes sense if the committed load started after the
  // aborted provisional loads started. Thus we ignore cases where the committed
  // load started before the aborted provisional load, as this would result in
  // recording a negative time-to-abort. The real issue here is that we have to
  // infer the cause of aborts. It would be better if the navigation code could
  // instead report the actual cause of an aborted navigation. See crbug/571647
  // for details.
  if (timestamp < navigation_start_) {
    RecordInternalError(ERR_END_BEFORE_NAVIGATION_START);
    page_end_reason_ = END_NONE;
    page_end_time_ = base::TimeTicks();
    return;
  }
  page_end_reason_ = page_end_reason;
  page_end_time_ = timestamp;
  // A client redirect can never be user initiated. Due to the way Blink
  // implements user gesture tracking, where all events that occur within a few
  // seconds after a user interaction are considered to be triggered by user
  // activation (based on the HTML spec:
  // https://html.spec.whatwg.org/multipage/interaction.html#tracking-user-activation),
  // these navs may sometimes be reported as user initiated by Blink. Thus, we
  // explicitly filter these types of aborts out when deciding if the abort was
  // user initiated.
  if (page_end_reason != END_CLIENT_REDIRECT) {
    page_end_user_initiated_info_ = user_initiated_info;
  }

  if (is_certainly_browser_timestamp) {
    ClampBrowserTimestampIfInterProcessTimeTickSkew(&page_end_time_);
  }
}

void PageLoadTracker::MediaStartedPlaying(
    const content::WebContentsObserver::MediaPlayerInfo& video_type,
    content::RenderFrameHost* render_frame_host) {
  for (const auto& observer : observers_) {
    observer->MediaStartedPlaying(video_type, render_frame_host);
  }
}

bool PageLoadTracker::IsPageMainFrame(content::RenderFrameHost* rfh) const {
  DCHECK(page_main_frame_);
  return rfh == page_main_frame_;
}

void PageLoadTracker::OnTimingChanged() {
  DCHECK(!last_dispatched_merged_page_timing_->Equals(
      metrics_update_dispatcher_.timing()));

  const mojom::PageLoadTiming& new_timing = metrics_update_dispatcher_.timing();

  if (new_timing.activation_start &&
      !last_dispatched_merged_page_timing_->activation_start &&
      prerendering_state_ != PrerenderingState::kNoPrerendering) {
    CHECK_EQ(prerendering_state_,
             PrerenderingState::kActivatedNoActivationStart);
    prerendering_state_ = PrerenderingState::kActivated;
    activation_start_ = new_timing.activation_start;
  }

  const mojom::PaintTimingPtr& paint_timing =
      metrics_update_dispatcher_.timing().paint_timing;

  largest_contentful_paint_handler_.RecordMainFrameTiming(
      *paint_timing->largest_contentful_paint,
      paint_timing->first_input_or_scroll_notified_timestamp);
  experimental_largest_contentful_paint_handler_.RecordMainFrameTiming(
      *paint_timing->experimental_largest_contentful_paint,
      paint_timing->first_input_or_scroll_notified_timestamp);

  for (const auto& observer : observers_) {
    DispatchObserverTimingCallbacks(
        observer.get(), *last_dispatched_merged_page_timing_, new_timing);
  }
  last_dispatched_merged_page_timing_ =
      metrics_update_dispatcher_.timing().Clone();
}

void PageLoadTracker::OnPageInputTimingChanged(uint64_t num_interactions) {
  for (const auto& observer : observers_) {
    observer->OnPageInputTimingUpdate(num_interactions);
  }
}

void PageLoadTracker::OnSubFrameTimingChanged(
    content::RenderFrameHost* rfh,
    const mojom::PageLoadTiming& timing) {
  DCHECK(rfh->GetParentOrOuterDocument());
  const mojom::PaintTimingPtr& paint_timing = timing.paint_timing;
  largest_contentful_paint_handler_.RecordSubFrameTiming(
      *paint_timing->largest_contentful_paint,
      paint_timing->first_input_or_scroll_notified_timestamp, rfh, url_);
  experimental_largest_contentful_paint_handler_.RecordSubFrameTiming(
      *paint_timing->experimental_largest_contentful_paint,
      paint_timing->first_input_or_scroll_notified_timestamp, rfh, url_);
  for (const auto& observer : observers_) {
    observer->OnTimingUpdate(rfh, timing);
  }
}

void PageLoadTracker::OnSubFrameInputTimingChanged(
    content::RenderFrameHost* rfh,
    const mojom::InputTiming& input_timing_delta) {
  DCHECK(rfh->GetParentOrOuterDocument());
  for (const auto& observer : observers_) {
    observer->OnInputTimingUpdate(rfh, input_timing_delta);
  }
}

void PageLoadTracker::OnPageRenderDataChanged(
    const mojom::FrameRenderDataUpdate& render_data,
    bool is_main_frame) {
  for (const auto& observer : observers_) {
    observer->OnPageRenderDataUpdate(render_data, is_main_frame);
  }
}

void PageLoadTracker::OnSubFrameRenderDataChanged(
    content::RenderFrameHost* rfh,
    const mojom::FrameRenderDataUpdate& render_data) {
  DCHECK(rfh->GetParentOrOuterDocument());
  for (const auto& observer : observers_) {
    observer->OnSubFrameRenderDataUpdate(rfh, render_data);
  }
}

void PageLoadTracker::OnMainFrameMetadataChanged() {
  for (const auto& observer : observers_) {
    observer->OnLoadingBehaviorObserved(nullptr,
                                        GetMainFrameMetadata().behavior_flags);
    observer->OnJavaScriptFrameworksObserved(
        nullptr, GetMainFrameMetadata().framework_detection_result);
  }
}

void PageLoadTracker::OnSubframeMetadataChanged(
    content::RenderFrameHost* rfh,
    const mojom::FrameMetadata& metadata) {
  for (const auto& observer : observers_) {
    observer->OnLoadingBehaviorObserved(rfh, metadata.behavior_flags);
  }
}

void PageLoadTracker::OnSoftNavigationChanged(
    const mojom::SoftNavigationMetrics& new_soft_navigation_metrics) {
  if (new_soft_navigation_metrics.Equals(*soft_navigation_metrics_)) {
    return;
  }

  // TODO(crbug.com/40065440): For soft navigation detections, the count and
  // start time should be monotonically increasing and navigation id different
  // each time. But we do see check failures on
  // soft_navigation_metrics.count >= soft_navigation_metrics_->count when this
  // OnSoftNavigationChanged is only invoked by soft navigation detection.
  // we should investigate this issue.

  for (const auto& observer : observers_) {
    observer->OnSoftNavigationUpdated(new_soft_navigation_metrics);
  }

  largest_contentful_paint_handler_.UpdateSoftNavigationLargestContentfulPaint(
      *new_soft_navigation_metrics.largest_contentful_paint);

  // Reset the soft_navigation_interval_responsiveness_metrics_normalization_
  // when a new soft nav comes in.
  if (new_soft_navigation_metrics.count > soft_navigation_metrics_->count) {
    metrics_update_dispatcher_
        .ResetSoftNavigationIntervalResponsivenessMetricsNormalization();
    metrics_update_dispatcher_.ResetSoftNavigationIntervalLayoutShift();
  }

  soft_navigation_metrics_ = new_soft_navigation_metrics.Clone();
}

void PageLoadTracker::OnPrefetchLikely() {
  for (const auto& observer : observers_) {
    observer->OnPrefetchLikely();
  }
}

void PageLoadTracker::UpdateFeaturesUsage(
    content::RenderFrameHost* rfh,
    const std::vector<blink::UseCounterFeature>& new_features) {
  for (const auto& observer : observers_) {
    observer->OnFeaturesUsageObserved(rfh, new_features);
  }
}

void PageLoadTracker::SetUpSharedMemoryForSmoothness(
    base::ReadOnlySharedMemoryRegion shared_memory) {
  DCHECK(shared_memory.IsValid());
  for (auto& observer : observers_) {
    observer->SetUpSharedMemoryForSmoothness(shared_memory);
  }
}

void PageLoadTracker::UpdateResourceDataUse(
    content::RenderFrameHost* rfh,
    const std::vector<mojom::ResourceDataUpdatePtr>& resources) {
  resource_tracker_.UpdateResourceDataUse(rfh->GetProcess()->GetID(),
                                          resources);
  for (const auto& observer : observers_) {
    observer->OnResourceDataUseObserved(rfh, resources);
  }
}

void PageLoadTracker::UpdateFrameCpuTiming(content::RenderFrameHost* rfh,
                                           const mojom::CpuTiming& timing) {
  for (const auto& observer : observers_) {
    observer->OnCpuTimingUpdate(rfh, timing);
  }
}

void PageLoadTracker::OnMainFrameIntersectionRectChanged(
    content::RenderFrameHost* rfh,
    const gfx::Rect& main_frame_intersection_rect) {
  for (const auto& observer : observers_) {
    observer->OnMainFrameIntersectionRectChanged(rfh,
                                                 main_frame_intersection_rect);
  }
}

void PageLoadTracker::OnMainFrameViewportRectChanged(
    const gfx::Rect& main_frame_viewport_rect) {
  for (const auto& observer : observers_) {
    observer->OnMainFrameViewportRectChanged(main_frame_viewport_rect);
  }
}

void PageLoadTracker::OnMainFrameImageAdRectsChanged(
    const base::flat_map<int, gfx::Rect>& main_frame_image_ad_rects) {
  for (const auto& observer : observers_) {
    observer->OnMainFrameImageAdRectsChanged(main_frame_image_ad_rects);
  }
}

content::WebContents* PageLoadTracker::GetWebContents() const {
  return web_contents_;
}

base::TimeTicks PageLoadTracker::GetNavigationStart() const {
  return navigation_start_;
}

std::optional<base::TimeDelta>
PageLoadTracker::DurationSinceNavigationStartForTime(
    const std::optional<base::TimeTicks>& time) const {
  std::optional<base::TimeDelta> duration;

  if (!time.has_value()) {
    return duration;
  }

  DCHECK_GE(time.value(), navigation_start_);
  duration = time.value() - navigation_start_;
  return duration;
}

std::optional<base::TimeDelta> PageLoadTracker::GetTimeToFirstBackground()
    const {
  return DurationSinceNavigationStartForTime(first_background_time_);
}

std::optional<base::TimeDelta> PageLoadTracker::GetTimeToFirstForeground()
    const {
  return DurationSinceNavigationStartForTime(first_foreground_time_);
}

const PageLoadMetricsObserverDelegate::BackForwardCacheRestore&
PageLoadTracker::GetBackForwardCacheRestore(size_t index) const {
  return back_forward_cache_restores_[index];
}

bool PageLoadTracker::StartedInForeground() const {
  return started_in_foreground_;
}

PageVisibility PageLoadTracker::GetVisibilityAtActivation() const {
  return visibility_at_activation_;
}

bool PageLoadTracker::WasPrerenderedThenActivatedInForeground() const {
  return GetVisibilityAtActivation() == PageVisibility::kForeground;
}

PrerenderingState PageLoadTracker::GetPrerenderingState() const {
  return prerendering_state_;
}

std::optional<base::TimeDelta> PageLoadTracker::GetActivationStart() const {
  return activation_start_;
}

const UserInitiatedInfo& PageLoadTracker::GetUserInitiatedInfo() const {
  return user_initiated_info_;
}

const GURL& PageLoadTracker::GetUrl() const {
  return url();
}

const GURL& PageLoadTracker::GetStartUrl() const {
  return start_url_;
}

bool PageLoadTracker::DidCommit() const {
  return did_commit_;
}

PageEndReason PageLoadTracker::GetPageEndReason() const {
  return page_end_reason_;
}

const UserInitiatedInfo& PageLoadTracker::GetPageEndUserInitiatedInfo() const {
  return page_end_user_initiated_info_;
}

std::optional<base::TimeDelta> PageLoadTracker::GetTimeToPageEnd() const {
  if (page_end_reason_ != END_NONE) {
    return DurationSinceNavigationStartForTime(page_end_time_);
  }
  DCHECK(page_end_time_.is_null());
  return std::nullopt;
}

const base::TimeTicks& PageLoadTracker::GetPageEndTime() const {
  return page_end_time_;
}

const mojom::FrameMetadata& PageLoadTracker::GetMainFrameMetadata() const {
  return metrics_update_dispatcher_.main_frame_metadata();
}

const mojom::FrameMetadata& PageLoadTracker::GetSubframeMetadata() const {
  return metrics_update_dispatcher_.subframe_metadata();
}

const PageRenderData& PageLoadTracker::GetPageRenderData() const {
  return metrics_update_dispatcher_.page_render_data();
}

const NormalizedCLSData& PageLoadTracker::GetNormalizedCLSData(
    BfcacheStrategy bfcache_strategy) const {
  return metrics_update_dispatcher_.normalized_cls_data(bfcache_strategy);
}

const NormalizedCLSData&
PageLoadTracker::GetSoftNavigationIntervalNormalizedCLSData() const {
  return metrics_update_dispatcher_
      .soft_navigation_interval_normalized_layout_shift();
}

const ResponsivenessMetricsNormalization&
PageLoadTracker::GetResponsivenessMetricsNormalization() const {
  return metrics_update_dispatcher_.responsiveness_metrics_normalization();
}

const ResponsivenessMetricsNormalization&
PageLoadTracker::GetSoftNavigationIntervalResponsivenessMetricsNormalization()
    const {
  return metrics_update_dispatcher_
      .soft_navigation_interval_responsiveness_metrics_normalization();
}

const mojom::InputTiming& PageLoadTracker::GetPageInputTiming() const {
  return metrics_update_dispatcher_.page_input_timing();
}

const std::optional<blink::SubresourceLoadMetrics>&
PageLoadTracker::GetSubresourceLoadMetrics() const {
  return metrics_update_dispatcher_.subresource_load_metrics();
}

const PageRenderData& PageLoadTracker::GetMainFrameRenderData() const {
  return metrics_update_dispatcher_.main_frame_render_data();
}

const ui::ScopedVisibilityTracker& PageLoadTracker::GetVisibilityTracker()
    const {
  return visibility_tracker_;
}

const ResourceTracker& PageLoadTracker::GetResourceTracker() const {
  return resource_tracker_;
}

const LargestContentfulPaintHandler&
PageLoadTracker::GetLargestContentfulPaintHandler() const {
  return largest_contentful_paint_handler_;
}

const LargestContentfulPaintHandler&
PageLoadTracker::GetExperimentalLargestContentfulPaintHandler() const {
  return experimental_largest_contentful_paint_handler_;
}

ukm::SourceId PageLoadTracker::GetPageUkmSourceId() const {
  DCHECK_NE(ukm::kInvalidSourceId, source_id_)
      << "GetPageUkmSourceId was called on a prerendered page before its "
         "activation. We should not collect UKM while prerendering pages.";
  return source_id_;
}

mojom::SoftNavigationMetrics& PageLoadTracker::GetSoftNavigationMetrics()
    const {
  return *soft_navigation_metrics_;
}

ukm::SourceId PageLoadTracker::GetUkmSourceIdForSoftNavigation() const {
  return potential_soft_navigation_source_id_;
}

ukm::SourceId PageLoadTracker::GetPreviousUkmSourceIdForSoftNavigation() const {
  return previous_soft_navigation_source_id_;
}

bool PageLoadTracker::IsFirstNavigationInWebContents() const {
  return is_first_navigation_in_web_contents_;
}

bool PageLoadTracker::IsOriginVisit() const {
  return is_origin_visit_;
}

bool PageLoadTracker::IsTerminalVisit() const {
  return is_terminal_visit_;
}

int64_t PageLoadTracker::GetNavigationId() const {
  return navigation_id_;
}

void PageLoadTracker::RecordLinkNavigation() {
  is_terminal_visit_ = false;
}

void PageLoadTracker::OnEnterBackForwardCache() {
  // In case of BackForwardCache, invoke and update the
  // PageLoadMetricsUpdateDispatcher before the page is hidden to enable
  // recording metrics that requires the page to be in foreground before
  // entering BackForwardCache on navigation.
  InvokeAndPruneObservers(
      "PageLoadMetricsObserver::OnEnterBackForwardCache",
      base::BindRepeating(
          [](const mojom::PageLoadTiming* timing,
             PageLoadMetricsObserverInterface* observer) {
            return observer->OnEnterBackForwardCache(*timing);
          },
          &metrics_update_dispatcher_.timing()),
      /*permit_forwarding=*/false);
  metrics_update_dispatcher_.UpdateLayoutShiftNormalizationForBfcache();
  metrics_update_dispatcher_
      .UpdateResponsivenessMetricsNormalizationForBfcache();
  if (GetWebContents()->GetVisibility() == content::Visibility::VISIBLE) {
    PageHidden();
  }
}

void PageLoadTracker::OnRestoreFromBackForwardCache(
    content::NavigationHandle* navigation_handle) {
  DCHECK(!visibility_tracker_.currently_in_foreground());
  bool visible =
      GetWebContents()->GetVisibility() == content::Visibility::VISIBLE;

  BackForwardCacheRestore back_forward_cache_restore(
      visible, navigation_handle->NavigationStart());
  back_forward_cache_restores_.push_back(back_forward_cache_restore);

  if (visible) {
    PageShown();
  }

  for (const auto& observer : observers_) {
    observer->OnRestoreFromBackForwardCache(metrics_update_dispatcher_.timing(),
                                            navigation_handle);
  }

  // Reset the page end reason to END_NONE. The page has been restored, its
  // previous end reason is no longer relevant. Similarly, its page end time is
  // no longer accurate, so reset that as well.
  page_end_reason_ = END_NONE;
  page_end_time_ = base::TimeTicks();
}

void PageLoadTracker::OnV8MemoryChanged(
    const std::vector<MemoryUpdate>& memory_updates) {
  for (const auto& observer : observers_) {
    observer->OnV8MemoryChanged(memory_updates);
  }
}

void PageLoadTracker::OnSharedStorageWorkletHostCreated() {
  for (const auto& observer : observers_) {
    observer->OnSharedStorageWorkletHostCreated();
  }
}

void PageLoadTracker::OnSharedStorageSelectURLCalled() {
  for (const auto& observer : observers_) {
    observer->OnSharedStorageSelectURLCalled();
  }
}

void PageLoadTracker::OnAdAuctionComplete(bool is_server_auction,
                                          bool is_on_device_auction,
                                          content::AuctionResult result) {
  for (const auto& observer : observers_) {
    observer->OnAdAuctionComplete(is_server_auction, is_on_device_auction,
                                  result);
  }
}

void PageLoadTracker::UpdateMetrics(
    content::RenderFrameHost* render_frame_host,
    mojom::PageLoadTimingPtr timing,
    mojom::FrameMetadataPtr metadata,
    const std::vector<blink::UseCounterFeature>& features,
    const std::vector<mojom::ResourceDataUpdatePtr>& resources,
    mojom::FrameRenderDataUpdatePtr render_data,
    mojom::CpuTimingPtr cpu_timing,
    mojom::InputTimingPtr input_timing_delta,
    const std::optional<blink::SubresourceLoadMetrics>&
        subresource_load_metrics,
    mojom::SoftNavigationMetricsPtr soft_navigation_metrics) {
  if (parent_tracker_) {
    parent_tracker_->UpdateMetrics(
        render_frame_host, timing.Clone(), metadata.Clone(), features,
        resources, render_data.Clone(), cpu_timing.Clone(),
        input_timing_delta.Clone(), subresource_load_metrics,
        soft_navigation_metrics.Clone());
  }

  metrics_update_dispatcher_.UpdateMetrics(
      render_frame_host, std::move(timing), std::move(metadata),
      std::move(features), resources, std::move(render_data),
      std::move(cpu_timing), std::move(input_timing_delta),
      subresource_load_metrics, std::move(soft_navigation_metrics), page_type_);
}

void PageLoadTracker::AddCustomUserTimings(
    std::vector<mojom::CustomUserTimingMarkPtr> custom_timings) {
  for (const auto& observer : observers_) {
    observer->OnCustomUserTimingMarkObserved(custom_timings);
  }
}

void PageLoadTracker::SetPageMainFrame(content::RenderFrameHost* rfh) {
  page_main_frame_ = rfh;
}

base::WeakPtr<PageLoadTracker> PageLoadTracker::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PageLoadTracker::InvokeAndPruneObservers(
    const char* trace_name,
    PageLoadTracker::InvokeCallback callback,
    bool permit_forwarding) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"), trace_name);
  std::vector<std::unique_ptr<PageLoadMetricsObserverInterface>>
      forward_observers;
  for (auto it = observers_.begin(); it != observers_.end();) {
    auto policy = callback.Run(it->get());
    switch (policy) {
      case PageLoadMetricsObserver::CONTINUE_OBSERVING:
        ++it;
        break;
      case PageLoadMetricsObserver::STOP_OBSERVING:
        if ((*it)->GetObserverName()) {
          observers_map_.erase((*it)->GetObserverName());
        }
        it = observers_.erase(it);
        break;
      case PageLoadMetricsObserver::FORWARD_OBSERVING:
        DCHECK(permit_forwarding);
        DCHECK((*it)->GetObserverName())
            << "GetObserverName should be implemented";
        auto target_observer =
            parent_tracker_
                ? parent_tracker_->FindObserver((*it)->GetObserverName())
                : nullptr;
        if (target_observer) {
          forward_observers.emplace_back(
              std::make_unique<PageLoadMetricsForwardObserver>(
                  target_observer));
        }
        observers_map_.erase((*it)->GetObserverName());
        it = observers_.erase(it);
        break;
    }
  }
  for (auto& observer : forward_observers) {
    DCHECK(observers_map_.find(observer->GetObserverName()) ==
           observers_map_.end());
    AddObserver(std::move(observer));
  }
}

}  // namespace page_load_metrics
