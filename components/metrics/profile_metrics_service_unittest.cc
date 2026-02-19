// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/profile_metrics_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

enum class TestEnum {
  kValue1 = 0,
  kMaxValue = kValue1,
};

}  // namespace

class ProfileMetricsServiceTest : public testing::Test {
 public:
  ProfileMetricsServiceTest() = default;
  ~ProfileMetricsServiceTest() override = default;

 protected:
  base::HistogramTester histogram_tester_;
};

TEST_F(ProfileMetricsServiceTest, LogsHistogramsWithSuffix) {
  ProfileMetricsContext context(1);
  ProfileMetricsService service(context);

  service.UmaHistogramEnumeration("Test.Histogram", TestEnum::kValue1);

  histogram_tester_.ExpectUniqueSample("Test.Histogram", TestEnum::kValue1, 1);
  histogram_tester_.ExpectUniqueSample("Test.Histogram.Profile1",
                                       TestEnum::kValue1, 1);
}

TEST_F(ProfileMetricsServiceTest,
       DoesNotLogSuffixedHistogramsIfIndexIsTooHigh) {
  // 20 is higher than kMaxProfileIndexToLog (19).
  ProfileMetricsContext context(20);
  ProfileMetricsService service(context);

  service.UmaHistogramEnumeration("Test.Histogram", TestEnum::kValue1);

  histogram_tester_.ExpectUniqueSample("Test.Histogram", TestEnum::kValue1, 1);
  histogram_tester_.ExpectTotalCount("Test.Histogram.Profile20", 0);
}

TEST_F(ProfileMetricsServiceTest, DoesNotLogSuffixedHistogramsForEmptyContext) {
  // This creates an empty context.
  ProfileMetricsContext context;
  ProfileMetricsService service(context);

  service.UmaHistogramEnumeration("Test.Histogram", TestEnum::kValue1);

  histogram_tester_.ExpectUniqueSample("Test.Histogram", TestEnum::kValue1, 1);
  // No other histograms with the same prefix should be logged.
  EXPECT_EQ(histogram_tester_.GetTotalCountsForPrefix("Test.Histogram").size(),
            1u);
}

}  // namespace metrics
