// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/renderer/fake_page_timing_sender.h"

#include <algorithm>

#include "components/page_load_metrics/common/page_load_metrics_debug_string.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_load_metrics {

FakePageTimingSender::FakePageTimingSender(PageTimingValidator* validator)
    : validator_(validator) {}

FakePageTimingSender::~FakePageTimingSender() = default;

void FakePageTimingSender::SendTiming(
    const mojom::PageLoadTimingPtr& timing,
    const mojom::FrameMetadataPtr& metadata,
    const std::vector<blink::UseCounterFeature>& new_features,
    std::vector<mojom::ResourceDataUpdatePtr> resources,
    const mojom::FrameRenderDataUpdate& render_data,
    const mojom::CpuTimingPtr& cpu_timing,
    std::vector<mojom::EventTimingPtr> event_timings,
    const std::optional<blink::SubresourceLoadMetrics>&
        subresource_load_metrics,
    const mojom::SoftNavigationMetricsPtr& soft_navigation_metrics,
    const mojom::LargestContentfulPaintTimingPtr& soft_largest_contentful_paint,
    std::vector<mojom::CustomUserTimingMarkPtr> user_timings) {
  validator_->UpdateTiming(timing, metadata, new_features, resources,
                           render_data, cpu_timing, event_timings,
                           subresource_load_metrics, soft_navigation_metrics,
                           soft_largest_contentful_paint);
}

void FakePageTimingSender::SetUpDroppedFramesReporting(
    base::ReadOnlySharedMemoryRegion dropped_frames_memory) {}

void FakePageTimingSender::SendCustomUserTiming(
    mojom::CustomUserTimingMarkPtr timing) {}

FakePageTimingSender::PageTimingValidator::PageTimingValidator() = default;

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

void FakePageTimingSender::PageTimingValidator::ExpectSoftNavigationMetrics(
    const mojom::SoftNavigationMetrics& soft_navigation_metrics) {
  VerifyExpectedSoftNavigationMetrics();
  expected_soft_navigation_metrics_.push_back(soft_navigation_metrics.Clone());
}

void FakePageTimingSender::PageTimingValidator::
    VerifyExpectedSoftNavigationMetrics() const {
  ASSERT_EQ(actual_soft_navigation_metrics_.size(),
            expected_soft_navigation_metrics_.size());
  for (size_t i = 0; i < actual_soft_navigation_metrics_.size(); ++i) {
    if (actual_soft_navigation_metrics_.at(i)->Equals(
            *expected_soft_navigation_metrics_.at(i))) {
      continue;
    }
    ADD_FAILURE() << "Actual soft navigation metric != expected at index "
                  << i << "\n"
                  << "Actual: "
                  << DebugString(*actual_soft_navigation_metrics_.at(i))
                  << "\nExpected: "
                  << DebugString(*expected_soft_navigation_metrics_.at(i));
  }
}

void FakePageTimingSender::PageTimingValidator::
    ExpectSoftLargestContentfulPaint(const mojom::LargestContentfulPaintTiming&
                                         soft_largest_contentful_paint) {
  VerifyExpectedSoftLargestContentfulPaint();
  expected_soft_largest_contentful_paint_.push_back(
      soft_largest_contentful_paint.Clone());
}

void FakePageTimingSender::PageTimingValidator::
    VerifyExpectedSoftLargestContentfulPaint() const {
  ASSERT_EQ(actual_soft_largest_contentful_paint_.size(),
            expected_soft_largest_contentful_paint_.size());
  for (size_t i = 0; i < actual_soft_largest_contentful_paint_.size(); ++i) {
    if (actual_soft_largest_contentful_paint_.at(i)->Equals(
            *expected_soft_largest_contentful_paint_.at(i))) {
      continue;
    }
    ADD_FAILURE()
        << "Actual soft largest contentful paint != expected at index " << i
        << "\n"
        << "Actual: "
        << DebugString(*actual_soft_largest_contentful_paint_.at(i))
        << "\nExpected: "
        << DebugString(*expected_soft_largest_contentful_paint_.at(i));
  }
}

void FakePageTimingSender::PageTimingValidator::UpdateExpectedInteractionTiming(
    const base::TimeDelta interaction_duration,
    uint64_t interaction_offset,
    const base::TimeTicks interaction_time) {
  expected_event_timings_.push_back(mojom::EventTiming::New(
      interaction_duration, interaction_offset, interaction_time));
}
void FakePageTimingSender::PageTimingValidator::
    VerifyExpectedInteractionTiming() const {
  ASSERT_EQ(expected_event_timings_.size(), actual_event_timings_.size());
  for (size_t i = 0; i < expected_event_timings_.size(); ++i) {
    ASSERT_EQ(expected_event_timings_[i]->duration,
              actual_event_timings_[i]->duration);
  }
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
  if (expected_render_data_.is_null()) {
    EXPECT_TRUE(actual_render_data_.new_layout_shifts.empty());
  } else {
    EXPECT_EQ(expected_render_data_->new_layout_shifts.size(),
              actual_render_data_.new_layout_shifts.size());
    for (size_t ii = 0; ii < expected_render_data_->new_layout_shifts.size();
         ++ii) {
      EXPECT_FLOAT_EQ(
          expected_render_data_->new_layout_shifts[ii]->layout_shift_score,
          actual_render_data_.new_layout_shifts[ii]->layout_shift_score);
      EXPECT_EQ(
          expected_render_data_->new_layout_shifts[ii]->after_input_or_scroll,
          actual_render_data_.new_layout_shifts[ii]->after_input_or_scroll);
    }
  }
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
    const std::vector<mojom::EventTimingPtr>& event_timings,
    const std::optional<blink::SubresourceLoadMetrics>&
        subresource_load_metrics,
    const mojom::SoftNavigationMetricsPtr& soft_navigation_metrics,
    const mojom::LargestContentfulPaintTimingPtr&
        soft_largest_contentful_paint) {
  actual_timings_.push_back(timing.Clone());
  actual_soft_navigation_metrics_.push_back(soft_navigation_metrics->Clone());
  actual_soft_largest_contentful_paint_.push_back(
      soft_largest_contentful_paint.Clone());

  if (!cpu_timing->task_time.is_zero()) {
    actual_cpu_timings_.push_back(cpu_timing.Clone());
  }

  for (const blink::UseCounterFeature& feature : new_features) {
    EXPECT_EQ(actual_features_.find(feature), actual_features_.end())
        << "Feature " << feature.type() << ": " << feature.value()
        << " has been sent more than once";
    actual_features_.insert(feature);
  }

  for (const auto& entry : render_data.new_layout_shifts) {
    actual_render_data_.new_layout_shifts.emplace_back(entry.Clone());
  }

  actual_main_frame_intersection_rect_ = metadata->main_frame_intersection_rect;
  actual_main_frame_viewport_rect_ = metadata->main_frame_viewport_rect;

  for (const mojom::EventTimingPtr& user_interaction : event_timings) {
    actual_event_timings_.emplace_back(mojom::EventTiming::New(
        user_interaction->duration, user_interaction->interaction_id,
        user_interaction->start_time));
  }

  actual_subresource_load_metrics_ = subresource_load_metrics;

  VerifyExpectedTimings();
  VerifyExpectedCpuTimings();
  VerifyExpectedFeatures();
  VerifyExpectedRenderData();
  VerifyExpectedMainFrameIntersectionRect();
  VerifyExpectedMainFrameViewportRect();
  VerifyExpectedSubresourceLoadMetrics();
  VerifyExpectedSoftNavigationMetrics();
}

}  // namespace page_load_metrics
