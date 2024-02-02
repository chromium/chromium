// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/cast_runtime_histogram_flattener.h"

#include <memory>
#include <string_view>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "chromecast/cast_core/runtime/browser/cast_runtime_metrics_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace {

const cast::metrics::Histogram* LookupHistogram(
    std::string_view name,
    const std::vector<cast::metrics::Histogram>& deltas) {
  for (const auto& histogram : deltas) {
    if (histogram.name() == name) {
      return &histogram;
    }
  }
  return nullptr;
}

}  // namespace

class CastRuntimeHistogramFlattenerTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Create the temporary stats recorder to get a clean state
    // for each test.
    statistics_recorder_ =
        base::StatisticsRecorder::CreateTemporaryForTesting();

    // Ensure existing deltas from previous tests in the process are cleared.
    ASSERT_TRUE(GetHistogramDeltas().empty());
  }

  void TearDown() override {
    // Destroy the temporary stats recorder, implicitly restoring
    // the previous global stats recorder.
    statistics_recorder_.reset();
  }

  // Stand-in `StatisticsRecorder` for this test suite, to ensure
  // each test is isolated from everything else in the process that
  // could emit histograms.
  std::unique_ptr<base::StatisticsRecorder> statistics_recorder_;
};

TEST_F(CastRuntimeHistogramFlattenerTest, Empty) {
  EXPECT_TRUE(GetHistogramDeltas().empty());
}

TEST_F(CastRuntimeHistogramFlattenerTest, EmptyAfterPoll) {
  constexpr char kHistName[] = "Foo.Bar.SomeCount";
  UMA_HISTOGRAM_COUNTS_1M(kHistName, 13);
  auto deltas = GetHistogramDeltas();
  EXPECT_EQ(1, GetCount(13, LookupHistogram(kHistName, deltas)));

  EXPECT_TRUE(GetHistogramDeltas().empty());
}

TEST_F(CastRuntimeHistogramFlattenerTest, Boolean) {
  constexpr char kBooleanName[] = "Foo.Bar.SomeBool";
  UMA_HISTOGRAM_BOOLEAN(kBooleanName, true);
  UMA_HISTOGRAM_BOOLEAN(kBooleanName, true);
  UMA_HISTOGRAM_BOOLEAN(kBooleanName, false);

  auto deltas = GetHistogramDeltas();
  EXPECT_EQ(GetCount(0, LookupHistogram(kBooleanName, deltas)), 1);
  EXPECT_EQ(GetCount(1, LookupHistogram(kBooleanName, deltas)), 2);
}

TEST_F(CastRuntimeHistogramFlattenerTest, Linear) {
  constexpr char kLinearName[] = "Foo.Bar.SomeLinear";
  constexpr int kHistogramLimit = 100;
  for (int i = 0; i < kHistogramLimit; ++i) {
    UMA_HISTOGRAM_EXACT_LINEAR(kLinearName, i, kHistogramLimit);
  }

  auto deltas = GetHistogramDeltas();

  const cast::metrics::Histogram* histogram =
      LookupHistogram(kLinearName, deltas);
  for (int i = 0; i < kHistogramLimit; ++i) {
    EXPECT_EQ(GetCount(i, histogram), 1);
  }
}

TEST_F(CastRuntimeHistogramFlattenerTest, Percentage) {
  constexpr char kPercentageName[] = "Foo.Bar.SomePercentage";
  for (int i = 0; i <= 100; ++i) {
    for (int j = 0; j < 2 * i; ++j) {
      UMA_HISTOGRAM_PERCENTAGE(kPercentageName, i);
    }
  }

  auto deltas = GetHistogramDeltas();

  const cast::metrics::Histogram* histogram =
      LookupHistogram(kPercentageName, deltas);
  for (int i = 0; i <= 100; ++i) {
    EXPECT_EQ(GetCount(i, histogram), 2 * i);
  }
}

TEST_F(CastRuntimeHistogramFlattenerTest, Enumeration) {
  constexpr char kEnumName[] = "Foo.Bar.SomeEnum";
  enum Category {
    kAlpha,
    kBeta,
    kGamma,
    kDelta,
    kCategoryMax = kDelta,
  };

  UMA_HISTOGRAM_ENUMERATION(kEnumName, kAlpha, kCategoryMax);
  UMA_HISTOGRAM_ENUMERATION(kEnumName, kGamma, kCategoryMax);
  UMA_HISTOGRAM_ENUMERATION(kEnumName, kDelta, kCategoryMax);
  UMA_HISTOGRAM_ENUMERATION(kEnumName, kGamma, kCategoryMax);

  auto deltas = GetHistogramDeltas();

  const cast::metrics::Histogram* histogram =
      LookupHistogram(kEnumName, deltas);
  EXPECT_EQ(1, GetCount(kAlpha, histogram));
  EXPECT_EQ(0, GetCount(kBeta, histogram));
  EXPECT_EQ(2, GetCount(kGamma, histogram));
  EXPECT_EQ(1, GetCount(kDelta, histogram));
}

TEST_F(CastRuntimeHistogramFlattenerTest, MultipleHistograms) {
  constexpr char kHistName1[] = "Foo.Bar.SomeCount";
  constexpr char kHistName2[] = "Foo.Bar.SomeOtherCount";

  UMA_HISTOGRAM_COUNTS_1M(kHistName1, 13);
  UMA_HISTOGRAM_COUNTS_1M(kHistName2, 1337);
  UMA_HISTOGRAM_COUNTS_1M(kHistName2, 1337);

  auto deltas = GetHistogramDeltas();
  EXPECT_EQ(1, GetCount(13, LookupHistogram(kHistName1, deltas)));
  EXPECT_EQ(2, GetCount(1337, LookupHistogram(kHistName2, deltas)));
}

TEST_F(CastRuntimeHistogramFlattenerTest, SeparateBuckets) {
  constexpr char kHistName[] = "Foo.Bar.SomeCount";

  UMA_HISTOGRAM_COUNTS_1M(kHistName, 13);
  UMA_HISTOGRAM_COUNTS_1M(kHistName, 13000);

  auto deltas = GetHistogramDeltas();
  EXPECT_EQ(1, GetCount(13, LookupHistogram(kHistName, deltas)));
  EXPECT_EQ(1, GetCount(13000, LookupHistogram(kHistName, deltas)));

  UMA_HISTOGRAM_COUNTS_1M(kHistName, 13000);
  UMA_HISTOGRAM_COUNTS_1M(kHistName, 13000);
  deltas = GetHistogramDeltas();
  EXPECT_EQ(2, GetCount(13000, LookupHistogram(kHistName, deltas)));
}

}  // namespace chromecast
