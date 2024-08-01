// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/core/quantization_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace language_detection {

TEST(QuantizationUtilsTest, FloatToQuantized) {
  const float min_val = 0.f;
  const float max_val = 4.f;
  const int num_bits = 16;

  // 0.f should map to quantized 0.
  EXPECT_EQ(0u, FloatToQuantized(/*x =*/0.f, min_val, max_val, num_bits));

  // A value below the floating point minimum should also map to quantized 0.
  EXPECT_EQ(0u, FloatToQuantized(/*x =*/-1.f, min_val, max_val, num_bits));

  // 2.f which lies in the middle of the range should map to the middle of the
  // quantized range.
  EXPECT_EQ((1u << (num_bits - 1)),
            FloatToQuantized(/*x=*/2.f, min_val, max_val, num_bits));

  // 4.f should map to the highest quantized value.
  EXPECT_EQ(((1u << num_bits) - 1),
            FloatToQuantized(/*x=*/4.f, min_val, max_val, num_bits));

  // A value above the floating point maximum should also map to the highest
  // quantized value.
  EXPECT_EQ(((1u << num_bits) - 1),
            FloatToQuantized(/*x=*/5.f, min_val, max_val, num_bits));
}

class QuantizedToFloatParamTest : public testing::Test,
                                  public testing::WithParamInterface<bool> {};

// A single parameterized test which validates that the dequantization happens
// correctly, with both single stage and two stage dequantization.
TEST_P(QuantizedToFloatParamTest, QuantizedToFloat) {
  const bool two_stage_dequantization = QuantizedToFloatParamTest::GetParam();

  const float min_val = 0.0;
  const float max_val = 4.0;
  const float max_abs_err = (max_val - min_val) * 1e-5;
  const int num_bits = 16;

  auto dequantize = [two_stage_dequantization](float x, float min_val,
                                               float max_val,
                                               int num_bits) -> float {
    if (two_stage_dequantization) {
      QuantizationParams params =
          GetQuantizationParams(min_val, max_val, num_bits);
      return QuantizedToFloatWithQuantParams(x, params);
    } else {
      return QuantizedToFloat(x, min_val, max_val, num_bits);
    }
  };

  // Quantized 0 should map to `min_vals`.
  EXPECT_EQ(min_val, dequantize(/*x =*/0, min_val, max_val, num_bits));

  // Middle of the quantized range should map to the middle of the floating
  // point range.
  EXPECT_NEAR(
      (max_val + min_val) / 2.0,
      dequantize(/*x=*/(1 << (num_bits - 1)), min_val, max_val, num_bits),
      max_abs_err);

  // The highest quantized value should map to `max_val`.
  EXPECT_NEAR(
      max_val,
      dequantize(/*x=*/((1 << num_bits) - 1), min_val, max_val, num_bits),
      max_abs_err);

  // A value above the quantized maximum should also map to the highest
  // floating point value.
  EXPECT_NEAR(
      max_val,
      dequantize(/*x=*/((1 << num_bits) - 1), min_val, max_val, num_bits),
      max_abs_err);
}

INSTANTIATE_TEST_SUITE_P(QuantizedToFloatParamTests,
                         QuantizedToFloatParamTest,
                         testing::Bool());

}  // namespace language_detection
