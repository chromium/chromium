// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/sampling_helpers.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gwp_asan::internal {
namespace {

constexpr size_t kSamplingFrequency = 10;
constexpr size_t kAllocationsBeforeOom = 15;

TEST(SamplingHelpers, DontRecordUnknownProcesses) {
  base::HistogramTester histogram_tester;

  CreateOomCallback("Foo", "UNKNOWN", kSamplingFrequency)
      .Run(kAllocationsBeforeOom);
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix("Security.GwpAsan.AllocatorOom"),
      testing::IsEmpty());
}

TEST(SamplingHelpers, BasicFunctionality) {
  base::HistogramTester histogram_tester;

  CreateOomCallback("Foo", "", kSamplingFrequency).Run(kAllocationsBeforeOom);
  CreateOomCallback("Bar", "utility", kSamplingFrequency)
      .Run(kAllocationsBeforeOom);

  const base::HistogramTester::CountsMap expected_counts = {
      {"Security.GwpAsan.AllocatorOom.Foo.Browser", 1},
      {"Security.GwpAsan.AllocatorOom.Bar.Utility", 1},
  };
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix("Security.GwpAsan.AllocatorOom"),
      testing::ContainerEq(expected_counts));

  histogram_tester.ExpectUniqueSample(
      "Security.GwpAsan.AllocatorOom.Foo.Browser",
      kSamplingFrequency * kAllocationsBeforeOom, 1);
  histogram_tester.ExpectUniqueSample(
      "Security.GwpAsan.AllocatorOom.Bar.Utility",
      kSamplingFrequency * kAllocationsBeforeOom, 1);
}

TEST(SamplingHelpers, RecordClampedMax) {
  base::HistogramTester histogram_tester;

  CreateOomCallback("Foo", "", kSamplingFrequency).Run(size_t{-1u});
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix("Security.GwpAsan.AllocatorOom"),
      testing::SizeIs(1));

  // The sample is too big to log and saturates to the histogram max.
  histogram_tester.ExpectUniqueSample(
      "Security.GwpAsan.AllocatorOom.Foo.Browser", 1'000'000'000u - 1, 1);
}

TEST(SamplingHelpers, RecordNonActivated) {
  base::HistogramTester histogram_tester;

  ReportGwpAsanActivated("Foo", "", false);
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix("Security.GwpAsan.Activated"),
      testing::SizeIs(1));

  histogram_tester.ExpectUniqueSample("Security.GwpAsan.Activated.Foo.Browser",
                                      false, 1);
}

TEST(SamplingHelpers, RecordActivated) {
  base::HistogramTester histogram_tester;

  ReportGwpAsanActivated("Foo", "", true);
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix("Security.GwpAsan.Activated"),
      testing::SizeIs(1));

  histogram_tester.ExpectUniqueSample("Security.GwpAsan.Activated.Foo.Browser",
                                      true, 1);
}

}  // namespace
}  // namespace gwp_asan::internal
