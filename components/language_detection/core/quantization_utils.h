// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CORE_QUANTIZATION_UTILS_H_
#define COMPONENTS_LANGUAGE_DETECTION_CORE_QUANTIZATION_UTILS_H_

#include <cmath>
#include <cstdint>

namespace language_detection {

// Converts the given floating point value (`x`) to a quantized value, with
// `num_bits` of precision.
//
// Floating Point `min_val` (and all values below it) map to Quantized 0, and,
// Floating Point `max_val` (and all values above it) map to Quantized
//   (1 << num_bits) - 1.
//
// `num_bits` must be greater than 1, and less than 32.
uint32_t FloatToQuantized(float x, float min_val, float max_val, int num_bits);

// Converts the given quantized value (`x`) to a floating point value, with
// `num_bits` of precision.
//
// Floating Point `min_val` (and all values below it) map to Quantized 0, and,
// Floating Point `max_val` (and all values above it) map to Quantized
//   (1 << num_bits) - 1.
//
// `num_bits` must be greater than 1, and less than 32.
float QuantizedToFloat(uint32_t x, float min_val, float max_val, int num_bits);

// Params required for quantizing / dequantizing a given value.
// These are populated by the `GetQuantizationParams` method, and are
// used internally by the `QuantizedToFloat` and `FloatToQuantized`
// methods, or can be used by the caller to cache these values once, and use
// when invoking the `QuantizedToFloatWithQuantParams` method repeatedly on
// values from the same tensor.
struct QuantizationParams {
  float nudged_scale;
  float nudged_min;
  float nudged_max;
  uint32_t quant_max_uint32;
};

// Compute the params required for quantization / dequantization.
// This is the first part of the `FloatToQuantized` and
// `QuantizedToFloat` methods, and is useful to compute once when there are
// a large number of values from the same tensor, that need to be quantized or
// dequantized.
//
// `num_bits` must be greater than 1, and less than 32.
QuantizationParams GetQuantizationParams(float min_val,
                                         float max_val,
                                         int num_bits);

// Converts the given quantized value (`x`) to a floating point value, using
// the QuantizationParams obtained from the `GetQuantizationParams`
// method.
//
// This is the second part of the `QuantizedToFloat` method, and is
// useful to quickly compute the dequantized value for a large number of
// quantized values, once the scale and min have been computed.
inline float QuantizedToFloatWithQuantParams(uint32_t x,
                                             const QuantizationParams& params) {
  return (x * params.nudged_scale + params.nudged_min);
}

}  // namespace language_detection

#endif  // COMPONENTS_LANGUAGE_DETECTION_CORE_QUANTIZATION_UTILS_H_
