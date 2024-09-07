// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_INTERFACE_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_INTERFACE_H_

#include <memory>
#include <string>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/browser/auction_result.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/navigation_discard_reason.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/use_counter/use_counter_feature.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "url/gurl.h"

namespace blink {
struct JavaScriptFrameworkDetectionResult;
}  // namespace blink

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

namespace net {
struct LoadTimingInfo;
}

namespace page_load_metrics {

// Storage types reported to page load metrics observers on storage accesses.
enum class StorageType {
  kLocalStorage,
  kSessionStorage,
  kFileSystem,
  kIndexedDb,
  kCacheStorage
};

// Container for various information about a completed request within a page
// load.
struct ExtraRequestCompleteInfo {
  ExtraRequestCompleteInfo(
      const url::SchemeHostPort& final_url,
      const net::IPEndPoint& remote_endpoint,
      content::FrameTreeNodeId frame_tree_node_id,
      bool was_cached,
      int64_t raw_body_bytes,
      int64_t original_network_content_length,
      network::mojom::RequestDestination request_destination,
      int net_error,
      std::unique_ptr<net::LoadTimingInfo> load_timing_info);

  ExtraRequestCompleteInfo(const ExtraRequestCompleteInfo& other);

  ~ExtraRequestCompleteInfo();

  // The scheme/host/port of the final URL for the request
  // (final = after redirects).
  //
  // The full URL is not available, because in some cases the path and query
  // may be sanitized away - see https://crbug.com/973885.
  const url::SchemeHostPort final_url;

  // The host (IP address) and port for the request.
  const net::IPEndPoint remote_endpoint;

  // The frame tree node id that initiated the request.
  const content::FrameTreeNodeId frame_tree_node_id;

  // True if the resource was loaded from cache.
  const bool was_cached;

  // The number of body (not header) prefilter bytes.
  const int64_t raw_body_bytes;

  // The number of body (not header) bytes that the data reduction proxy saw
  // before it compressed the requests.
  const int64_t original_network_content_length;

  // The type of the request as gleaned from the mime type.  This may
  // be more accurate than the type in the ExtraRequestStartInfo since we can
  // examine the type headers that arrived with the request.  During XHRs, we
  // sometimes see resources come back as a different type than we expected.
  const network::mojom::RequestDestination request_destination;

  // The network error encountered by the request, as defined by
  // net/base/net_error_list.h. If no error was encountered, this value will
  // be 0.
  const int net_error;

  // Additional timing information.
  const std::unique_ptr<net::LoadTimingInfo> load_timing_info;
};

// Information related to failed provisional loads.
struct FailedProvisionalLoadInfo {
  FailedProvisionalLoadInfo(base::TimeDelta interval,
                            net::Error error,
                            content::NavigationDiscardReason discard_reason);
  ~FailedProvisionalLoadInfo();

  base::TimeDelta time_to_failed_provisional_load;
  net::Error error;
  content::NavigationDiscardReason discard_reason;
};

// Struct for storing per-frame memory update data.
struct MemoryUpdate {
  content::GlobalRenderFrameHostId routing_id;
  int64_t delta_bytes;
  MemoryUpdate(content::GlobalRenderFrameHostId id, int64_t delta);
};

// Interface for PageLoadMetrics observers. Only PageLoadMetricsForwardObserver
// should inherit this interface directly, and others should do
// PageLoadMetricsObserver class.
// All virtual methods in this class should be pure virtual.
class PageLoadMetricsObserverInterface {
 public:
  // ObservePolicy is used as a return value on some PageLoadMetricsObserver
  // callbacks to indicate how the observer would like to handle subsequent
  // callbacks. Observers that wish to continue observing metric callbacks
  // should return CONTINUE_OBSERVING; observers that wish to stop observing
  // callbacks should return STOP_OBSERVING; observers that wish to forward
  // callbacks to the one bound with the parent page should return
  // FORWARD_OBSERVING. Observers that return STOP_OBSERVING or
  // FORWARD_OBSERVING may be deleted. If the observer in the parent page
  // receives forward metrics via FORWARD_OBSERVING, and returns STOP_OBSERVING,
  // It just stop observing forward metrics, and still see other callbacks for
  // the orinally bound page.
  // Most events requiring preprocesses, such as lifecycle events, are forwarded
  // to the outer page at the PageLoadTracker layer, and only events that are
  // directly delivered to the observers need FORWARD_OBSERVING. See
  // PageLoadMetricsForwardObserver to know which events need the observer layer
  // forwarding. Eventually, we may treat all forwarding at the PageLoadTracker
  // layer to deprecate the FORWARD_OBSERVING for simplicity. FORWARD_OBSERVING
  // is available only for OnFencedFramesStart().
  //
  // FORWARD_OBSERVING was introduced to migrate existing observers to support
  // FencedFrames. Some events need to use this policy to correct metrics that
  // need observer level forwarding, but most metrics can be gathered by
  // CONTINUE_OBSERVING. You can check PageLoadMetricsForwardObserver's
  // implementation. If it does nothing, CONTINUE_OBSERVING just works for the
  // event. We track FORWARD_OBSERVING users in the following sheet. Please
  // contact toyoshim@chromium.org or kenoss@chromium.org when you need
  // FORWARD_OBSERVING. We will replace the observer level forwarding with the
  // tracker level forwarding so that CONTINUE_OBSERVING just works for all
  // events.
  // https://docs.google.com/spreadsheets/d/1ftmGPs5Q9iqSUKLJiS_hAU3m41iDXFq9p7zfGODmKGg/edit#gid=0
  enum ObservePolicy {
    CONTINUE_OBSERVING,
    STOP_OBSERVING,
    FORWARD_OBSERVING,  // Deprecated. See the detailed comments above.
  };

  PageLoadMetricsObserverInterface();
  virtual ~PageLoadMetricsObserverInterface();

  PageLoadMetricsObserverInterface(const PageLoadMetricsObserverInterface&) =
      delete;
  PageLoadMetricsObserverInterface& operator=(
      const PageLoadMetricsObserverInterface&) = delete;

  // Obtains a weak pointer for this instance.
  base::WeakPtr<PageLoadMetricsObserverInterface> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Returns the observer name. It should points a fixed address that is bound
  // to the class as we use the pointer as a key in a map at PageLoadTracker.
  // Should be implemented when the class needs to return FORWARD_OBSERVING.
  // TODO(crbug.com/40216775): Make all inheritances override this method
  // and make it pure virtual method.
  virtual const char* GetObserverName() const = 0;

  // Gets/Sets the delegate. The delegate must outlive the observer and is
  // normally set when the observer is first registered for the page load. The
  // delegate can only be set once.
  virtual const PageLoadMetricsObserverDelegate& GetDelegate() const = 0;
  virtual void SetDelegate(PageLoadMetricsObserverDelegate*) = 0;

  // The page load started, with the given navigation handle.
  // currently_committed_url contains the URL of the committed page load at the
  // time the navigation for navigation_handle was initiated, or the empty URL
  // if there was no committed page load at the time the navigation was
  // initiated.
  virtual ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                                const GURL& currently_committed_url,
                                bool started_in_foreground) = 0;

  // For FencedFrames pages, OnFencedFramesStart is called instead of OnStart.
  // This method is pure virtual and each observer must explicitly override it
  // and return appropriate policy. Currently, all observers that are not aware
  // of FencedFrames return STOP_OBSERVING not to mix FencedFrames metrics into
  // the existing reports. FencedFrames will show different characteristics as
  // its content is likely a subframe rather than a main frame. A guideline is:
  //
  // - FORWARD_OBSERVING: Default. Use it if no special reason.
  // - CONTINUE_OBSERVING: Use it if the observer want to capture events for
  //   FencedFrames in a different way, e.g. using a FencedFrames variant for
  //   name of histogram.
  // - STOP_OBSERVING: Use it if the observer want to exclude metrics to
  //   FencedFrames from the reports. Even with this policy, FencedFrames still
  //   affect per-outermost page lifecycle events that are preprocessed in the
  //   PageLoadTracker
  //
  // TODO(crbug.com/40222513): FencedFrames support is still in progress. Update
  // the above description once we fixed all subclasses.
  virtual ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) = 0;

  // For prerendered pages, OnPrerenderStart is called instead of OnStart.
  virtual ObservePolicy OnPrerenderStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) = 0;

  // For primary pages in the preview mode, OnPreviewStart is called instead of
  // OnStart. The default implementation in PageLoadMetricsObserver returns
  // STOP_OBSERVING. See b:291867362 to track the project progress.
  virtual ObservePolicy OnPreviewStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) = 0;

  // Called when the NavigationHandleTiming associated with `navigation_handle`
  // has been updated. This is called only for main frame navigations. See the
  // comment at `WebContentsObserver::DidUpdateNavigationHandleTiming()` for
  // more details.
  virtual ObservePolicy OnNavigationHandleTimingUpdated(
      content::NavigationHandle* navigation_handle) = 0;

  // OnRedirect is triggered when a page load redirects to another URL.
  // The navigation handle holds relevant data for the navigation, but will
  // be destroyed soon after this call. Don't hold a reference to it. This can
  // be called multiple times.
  virtual ObservePolicy OnRedirect(
      content::NavigationHandle* navigation_handle) = 0;

  // OnCommit is triggered when a page load commits, i.e. when we receive the
  // first data for the request. The navigation handle holds relevant data for
  // the navigation, but will be destroyed soon after this call. Don't hold a
  // reference to it.
  // Observers that return STOP_OBSERVING will not receive any additional
  // callbacks, and will be deleted after invocation of this method returns.
  virtual ObservePolicy OnCommit(
      content::NavigationHandle* navigation_handle) = 0;

  // OnDidInternalNavigationAbort is triggered when the main frame navigation
  // aborts with HTTP responses that don't commit, such as HTTP 204 responses
  // and downloads. Note that |navigation_handle| will be destroyed
  // soon after this call. Don't hold a reference to it.
  virtual void OnDidInternalNavigationAbort(
      content::NavigationHandle* navigation_handle) = 0;

  // ReadyToCommitNextNavigation is triggered when a frame navigation is
  // ready to commit, but has not yet been committed. This is only called by
  // a PageLoadTracker for a committed load, meaning that this call signals we
  // are ready to commit a navigation to a new page.
  virtual void ReadyToCommitNextNavigation(
      content::NavigationHandle* navigation_handle) = 0;

  // OnDidFinishSubFrameNavigation is triggered when a sub-frame of the
  // committed page has finished navigating. It has either committed, aborted,
  // was a same document navigation, or has been replaced. It is up to the
  // observer to query |navigation_handle| to determine which happened. Note
  // that |navigation_handle| will be destroyed soon after this call. Don't
  // hold a reference to it.
  virtual void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) = 0;

  // OnCommitSameDocumentNavigation is triggered when a same-document navigation
  // commits within the main frame of the current page. Note that
  // |navigation_handle| will be destroyed soon after this call. Don't hold a
  // reference to it.
  virtual void OnCommitSameDocumentNavigation(
      content::NavigationHandle* navigation_handle) = 0;

  // OnHidden is triggered when a page leaves the foreground. It does not fire
  // when a foreground page is permanently closed; for that, listen to
  // OnComplete instead.
  virtual ObservePolicy OnHidden(const mojom::PageLoadTiming& timing) = 0;

  // OnShown is triggered when a page is brought to the foreground. It does not
  // fire when the page first loads; for that, listen for OnStart instead.
  virtual ObservePolicy OnShown() = 0;

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
      const mojom::PageLoadTiming& timing) = 0;

  // OnRestoreFromBackForwardCache is triggered when a page is restored from
  // the back-forward cache.
  virtual void OnRestoreFromBackForwardCache(
      const mojom::PageLoadTiming& timing,
      content::NavigationHandle* navigation_handle) = 0;

  // Called before OnCommit. The observer should return whether it wishes to
  // observe navigations whose main resource has MIME type |mime_type|. The
  // default is to observe HTML and XHTML only. Note that PageLoadTrackers only
  // track XHTML, HTML, and MHTML (related/multipart).
  virtual ObservePolicy ShouldObserveMimeType(
      const std::string& mime_type) const = 0;

  // Called before OnCommit. The observer should return whether it wishes to
  // observe navigations for |url|'s scheme. The default is to observe http and
  // https only.
  virtual ObservePolicy ShouldObserveScheme(const GURL& url) const = 0;

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
                              const mojom::PageLoadTiming& timing) = 0;

  // The callback is invoked when a soft navigation is detected.
  // See https://bit.ly/soft-navigation for more details.
  virtual void OnSoftNavigationUpdated(const mojom::SoftNavigationMetrics&) = 0;

  // OnInputTimingUpdate is triggered when an updated InputTiming is available
  // at the subframe level. This method may be called multiple times over the
  // course of the page load.
  virtual void OnInputTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const mojom::InputTiming& input_timing_delta) = 0;

  // OnPageInputTimingUpdate is triggered when an updated InputTiming is
  // available at the page level.
  virtual void OnPageInputTimingUpdate(uint64_t num_interactions) = 0;

  // OnPageRenderDataChanged is triggered when an updated PageRenderData is
  // available at the page level. This method may be called multiple times over
  // the course of the page load.
  virtual void OnPageRenderDataUpdate(
      const mojom::FrameRenderDataUpdate& render_data,
      bool is_main_frame) = 0;

  // OnRenderDataUpdate is triggered when an updated PageRenderData is available
  // at the subframe level. This method may be called multiple times over the
  // course of the page load.
  virtual void OnSubFrameRenderDataUpdate(
      content::RenderFrameHost* subframe_rfh,
      const mojom::FrameRenderDataUpdate& render_data) = 0;

  // Triggered when an updated CpuTiming is available at the page or subframe
  // level. This method is intended for monitoring cpu usage and load across
  // the frames on a page during navigation.
  virtual void OnCpuTimingUpdate(content::RenderFrameHost* subframe_rfh,
                                 const mojom::CpuTiming& timing) = 0;

  // OnUserInput is triggered when a new user input is passed in to
  // web_contents.
  virtual void OnUserInput(const blink::WebInputEvent& event,
                           const mojom::PageLoadTiming& timing) = 0;

  // The following methods are invoked at most once, when the timing for the
  // associated event first becomes available.
  virtual void OnDomContentLoadedEventStart(
      const mojom::PageLoadTiming& timing) = 0;
  virtual void OnLoadEventStart(const mojom::PageLoadTiming& timing) = 0;
  virtual void OnParseStart(const mojom::PageLoadTiming& timing) = 0;
  virtual void OnParseStop(const mojom::PageLoadTiming& timing) = 0;
  virtual void OnConnectStart(const mojom::PageLoadTiming& timing) = 0;
  virtual void OnDomainLookupStart(const mojom::PageLoadTiming& timing) = 0;
  virtual void OnDomainLookupEnd(const mojom::PageLoadTiming& timing) = 0;

  // On*PaintInPage(...) are invoked when the first relevant paint in the
  // page, across all frames, is observed.
  virtual void OnFirstPaintInPage(const mojom::PageLoadTiming& timing) = 0;
  virtual void OnFirstImagePaintInPage(const mojom::PageLoadTiming& timing) = 0;
  virtual void OnFirstContentfulPaintInPage(
      const mojom::PageLoadTiming& timing) = 0;

  // These are called once every time when the page is restored from the
  // back-forward cache. |index| indicates |index|-th restore.
  virtual void OnFirstPaintAfterBackForwardCacheRestoreInPage(
      const mojom::BackForwardCacheTiming& timing,
      size_t index) = 0;
  virtual void OnFirstInputAfterBackForwardCacheRestoreInPage(
      const mojom::BackForwardCacheTiming& timing,
      size_t index) = 0;

  // This is called several times on requestAnimationFrame after the page is
  // restored from the back-forward cache. The number of the calls is hard-
  // coded as WebPerformance::
  // kRequestAnimationFramesToRecordAfterBackForwardCacheRestore.
  virtual void OnRequestAnimationFramesAfterBackForwardCacheRestoreInPage(
      const mojom::BackForwardCacheTiming& timing,
      size_t index) = 0;

  // Unlike other paint callbacks, OnFirstMeaningfulPaintInMainFrameDocument is
  // tracked per document, and is reported for the main frame document only.
  virtual void OnFirstMeaningfulPaintInMainFrameDocument(
      const mojom::PageLoadTiming& timing) = 0;

  virtual void OnFirstInputInPage(const mojom::PageLoadTiming& timing) = 0;

  // Invoked when there is an update to the loading behavior_flags in the given
  // frame.
  virtual void OnLoadingBehaviorObserved(content::RenderFrameHost* rfh,
                                         int behavior_flags) = 0;

  virtual void OnJavaScriptFrameworksObserved(
      content::RenderFrameHost* rfh,
      const blink::JavaScriptFrameworkDetectionResult&) = 0;

  // Invoked when new use counter features are observed across all frames.
  virtual void OnFeaturesUsageObserved(
      content::RenderFrameHost* rfh,
      const std::vector<blink::UseCounterFeature>& features) = 0;

  // The smoothness metrics is shared over shared-memory. The observer should
  // create a mapping (by calling |shared_memory.Map()|) so that they are able
  // to read from the shared memory.
  virtual void SetUpSharedMemoryForSmoothness(
      const base::ReadOnlySharedMemoryRegion& shared_memory) = 0;

  // Invoked when there is data use for loading a resource on the page
  // for a given RenderFrameHost. This only contains resources that have had
  // new data use since the last callback. Resources loaded from the cache only
  // receive a single update. Multiple updates can be received for the same
  // resource if it is loaded in multiple documents.
  virtual void OnResourceDataUseObserved(
      content::RenderFrameHost* rfh,
      const std::vector<mojom::ResourceDataUpdatePtr>& resources) = 0;

  // Invoked when a media element starts playing.
  virtual void MediaStartedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& video_type,
      content::RenderFrameHost* render_frame_host) = 0;

  // For the main frame, called when the main frame's dimensions have changed,
  // e.g. resizing a tab causes the document width to change; loading additional
  // content causes the document height to increase; explicitly changing the
  // height of the body element.
  //
  // For a subframe, called when the intersection rect between the main frame
  // and the subframe has changed, e.g. the subframe is initially added; the
  // subframe's position is updated explicitly or inherently (e.g. sticky
  // position while the page is being scrolled).
  //
  // TODO(crbug.com/40117157): Expose intersections to observers via shared
  // delegate.
  virtual void OnMainFrameIntersectionRectChanged(
      content::RenderFrameHost* rfh,
      const gfx::Rect& main_frame_intersection_rect) = 0;

  // Called when the main frame's viewport rectangle (the viewport dimensions
  // and the scroll position) changed, e.g. the user scrolled the main frame or
  // the viewport dimensions themselves changed. Only invoked on the main frame.
  virtual void OnMainFrameViewportRectChanged(
      const gfx::Rect& main_frame_viewport_rect) = 0;

  // Called when an image ad rectangle changed. An empty `image_ad_rect` is used
  // to signal the removal of the rectangle. Only invoked on the main frame.
  virtual void OnMainFrameImageAdRectsChanged(
      const base::flat_map<int, gfx::Rect>& main_frame_image_ad_rects) = 0;

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
      const mojom::PageLoadTiming& timing) = 0;

  // A destructor of observer is invoked in the following mutually exclusive
  // paths:
  //
  // - (If an ovserver doesn't override OnEnterBackForwardCache) When
  //   OnEnterBackForwardCache is invoked, it calls OnComplete and returns
  //   STOP_OBSERVING, then the ovserver is pruned.
  // - When some callback returned STOP_OBSERVING, the observer is pruned with
  //   no more callback.
  // - When PageLoadTracker destructed, OnComplete is invoked just before
  //   destruction if the load is committed.
  // - When PageLoadTracker destructed, OnFailedProvisionalLoad is invoked just
  //   before destruction if the load is not committed.
  //
  // Observers that implement OnComplete may also want to implement
  // FlushMetricsOnAppEnterBackground, to avoid loss of data if the application
  // is killed while in the background (this happens frequently on Android).
  virtual void OnComplete(const mojom::PageLoadTiming& timing) = 0;
  virtual void OnFailedProvisionalLoad(
      const FailedProvisionalLoadInfo& failed_provisional_load_info) = 0;

  // Called whenever a request is loaded for this page load. This is restricted
  // to requests with HTTP or HTTPS only schemes.
  virtual void OnLoadedResource(
      const ExtraRequestCompleteInfo& extra_request_complete_info) = 0;

  virtual void FrameReceivedUserActivation(
      content::RenderFrameHost* render_frame_host) = 0;

  // Called when the display property changes on the frame.
  virtual void FrameDisplayStateChanged(
      content::RenderFrameHost* render_frame_host,
      bool is_display_none) = 0;

  // Called when a frames size changes.
  virtual void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                                const gfx::Size& frame_size) = 0;

  // OnRenderFrameDeleted is called when RenderFrameHost for a frame is deleted.
  // OnSubFrameDeleted is called when FrameTreeNode for a subframe is deleted.
  // The differences are:
  //
  // - OnRenderFrameDeleted is called for all frames. OnSubFrameDeleted is not
  //   called for main frames. This is because PageLoadTracker is bound with
  //   RenderFrameHost of the main frame and destruction of PageLoadTracker is
  //   earlier than one of FrameTreeNode.
  // - OnRenderFrameDeleted can be called in navigation commit to discard the
  //   previous RenderFrameHost. At that timing, there are two RenderFrameHost
  //   that have the same RenderFrameHost::GetFrameNodeId.
  //
  // Note that navigation may not trigger deletion of RenderFrameHost, e.g. in
  // the case of the page entered to Back/Forward cache. If observer only wants
  // to observe deletion of node, OnSubFrameDeleted is more relevant.
  virtual void OnRenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) = 0;
  virtual void OnSubFrameDeleted(
      content::FrameTreeNodeId frame_tree_node_id) = 0;

  // Called when a cookie is read for a resource request or by document.cookie.
  virtual void OnCookiesRead(
      const GURL& url,
      const GURL& first_party_url,
      bool blocked_by_policy,
      bool is_ad_tagged,
      const net::CookieSettingOverrides& cookie_setting_overrides,
      bool is_partitioned_access) = 0;

  // Called when a cookie is set by a header or via document.cookie.
  virtual void OnCookieChange(
      const GURL& url,
      const GURL& first_party_url,
      const net::CanonicalCookie& cookie,
      bool blocked_by_policy,
      bool is_ad_tagged,
      const net::CookieSettingOverrides& cookie_setting_overrides,
      bool is_partitioned_access) = 0;

  // Called when a storage access attempt by the origin |url| to |storage_type|
  // is checked by the content settings manager. |blocked_by_policy| is false
  // when cookie access is not allowed for |url|.
  virtual void OnStorageAccessed(const GURL& url,
                                 const GURL& first_party_url,
                                 bool blocked_by_policy,
                                 StorageType access_type) = 0;

  // Called when prefetch is likely to occur in this page load.
  virtual void OnPrefetchLikely() = 0;

  // Called when the page tracked was just activated after being prerendered.
  // |navigation_handle| is for the activation navigation.
  virtual void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) = 0;

  // Called when the previewed page is activated for the tab promotion.
  virtual void DidActivatePreviewedPage(base::TimeTicks activation_time) = 0;

  // Called when V8 per-frame memory usage updates are available. Each
  // MemoryUpdate consists of a GlobalRenderFrameHostId and a nonzero int64_t
  // change in bytes used.
  virtual void OnV8MemoryChanged(
      const std::vector<MemoryUpdate>& memory_updates) = 0;

  // Called when a `SharedStorageWorkletHost` is created.
  virtual void OnSharedStorageWorkletHostCreated() = 0;

  // Called when `sharedStorage.selectURL()` is called for some frame on the
  // page tracked.
  virtual void OnSharedStorageSelectURLCalled() = 0;

  // Called when `performance.mark()` is emitted in the main frame except for
  // the standard UserTiming marks `mark_fully_loaded`, `mark_fully_visible`,
  // and `mark_interactive` occur. Those are managed in PageLoadTiming
  // separately and tracked in in a different timing.
  virtual void OnCustomUserTimingMarkObserved(
      const std::vector<mojom::CustomUserTimingMarkPtr>& timings) = 0;

  // Called when a Fledge auction completes.
  virtual void OnAdAuctionComplete(bool is_server_auction,
                                   bool is_on_device_auction,
                                   content::AuctionResult result) = 0;

 private:
  base::WeakPtrFactory<PageLoadMetricsObserverInterface> weak_factory_{this};
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_INTERFACE_H_
