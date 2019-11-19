// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_TRACKER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_TRACKER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/page_load_metrics_update_dispatcher.h"
#include "components/page_load_metrics/browser/resource_tracker.h"
#include "components/page_load_metrics/common/page_end_reason.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/cookies/canonical_cookie.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/scoped_visibility_tracker.h"
#include "ui/gfx/geometry/size.h"

class GURL;

namespace blink {
class WebInputEvent;
}  // namespace blink

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace page_load_metrics {

class PageLoadMetricsEmbedderInterface;

namespace internal {

extern const char kErrorEvents[];
extern const char kAbortChainSizeReload[];
extern const char kAbortChainSizeForwardBack[];
extern const char kAbortChainSizeNewNavigation[];
extern const char kAbortChainSizeNoCommit[];
extern const char kAbortChainSizeSameURL[];
extern const char kPageLoadCompletedAfterAppBackground[];
extern const char kPageLoadStartedInForeground[];

}  // namespace internal

// These errors are internal to the page_load_metrics subsystem and do not
// reflect actual errors that occur during a page load.
//
// If you add elements to this enum, make sure you update the enum
// value in histograms.xml. Only add elements to the end to prevent
// inconsistencies between versions.
enum InternalErrorLoadEvent {
  // A timing IPC was sent from the renderer that did not line up with previous
  // data we've received (i.e. navigation start is different or the timing
  // struct is somehow invalid). This error can only occur once the IPC is
  // vetted in other ways (see other errors). This error is deprecated as it has
  // been replaced by the more detailed ERR_BAD_TIMING_IPC_* error codes.
  DEPRECATED_ERR_BAD_TIMING_IPC,

  // The following IPCs are not mutually exclusive.
  //
  // We received an IPC when we weren't tracking a committed load. This will
  // often happen if we get an IPC from a bad URL scheme (that is, the renderer
  // sent us an IPC from a navigation we don't care about).
  ERR_IPC_WITH_NO_RELEVANT_LOAD,

  // Received a notification from a frame that has been navigated away from.
  ERR_IPC_FROM_WRONG_FRAME,

  // We received an IPC even through the last committed url from the browser
  // was not http/s. This can happen with the renderer sending IPCs for the
  // new tab page. This will often come paired with
  // ERR_IPC_WITH_NO_RELEVANT_LOAD.
  ERR_IPC_FROM_BAD_URL_SCHEME,

  // If we track a navigation, but the renderer sends us no IPCs. This could
  // occur if the browser filters loads less aggressively than the renderer.
  ERR_NO_IPCS_RECEIVED,

  // Tracks frequency with which we record an end time that occurred before
  // navigation start. This is expected to happen in some cases (see comments in
  // cc file for details). We use this error counter to understand how often it
  // happens.
  ERR_END_BEFORE_NAVIGATION_START,

  // A new navigation triggers abort updates in multiple trackers in
  // |aborted_provisional_loads_|, when usually there should only be one (the
  // navigation that just aborted because of this one). If this happens, the
  // latest aborted load is used to track the chain size.
  ERR_NAVIGATION_SIGNALS_MULIPLE_ABORTED_LOADS,

  // Received user input without a relevant load. This error type is deprecated,
  // as it is valid to receive user input without a relevant load. We leave the
  // enum value here since it's also used in histogram recording, so it's
  // important that we not re-use this enum entry for a different value.
  DEPRECATED_ERR_USER_INPUT_WITH_NO_RELEVANT_LOAD,

  // A TimeTicks value in the browser process has value less than
  // navigation_start_. This could happen if navigation_start_ was computed in
  // renderer process and the system clock has inter process time tick skew.
  ERR_INTER_PROCESS_TIME_TICK_SKEW,

  // At the time a PageLoadTracker was destroyed, we had received neither a
  // commit nor a failed provisional load.
  ERR_NO_COMMIT_OR_FAILED_PROVISIONAL_LOAD,

  // No page load end time was recorded for this page load.
  ERR_NO_PAGE_LOAD_END_TIME,

  // Received a timing update from a subframe (deprecated).
  DEPRECATED_ERR_TIMING_IPC_FROM_SUBFRAME,

  // A timing IPC was sent from the renderer that contained timing data which
  // was inconsistent with our timing data for the currently committed load.
  ERR_BAD_TIMING_IPC_INVALID_TIMING_DESCENDENT,

  // A timing IPC was sent from the renderer that contained loading behavior
  // data which was inconsistent with our loading behavior data for the
  // currently committed load.
  ERR_BAD_TIMING_IPC_INVALID_BEHAVIOR_DESCENDENT,

  // A timing IPC was sent from the renderer that contained invalid timing data
  // (e.g. out of order timings, or other issues).
  ERR_BAD_TIMING_IPC_INVALID_TIMING,

  // We received a navigation start for a child frame that is before the
  // navigation start of the main frame.
  ERR_SUBFRAME_NAVIGATION_START_BEFORE_MAIN_FRAME,

  // We received an IPC from a subframe when we weren't tracking a committed
  // load. We expect this error to happen, and track it so we can understand how
  // frequently this case is encountered.
  ERR_SUBFRAME_IPC_WITH_NO_RELEVANT_LOAD,

  // We received browser-process reported metrics when we weren't tracking a
  // committed load. We expect this error to happen, and track it so we can
  // understand how frequently this case is encountered.
  ERR_BROWSER_USAGE_WITH_NO_RELEVANT_LOAD,

  // Add values before this final count.
  ERR_LAST_ENTRY,
};

// NOTE: these functions are shared by page_load_tracker.cc and
// metrics_web_contents_observer.cc. They are declared here to allow both files
// to access them.
void RecordInternalError(InternalErrorLoadEvent event);
PageEndReason EndReasonForPageTransition(ui::PageTransition transition);
void LogAbortChainSameURLHistogram(int aborted_chain_size_same_url);
bool IsNavigationUserInitiated(content::NavigationHandle* handle);

// This class tracks a given page load, starting from navigation start /
// provisional load, until a new navigation commits or the navigation fails.
// MetricsWebContentsObserver manages a set of provisional PageLoadTrackers, as
// well as a committed PageLoadTracker.
class PageLoadTracker : public PageLoadMetricsUpdateDispatcher::Client,
                        public PageLoadMetricsObserverDelegate {
 public:
  // Caller must guarantee that the embedder_interface pointer outlives this
  // class. The PageLoadTracker must not hold on to
  // currently_committed_load_or_null or navigation_handle beyond the scope of
  // the constructor.
  PageLoadTracker(bool in_foreground,
                  PageLoadMetricsEmbedderInterface* embedder_interface,
                  const GURL& currently_committed_url,
                  bool is_first_navigation_in_web_contents,
                  content::NavigationHandle* navigation_handle,
                  UserInitiatedInfo user_initiated_info,
                  int aborted_chain_size,
                  int aborted_chain_size_same_url);
  ~PageLoadTracker() override;

  // PageLoadMetricsUpdateDispatcher::Client implementation:
  void OnTimingChanged() override;
  void OnSubFrameTimingChanged(content::RenderFrameHost* rfh,
                               const mojom::PageLoadTiming& timing) override;
  void OnSubFrameRenderDataChanged(
      content::RenderFrameHost* rfh,
      const mojom::FrameRenderDataUpdate& render_data) override;
  void OnMainFrameMetadataChanged() override;
  void OnSubframeMetadataChanged(
      content::RenderFrameHost* rfh,
      const mojom::PageLoadMetadata& metadata) override;
  void UpdateFeaturesUsage(
      content::RenderFrameHost* rfh,
      const mojom::PageLoadFeatures& new_features) override;
  void UpdateResourceDataUse(
      content::RenderFrameHost* rfh,
      const std::vector<mojom::ResourceDataUpdatePtr>& resources) override;
  void OnNewDeferredResourceCounts(
      const mojom::DeferredResourceCounts& new_deferred_resource_data) override;
  void UpdateFrameCpuTiming(content::RenderFrameHost* rfh,
                            const mojom::CpuTiming& timing) override;

  // PageLoadMetricsDelegate implementation:
  content::WebContents* GetWebContents() const override;
  base::TimeTicks GetNavigationStart() const override;
  const base::Optional<base::TimeDelta>& GetFirstBackgroundTime()
      const override;
  const base::Optional<base::TimeDelta>& GetFirstForegroundTime()
      const override;
  bool StartedInForeground() const override;
  const UserInitiatedInfo& GetUserInitiatedInfo() const override;
  const GURL& GetUrl() const override;
  const GURL& GetStartUrl() const override;
  bool DidCommit() const override;
  PageEndReason GetPageEndReason() const override;
  const UserInitiatedInfo& GetPageEndUserInitiatedInfo() const override;
  base::Optional<base::TimeDelta> GetPageEndTime() const override;
  const mojom::PageLoadMetadata& GetMainFrameMetadata() const override;
  const mojom::PageLoadMetadata& GetSubframeMetadata() const override;
  const PageRenderData& GetPageRenderData() const override;
  const PageRenderData& GetMainFrameRenderData() const override;
  const ui::ScopedVisibilityTracker& GetVisibilityTracker() const override;
  const ResourceTracker& GetResourceTracker() const override;
  ukm::SourceId GetSourceId() const override;
  bool IsFirstNavigationInWebContents() const override;

  void Redirect(content::NavigationHandle* navigation_handle);
  void WillProcessNavigationResponse(
      content::NavigationHandle* navigation_handle);
  void Commit(content::NavigationHandle* navigation_handle);
  void DidCommitSameDocumentNavigation(
      content::NavigationHandle* navigation_handle);
  void DidInternalNavigationAbort(content::NavigationHandle* navigation_handle);
  void ReadyToCommitNavigation(content::NavigationHandle* navigation_handle);
  void DidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle);
  void FailedProvisionalLoad(content::NavigationHandle* navigation_handle,
                             base::TimeTicks failed_load_time);
  void WebContentsHidden();
  void WebContentsShown();
  void FrameDeleted(content::RenderFrameHost* rfh);

  void OnInputEvent(const blink::WebInputEvent& event);

  // Flush any buffered metrics, as part of the metrics subsystem persisting
  // metrics as the application goes into the background. The application may be
  // killed at any time after this method is invoked without further
  // notification.
  void FlushMetricsOnAppEnterBackground();

  // Replaces the |visibility_tracker_| for testing, which can mock a clock.
  void SetVisibilityTrackerForTesting(
      const ui::ScopedVisibilityTracker& tracker) {
    visibility_tracker_ = tracker;
  }

  void NotifyClientRedirectTo(const PageLoadTracker& destination);

  void OnLoadedResource(
      const ExtraRequestCompleteInfo& extra_request_complete_info);

  void FrameReceivedFirstUserActivation(content::RenderFrameHost* rfh);
  void FrameDisplayStateChanged(content::RenderFrameHost* render_frame_host,
                                bool is_display_none);
  void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                        const gfx::Size& frame_size);

  void OnCookiesRead(const GURL& url,
                     const GURL& first_party_url,
                     const net::CookieList& cookie_list,
                     bool blocked_by_policy);

  void OnCookieChange(const GURL& url,
                      const GURL& first_party_url,
                      const net::CanonicalCookie& cookie,
                      bool blocked_by_policy);

  void OnDomStorageAccessed(const GURL& url,
                            const GURL& first_party_url,
                            bool local,
                            bool blocked_by_policy);

  // Signals that we should stop tracking metrics for the associated page load.
  // We may stop tracking a page load if it doesn't meet the criteria for
  // tracking metrics in DidFinishNavigation.
  void StopTracking();

  int aborted_chain_size() const { return aborted_chain_size_; }
  int aborted_chain_size_same_url() const {
    return aborted_chain_size_same_url_;
  }

  PageEndReason page_end_reason() const { return page_end_reason_; }
  base::TimeTicks page_end_time() const { return page_end_time_; }

  void AddObserver(std::unique_ptr<PageLoadMetricsObserver> observer);

  // If the user performs some abort-like action while we are tracking this page
  // load, notify the tracker. Note that we may not classify this as an abort if
  // we've already performed a first paint.
  // is_certainly_browser_timestamp signifies if the timestamp passed is taken
  // in the
  // browser process or not. We need this to possibly clamp browser timestamp on
  // a machine with inter process time tick skew.
  void NotifyPageEnd(PageEndReason page_end_reason,
                     UserInitiatedInfo user_initiated_info,
                     base::TimeTicks timestamp,
                     bool is_certainly_browser_timestamp);
  void UpdatePageEnd(PageEndReason page_end_reason,
                     UserInitiatedInfo user_initiated_info,
                     base::TimeTicks timestamp,
                     bool is_certainly_browser_timestamp);

  // This method returns true if this page load has been aborted with type of
  // END_OTHER, and the |abort_cause_time| is within a sufficiently close
  // delta to when it was aborted. Note that only provisional loads can be
  // aborted with END_OTHER. While this heuristic is coarse, it works better
  // and is simpler than other feasible methods. See https://goo.gl/WKRG98.
  bool IsLikelyProvisionalAbort(base::TimeTicks abort_cause_time) const;

  bool MatchesOriginalNavigation(content::NavigationHandle* navigation_handle);

  bool did_commit() const { return did_commit_; }
  const GURL& url() const { return url_; }

  base::TimeTicks navigation_start() const { return navigation_start_; }

  ui::PageTransition page_transition() const { return page_transition_; }

  UserInitiatedInfo user_initiated_info() const { return user_initiated_info_; }

  PageLoadMetricsUpdateDispatcher* metrics_update_dispatcher() {
    return &metrics_update_dispatcher_;
  }

  // Whether this PageLoadTracker has a navigation GlobalRequestID that matches
  // the given request_id. This method will return false before
  // WillProcessNavigationResponse has been invoked, as PageLoadTracker doesn't
  // know its GlobalRequestID until WillProcessNavigationResponse has been
  // invoked.
  bool HasMatchingNavigationRequestID(
      const content::GlobalRequestID& request_id) const;

  // Invoked when a media element starts playing.
  void MediaStartedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& video_type,
      content::RenderFrameHost* render_frame_host);

  // Informs the observers that the event corresponding to |event_key| has
  // occurred.
  void BroadcastEventToObservers(const void* const event_key);

 private:
  // This function converts a TimeTicks value taken in the browser process
  // to navigation_start_ if:
  // - base::TimeTicks is not comparable across processes because the clock
  // is not system wide monotonic.
  // - *event_time < navigation_start_
  void ClampBrowserTimestampIfInterProcessTimeTickSkew(
      base::TimeTicks* event_time);

  void UpdatePageEndInternal(PageEndReason page_end_reason,
                             UserInitiatedInfo user_initiated_info,
                             base::TimeTicks timestamp,
                             bool is_certainly_browser_timestamp);

  // If |final_navigation| is null, then this is an "unparented" abort chain,
  // and represents a sequence of provisional aborts that never ends with a
  // committed load.
  void LogAbortChainHistograms(content::NavigationHandle* final_navigation);

  // Whether we stopped tracking this navigation after it was initiated. We may
  // stop tracking a navigation if it doesn't meet the criteria for tracking
  // metrics in DidFinishNavigation.
  bool did_stop_tracking_;

  // Whether the application went into the background when this PageLoadTracker
  // was active. This is a temporary boolean for UMA tracking.
  bool app_entered_background_;

  // The navigation start in TimeTicks, not the wall time reported by Blink.
  const base::TimeTicks navigation_start_;

  // The most recent URL of this page load. Updated at navigation start, upon
  // redirection, and at commit time.
  GURL url_;

  // The start URL for this page load (before redirects).
  GURL start_url_;

  ui::ScopedVisibilityTracker visibility_tracker_;

  // Whether this page load committed.
  bool did_commit_;

  std::unique_ptr<FailedProvisionalLoadInfo> failed_provisional_load_info_;

  // Will be END_NONE if we have not ended this load yet. Otherwise will
  // be the first page end reason encountered.
  PageEndReason page_end_reason_;

  // Whether the page end cause for this page load was user initiated. For
  // example, if this page load was ended by a new navigation, this field tracks
  // whether that new navigation was user-initiated. This field is only useful
  // if this page load's end reason is a value other than END_NONE. Note that
  // this value is currently experimental, and is subject to change. In
  // particular, this field is never set to true for some page end reasons, such
  // as stop and close, since we don't yet have sufficient instrumentation to
  // know if a stop or close was caused by a user action.
  UserInitiatedInfo page_end_user_initiated_info_;

  base::TimeTicks page_end_time_;

  // We record separate metrics for events that occur after a background,
  // because metrics like layout/paint are delayed artificially
  // when they occur in the background.
  base::Optional<base::TimeDelta> first_background_time_;
  base::Optional<base::TimeDelta> first_foreground_time_;
  bool started_in_foreground_;

  mojom::PageLoadTimingPtr last_dispatched_merged_page_timing_;

  ui::PageTransition page_transition_;

  base::Optional<content::GlobalRequestID> navigation_request_id_;

  // Whether this page load was user initiated.
  UserInitiatedInfo user_initiated_info_;

  // This is a subtle member. If a provisional load A gets aborted by
  // provisional load B, which gets aborted by C that eventually commits, then
  // there exists an abort chain of length 2, starting at A's navigation_start.
  // This is useful because it allows histograming abort chain lengths based on
  // what the last load's transition type is. i.e. holding down F-5 to spam
  // reload will produce a long chain with the RELOAD transition.
  const int aborted_chain_size_;

  // This member counts consecutive provisional aborts that share a url. It will
  // always be less than or equal to |aborted_chain_size_|.
  const int aborted_chain_size_same_url_;

  // Keeps track of actively loading resources on the page.
  ResourceTracker resource_tracker_;

  // Interface to chrome features. Must outlive the class.
  PageLoadMetricsEmbedderInterface* const embedder_interface_;

  std::vector<std::unique_ptr<PageLoadMetricsObserver>> observers_;

  PageLoadMetricsUpdateDispatcher metrics_update_dispatcher_;

  const ukm::SourceId source_id_;

  content::WebContents* const web_contents_;

  const bool is_first_navigation_in_web_contents_;

  DISALLOW_COPY_AND_ASSIGN(PageLoadTracker);
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_TRACKER_H_
