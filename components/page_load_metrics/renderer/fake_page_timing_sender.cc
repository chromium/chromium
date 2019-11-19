// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/renderer/fake_page_timing_sender.h"

#include <algorithm>

#include "testing/gtest/include/gtest/gtest.h"

namespace page_load_metrics {

FakePageTimingSender::FakePageTimingSender(PageTimingValidator* validator)
    : validator_(validator) {}

FakePageTimingSender::~FakePageTimingSender() {}

void FakePageTimingSender::SendTiming(
    const mojom::PageLoadTimingPtr& timing,
    const mojom::PageLoadMetadataPtr& metadata,
    mojom::PageLoadFeaturesPtr new_features,
    std::vector<mojom::ResourceDataUpdatePtr> resources,
    const mojom::FrameRenderDataUpdate& render_data,
    const mojom::CpuTimingPtr& cpu_timing,
    mojom::DeferredResourceCountsPtr new_deferred_resource_data) {
  validator_->UpdateTiming(timing, metadata, new_features, resources,
                           render_data, cpu_timing, new_deferred_resource_data);
}

FakePageTimingSender::PageTimingValidator::PageTimingValidator() {}

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
    const blink::mojom::WebFeature feature) {
  expected_features_.insert(feature);
}

void FakePageTimingSender::PageTimingValidator::
    UpdateExpectPageLoadCssProperties(
        blink::mojom::CSSSampleId css_property_id) {
  expected_css_properties_.insert(css_property_id);
}

void FakePageTimingSender::PageTimingValidator::VerifyExpectedFeatures() const {
  ASSERT_EQ(actual_features_.size(), expected_features_.size());
  std::vector<blink::mojom::WebFeature> diff;
  std::set_difference(actual_features_.begin(), actual_features_.end(),
                      expected_features_.begin(), expected_features_.end(),
                      diff.begin());
  EXPECT_TRUE(diff.empty())
      << "Expected more features than the actual features observed";

  std::set_difference(expected_features_.begin(), expected_features_.end(),
                      actual_features_.begin(), actual_features_.end(),
                      diff.begin());
  EXPECT_TRUE(diff.empty())
      << "More features are actually observed than expected";
}

void FakePageTimingSender::PageTimingValidator::VerifyExpectedCssProperties()
    const {
  ASSERT_EQ(actual_css_properties_.size(), expected_css_properties_.size());
  std::vector<blink::mojom::CSSSampleId> diff;
  std::set_difference(actual_css_properties_.begin(),
                      actual_css_properties_.end(),
                      expected_css_properties_.begin(),
                      expected_css_properties_.end(), diff.begin());
  EXPECT_TRUE(diff.empty())
      << "Expected more CSS properties than the actual features observed";

  std::set_difference(expected_css_properties_.begin(),
                      expected_css_properties_.end(),
                      actual_css_properties_.begin(),
                      actual_css_properties_.end(), diff.begin());
  EXPECT_TRUE(diff.empty())
      << "More CSS Properties are actually observed than expected";
}

void FakePageTimingSender::PageTimingValidator::VerifyExpectedRenderData()
    const {
  EXPECT_FLOAT_EQ(expected_render_data_.layout_shift_delta,
                  actual_render_data_.layout_shift_delta);
}

void FakePageTimingSender::PageTimingValidator::UpdateTiming(
    const mojom::PageLoadTimingPtr& timing,
    const mojom::PageLoadMetadataPtr& metadata,
    const mojom::PageLoadFeaturesPtr& new_features,
    const std::vector<mojom::ResourceDataUpdatePtr>& resources,
    const mojom::FrameRenderDataUpdate& render_data,
    const mojom::CpuTimingPtr& cpu_timing,
    const mojom::DeferredResourceCountsPtr& new_deferred_resource_data) {
  actual_timings_.push_back(timing.Clone());
  if (!cpu_timing->task_time.is_zero()) {
    actual_cpu_timings_.push_back(cpu_timing.Clone());
  }
  for (const auto feature : new_features->features) {
    EXPECT_EQ(actual_features_.find(feature), actual_features_.end())
        << "Feature " << feature << "has been sent more than once";
    actual_features_.insert(feature);
  }
  for (const auto css_property_id : new_features->css_properties) {
    EXPECT_EQ(actual_css_properties_.find(css_property_id),
              actual_css_properties_.end())
        << "CSS Property ID " << css_property_id
        << "has been sent more than once";
    actual_css_properties_.insert(css_property_id);
  }
  actual_render_data_.layout_shift_delta = render_data.layout_shift_delta;
  VerifyExpectedTimings();
  VerifyExpectedCpuTimings();
  VerifyExpectedFeatures();
  VerifyExpectedCssProperties();
  VerifyExpectedRenderData();
}

}  // namespace page_load_metrics
