// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fuchsia_legacymetrics/legacymetrics_histogram_flattener.h"

#include <string_view>

#include "base/metrics/histogram_macros.h"
#include "testing/gtest/include/gtest/gtest.h"

using fuchsia::legacymetrics::Histogram;
using fuchsia::legacymetrics::HistogramBucket;

namespace fuchsia_legacymetrics {
namespace {

constexpr char kHistogramCount1M[] = "Foo.Bar";

int64_t GetCount(int64_t value, const std::vector<HistogramBucket>& buckets) {
  for (const HistogramBucket& bucket : buckets) {
    if (value >= bucket.min && value < bucket.max)
      return bucket.count;
  }

  return 0;
}

const fuchsia::legacymetrics::Histogram* LookupHistogram(
    std::string_view name,
    const std::vector<Histogram>& histograms) {
  for (const auto& histogram : histograms) {
    if (histogram.name() == name)
      return &histogram;
  }
  return nullptr;
}

class LegacyMetricsHistogramFlattenerTest : public testing::Test {
 public:
  LegacyMetricsHistogramFlattenerTest() = default;
  ~LegacyMetricsHistogramFlattenerTest() override = default;

  void SetUp() override {
    // Flush all histogram deltas from prior tests executed in this process.
    GetLegacyMetricsDeltas();
  }
};

TEST_F(LegacyMetricsHistogramFlattenerTest, NoHistogramData) {
  EXPECT_TRUE(GetLegacyMetricsDeltas().empty());
}

TEST_F(LegacyMetricsHistogramFlattenerTest, Boolean) {
  constexpr char kBooleanHistogram[] = "Foo.Bar.Boolean";
  UMA_HISTOGRAM_BOOLEAN(kBooleanHistogram, true);
  UMA_HISTOGRAM_BOOLEAN(kBooleanHistogram, true);
  UMA_HISTOGRAM_BOOLEAN(kBooleanHistogram, false);

  auto deltas = GetLegacyMetricsDeltas();
  EXPECT_EQ(1,
            GetCount(0, LookupHistogram(kBooleanHistogram, deltas)->buckets()));
  EXPECT_EQ(2,
            GetCount(1, LookupHistogram(kBooleanHistogram, deltas)->buckets()));
}

TEST_F(LegacyMetricsHistogramFlattenerTest, Linear) {
  constexpr char kLinearHistogram[] = "Foo.Bar.Linear";

  for (int i = 0; i < 200; ++i) {
    UMA_HISTOGRAM_EXACT_LINEAR(kLinearHistogram, i, 200);
  }

  auto deltas = GetLegacyMetricsDeltas();

  for (int i = 0; i < 200; ++i) {
    EXPECT_EQ(
        1, GetCount(i, LookupHistogram(kLinearHistogram, deltas)->buckets()));
  }
}

TEST_F(LegacyMetricsHistogramFlattenerTest, Percentage) {
  constexpr char kPercentageHistogram[] = "Foo.Bar.Percentage";

  for (int i = 0; i <= 100; ++i) {
    for (int j = 0; j < i; ++j)
      UMA_HISTOGRAM_PERCENTAGE(kPercentageHistogram, i);
  }

  auto deltas = GetLegacyMetricsDeltas();

  for (int i = 0; i <= 100; ++i) {
    EXPECT_EQ(
        i,
        GetCount(i, LookupHistogram(kPercentageHistogram, deltas)->buckets()));
  }
}

enum Fruit {
  APPLE,
  BANANA,
  PEAR,
  FRUIT_MAX = PEAR,
};

TEST_F(LegacyMetricsHistogramFlattenerTest, Enumeration) {
  constexpr char kEnumHistogram[] = "Foo.Bar.Enumeration";

  UMA_HISTOGRAM_ENUMERATION(kEnumHistogram, APPLE, FRUIT_MAX);
  UMA_HISTOGRAM_ENUMERATION(kEnumHistogram, BANANA, FRUIT_MAX);
  UMA_HISTOGRAM_ENUMERATION(kEnumHistogram, BANANA, FRUIT_MAX);
  UMA_HISTOGRAM_ENUMERATION(kEnumHistogram, BANANA, FRUIT_MAX);
  UMA_HISTOGRAM_ENUMERATION(kEnumHistogram, PEAR, FRUIT_MAX);

  auto deltas = GetLegacyMetricsDeltas();

  EXPECT_EQ(
      1, GetCount(APPLE, LookupHistogram(kEnumHistogram, deltas)->buckets()));
  EXPECT_EQ(
      3, GetCount(BANANA, LookupHistogram(kEnumHistogram, deltas)->buckets()));
  EXPECT_EQ(1,
            GetCount(PEAR, LookupHistogram(kEnumHistogram, deltas)->buckets()));
}

TEST_F(LegacyMetricsHistogramFlattenerTest, NoNewData) {
  UMA_HISTOGRAM_COUNTS_1M(kHistogramCount1M, 20);

  auto deltas = GetLegacyMetricsDeltas();
  EXPECT_EQ(
      1, GetCount(20, LookupHistogram(kHistogramCount1M, deltas)->buckets()));

  // No changes to a histogram means we should not be seeing it in the deltas.
  deltas = GetLegacyMetricsDeltas();
  EXPECT_TRUE(deltas.empty());
}

TEST_F(LegacyMetricsHistogramFlattenerTest, MultipleHistograms) {
  constexpr char kAnotherHistogram[] = "Foo.Bar2";

  UMA_HISTOGRAM_COUNTS_1M(kHistogramCount1M, 20);
  UMA_HISTOGRAM_COUNTS_1M(kAnotherHistogram, 1000);
  UMA_HISTOGRAM_COUNTS_1M(kAnotherHistogram, 1000);

  auto deltas = GetLegacyMetricsDeltas();
  EXPECT_EQ(
      1, GetCount(20, LookupHistogram(kHistogramCount1M, deltas)->buckets()));
  EXPECT_EQ(
      2, GetCount(1000, LookupHistogram(kAnotherHistogram, deltas)->buckets()));
}

TEST_F(LegacyMetricsHistogramFlattenerTest, MultipleBuckets) {
  UMA_HISTOGRAM_COUNTS_1M(kHistogramCount1M, 20);
  UMA_HISTOGRAM_COUNTS_1M(kHistogramCount1M, 1000);

  auto deltas = GetLegacyMetricsDeltas();
  EXPECT_EQ(
      1, GetCount(20, LookupHistogram(kHistogramCount1M, deltas)->buckets()));
  EXPECT_EQ(
      1, GetCount(1000, LookupHistogram(kHistogramCount1M, deltas)->buckets()));

  UMA_HISTOGRAM_COUNTS_1M(kHistogramCount1M, 1000);
  UMA_HISTOGRAM_COUNTS_1M(kHistogramCount1M, 1000);
  deltas = GetLegacyMetricsDeltas();
  EXPECT_EQ(
      2, GetCount(1000, LookupHistogram(kHistogramCount1M, deltas)->buckets()));
}

}  // namespace
}  // namespace fuchsia_legacymetrics
