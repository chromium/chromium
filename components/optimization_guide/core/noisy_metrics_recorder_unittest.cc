// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/noisy_metrics_recorder.h"

#include <cmath>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests that the original metric is returned as it is when the bit flip
// probability is set to 0.
TEST(NoisyMetricsRecorderTest, BasicsNoNoise) {
  const struct TestCase {
    uint32_t original_metric;
    uint8_t bits_count;
  } kTestCases[] = {{0, 1}, {1, 1}, {10, 4}, {10, 31}, {100, 7}, {4242, 31}};

  for (const auto& test_case : kTestCases) {
    NoisyMetricsRecorder recorder;
    uint32_t flipped = recorder.GetNoisyMetric(0.0f, test_case.original_metric,
                                               test_case.bits_count);
    EXPECT_EQ(flipped, test_case.original_metric);
  }
}

// Tests that the original metric is within the expected bounds.
TEST(NoisyMetricsRecorderTest, RandomNoise) {
  const struct TestCase {
    uint32_t original_metric;
    uint8_t bits_count;
  } kTestCases[] = {{0, 1}, {1, 1}, {10, 4}, {10, 31}, {100, 7}, {4242, 31}};

  for (const auto& test_case : kTestCases) {
    NoisyMetricsRecorder recorder;
    uint32_t flipped = recorder.GetNoisyMetric(0.5f, test_case.original_metric,
                                               test_case.bits_count);
    EXPECT_GE(flipped, 0u);
    EXPECT_LE(static_cast<int>(flipped), std::pow(2, test_case.bits_count) - 1);
  }
}

// TestNoisyMetricsRecorder returns deterministic random number based on how
// it's configured.
class TestNoisyMetricsRecorder : public NoisyMetricsRecorder {
 public:
  enum class Mode { kAlternate0And1, kAll0, kAll1 };

  explicit TestNoisyMetricsRecorder(Mode mode) : mode_(mode) {}
  ~TestNoisyMetricsRecorder() = default;

  uint32_t GenerateNumberWithAlternate0And1Bits(size_t count_bits) const {
    int flipped_value = 0;
    int next_bit = 1;
    for (size_t i = 0; i < count_bits; ++i) {
      flipped_value |= (next_bit << i);
      next_bit = next_bit == 0 ? 1 : 0;
    }
    return flipped_value;
  }

 private:
  int GetRandEither0Or1() const override {
    if (mode_ == Mode::kAll0)
      return 0;
    if (mode_ == Mode::kAll1)
      return 1;

    if (last_bit_returned_ == 0) {
      last_bit_returned_ = 1;
    } else {
      last_bit_returned_ = 0;
    }
    return last_bit_returned_;
  }

  mutable int last_bit_returned_ = 0;
  const Mode mode_;
};

TEST(NoisyMetricsRecorderTest, All0s) {
  const struct TestCase {
    uint32_t original_metric;
    uint8_t bits_count;
  } kTestCases[] = {{0, 1}, {1, 1}, {10, 4}, {10, 31}, {100, 7}, {4242, 31}};

  for (const auto& test_case : kTestCases) {
    TestNoisyMetricsRecorder recorder(TestNoisyMetricsRecorder::Mode::kAll0);
    uint32_t flipped = recorder.GetNoisyMetric(1.0f, test_case.original_metric,
                                               test_case.bits_count);
    EXPECT_EQ(flipped, 0u);
  }
}

TEST(NoisyMetricsRecorderTest, All1s) {
  const struct TestCase {
    uint32_t original_metric;
    uint8_t bits_count;
  } kTestCases[] = {{0, 1}, {1, 1}, {10, 4}, {10, 31}, {100, 7}, {4242, 31}};

  for (const auto& test_case : kTestCases) {
    TestNoisyMetricsRecorder recorder(TestNoisyMetricsRecorder::Mode::kAll1);
    uint32_t flipped = recorder.GetNoisyMetric(1.0f, test_case.original_metric,
                                               test_case.bits_count);
    EXPECT_EQ(flipped, std::pow(2, test_case.bits_count) - 1);
  }
}

// Tests that the flipped return value matches the expected value. The noise is
// added deterministically using TestNoisyMetricsRecorder.
TEST(NoisyMetricsRecorderTest, AlternateBits) {
  const struct TestCase {
    uint32_t original_metric;
    uint8_t bits_count;
  } kTestCases[] = {{0, 1}, {1, 1}, {10, 4}, {10, 31}, {100, 7}, {4242, 31}};

  for (const auto& test_case : kTestCases) {
    TestNoisyMetricsRecorder recorder(
        TestNoisyMetricsRecorder::Mode::kAlternate0And1);
    uint32_t flipped = recorder.GetNoisyMetric(1.0f, test_case.original_metric,
                                               test_case.bits_count);
    EXPECT_EQ(flipped, recorder.GenerateNumberWithAlternate0And1Bits(
                           test_case.bits_count));
  }
}
