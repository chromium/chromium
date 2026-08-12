// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/public/cpp/merge_samples.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace heap_profiling {
namespace {

TEST(MergeSamplesTest, MergeSamples) {
  using Sample = base::SamplingHeapProfiler::Sample;
  Sample sample1(/*size=*/5, /*total=*/100);
  sample1.stack = {reinterpret_cast<void*>(0x1), reinterpret_cast<void*>(0x2)};
  Sample sample2(/*size=*/6, /*total=*/102);
  sample2.stack = {reinterpret_cast<void*>(0x1), reinterpret_cast<void*>(0x3)};
  Sample sample3(/*size=*/7, /*total=*/105);
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

TEST(MergeSamplesTest, MergeSamplesWithResidency) {
  using Sample = base::SamplingHeapProfiler::Sample;
  Sample sample1(/*size=*/10, /*total=*/100);
  sample1.stack = {reinterpret_cast<void*>(0x1), reinterpret_cast<void*>(0x2)};
  sample1.resident_total = 100;
  Sample sample2(/*size=*/10, /*total=*/200);
  sample2.stack = {reinterpret_cast<void*>(0x1), reinterpret_cast<void*>(0x2)};
  sample2.resident_total = 0;
  Sample sample3(/*size=*/10, /*total=*/300);
  sample3.stack = {reinterpret_cast<void*>(0x1), reinterpret_cast<void*>(0x2)};
  sample3.resident_total = 150;

  std::vector<Sample> samples = {sample1, sample2, sample3};

  SampleMap map = heap_profiling::MergeSamples(samples);
  ASSERT_EQ(map.size(), 1u);
  auto it = map.find(sample1);
  ASSERT_TRUE(it != map.end());
  EXPECT_EQ(it->second.count, 60u);  // 100/10 + 200/10 + 300/10 = 60
  EXPECT_EQ(it->second.total, 600u);
  EXPECT_TRUE(it->second.resident_total.has_value());
  // resident_total: 100 + 0 + 150 = 250
  EXPECT_EQ(*it->second.resident_total, 250u);
}

TEST(MergeSamplesTest, MergeSamplesWithNulloptResidency) {
  using Sample = base::SamplingHeapProfiler::Sample;
  Sample sample1(/*size=*/10, /*total=*/100);
  sample1.stack = {reinterpret_cast<void*>(0x1), reinterpret_cast<void*>(0x2)};
  sample1.resident_total = 100;
  Sample sample2(/*size=*/10, /*total=*/200);
  sample2.stack = {reinterpret_cast<void*>(0x1), reinterpret_cast<void*>(0x2)};
  sample2.resident_total = std::nullopt;

  std::vector<Sample> samples = {sample1, sample2};

  SampleMap map = heap_profiling::MergeSamples(samples);
  ASSERT_EQ(map.size(), 1u);
  auto it = map.find(sample1);
  ASSERT_TRUE(it != map.end());
  EXPECT_EQ(it->second.count, 30u);
  EXPECT_EQ(it->second.total, 300u);
  EXPECT_FALSE(it->second.resident_total.has_value());
}

}  // namespace
}  // namespace heap_profiling
