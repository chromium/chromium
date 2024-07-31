// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/core/quantization_utils.h"

#include <algorithm>
#include <cmath>

#include "base/check_op.h"

namespace language_detection {

namespace {

void Nudge(const float min,
           const float max,
           const unsigned int quant_min,
           const unsigned int quant_max,
           float* nudged_min,
           float* nudged_max,
           float* scale) {
  const float quant_min_float = static_cast<float>(quant_min);
  const float quant_max_float = static_cast<float>(quant_max);
  *scale = (max - min) / (quant_max_float - quant_min_float);
  const float zero_point_from_min = quant_min_float - min / *scale;
  uint16_t nudged_zero_point;
  if (zero_point_from_min < quant_min_float) {
    nudged_zero_point = static_cast<uint16_t>(quant_min);
  } else if (zero_point_from_min > quant_max_float) {
    nudged_zero_point = static_cast<uint16_t>(quant_max);
  } else {
    nudged_zero_point = static_cast<uint16_t>(std::round(zero_point_from_min));
  }
  *nudged_min = (quant_min_float - nudged_zero_point) * (*scale);
  *nudged_max = (quant_max_float - nudged_zero_point) * (*scale);
}

}  // namespace

QuantizationParams GetQuantizationParams(float min_val,
                                         float max_val,
                                         int num_bits) {
  DCHECK_GT(num_bits, 1);
  DCHECK_LT(num_bits, 32);
  QuantizationParams params;
  float quant_min = 0.f;
  params.quant_max_uint32 = (1 << num_bits) - 1;
  float quant_max = static_cast<float>(params.quant_max_uint32);
  Nudge(min_val, max_val, quant_min, quant_max, &params.nudged_min,
        &params.nudged_max, &params.nudged_scale);
  return params;
}

uint32_t FloatToQuantized(float x, float min_val, float max_val, int num_bits) {
  QuantizationParams params = GetQuantizationParams(min_val, max_val, num_bits);
  const float inv_nudged_scale = 1.0f / params.nudged_scale;
  float clamped = std::clamp(x, params.nudged_min, params.nudged_max);
  float clamped_shifted = clamped - params.nudged_min;
  return std::min(
      static_cast<uint32_t>(clamped_shifted * inv_nudged_scale + 0.5f),
      params.quant_max_uint32);
}

float QuantizedToFloat(uint32_t x, float min_val, float max_val, int num_bits) {
  const QuantizationParams params =
      GetQuantizationParams(min_val, max_val, num_bits);
  return QuantizedToFloatWithQuantParams(x, params);
}

}  // namespace language_detection
