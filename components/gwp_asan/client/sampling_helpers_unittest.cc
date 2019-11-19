// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/sampling_helpers.h"

#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gwp_asan {
namespace internal {

TEST(SamplingHelpers, BasicFunctionality) {
  constexpr size_t kSamplingFrequency = 10;
  constexpr size_t kAllocationsBeforeOom = 15;

  base::HistogramTester histogram_tester;

  CreateOomCallback("Foo", "", kSamplingFrequency).Run(kAllocationsBeforeOom);
  CreateOomCallback("Bar", "utility", kSamplingFrequency)
      .Run(kAllocationsBeforeOom);
  // Ignore unknown process types
  CreateOomCallback("Baz", "UNKNOWN", kSamplingFrequency)
      .Run(kAllocationsBeforeOom);

  base::HistogramTester::CountsMap expected_counts;
  expected_counts["GwpAsan.AllocatorOom.Foo.Browser"] = 1;
  expected_counts["GwpAsan.AllocatorOom.Bar.Utility"] = 1;
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("GwpAsan.AllocatorOom"),
              testing::ContainerEq(expected_counts));

  histogram_tester.ExpectUniqueSample(
      "GwpAsan.AllocatorOom.Foo.Browser",
      kSamplingFrequency * kAllocationsBeforeOom, 1);
  histogram_tester.ExpectUniqueSample(
      "GwpAsan.AllocatorOom.Bar.Utility",
      kSamplingFrequency * kAllocationsBeforeOom, 1);
}

}  // namespace internal
}  // namespace gwp_asan
