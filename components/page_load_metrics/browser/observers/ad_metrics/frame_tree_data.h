// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_FRAME_TREE_DATA_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_FRAME_TREE_DATA_H_

#include <stdint.h>

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/frame_data_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom-forward.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/gfx/geometry/size.h"
#include "url/origin.h"

// Resource usage thresholds for the Heavy Ad Intervention feature. These
// numbers are platform specific and are intended to target 1 in 1000 ad iframes
// on each platform, for network and CPU use respectively.
namespace heavy_ad_thresholds {

// Maximum number of network bytes allowed to be loaded by a frame. These
// numbers reflect the 99.9th percentile of the
// PageLoad.Clients.Ads.Bytes.AdFrames.PerFrame.Network histogram on mobile and
// desktop. Additive noise is added to this threshold, see
// AdsPageLoadMetricsObserver::HeavyAdThresholdNoiseProvider.
const int kMaxNetworkBytes = 4.0 * 1024 * 1024;

// CPU thresholds are selected from AdFrameLoad UKM, and are intended to target
// 1 in 1000 ad iframes combined, with each threshold responsible for roughly
// half of those intervention. Maximum number of milliseconds of CPU use allowed
// to be used by a frame.
const int kMaxCpuTime = 60 * 1000;

// Maximum percentage of CPU utilization over a 30 second window allowed.
const int kMaxPeakWindowedPercent = 50;

}  // namespace heavy_ad_thresholds

namespace page_load_metrics {

// The origin of the ad relative to the main frame's origin.
// Note: Logged to UMA, keep in sync with CrossOriginAdStatus in enums.xml.
//   Add new entries to the end, and do not renumber.
enum class OriginStatus {
  kUnknown = 0,
  kSame = 1,
  kCross = 2,
  kMaxValue = kCross,
};

// Origin status further broken down by whether the ad frame tree has a
// frame currently not render-throttled (i.e. is eligible to be painted).
// Note that since creative origin status is based on first contentful paint,
// only ad frame trees with unknown creative origin status can be without any
// frames that are eligible to be painted.
// Note: Logged to UMA, keep in sync with
// CrossOriginCreativeStatusWithThrottling in enums.xml.
// Add new entries to the end, and do not renumber.
enum class OriginStatusWithThrottling {
  kUnknownAndUnthrottled = 0,
  kUnknownAndThrottled = 1,
  kSameAndUnthrottled = 2,
  kCrossAndUnthrottled = 3,
  kMaxValue = kCrossAndUnthrottled,
};

// The type of heavy ad this frame is classified as per the Heavy Ad
// Intervention.
enum class HeavyAdStatus {
  kNone = 0,
  kNetwork = 1,
  kTotalCpu = 2,
  kPeakCpu = 3,
  kMaxValue = kPeakCpu,
};

// Controls what values of HeavyAdStatus will be cause an unload due to the
// intervention.
enum class HeavyAdUnloadPolicy {
  kNetworkOnly = 0,
  kCpuOnly = 1,
  kAll = 2,
};

// Represents how a frame should be treated by the heavy ad intervention.
enum class HeavyAdAction {
  // Nothing should be done, i.e. the ad is not heavy or the intervention is
  // not enabled.
  kNone = 0,
  // The ad should be reported as heavy.
  kReport = 1,
  // The ad should be reported and unloaded.
  kUnload = 2,
  // The frame was ignored, i.e. the blocklist was full or page is a reload.
  kIgnored = 3,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. For any additions, also update the
// corresponding PageEndReason enum in enums.xml.
enum class UserActivationStatus {
  kNoActivation = 0,
  kReceivedActivation = 1,
  kMaxValue = kReceivedActivation,
};

// Whether or not media has been played in this frame. These values are
// persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class MediaStatus {
  kNotPlayed = 0,
  kPlayed = 1,
  kMaxValue = kPlayed,
};

// FrameTreeData represents a frame along with its entire subtree, and is
// typically used to capture an ad creative. It stores frame-specific
// information (such as size, activation status, and origin), which is typically
// specific to the top frame in the tree.
class FrameTreeData final {
 public:
  // |root_frame_tree_node_id| is the root frame of the subtree that
  // FrameTreeData stores information for.
  explicit FrameTreeData(content::FrameTreeNodeId root_frame_tree_node_id,
                         int heavy_ad_network_threshold_noise);
  ~FrameTreeData();

  // Processes a resource load in frame, calling ResourceLoadAggregator.
  void ProcessResourceLoadInFrame(const mojom::ResourceDataUpdatePtr& resource,
                                  int process_id,
                                  const ResourceTracker& resource_tracker);

  // Adjusts ad bytes after call to ProcessResourceLoadInFrame, calling
  // ResourceLoadAggregator.
  void AdjustAdBytes(int64_t unaccounted_ad_bytes, ResourceMimeType mime_type);

  // Updates the cpu usage of this frame.
  void UpdateCpuUsage(base::TimeTicks update_time, base::TimeDelta update);

  // Get the total cpu usage of this frame;
  base::TimeDelta GetTotalCpuUsage() const;

  // Update the metadata of this frame if it is being navigated.
  void UpdateForNavigation(content::RenderFrameHost* render_frame_host);

  // Returns how the frame should be treated by the heavy ad intervention.
  // This intervention is triggered when the frame is considered heavy, has not
  // received user gesture, and the intervention feature is enabled. This
  // returns an action the first time the criteria is met, and false afterwards.
  HeavyAdAction MaybeTriggerHeavyAdIntervention();

  // Updates the max frame depth of this frames tree given the newly seen child
  // frame.
  void MaybeUpdateFrameDepth(content::RenderFrameHost* render_frame_host);

  // Updates the recorded bytes of memory used.
  void UpdateMemoryUsage(int64_t delta_bytes);

  // Returns whether the frame should be recorded for UKMs and UMA histograms.
  // A frame should be recorded if it has non-zero bytes or non-zero CPU usage
  // (or both).
  bool ShouldRecordFrameForMetrics() const;

  // Construct and record an AdFrameLoad UKM event for this frame. Only records
  // events for frames that have non-zero bytes.
  void RecordAdFrameLoadUkmEvent(ukm::SourceId source_id) const;

  // Returns the corresponding enum value to split the creative origin status
  // by whether any frame in the ad frame tree is throttled.
  OriginStatusWithThrottling GetCreativeOriginStatusWithThrottling() const;

  // Get the cpu usage for the appropriate activation period.
  base::TimeDelta GetActivationCpuUsage(UserActivationStatus status) const {
    return cpu_usage_[static_cast<size_t>(status)];
  }

  content::FrameTreeNodeId root_frame_tree_node_id() const {
    return root_frame_tree_node_id_;
  }

  OriginStatus origin_status() const { return origin_status_; }

  OriginStatus creative_origin_status() const {
    return creative_origin_status_;
  }

  std::optional<base::TimeDelta> first_eligible_to_paint() const {
    return first_eligible_to_paint_;
  }

  std::optional<base::TimeDelta> earliest_first_contentful_paint() const {
    return earliest_first_contentful_paint_;
  }

  std::optional<base::TimeDelta> earliest_fcp_since_top_nav_start() const {
    return earliest_fcp_since_top_nav_start_;
  }

  // Sets the size of the frame and updates its visibility state.
  void SetFrameSize(gfx::Size frame_size_);

  // Sets the display state of the frame and updates its visibility state.
  void SetDisplayState(bool is_display_none);

  UserActivationStatus user_activation_status() const {
    return user_activation_status_;
  }

  FrameVisibility visibility() const { return visibility_; }

  gfx::Size frame_size() const { return frame_size_; }

  bool is_display_none() const { return is_display_none_; }

  MediaStatus media_status() const { return media_status_; }

  void set_media_status(MediaStatus media_status) {
    media_status_ = media_status;
  }

  // Records that the sticky user activation bit has been set on the frame.
  // Cannot be unset.
  void set_received_user_activation() {
    user_activation_status_ = UserActivationStatus::kReceivedActivation;
  }

  void set_creative_origin_status(OriginStatus creative_origin_status) {
    creative_origin_status_ = creative_origin_status;
  }

  void SetFirstEligibleToPaint(std::optional<base::TimeDelta> time_stamp);

  // Returns whether a new FCP is set.
  bool SetEarliestFirstContentfulPaint(
      std::optional<base::TimeDelta> time_stamp);

  void SetEarliestFirstContentfulPaintSinceTopNavStart(
      base::TimeDelta time_since_top_nav_start);

  HeavyAdStatus heavy_ad_status_with_noise() const {
    return heavy_ad_status_with_noise_;
  }

  HeavyAdStatus heavy_ad_status_with_policy() const {
    return heavy_ad_status_with_policy_;
  }

  void set_heavy_ad_action(HeavyAdAction heavy_ad_action) {
    heavy_ad_action_ = heavy_ad_action;
  }

  // Accessor for the total resource data of the frame tree.
  const ResourceLoadAggregator& resource_data() const { return resource_data_; }

  // Accessor for the peak windowed cpu usage of the frame tree.
  int peak_windowed_cpu_percent() const {
    return peak_cpu_.peak_windowed_percent();
  }

  base::WeakPtr<FrameTreeData> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Updates whether or not this frame meets the criteria for visibility.
  void UpdateFrameVisibility();

  // Computes whether this frame meets the criteria for being a heavy frame for
  // the heavy ad intervention and returns the type of threshold hit if any.
  // If |use_network_threshold_noise| is set,
  // |heavy_ad_network_threshold_noise_| is added to the network threshold when
  // computing the status. |policy| controls which thresholds are used when
  // computing the status.
  HeavyAdStatus ComputeHeavyAdStatus(bool use_network_threshold_noise,
                                     HeavyAdUnloadPolicy policy) const;

  // The frame tree node id of root frame of the subtree that |this| is
  // tracking information for.
  const content::FrameTreeNodeId root_frame_tree_node_id_;

  // TODO(ericrobinson): May want to move this to ResourceLoadAggregator.
  // Number of resources loaded by the frame (both complete and incomplete).
  int num_resources_ = 0;

  // The depth of this FrameTreeData's root frame.
  unsigned int root_frame_depth_ = 0;

  // The max depth of this frames frame tree.
  unsigned int frame_depth_ = 0;

  // The origin status of the ad frame for the creative.
  OriginStatus origin_status_ = OriginStatus::kUnknown;

  // The origin status of the creative content.
  OriginStatus creative_origin_status_ = OriginStatus::kUnknown;

  // Whether or not the frame is set to not display.
  bool is_display_none_ = false;

  // Whether or not a creative is large enough to be visible by the user.
  FrameVisibility visibility_ = FrameVisibility::kVisible;

  // The size of the frame.
  gfx::Size frame_size_;

  // Whether or not the frame has started media playing.
  MediaStatus media_status_ = MediaStatus::kNotPlayed;

  // Earliest time that any frame in the ad frame tree has reported
  // as being eligible to paint, or null if all frames are currently
  // render-throttled and there hasn't been a first paint. Note that this
  // timestamp and the implied throttling status are best-effort.
  std::optional<base::TimeDelta> first_eligible_to_paint_;

  // The smallest FCP seen for any any frame in this ad frame tree, if a
  // frame has painted.
  std::optional<base::TimeDelta> earliest_first_contentful_paint_;

  // The smallest FCP time seen for any frame in this ad frame tree less the
  // time from top-frame navigation start.
  std::optional<base::TimeDelta> earliest_fcp_since_top_nav_start_;

  // Indicates whether or not this frame met the criteria for the heavy ad
  // intervention with additional additive noise for the
  // network threshold. A frame can be considered a heavy ad by
  // |heavy_ad_status_| but not |heavy_ad_status_with_noise_|. The noised
  // threshold is used when determining whether to actually trigger the
  // intervention.
  HeavyAdStatus heavy_ad_status_with_noise_ = HeavyAdStatus::kNone;

  // Same as |heavy_ad_status_with_noise_| but selectively uses thresholds based
  // on a field trial param. This status is used to control when the
  // intervention fires.
  HeavyAdStatus heavy_ad_status_with_policy_ = HeavyAdStatus::kNone;

  // The action taken on this frame by the heavy ad intervention if any.
  HeavyAdAction heavy_ad_action_ = HeavyAdAction::kNone;

  // Number of bytes of noise that should be added to the network threshold.
  const int heavy_ad_network_threshold_noise_;

  // Whether or not the frame has been activated (clicked on).
  UserActivationStatus user_activation_status_ =
      UserActivationStatus::kNoActivation;

  // The cpu usage for both the activated and unactivated time periods.
  base::TimeDelta
      cpu_usage_[static_cast<size_t>(UserActivationStatus::kMaxValue) + 1];

  // The resource data for this frame tree.
  ResourceLoadAggregator resource_data_;

  // The peak cpu usage for this frame tree.
  PeakCpuAggregator peak_cpu_;

  // Memory usage by v8 in this ad frame tree.
  MemoryUsageAggregator memory_usage_;

  // Owns weak pointers to the instance.
  base::WeakPtrFactory<FrameTreeData> weak_ptr_factory_{this};
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_FRAME_TREE_DATA_H_
