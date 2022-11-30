// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/public/cpp/merge_samples.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace heap_profiling {
namespace {

TEST(MergeSamplesTest, MergeSamples) {
  using Sample = base::SamplingHeapProfiler::Sample;
  Sample sample1(/*size=*/5, /*total=*/100, /*ordinal=*/1);
  sample1.stack = {reinterpret_cast<void*>(0x1), reinterpret_cast<void*>(0x2)};
  Sample sample2(/*size=*/6, /*total=*/102, /*ordinal=*/2);
  sample2.stack = {reinterpret_cast<void*>(0x1), reinterpret_cast<void*>(0x3)};
  Sample sample3(/*size=*/7, /*total=*/105, /*ordinal=*/3);
  sample3.stack = {reinterpret_cast<void*>(0x1), reinterpret_cast<void*>(0x2)};

  std::vector<Sample> samples = {sample1, sample2, sample3};

  SampleMap map = heap_profiling::MergeSamples(samples);
  ASSERT_EQ(map.size(), 2u);
  auto it = map.find(sample1);
  ASSERT_TRUE(it != map.end());
  EXPECT_EQ(it->second.count, 35u);  // 100 / 5 + 105 / 7  = 35
  EXPECT_EQ(it->second.total, 205u);
  it = map.find(sample2);
  ASSERT_TRUE(it != map.end());
  EXPECT_EQ(it->second.count, 17u);  // 102 / 6 = 17
  EXPECT_EQ(it->second.total, 102u);
}

}  // namespace
}  // namespace heap_profiling
