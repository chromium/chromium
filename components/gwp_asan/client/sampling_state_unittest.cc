// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/sampling_state.h"

#include <cstddef>

#include "testing/gtest/include/gtest/gtest.h"

namespace gwp_asan::internal {
namespace {

using Sampler = SmartSamplingState<MALLOC>;

constexpr size_t kSamplingFrequency = 1500;
constexpr size_t kEveryAllocationSamplingFrequency = 1;
constexpr size_t kAllocSize = 64;
constexpr size_t kToleranceDivisor = 10;
constexpr bool kEnableSmartSampling = true;
constexpr bool kDisableSmartSampling = false;
constexpr size_t kSmartSamplingRateMultiplier = 8;

constexpr size_t kTokenA = 1;
constexpr size_t kTokenB = 2;

class SamplingStateTest : public testing::Test {
 protected:
  void SetUp() override { sampler_.ResetForTesting(); }

  Sampler sampler_;
};

TEST_F(SamplingStateTest, SmartSamplingDisabledSamplesEveryCandidate) {
  sampler_.Init(kEveryAllocationSamplingFrequency, kDisableSmartSampling,
                kSmartSamplingRateMultiplier);

  size_t sampled = 0;
  for (size_t i = 0; i < 16; i++) {
    if (sampler_.Sample(kAllocSize, kTokenA)) {
      sampled++;
    }
  }
  EXPECT_EQ(sampled, 16U);
}

TEST_F(SamplingStateTest, SuppressesRepeatedTypes) {
  sampler_.Init(kEveryAllocationSamplingFrequency, kEnableSmartSampling,
                kSmartSamplingRateMultiplier);

  EXPECT_TRUE(sampler_.Sample(kAllocSize, kTokenA));

  // Rejected first at the baseline, and then within the reserve as a seen type.
  constexpr size_t kNumSuppressed =
      (kSmartSamplingRateMultiplier - 1) +
      kSmartSamplingReservedForUnseenTypes * kSmartSamplingRateMultiplier;
  for (size_t i = 0; i < kNumSuppressed; i++) {
    EXPECT_FALSE(sampler_.Sample(kAllocSize, kTokenA));
  }
  EXPECT_TRUE(sampler_.Sample(kAllocSize, kTokenA));

  // Unseen types are sampled even when repeated types are suppressed.
  EXPECT_FALSE(sampler_.Sample(kAllocSize, kTokenA));
  EXPECT_TRUE(sampler_.Sample(kAllocSize, kTokenB));
}

TEST_F(SamplingStateTest, RateMultiplierIsConfigurable) {
  constexpr size_t kCustomRateMultiplier = 3;
  sampler_.Init(kEveryAllocationSamplingFrequency, kEnableSmartSampling,
                kCustomRateMultiplier);

  EXPECT_TRUE(sampler_.Sample(kAllocSize, kTokenA));

  // Rejected first at the baseline, and then within the reserve as a seen type.
  constexpr size_t kNumSuppressed =
      (kCustomRateMultiplier - 1) +
      kSmartSamplingReservedForUnseenTypes * kCustomRateMultiplier;
  for (size_t i = 0; i < kNumSuppressed; i++) {
    EXPECT_FALSE(sampler_.Sample(kAllocSize, kTokenA));
  }
  EXPECT_TRUE(sampler_.Sample(kAllocSize, kTokenA));
}

TEST_F(SamplingStateTest, TypeIsSampledAgainAfterDecay) {
  sampler_.Init(kEveryAllocationSamplingFrequency, kEnableSmartSampling,
                kSmartSamplingRateMultiplier);

  ASSERT_TRUE(sampler_.Sample(kAllocSize, kTokenA));
  ASSERT_FALSE(sampler_.Sample(kAllocSize, kTokenA));

  // Fill the ring buffer with `kTokenB` to evict `kTokenA`.
  size_t accepted_b = 0;
  while (accepted_b < kSmartSamplingDecaySteps) {
    if (sampler_.Sample(kAllocSize, kTokenB)) {
      accepted_b++;
    }
  }

  // `kTokenB` is suppressed, while evicted `kTokenA` is sampled as a new type.
  ASSERT_FALSE(sampler_.Sample(kAllocSize, kTokenB));
  EXPECT_TRUE(sampler_.Sample(kAllocSize, kTokenA));
}

TEST_F(SamplingStateTest, AllocationsOutsideTheSizeRangeAreNotSampled) {
  sampler_.Init(kEveryAllocationSamplingFrequency, kEnableSmartSampling,
                kSmartSamplingRateMultiplier);
  sampler_.SetSampleSizeRestriction(/*sampling_min_size=*/8,
                                    /*sampling_max_size=*/16);

  EXPECT_FALSE(sampler_.Sample(/*alloc_size=*/4, kTokenA));
  EXPECT_FALSE(sampler_.Sample(/*alloc_size=*/32, kTokenA));
  EXPECT_TRUE(sampler_.Sample(/*alloc_size=*/8, kTokenA));
}

TEST_F(SamplingStateTest, Unseen0PercentRepeated100Percent) {
  constexpr size_t kNumAllocations = 1500000;

  sampler_.Init(kSamplingFrequency, kEnableSmartSampling,
                kSmartSamplingRateMultiplier);
  size_t sampled = 0;
  for (size_t i = 0; i < kNumAllocations; i++) {
    if (sampler_.Sample(kAllocSize, kTokenA)) {
      sampled++;
    }
  }

  // Repeated types are throttled back to the baseline sampling frequency.
  constexpr size_t kExpectedSamples = kNumAllocations / kSamplingFrequency;
  EXPECT_NEAR(sampled, kExpectedSamples, kExpectedSamples / kToleranceDivisor);
}

TEST_F(SamplingStateTest, Unseen10PercentRepeated90Percent) {
  constexpr size_t kNumAllocations = 3000000;
  constexpr size_t kUnseenStep = 10;

  sampler_.Init(kSamplingFrequency, kEnableSmartSampling,
                kSmartSamplingRateMultiplier);
  size_t sampled_repeated = 0;
  size_t sampled_unseen = 0;
  for (size_t i = 0; i < kNumAllocations; i++) {
    if (i % kUnseenStep == 0) {
      if (sampler_.Sample(kAllocSize, kTokenB + i)) {
        sampled_unseen++;
      }
    } else {
      if (sampler_.Sample(kAllocSize, kTokenA)) {
        sampled_repeated++;
      }
    }
  }

  // All unseen candidates (10%) are accepted, and repeated candidates are
  // accepted until total acceptance reaches 1 / kSmartSamplingRateMultiplier
  // (12.5%).
  constexpr size_t kExpectedTotalSamples = kNumAllocations / kSamplingFrequency;
  constexpr size_t kExpectedUnseenSamples =
      (kExpectedTotalSamples / kUnseenStep) * kSmartSamplingRateMultiplier;
  constexpr size_t kExpectedRepeatedSamples =
      kExpectedTotalSamples - kExpectedUnseenSamples;
  EXPECT_NEAR(sampled_unseen + sampled_repeated, kExpectedTotalSamples,
              kExpectedTotalSamples / kToleranceDivisor);
  EXPECT_NEAR(sampled_unseen, kExpectedUnseenSamples,
              kExpectedTotalSamples / kToleranceDivisor);
  EXPECT_NEAR(sampled_repeated, kExpectedRepeatedSamples,
              kExpectedTotalSamples / kToleranceDivisor);
}

TEST_F(SamplingStateTest, Unseen50PercentRepeated50Percent) {
  constexpr size_t kNumAllocations = 1500000;
  constexpr size_t kUnseenStep = 2;

  sampler_.Init(kSamplingFrequency, kEnableSmartSampling,
                kSmartSamplingRateMultiplier);
  size_t sampled_unseen = 0;
  for (size_t i = 0; i < kNumAllocations; i++) {
    if (i % kUnseenStep == 0) {
      if (sampler_.Sample(kAllocSize, kTokenB + i)) {
        sampled_unseen++;
      }
    } else {
      sampler_.Sample(kAllocSize, kTokenA);
    }
  }

  // Unseen candidates alone exceed the baseline, so the samples are capped at
  // the baseline.
  constexpr size_t kExpectedUnseenSamples =
      kNumAllocations / kSamplingFrequency;
  EXPECT_NEAR(sampled_unseen, kExpectedUnseenSamples,
              kExpectedUnseenSamples / kToleranceDivisor);
}

TEST_F(SamplingStateTest, Unseen100PercentRepeated0Percent) {
  constexpr size_t kNumAllocations = 1500000;

  sampler_.Init(kSamplingFrequency, kEnableSmartSampling,
                kSmartSamplingRateMultiplier);
  size_t sampled = 0;
  for (size_t i = 0; i < kNumAllocations; i++) {
    if (sampler_.Sample(kAllocSize, i)) {
      sampled++;
    }
  }

  // Capped at the baseline even if all types are unseen.
  constexpr size_t kExpectedSamples = kNumAllocations / kSamplingFrequency;
  EXPECT_NEAR(sampled, kExpectedSamples, kExpectedSamples / kToleranceDivisor);
}

TEST_F(SamplingStateTest, RateMultiplierIsNotTruncated) {
  // 12 / 8 would be truncated to 1 with integer division.
  constexpr size_t kSmallSamplingFrequency = 12;
  constexpr size_t kNumAllocations = 120000;

  sampler_.Init(kSmallSamplingFrequency, kEnableSmartSampling,
                kSmartSamplingRateMultiplier);
  size_t sampled = 0;
  for (size_t i = 0; i < kNumAllocations; i++) {
    if (sampler_.Sample(kAllocSize, i)) {
      sampled++;
    }
  }

  constexpr size_t kExpectedSamples = kNumAllocations / kSmallSamplingFrequency;
  EXPECT_NEAR(sampled, kExpectedSamples, kExpectedSamples / kToleranceDivisor);
}

}  // namespace
}  // namespace gwp_asan::internal
