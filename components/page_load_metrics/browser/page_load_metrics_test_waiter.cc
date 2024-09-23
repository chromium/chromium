// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"

#include "base/check_op.h"
#include "base/i18n/number_formatting.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_tester.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace page_load_metrics {

namespace {

template <class Set>
bool IsSubset(const Set& set1, const Set& set2) {
  for (const auto& elem : set1) {
    if (set2.find(elem) == set2.end())
      return false;
  }
  return true;
}

}  // namespace

// PageLoadMetricsObserver used by the PageLoadMetricsTestWaiter to observe
// metrics updates.
class WaiterMetricsObserver final : public PageLoadMetricsObserver {
 public:
  // We use a WeakPtr to the PageLoadMetricsTestWaiter because |waiter| can be
  // destroyed before this WaiterMetricsObserver.
  explicit WaiterMetricsObserver(
      base::WeakPtr<PageLoadMetricsTestWaiter> waiter,
      const char* observer_name)
      : waiter_(waiter), observer_name_(observer_name) {}

  ~WaiterMetricsObserver() override = default;

  const char* GetObserverName() const override;

  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;

  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;

  void OnTimingUpdate(content::RenderFrameHost* subframe_rfh,
                      const mojom::PageLoadTiming& timing) override;

  void OnSoftNavigationUpdated(
      const mojom::SoftNavigationMetrics& soft_navigation_metrics) override;

  void OnPageInputTimingUpdate(uint64_t num_interactions) override;

  void OnCpuTimingUpdate(content::RenderFrameHost* subframe_rfh,
                         const mojom::CpuTiming& timing) override;

  void OnLoadingBehaviorObserved(content::RenderFrameHost* rfh,
                                 int behavior_flags) override;
  void OnLoadedResource(
      const ExtraRequestCompleteInfo& extra_request_complete_info) override;

  void OnResourceDataUseObserved(
      content::RenderFrameHost* rfh,
      const std::vector<mojom::ResourceDataUpdatePtr>& resources) override;

  void OnFeaturesUsageObserved(
      content::RenderFrameHost* rfh,
      const std::vector<blink::UseCounterFeature>&) override;

  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) override;
  void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                        const gfx::Size& frame_size) override;
  void OnMainFrameIntersectionRectChanged(
      content::RenderFrameHost* rfh,
      const gfx::Rect& main_frame_intersection_rect) override;
  void OnMainFrameViewportRectChanged(
      const gfx::Rect& main_frame_viewport_rect) override;
  void OnMainFrameImageAdRectsChanged(
      const base::flat_map<int, gfx::Rect>& main_frame_image_ad_rects) override;
  void OnV8MemoryChanged(
      const std::vector<MemoryUpdate>& memory_updates) override;
  void OnPageRenderDataUpdate(const mojom::FrameRenderDataUpdate& render_data,
                              bool is_main_frame) override;
  void OnComplete(const mojom::PageLoadTiming& timing) override;

 private:
  const base::WeakPtr<PageLoadMetricsTestWaiter> waiter_;
  const char* observer_name_;
};

std::string PageLoadMetricsTestWaiter::TimingFieldBitSet::ToDebugString()
    const {
  std::string debug_string = "";
  if (ContainsTimingField(TimingField::kFirstPaint))
    debug_string += "FirstPaint|";

  if (ContainsTimingField(TimingField::kFirstContentfulPaint))
    debug_string += "FirstContentfulPaint|";

  if (ContainsTimingField(TimingField::kFirstMeaningfulPaint))
    debug_string += "FirstMeaningfulPaint|";

  if (ContainsTimingField(TimingField::kFirstPaintAfterBackForwardCacheRestore))
    debug_string += "FirstPaintAfterBackForwardCacheRestore|";

  if (ContainsTimingField(
          TimingField::kRequestAnimationFrameAfterBackForwardCacheRestore))
    debug_string += "RequestAnimationFrameAfterBackForwardCacheRestore|";

  if (ContainsTimingField(TimingField::kDocumentWriteBlockReload))
    debug_string += "DocumentWriteBlockReload|";

  if (ContainsTimingField(TimingField::kFirstInputDelay))
    debug_string += "FirstInputDelay|";

  if (ContainsTimingField(TimingField::kFirstInputOrScroll))
    debug_string += "FirstInputOrScroll|";

  if (ContainsTimingField(
          TimingField::kFirstInputDelayAfterBackForwardCacheRestore))
    debug_string += "FirstInputDelayAfterBackForwardCacheRestore|";

  if (ContainsTimingField(TimingField::kLoadTimingInfo))
    debug_string += "LoadTimingInfo|";

  if (ContainsTimingField(TimingField::kLargestContentfulPaint))
    debug_string += "LargestContentfulPaint|";

  if (ContainsTimingField(TimingField::kFirstScrollDelay))
    debug_string += "FirstScrollDelay|";

  if (ContainsTimingField(TimingField::kLoadEvent))
    debug_string += "LoadEvent|";

  if (ContainsTimingField(TimingField::kSoftNavigationCountUpdated))
    debug_string += "SoftNavigationCountUpdated";

  return debug_string;
}

PageLoadMetricsTestWaiter::PageLoadMetricsTestWaiter(
    content::WebContents* web_contents)
    : MetricsLifecycleObserver(web_contents),
      observer_name_("WaiterMetricsObserver") {}

PageLoadMetricsTestWaiter::PageLoadMetricsTestWaiter(
    content::WebContents* web_contents,
    const char* observer_name_)
    : MetricsLifecycleObserver(web_contents), observer_name_(observer_name_) {}

PageLoadMetricsTestWaiter::~PageLoadMetricsTestWaiter() {
  CHECK(did_add_observer_);
  CHECK_EQ(nullptr, run_loop_.get());
}

void PageLoadMetricsTestWaiter::AddPageExpectation(TimingField field) {
  expected_.page_fields_.Set(field);
  if (field == TimingField::kLoadTimingInfo) {
    attach_on_tracker_creation_ = true;
  }
}

void PageLoadMetricsTestWaiter::AddFrameSizeExpectation(const gfx::Size& size) {
  expected_.frame_sizes_.insert(size);
}

void PageLoadMetricsTestWaiter::AddMainFrameIntersectionExpectation(
    const gfx::Rect& rect) {
  expected_.did_set_main_frame_intersection_ = true;
  expected_.main_frame_intersections_.push_back(rect);
}

void PageLoadMetricsTestWaiter::SetMainFrameIntersectionExpectation() {
  expected_.did_set_main_frame_intersection_ = true;
}

void PageLoadMetricsTestWaiter::SetMainFrameImageAdRectsExpectation() {
  expected_.did_observed_main_frame_image_ad_rects_ = true;
}

void PageLoadMetricsTestWaiter::AddMainFrameViewportRectExpectation(
    const gfx::Rect& rect) {
  expected_.main_frame_viewport_rect_ = rect;
}

void PageLoadMetricsTestWaiter::AddSubFrameExpectation(TimingField field) {
  CHECK_NE(field, TimingField::kLoadTimingInfo)
      << "LOAD_TIMING_INFO should only be used as a page-level expectation";
  expected_.subframe_fields_.Set(field);
}

void PageLoadMetricsTestWaiter::AddWebFeatureExpectation(
    blink::mojom::WebFeature web_feature) {
  AddUseCounterFeatureExpectation(
      {blink::mojom::UseCounterFeatureType::kWebFeature,
       static_cast<blink::UseCounterFeature::EnumValue>(web_feature)});
}

void PageLoadMetricsTestWaiter::AddUseCounterFeatureExpectation(
    const blink::UseCounterFeature& feature) {
  expected_.feature_tracker_.TestAndSet(feature);
}

void PageLoadMetricsTestWaiter::AddSubframeNavigationExpectation() {
  expected_.subframe_navigation_ = true;
}

void PageLoadMetricsTestWaiter::AddSubframeDataExpectation() {
  expected_.subframe_data_ = true;
}

void PageLoadMetricsTestWaiter::AddMinimumCompleteResourcesExpectation(
    int expected_minimum_complete_resources) {
  expected_minimum_complete_resources_ = expected_minimum_complete_resources;
}

void PageLoadMetricsTestWaiter::AddMinimumNetworkBytesExpectation(
    int expected_minimum_network_bytes) {
  expected_minimum_network_bytes_ = expected_minimum_network_bytes;
}

void PageLoadMetricsTestWaiter::AddMinimumAggregateCpuTimeExpectation(
    base::TimeDelta minimum) {
  expected_minimum_aggregate_cpu_time_ = minimum;
}

void PageLoadMetricsTestWaiter::AddMemoryUpdateExpectation(
    content::GlobalRenderFrameHostId routing_id) {
  expected_.memory_update_frame_ids_.insert(routing_id);
}

void PageLoadMetricsTestWaiter::AddLoadingBehaviorExpectation(
    int behavior_flags) {
  expected_.loading_behavior_flags_ |= behavior_flags;
}

void PageLoadMetricsTestWaiter::
    AddMinimumLargestContentfulPaintImageExpectation(int expected_minimum) {
  expected_num_largest_contentful_paint_image_ = expected_minimum;
  current_num_largest_contentful_paint_image_ = 0;
}

void PageLoadMetricsTestWaiter::AddMinimumLargestContentfulPaintTextExpectation(
    int expected_minimum) {
  expected_num_largest_contentful_paint_text_ = expected_minimum;
  current_num_largest_contentful_paint_text_ = 0;
}

void PageLoadMetricsTestWaiter::AddLargestContentfulPaintGreaterThanExpectation(
    double timestamp) {
  expected_min_largest_contentful_paint_ = timestamp;
}

void PageLoadMetricsTestWaiter::AddPageLayoutShiftExpectation(
    ShiftFrame shift_frame,
    uint64_t num_layout_shifts) {
  expected_.num_layout_shifts_ = num_layout_shifts;
  shift_frame_ = shift_frame;
  observed_.num_layout_shifts_ = 0;
}

void PageLoadMetricsTestWaiter::AddSoftNavigationCountExpectation(
    int expected_count) {
  expected_soft_navigation_count_ = expected_count;
}

void PageLoadMetricsTestWaiter::AddOnCompleteCalledExpectation() {
  expected_.on_complete_ = true;
}

bool PageLoadMetricsTestWaiter::DidObserveInPage(TimingField field) const {
  return observed_.page_fields_.IsSet(field);
}

bool PageLoadMetricsTestWaiter::DidObserveWebFeature(
    blink::mojom::WebFeature feature) const {
  return observed_.feature_tracker_.Test(
      {blink::mojom::UseCounterFeatureType::kWebFeature,
       static_cast<blink::UseCounterFeature::EnumValue>(feature)});
}

bool PageLoadMetricsTestWaiter::DidObserveMainFrameImageAdRect(
    const gfx::Rect& rect) const {
  for (auto& [id, observed_rect] : main_frame_image_ad_rects_) {
    if (observed_rect == rect) {
      return true;
    }
  }

  return false;
}

void PageLoadMetricsTestWaiter::Wait() {
  if (!ExpectationsSatisfied()) {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_ = nullptr;
  }
  AssertExpectationsSatisfied();
  ResetExpectations();
}

void PageLoadMetricsTestWaiter::OnTimingUpdated(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  auto* delegate = GetDelegateForCommittedLoad();
  if (!delegate)
    return;

  const page_load_metrics::mojom::FrameMetadata& metadata =
      subframe_rfh ? delegate->GetSubframeMetadata()
                   : delegate->GetMainFrameMetadata();
  TimingFieldBitSet matched_bits = GetMatchedBits(timing, metadata);

  if (subframe_rfh)
    observed_.subframe_fields_.Merge(matched_bits);
  else
    observed_.page_fields_.Merge(matched_bits);

  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnSoftNavigationMetricsUpdated(
    const page_load_metrics::mojom::SoftNavigationMetrics&
        new_soft_navigation_metrics) {
  soft_navigation_count_updated_ = true;
  // Increment soft navigation count.
  if (new_soft_navigation_metrics.count > current_soft_navigation_count_) {
    current_soft_navigation_count_ = new_soft_navigation_metrics.count;
  }

  // Increment image lcp update counts.
  if (!new_soft_navigation_metrics.largest_contentful_paint.is_null()) {
    if (new_soft_navigation_metrics.largest_contentful_paint
            ->largest_image_paint.has_value() &&
        new_soft_navigation_metrics.largest_contentful_paint
            ->largest_image_paint->is_positive() &&
        new_soft_navigation_metrics.largest_contentful_paint
                ->largest_image_paint->InMillisecondsF() !=
            observed_soft_navigation_image_lcp_) {
      observed_soft_navigation_image_lcp_update_++;
      observed_soft_navigation_image_lcp_ =
          new_soft_navigation_metrics.largest_contentful_paint
              ->largest_image_paint->InMillisecondsF();
    }
  }

  // Increment text lcp update counts.
  if (!new_soft_navigation_metrics.largest_contentful_paint.is_null()) {
    if (new_soft_navigation_metrics.largest_contentful_paint->largest_text_paint
            .has_value() &&
        new_soft_navigation_metrics.largest_contentful_paint->largest_text_paint
            ->is_positive() &&
        new_soft_navigation_metrics.largest_contentful_paint->largest_text_paint
                ->InMillisecondsF() != observed_soft_navigation_text_lcp_) {
      observed_soft_navigation_text_lcp_update_++;
      observed_soft_navigation_text_lcp_ =
          new_soft_navigation_metrics.largest_contentful_paint
              ->largest_text_paint->InMillisecondsF();
    }
  }
}

void PageLoadMetricsTestWaiter::OnPageInputTimingUpdated(
    uint64_t num_interactions) {
  // The number of user interactions, including click, tap and key press in this
  // update.
  current_num_interactions_ += num_interactions;
  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnCpuTimingUpdated(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::CpuTiming& timing) {
  current_aggregate_cpu_time_ += timing.task_time;
  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnLoadingBehaviorObserved(int behavior_flags) {
  observed_.loading_behavior_flags_ |= behavior_flags;
  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo&
        extra_request_complete_info) {
  if (extra_request_complete_info.request_destination !=
      network::mojom::RequestDestination::kDocument) {
    // The waiter confirms loading timing for the main frame only.
    return;
  }

  if (!extra_request_complete_info.load_timing_info->send_start.is_null() &&
      !extra_request_complete_info.load_timing_info->send_end.is_null() &&
      !extra_request_complete_info.load_timing_info->request_start.is_null()) {
    observed_.page_fields_.Set(TimingField::kLoadTimingInfo);
  }
  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnResourceDataUseObserved(
    content::RenderFrameHost* rfh,
    const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
        resources) {
  for (auto const& resource : resources) {
    HandleResourceUpdate(resource);
    if (resource->is_complete) {
      current_complete_resources_++;
      if (resource->cache_type ==
          page_load_metrics::mojom::CacheType::kNotCached)
        current_network_body_bytes_ += resource->encoded_body_length;
    }
    current_network_bytes_ += resource->delta_bytes;

    // If |rfh| is a subframe with nonzero bytes, update the subframe
    // data observation.
    if (rfh->GetParent() && resource->delta_bytes > 0)
      observed_.subframe_data_ = true;
  }
  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnFeaturesUsageObserved(
    content::RenderFrameHost* rfh,
    const std::vector<blink::UseCounterFeature>& features) {
  for (const auto& feature : features) {
    observed_.feature_tracker_.TestAndSet(feature);
  }

  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnMainFrameIntersectionRectChanged(
    content::RenderFrameHost* rfh,
    const gfx::Rect& main_frame_intersection_rect) {
  observed_.did_set_main_frame_intersection_ = true;
  observed_.main_frame_intersections_.push_back(main_frame_intersection_rect);

  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnMainFrameViewportRectChanged(
    const gfx::Rect& main_frame_viewport_rect) {
  observed_.main_frame_viewport_rect_ = main_frame_viewport_rect;

  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnMainFrameImageAdRectsChanged(
    const base::flat_map<int, gfx::Rect>& main_frame_image_ad_rects) {
  if (main_frame_image_ad_rects.empty()) {
    return;
  }

  observed_.did_observed_main_frame_image_ad_rects_ = true;

  for (auto& [id, rect] : main_frame_image_ad_rects) {
    main_frame_image_ad_rects_[id] = rect;
  }

  if (ExpectationsSatisfied() && run_loop_) {
    run_loop_->Quit();
  }
}

void PageLoadMetricsTestWaiter::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  observed_.subframe_navigation_ = true;

  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnV8MemoryChanged(
    const std::vector<MemoryUpdate>& memory_updates) {
  for (const auto& update : memory_updates)
    observed_.memory_update_frame_ids_.insert(update.routing_id);

  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::FrameSizeChanged(
    content::RenderFrameHost* render_frame_host,
    const gfx::Size& frame_size) {
  observed_.frame_sizes_.insert(frame_size);
  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnPageRenderDataUpdate(
    const mojom::FrameRenderDataUpdate& render_data,
    bool is_main_frame) {
  auto* delegate = GetDelegateForCommittedLoad();
  if (!delegate)
    return;

  bool is_relevant_frame =
      is_main_frame ? shift_frame_ == ShiftFrame::LayoutShiftOnlyInMainFrame
                    : shift_frame_ == ShiftFrame::LayoutShiftOnlyInSubFrame;
  if ((is_relevant_frame ||
       shift_frame_ == ShiftFrame::LayoutShiftOnlyInBothFrames) &&
      render_data.layout_shift_delta > 0) {
    observed_.num_layout_shifts_ += render_data.new_layout_shifts.size();
  }

  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnComplete(
    const mojom::PageLoadTiming& timing) {
  observed_.on_complete_ = true;
  if (ExpectationsSatisfied() && run_loop_) {
    run_loop_->Quit();
  }
}

PageLoadMetricsTestWaiter::TimingFieldBitSet
PageLoadMetricsTestWaiter::GetMatchedBits(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::mojom::FrameMetadata& metadata) {
  PageLoadMetricsTestWaiter::TimingFieldBitSet matched_bits;
  if (timing.document_timing->load_event_start)
    matched_bits.Set(TimingField::kLoadEvent);
  if (timing.paint_timing->first_paint)
    matched_bits.Set(TimingField::kFirstPaint);
  if (timing.paint_timing->first_contentful_paint)
    matched_bits.Set(TimingField::kFirstContentfulPaint);
  if (timing.paint_timing->first_meaningful_paint)
    matched_bits.Set(TimingField::kFirstMeaningfulPaint);
  // The largest contentful paint's size can be nonzero while the time can be 0
  // since a time of 0 is sent when the image is still painting. We set
  // LargestContentfulPaint to be observed when its time is non-zero.
  if (timing.paint_timing->largest_contentful_paint->largest_image_paint
          .has_value() &&
      timing.paint_timing->largest_contentful_paint->largest_image_paint
          ->is_positive()) {
    // Set matched bit for largest contentful paint.
    matched_bits.Set(TimingField::kLargestContentfulPaint);

    // Set observed largest contentful paint.
    if (timing.paint_timing->largest_contentful_paint->largest_image_paint
            ->InMillisecondsF() > observed_largest_contentful_paint_) {
      observed_largest_contentful_paint_ =
          timing.paint_timing->largest_contentful_paint->largest_image_paint
              ->InMillisecondsF();

      current_num_largest_contentful_paint_image_++;
    }
  }
  if (timing.paint_timing->largest_contentful_paint->largest_text_paint
          .has_value() &&
      timing.paint_timing->largest_contentful_paint->largest_text_paint
          ->is_positive()) {
    // Set matched bit for largest contentful paint.
    matched_bits.Set(TimingField::kLargestContentfulPaint);

    // Set observed largest contentful paint.
    if (timing.paint_timing->largest_contentful_paint->largest_text_paint
            ->InMillisecondsF() > observed_largest_contentful_paint_) {
      observed_largest_contentful_paint_ =
          timing.paint_timing->largest_contentful_paint->largest_text_paint
              ->InMillisecondsF();

      current_num_largest_contentful_paint_text_++;
    }
  }
  if (timing.paint_timing->first_input_or_scroll_notified_timestamp)
    matched_bits.Set(TimingField::kFirstInputOrScroll);
  if (timing.interactive_timing->first_input_delay)
    matched_bits.Set(TimingField::kFirstInputDelay);
  if (!timing.back_forward_cache_timings.empty()) {
    if (!timing.back_forward_cache_timings.back()
             ->first_paint_after_back_forward_cache_restore.is_zero()) {
      matched_bits.Set(TimingField::kFirstPaintAfterBackForwardCacheRestore);
    }
    if (timing.back_forward_cache_timings.back()
            ->first_input_delay_after_back_forward_cache_restore.has_value()) {
      matched_bits.Set(
          TimingField::kFirstInputDelayAfterBackForwardCacheRestore);
    }
    if (!timing.back_forward_cache_timings.back()
             ->request_animation_frames_after_back_forward_cache_restore
             .empty()) {
      matched_bits.Set(
          TimingField::kRequestAnimationFrameAfterBackForwardCacheRestore);
    }
  }
  if (timing.interactive_timing->first_scroll_delay)
    matched_bits.Set(TimingField::kFirstScrollDelay);

  if (soft_navigation_count_updated_) {
    soft_navigation_count_updated_ = false;
    matched_bits.Set(TimingField::kSoftNavigationCountUpdated);
  }

  return matched_bits;
}

void PageLoadMetricsTestWaiter::OnTrackerCreated(
    page_load_metrics::PageLoadTracker* tracker) {
  if (!attach_on_tracker_creation_)
    return;
  AddObserver(tracker);
}

void PageLoadMetricsTestWaiter::OnCommit(
    page_load_metrics::PageLoadTracker* tracker) {
  // Prevent double registration of two WaiterMetricsObservers.
  if (did_add_observer_) {
    return;
  }
  AddObserver(tracker);
}

void PageLoadMetricsTestWaiter::OnActivate(
    page_load_metrics::PageLoadTracker* tracker) {
  // Prevent double registration if a test added expectation before
  // prerendering navigation.
  if (did_add_observer_)
    return;
  AddObserver(tracker);
}

void PageLoadMetricsTestWaiter::AddObserver(
    page_load_metrics::PageLoadTracker* tracker) {
  ASSERT_FALSE(did_add_observer_);
  tracker->AddObserver(std::make_unique<WaiterMetricsObserver>(
      weak_factory_.GetWeakPtr(), observer_name_));
  did_add_observer_ = true;
}

bool PageLoadMetricsTestWaiter::CpuTimeExpectationsSatisfied() const {
  return current_aggregate_cpu_time_ >= expected_minimum_aggregate_cpu_time_;
}

bool PageLoadMetricsTestWaiter::LoadingBehaviorExpectationsSatisfied() const {
  // Once we've observed everything we've expected, we're satisfied. We allow
  // other behaviors to be present incidentally.
  return (expected_.loading_behavior_flags_ &
          observed_.loading_behavior_flags_) ==
         expected_.loading_behavior_flags_;
}

bool PageLoadMetricsTestWaiter::ResourceUseExpectationsSatisfied() const {
  return (expected_minimum_complete_resources_ == 0 ||
          current_complete_resources_ >=
              expected_minimum_complete_resources_) &&
         (expected_minimum_network_bytes_ == 0 ||
          current_network_bytes_ >= expected_minimum_network_bytes_);
}

bool PageLoadMetricsTestWaiter::UseCounterExpectationsSatisfied() const {
  // We are only interested to see if all features being set in
  // |expected_.feature_tracker| are observed, but don't care about whether
  // extra features are observed.
  return observed_.feature_tracker_.ContainsForTesting(
      expected_.feature_tracker_);
}

bool PageLoadMetricsTestWaiter::SubframeNavigationExpectationsSatisfied()
    const {
  return !expected_.subframe_navigation_ || observed_.subframe_navigation_;
}

bool PageLoadMetricsTestWaiter::SubframeDataExpectationsSatisfied() const {
  return !expected_.subframe_data_ || observed_.subframe_data_;
}

bool PageLoadMetricsTestWaiter::MainFrameIntersectionExpectationsSatisfied()
    const {
  if (!expected_.did_set_main_frame_intersection_)
    return true;
  if (!observed_.did_set_main_frame_intersection_)
    return false;

  // All expectations must be observed, in the same order.
  // But extra observations are ok.
  auto it = observed_.main_frame_intersections_.begin();
  for (const gfx::Rect& expected : expected_.main_frame_intersections_) {
    while (true) {
      if (it == observed_.main_frame_intersections_.end())
        return false;
      if (*it++ == expected)
        break;
    }
  }
  return true;
}

bool PageLoadMetricsTestWaiter::MainFrameViewportRectExpectationsSatisfied()
    const {
  return !expected_.main_frame_viewport_rect_ ||
         observed_.main_frame_viewport_rect_ ==
             expected_.main_frame_viewport_rect_;
}

bool PageLoadMetricsTestWaiter::MainFrameImageAdRectsExpectationsSatisfied()
    const {
  if (!expected_.did_observed_main_frame_image_ad_rects_) {
    return true;
  }

  return observed_.did_observed_main_frame_image_ad_rects_;
}

bool PageLoadMetricsTestWaiter::MemoryUpdateExpectationsSatisfied() const {
  return IsSubset(expected_.memory_update_frame_ids_,
                  observed_.memory_update_frame_ids_);
}
bool PageLoadMetricsTestWaiter::LayoutShiftExpectationsSatisfied() const {
  return expected_.num_layout_shifts_ <= observed_.num_layout_shifts_;
}

bool PageLoadMetricsTestWaiter::NumInteractionsExpectationsSatisfied() const {
  if (expected_num_interactions_ == 0)
    return true;
  return current_num_interactions_ >= expected_num_interactions_;
}

bool PageLoadMetricsTestWaiter::NumLargestContentfulPaintImageSatisfied()
    const {
  return expected_num_largest_contentful_paint_image_ <=
         current_num_largest_contentful_paint_image_;
}

bool PageLoadMetricsTestWaiter::NumLargestContentfulPaintTextSatisfied() const {
  return expected_num_largest_contentful_paint_text_ <=
         current_num_largest_contentful_paint_text_;
}

bool PageLoadMetricsTestWaiter::
    LargestContentfulPaintGreaterThanExpectationSatisfied() const {
  return observed_largest_contentful_paint_ >=
         expected_min_largest_contentful_paint_;
}

bool PageLoadMetricsTestWaiter::SoftNavigationCountExpectationSatisfied()
    const {
  return current_soft_navigation_count_ >= expected_soft_navigation_count_;
}

void PageLoadMetricsTestWaiter::AddSoftNavigationImageLCPExpectation(
    int expected_soft_navigation_image_lcp_update) {
  expected_soft_navigation_image_lcp_update_ =
      expected_soft_navigation_image_lcp_update;
}

void PageLoadMetricsTestWaiter::AddSoftNavigationTextLCPExpectation(
    int expected_soft_navigation_text_lcp_update) {
  expected_soft_navigation_text_lcp_update_ =
      expected_soft_navigation_text_lcp_update;
}

bool PageLoadMetricsTestWaiter::SoftNavigationImageLCPExpectationSatisfied()
    const {
  return observed_soft_navigation_image_lcp_update_ >=
         expected_soft_navigation_image_lcp_update_;
}

bool PageLoadMetricsTestWaiter::SoftNavigationTextLCPExpectationSatisfied()
    const {
  return observed_soft_navigation_text_lcp_update_ >=
         expected_soft_navigation_text_lcp_update_;
}

bool PageLoadMetricsTestWaiter::ExpectationsSatisfied() const {
  return expected_.page_fields_.AreAllSetIn(observed_.page_fields_) &&
         expected_.subframe_fields_.AreAllSetIn(observed_.subframe_fields_) &&
         expected_.on_complete_ == observed_.on_complete_ &&
         ResourceUseExpectationsSatisfied() &&
         UseCounterExpectationsSatisfied() &&
         SubframeNavigationExpectationsSatisfied() &&
         SubframeDataExpectationsSatisfied() &&
         IsSubset(expected_.frame_sizes_, observed_.frame_sizes_) &&
         LoadingBehaviorExpectationsSatisfied() &&
         CpuTimeExpectationsSatisfied() &&
         MainFrameIntersectionExpectationsSatisfied() &&
         MainFrameViewportRectExpectationsSatisfied() &&
         MainFrameImageAdRectsExpectationsSatisfied() &&
         MemoryUpdateExpectationsSatisfied() &&
         LayoutShiftExpectationsSatisfied() &&
         NumInteractionsExpectationsSatisfied() &&
         NumLargestContentfulPaintImageSatisfied() &&
         NumLargestContentfulPaintTextSatisfied() &&
         LargestContentfulPaintGreaterThanExpectationSatisfied() &&
         SoftNavigationCountExpectationSatisfied() &&
         SoftNavigationImageLCPExpectationSatisfied() &&
         SoftNavigationTextLCPExpectationSatisfied();
}

void PageLoadMetricsTestWaiter::AssertExpectationsSatisfied() const {
  EXPECT_TRUE(expected_.page_fields_.AreAllSetIn(observed_.page_fields_));
  EXPECT_TRUE(
      expected_.subframe_fields_.AreAllSetIn(observed_.subframe_fields_));
  EXPECT_EQ(expected_.on_complete_, observed_.on_complete_);
  EXPECT_TRUE(ResourceUseExpectationsSatisfied());
  EXPECT_TRUE(UseCounterExpectationsSatisfied());
  EXPECT_TRUE(SubframeNavigationExpectationsSatisfied());
  EXPECT_TRUE(SubframeDataExpectationsSatisfied());
  EXPECT_TRUE(IsSubset(expected_.frame_sizes_, observed_.frame_sizes_));
  EXPECT_TRUE(LoadingBehaviorExpectationsSatisfied());
  EXPECT_TRUE(CpuTimeExpectationsSatisfied());
  EXPECT_TRUE(MainFrameIntersectionExpectationsSatisfied());
  EXPECT_TRUE(MainFrameViewportRectExpectationsSatisfied());
  EXPECT_TRUE(MemoryUpdateExpectationsSatisfied());
}

void PageLoadMetricsTestWaiter::ResetExpectations() {
  expected_ = State();
  observed_ = State();
  expected_minimum_complete_resources_ = 0;
  expected_minimum_network_bytes_ = 0;
  expected_minimum_aggregate_cpu_time_ = base::TimeDelta();
}

const char* WaiterMetricsObserver::GetObserverName() const {
  return observer_name_;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
WaiterMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return FORWARD_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
WaiterMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  NOTREACHED_IN_MIGRATION()
      << "Waiters are not currently used directly on Prerendered pages.";
  return STOP_OBSERVING;
}

void WaiterMetricsObserver::OnTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (waiter_) {
    waiter_->OnTimingUpdated(subframe_rfh, timing);
  }
}

void WaiterMetricsObserver::OnSoftNavigationUpdated(
    const mojom::SoftNavigationMetrics& soft_navigation_metrics) {
  if (waiter_) {
    waiter_->OnSoftNavigationMetricsUpdated(soft_navigation_metrics);
  }
}

void WaiterMetricsObserver::OnPageInputTimingUpdate(uint64_t num_interactions) {
  if (waiter_) {
    waiter_->OnPageInputTimingUpdated(num_interactions);
  }
}

void WaiterMetricsObserver::OnCpuTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::CpuTiming& timing) {
  if (waiter_) {
    waiter_->OnCpuTimingUpdated(subframe_rfh, timing);
  }
}

void WaiterMetricsObserver::OnLoadingBehaviorObserved(content::RenderFrameHost*,
                                                      int behavior_flags) {
  if (waiter_) {
    waiter_->OnLoadingBehaviorObserved(behavior_flags);
  }
}

void WaiterMetricsObserver::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo&
        extra_request_complete_info) {
  if (waiter_) {
    waiter_->OnLoadedResource(extra_request_complete_info);
  }
}

void WaiterMetricsObserver::OnResourceDataUseObserved(
    content::RenderFrameHost* rfh,
    const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
        resources) {
  if (waiter_) {
    waiter_->OnResourceDataUseObserved(rfh, resources);
  }
}

void WaiterMetricsObserver::OnFeaturesUsageObserved(
    content::RenderFrameHost* rfh,
    const std::vector<blink::UseCounterFeature>& features) {
  if (waiter_) {
    waiter_->OnFeaturesUsageObserved(nullptr, features);
  }
}

void WaiterMetricsObserver::OnMainFrameIntersectionRectChanged(
    content::RenderFrameHost* rfh,
    const gfx::Rect& main_frame_intersection_rect) {
  if (waiter_) {
    waiter_->OnMainFrameIntersectionRectChanged(rfh,
                                                main_frame_intersection_rect);
  }
}

void WaiterMetricsObserver::OnMainFrameViewportRectChanged(
    const gfx::Rect& main_frame_viewport_rect) {
  if (waiter_) {
    waiter_->OnMainFrameViewportRectChanged(main_frame_viewport_rect);
  }
}

void WaiterMetricsObserver::OnMainFrameImageAdRectsChanged(
    const base::flat_map<int, gfx::Rect>& main_frame_image_ad_rects) {
  if (waiter_) {
    waiter_->OnMainFrameImageAdRectsChanged(main_frame_image_ad_rects);
  }
}

void WaiterMetricsObserver::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  if (waiter_) {
    waiter_->OnDidFinishSubFrameNavigation(navigation_handle);
  }
}

void WaiterMetricsObserver::FrameSizeChanged(
    content::RenderFrameHost* render_frame_host,
    const gfx::Size& frame_size) {
  if (waiter_) {
    waiter_->FrameSizeChanged(render_frame_host, frame_size);
  }
}

void WaiterMetricsObserver::OnV8MemoryChanged(
    const std::vector<MemoryUpdate>& memory_updates) {
  if (waiter_) {
    waiter_->OnV8MemoryChanged(memory_updates);
  }
}

void WaiterMetricsObserver::OnPageRenderDataUpdate(
    const mojom::FrameRenderDataUpdate& render_data,
    bool is_main_frame) {
  if (waiter_) {
    waiter_->OnPageRenderDataUpdate(render_data, is_main_frame);
  }
}

void WaiterMetricsObserver::OnComplete(const mojom::PageLoadTiming& timing) {
  if (waiter_) {
    waiter_->OnComplete(timing);
  }
}

bool PageLoadMetricsTestWaiter::FrameSizeComparator::operator()(
    const gfx::Size a,
    const gfx::Size b) const {
  return a.width() < b.width() ||
         (a.width() == b.width() && a.height() < b.height());
}

PageLoadMetricsTestWaiter::State::State() = default;

PageLoadMetricsTestWaiter::State::~State() = default;

}  // namespace page_load_metrics
