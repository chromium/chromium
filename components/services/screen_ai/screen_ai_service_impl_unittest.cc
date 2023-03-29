// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/screen_ai_service_impl.h"

#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace screen_ai {

TEST(ScreenAIServiceImplTest, RecordMetrics_Success) {
  ukm::TestUkmRecorder test_recorder;
  base::TimeDelta elapsed_time = base::Microseconds(1000);
  ScreenAIService::RecordMetrics(test_recorder.GetNewSourceID(), &test_recorder,
                                 elapsed_time, true);

  auto entries = test_recorder.GetEntriesByName(
      ukm::builders::Accessibility_ScreenAI::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  test_recorder.ExpectEntryMetric(entries.front(),
                                  "Screen2xDistillationTime.Success", 1);
}

TEST(ScreenAIServiceImplTest, RecordMetrics_Failure) {
  ukm::TestUkmRecorder test_recorder;
  base::TimeDelta elapsed_time = base::Microseconds(2000);
  ScreenAIService::RecordMetrics(test_recorder.GetNewSourceID(), &test_recorder,
                                 elapsed_time, false);

  auto entries = test_recorder.GetEntriesByName(
      ukm::builders::Accessibility_ScreenAI::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  test_recorder.ExpectEntryMetric(entries.front(),
                                  "Screen2xDistillationTime.Failure", 2);
}

TEST(ScreenAIServiceImplTest, RecordMetrics_InvalidSourceID) {
  ukm::TestUkmRecorder test_recorder;
  base::TimeDelta elapsed_time = base::Microseconds(1000);
  ScreenAIService::RecordMetrics(ukm::kInvalidSourceId, &test_recorder,
                                 elapsed_time, false);

  auto entries = test_recorder.GetEntriesByName(
      ukm::builders::Accessibility_ScreenAI::kEntryName);
  ASSERT_EQ(entries.size(), 0u);
}

}  // namespace screen_ai
