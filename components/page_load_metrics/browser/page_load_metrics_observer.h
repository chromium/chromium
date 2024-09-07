// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_interface.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/use_counter/use_counter_feature.h"
#include "url/gurl.h"

namespace blink {
struct JavaScriptFrameworkDetectionResult;
}  // namespace blink

namespace page_load_metrics {

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
  PageRenderData() = default;

  // How much visible elements on the page shifted (bit.ly/lsm-explainer).
  float layout_shift_score = 0;

  // How much visible elements on the page shifted (bit.ly/lsm-explainer),
  // before user input or document scroll. This field's meaning is context-
  // dependent (see comments on page_render_data_ and main_frame_render_data_
  // in PageLoadMetricsUpdateDispatcher).
  float layout_shift_score_before_input_or_scroll = 0;
};

// Information related to layout shift normalization for different strategies.
struct NormalizedCLSData {
  // Maximum CLS of session windows. The gap between two consecutive shifts is
  // not bigger than 1000ms and the maximum window size is 5000ms.
  float session_windows_gap1000ms_max5000ms_max_cls = 0.0;

  // If true, will not report the data in UKM.
  bool data_tainted = false;
};

// Base class for PageLoadMetrics observers. All instances of this class are
// owned by the PageLoadTracker tracking a page load. The page would be a
// primary page, Prerendering page, FencedFrames page, or pages for new other
// features based on MPArch.
class PageLoadMetricsObserver : public PageLoadMetricsObserverInterface {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class LargestContentState {
    kReported = 0,
    kLargestImageLoading = 1,
    kNotFound = 2,
    kFoundButNotReported = 3,
    kMaxValue = kFoundButNotReported,
  };

  PageLoadMetricsObserver();
  ~PageLoadMetricsObserver() override;

  static bool IsStandardWebPageMimeType(const std::string& mime_type);

  // PageLoadMetricsObserverInterface implementation:
  const PageLoadMetricsObserverDelegate& GetDelegate() const override;
  void SetDelegate(PageLoadMetricsObserverDelegate*) override;
  const char* GetObserverName() const override;
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnPreviewStart(content::NavigationHandle* navigation_handle,
                               const GURL& currently_committed_url) override;
  ObservePolicy OnNavigationHandleTimingUpdated(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy OnRedirect(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void OnDidInternalNavigationAbort(
      content::NavigationHandle* navigation_handle) override {}
  void ReadyToCommitNextNavigation(
      content::NavigationHandle* navigation_handle) override {}
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) override {}
  void OnCommitSameDocumentNavigation(
      content::NavigationHandle* navigation_handle) override {}
  ObservePolicy OnHidden(const mojom::PageLoadTiming& timing) override;
  ObservePolicy OnShown() override;
  ObservePolicy OnEnterBackForwardCache(
      const mojom::PageLoadTiming& timing) override;
  void OnRestoreFromBackForwardCache(
      const mojom::PageLoadTiming& timing,
      content::NavigationHandle* navigation_handle) override {}
  ObservePolicy ShouldObserveMimeType(
      const std::string& mime_type) const override;
  ObservePolicy ShouldObserveScheme(const GURL& url) const override;
  void OnTimingUpdate(content::RenderFrameHost* subframe_rfh,
                      const mojom::PageLoadTiming& timing) override {}
  void OnSoftNavigationUpdated(const mojom::SoftNavigationMetrics&) override {}
  void OnInputTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const mojom::InputTiming& input_timing_delta) override {}
  void OnPageInputTimingUpdate(uint64_t num_interactions) override {}
  void OnPageRenderDataUpdate(const mojom::FrameRenderDataUpdate& render_data,
                              bool is_main_frame) override {}
  void OnSubFrameRenderDataUpdate(
      content::RenderFrameHost* subframe_rfh,
      const mojom::FrameRenderDataUpdate& render_data) override {}
  void OnCpuTimingUpdate(content::RenderFrameHost* subframe_rfh,
                         const mojom::CpuTiming& timing) override {}
  void OnUserInput(const blink::WebInputEvent& event,
                   const mojom::PageLoadTiming& timing) override {}
  void OnDomContentLoadedEventStart(
      const mojom::PageLoadTiming& timing) override {}
  void OnLoadEventStart(const mojom::PageLoadTiming& timing) override {}
  void OnParseStart(const mojom::PageLoadTiming& timing) override {}
  void OnParseStop(const mojom::PageLoadTiming& timing) override {}
  void OnConnectStart(const mojom::PageLoadTiming& timing) override {}
  void OnDomainLookupStart(const mojom::PageLoadTiming& timing) override {}
  void OnDomainLookupEnd(const mojom::PageLoadTiming& timing) override {}
  void OnFirstPaintInPage(const mojom::PageLoadTiming& timing) override {}
  void OnFirstImagePaintInPage(const mojom::PageLoadTiming& timing) override {}
  void OnFirstContentfulPaintInPage(
      const mojom::PageLoadTiming& timing) override {}
  void OnFirstPaintAfterBackForwardCacheRestoreInPage(
      const mojom::BackForwardCacheTiming& timing,
      size_t index) override {}
  void OnFirstInputAfterBackForwardCacheRestoreInPage(
      const mojom::BackForwardCacheTiming& timing,
      size_t index) override {}
  void OnRequestAnimationFramesAfterBackForwardCacheRestoreInPage(
      const mojom::BackForwardCacheTiming& timing,
      size_t index) override {}
  void OnFirstMeaningfulPaintInMainFrameDocument(
      const mojom::PageLoadTiming& timing) override {}
  void OnFirstInputInPage(const mojom::PageLoadTiming& timing) override {}
  void OnLoadingBehaviorObserved(content::RenderFrameHost* rfh,
                                 int behavior_flags) override {}
  void OnJavaScriptFrameworksObserved(
      content::RenderFrameHost* rfh,
      const blink::JavaScriptFrameworkDetectionResult&) override {}
  void OnFeaturesUsageObserved(
      content::RenderFrameHost* rfh,
      const std::vector<blink::UseCounterFeature>& features) override {}
  void SetUpSharedMemoryForSmoothness(
      const base::ReadOnlySharedMemoryRegion& shared_memory) override {}
  void OnResourceDataUseObserved(
      content::RenderFrameHost* rfh,
      const std::vector<mojom::ResourceDataUpdatePtr>& resources) override {}
  void MediaStartedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& video_type,
      content::RenderFrameHost* render_frame_host) override {}
  void OnMainFrameIntersectionRectChanged(
      content::RenderFrameHost* rfh,
      const gfx::Rect& main_frame_intersection_rect) override {}
  void OnMainFrameViewportRectChanged(
      const gfx::Rect& main_frame_viewport_rect) override {}
  void OnMainFrameImageAdRectsChanged(const base::flat_map<int, gfx::Rect>&
                                          main_frame_image_ad_rects) override {}
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const mojom::PageLoadTiming& timing) override;
  void OnComplete(const mojom::PageLoadTiming& timing) override {}
  void OnFailedProvisionalLoad(
      const FailedProvisionalLoadInfo& failed_provisional_load_info) override {}
  void OnLoadedResource(
      const ExtraRequestCompleteInfo& extra_request_complete_info) override {}
  void FrameReceivedUserActivation(
      content::RenderFrameHost* render_frame_host) override {}
  void FrameDisplayStateChanged(content::RenderFrameHost* render_frame_host,
                                bool is_display_none) override {}
  void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                        const gfx::Size& frame_size) override {}
  void OnRenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override {}
  void OnSubFrameDeleted(content::FrameTreeNodeId frame_tree_node_id) override {
  }
  void OnCookiesRead(
      const GURL& url,
      const GURL& first_party_url,
      bool blocked_by_policy,
      bool is_ad_tagged,
      const net::CookieSettingOverrides& cookie_setting_overrides,
      bool is_partitioned_access) override {}
  void OnCookieChange(
      const GURL& url,
      const GURL& first_party_url,
      const net::CanonicalCookie& cookie,
      bool blocked_by_policy,
      bool is_ad_tagged,
      const net::CookieSettingOverrides& cookie_setting_overrides,
      bool is_partitioned_access) override {}
  void OnStorageAccessed(const GURL& url,
                         const GURL& first_party_url,
                         bool blocked_by_policy,
                         StorageType access_type) override {}
  void OnPrefetchLikely() override {}
  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override {}
  void DidActivatePreviewedPage(base::TimeTicks activation_time) override {}
  void OnV8MemoryChanged(
      const std::vector<MemoryUpdate>& memory_updates) override {}
  void OnSharedStorageWorkletHostCreated() override {}
  void OnSharedStorageSelectURLCalled() override {}
  void OnCustomUserTimingMarkObserved(
      const std::vector<mojom::CustomUserTimingMarkPtr>& timings) override {}
  void OnAdAuctionComplete(bool is_server_auction,
                           bool is_on_device_auction,
                           content::AuctionResult result) override {}

 private:
  raw_ptr<PageLoadMetricsObserverDelegate> delegate_ = nullptr;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_H_
