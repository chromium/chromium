// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduling_metrics/total_duration_metric_reporter.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace scheduling_metrics {

TEST(TotalDurationMetricReporterTest, Test) {
  base::HistogramTester histogram_tester;

  TotalDurationMetricReporter metric_reporter("TestHistogram.Positive",
                                              "TestHistogram.Negative");

  metric_reporter.RecordAdditionalDuration(base::Seconds(1));
  metric_reporter.RecordAdditionalDuration(base::Seconds(2));
  metric_reporter.RecordAdditionalDuration(base::Seconds(5));

  metric_reporter.Reset();

  metric_reporter.RecordAdditionalDuration(base::Seconds(10));

  std::map<base::Histogram::Sample, base::HistogramBase::Count> result;

  for (const base::Bucket& bucket :
       histogram_tester.GetAllSamples("TestHistogram.Positive")) {
    result[bucket.min] += bucket.count;
  }
  for (const base::Bucket& bucket :
       histogram_tester.GetAllSamples("TestHistogram.Negative")) {
    result[bucket.min] -= bucket.count;
  }

  // 1 and 3 correspond to "reverted" values.
  EXPECT_THAT(result, testing::UnorderedElementsAre(
                          std::make_pair(1, 0), std::make_pair(3, 0),
                          std::make_pair(8, 1), std::make_pair(10, 1)));
}

}  // namespace scheduling_metrics
