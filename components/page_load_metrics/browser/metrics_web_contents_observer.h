// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_METRICS_WEB_CONTENTS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_METRICS_WEB_CONTENTS_OBSERVER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_receiver_set.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/mojom/fetch_api.mojom-forward.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

namespace page_load_metrics {

class PageLoadMetricsEmbedderInterface;
class PageLoadTracker;

// MetricsWebContentsObserver tracks page loads and loading metrics
// related data based on IPC messages received from a
// MetricsRenderFrameObserver.
class MetricsWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<MetricsWebContentsObserver>,
      public content::RenderWidgetHost::InputEventObserver,
      public mojom::PageLoadMetrics {
 public:
  // TestingObserver allows tests to observe MetricsWebContentsObserver state
  // changes. Tests may use TestingObserver to wait until certain state changes,
  // such as the arrivial of PageLoadTiming messages from the render process,
  // have been observed.
  class TestingObserver {
   public:
    explicit TestingObserver(content::WebContents* web_contents);
    virtual ~TestingObserver();

    void OnGoingAway();

    // Some PageLoadTiming messages will race with the navigation
    // commit. OnTrackerCreated() allows tests to manipulate the tracker very
    // early (eg, to add observers) to handle those cases.
    virtual void OnTrackerCreated(PageLoadTracker* tracker) {}

    // In cases where LoadTimingInfo is not needed, waiting until commit is
    // fine.
    virtual void OnCommit(PageLoadTracker* tracker) {}

    virtual void OnRestoredFromBackForwardCache(PageLoadTracker* tracker) {}

    // Returns the observer delegate for the committed load associated with
    // the MetricsWebContentsObserver.
    const PageLoadMetricsObserverDelegate& GetDelegateForCommittedLoad();

   private:
    page_load_metrics::MetricsWebContentsObserver* observer_;

    DISALLOW_COPY_AND_ASSIGN(TestingObserver);
  };

  // Record a set of PageLoadFeatures directly from the browser process. This
  // should only be used for features that were detected browser-side; features
  // sources from the renderer should go via MetricsRenderFrameObserver.
  static void RecordFeatureUsage(content::RenderFrameHost* render_frame_host,
                                 const mojom::PageLoadFeatures& new_features);

  // Note that the returned metrics is owned by the web contents.
  static MetricsWebContentsObserver* CreateForWebContents(
      content::WebContents* web_contents,
      std::unique_ptr<PageLoadMetricsEmbedderInterface> embedder_interface);
  ~MetricsWebContentsObserver() override;

  // Any visibility changes that occur after this method should be ignored since
  // they are just clean up prior to destroying the WebContents instance.
  void WebContentsWillSoonBeDestroyed();

  // content::WebContentsObserver implementation:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void NavigationStopped() override;
  void OnInputEvent(const blink::WebInputEvent& event) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;
  void FrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void MediaStartedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& video_type,
      const content::MediaPlayerId& id) override;
  void WebContentsDestroyed() override;
  void ResourceLoadComplete(
      content::RenderFrameHost* render_frame_host,
      const content::GlobalRequestID& request_id,
      const blink::mojom::ResourceLoadInfo& resource_load_info) override;
  void FrameReceivedFirstUserActivation(
      content::RenderFrameHost* render_frame_host) override;
  void FrameDisplayStateChanged(content::RenderFrameHost* render_frame_host,
                                bool is_display_none) override;
  void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                        const gfx::Size& frame_size) override;
  void OnCookiesAccessed(content::NavigationHandle* navigation,
                         const content::CookieAccessDetails& details) override;
  void OnCookiesAccessed(content::RenderFrameHost* rfh,
                         const content::CookieAccessDetails& details) override;
  void OnStorageAccessed(const GURL& url,
                         const GURL& first_party_url,
                         bool blocked_by_policy,
                         StorageType storage_type);
  void DidActivatePortal(content::WebContents* predecessor_web_contents,
                         base::TimeTicks activation_time) override;

  // These methods are forwarded from the MetricsNavigationThrottle.
  void WillStartNavigationRequest(content::NavigationHandle* navigation_handle);
  void WillProcessNavigationResponse(
      content::NavigationHandle* navigation_handle);

  // Flush any buffered metrics, as part of the metrics subsystem persisting
  // metrics as the application goes into the background. The application may be
  // killed at any time after this method is invoked without further
  // notification.
  void FlushMetricsOnAppEnterBackground();

  // Returns the delegate for the current committed load, required for testing.
  const PageLoadMetricsObserverDelegate& GetDelegateForCommittedLoad();

  // Register / unregister TestingObservers. Should only be called from tests.
  void AddTestingObserver(TestingObserver* observer);
  void RemoveTestingObserver(TestingObserver* observer);

  // public only for testing
  void OnTimingUpdated(
      content::RenderFrameHost* render_frame_host,
      mojom::PageLoadTimingPtr timing,
      mojom::FrameMetadataPtr metadata,
      mojom::PageLoadFeaturesPtr new_features,
      const std::vector<mojom::ResourceDataUpdatePtr>& resources,
      mojom::FrameRenderDataUpdatePtr render_data,
      mojom::CpuTimingPtr cpu_timing,
      mojom::DeferredResourceCountsPtr new_deferred_resource_data,
      mojom::InputTimingPtr input_timing_delta);

  // Informs the observers of the currently committed load that the event
  // corresponding to |event_key| has occurred. This should not be called within
  // WebContentsObserver::DidFinishNavigation methods.
  // This method is subject to change and may be removed in the future.
  void BroadcastEventToObservers(const void* const event_key);

 private:
  friend class content::WebContentsUserData<MetricsWebContentsObserver>;

  MetricsWebContentsObserver(
      content::WebContents* web_contents,
      std::unique_ptr<PageLoadMetricsEmbedderInterface> embedder_interface);

  void WillStartNavigationRequestImpl(
      content::NavigationHandle* navigation_handle);

  // page_load_metrics::mojom::PageLoadMetrics implementation.
  void UpdateTiming(mojom::PageLoadTimingPtr timing,
                    mojom::FrameMetadataPtr metadata,
                    mojom::PageLoadFeaturesPtr new_features,
                    std::vector<mojom::ResourceDataUpdatePtr> resources,
                    mojom::FrameRenderDataUpdatePtr render_data,
                    mojom::CpuTimingPtr cpu_timing,
                    mojom::DeferredResourceCountsPtr new_deferred_resource_data,
                    mojom::InputTimingPtr input_timing) override;
  void SetUpSharedMemoryForSmoothness(
      base::ReadOnlySharedMemoryRegion shared_memory) override;

  // Common part for UpdateThroughput and OnTimingUpdated.
  bool DoesTimingUpdateHaveError();

  void HandleFailedNavigationForTrackedLoad(
      content::NavigationHandle* navigation_handle,
      std::unique_ptr<PageLoadTracker> tracker);

  void HandleCommittedNavigationForTrackedLoad(
      content::NavigationHandle* navigation_handle,
      std::unique_ptr<PageLoadTracker> tracker);

  void FinalizeCurrentlyCommittedLoad(
      content::NavigationHandle* newly_committed_navigation,
      PageLoadTracker* newly_committed_navigation_tracker);

  // Return a PageLoadTracker (either provisional or committed) that matches the
  // given request attributes, or nullptr if there are no matching
  // PageLoadTrackers.
  PageLoadTracker* GetTrackerOrNullForRequest(
      const content::GlobalRequestID& request_id,
      content::RenderFrameHost* render_frame_host_or_null,
      network::mojom::RequestDestination request_destination,
      base::TimeTicks creation_time);

  // Notify all loads, provisional and committed, that we performed an action
  // that might abort them.
  void NotifyPageEndAllLoads(PageEndReason page_end_reason,
                             UserInitiatedInfo user_initiated_info);
  void NotifyPageEndAllLoadsWithTimestamp(PageEndReason page_end_reason,
                                          UserInitiatedInfo user_initiated_info,
                                          base::TimeTicks timestamp,
                                          bool is_certainly_browser_timestamp);

  // Register / Unregister input event callback to given RenderViewHost
  void RegisterInputEventObserver(content::RenderViewHost* host);
  void UnregisterInputEventObserver(content::RenderViewHost* host);

  // Notify aborted provisional loads that a new navigation occurred. This is
  // used for more consistent attribution tracking for aborted provisional
  // loads. This method returns the provisional load that was likely aborted
  // by this navigation, to help instantiate the new PageLoadTracker.
  std::unique_ptr<PageLoadTracker> NotifyAbortedProvisionalLoadsNewNavigation(
      content::NavigationHandle* new_navigation,
      UserInitiatedInfo user_initiated_info);

  // Whether metrics should be tracked, and a PageLoadTracker should be created,
  // for the given main frame navigation.
  bool ShouldTrackMainFrameNavigation(
      content::NavigationHandle* navigation_handle) const;

  void OnBrowserFeatureUsage(content::RenderFrameHost* render_frame_host,
                             const mojom::PageLoadFeatures& new_features);

  // Before deleting PageLoadTracker, check if we need to keep it alive as the
  // page is stored in back-forward cache. The page can either be restored later
  // (we will be notified via DidFinishNavigation and NavigationHandle::
  // IsServedFromBackForwardCache) or will be evicted from the cache (we will be
  // notified via RenderFrameDeleted).
  void MaybeStorePageLoadTrackerForBackForwardCache(
      content::NavigationHandle* next_navigation_handle,
      std::unique_ptr<PageLoadTracker> page_load_tracker);

  // Try to restore a PageLoadTracker when a navigation restores corresponding
  // page from back-forward cache. Returns true if the page was restored.
  bool MaybeRestorePageLoadTrackerForBackForwardCache(
      content::NavigationHandle* navigation_handle);

  // Notify PageLoadTrackers about cookie read or write.
  void OnCookiesAccessedImpl(const content::CookieAccessDetails& details);

  // True if the web contents is currently in the foreground.
  bool in_foreground_;

  // The PageLoadTrackers must be deleted before the |embedder_interface_|,
  // because they hold a pointer to the |embedder_interface_|.
  std::unique_ptr<PageLoadMetricsEmbedderInterface> embedder_interface_;

  // This map tracks all of the navigations ongoing that are not committed
  // yet. Once a navigation is committed, it moves from the map to
  // |committed_load_|. Note that a PageLoadTrackers NavigationHandle is only
  // valid until commit time, when we remove it from the map.
  std::map<content::NavigationHandle*, std::unique_ptr<PageLoadTracker>>
      provisional_loads_;

  // Tracks aborted provisional loads for a little bit longer than usual (one
  // more navigation commit at the max), in order to better understand how the
  // navigation failed. This is because most provisional loads are destroyed
  // and vanish before we get signal about what caused the abort (new
  // navigation, stop button, etc.).
  std::vector<std::unique_ptr<PageLoadTracker>> aborted_provisional_loads_;

  std::unique_ptr<PageLoadTracker> committed_load_;

  // This is currently set only for the main frame.
  base::ReadOnlySharedMemoryRegion ukm_smoothness_data_;

  // A page can be stored in back-forward cache - in this case its
  // PageLoadTracker should be preserved as well. Here we store PageLoadTracker
  // for each main frame that we navigated away from until we are notified that
  // it is deleted (would happen almost immediately if back-forward cache is not
  // enabled or page is not stored).
  base::flat_map<content::RenderFrameHost*, std::unique_ptr<PageLoadTracker>>
      back_forward_cached_pages_;

  // Has the MWCO observed at least one navigation?
  bool has_navigated_;

  base::ObserverList<TestingObserver>::Unchecked testing_observers_;
  content::WebContentsFrameReceiverSet<mojom::PageLoadMetrics>
      page_load_metrics_receiver_;

  bool web_contents_will_soon_be_destroyed_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(MetricsWebContentsObserver);
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_METRICS_WEB_CONTENTS_OBSERVER_H_
