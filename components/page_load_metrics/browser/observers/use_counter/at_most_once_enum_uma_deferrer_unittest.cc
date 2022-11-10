// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/use_counter/at_most_once_enum_uma_deferrer.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

enum class UmaHistogramTestingEnum {
  kFirst = 0,
  kSecond = 1,
  kThird = 2,
  kMaxValue = kThird,
};

TEST(AtMostOnceEnumUmaDeferrerTest, SingleSample) {
  const char* histogram = "Testing.UMA.HistogramEnumeration";
  base::HistogramTester tester;

  AtMostOnceEnumUmaDeferrer<UmaHistogramTestingEnum> deferrer(histogram);

  // Not recorded before DisableDeferAndFlush.
  deferrer.RecordOrDefer(UmaHistogramTestingEnum::kThird);
  std::vector<base::Bucket> expected_0;
  EXPECT_EQ(expected_0, tester.GetAllSamples(histogram));

  deferrer.RecordOrDefer(UmaHistogramTestingEnum::kFirst);
  EXPECT_EQ(expected_0, tester.GetAllSamples(histogram));

  // Not recorded already RecordOrDefer-ed one.
  deferrer.RecordOrDefer(UmaHistogramTestingEnum::kFirst);
  EXPECT_EQ(expected_0, tester.GetAllSamples(histogram));

  deferrer.DisableDeferAndFlush();

  // Effect of flush
  std::vector<base::Bucket> expected_1 = {
      base::Bucket(UmaHistogramTestingEnum::kFirst, 1),
      base::Bucket(UmaHistogramTestingEnum::kThird, 1),
  };
  EXPECT_EQ(expected_1, tester.GetAllSamples(histogram));

  // Bypass deferring after DisableDeferAndFlush.
  deferrer.RecordOrDefer(UmaHistogramTestingEnum::kSecond);
  std::vector<base::Bucket> expected_2 = {
      base::Bucket(UmaHistogramTestingEnum::kFirst, 1),
      base::Bucket(UmaHistogramTestingEnum::kSecond, 1),
      base::Bucket(UmaHistogramTestingEnum::kThird, 1)};
  EXPECT_EQ(expected_2, tester.GetAllSamples(histogram));

  // Not recorded already RecordOrDefer-ed one.
  deferrer.RecordOrDefer(UmaHistogramTestingEnum::kSecond);
  EXPECT_EQ(expected_2, tester.GetAllSamples(histogram));
}
