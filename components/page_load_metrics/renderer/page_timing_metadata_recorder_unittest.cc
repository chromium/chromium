// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/renderer/page_timing_metadata_recorder.h"

#include <vector>

#include "base/profiler/sample_metadata.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_load_metrics {

struct MetadataTaggingRequest {
  base::TimeTicks period_start;
  base::TimeTicks period_end;
  base::StringPiece name;
  int64_t key;
  int64_t value;
};

class TestPageTimingMetadataRecorder : public PageTimingMetadataRecorder {
 public:
  explicit TestPageTimingMetadataRecorder(const MonotonicTiming& initial_timing)
      : PageTimingMetadataRecorder(initial_timing) {}

  void ApplyMetadataToPastSamples(base::TimeTicks period_start,
                                  base::TimeTicks period_end,
                                  base::StringPiece name,
                                  int64_t key,
                                  int64_t value,
                                  base::SampleMetadataScope scope) override {
    requests_.push_back({
        period_start,
        period_end,
        name,
        key,
        value,
    });
  }

  const std::vector<MetadataTaggingRequest>& GetMetadataTaggingRequests()
      const {
    return requests_;
  }

 private:
  std::vector<MetadataTaggingRequest> requests_ = {};
};

using PageTimingMetadataRecorderTest = testing::Test;

TEST_F(PageTimingMetadataRecorderTest, FirstContentfulPaintUpdate) {
  PageTimingMetadataRecorder::MonotonicTiming timing = {};
  // The PageTimingMetadataRecorder constructor is supposed to call
  // UpdateMetadata once, but due to class construction limitation, the
  // call to ApplyMetadataToPastSample will not be captured by test class,
  // as the test class is not ready yet.
  TestPageTimingMetadataRecorder recorder(timing);
  const std::vector<MetadataTaggingRequest>& requests =
      recorder.GetMetadataTaggingRequests();

  timing.navigation_start = base::TimeTicks::Now() - base::Milliseconds(500);
  timing.first_contentful_paint =
      *timing.navigation_start + base::Milliseconds(10);
  recorder.UpdateMetadata(timing);
  ASSERT_EQ(1u, requests.size());
  EXPECT_EQ(*timing.navigation_start, requests.at(0).period_start);
  EXPECT_EQ(*timing.first_contentful_paint, requests.at(0).period_end);
  EXPECT_EQ("PageLoad.PaintTiming.NavigationToFirstContentfulPaint",
            requests.at(0).name);

  // Update first contentful paint timetick should sends another request.
  timing.first_contentful_paint =
      *timing.navigation_start + base::Milliseconds(20);
  recorder.UpdateMetadata(timing);
  ASSERT_EQ(2u, requests.size());
  EXPECT_EQ(*timing.navigation_start, requests.at(1).period_start);
  EXPECT_EQ(*timing.first_contentful_paint, requests.at(1).period_end);
  EXPECT_EQ("PageLoad.PaintTiming.NavigationToFirstContentfulPaint",
            requests.at(1).name);

  // If nothing modified, should not send any requests.
  recorder.UpdateMetadata(timing);
  EXPECT_EQ(2u, requests.size());
}

TEST_F(PageTimingMetadataRecorderTest, FirstInputDelayUpdate) {
  PageTimingMetadataRecorder::MonotonicTiming timing = {};
  // The PageTimingMetadataRecorder constructor is supposed to call
  // UpdateMetadata once, but due to class construction limitation, the
  // call to ApplyMetadataToPastSample will not be captured by test class,
  // as the test class is not ready yet.
  TestPageTimingMetadataRecorder recorder(timing);
  const std::vector<MetadataTaggingRequest>& requests =
      recorder.GetMetadataTaggingRequests();

  timing.first_input_delay = base::Milliseconds(10);
  timing.first_input_timestamp = base::TimeTicks::Now();
  recorder.UpdateMetadata(timing);
  ASSERT_EQ(1u, requests.size());
  EXPECT_EQ(*timing.first_input_timestamp, requests.at(0).period_start);
  EXPECT_EQ(*timing.first_input_timestamp + *timing.first_input_delay,
            requests.at(0).period_end);
  EXPECT_EQ("PageLoad.InteractiveTiming.FirstInputDelay4", requests.at(0).name);

  // Update first input delay should sends another request.
  timing.first_input_delay = base::Milliseconds(11);
  recorder.UpdateMetadata(timing);
  ASSERT_EQ(2u, requests.size());
  EXPECT_EQ(*timing.first_input_timestamp, requests.at(1).period_start);
  EXPECT_EQ(*timing.first_input_timestamp + *timing.first_input_delay,
            requests.at(1).period_end);
  EXPECT_EQ("PageLoad.InteractiveTiming.FirstInputDelay4", requests.at(1).name);

  // If nothing modified, should not send any requests.
  recorder.UpdateMetadata(timing);
  EXPECT_EQ(2u, requests.size());
}

}  // namespace page_load_metrics
