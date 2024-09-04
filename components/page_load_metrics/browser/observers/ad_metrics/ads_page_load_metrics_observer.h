// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_ADS_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_ADS_PAGE_LOAD_METRICS_OBSERVER_H_

#include <bitset>
#include <map>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/scoped_observation.h"
#include "base/time/tick_clock.h"
#include "build/build_config.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_data.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/aggregate_frame_data.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/frame_data_utils.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/frame_tree_data.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/page_ad_density_tracker.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom-forward.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "net/http/http_response_info.h"
#include "services/metrics/public/cpp/ukm_source.h"

namespace heavy_ad_intervention {
class HeavyAdBlocklist;
class HeavyAdService;
}  // namespace heavy_ad_intervention

namespace page_load_metrics {

namespace features {
BASE_DECLARE_FEATURE(kRestrictedNavigationAdTagging);
}

// This observer labels each sub-frame as an ad or not, and keeps track of
// relevant per-frame and whole-page byte statistics.
class AdsPageLoadMetricsObserver
    : public PageLoadMetricsObserver,
      public subresource_filter::SubresourceFilterObserver {
 public:
  using AggregateFrameData = page_load_metrics::AggregateFrameData;
  using FrameTreeData = page_load_metrics::FrameTreeData;
  using ResourceMimeType = page_load_metrics::ResourceMimeType;
  using ApplicationLocaleGetter = base::RepeatingCallback<std::string()>;

  // Helper class that generates a random amount of noise to apply to thresholds
  // for heavy ads. A different noise should be generated for each frame.
  class HeavyAdThresholdNoiseProvider {
   public:
    // |use_noise| indicates whether this provider should give values of noise
    // or just 0. If the heavy ad blocklist mitigation is disabled, |use_noise|
    // should be set to false to provide a deterministic debugging path.
    explicit HeavyAdThresholdNoiseProvider(bool use_noise);
    virtual ~HeavyAdThresholdNoiseProvider() = default;

    // Gets a random amount of noise to add to a threshold. The generated noise
    // is uniform random over the range 0 to kMaxThresholdNoiseBytes. Virtual
    // for testing.
    virtual int GetNetworkThresholdNoiseForFrame() const;

    // Maximum amount of additive noise to add to the network threshold to
    // obscure cross origin resource sizes: 1303 KB.
    static const int kMaxNetworkThresholdNoiseBytes = 1303 * 1024;

   private:
    // Whether to use noise.
    const bool use_noise_;
  };

  // Returns a new AdsPageLoadMetricsObserver. If the feature is disabled it
  // returns nullptr.
  static std::unique_ptr<AdsPageLoadMetricsObserver> CreateIfNeeded(
      content::WebContents* web_contents,
      heavy_ad_intervention::HeavyAdService* heavy_ad_service,
      const ApplicationLocaleGetter& application_local_getter);

  // For a given frame, returns whether or not the frame's url would be
  // considered same origin to the outermost main frame's url.
  static bool IsFrameSameOriginToOutermostMainFrame(
      content::RenderFrameHost* sub_host);

  // |clock| and |blocklist| should be set only by tests. In particular,
  // |blocklist| should be set only if |heavy_ad_service| is null.
  explicit AdsPageLoadMetricsObserver(
      heavy_ad_intervention::HeavyAdService* heavy_ad_service,
      const ApplicationLocaleGetter& application_local_getter,
      base::TickClock* clock = nullptr,
      heavy_ad_intervention::HeavyAdBlocklist* blocklist = nullptr);

  AdsPageLoadMetricsObserver(const AdsPageLoadMetricsObserver&) = delete;
  AdsPageLoadMetricsObserver& operator=(const AdsPageLoadMetricsObserver&) =
      delete;

  ~AdsPageLoadMetricsObserver() override;

  // PageLoadMetricsObserver
  const char* GetObserverName() const override;
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void OnTimingUpdate(content::RenderFrameHost* subframe_rfh,
                      const mojom::PageLoadTiming& timing) override;
  void OnCpuTimingUpdate(content::RenderFrameHost* subframe_rfh,
                         const mojom::CpuTiming& timing) override;
  void ReadyToCommitNextNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const mojom::PageLoadTiming& timing) override;
  void OnComplete(const mojom::PageLoadTiming& timing) override;
  void OnResourceDataUseObserved(
      content::RenderFrameHost* rfh,
      const std::vector<mojom::ResourceDataUpdatePtr>& resources) override;
  void FrameReceivedUserActivation(content::RenderFrameHost* rfh) override;
  void FrameDisplayStateChanged(content::RenderFrameHost* render_frame_host,
                                bool is_display_none) override;
  void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                        const gfx::Size& frame_size) override;
  void MediaStartedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& video_type,
      content::RenderFrameHost* render_frame_host) override;
  void OnMainFrameIntersectionRectChanged(
      content::RenderFrameHost* render_frame_host,
      const gfx::Rect& main_frame_intersection_rect) override;
  void OnMainFrameViewportRectChanged(
      const gfx::Rect& main_frame_viewport_rect) override;
  void OnMainFrameImageAdRectsChanged(
      const base::flat_map<int, gfx::Rect>& main_frame_image_ad_rects) override;
  void OnSubFrameDeleted(content::FrameTreeNodeId frame_tree_node_id) override;
  void OnV8MemoryChanged(
      const std::vector<MemoryUpdate>& memory_updates) override;
  void OnAdAuctionComplete(bool is_server_auction,
                           bool is_on_device_auction,
                           content::AuctionResult result) override;

  void SetHeavyAdThresholdNoiseProviderForTesting(
      std::unique_ptr<HeavyAdThresholdNoiseProvider> noise_provider) {
    heavy_ad_threshold_noise_provider_ = std::move(noise_provider);
  }

  void UpdateAggregateMemoryUsage(int64_t bytes, FrameVisibility visibility);

  void CleanupDeletedFrame(content::FrameTreeNodeId id,
                           FrameTreeData* frame_data,
                           bool update_density_tracker,
                           bool record_metrics);

 private:
  // Object which maps to a FrameTreeData object. This can either own the
  // FrameTreeData object, or hold a reference to a FrameTreeData owned by a
  // different FrameInstance.
  class FrameInstance {
   public:
    // Default constructor to indicate no frame is referenced.
    FrameInstance();
    explicit FrameInstance(std::unique_ptr<FrameTreeData> frame_data);
    explicit FrameInstance(base::WeakPtr<FrameTreeData> frame_data);
    FrameInstance& operator=(FrameInstance&& other) = default;

    FrameInstance(const FrameInstance& other) = delete;
    FrameInstance& operator=(const FrameInstance& other) = delete;

    ~FrameInstance();

    // Returns underlying pointer from |owned_frame_data_|,
    // |unowned_frame_data_| or nullptr.
    FrameTreeData* Get();

    // Returns underlying pointer from |owned_frame_data_| if it exists.
    FrameTreeData* GetOwnedFrame();

   private:
    // Only |owned_frame_data_| or |unowned_frame_data_| can be set at one time.
    // Both can be nullptr.
    std::unique_ptr<FrameTreeData> owned_frame_data_;
    base::WeakPtr<FrameTreeData> unowned_frame_data_;
  };

  // Checks the current page ad density by height for an better ads standard
  // violation. The better ads standard defines ad density violations as any
  // site with more than 30 percent ad density by height.
  void CheckForAdDensityViolation();

  // subresource_filter::SubresourceFilterObserver:
  void OnSubresourceFilterGoingAway() override;
  void OnPageActivationComputed(
      content::NavigationHandle* navigation_handle,
      const subresource_filter::mojom::ActivationState& activation_state)
      override;

  void UpdateAdFrameData(content::NavigationHandle* navigation_handle,
                         bool is_adframe,
                         bool should_ignore_detected_ad);

  // Gets the number of bytes that we may have not attributed to ad
  // resources due to the resource being reported as an ad late.
  int GetUnaccountedAdBytes(int process_id,
                            const mojom::ResourceDataUpdatePtr& resource) const;

  // Updates page level counters for resource loads.
  void ProcessResourceForPage(content::RenderFrameHost* render_frame_host,
                              const mojom::ResourceDataUpdatePtr& resource);
  void ProcessResourceForFrame(content::RenderFrameHost* render_frame_host,
                               const mojom::ResourceDataUpdatePtr& resource);

  void RecordPageResourceTotalHistograms(ukm::SourceId source_id);
  void RecordHistograms(ukm::SourceId source_id);
  void RecordAggregateHistogramsForCpuUsage();
  void RecordAggregateHistogramsForAdTagging(FrameVisibility visibility);
  void RecordAggregateHistogramsForHeavyAds();

  // Should be called on all frames prior to recording any aggregate histograms.
  void RecordPerFrameMetrics(const FrameTreeData& ad_frame_data,
                             ukm::SourceId source_id);
  void RecordPerFrameHistogramsForAdTagging(const FrameTreeData& ad_frame_data);
  void RecordPerFrameHistogramsForCpuUsage(const FrameTreeData& ad_frame_data);
  void RecordPerFrameHistogramsForHeavyAds(const FrameTreeData& ad_frame_data);

  // Checks to see if a resource is waiting for a navigation in the given
  // frame to commit before it can be processed. If so, call
  // OnResourceDataUpdate for the delayed resource.
  void ProcessOngoingNavigationResource(
      content::NavigationHandle* navigation_handle);

  // Find the FrameTreeData object associated with a given FrameTreeNodeId in
  // |ad_frames_data_storage_|.
  FrameTreeData* FindFrameData(content::FrameTreeNodeId id);

  // When a page has reached its limit of heavy ads interventions, will trigger
  // ads interventions for all ads on the page if appropriate.
  void MaybeTriggerStrictHeavyAdIntervention();

  // Triggers the heavy ad intervention page in the target frame if it is safe
  // to do so on this origin, and the frame meets the criteria to be considered
  // a heavy ad. This first sends an intervention report to every affected
  // frame then loads an error page in the root ad frame.
  void MaybeTriggerHeavyAdIntervention(
      content::RenderFrameHost* render_frame_host,
      FrameTreeData* frame_data);

  bool IsBlocklisted(bool report);
  heavy_ad_intervention::HeavyAdBlocklist* GetHeavyAdBlocklist();

  // Maps a frame (by id) to the corresponding FrameInstance. Multiple frame ids
  // can point to the same underlying FrameTreeData. The responsible frame is
  // the top-most frame labeled as an ad in the frame's ancestry, which may be
  // itself. If the frame is not an ad, the id will point to a FrameInstance
  // where FrameInstance::Get() returns nullptr..
  std::map<content::FrameTreeNodeId, FrameInstance> ad_frames_data_;

  std::map<content::FrameTreeNodeId, base::TimeTicks> frame_navigation_starts_;

  int64_t navigation_id_ = -1;
  bool subresource_filter_is_enabled_ = false;

  // When the observer receives report of a document resource loading for a
  // sub-frame before the sub-frame commit occurs, hold onto the resource
  // request info (delay it) until the sub-frame commits.
  std::map<content::FrameTreeNodeId, mojom::ResourceDataUpdatePtr>
      ongoing_navigation_resources_;

  // Per-frame memory usage by V8 in bytes. Memory data is stored for each frame
  // on the page during the navigation.
  std::unordered_map<content::FrameTreeNodeId,
                     uint64_t,
                     content::FrameTreeNodeId::Hasher>
      v8_current_memory_usage_map_;

  // Tracks page-level information for the navigation.
  std::unique_ptr<AggregateFrameData> aggregate_frame_data_;

  // Flag denoting that this observer should no longer monitor changes in
  // display state for frames. This prevents us from receiving the updates when
  // the frame elements are being destroyed in the renderer.
  bool process_display_state_updates_ = true;

  base::ScopedObservation<subresource_filter::SubresourceFilterObserverManager,
                          subresource_filter::SubresourceFilterObserver>
      subresource_observation_{this};

  // The tick clock used to get the current time. Can be replaced by tests.
  raw_ptr<const base::TickClock> clock_;

  // Whether the page load currently being observed is a reload of a previous
  // page.
  bool page_load_is_reload_ = false;

  // Whether the restricted navigation ad tagging feature is enabled on this
  // page load.
  const bool restricted_navigation_ad_tagging_enabled_;

  // Stores whether the heavy ad intervention is blocklisted or not for the user
  // on the URL of this page. Incognito Profiles will cause this to be set to
  // true. Used as a cache to avoid checking the blocklist once the page is
  // blocklisted. Once blocklisted, a page load cannot be unblocklisted.
  std::optional<blocklist::BlocklistReason> heavy_ads_blocklist_reason_;

  // Pointer to the HeavyAdService from which the heavy ad blocklist is obtained
  // in production.
  raw_ptr<heavy_ad_intervention::HeavyAdService> heavy_ad_service_;

  ApplicationLocaleGetter application_locale_getter_;

  // Pointer to the blocklist used to throttle the heavy ad intervention. Can
  // be replaced by tests.
  raw_ptr<heavy_ad_intervention::HeavyAdBlocklist> heavy_ad_blocklist_;

  // Whether the heavy ad privacy mitigations feature is enabled.
  const bool heavy_ad_privacy_mitigations_enabled_;

  // Whether there was a heavy ad on the page at some point.
  bool heavy_ad_on_page_ = false;

  // Whether or not the metrics for this observer have already been recorded.
  // This can occur if the Chrome app is backgrounded.  If so, we continue to
  // keep track of things for interventions, but don't report anything further.
  bool histograms_recorded_ = false;

  std::unique_ptr<HeavyAdThresholdNoiseProvider>
      heavy_ad_threshold_noise_provider_;

  // The maximum ad density measurements for the page during its lifecycle.
  PageAdDensityTracker page_ad_density_tracker_;

  // Tracks number of memory updates received.
  int memory_update_count_ = 0;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_ADS_PAGE_LOAD_METRICS_OBSERVER_H_
