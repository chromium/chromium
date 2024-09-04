// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/ad_metrics/ads_page_load_metrics_observer.h"

#include <algorithm>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/heavy_ad_intervention/heavy_ad_blocklist.h"
#include "components/heavy_ad_intervention/heavy_ad_features.h"
#include "components/heavy_ad_intervention/heavy_ad_helper.h"
#include "components/heavy_ad_intervention/heavy_ad_service.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_memory_tracker.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/browser/resource_tracker.h"
#include "components/page_load_metrics/common/page_end_reason.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_web_contents_helper.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-shared.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace page_load_metrics {

namespace features {

// Enables or disables the restricted navigation ad tagging feature. When
// enabled, the AdTagging heuristic is modified to additional information to
// determine if a frame is an ad. If the frame's navigation url matches an allow
// list rule, it is not an ad.
//
// If a frame's navigation url does not match a blocked rule, but was created by
// ad script and is same domain to the top-level frame, it is not an ad.
//
// Currently this feature only changes AdTagging behavior for metrics recorded
// in AdsPageLoadMetricsObserver, and for triggering the Heavy Ad Intervention.
BASE_FEATURE(kRestrictedNavigationAdTagging,
             "RestrictedNavigationAdTagging",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features

namespace {

using RectId = PageAdDensityTracker::RectId;
using RectType = PageAdDensityTracker::RectType;

#define ADS_HISTOGRAM(suffix, hist_macro, visibility, value)        \
  switch (visibility) {                                             \
    case kNonVisible:                                               \
      hist_macro("PageLoad.Clients.Ads.NonVisible." suffix, value); \
      break;                                                        \
    case kVisible:                                                  \
      hist_macro("PageLoad.Clients.Ads.Visible." suffix, value);    \
      break;                                                        \
    case kAnyVisibility:                                            \
      hist_macro("PageLoad.Clients.Ads." suffix, value);            \
      break;                                                        \
  }

std::string GetHeavyAdReportMessage(const FrameTreeData& frame_data,
                                    bool will_unload_adframe) {
  const char kChromeStatusMessage[] =
      "See "
      "https://www.chromestatus.com/feature/"
      "4800491902992384?utm_source=devtools";
  const char kReportingOnlyMessage[] =
      "A future version of Chrome may remove this ad";
  const char kInterventionMessage[] = "Ad was removed";

  std::string_view intervention_mode =
      will_unload_adframe ? kInterventionMessage : kReportingOnlyMessage;

  switch (frame_data.heavy_ad_status_with_noise()) {
    case HeavyAdStatus::kNetwork:
      return base::StrCat({intervention_mode,
                           " because its network usage exceeded the limit. ",
                           kChromeStatusMessage});
    case HeavyAdStatus::kTotalCpu:
      return base::StrCat({intervention_mode,
                           " because its total CPU usage exceeded the limit. ",
                           kChromeStatusMessage});
    case HeavyAdStatus::kPeakCpu:
      return base::StrCat({intervention_mode,
                           " because its peak CPU usage exceeded the limit. ",
                           kChromeStatusMessage});
    case HeavyAdStatus::kNone:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}

const char kDisallowedByBlocklistHistogramName[] =
    "PageLoad.Clients.Ads.HeavyAds.DisallowedByBlocklist";

void RecordHeavyAdInterventionDisallowedByBlocklist(bool disallowed) {
  base::UmaHistogramBoolean(kDisallowedByBlocklistHistogramName, disallowed);
}

using ResourceMimeType = AdsPageLoadMetricsObserver::ResourceMimeType;

blink::mojom::HeavyAdReason GetHeavyAdReason(HeavyAdStatus status) {
  switch (status) {
    case HeavyAdStatus::kNetwork:
      return blink::mojom::HeavyAdReason::kNetworkTotalLimit;
    case HeavyAdStatus::kTotalCpu:
      return blink::mojom::HeavyAdReason::kCpuTotalLimit;
    case HeavyAdStatus::kPeakCpu:
      return blink::mojom::HeavyAdReason::kCpuPeakLimit;
    case HeavyAdStatus::kNone:
      NOTREACHED_IN_MIGRATION();
      return blink::mojom::HeavyAdReason::kNetworkTotalLimit;
  }
}

int64_t GetExponentialBucketForDistributionMoment(double sample) {
  constexpr static double kBucketSpacing = 1.3;

  base::ClampedNumeric<int64_t> rounded = base::ClampRound<int64_t>(sample);

  // If sample is negative, we need to first bucket it as a positive value.
  return (sample >= 0 ? 1 : -1) *
         ukm::GetExponentialBucketMin(rounded.Abs(), kBucketSpacing);
}

void RecordPageLoadInitiatorForAdTaggingUkm(
    content::NavigationHandle* navigation_handle) {
  auto* ukm_recorder = ukm::UkmRecorder::Get();

  ukm::builders::PageLoadInitiatorForAdTagging builder(
      navigation_handle->GetRenderFrameHost()->GetPageUkmSourceId());

  bool renderer_initiated = navigation_handle->IsRendererInitiated();
  bool renderer_initiated_with_user_activation =
      (navigation_handle->GetNavigationInitiatorActivationAndAdStatus() !=
       blink::mojom::NavigationInitiatorActivationAndAdStatus::
           kDidNotStartWithTransientActivation);
  bool renderer_initiated_with_user_activation_from_ad =
      (navigation_handle->GetNavigationInitiatorActivationAndAdStatus() ==
       blink::mojom::NavigationInitiatorActivationAndAdStatus::
           kStartedWithTransientActivationFromAd);

  builder.SetFromUser(!renderer_initiated ||
                      renderer_initiated_with_user_activation);
  builder.SetFromAdClick(renderer_initiated_with_user_activation_from_ad);

  builder.Record(ukm_recorder->Get());
}

}  // namespace

// static
std::unique_ptr<AdsPageLoadMetricsObserver>
AdsPageLoadMetricsObserver::CreateIfNeeded(
    content::WebContents* web_contents,
    heavy_ad_intervention::HeavyAdService* heavy_ad_service,
    const ApplicationLocaleGetter& application_locale_getter) {
  // TODO(bokan): ContentSubresourceFilterThrottleManager is now associated
  // with a FrameTree. When AdsPageLoadMetricsObserver becomes aware of MPArch
  // this should use the associated page rather than the primary page.
  if (!base::FeatureList::IsEnabled(subresource_filter::kAdTagging) ||
      !subresource_filter::ContentSubresourceFilterWebContentsHelper::
          FromWebContents(web_contents))
    return nullptr;
  return std::make_unique<AdsPageLoadMetricsObserver>(
      heavy_ad_service, application_locale_getter);
}

// static
bool AdsPageLoadMetricsObserver::IsFrameSameOriginToOutermostMainFrame(
    content::RenderFrameHost* host) {
  DCHECK(host);
  // In navigation for prerendering, `AdsPageLoadMetricsObserver` is removed
  // from PageLoadTracker.
  // TODO(crbug.com/40222513): Enable it if possible.
  DCHECK_NE(content::RenderFrameHost::LifecycleState::kPrerendering,
            host->GetLifecycleState());
  content::RenderFrameHost* outermost_main_host = host->GetOutermostMainFrame();
  url::Origin frame_origin = host->GetLastCommittedOrigin();
  url::Origin outermost_mainframe_origin =
      outermost_main_host->GetLastCommittedOrigin();
  return frame_origin.IsSameOriginWith(outermost_mainframe_origin);
}

AdsPageLoadMetricsObserver::FrameInstance::FrameInstance()
    : owned_frame_data_(nullptr), unowned_frame_data_(nullptr) {}

AdsPageLoadMetricsObserver::FrameInstance::FrameInstance(
    std::unique_ptr<FrameTreeData> frame_data)
    : owned_frame_data_(std::move(frame_data)), unowned_frame_data_(nullptr) {}

AdsPageLoadMetricsObserver::FrameInstance::FrameInstance(
    base::WeakPtr<FrameTreeData> frame_data)
    : owned_frame_data_(nullptr), unowned_frame_data_(frame_data) {}

AdsPageLoadMetricsObserver::FrameInstance::~FrameInstance() = default;

FrameTreeData* AdsPageLoadMetricsObserver::FrameInstance::Get() {
  if (owned_frame_data_)
    return owned_frame_data_.get();
  if (unowned_frame_data_)
    return unowned_frame_data_.get();

  DCHECK(!unowned_frame_data_.WasInvalidated());
  return nullptr;
}

FrameTreeData* AdsPageLoadMetricsObserver::FrameInstance::GetOwnedFrame() {
  if (owned_frame_data_)
    return owned_frame_data_.get();
  return nullptr;
}

AdsPageLoadMetricsObserver::HeavyAdThresholdNoiseProvider::
    HeavyAdThresholdNoiseProvider(bool use_noise)
    : use_noise_(use_noise) {}

int AdsPageLoadMetricsObserver::HeavyAdThresholdNoiseProvider::
    GetNetworkThresholdNoiseForFrame() const {
  return use_noise_ ? base::RandInt(0, kMaxNetworkThresholdNoiseBytes) : 0;
}

AdsPageLoadMetricsObserver::AdsPageLoadMetricsObserver(
    heavy_ad_intervention::HeavyAdService* heavy_ad_service,
    const ApplicationLocaleGetter& application_locale_getter,
    base::TickClock* clock,
    heavy_ad_intervention::HeavyAdBlocklist* blocklist)
    : clock_(clock ? clock : base::DefaultTickClock::GetInstance()),
      restricted_navigation_ad_tagging_enabled_(base::FeatureList::IsEnabled(
          features::kRestrictedNavigationAdTagging)),
      heavy_ad_service_(heavy_ad_service),
      application_locale_getter_(application_locale_getter),
      heavy_ad_blocklist_(blocklist),
      heavy_ad_privacy_mitigations_enabled_(base::FeatureList::IsEnabled(
          heavy_ad_intervention::features::kHeavyAdPrivacyMitigations)),
      heavy_ad_threshold_noise_provider_(
          std::make_unique<HeavyAdThresholdNoiseProvider>(
              heavy_ad_privacy_mitigations_enabled_ /* use_noise */)),
      page_ad_density_tracker_(clock) {
  // Manual setting of the heavy ad blocklist should be used only as a
  // convenience for tests that don't create HeavyAdService.
  DCHECK(!heavy_ad_service_ || !heavy_ad_blocklist_);
}

AdsPageLoadMetricsObserver::~AdsPageLoadMetricsObserver() = default;

const char* AdsPageLoadMetricsObserver::GetObserverName() const {
  static const char kName[] = "AdsPageLoadMetricsObserver";
  return kName;
}

PageLoadMetricsObserver::ObservePolicy AdsPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  navigation_id_ = navigation_handle->GetNavigationId();
  auto* observer_manager =
      subresource_filter::SubresourceFilterObserverManager::FromWebContents(
          navigation_handle->GetWebContents());
  // |observer_manager| isn't constructed if the feature for subresource
  // filtering isn't enabled.
  if (observer_manager)
    subresource_observation_.Observe(observer_manager);
  aggregate_frame_data_ = std::make_unique<AggregateFrameData>();
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
AdsPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // TODO(crbug.com/40222513): Handle Prerendering cases.
  return STOP_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
AdsPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Need the observer-level forwarding for FrameReceivedUserActivation,
  // FrameDisplayStateChanged, FrameSizeChanged, MediaStartedPlaying,
  // OnMainFrameIntersectionRectChanged, OnMainFrameViewportRectChanged,
  // and OnV8MemoryChanged.
  return FORWARD_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy AdsPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  DCHECK(ad_frames_data_.empty());

  RecordPageLoadInitiatorForAdTaggingUkm(navigation_handle);

  page_load_is_reload_ =
      navigation_handle->GetReloadType() != content::ReloadType::NONE;

  // The main frame is never considered an ad, so it should reference an empty
  // FrameInstance.
  ad_frames_data_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(navigation_handle->GetFrameTreeNodeId()),
      std::forward_as_tuple());

  ProcessOngoingNavigationResource(navigation_handle);

  // If the frame is blocked by the subresource filter, we don't want to record
  // any AdsPageLoad metrics.
  return subresource_filter_is_enabled_ ? STOP_OBSERVING : CONTINUE_OBSERVING;
}

void AdsPageLoadMetricsObserver::OnTimingUpdate(
    content::RenderFrameHost* frame_rfh,
    const mojom::PageLoadTiming& timing) {
  if (!frame_rfh)
    return;

  FrameTreeData* ancestor_data = FindFrameData(frame_rfh->GetFrameTreeNodeId());

  if (!ancestor_data)
    return;

  // Set paint eligiblity status.
  ancestor_data->SetFirstEligibleToPaint(
      timing.paint_timing->first_eligible_to_paint);

  // Update earliest FCP as needed.
  bool has_new_fcp = ancestor_data->SetEarliestFirstContentfulPaint(
      timing.paint_timing->first_contentful_paint);

  if (has_new_fcp) {
    // If this is the earliest FCP for any frame in the root ad frame's subtree,
    // set Creative Origin Status.
    OriginStatus origin_status =
        IsFrameSameOriginToOutermostMainFrame(frame_rfh) ? OriginStatus::kSame
                                                         : OriginStatus::kCross;
    ancestor_data->set_creative_origin_status(origin_status);

    // Determine the offset of this ad-frame FCP from main frame navigation
    // start, and remember it if it's the lowest. The time calculation is
    // (frame_fcp + frame_nav_start) - main_frame_nav_start.
    auto id_and_start =
        frame_navigation_starts_.find(frame_rfh->GetFrameTreeNodeId());
    if (id_and_start != frame_navigation_starts_.end()) {
      base::TimeDelta time_since_top_nav_start =
          (id_and_start->second +
           timing.paint_timing->first_contentful_paint.value()) -
          GetDelegate().GetNavigationStart();

      ancestor_data->SetEarliestFirstContentfulPaintSinceTopNavStart(
          time_since_top_nav_start);
      aggregate_frame_data_->UpdateFirstAdFCPSinceNavStart(
          time_since_top_nav_start);
    }
  }
}

void AdsPageLoadMetricsObserver::OnCpuTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const mojom::CpuTiming& timing) {
  // We should never trigger if the timing is null, no data should be sent.
  DCHECK(!timing.task_time.is_zero());

  // Get the current time, considered to be when this update occurred.
  base::TimeTicks current_time = clock_->NowTicks();

  FrameTreeData* ancestor_data =
      FindFrameData(subframe_rfh->GetFrameTreeNodeId());
  aggregate_frame_data_->UpdateCpuUsage(current_time, timing.task_time,
                                        ancestor_data);
  if (ancestor_data) {
    ancestor_data->UpdateCpuUsage(current_time, timing.task_time);
    MaybeTriggerHeavyAdIntervention(subframe_rfh, ancestor_data);
  }
}

// Given an ad being triggered for a frame or navigation, get its FrameTreeData
// and record it into the appropriate data structures.
void AdsPageLoadMetricsObserver::UpdateAdFrameData(
    content::NavigationHandle* navigation_handle,
    bool is_adframe,
    bool should_ignore_detected_ad) {
  const content::FrameTreeNodeId ad_id =
      navigation_handle->GetFrameTreeNodeId();
  // If an existing subframe is navigating and it was an ad previously that
  // hasn't navigated yet, then we need to update it.
  const auto& id_and_data = ad_frames_data_.find(ad_id);
  FrameTreeData* previous_data = id_and_data != ad_frames_data_.end()
                                     ? id_and_data->second.Get()
                                     : nullptr;

  if (previous_data) {
    // Frames that are no longer ad frames or are ignored as ad frames due to
    // restricted navigation ad tagging should have their tracked data reset.
    // TODO(crbug.com/40138413): Simplify the condition when restricted
    // navigation ad tagging is moved to subresource_filter/.
    if (!is_adframe || (should_ignore_detected_ad &&
                        (ad_id == previous_data->root_frame_tree_node_id()))) {
      DCHECK_EQ(ad_id, previous_data->root_frame_tree_node_id());
      CleanupDeletedFrame(ad_id, previous_data,
                          true /* update_density_tracker */,
                          false /* record_metrics */);

      ad_frames_data_.erase(id_and_data);

      // Replace the tracked frame with null frame reference. This
      // allows child frames to still be tracked as ads.
      ad_frames_data_.emplace(std::piecewise_construct,
                              std::forward_as_tuple(ad_id),
                              std::forward_as_tuple());

      return;
    }

    // As the frame has already navigated, we need to process the new navigation
    // resource in the frame.
    ProcessOngoingNavigationResource(navigation_handle);
    return;
  }

  // Determine who the parent frame's ad ancestor is.  If we don't know who it
  // is, return, such as with a frame from a previous navigation.
  content::RenderFrameHost* parent_frame_host =
      navigation_handle->GetParentFrameOrOuterDocument();
  const auto& parent_id_and_data =
      parent_frame_host
          ? ad_frames_data_.find(parent_frame_host->GetFrameTreeNodeId())
          : ad_frames_data_.end();
  bool parent_exists = parent_id_and_data != ad_frames_data_.end();
  if (!parent_exists)
    return;

  FrameTreeData* ad_data = parent_id_and_data->second.Get();

  bool should_create_new_frame_data =
      !ad_data && is_adframe && !should_ignore_detected_ad;

  // NOTE: Frame look-up only used for determining cross-origin
  // status for metrics, not granting security permissions.
  content::RenderFrameHost* ad_host =
      (navigation_handle->HasCommitted() ||
       navigation_handle->IsWaitingToCommit())
          ? navigation_handle->GetRenderFrameHost()
          : content::RenderFrameHost::FromID(
                navigation_handle->GetPreviousRenderFrameHostId());

  if (should_create_new_frame_data) {
    // Construct a new FrameTreeData to track this ad frame, and update it for
    // the navigation.
    auto frame_data = std::make_unique<FrameTreeData>(
        ad_id,
        heavy_ad_threshold_noise_provider_->GetNetworkThresholdNoiseForFrame());
    frame_data->UpdateForNavigation(ad_host);
    frame_data->MaybeUpdateFrameDepth(ad_host);

    FrameInstance frame_instance(std::move(frame_data));
    ad_frames_data_[ad_id] = std::move(frame_instance);
    return;
  }

  if (ad_data)
    ad_data->MaybeUpdateFrameDepth(ad_host);

  // Don't overwrite the frame id if it is associated with an ad.
  if (previous_data)
    return;

  // Frames who are the children of ad frames should be associated with the
  // ads FrameInstance. Otherwise, |ad_id| should be associated with an empty
  // FrameInstance to indicate it is not associated with an ad, but that the
  // frames navigation has been observed.
  FrameInstance frame_instance;
  if (ad_data)
    frame_instance = FrameInstance(ad_data->AsWeakPtr());

  ad_frames_data_[ad_id] = std::move(frame_instance);
}

void AdsPageLoadMetricsObserver::ReadyToCommitNextNavigation(
    content::NavigationHandle* navigation_handle) {
  // When the renderer receives a CommitNavigation message for the main frame,
  // all subframes detach and become display : none. Since this is not user
  // visible, and not reflective of the frames state during the page lifetime,
  // ignore any such messages when a navigation is about to commit.
  if (!navigation_handle->IsInMainFrame())
    return;
  // Prerendering navigation doesn't get here since this observer in
  // prerendering is removed from PageLoadTracker.
  // TODO(crbug.com/40222513): Consider enabling this observer for
  // prerendering.
  DCHECK(!navigation_handle->IsInPrerenderedMainFrame());
  process_display_state_updates_ = false;
}

// Determine if the frame is part of an existing ad, the root of a new ad, or a
// non-ad frame. Once a frame is labeled as an ad, it is always considered an
// ad, even if it navigates to a non-ad page. This function labels all of a
// page's frames, even those that fail to commit.
void AdsPageLoadMetricsObserver::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  // If the AdsPageLoadMetricsObserver is created, this does not return nullptr.
  auto* throttle_manager =
      subresource_filter::ContentSubresourceFilterThrottleManager::
          FromNavigationHandle(*navigation_handle);
  DCHECK(throttle_manager);

  frame_navigation_starts_[navigation_handle->GetFrameTreeNodeId()] =
      navigation_handle->NavigationStart();

  const bool is_adframe = throttle_manager->IsFrameTaggedAsAd(
      navigation_handle->GetFrameTreeNodeId());

  // TODO(crbug.com/40109934): The following block is a hack to ignore
  // certain frames that are detected by AdTagging. These frames are ignored
  // specifically for ad metrics and for the heavy ad intervention. The frames
  // ignored here are still considered ads by the heavy ad intervention. This
  // logic should be moved into /subresource_filter/ and applied to all of ad
  // tagging, rather than being implemented in AdsPLMO.
  bool should_ignore_detected_ad = false;
  std::optional<subresource_filter::LoadPolicy> load_policy =
      throttle_manager->LoadPolicyForLastCommittedNavigation(
          navigation_handle->GetFrameTreeNodeId());

  // Only un-tag frames as ads if the navigation has committed. This prevents
  // frames from being untagged that have an aborted navigation to allowlist
  // urls.
  if (restricted_navigation_ad_tagging_enabled_ && load_policy &&
      navigation_handle->GetNetErrorCode() == net::OK &&
      navigation_handle->HasCommitted()) {
    // If a filter list explicitly allows the rule, we should ignore a detected
    // ad.
    bool navigation_is_explicitly_allowed =
        *load_policy == subresource_filter::LoadPolicy::EXPLICITLY_ALLOW;

    const GURL& last_committed_url =
        navigation_handle->GetRenderFrameHost()->GetLastCommittedURL();
    const GURL& outermost_main_frame_last_committed_url =
        navigation_handle->GetRenderFrameHost()
            ->GetOutermostMainFrame()
            ->GetLastCommittedURL();
    // If a frame is detected to be an ad, but is same domain to the top frame,
    // and does not match a disallowed rule, ignore it.
    bool should_ignore_same_domain_ad =
        (*load_policy != subresource_filter::LoadPolicy::DISALLOW) &&
        (*load_policy != subresource_filter::LoadPolicy::WOULD_DISALLOW) &&
        net::registry_controlled_domains::SameDomainOrHost(
            last_committed_url, outermost_main_frame_last_committed_url,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    should_ignore_detected_ad =
        navigation_is_explicitly_allowed || should_ignore_same_domain_ad;
  }

  UpdateAdFrameData(navigation_handle, is_adframe, should_ignore_detected_ad);

  ProcessOngoingNavigationResource(navigation_handle);
}

void AdsPageLoadMetricsObserver::FrameReceivedUserActivation(
    content::RenderFrameHost* render_frame_host) {
  FrameTreeData* ancestor_data =
      FindFrameData(render_frame_host->GetFrameTreeNodeId());
  if (ancestor_data) {
    ancestor_data->set_received_user_activation();
  }
}

PageLoadMetricsObserver::ObservePolicy
AdsPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const mojom::PageLoadTiming& timing) {
  // The browser may come back, but there is no guarantee. To be safe, record
  // what we have now and keep tracking only for the purposes of interventions.
  if (GetDelegate().DidCommit() && !histograms_recorded_)
    RecordHistograms(GetDelegate().GetPageUkmSourceId());
  // Even if we didn't commit/record histograms, set histograms_recorded_ to
  // true, because this preserves the behavior of not reporting after the
  // browser app has been backgrounded.
  histograms_recorded_ = true;

  // TODO(ericrobinson): We could potentially make this contingent on whether
  // heavy_ads is enabled, but it's probably simpler to continue to monitor
  // silently in case future interventions require similar behavior.
  return CONTINUE_OBSERVING;
}

void AdsPageLoadMetricsObserver::OnComplete(
    const mojom::PageLoadTiming& timing) {
  // If Chrome was backgrounded previously, then we have already recorded the
  // histograms, otherwise we need to.
  if (!histograms_recorded_)
    RecordHistograms(GetDelegate().GetPageUkmSourceId());
  histograms_recorded_ = true;
}

void AdsPageLoadMetricsObserver::OnResourceDataUseObserved(
    content::RenderFrameHost* rfh,
    const std::vector<mojom::ResourceDataUpdatePtr>& resources) {
  for (auto const& resource : resources) {
    ProcessResourceForPage(rfh, resource);
    ProcessResourceForFrame(rfh, resource);
  }
}

void AdsPageLoadMetricsObserver::FrameDisplayStateChanged(
    content::RenderFrameHost* render_frame_host,
    bool is_display_none) {
  if (!process_display_state_updates_)
    return;
  FrameTreeData* ancestor_data =
      FindFrameData(render_frame_host->GetFrameTreeNodeId());
  // If the frame whose display state has changed is the root of the ad ancestry
  // chain, then update it. The display property is propagated to all child
  // frames.
  if (ancestor_data && render_frame_host->GetFrameTreeNodeId() ==
                           ancestor_data->root_frame_tree_node_id()) {
    ancestor_data->SetDisplayState(is_display_none);
  }
}

void AdsPageLoadMetricsObserver::FrameSizeChanged(
    content::RenderFrameHost* render_frame_host,
    const gfx::Size& frame_size) {
  FrameTreeData* ancestor_data =
      FindFrameData(render_frame_host->GetFrameTreeNodeId());
  // If the frame whose size has changed is the root of the ad ancestry chain,
  // then update it
  if (ancestor_data && render_frame_host->GetFrameTreeNodeId() ==
                           ancestor_data->root_frame_tree_node_id()) {
    ancestor_data->SetFrameSize(frame_size);
  }
}

void AdsPageLoadMetricsObserver::MediaStartedPlaying(
    const content::WebContentsObserver::MediaPlayerInfo& video_type,
    content::RenderFrameHost* render_frame_host) {
  FrameTreeData* ancestor_data =
      FindFrameData(render_frame_host->GetFrameTreeNodeId());
  if (ancestor_data)
    ancestor_data->set_media_status(MediaStatus::kPlayed);
}

void AdsPageLoadMetricsObserver::OnMainFrameIntersectionRectChanged(
    content::RenderFrameHost* render_frame_host,
    const gfx::Rect& main_frame_intersection_rect) {
  content::FrameTreeNodeId frame_tree_node_id =
      render_frame_host->GetFrameTreeNodeId();
  if (render_frame_host->IsInPrimaryMainFrame()) {
    page_ad_density_tracker_.UpdateMainFrameRect(main_frame_intersection_rect);
    return;
  }

  // If the frame whose size has changed is the root of the ad ancestry chain,
  // then update it.
  FrameTreeData* ancestor_data = FindFrameData(frame_tree_node_id);
  if (ancestor_data &&
      frame_tree_node_id == ancestor_data->root_frame_tree_node_id()) {
    RectId rect_id = RectId(RectType::kIFrame, frame_tree_node_id.value());

    // Only add frames if they are visible.
    if (!ancestor_data->is_display_none()) {
      page_ad_density_tracker_.RemoveRect(
          rect_id,
          /*recalculate_viewport_density=*/false);
      page_ad_density_tracker_.AddRect(rect_id, main_frame_intersection_rect,
                                       /*recalculate_density=*/true);
    } else {
      page_ad_density_tracker_.RemoveRect(
          rect_id,
          /*recalculate_viewport_density=*/true);
    }
  }

  CheckForAdDensityViolation();
}

void AdsPageLoadMetricsObserver::OnMainFrameViewportRectChanged(
    const gfx::Rect& main_frame_viewport_rect) {
  page_ad_density_tracker_.UpdateMainFrameViewportRect(
      main_frame_viewport_rect);
}

void AdsPageLoadMetricsObserver::OnMainFrameImageAdRectsChanged(
    const base::flat_map<int, gfx::Rect>& main_frame_image_ad_rects) {
  page_ad_density_tracker_.UpdateMainFrameImageAdRects(
      main_frame_image_ad_rects);
}

// TODO(crbug.com/40727873): Evaluate imposing width requirements
// for ad density violations.
void AdsPageLoadMetricsObserver::CheckForAdDensityViolation() {
#if BUILDFLAG(IS_ANDROID)
  const int kMaxMobileAdDensityByHeight = 30;
  if (page_ad_density_tracker_.MaxPageAdDensityByHeight() >
      kMaxMobileAdDensityByHeight) {
    // TODO(bokan): ContentSubresourceFilterThrottleManager is now associated
    // with a FrameTree. When AdsPageLoadMetricsObserver becomes aware of MPArch
    // this should use the associated page rather than the primary page.
    auto* throttle_manager =
        subresource_filter::ContentSubresourceFilterThrottleManager::FromPage(
            GetDelegate().GetWebContents()->GetPrimaryPage());
    // AdsPageLoadMetricsObserver is not created unless there is a
    // throttle manager.
    DCHECK(throttle_manager);

    // Violations can be triggered multiple times for the same page as
    // violations after the first are ignored. Ad frame violations are
    // attributed to the main frame url.
    throttle_manager->OnAdsViolationTriggered(
        GetDelegate().GetWebContents()->GetPrimaryMainFrame(),
        subresource_filter::mojom::AdsViolation::
            kMobileAdDensityByHeightAbove30);
  }
#endif
}

void AdsPageLoadMetricsObserver::OnSubFrameDeleted(
    content::FrameTreeNodeId frame_tree_node_id) {
  frame_navigation_starts_.erase(frame_tree_node_id);

  const auto& id_and_data = ad_frames_data_.find(frame_tree_node_id);
  if (id_and_data == ad_frames_data_.end())
    return;

  FrameTreeData* ancestor_data = nullptr;
  bool is_root_ad = false;

  if ((ancestor_data = id_and_data->second.GetOwnedFrame()))
    is_root_ad = true;
  else
    ancestor_data = id_and_data->second.Get();

  if (ancestor_data) {
    // If an ad frame has been deleted, update the aggregate memory usage by
    // removing the entry for this frame.
    // Moreover, if the root ad frame has been deleted, all child frames should
    // be deleted by this point, so flush histograms for the frame.
    CleanupDeletedFrame(id_and_data->first, ancestor_data,
                        is_root_ad /* update_density_tracker */,
                        is_root_ad /* record_metrics */);
  }

  // Delete the frame data.
  ad_frames_data_.erase(id_and_data);
}

void AdsPageLoadMetricsObserver::OnV8MemoryChanged(
    const std::vector<MemoryUpdate>& memory_updates) {
  for (const auto& update : memory_updates) {
    memory_update_count_++;

    content::RenderFrameHost* render_frame_host =
        content::RenderFrameHost::FromID(update.routing_id);

    if (!render_frame_host)
      continue;

    content::FrameTreeNodeId frame_node_id =
        render_frame_host->GetFrameTreeNodeId();
    FrameTreeData* ad_frame_data = FindFrameData(frame_node_id);

    if (ad_frame_data) {
      ad_frame_data->UpdateMemoryUsage(update.delta_bytes);
      UpdateAggregateMemoryUsage(update.delta_bytes,
                                 ad_frame_data->visibility());
    } else if (!render_frame_host->GetParentOrOuterDocument()) {
      // |render_frame_host| is the outermost main frame.
      aggregate_frame_data_->update_outermost_main_frame_memory(
          update.delta_bytes);
    }
  }
}

void AdsPageLoadMetricsObserver::OnAdAuctionComplete(
    bool is_server_auction,
    bool is_on_device_auction,
    content::AuctionResult result) {
  aggregate_frame_data_->OnAdAuctionComplete(is_server_auction,
                                             is_on_device_auction, result);
}

void AdsPageLoadMetricsObserver::OnSubresourceFilterGoingAway() {
  subresource_observation_.Reset();
}

void AdsPageLoadMetricsObserver::OnPageActivationComputed(
    content::NavigationHandle* navigation_handle,
    const subresource_filter::mojom::ActivationState& activation_state) {
  DCHECK(navigation_handle);
  DCHECK_GE(navigation_id_, 0);

  // The subresource filter's activation level and navigation id is the same for
  // all frames on a page, so we only record this for the main frame.
  if (navigation_handle->IsInMainFrame() &&
      navigation_handle->GetNavigationId() == navigation_id_ &&
      activation_state.activation_level ==
          subresource_filter::mojom::ActivationLevel::kEnabled) {
    // Prerendering navigation is filtered out by checking `navigation_id_`.
    // TODO(crbug.com/40222513): Consider enabling this observer for
    // prerendering.
    DCHECK(!navigation_handle->IsInPrerenderedMainFrame());
    DCHECK(!subresource_filter_is_enabled_);
    subresource_filter_is_enabled_ = true;
  }
}

int AdsPageLoadMetricsObserver::GetUnaccountedAdBytes(
    int process_id,
    const mojom::ResourceDataUpdatePtr& resource) const {
  if (!resource->reported_as_ad_resource)
    return 0;
  content::GlobalRequestID global_request_id(process_id, resource->request_id);

  // Resource just started loading.
  if (!GetDelegate().GetResourceTracker().HasPreviousUpdateForResource(
          global_request_id))
    return 0;

  // If the resource had already started loading, and is now labeled as an ad,
  // but was not before, we need to account for all the previously received
  // bytes.
  auto const& previous_update =
      GetDelegate().GetResourceTracker().GetPreviousUpdateForResource(
          global_request_id);
  bool is_new_ad = !previous_update->reported_as_ad_resource;
  return is_new_ad ? resource->received_data_length - resource->delta_bytes : 0;
}

void AdsPageLoadMetricsObserver::ProcessResourceForPage(
    content::RenderFrameHost* render_frame_host,
    const mojom::ResourceDataUpdatePtr& resource) {
  int process_id = render_frame_host->GetProcess()->GetID();
  auto mime_type = ResourceLoadAggregator::GetResourceMimeType(resource);
  int unaccounted_ad_bytes = GetUnaccountedAdBytes(process_id, resource);
  bool is_outermost_main_frame = !render_frame_host->GetParentOrOuterDocument();
  aggregate_frame_data_->ProcessResourceLoadInFrame(resource,
                                                    is_outermost_main_frame);
  if (unaccounted_ad_bytes)
    aggregate_frame_data_->AdjustAdBytes(unaccounted_ad_bytes, mime_type,
                                         is_outermost_main_frame);
}

void AdsPageLoadMetricsObserver::ProcessResourceForFrame(
    content::RenderFrameHost* render_frame_host,
    const mojom::ResourceDataUpdatePtr& resource) {
  const auto& id_and_data =
      ad_frames_data_.find(render_frame_host->GetFrameTreeNodeId());
  if (id_and_data == ad_frames_data_.end()) {
    if (resource->is_primary_frame_resource) {
      // Only hold onto primary resources if their load has finished, otherwise
      // we will receive a future update for them if the navigation finishes.
      if (!resource->is_complete)
        return;

      // This resource request is the primary resource load for a frame that
      // hasn't yet finished navigating. Hang onto the request info and replay
      // it once the frame finishes navigating.
      ongoing_navigation_resources_.emplace(
          std::piecewise_construct,
          std::forward_as_tuple(render_frame_host->GetFrameTreeNodeId()),
          std::forward_as_tuple(resource.Clone()));
    } else {
      // This is unexpected, it could be:
      // 1. a resource from a previous navigation that started its resource
      //    load after this page started navigation.
      // 2. possibly a resource from a document.written frame whose frame
      //    failure message has yet to arrive. (uncertain of this)
    }
    return;
  }

  // Determine if the frame (or its ancestor) is an ad, if so attribute the
  // bytes to the highest ad ancestor.
  FrameTreeData* ancestor_data = id_and_data->second.Get();
  if (!ancestor_data)
    return;

  auto mime_type = ResourceLoadAggregator::GetResourceMimeType(resource);
  int unaccounted_ad_bytes =
      GetUnaccountedAdBytes(render_frame_host->GetProcess()->GetID(), resource);
  if (unaccounted_ad_bytes)
    ancestor_data->AdjustAdBytes(unaccounted_ad_bytes, mime_type);
  ancestor_data->ProcessResourceLoadInFrame(
      resource, render_frame_host->GetProcess()->GetID(),
      GetDelegate().GetResourceTracker());
  MaybeTriggerHeavyAdIntervention(render_frame_host, ancestor_data);
}

void AdsPageLoadMetricsObserver::RecordPageResourceTotalHistograms(
    ukm::SourceId source_id) {
  const auto& resource_data = aggregate_frame_data_->resource_data();

  auto* ukm_recorder = ukm::UkmRecorder::Get();

  // AdPageLoadCustomSampling3 is recorded on all pages
  ukm::builders::AdPageLoadCustomSampling3 custom_sampling_builder(source_id);

  page_ad_density_tracker_.Finalize();

  UnivariateStats::DistributionMoments moments =
      page_ad_density_tracker_.GetAdDensityByAreaStats();

  custom_sampling_builder.SetAverageViewportAdDensity(
      std::llround(moments.mean));
  custom_sampling_builder.SetVarianceViewportAdDensity(
      GetExponentialBucketForDistributionMoment(moments.variance));
  custom_sampling_builder.SetSkewnessViewportAdDensity(
      GetExponentialBucketForDistributionMoment(moments.skewness));
  custom_sampling_builder.SetKurtosisViewportAdDensity(
      GetExponentialBucketForDistributionMoment(moments.excess_kurtosis));
  custom_sampling_builder.Record(ukm_recorder->Get());

  ADS_HISTOGRAM("AverageViewportAdDensity", base::UmaHistogramPercentage,
                FrameVisibility::kAnyVisibility, std::llround(moments.mean));

  // Only records histograms on pages that have some ad bytes.
  if (resource_data.ad_bytes() == 0)
    return;

  PAGE_BYTES_HISTOGRAM("PageLoad.Clients.Ads.Resources.Bytes.Ads2",
                       resource_data.ad_network_bytes());

  ukm::builders::AdPageLoad builder(source_id);
  builder.SetTotalBytes(resource_data.network_bytes() >> 10)
      .SetAdBytes(resource_data.ad_network_bytes() >> 10)
      .SetAdJavascriptBytes(resource_data.GetAdNetworkBytesForMime(
                                ResourceMimeType::kJavascript) >>
                            10)
      .SetAdVideoBytes(
          resource_data.GetAdNetworkBytesForMime(ResourceMimeType::kVideo) >>
          10)
      .SetMainframeAdBytes(ukm::GetExponentialBucketMinForBytes(
          aggregate_frame_data_->outermost_main_frame_resource_data()
              .ad_network_bytes()))
      .SetMaxAdDensityByArea(page_ad_density_tracker_.MaxPageAdDensityByArea())
      .SetMaxAdDensityByHeight(
          page_ad_density_tracker_.MaxPageAdDensityByHeight());

  // Record cpu metrics for the page.
  builder.SetAdCpuTime(
      aggregate_frame_data_->total_ad_cpu_usage().InMilliseconds());
  builder.Record(ukm_recorder->Get());
}

void AdsPageLoadMetricsObserver::RecordHistograms(ukm::SourceId source_id) {
  // Record per-frame metrics for any existing frames.
  for (auto& id_and_instance : ad_frames_data_) {
    // We only log metrics for FrameInstance which own a FrameTreeData,
    // otherwise we would be double counting frames.
    if (FrameTreeData* frame_data = id_and_instance.second.GetOwnedFrame()) {
      RecordPerFrameMetrics(*frame_data, source_id);
    }
  }

  std::optional<base::TimeDelta> first_ad_fcp_after_main_nav_start =
      aggregate_frame_data_->first_ad_fcp_after_main_nav_start();
  if (first_ad_fcp_after_main_nav_start) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.Ads.AdPaintTiming."
        "TopFrameNavigationToFirstAdFirstContentfulPaint",
        first_ad_fcp_after_main_nav_start.value());

    std::string fcp_after_auction_metric_name;
    if (aggregate_frame_data_->completed_fledge_server_auction_before_fcp()) {
      if (aggregate_frame_data_
              ->completed_fledge_on_device_auction_before_fcp()) {
        fcp_after_auction_metric_name =
            "PageLoad.Clients.Ads.AdPaintTiming."
            "TopFrameNavigationToFirstAdFirstContentfulPaintAfter"
            "ServerAndDeviceAuctions";
      } else {
        fcp_after_auction_metric_name =
            "PageLoad.Clients.Ads.AdPaintTiming."
            "TopFrameNavigationToFirstAdFirstContentfulPaintAfterServerAuction";
        if (aggregate_frame_data_->completed_only_winning_fledge_auctions()) {
          PAGE_LOAD_HISTOGRAM(
              "PageLoad.Clients.Ads.AdPaintTiming."
              "TopFrameNavigationToFirstAdFirstContentfulPaintAfterWinning"
              "ServerAuction",
              first_ad_fcp_after_main_nav_start.value());
        }
      }
    } else if (aggregate_frame_data_
                   ->completed_fledge_on_device_auction_before_fcp()) {
      fcp_after_auction_metric_name =
          "PageLoad.Clients.Ads.AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterDeviceAuction";
      if (aggregate_frame_data_->completed_only_winning_fledge_auctions()) {
        PAGE_LOAD_HISTOGRAM(
            "PageLoad.Clients.Ads.AdPaintTiming."
            "TopFrameNavigationToFirstAdFirstContentfulPaintAfter"
            "WinningDeviceAuction",
            first_ad_fcp_after_main_nav_start.value());
      }
    } else {
      fcp_after_auction_metric_name =
          "PageLoad.Clients.Ads.AdPaintTiming."
          "TopFrameNavigationToFirstAdFirstContentfulPaintAfterNoAuction";
    }
    PAGE_LOAD_HISTOGRAM(fcp_after_auction_metric_name,
                        first_ad_fcp_after_main_nav_start.value());
  }

  RecordAggregateHistogramsForAdTagging(FrameVisibility::kNonVisible);
  RecordAggregateHistogramsForAdTagging(FrameVisibility::kVisible);
  RecordAggregateHistogramsForAdTagging(FrameVisibility::kAnyVisibility);
  RecordAggregateHistogramsForCpuUsage();
  RecordPageResourceTotalHistograms(source_id);
}

void AdsPageLoadMetricsObserver::RecordAggregateHistogramsForCpuUsage() {
  // If the page has an ad with the relevant visibility and non-zero bytes.
  if (aggregate_frame_data_
          ->get_ad_data_by_visibility(FrameVisibility::kAnyVisibility)
          .frames == 0) {
    return;
  }

  // Only record cpu usage aggregate data for the AnyVisibility suffix as these
  // numbers do not change for different visibility types.
  FrameVisibility visibility = FrameVisibility::kAnyVisibility;

  // Record the aggregate data, which is never considered activated.
  // TODO(crbug.com/40141881): Does it make sense to include an aggregate peak
  // windowed percent?  Obviously this would be a max of maxes, but might be
  // useful to have that for comparisons as well.
  ADS_HISTOGRAM("Cpu.AdFrames.Aggregate.TotalUsage2", PAGE_LOAD_HISTOGRAM,
                visibility, aggregate_frame_data_->total_ad_cpu_usage());
  ADS_HISTOGRAM("Cpu.NonAdFrames.Aggregate.TotalUsage2", PAGE_LOAD_HISTOGRAM,
                visibility,
                aggregate_frame_data_->total_cpu_usage() -
                    aggregate_frame_data_->total_ad_cpu_usage());
  ADS_HISTOGRAM("Cpu.NonAdFrames.Aggregate.PeakWindowedPercent2",
                base::UmaHistogramPercentage, visibility,
                aggregate_frame_data_->peak_windowed_non_ad_cpu_percent());
  ADS_HISTOGRAM("Cpu.FullPage.TotalUsage2", PAGE_LOAD_HISTOGRAM, visibility,
                aggregate_frame_data_->total_cpu_usage());
  ADS_HISTOGRAM("Cpu.FullPage.PeakWindowedPercent2",
                base::UmaHistogramPercentage, visibility,
                aggregate_frame_data_->peak_windowed_cpu_percent());
}

void AdsPageLoadMetricsObserver::RecordAggregateHistogramsForAdTagging(
    FrameVisibility visibility) {
  const auto& resource_data = aggregate_frame_data_->resource_data();

  if (resource_data.bytes() == 0)
    return;

  const auto& visibility_data =
      aggregate_frame_data_->get_ad_data_by_visibility(visibility);

  ADS_HISTOGRAM("FrameCounts.AdFrames.Total", base::UmaHistogramCounts1000,
                visibility, visibility_data.frames);

  // Only record AllPages histograms for the AnyVisibility suffix as these
  // numbers do not change for different visibility types.
  if (visibility == FrameVisibility::kAnyVisibility) {
    ADS_HISTOGRAM("AllPages.PercentTotalBytesAds", base::UmaHistogramPercentage,
                  visibility,
                  resource_data.ad_bytes() * 100 / resource_data.bytes());
    if (resource_data.network_bytes()) {
      ADS_HISTOGRAM("AllPages.PercentNetworkBytesAds",
                    base::UmaHistogramPercentage, visibility,
                    resource_data.ad_network_bytes() * 100 /
                        resource_data.network_bytes());
    }
    ADS_HISTOGRAM(
        "AllPages.NonAdNetworkBytes", PAGE_BYTES_HISTOGRAM, visibility,
        resource_data.network_bytes() - resource_data.ad_network_bytes());
  }

  // Only post AllPages and FrameCounts UMAs for pages that don't have ads.
  if (visibility_data.frames == 0)
    return;

  ADS_HISTOGRAM("Bytes.NonAdFrames.Aggregate.Total2", PAGE_BYTES_HISTOGRAM,
                visibility, resource_data.bytes() - visibility_data.bytes);

  ADS_HISTOGRAM("Bytes.FullPage.Total2", PAGE_BYTES_HISTOGRAM, visibility,
                resource_data.bytes());
  ADS_HISTOGRAM("Bytes.FullPage.Network", PAGE_BYTES_HISTOGRAM, visibility,
                resource_data.network_bytes());

  if (resource_data.bytes()) {
    ADS_HISTOGRAM("Bytes.FullPage.Total2.PercentAdFrames",
                  base::UmaHistogramPercentage, visibility,
                  visibility_data.bytes * 100 / resource_data.bytes());
  }
  if (resource_data.network_bytes()) {
    ADS_HISTOGRAM(
        "Bytes.FullPage.Network.PercentAdFrames", base::UmaHistogramPercentage,
        visibility,
        visibility_data.network_bytes * 100 / resource_data.network_bytes());
  }

  ADS_HISTOGRAM("Bytes.AdFrames.Aggregate.Total2", PAGE_BYTES_HISTOGRAM,
                visibility, visibility_data.bytes);
  ADS_HISTOGRAM("Bytes.AdFrames.Aggregate.Network", PAGE_BYTES_HISTOGRAM,
                visibility, visibility_data.network_bytes);

  // Only record same origin and main frame totals for the AnyVisibility suffix
  // as these numbers do not change for different visibility types.
  if (visibility != FrameVisibility::kAnyVisibility)
    return;

  const auto& outermost_main_frame_resource_data =
      aggregate_frame_data_->outermost_main_frame_resource_data();
  ADS_HISTOGRAM("Bytes.MainFrame.Network", PAGE_BYTES_HISTOGRAM, visibility,
                outermost_main_frame_resource_data.network_bytes());
  ADS_HISTOGRAM("Bytes.MainFrame.Total2", PAGE_BYTES_HISTOGRAM, visibility,
                outermost_main_frame_resource_data.bytes());
  ADS_HISTOGRAM("Bytes.MainFrame.Ads.Network", PAGE_BYTES_HISTOGRAM, visibility,
                outermost_main_frame_resource_data.ad_network_bytes());
  ADS_HISTOGRAM("Bytes.MainFrame.Ads.Total2", PAGE_BYTES_HISTOGRAM, visibility,
                outermost_main_frame_resource_data.ad_bytes());
  if (base::FeatureList::IsEnabled(::features::kV8PerFrameMemoryMonitoring)) {
    PAGE_BYTES_HISTOGRAM(
        "PageLoad.Clients.Ads.Memory.MainFrame.Max",
        aggregate_frame_data_->outermost_main_frame_max_memory());
    base::UmaHistogramCounts10000("PageLoad.Clients.Ads.Memory.UpdateCount",
                                  memory_update_count_);
  }
}

void AdsPageLoadMetricsObserver::RecordPerFrameMetrics(
    const FrameTreeData& ad_frame_data,
    ukm::SourceId source_id) {
  // If we've previously recorded histograms, then don't do anything.
  if (histograms_recorded_)
    return;
  RecordPerFrameHistogramsForCpuUsage(ad_frame_data);
  RecordPerFrameHistogramsForAdTagging(ad_frame_data);
  RecordPerFrameHistogramsForHeavyAds(ad_frame_data);
  ad_frame_data.RecordAdFrameLoadUkmEvent(source_id);
}

void AdsPageLoadMetricsObserver::RecordPerFrameHistogramsForCpuUsage(
    const FrameTreeData& ad_frame_data) {
  // This aggregate gets reported regardless of whether the frame used bytes.
  aggregate_frame_data_->update_ad_cpu_usage(ad_frame_data.GetTotalCpuUsage());

  if (!ad_frame_data.ShouldRecordFrameForMetrics())
    return;

  // Record per-frame histograms to the appropriate visibility prefixes.
  for (const auto visibility :
       {FrameVisibility::kAnyVisibility, ad_frame_data.visibility()}) {
    // Report the peak windowed usage, which is independent of activation status
    // (measured only for the unactivated period).
    ADS_HISTOGRAM("Cpu.AdFrames.PerFrame.PeakWindowedPercent2",
                  base::UmaHistogramPercentage, visibility,
                  ad_frame_data.peak_windowed_cpu_percent());

    if (ad_frame_data.user_activation_status() ==
        UserActivationStatus::kNoActivation) {
      ADS_HISTOGRAM("Cpu.AdFrames.PerFrame.TotalUsage2.Unactivated",
                    PAGE_LOAD_HISTOGRAM, visibility,
                    ad_frame_data.GetTotalCpuUsage());
    } else {
      base::TimeDelta task_duration_pre = ad_frame_data.GetActivationCpuUsage(
          UserActivationStatus::kNoActivation);
      base::TimeDelta task_duration_post = ad_frame_data.GetActivationCpuUsage(
          UserActivationStatus::kReceivedActivation);
      base::TimeDelta task_duration_total =
          task_duration_pre + task_duration_post;
      ADS_HISTOGRAM("Cpu.AdFrames.PerFrame.TotalUsage2.Activated",
                    PAGE_LOAD_HISTOGRAM, visibility, task_duration_total);
      ADS_HISTOGRAM("Cpu.AdFrames.PerFrame.TotalUsage2.Activated.PreActivation",
                    PAGE_LOAD_HISTOGRAM, visibility, task_duration_pre);
      ADS_HISTOGRAM(
          "Cpu.AdFrames.PerFrame.TotalUsage2.Activated.PostActivation",
          PAGE_LOAD_HISTOGRAM, visibility, task_duration_post);
    }
  }
}

void AdsPageLoadMetricsObserver::RecordPerFrameHistogramsForAdTagging(
    const FrameTreeData& ad_frame_data) {
  if (!ad_frame_data.ShouldRecordFrameForMetrics())
    return;

  // Record per-frame histograms to the appropriate visibility prefixes.
  for (const auto visibility :
       {FrameVisibility::kAnyVisibility, ad_frame_data.visibility()}) {
    const auto& resource_data = ad_frame_data.resource_data();

    // Update aggregate ad information.
    aggregate_frame_data_->update_ad_bytes_by_visibility(visibility,
                                                         resource_data.bytes());
    aggregate_frame_data_->update_ad_network_bytes_by_visibility(
        visibility, resource_data.network_bytes());
    aggregate_frame_data_->update_ad_frames_by_visibility(visibility, 1);

    ADS_HISTOGRAM("Bytes.AdFrames.PerFrame.Total2", PAGE_BYTES_HISTOGRAM,
                  visibility, resource_data.bytes());
    ADS_HISTOGRAM("Bytes.AdFrames.PerFrame.Network", PAGE_BYTES_HISTOGRAM,
                  visibility, resource_data.network_bytes());
    ADS_HISTOGRAM("FrameCounts.AdFrames.PerFrame.OriginStatus",
                  base::UmaHistogramEnumeration, visibility,
                  ad_frame_data.origin_status());

    ADS_HISTOGRAM("FrameCounts.AdFrames.PerFrame.CreativeOriginStatus",
                  base::UmaHistogramEnumeration, visibility,
                  ad_frame_data.creative_origin_status());

    ADS_HISTOGRAM(
        "FrameCounts.AdFrames.PerFrame.CreativeOriginStatusWithThrottling",
        base::UmaHistogramEnumeration, visibility,
        ad_frame_data.GetCreativeOriginStatusWithThrottling());

    ADS_HISTOGRAM("FrameCounts.AdFrames.PerFrame.UserActivation",
                  base::UmaHistogramEnumeration, visibility,
                  ad_frame_data.user_activation_status());

    if (auto first_contentful_paint =
            ad_frame_data.earliest_first_contentful_paint()) {
      ADS_HISTOGRAM("AdPaintTiming.NavigationToFirstContentfulPaint3",
                    PAGE_LOAD_LONG_HISTOGRAM, visibility,
                    first_contentful_paint.value());
    }

    if (auto earliest_fcp_since_top_nav_start =
            ad_frame_data.earliest_fcp_since_top_nav_start()) {
      ADS_HISTOGRAM("AdPaintTiming.TopFrameNavigationToFirstContentfulPaint",
                    PAGE_LOAD_LONG_HISTOGRAM, visibility,
                    earliest_fcp_since_top_nav_start.value());
    }
  }
}

void AdsPageLoadMetricsObserver::RecordPerFrameHistogramsForHeavyAds(
    const FrameTreeData& ad_frame_data) {
  if (!ad_frame_data.ShouldRecordFrameForMetrics())
    return;

  // Record per-frame histograms to the appropriate visibility prefixes.
  for (const auto visibility :
       {FrameVisibility::kAnyVisibility, ad_frame_data.visibility()}) {
    ADS_HISTOGRAM("HeavyAds.ComputedTypeWithThresholdNoise",
                  base::UmaHistogramEnumeration, visibility,
                  ad_frame_data.heavy_ad_status_with_noise());
  }
}

void AdsPageLoadMetricsObserver::ProcessOngoingNavigationResource(
    content::NavigationHandle* navigation_handle) {
  const auto& frame_id_and_request = ongoing_navigation_resources_.find(
      navigation_handle->GetFrameTreeNodeId());
  if (frame_id_and_request == ongoing_navigation_resources_.end())
    return;

  // NOTE: Frame look-up is not for granting security permissions.
  content::RenderFrameHost* rfh =
      (navigation_handle->HasCommitted() ||
       navigation_handle->IsWaitingToCommit())
          ? navigation_handle->GetRenderFrameHost()
          : content::RenderFrameHost::FromID(
                navigation_handle->GetPreviousRenderFrameHostId());

  ProcessResourceForFrame(rfh, frame_id_and_request->second);
  ongoing_navigation_resources_.erase(frame_id_and_request);
}

FrameTreeData* AdsPageLoadMetricsObserver::FindFrameData(
    content::FrameTreeNodeId id) {
  const auto& id_and_data = ad_frames_data_.find(id);
  if (id_and_data == ad_frames_data_.end())
    return nullptr;

  return id_and_data->second.Get();
}

void AdsPageLoadMetricsObserver::MaybeTriggerStrictHeavyAdIntervention() {
  DCHECK(heavy_ads_blocklist_reason_.has_value());
  if (heavy_ads_blocklist_reason_ !=
      blocklist::BlocklistReason::kUserOptedOutOfHost)
    return;

  // TODO(bokan): ContentSubresourceFilterThrottleManager is now associated
  // with a FrameTree. When AdsPageLoadMetricsObserver becomes aware of MPArch
  // this should use the associated page rather than the primary page.
  auto* throttle_manager =
      subresource_filter::ContentSubresourceFilterThrottleManager::FromPage(
          GetDelegate().GetWebContents()->GetPrimaryPage());
  // AdsPageLoadMetricsObserver is not created unless there is a
  // throttle manager.
  DCHECK(throttle_manager);

  // Violations can be triggered multiple times for the same page as
  // violations after the first are ignored. Ad frame violations are
  // attributed to the main frame url.
  throttle_manager->OnAdsViolationTriggered(
      GetDelegate().GetWebContents()->GetPrimaryMainFrame(),
      subresource_filter::mojom::AdsViolation::
          kHeavyAdsInterventionAtHostLimit);
}

void AdsPageLoadMetricsObserver::MaybeTriggerHeavyAdIntervention(
    content::RenderFrameHost* render_frame_host,
    FrameTreeData* frame_data) {
  DCHECK(render_frame_host);
  HeavyAdAction action = frame_data->MaybeTriggerHeavyAdIntervention();
  if (action == HeavyAdAction::kNone)
    return;

  // Don't trigger the heavy ad intervention on reloads. Gate this behind the
  // privacy mitigations flag to help developers debug (otherwise they need to
  // trigger new navigations to the site to test it).
  if (heavy_ad_privacy_mitigations_enabled_) {
    // Skip firing the intervention, but mark that an action occurred on the
    // frame.
    if (page_load_is_reload_) {
      frame_data->set_heavy_ad_action(HeavyAdAction::kIgnored);
      return;
    }
  }

  // Check to see if we are allowed to activate on this host.
  if (IsBlocklisted(true)) {
    frame_data->set_heavy_ad_action(HeavyAdAction::kIgnored);
    return;
  }

  // We should always unload the root of the ad subtree. Find the
  // RenderFrameHost of the root ad frame associated with |frame_data|.
  // |render_frame_host| may be the frame host for a subframe of the ad which we
  // received a resource update for. Traversing the tree here guarantees
  // that the frame we unload is an ancestor of |render_frame_host|. We cannot
  // check if RenderFrameHosts are ads so we rely on matching the
  // root_frame_tree_node_id of |frame_data|. It is possible that this frame no
  // longer exists. We do not care if the frame has moved to a new process
  // because once the frame has been tagged as an ad, it is always considered an
  // ad by our heuristics.
  while (render_frame_host && render_frame_host->GetFrameTreeNodeId() !=
                                  frame_data->root_frame_tree_node_id()) {
    render_frame_host = render_frame_host->GetParentOrOuterDocument();
  }
  if (!render_frame_host) {
    frame_data->set_heavy_ad_action(HeavyAdAction::kIgnored);
    return;
  }

  // Ensure that this RenderFrameHost is a subframe.
  DCHECK(render_frame_host->GetParentOrOuterDocument());

  frame_data->set_heavy_ad_action(action);

  // Add an inspector issue for the root of the ad subtree.
  auto issue = blink::mojom::InspectorIssueInfo::New();
  issue->code = blink::mojom::InspectorIssueCode::kHeavyAdIssue;
  issue->details = blink::mojom::InspectorIssueDetails::New();
  auto heavy_ad_details = blink::mojom::HeavyAdIssueDetails::New();
  heavy_ad_details->resolution =
      action == HeavyAdAction::kUnload
          ? blink::mojom::HeavyAdResolutionStatus::kHeavyAdBlocked
          : blink::mojom::HeavyAdResolutionStatus::kHeavyAdWarning;
  heavy_ad_details->reason =
      GetHeavyAdReason(frame_data->heavy_ad_status_with_policy());
  heavy_ad_details->frame = blink::mojom::AffectedFrame::New();
  heavy_ad_details->frame->frame_id =
      render_frame_host->GetDevToolsFrameToken().ToString();
  issue->details->heavy_ad_issue_details = std::move(heavy_ad_details);
  render_frame_host->ReportInspectorIssue(std::move(issue));

  // Report to all child frames that will be unloaded. Once all reports are
  // queued, the frame will be unloaded. Because the IPC messages are ordered
  // wrt to each frames unload, we do not need to wait before loading the
  // error page. Reports will be added to ReportingObserver queues
  // synchronously when the IPC message is handled, which guarantees they will
  // be available in the the unload handler.
  std::string report_message =
      GetHeavyAdReportMessage(*frame_data, action == HeavyAdAction::kUnload);
  render_frame_host->ForEachRenderFrameHostWithAction(
      [&report_message,
       &page = render_frame_host->GetPage()](content::RenderFrameHost* frame) {
        // If `frame`'s page doesn't match the one we are associated with (for
        // fenced frames or portals) skip the subtree.
        if (&page != &frame->GetPage())
          return content::RenderFrameHost::FrameIterationAction::kSkipChildren;
        static constexpr char kReportId[] = "HeavyAdIntervention";
        if (frame->IsRenderFrameLive())
          frame->SendInterventionReport(kReportId, report_message);
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });

  // Report intervention to the blocklist.
  if (auto* blocklist = GetHeavyAdBlocklist()) {
    blocklist->AddEntry(
        GetDelegate().GetWebContents()->GetLastCommittedURL().host(),
        true /* opt_out */,
        static_cast<int>(
            heavy_ad_intervention::HeavyAdBlocklistType::kHeavyAdOnlyType));
    // Once we report, we need to check and see if we are now blocklisted.
    // If we are, then we might trigger stricter interventions.
    // TODO(ericrobinson): This does a couple fetches of the blocklist.  It
    // might be simpler to fetch it once at the start of this function and use
    // it throughout.
    if (IsBlocklisted(false)) {
      MaybeTriggerStrictHeavyAdIntervention();
    }
  }

  // Record this UMA regardless of if we actually unload or not, as sending
  // reports is subject to the same noise and throttling as the intervention.
  MetricsWebContentsObserver::RecordFeatureUsage(
      render_frame_host, blink::mojom::WebFeature::kHeavyAdIntervention);

  ADS_HISTOGRAM("HeavyAds.InterventionType2", base::UmaHistogramEnumeration,
                FrameVisibility::kAnyVisibility,
                frame_data->heavy_ad_status_with_policy());
  ADS_HISTOGRAM("HeavyAds.InterventionType2", base::UmaHistogramEnumeration,
                frame_data->visibility(),
                frame_data->heavy_ad_status_with_policy());

  if (action != HeavyAdAction::kUnload)
    return;

  // Record heavy ad network size only when an ad is unloaded as a result of
  // network usage.
  if (frame_data->heavy_ad_status_with_noise() == HeavyAdStatus::kNetwork) {
    ADS_HISTOGRAM("HeavyAds.NetworkBytesAtFrameUnload", PAGE_BYTES_HISTOGRAM,
                  kAnyVisibility, frame_data->resource_data().network_bytes());
  }

  GetDelegate().GetWebContents()->GetController().LoadPostCommitErrorPage(
      render_frame_host, render_frame_host->GetLastCommittedURL(),
      heavy_ad_intervention::PrepareHeavyAdPage(
          application_locale_getter_.Run()));
}

bool AdsPageLoadMetricsObserver::IsBlocklisted(bool report) {
  if (!heavy_ad_privacy_mitigations_enabled_)
    return false;

  auto* blocklist = GetHeavyAdBlocklist();

  // Treat instances where the blocklist is unavailable as blocklisted.
  if (!blocklist) {
    heavy_ads_blocklist_reason_ =
        blocklist::BlocklistReason::kBlocklistNotLoaded;
    return true;
  }

  // If we haven't computed a blocklist reason previously or it was allowed
  // previously, we need to compute/re-compute the value and store it.
  if (!heavy_ads_blocklist_reason_.has_value() ||
      heavy_ads_blocklist_reason_ == blocklist::BlocklistReason::kAllowed) {
    std::vector<blocklist::BlocklistReason> passed_reasons;
    heavy_ads_blocklist_reason_ = blocklist->IsLoadedAndAllowed(
        GetDelegate().GetWebContents()->GetLastCommittedURL().host(),
        static_cast<int>(
            heavy_ad_intervention::HeavyAdBlocklistType::kHeavyAdOnlyType),
        false /* opt_out */, &passed_reasons);
  }

  // Record whether this intervention hit the blocklist.
  if (report) {
    RecordHeavyAdInterventionDisallowedByBlocklist(
        heavy_ads_blocklist_reason_ != blocklist::BlocklistReason::kAllowed);
  }

  return heavy_ads_blocklist_reason_ != blocklist::BlocklistReason::kAllowed;
}

heavy_ad_intervention::HeavyAdBlocklist*
AdsPageLoadMetricsObserver::GetHeavyAdBlocklist() {
  if (heavy_ad_blocklist_)
    return heavy_ad_blocklist_;
  if (!heavy_ad_service_)
    return nullptr;

  return heavy_ad_service_->heavy_ad_blocklist();
}

void AdsPageLoadMetricsObserver::UpdateAggregateMemoryUsage(
    int64_t delta_bytes,
    FrameVisibility frame_visibility) {
  // For both the given |frame_visibility| and kAnyVisibility, update the
  // current aggregate memory usage by adding the needed delta, and then
  // if the current aggregate usage is greater than the recorded
  // max aggregate usage, update the max aggregate usage.
  for (const auto visibility :
       {FrameVisibility::kAnyVisibility, frame_visibility}) {
    aggregate_frame_data_->update_ad_memory_by_visibility(visibility,
                                                          delta_bytes);
  }
}

void AdsPageLoadMetricsObserver::CleanupDeletedFrame(
    content::FrameTreeNodeId id,
    FrameTreeData* frame_data,
    bool update_density_tracker,
    bool record_metrics) {
  if (!frame_data)
    return;

  if (record_metrics)
    RecordPerFrameMetrics(*frame_data, GetDelegate().GetPageUkmSourceId());

  if (update_density_tracker) {
    page_ad_density_tracker_.RemoveRect(RectId(RectType::kIFrame, id.value()),
                                        /*recalculate_viewport_density=*/true);
  }
}

}  // namespace page_load_metrics
