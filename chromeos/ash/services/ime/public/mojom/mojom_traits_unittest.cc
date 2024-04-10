// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/public/mojom/mojom_traits.h"

#include "base/metrics/statistics_recorder.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_host.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::ime {
namespace {

class BucketedHistogramTraitsTest
    : public ::testing::TestWithParam<mojom::HistogramBucketType> {
 private:
  // StatisticsRecorder is a singleton that records histogram parameters and
  // throws an error if the same histogram is created with different parameters.
  // This creates a new StatisticsRecorder in scope for each test to ensure the
  // tests are hermetic.
  std::unique_ptr<base::StatisticsRecorder> statistic_recorder_{
      base::StatisticsRecorder::CreateTemporaryForTesting()};
};

INSTANTIATE_TEST_SUITE_P(
    BucketedHistogramTraitsTestAllBucketTypes,
    BucketedHistogramTraitsTest,
    ::testing::Values(mojom::HistogramBucketType::kExponential,
                      mojom::HistogramBucketType::kLinear));

TEST_P(BucketedHistogramTraitsTest, ValidHistogram) {
  auto histogram = mojom::BucketedHistogram::New(
      "Untrusted.Metric", /*bucket_type=*/GetParam(), /*minimum=*/1,
      /*maximum=*/10, /*bucket_count=*/3);

  base::Histogram* output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::BucketedHistogram>(
      histogram, output));

  EXPECT_STREQ(output->histogram_name(), "Untrusted.Metric");
  EXPECT_EQ(output->declared_min(), 1);
  EXPECT_EQ(output->declared_max(), 10);
  EXPECT_EQ(output->bucket_count(), 3u);
}

TEST_P(BucketedHistogramTraitsTest, ValidHistogramRoundTrip) {
  base::Histogram* histogram = nullptr;
  switch (GetParam()) {
    case mojom::HistogramBucketType::kExponential:
      histogram = static_cast<base::Histogram*>(base::Histogram::FactoryGet(
          "Untrusted.Metric", /*minimum=*/1, /*maximum=*/10, /*bucket_count=*/3,
          base::HistogramBase::kUmaTargetedHistogramFlag));
      break;
    case mojom::HistogramBucketType::kLinear:
      histogram =
          static_cast<base::Histogram*>(base::LinearHistogram::FactoryGet(
              "Untrusted.Metric", /*minimum=*/1, /*maximum=*/10,
              /*bucket_count=*/3,
              base::HistogramBase::kUmaTargetedHistogramFlag));
      break;
  }

  base::Histogram* output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::BucketedHistogram>(
      histogram, output));

  // It should produce the exact same histogram instance, so do a pointer
  // comparison here.
  EXPECT_EQ(histogram, output);
}

TEST_P(BucketedHistogramTraitsTest, InvalidHistogramName) {
  auto histogram = mojom::BucketedHistogram::New(
      "UntrustedMetric", /*bucket_type=*/GetParam(), /*minimum=*/1,
      /*maximum=*/10, /*bucket_count=*/3);

  base::Histogram* output;
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::BucketedHistogram>(
      histogram, output));
}

TEST_P(BucketedHistogramTraitsTest, InvalidHistogramMinimumIsZero) {
  auto histogram = mojom::BucketedHistogram::New(
      "Untrusted.Metric", /*bucket_type=*/GetParam(), /*minimum=*/0,
      /*maximum=*/10, /*bucket_count=*/3);

  base::Histogram* output;
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::BucketedHistogram>(
      histogram, output));
}

TEST_P(BucketedHistogramTraitsTest, InvalidHistogramMaximumTooLow) {
  auto histogram = mojom::BucketedHistogram::New(
      "Untrusted.Metric", /*bucket_type=*/GetParam(), /*minimum=*/5,
      /*maximum=*/5, /*bucket_count=*/3);

  base::Histogram* output;
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::BucketedHistogram>(
      histogram, output));
}

TEST_P(BucketedHistogramTraitsTest, InvalidHistogramBucketCountTooLow) {
  auto histogram = mojom::BucketedHistogram::New(
      "Untrusted.Metric", /*bucket_type=*/GetParam(), /*minimum=*/1,
      /*maximum=*/10, /*bucket_count=*/2);

  base::Histogram* output;
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::BucketedHistogram>(
      histogram, output));
}

TEST_P(BucketedHistogramTraitsTest, InvalidHistogramBucketCountTooHigh) {
  auto histogram = mojom::BucketedHistogram::New(
      "Untrusted.Metric", /*bucket_type=*/GetParam(), /*minimum=*/1,
      /*maximum=*/10, /*bucket_count=*/12);

  base::Histogram* output;
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::BucketedHistogram>(
      histogram, output));
}

TEST_P(BucketedHistogramTraitsTest, InvalidHistogramNull) {
  mojom::BucketedHistogramPtr histogram = nullptr;

  base::Histogram* output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::BucketedHistogram>(
      histogram, output));
  EXPECT_EQ(output, nullptr);
}

}  // namespace
}  // namespace ash::ime
