// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/cookies/canonical_cookie.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace page_load_metrics {

// Storage types reported to page load metrics observers on storage
// accesses.
enum class StorageType {
  kLocalStorage,
  kSessionStorage,
  kFileSystem,
  kIndexedDb,
  kCacheStorage
};

// Information related to failed provisional loads.
struct FailedProvisionalLoadInfo {
  FailedProvisionalLoadInfo(base::TimeDelta interval, net::Error error);
  ~FailedProvisionalLoadInfo();

  base::TimeDelta time_to_failed_provisional_load;
  net::Error error;
};

// Information related to whether an associated action, such as a navigation or
// an abort, was initiated by a user. Clicking a link or tapping on a UI
// element are examples of user initiation actions.
struct UserInitiatedInfo {
  static UserInitiatedInfo NotUserInitiated() {
    return UserInitiatedInfo(false, false, false);
  }

  static UserInitiatedInfo BrowserInitiated() {
    return UserInitiatedInfo(true, false, false);
  }

  static UserInitiatedInfo RenderInitiated(bool user_gesture,
                                           bool user_input_event) {
    return UserInitiatedInfo(false, user_gesture, user_input_event);
  }

  // Whether the associated action was initiated from the browser process, as
  // opposed to from the render process. We generally assume that all actions
  // initiated from the browser process are user initiated.
  bool browser_initiated;

  // Whether the associated action was initiated by a user, according to user
  // gesture tracking in content and Blink, as reported by NavigationHandle.
  // This is based on the heuristic the popup blocker uses.
  bool user_gesture;

  // Whether an input even directly led to the navigation, according to
  // input start time tracking in the renderer, as reported by NavigationHandle.
  // Note that this metric is still experimental and may not be fully
  // implemented. All known issues are blocking crbug.com/889220. Currently
  // all known gaps affect browser-side navigations.
  bool user_input_event;

 private:
  UserInitiatedInfo(bool browser_initiated,
                    bool user_gesture,
                    bool user_input_event)
      : browser_initiated(browser_initiated),
        user_gesture(user_gesture),
        user_input_event(user_input_event) {}
};

// Information about how the page rendered during the browsing session.
// Derived from the FrameRenderDataUpdate that is sent via UpdateTiming IPC.
struct PageRenderData {
  PageRenderData()
      : layout_shift_score(0), layout_shift_score_before_input_or_scroll(0) {}

  // How much visible elements on the page shifted (bit.ly/lsm-explainer).
  float layout_shift_score;

  // How much visible elements on the page shifted (bit.ly/lsm-explainer),
  // before user input or document scroll.
  float layout_shift_score_before_input_or_scroll;

  // How many LayoutBlock instances were created.
  uint64_t all_layout_block_count = 0;

  // How many LayoutNG-based LayoutBlock instances were created.
  uint64_t ng_layout_block_count = 0;

  // How many times LayoutObject::UpdateLayout() is called.
  uint64_t all_layout_call_count = 0;

  // How many times LayoutNG-based LayoutObject::UpdateLayout() is called.
  uint64_t ng_layout_call_count = 0;
};

// Container for various information about a completed request within a page
// load.
struct ExtraRequestCompleteInfo {
  ExtraRequestCompleteInfo(
      const url::Origin& origin_of_final_url,
      const net::IPEndPoint& remote_endpoint,
      int frame_tree_node_id,
      bool was_cached,
      int64_t raw_body_bytes,
      int64_t original_network_content_length,
      std::unique_ptr<data_reduction_proxy::DataReductionProxyData>
          data_reduction_proxy_data,
      network::mojom::RequestDestination request_destination,
      int net_error,
      std::unique_ptr<net::LoadTimingInfo> load_timing_info);

  ExtraRequestCompleteInfo(const ExtraRequestCompleteInfo& other);

  ~ExtraRequestCompleteInfo();

  // The origin of the final URL for the request (final = after redirects).
  //
  // The full URL is not available, because in some cases the path and query
  // be sanitized away - see https://crbug.com/973885.
  const url::Origin origin_of_final_url;

  // The host (IP address) and port for the request.
  const net::IPEndPoint remote_endpoint;

  // The frame tree node id that initiated the request.
  const int frame_tree_node_id;

  // True if the resource was loaded from cache.
  const bool was_cached;

  // The number of body (not header) prefilter bytes.
  const int64_t raw_body_bytes;

  // The number of body (not header) bytes that the data reduction proxy saw
  // before it compressed the requests.
  const int64_t original_network_content_length;

  // Data related to data saver.
  const std::unique_ptr<data_reduction_proxy::DataReductionProxyData>
      data_reduction_proxy_data;

  // The type of the request as gleaned from the mime type.  This may
  // be more accurate than the type in the ExtraRequestStartInfo since we can
  // examine the type headers that arrived with the request.  During XHRs, we
  // sometimes see resources come back as a different type than we expected.
  const network::mojom::RequestDestination request_destination;

  // The network error encountered by the request, as defined by
  // net/base/net_error_list.h. If no error was encountered, this value will be
  // 0.
  const int net_error;

  // Additional timing information.
  const std::unique_ptr<net::LoadTimingInfo> load_timing_info;
};

// Interface for PageLoadMetrics observers. All instances of this class are
// owned by the PageLoadTracker tracking a page load.
class PageLoadMetricsObserver {
 public:
  // ObservePolicy is used as a return value on some PageLoadMetricsObserver
  // callbacks to indicate whether the observer would like to continue observing
  // metric callbacks. Observers that wish to continue observing metric
  // callbacks should return CONTINUE_OBSERVING; observers that wish to stop
  // observing callbacks should return STOP_OBSERVING. Observers that return
  // STOP_OBSERVING may be deleted.
  enum ObservePolicy {
    CONTINUE_OBSERVING,
    STOP_OBSERVING,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class LargestContentState {
    kReported = 0,
    kLargestImageLoading = 1,
    kNotFound = 2,
    kFoundButNotReported = 3,
    kMaxValue = kFoundButNotReported,
  };

  using FrameTreeNodeId = int;

  virtual ~PageLoadMetricsObserver() {}

  static bool IsStandardWebPageMimeType(const std::string& mime_type);

  // Gets/Sets the delegate. The delegate must outlive the observer and is
  // normally set when the observer is first registered for the page load. The
  // delegate can only be set once.
  const PageLoadMetricsObserverDelegate& GetDelegate() const;
  void SetDelegate(PageLoadMetricsObserverDelegate*);

  // The page load started, with the given navigation handle.
  // currently_committed_url contains the URL of the committed page load at the
  // time the navigation for navigation_handle was initiated, or the empty URL
  // if there was no committed page load at the time the navigation was
  // initiated.
  virtual ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                                const GURL& currently_committed_url,
                                bool started_in_foreground);

  // OnRedirect is triggered when a page load redirects to another URL.
  // The navigation handle holds relevant data for the navigation, but will
  // be destroyed soon after this call. Don't hold a reference to it. This can
  // be called multiple times.
  virtual ObservePolicy OnRedirect(
      content::NavigationHandle* navigation_handle);

  // OnCommit is triggered when a page load commits, i.e. when we receive the
  // first data for the request. The navigation handle holds relevant data for
  // the navigation, but will be destroyed soon after this call. Don't hold a
  // reference to it.
  // Observers that return STOP_OBSERVING will not receive any additional
  // callbacks, and will be deleted after invocation of this method returns.
  virtual ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                                 ukm::SourceId source_id);

  // OnDidInternalNavigationAbort is triggered when the main frame navigation
  // aborts with HTTP responses that don't commit, such as HTTP 204 responses
  // and downloads. Note that |navigation_handle| will be destroyed
  // soon after this call. Don't hold a reference to it.
  virtual void OnDidInternalNavigationAbort(
      content::NavigationHandle* navigation_handle) {}

  // ReadyToCommitNextNavigation is triggered when a frame navigation is
  // ready to commit, but has not yet been committed. This is only called by
  // a PageLoadTracker for a committed load, meaning that this call signals we
  // are ready to commit a navigation to a new page.
  virtual void ReadyToCommitNextNavigation(
      content::NavigationHandle* navigation_handle) {}

  // OnDidFinishSubFrameNavigation is triggered when a sub-frame of the
  // committed page has finished navigating. It has either committed, aborted,
  // was a same document navigation, or has been replaced. It is up to the
  // observer to query |navigation_handle| to determine which happened. Note
  // that |navigation_handle| will be destroyed soon after this call. Don't
  // hold a reference to it.
  virtual void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) {}

  // OnCommitSameDocumentNavigation is triggered when a same-document navigation
  // commits within the main frame of the current page. Note that
  // |navigation_handle| will be destroyed soon after this call. Don't hold a
  // reference to it.
  virtual void OnCommitSameDocumentNavigation(
      content::NavigationHandle* navigation_handle) {}

  // OnHidden is triggered when a page leaves the foreground. It does not fire
  // when a foreground page is permanently closed; for that, listen to
  // OnComplete instead.
  virtual ObservePolicy OnHidden(const mojom::PageLoadTiming& timing);

  // OnShown is triggered when a page is brought to the foreground. It does not
  // fire when the page first loads; for that, listen for OnStart instead.
  virtual ObservePolicy OnShown();

  // OnEnterBackForwardCache is triggered when a page is put into the
  // back-forward cache. This page can be reused in the future for a
  // back-forward navigation, in this case this OnRestoreFromBackForwardCache
  // will be called for this PageLoadMetricsObserver. Note that the page in the
  // back-forward cache can be evicted at any moment, and in this case
  // OnComplete will be called.
  //
  // At the moment, the default implementtion of OnEnterBackForwardCache()
  // invokes OnComplete and returns STOP_OBSERVING, so the page will not be
  // tracked after it is stored in the back-forward cache and after it is
  // restored. Return CONTINUE_OBSERVING explicitly to ensure that you cover the
  // entire lifetime of the page, which is important for cases like tracking
  // feature use counts or total network usage.
  //
  // TODO(hajimehoshi): Consider to remove |timing| argument by adding a
  // function to PageLoadMetricsObserverDelegate. This would require
  // investigation to determine exposing the timing from the delegate would be
  // really safe.
  virtual ObservePolicy OnEnterBackForwardCache(
      const mojom::PageLoadTiming& timing);

  // OnRestoreFromBackForwardCache is triggered when a page is restored from
  // the back-forward cache.
  virtual void OnRestoreFromBackForwardCache(
      const mojom::PageLoadTiming& timing,
      content::NavigationHandle* navigation_handle) {}

  // Called before OnCommit. The observer should return whether it wishes to
  // observe navigations whose main resource has MIME type |mine_type|. The
  // default is to observe HTML and XHTML only. Note that PageLoadTrackers only
  // track XHTML, HTML, and MHTML (related/multipart).
  virtual ObservePolicy ShouldObserveMimeType(
      const std::string& mime_type) const;

  // The callbacks below are only invoked after a navigation commits, for
  // tracked page loads. Page loads that don't meet the criteria for being
  // tracked at the time a navigation commits will not receive any of the
  // callbacks below.

  // OnTimingUpdate is triggered when an updated PageLoadTiming is available at
  // the page (page is essentially main frame, with merged values across all
  // frames for some paint timing values) or subframe level. This method may be
  // called multiple times over the course of the page load. This method is
  // currently only intended for use in testing. Most implementers should
  // implement one of the On* callbacks, such as OnFirstContentfulPaint or
  // OnDomContentLoadedEventStart. Please email loading-dev@chromium.org if you
  // intend to override this method.
  //
  // If |subframe_rfh| is nullptr, the update took place in the main frame.
  virtual void OnTimingUpdate(content::RenderFrameHost* subframe_rfh,
                              const mojom::PageLoadTiming& timing) {}

  // OnRenderDataUpdate is triggered when an updated PageRenderData is available
  // at the subframe level. This method may be called multiple times over the
  // course of the page load.
  virtual void OnSubFrameRenderDataUpdate(
      content::RenderFrameHost* subframe_rfh,
      const mojom::FrameRenderDataUpdate& render_data) {}

  // Triggered when an updated CpuTiming is available at the page or subframe
  // level. This method is intended for monitoring cpu usage and load across
  // the frames on a page during navigation.
  virtual void OnCpuTimingUpdate(content::RenderFrameHost* subframe_rfh,
                                 const mojom::CpuTiming& timing) {}

  // OnUserInput is triggered when a new user input is passed in to
  // web_contents.
  virtual void OnUserInput(const blink::WebInputEvent& event,
                           const mojom::PageLoadTiming& timing) {}

  // The following methods are invoked at most once, when the timing for the
  // associated event first becomes available.
  virtual void OnDomContentLoadedEventStart(
      const mojom::PageLoadTiming& timing) {}
  virtual void OnLoadEventStart(const mojom::PageLoadTiming& timing) {}
  virtual void OnFirstLayout(const mojom::PageLoadTiming& timing) {}
  virtual void OnParseStart(const mojom::PageLoadTiming& timing) {}
  virtual void OnParseStop(const mojom::PageLoadTiming& timing) {}

  // On*PaintInPage(...) are invoked when the first relevant paint in the page,
  // across all frames, is observed.
  virtual void OnFirstPaintInPage(const mojom::PageLoadTiming& timing) {}
  virtual void OnFirstImagePaintInPage(const mojom::PageLoadTiming& timing) {}
  virtual void OnFirstContentfulPaintInPage(
      const mojom::PageLoadTiming& timing) {}

  // These are called once every time when the page is restored from the
  // back-forward cache. |index| indicates |index|-th restore.
  virtual void OnFirstPaintAfterBackForwardCacheRestoreInPage(
      const mojom::BackForwardCacheTiming& timing,
      size_t index) {}
  virtual void OnFirstInputAfterBackForwardCacheRestoreInPage(
      const mojom::BackForwardCacheTiming& timing,
      size_t index) {}

  // Unlike other paint callbacks, OnFirstMeaningfulPaintInMainFrameDocument is
  // tracked per document, and is reported for the main frame document only.
  virtual void OnFirstMeaningfulPaintInMainFrameDocument(
      const mojom::PageLoadTiming& timing) {}

  virtual void OnFirstInputInPage(const mojom::PageLoadTiming& timing) {}

  // Invoked when there is an update to the loading behavior_flags in the given
  // frame.
  virtual void OnLoadingBehaviorObserved(content::RenderFrameHost* rfh,
                                         int behavior_flags) {}

  // Invoked when new use counter features are observed across all frames.
  virtual void OnFeaturesUsageObserved(
      content::RenderFrameHost* rfh,
      const mojom::PageLoadFeatures& features) {}

  // The smoothness metrics is shared over shared-memory. The observer should
  // create a mapping (by calling |shared_memory.Map()|) so that they are able
  // to read from the shared memory.
  virtual void SetUpSharedMemoryForSmoothness(
      const base::ReadOnlySharedMemoryRegion& shared_memory) {}

  // Invoked when there is data use for loading a resource on the page
  // for a given render frame host. This only contains resources that have had
  // new data use since the last callback. Resources loaded from the cache only
  // receive a single update. Multiple updates can be received for the same
  // resource if it is loaded in multiple documents.
  virtual void OnResourceDataUseObserved(
      content::RenderFrameHost* rfh,
      const std::vector<mojom::ResourceDataUpdatePtr>& resources) {}

  // Invoked when there is new information about lazy loaded or deferred
  // resources. |new_deferred_resource_data| only has new deferral/lazy load
  // events since the last update.
  virtual void OnNewDeferredResourceCounts(
      const mojom::DeferredResourceCounts& new_deferred_resource_data) {}

  // Invoked when a media element starts playing.
  virtual void MediaStartedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& video_type,
      content::RenderFrameHost* render_frame_host) {}

  // Invoked when a frame's intersections with page elements changes and an
  // update is received. The main_frame_document_intersection_rect
  // returns an empty rect for out of view subframes and the root document size
  // for the main frame.
  // TODO(crbug/1048175): Expose intersections to observers via shared delegate.
  virtual void OnFrameIntersectionUpdate(
      content::RenderFrameHost* rfh,
      const mojom::FrameIntersectionUpdate& intersection_update) {}

  // Invoked when the UMA metrics subsystem is persisting metrics as the
  // application goes into the background, on platforms where the browser
  // process may be killed after backgrounding (Android). Implementers should
  // persist any metrics that have been buffered in memory in this callback, as
  // the application may be killed at any time after this method is invoked
  // without further notification. Note that this may be called both for
  // provisional loads as well as committed loads. Implementations that only
  // want to track committed loads should check GetDelegate().DidCommit()
  // to determine if the load had committed. If the implementation returns
  // CONTINUE_OBSERVING, this method may be called multiple times per observer,
  // once for each time that the application enters the background.
  //
  // The default implementation does nothing, and returns CONTINUE_OBSERVING.
  virtual ObservePolicy FlushMetricsOnAppEnterBackground(
      const mojom::PageLoadTiming& timing);

  // One of OnComplete or OnFailedProvisionalLoad is invoked for tracked page
  // loads, immediately before the observer is deleted. These callbacks will not
  // be invoked for page loads that did not meet the criteria for being tracked
  // at the time the navigation completed. The PageLoadTiming struct contains
  // timing data. Other useful data collected over the course of the page load
  // is exposed by the observer delegate API. Most observers should not need
  // to implement these callbacks, and should implement the On* timing callbacks
  // instead.

  // OnComplete is invoked for tracked page loads that committed, immediately
  // before the observer is deleted. Observers that implement OnComplete may
  // also want to implement FlushMetricsOnAppEnterBackground, to avoid loss of
  // data if the application is killed while in the background (this happens
  // frequently on Android).
  virtual void OnComplete(const mojom::PageLoadTiming& timing) {}

  // OnFailedProvisionalLoad is invoked for tracked page loads that did not
  // commit, immediately before the observer is deleted.
  virtual void OnFailedProvisionalLoad(
      const FailedProvisionalLoadInfo& failed_provisional_load_info) {}

  // Called whenever a request is loaded for this page load. This is restricted
  // to requests with HTTP or HTTPS only schemes.
  virtual void OnLoadedResource(
      const ExtraRequestCompleteInfo& extra_request_complete_info) {}

  virtual void FrameReceivedFirstUserActivation(
      content::RenderFrameHost* render_frame_host) {}

  // Called when the display property changes on the frame.
  virtual void FrameDisplayStateChanged(
      content::RenderFrameHost* render_frame_host,
      bool is_display_none) {}

  // Called when a frames size changes.
  virtual void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                                const gfx::Size& frame_size) {}

  virtual void OnFrameDeleted(content::RenderFrameHost* render_frame_host) {}

  // Called when a cookie is read for a resource request or by document.cookie.
  virtual void OnCookiesRead(const GURL& url,
                             const GURL& first_party_url,
                             const net::CookieList& cookie_list,
                             bool blocked_by_policy) {}

  // Called when a cookie is set by a header or via document.cookie.
  virtual void OnCookieChange(const GURL& url,
                              const GURL& first_party_url,
                              const net::CanonicalCookie& cookie,
                              bool blocked_by_policy) {}

  // Called when a storage access attempt by the origin |url| to |storage_type|
  // is checked by the content settings manager. |blocked_by_policy| is false
  // when cookie access is not allowed for |url|.
  virtual void OnStorageAccessed(const GURL& url,
                                 const GURL& first_party_url,
                                 bool blocked_by_policy,
                                 StorageType access_type) {}

  // Called when the event corresponding to |event_key| occurs in this page
  // load.
  virtual void OnEventOccurred(const void* const event_key) {}

  // Called when the page tracked was just activated after being loaded inside a
  // portal.
  virtual void DidActivatePortal(base::TimeTicks activation_time) {}

 private:
  PageLoadMetricsObserverDelegate* delegate_ = nullptr;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_H_
