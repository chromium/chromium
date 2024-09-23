// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/accessibility/android/page_zoom_metrics.h"

#include <string_view>

#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_ui {

TEST(PageZoomMetricsTest, PageZoomUkmExactValue) {
  ukm::TestUkmRecorder test_recorder;
  ukm::SourceId mock_source_id = test_recorder.GetNewSourceID();

  PageZoomMetrics::LogZoomLevelUKMHelper(mock_source_id, 0.75, &test_recorder);

  std::string_view expectedMetricName = "SliderZoomValue";

  auto entries = test_recorder.GetEntriesByName(
      ukm::builders::Accessibility_PageZoom::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  test_recorder.ExpectEntryMetric(entries.front(), expectedMetricName, 75);
}

TEST(PageZoomMetricsTest, PageZoomUkmBucket) {
  ukm::TestUkmRecorder test_recorder;
  ukm::SourceId mock_source_id = test_recorder.GetNewSourceID();

  PageZoomMetrics::LogZoomLevelUKMHelper(mock_source_id, 0.78, &test_recorder);

  std::string_view expectedMetricName = "SliderZoomValue";

  auto entries = test_recorder.GetEntriesByName(
      ukm::builders::Accessibility_PageZoom::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  test_recorder.ExpectEntryMetric(entries.front(), expectedMetricName, 75);
}

}  // namespace browser_ui
