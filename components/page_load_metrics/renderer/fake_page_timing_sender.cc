// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/renderer/fake_page_timing_sender.h"

#include <algorithm>

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_load_metrics {

FakePageTimingSender::FakePageTimingSender(PageTimingValidator* validator)
    : validator_(validator) {}

FakePageTimingSender::~FakePageTimingSender() {}

void FakePageTimingSender::SendTiming(
    const mojom::PageLoadTimingPtr& timing,
    const mojom::FrameMetadataPtr& metadata,
    const std::vector<blink::UseCounterFeature>& new_features,
    std::vector<mojom::ResourceDataUpdatePtr> resources,
    const mojom::FrameRenderDataUpdate& render_data,
    const mojom::CpuTimingPtr& cpu_timing,
    const mojom::InputTimingPtr new_input_timing,
    const absl::optional<blink::SubresourceLoadMetrics>&
        subresource_load_metrics,
    uint32_t soft_navigation_count) {
  validator_->UpdateTiming(timing, metadata, new_features, resources,
                           render_data, cpu_timing, new_input_timing,
                           subresource_load_metrics, soft_navigation_count);
}

void FakePageTimingSender::SetUpSmoothnessReporting(
    base::ReadOnlySharedMemoryRegion shared_memory) {}

FakePageTimingSender::PageTimingValidator::PageTimingValidator()
    : expected_input_timing(mojom::InputTiming::New()),
      actual_input_timing(mojom::InputTiming::New()) {}

FakePageTimingSender::PageTimingValidator::~PageTimingValidator() {
  VerifyExpectedTimings();
}

void FakePageTimingSender::PageTimingValidator::ExpectPageLoadTiming(
    const mojom::PageLoadTiming& timing) {
  VerifyExpectedTimings();
  expected_timings_.push_back(timing.Clone());
}

void FakePageTimingSender::PageTimingValidator::ExpectCpuTiming(
    const base::TimeDelta& timing) {
  VerifyExpectedCpuTimings();
  expected_cpu_timings_.push_back(mojom::CpuTiming(timing).Clone());
}

void FakePageTimingSender::PageTimingValidator::VerifyExpectedTimings() const {
  // Ideally we'd just call ASSERT_EQ(actual_timings_, expected_timings_) here,
  // but this causes the generated gtest code to fail to build on Windows. See
  // the comments in the header file for additional details.
  ASSERT_EQ(actual_timings_.size(), expected_timings_.size());
  for (size_t i = 0; i < actual_timings_.size(); ++i) {
    if (actual_timings_.at(i)->Equals(*expected_timings_.at(i)))
      continue;
    ADD_FAILURE() << "Actual timing != expected timing at index " << i;
  }
}

void FakePageTimingSender::PageTimingValidator::UpdateExpectedInputTiming(
    const base::TimeDelta input_delay) {
  expected_input_timing->num_input_events++;
  expected_input_timing->total_input_delay += input_delay;
  expected_input_timing->total_adjusted_input_delay += base::Milliseconds(
      std::max(int64_t(0), input_delay.InMilliseconds() - int64_t(50)));
}
void FakePageTimingSender::PageTimingValidator::VerifyExpectedInputTiming()
    const {
  ASSERT_EQ(expected_input_timing.is_null(), actual_input_timing.is_null());
  ASSERT_EQ(expected_input_timing->num_input_events,
            actual_input_timing->num_input_events);
  ASSERT_EQ(expected_input_timing->total_input_delay,
            actual_input_timing->total_input_delay);
  ASSERT_EQ(expected_input_timing->total_adjusted_input_delay,
            actual_input_timing->total_adjusted_input_delay);
}

void FakePageTimingSender::PageTimingValidator::
    UpdateExpectedSubresourceLoadMetrics(
        const blink::SubresourceLoadMetrics& subresource_load_metrics) {
  expected_subresource_load_metrics_ = subresource_load_metrics;
}

void FakePageTimingSender::PageTimingValidator::
    VerifyExpectedSubresourceLoadMetrics() const {
  ASSERT_EQ(expected_subresource_load_metrics_,
            actual_subresource_load_metrics_);
}

void FakePageTimingSender::PageTimingValidator::VerifyExpectedCpuTimings()
    const {
  ASSERT_EQ(actual_cpu_timings_.size(), expected_cpu_timings_.size());
  for (size_t i = 0; i < actual_cpu_timings_.size(); ++i) {
    if (actual_cpu_timings_.at(i)->task_time ==
        expected_cpu_timings_.at(i)->task_time)
      continue;
    ADD_FAILURE() << "Actual cpu timing != expected cpu timing at index " << i;
  }
}

void FakePageTimingSender::PageTimingValidator::UpdateExpectPageLoadFeatures(
    const blink::UseCounterFeature& feature) {
  expected_features_.insert(feature);
}

void FakePageTimingSender::PageTimingValidator::VerifyExpectedFeatures() const {
  EXPECT_THAT(actual_features_, ::testing::ContainerEq(expected_features_));
}

void FakePageTimingSender::PageTimingValidator::VerifyExpectedRenderData()
    const {
  EXPECT_FLOAT_EQ(expected_render_data_.is_null()
                      ? 0.0
                      : expected_render_data_->layout_shift_delta,
                  actual_render_data_.layout_shift_delta);
}

void FakePageTimingSender::PageTimingValidator::
    VerifyExpectedMainFrameIntersectionRect() const {
  EXPECT_EQ(expected_main_frame_intersection_rect_,
            actual_main_frame_intersection_rect_);
}

void FakePageTimingSender::PageTimingValidator::
    VerifyExpectedMainFrameViewportRect() const {
  EXPECT_EQ(expected_main_frame_viewport_rect_,
            actual_main_frame_viewport_rect_);
}

void FakePageTimingSender::PageTimingValidator::UpdateTiming(
    const mojom::PageLoadTimingPtr& timing,
    const mojom::FrameMetadataPtr& metadata,
    const std::vector<blink::UseCounterFeature>& new_features,
    const std::vector<mojom::ResourceDataUpdatePtr>& resources,
    const mojom::FrameRenderDataUpdate& render_data,
    const mojom::CpuTimingPtr& cpu_timing,
    const mojom::InputTimingPtr& new_input_timing,
    const absl::optional<blink::SubresourceLoadMetrics>&
        subresource_load_metrics,
    uint32_t soft_navigation_count) {
  actual_timings_.push_back(timing.Clone());
  if (!cpu_timing->task_time.is_zero()) {
    actual_cpu_timings_.push_back(cpu_timing.Clone());
  }
  for (const blink::UseCounterFeature& feature : new_features) {
    EXPECT_EQ(actual_features_.find(feature), actual_features_.end())
        << "Feature " << feature.type() << ": " << feature.value()
        << " has been sent more than once";
    actual_features_.insert(feature);
  }

  actual_render_data_.layout_shift_delta = render_data.layout_shift_delta;
  actual_main_frame_intersection_rect_ = metadata->main_frame_intersection_rect;
  actual_main_frame_viewport_rect_ = metadata->main_frame_viewport_rect;

  actual_input_timing->num_input_events += new_input_timing->num_input_events;
  actual_input_timing->total_input_delay += new_input_timing->total_input_delay;
  actual_input_timing->total_adjusted_input_delay +=
      new_input_timing->total_adjusted_input_delay;
  actual_subresource_load_metrics_ = subresource_load_metrics;

  VerifyExpectedTimings();
  VerifyExpectedCpuTimings();
  VerifyExpectedFeatures();
  VerifyExpectedRenderData();
  VerifyExpectedMainFrameIntersectionRect();
  VerifyExpectedMainFrameViewportRect();
  VerifyExpectedSubresourceLoadMetrics();
  // TODO(yoav): Verify that soft nav count matches expectations.
}

}  // namespace page_load_metrics
