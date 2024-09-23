// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_TRACKER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_TRACKER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/page_load_metrics_update_dispatcher.h"
#include "components/page_load_metrics/browser/resource_tracker.h"
#include "components/page_load_metrics/common/page_end_reason.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/cookies/canonical_cookie.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/performance/performance_timeline_constants.h"
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

struct MemoryUpdate;
class PageLoadMetricsEmbedderInterface;

namespace internal {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PageLoadPrerenderEvent {
  kNavigationInPrerenderedMainFrame = 0,
  kPrerenderActivationNavigation = 1,
  kMaxValue = kPrerenderActivationNavigation,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PageLoadTrackerPageType {
  kPrimaryPage = 0,
  kPrerenderPage = 1,
  kFencedFramesPage = 2,
  kPreviewPrimaryPage = 3,  // Primary page in the preview mode
  kMaxValue = kPreviewPrimaryPage,
};

extern const char kErrorEvents[];
extern const char kPageLoadPrerender2Event[];
extern const char kPageLoadTrackerPageType[];

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
bool IsNavigationUserInitiated(content::NavigationHandle* handle);

// This class tracks a given page load, starting from navigation start /
// provisional load, until a new navigation commits or the navigation fails.
// MetricsWebContentsObserver manages a set of provisional PageLoadTrackers, as
// well as a committed PageLoadTracker.
class PageLoadTracker : public PageLoadMetricsUpdateDispatcher::Client,
                        public PageLoadMetricsObserverDelegate {
 public:
  // Caller must guarantee that the `embedder_interface` pointer outlives this
  // class. The PageLoadTracker must not hold on to `navigation_handle` beyond
  // the scope of the constructor.
  PageLoadTracker(bool in_foreground,
                  PageLoadMetricsEmbedderInterface* embedder_interface,
                  const GURL& currently_committed_url,
                  bool is_first_navigation_in_web_contents,
                  content::NavigationHandle* navigation_handle,
                  UserInitiatedInfo user_initiated_info,
                  ukm::SourceId source_id,
                  base::WeakPtr<PageLoadTracker> parent_tracker);

  PageLoadTracker(const PageLoadTracker&) = delete;
  PageLoadTracker& operator=(const PageLoadTracker&) = delete;

  ~PageLoadTracker() override;

  // PageLoadMetricsUpdateDispatcher::Client implementation:
  bool IsPageMainFrame(content::RenderFrameHost* rfh) const override;
  void OnTimingChanged() override;
  void OnPageInputTimingChanged(uint64_t num_interactions) override;
  void OnSubFrameTimingChanged(content::RenderFrameHost* rfh,
                               const mojom::PageLoadTiming& timing) override;
  void OnSubFrameInputTimingChanged(
      content::RenderFrameHost* rfh,
      const mojom::InputTiming& input_timing_delta) override;
  void OnPageRenderDataChanged(const mojom::FrameRenderDataUpdate& render_data,
                               bool is_main_frame) override;
  void OnSubFrameRenderDataChanged(
      content::RenderFrameHost* rfh,
      const mojom::FrameRenderDataUpdate& render_data) override;
  void OnMainFrameMetadataChanged() override;
  void OnSubframeMetadataChanged(content::RenderFrameHost* rfh,
                                 const mojom::FrameMetadata& metadata) override;
  void OnSoftNavigationChanged(
      const mojom::SoftNavigationMetrics& soft_navigation_metrics) override;
  void UpdateFeaturesUsage(
      content::RenderFrameHost* rfh,
      const std::vector<blink::UseCounterFeature>& new_features) override;
  void UpdateResourceDataUse(
      content::RenderFrameHost* rfh,
      const std::vector<mojom::ResourceDataUpdatePtr>& resources) override;
  void UpdateFrameCpuTiming(content::RenderFrameHost* rfh,
                            const mojom::CpuTiming& timing) override;
  void OnMainFrameIntersectionRectChanged(
      content::RenderFrameHost* rfh,
      const gfx::Rect& main_frame_intersection_rect) override;
  void OnMainFrameViewportRectChanged(
      const gfx::Rect& main_frame_viewport_rect) override;
  void OnMainFrameImageAdRectsChanged(
      const base::flat_map<int, gfx::Rect>& main_frame_image_ad_rects) override;
  void SetUpSharedMemoryForSmoothness(
      base::ReadOnlySharedMemoryRegion shared_memory) override;

  // PageLoadMetricsObserverDelegate implementation:
  content::WebContents* GetWebContents() const override;
  base::TimeTicks GetNavigationStart() const override;
  std::optional<base::TimeDelta> GetTimeToFirstBackground() const override;
  std::optional<base::TimeDelta> GetTimeToFirstForeground() const override;
  PrerenderingState GetPrerenderingState() const override;
  std::optional<base::TimeDelta> GetActivationStart() const override;
  const BackForwardCacheRestore& GetBackForwardCacheRestore(
      size_t index) const override;
  bool StartedInForeground() const override;
  PageVisibility GetVisibilityAtActivation() const override;
  bool WasPrerenderedThenActivatedInForeground() const override;
  const UserInitiatedInfo& GetUserInitiatedInfo() const override;
  const GURL& GetUrl() const override;
  const GURL& GetStartUrl() const override;
  bool DidCommit() const override;
  PageEndReason GetPageEndReason() const override;
  const UserInitiatedInfo& GetPageEndUserInitiatedInfo() const override;
  std::optional<base::TimeDelta> GetTimeToPageEnd() const override;
  const base::TimeTicks& GetPageEndTime() const override;
  const mojom::FrameMetadata& GetMainFrameMetadata() const override;
  const mojom::FrameMetadata& GetSubframeMetadata() const override;
  const PageRenderData& GetPageRenderData() const override;
  const NormalizedCLSData& GetNormalizedCLSData(
      BfcacheStrategy bfcache_strategy) const override;
  const NormalizedCLSData& GetSoftNavigationIntervalNormalizedCLSData()
      const override;
  const ResponsivenessMetricsNormalization&
  GetResponsivenessMetricsNormalization() const override;
  const ResponsivenessMetricsNormalization&
  GetSoftNavigationIntervalResponsivenessMetricsNormalization() const override;
  const mojom::InputTiming& GetPageInputTiming() const override;
  const std::optional<blink::SubresourceLoadMetrics>&
  GetSubresourceLoadMetrics() const override;
  const PageRenderData& GetMainFrameRenderData() const override;
  const ui::ScopedVisibilityTracker& GetVisibilityTracker() const override;
  const ResourceTracker& GetResourceTracker() const override;
  const LargestContentfulPaintHandler& GetLargestContentfulPaintHandler()
      const override;
  const LargestContentfulPaintHandler&
  GetExperimentalLargestContentfulPaintHandler() const override;
  ukm::SourceId GetPageUkmSourceId() const override;
  mojom::SoftNavigationMetrics& GetSoftNavigationMetrics() const override;
  ukm::SourceId GetUkmSourceIdForSoftNavigation() const override;
  ukm::SourceId GetPreviousUkmSourceIdForSoftNavigation() const override;
  bool IsFirstNavigationInWebContents() const override;
  bool IsOriginVisit() const override;
  bool IsTerminalVisit() const override;
  int64_t GetNavigationId() const override;

  // The following methods are called on navigation related events.
  //
  // Called only on main frames.
  void DidUpdateNavigationHandleTiming(
      content::NavigationHandle* navigation_handle);
  void Redirect(content::NavigationHandle* navigation_handle);
  void Commit(content::NavigationHandle* navigation_handle);
  void DidCommitSameDocumentNavigation(
      content::NavigationHandle* navigation_handle);
  void DidInternalNavigationAbort(content::NavigationHandle* navigation_handle);
  void FailedProvisionalLoad(content::NavigationHandle* navigation_handle,
                             base::TimeTicks failed_load_time);
  // Called only on subframes.
  void DidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle);
  // Called on main and sub-frames.
  void ReadyToCommitNavigation(content::NavigationHandle* navigation_handle);
  void WillProcessNavigationResponse(
      content::NavigationHandle* navigation_handle);
  void PageHidden();
  void PageShown();
  void RenderFrameDeleted(content::RenderFrameHost* rfh);
  void FrameTreeNodeDeleted(content::FrameTreeNodeId frame_tree_node_id);

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

  void OnLoadedResource(
      const ExtraRequestCompleteInfo& extra_request_complete_info);

  void FrameReceivedUserActivation(content::RenderFrameHost* rfh);
  void FrameDisplayStateChanged(content::RenderFrameHost* render_frame_host,
                                bool is_display_none);
  void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                        const gfx::Size& frame_size);

  void OnCookiesRead(
      const GURL& url,
      const GURL& first_party_url,
      bool blocked_by_policy,
      bool is_ad_tagged,
      const net::CookieSettingOverrides& cookie_setting_overrides,
      bool is_partitioned_access);

  void OnCookieChange(
      const GURL& url,
      const GURL& first_party_url,
      const net::CanonicalCookie& cookie,
      bool blocked_by_policy,
      bool is_ad_tagged,
      const net::CookieSettingOverrides& cookie_setting_overrides,
      bool is_partitioned_access);

  void OnStorageAccessed(const GURL& url,
                         const GURL& first_party_url,
                         bool blocked_by_policy,
                         StorageType access_type);

  // Signals that we should stop tracking metrics for the associated page load.
  // We may stop tracking a page load if it doesn't meet the criteria for
  // tracking metrics in DidFinishNavigation.
  void StopTracking();

  void AddObserver(std::unique_ptr<PageLoadMetricsObserverInterface> observer);
  base::WeakPtr<PageLoadMetricsObserverInterface> FindObserver(
      char const* name);

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

  bool did_commit() const { return did_commit_; }
  const GURL& url() const { return url_; }

  base::TimeTicks navigation_start() const { return navigation_start_; }

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

  void OnPrefetchLikely();

  void OnEnterBackForwardCache();
  void OnRestoreFromBackForwardCache(
      content::NavigationHandle* navigation_handle);

  // Called when the page tracked was just activated after being prerendered.
  void DidActivatePrerenderedPage(content::NavigationHandle* navigation_handle);

  // Called when the previewed page was activated for the tab promotion.
  void DidActivatePreviewedPage(base::TimeTicks activation_time);

  // Called when V8 per-frame memory usage updates are available.
  void OnV8MemoryChanged(const std::vector<MemoryUpdate>& memory_updates);

  // Called when a `SharedStorageWorkletHost` is created.
  void OnSharedStorageWorkletHostCreated();

  // Called when `sharedStorage.selectURL()` is called for some frame on the
  // page tracked.
  void OnSharedStorageSelectURLCalled();

  // Called when a Fledge auction completes.
  void OnAdAuctionComplete(bool is_server_auction,
                           bool is_on_device_auction,
                           content::AuctionResult result);

  // Checks if this tracker is for outermost pages.
  bool IsOutermostTracker() const { return !parent_tracker_; }

  void UpdateMetrics(content::RenderFrameHost* render_frame_host,
                     mojom::PageLoadTimingPtr new_timing,
                     mojom::FrameMetadataPtr new_metadata,
                     const std::vector<blink::UseCounterFeature>& new_features,
                     const std::vector<mojom::ResourceDataUpdatePtr>& resources,
                     mojom::FrameRenderDataUpdatePtr render_data,
                     mojom::CpuTimingPtr new_cpu_timing,
                     mojom::InputTimingPtr input_timing_delta,
                     const std::optional<blink::SubresourceLoadMetrics>&
                         subresource_load_metrics,
                     mojom::SoftNavigationMetricsPtr soft_navigation_metrics);

  void AddCustomUserTimings(
      std::vector<mojom::CustomUserTimingMarkPtr> custom_timings);

  // Sets RenderFrameHost for the main frame of the page this tracker instance
  // is bound. This is called on moving the tracker to the active / inactive
  // tracker list after the provisional load is committed.
  void SetPageMainFrame(content::RenderFrameHost* rfh);

  // Records the fact link navigation from the tracking page happens.
  void RecordLinkNavigation();

  // Gets a bound ukm::SourceId without any check for testing.
  ukm::SourceId GetPageUkmSourceIdForTesting() const { return source_id_; }

  // Obtains a weak pointer for this instance.
  base::WeakPtr<PageLoadTracker> GetWeakPtr();

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

  // Given a |time|, returns the duration between |navigation_start_| and
  // |time|. |time| must be greater than or equal to |navigation_start_|.
  // Returns nullopt if and only if the |time| passed is nullopt.
  std::optional<base::TimeDelta> DurationSinceNavigationStartForTime(
      const std::optional<base::TimeTicks>& time) const;

  using InvokeCallback =
      base::RepeatingCallback<PageLoadMetricsObserver::ObservePolicy(
          PageLoadMetricsObserverInterface*)>;
  void InvokeAndPruneObservers(const char* trace_name,
                               InvokeCallback callback,
                               bool permit_forwarding);

  // Whether we stopped tracking this navigation after it was initiated. We may
  // stop tracking a navigation if it doesn't meet the criteria for tracking
  // metrics in DidFinishNavigation.
  bool did_stop_tracking_;

  int64_t navigation_id_;

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
  std::optional<base::TimeTicks> first_background_time_;
  std::optional<base::TimeTicks> first_foreground_time_;
  std::vector<BackForwardCacheRestore> back_forward_cache_restores_;
  const bool started_in_foreground_;
  PrerenderingState prerendering_state_ = PrerenderingState::kNoPrerendering;
  // Holds the page's visibility at activation.
  PageVisibility visibility_at_activation_ = PageVisibility::kNotInitialized;
  std::optional<base::TimeDelta> activation_start_ = std::nullopt;

  mojom::PageLoadTimingPtr last_dispatched_merged_page_timing_;

  std::optional<content::GlobalRequestID> navigation_request_id_;

  // Whether this page load was user initiated.
  UserInitiatedInfo user_initiated_info_;

  // Keeps track of actively loading resources on the page.
  ResourceTracker resource_tracker_;

  // Interface to chrome features. Must outlive the class.
  const raw_ptr<PageLoadMetricsEmbedderInterface> embedder_interface_;

  // Holds active PageLoadMetricsObserverInterface inheritances' instances bound
  // to the tracking page.
  std::vector<std::unique_ptr<PageLoadMetricsObserverInterface>> observers_;

  // Observer's name pointer to instance map. Can be raw_ptr as the instance is
  // owned `observers` above, and is removed from the map on destruction.
  base::flat_map<const char*, raw_ptr<PageLoadMetricsObserverInterface>>
      observers_map_;

  PageLoadMetricsUpdateDispatcher metrics_update_dispatcher_;

  ukm::SourceId source_id_;

  const raw_ptr<content::WebContents> web_contents_;

  // Holds the RenderFrameHost for the main frame of the page that this tracker
  // instance is bound. Safe to use raw_ptr as the tracker instance is accessed
  // via a map that uses the RenderFrameHost as the key while it's valid.
  raw_ptr<content::RenderFrameHost, AcrossTasksDanglingUntriaged>
      page_main_frame_;

  const bool is_first_navigation_in_web_contents_;
  const bool is_origin_visit_;
  bool is_terminal_visit_ = true;

  page_load_metrics::LargestContentfulPaintHandler
      largest_contentful_paint_handler_;
  page_load_metrics::LargestContentfulPaintHandler
      experimental_largest_contentful_paint_handler_;

  mojom::SoftNavigationMetricsPtr soft_navigation_metrics_;

  GURL potential_soft_navigation_url_;

  ukm::SourceId potential_soft_navigation_source_id_ = ukm::kInvalidSourceId;
  ukm::SourceId previous_soft_navigation_source_id_ = ukm::kInvalidSourceId;

  const internal::PageLoadTrackerPageType page_type_;

  const base::WeakPtr<PageLoadTracker> parent_tracker_;

  base::WeakPtrFactory<PageLoadTracker> weak_factory_{this};
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_TRACKER_H_
