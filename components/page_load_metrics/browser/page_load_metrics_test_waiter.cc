// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"

#include "base/logging.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/common/resource_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_load_metrics {

PageLoadMetricsTestWaiter::PageLoadMetricsTestWaiter(
    content::WebContents* web_contents)
    : TestingObserver(web_contents) {}

PageLoadMetricsTestWaiter::~PageLoadMetricsTestWaiter() {
  CHECK(did_add_observer_);
  CHECK_EQ(nullptr, run_loop_.get());
}

void PageLoadMetricsTestWaiter::AddPageExpectation(TimingField field) {
  page_expected_fields_.Set(field);
  if (field == TimingField::kLoadTimingInfo) {
    attach_on_tracker_creation_ = true;
  }
}

void PageLoadMetricsTestWaiter::AddFrameSizeExpectation(const gfx::Size& size) {
  // If we have already seen this size, don't add it to the expectations.
  if (observed_frame_sizes_.find(size) != observed_frame_sizes_.end())
    return;
  expected_frame_sizes_.insert(size);
}

void PageLoadMetricsTestWaiter::AddSubFrameExpectation(TimingField field) {
  CHECK_NE(field, TimingField::kLoadTimingInfo)
      << "LOAD_TIMING_INFO should only be used as a page-level expectation";
  subframe_expected_fields_.Set(field);
}

void PageLoadMetricsTestWaiter::AddWebFeatureExpectation(
    blink::mojom::WebFeature web_feature) {
  size_t feature_idx = static_cast<size_t>(web_feature);
  if (!expected_web_features_.test(feature_idx)) {
    expected_web_features_.set(feature_idx);
  }
}

void PageLoadMetricsTestWaiter::AddSubframeNavigationExpectation() {
  expected_subframe_navigation_ = true;
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

bool PageLoadMetricsTestWaiter::DidObserveInPage(TimingField field) const {
  return observed_page_fields_.IsSet(field);
}

bool PageLoadMetricsTestWaiter::DidObserveWebFeature(
    blink::mojom::WebFeature feature) const {
  return observed_web_features_.test(static_cast<size_t>(feature));
}

void PageLoadMetricsTestWaiter::Wait() {
  if (ExpectationsSatisfied())
    return;

  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_ = nullptr;

  EXPECT_TRUE(ExpectationsSatisfied());
}

void PageLoadMetricsTestWaiter::OnTimingUpdated(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (ExpectationsSatisfied())
    return;
  const page_load_metrics::mojom::PageLoadMetadata& metadata =
      subframe_rfh ? GetDelegateForCommittedLoad().GetSubframeMetadata()
                   : GetDelegateForCommittedLoad().GetMainFrameMetadata();
  TimingFieldBitSet matched_bits = GetMatchedBits(timing, metadata);
  if (subframe_rfh) {
    subframe_expected_fields_.ClearMatching(matched_bits);
  } else {
    page_expected_fields_.ClearMatching(matched_bits);
    observed_page_fields_.Merge(matched_bits);
  }
  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnCpuTimingUpdated(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::CpuTiming& timing) {
  if (ExpectationsSatisfied())
    return;
  current_aggregate_cpu_time_ += timing.task_time;
  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo&
        extra_request_complete_info) {
  if (ExpectationsSatisfied())
    return;

  if (extra_request_complete_info.resource_type !=
      content::ResourceType::kMainFrame) {
    // The waiter confirms loading timing for the main frame only.
    return;
  }

  if (!extra_request_complete_info.load_timing_info->send_start.is_null() &&
      !extra_request_complete_info.load_timing_info->send_end.is_null() &&
      !extra_request_complete_info.load_timing_info->request_start.is_null()) {
    page_expected_fields_.Clear(TimingField::kLoadTimingInfo);
    observed_page_fields_.Set(TimingField::kLoadTimingInfo);
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
  }
  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnFeaturesUsageObserved(
    content::RenderFrameHost* rfh,
    const mojom::PageLoadFeatures& features) {
  if (WebFeaturesExpectationsSatisfied())
    return;

  for (blink::mojom::WebFeature feature : features.features) {
    size_t feature_idx = static_cast<size_t>(feature);
    if (observed_web_features_.test(feature_idx))
      continue;
    observed_web_features_.set(feature_idx);
  }

  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  if (SubframeNavigationExpectationsSatisfied())
    return;

  expected_subframe_navigation_ = false;

  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::FrameSizeChanged(
    content::RenderFrameHost* render_frame_host,
    const gfx::Size& frame_size) {
  observed_frame_sizes_.insert(frame_size);
  expected_frame_sizes_.erase(frame_size);
  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

PageLoadMetricsTestWaiter::TimingFieldBitSet
PageLoadMetricsTestWaiter::GetMatchedBits(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::mojom::PageLoadMetadata& metadata) {
  PageLoadMetricsTestWaiter::TimingFieldBitSet matched_bits;
  if (timing.document_timing->first_layout)
    matched_bits.Set(TimingField::kFirstLayout);
  if (timing.document_timing->load_event_start)
    matched_bits.Set(TimingField::kLoadEvent);
  if (timing.paint_timing->first_paint)
    matched_bits.Set(TimingField::kFirstPaint);
  if (timing.paint_timing->first_contentful_paint)
    matched_bits.Set(TimingField::kFirstContentfulPaint);
  if (timing.paint_timing->first_meaningful_paint)
    matched_bits.Set(TimingField::kFirstMeaningfulPaint);
  if (metadata.behavior_flags &
      blink::LoadingBehaviorFlag::kLoadingBehaviorDocumentWriteBlockReload)
    matched_bits.Set(TimingField::kDocumentWriteBlockReload);

  return matched_bits;
}

void PageLoadMetricsTestWaiter::OnTrackerCreated(
    page_load_metrics::PageLoadTracker* tracker) {
  if (!attach_on_tracker_creation_)
    return;
  // A PageLoadMetricsWaiter should only wait for events from a single page
  // load.
  ASSERT_FALSE(did_add_observer_);
  tracker->AddObserver(
      std::make_unique<WaiterMetricsObserver>(weak_factory_.GetWeakPtr()));
  did_add_observer_ = true;
}

void PageLoadMetricsTestWaiter::OnCommit(
    page_load_metrics::PageLoadTracker* tracker) {
  if (attach_on_tracker_creation_)
    return;
  // A PageLoadMetricsWaiter should only wait for events from a single page
  // load.
  ASSERT_FALSE(did_add_observer_);
  tracker->AddObserver(
      std::make_unique<WaiterMetricsObserver>(weak_factory_.GetWeakPtr()));
  did_add_observer_ = true;
}

bool PageLoadMetricsTestWaiter::CpuTimeExpectationsSatisfied() const {
  return current_aggregate_cpu_time_ >= expected_minimum_aggregate_cpu_time_;
}

bool PageLoadMetricsTestWaiter::ResourceUseExpectationsSatisfied() const {
  return (expected_minimum_complete_resources_ == 0 ||
          current_complete_resources_ >=
              expected_minimum_complete_resources_) &&
         (expected_minimum_network_bytes_ == 0 ||
          current_network_bytes_ >= expected_minimum_network_bytes_);
}

bool PageLoadMetricsTestWaiter::WebFeaturesExpectationsSatisfied() const {
  // We are only interested to see if all features being set in
  // |expected_web_features_| are observed, but don't care about whether extra
  // features are observed.
  return (expected_web_features_ & observed_web_features_ ^
          expected_web_features_)
      .none();
}

bool PageLoadMetricsTestWaiter::SubframeNavigationExpectationsSatisfied()
    const {
  return !expected_subframe_navigation_;
}

bool PageLoadMetricsTestWaiter::ExpectationsSatisfied() const {
  return subframe_expected_fields_.Empty() && page_expected_fields_.Empty() &&
         ResourceUseExpectationsSatisfied() &&
         WebFeaturesExpectationsSatisfied() &&
         SubframeNavigationExpectationsSatisfied() &&
         expected_frame_sizes_.empty() && CpuTimeExpectationsSatisfied();
}

PageLoadMetricsTestWaiter::WaiterMetricsObserver::~WaiterMetricsObserver() {}

PageLoadMetricsTestWaiter::WaiterMetricsObserver::WaiterMetricsObserver(
    base::WeakPtr<PageLoadMetricsTestWaiter> waiter)
    : waiter_(waiter) {}

void PageLoadMetricsTestWaiter::WaiterMetricsObserver::OnTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (waiter_)
    waiter_->OnTimingUpdated(subframe_rfh, timing);
}

void PageLoadMetricsTestWaiter::WaiterMetricsObserver::OnCpuTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::CpuTiming& timing) {
  if (waiter_)
    waiter_->OnCpuTimingUpdated(subframe_rfh, timing);
}

void PageLoadMetricsTestWaiter::WaiterMetricsObserver::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo&
        extra_request_complete_info) {
  if (waiter_)
    waiter_->OnLoadedResource(extra_request_complete_info);
}

void PageLoadMetricsTestWaiter::WaiterMetricsObserver::
    OnResourceDataUseObserved(
        content::RenderFrameHost* rfh,
        const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
            resources) {
  if (waiter_)
    waiter_->OnResourceDataUseObserved(rfh, resources);
}

void PageLoadMetricsTestWaiter::WaiterMetricsObserver::OnFeaturesUsageObserved(
    content::RenderFrameHost* rfh,
    const mojom::PageLoadFeatures& features) {
  if (waiter_)
    waiter_->OnFeaturesUsageObserved(nullptr, features);
}

void PageLoadMetricsTestWaiter::WaiterMetricsObserver::
    OnDidFinishSubFrameNavigation(
        content::NavigationHandle* navigation_handle) {
  if (waiter_)
    waiter_->OnDidFinishSubFrameNavigation(navigation_handle);
}

void PageLoadMetricsTestWaiter::WaiterMetricsObserver::FrameSizeChanged(
    content::RenderFrameHost* render_frame_host,
    const gfx::Size& frame_size) {
  if (waiter_)
    waiter_->FrameSizeChanged(render_frame_host, frame_size);
}

bool PageLoadMetricsTestWaiter::FrameSizeComparator::operator()(
    const gfx::Size a,
    const gfx::Size b) const {
  return a.width() < b.width() ||
         (a.width() == b.width() && a.height() < b.height());
}

}  // namespace page_load_metrics
