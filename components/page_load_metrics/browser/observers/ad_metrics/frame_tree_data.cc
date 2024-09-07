// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/page_load_metrics/browser/observers/ad_metrics/frame_tree_data.h"

#include <algorithm>
#include <limits>
#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/heavy_ad_intervention/heavy_ad_features.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/ads_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/frame_data_utils.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

namespace page_load_metrics {

namespace {

// A frame with area less than kMinimumVisibleFrameArea is not considered
// visible.
const int kMinimumVisibleFrameArea = 25;

// Controls what types of heavy ads will be unloaded by the intervention.
const base::FeatureParam<int> kHeavyAdUnloadPolicyParam = {
    &heavy_ad_intervention::features::kHeavyAdIntervention, "kUnloadPolicy",
    static_cast<int>(HeavyAdUnloadPolicy::kAll)};

// Calculates the depth from the outermost main page to the given `rfh` beyond
// page boundaries. This is used to ensure that the depth in the created tree is
// same with the actual page/frame tree structure.
unsigned int GetFullFrameDepth(content::RenderFrameHost* rfh) {
  unsigned int depth = rfh->GetFrameDepth();
  while ((rfh = rfh->GetMainFrame()->GetParentOrOuterDocument())) {
    depth += rfh->GetFrameDepth() + 1;
  }
  return depth;
}

}  // namespace

FrameTreeData::FrameTreeData(content::FrameTreeNodeId root_frame_tree_node_id,
                             int heavy_ad_network_threshold_noise)
    : root_frame_tree_node_id_(root_frame_tree_node_id),
      frame_size_(gfx::Size()),
      heavy_ad_network_threshold_noise_(heavy_ad_network_threshold_noise) {}

FrameTreeData::~FrameTreeData() = default;

void FrameTreeData::MaybeUpdateFrameDepth(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host)
    return;
  DCHECK_GE(GetFullFrameDepth(render_frame_host), root_frame_depth_);
  if (GetFullFrameDepth(render_frame_host) - root_frame_depth_ > frame_depth_)
    frame_depth_ = GetFullFrameDepth(render_frame_host) - root_frame_depth_;
}

void FrameTreeData::UpdateMemoryUsage(int64_t delta_bytes) {
  memory_usage_.UpdateUsage(delta_bytes);
}

bool FrameTreeData::ShouldRecordFrameForMetrics() const {
  return resource_data().bytes() != 0 || !GetTotalCpuUsage().is_zero() ||
         memory_usage_.max_bytes_used() > 0;
}

void FrameTreeData::RecordAdFrameLoadUkmEvent(ukm::SourceId source_id) const {
  if (!ShouldRecordFrameForMetrics())
    return;

  auto* ukm_recorder = ukm::UkmRecorder::Get();
  ukm::builders::AdFrameLoad builder(source_id);
  builder
      .SetLoading_NetworkBytes(
          ukm::GetExponentialBucketMinForBytes(resource_data().network_bytes()))
      .SetLoading_CacheBytes2(ukm::GetExponentialBucketMinForBytes(
          (resource_data().bytes() - resource_data().network_bytes())))
      .SetLoading_VideoBytes(ukm::GetExponentialBucketMinForBytes(
          resource_data().GetAdNetworkBytesForMime(ResourceMimeType::kVideo)))
      .SetLoading_JavascriptBytes(ukm::GetExponentialBucketMinForBytes(
          resource_data().GetAdNetworkBytesForMime(
              ResourceMimeType::kJavascript)))
      .SetLoading_ImageBytes(ukm::GetExponentialBucketMinForBytes(
          resource_data().GetAdNetworkBytesForMime(ResourceMimeType::kImage)))
      .SetLoading_NumResources(num_resources_);

  builder.SetCpuTime_Total(GetTotalCpuUsage().InMilliseconds());
  if (user_activation_status() == UserActivationStatus::kReceivedActivation) {
    builder.SetCpuTime_PreActivation(
        GetActivationCpuUsage(UserActivationStatus::kNoActivation)
            .InMilliseconds());
  }

  builder.SetCpuTime_PeakWindowedPercent(peak_cpu_.peak_windowed_percent());

  builder
      .SetVisibility_FrameWidth(
          ukm::GetExponentialBucketMinForCounts1000(frame_size().width()))
      .SetVisibility_FrameHeight(
          ukm::GetExponentialBucketMinForCounts1000(frame_size().height()))
      .SetVisibility_Hidden(is_display_none_);

  builder.SetStatus_CrossOrigin(static_cast<int>(origin_status()))
      .SetStatus_Media(static_cast<int>(media_status()))
      .SetStatus_UserActivation(static_cast<int>(user_activation_status()));

  builder.SetFrameDepth(frame_depth_);

  if (auto earliest_fcp = earliest_first_contentful_paint()) {
    builder.SetTiming_FirstContentfulPaint(earliest_fcp->InMilliseconds());
  }
  builder.Record(ukm_recorder->Get());
}

OriginStatusWithThrottling
FrameTreeData::GetCreativeOriginStatusWithThrottling() const {
  bool is_throttled = !first_eligible_to_paint().has_value();

  switch (creative_origin_status()) {
    case OriginStatus::kUnknown:
      return is_throttled ? OriginStatusWithThrottling::kUnknownAndThrottled
                          : OriginStatusWithThrottling::kUnknownAndUnthrottled;
    case OriginStatus::kSame:
      DCHECK(!is_throttled);
      return OriginStatusWithThrottling::kSameAndUnthrottled;
    case OriginStatus::kCross:
      DCHECK(!is_throttled);
      return OriginStatusWithThrottling::kCrossAndUnthrottled;
    // We expect the above values to cover all cases.
    default:
      NOTREACHED_IN_MIGRATION();
      return OriginStatusWithThrottling::kUnknownAndUnthrottled;
  }
}

void FrameTreeData::SetFirstEligibleToPaint(
    std::optional<base::TimeDelta> time_stamp) {
  if (time_stamp.has_value()) {
    // If the ad frame tree hasn't already received an earlier paint
    // eligibility stamp, mark it as eligible to paint. Since multiple frames
    // may report timestamps, we keep the earliest reported stamp.
    // Note that this timestamp (or lack thereof) is best-effort.
    if (!first_eligible_to_paint_.has_value() ||
        first_eligible_to_paint_.value() > time_stamp.value())
      first_eligible_to_paint_ = time_stamp;
  } else if (!earliest_first_contentful_paint_.has_value()) {
    // If a frame in this ad frame tree has already painted, there is no
    // further need to update paint eligibility. But if nothing has
    // painted and a null value is passed into the setter, that means the
    // frame is now render-throttled and we should reset the paint-eligiblity
    // value.
    first_eligible_to_paint_.reset();
  }
}

bool FrameTreeData::SetEarliestFirstContentfulPaint(
    std::optional<base::TimeDelta> time_stamp) {
  if (!time_stamp.has_value() || time_stamp.value().is_zero())
    return false;

  if (earliest_first_contentful_paint_.has_value() &&
      time_stamp.value() >= earliest_first_contentful_paint_.value())
    return false;

  earliest_first_contentful_paint_ = time_stamp;
  return true;
}

void FrameTreeData::SetEarliestFirstContentfulPaintSinceTopNavStart(
    base::TimeDelta time_since_top_nav_start) {
  if (!earliest_fcp_since_top_nav_start_ ||
      earliest_fcp_since_top_nav_start_ > time_since_top_nav_start) {
    earliest_fcp_since_top_nav_start_ = time_since_top_nav_start;
  }
}

void FrameTreeData::UpdateFrameVisibility() {
  visibility_ =
      !is_display_none_ &&
              frame_size_.GetCheckedArea().ValueOrDefault(
                  std::numeric_limits<int>::max()) >= kMinimumVisibleFrameArea
          ? FrameVisibility::kVisible
          : FrameVisibility::kNonVisible;
}

HeavyAdStatus FrameTreeData::ComputeHeavyAdStatus(
    bool use_network_threshold_noise,
    HeavyAdUnloadPolicy policy) const {
  if (policy == HeavyAdUnloadPolicy::kCpuOnly ||
      policy == HeavyAdUnloadPolicy::kAll) {
    // Check if the frame meets the peak CPU usage threshold.
    if (peak_windowed_cpu_percent() >=
        heavy_ad_thresholds::kMaxPeakWindowedPercent) {
      return HeavyAdStatus::kPeakCpu;
    }

    // Check if the frame meets the absolute CPU time threshold.
    if (GetTotalCpuUsage().InMilliseconds() >= heavy_ad_thresholds::kMaxCpuTime)
      return HeavyAdStatus::kTotalCpu;
  }

  if (policy == HeavyAdUnloadPolicy::kNetworkOnly ||
      policy == HeavyAdUnloadPolicy::kAll) {
    size_t network_threshold =
        heavy_ad_thresholds::kMaxNetworkBytes +
        (use_network_threshold_noise ? heavy_ad_network_threshold_noise_ : 0);

    // Check if the frame meets the network threshold, possible including noise.
    if (resource_data().network_bytes() >= network_threshold)
      return HeavyAdStatus::kNetwork;
  }
  return HeavyAdStatus::kNone;
}

void FrameTreeData::UpdateCpuUsage(base::TimeTicks update_time,
                                   base::TimeDelta update) {
  // Update the overall usage for all of the relevant buckets.
  cpu_usage_[static_cast<size_t>(user_activation_status_)] += update;

  // If the frame has been activated, then we don't update the peak usage.
  if (user_activation_status_ == UserActivationStatus::kReceivedActivation)
    return;

  // Update the peak usage.
  peak_cpu_.UpdatePeakWindowedPercent(update, update_time);
}

base::TimeDelta FrameTreeData::GetTotalCpuUsage() const {
  base::TimeDelta total_cpu_time;
  for (base::TimeDelta cpu_time : cpu_usage_)
    total_cpu_time += cpu_time;
  return total_cpu_time;
}

void FrameTreeData::UpdateForNavigation(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host)
    return;

  SetDisplayState(render_frame_host->IsFrameDisplayNone());
  if (render_frame_host->GetFrameSize())
    SetFrameSize(*(render_frame_host->GetFrameSize()));

  // For frames triggered on render, their origin is their parent's origin.
  origin_status_ =
      AdsPageLoadMetricsObserver::IsFrameSameOriginToOutermostMainFrame(
          render_frame_host)
          ? OriginStatus::kSame
          : OriginStatus::kCross;

  root_frame_depth_ = GetFullFrameDepth(render_frame_host);
}

void FrameTreeData::ProcessResourceLoadInFrame(
    const mojom::ResourceDataUpdatePtr& resource,
    int process_id,
    const ResourceTracker& resource_tracker) {
  content::GlobalRequestID global_id(process_id, resource->request_id);
  if (!resource_tracker.HasPreviousUpdateForResource(global_id))
    num_resources_++;
  resource_data_.ProcessResourceLoad(resource);
}

void FrameTreeData::AdjustAdBytes(int64_t unaccounted_ad_bytes,
                                  ResourceMimeType mime_type) {
  resource_data_.AdjustAdBytes(unaccounted_ad_bytes, mime_type);
}

void FrameTreeData::SetFrameSize(gfx::Size frame_size) {
  frame_size_ = frame_size;
  UpdateFrameVisibility();
}

void FrameTreeData::SetDisplayState(bool is_display_none) {
  is_display_none_ = is_display_none;
  UpdateFrameVisibility();
}

HeavyAdAction FrameTreeData::MaybeTriggerHeavyAdIntervention() {
  // TODO(johnidel): This method currently does a lot of heavy lifting: tracking
  // noised and unnoised metrics, determining feature action, and branching
  // based on configuration. Consider splitting this out and letting AdsPLMO do
  // more of the feature specific logic.
  //
  // If the intervention has already performed an action on this frame, do not
  // perform another. Metrics will have been calculated already.
  if (user_activation_status_ == UserActivationStatus::kReceivedActivation ||
      heavy_ad_action_ != HeavyAdAction::kNone) {
    return HeavyAdAction::kNone;
  }

  if (heavy_ad_status_with_noise_ == HeavyAdStatus::kNone) {
    heavy_ad_status_with_noise_ = ComputeHeavyAdStatus(
        true /* use_network_threshold_noise */, HeavyAdUnloadPolicy::kAll);
  }

  // Only activate the field trial if there is a heavy ad. Getting the feature
  // param value activates the trial, so we cannot limit activating the trial
  // based on the HeavyAdUnloadPolicy. Therefore, we just use a heavy ad of any
  // type as a gate for activating trial.
  if (heavy_ad_status_with_noise_ == HeavyAdStatus::kNone)
    return HeavyAdAction::kNone;

  heavy_ad_status_with_policy_ = ComputeHeavyAdStatus(
      true /* use_network_threshold_noise */,
      static_cast<HeavyAdUnloadPolicy>(kHeavyAdUnloadPolicyParam.Get()));

  if (heavy_ad_status_with_policy_ == HeavyAdStatus::kNone)
    return HeavyAdAction::kNone;

  // Only check if the feature is enabled once we have a heavy ad. This is done
  // to ensure that any experiment for this feature will only be comparing
  // groups who have seen a heavy ad.
  if (!base::FeatureList::IsEnabled(
          heavy_ad_intervention::features::kHeavyAdIntervention)) {
    // If the intervention is not enabled, we return whether reporting is
    // enabled.
    return base::FeatureList::IsEnabled(
               heavy_ad_intervention::features::kHeavyAdInterventionWarning)
               ? HeavyAdAction::kReport
               : HeavyAdAction::kNone;
  }

  return HeavyAdAction::kUnload;
}

}  // namespace page_load_metrics
