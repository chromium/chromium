// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <cstdint>
#include <limits>

namespace cbor {

// This is adapted from the example in RFC8949 appendix D.
double DecodeHalfPrecisionFloat(uint16_t value) {
  uint16_t half = value;
  uint16_t exp = (half >> 10) & 0x1f;  // 5 bit exponent
  uint16_t mant = half & 0x3ff;        // 10 bit mantissa
  double val;

  if (exp == 0) {
    // Handle denormalized case
    val = std::ldexp(mant, -24);
  } else if (exp != 31) {
    // Handle normal case
    val = std::ldexp(mant + 1024, exp - 25);
  } else {
    // Handle special cases.
    if (mant == 0) {
      val = std::numeric_limits<double>::infinity();
    } else {
      val = std::numeric_limits<double>::quiet_NaN();
    }
  }
  // Handle sign bit.
  return half & 0x8000 ? -val : val;
}

uint16_t EncodeHalfPrecisionFloat(double input) {
  int exp = 0;
  uint16_t mantissa = 0;

  double abs_value = std::abs(input);
  // First handle special cases.
  if (!std::isfinite(input)) {
    // Set all-ones for exponent as a flag.
    exp = 0x1F;
    if (std::isnan(input)) {
      // A NaN value has a nonzero mantissa
      mantissa = 1;
    }  // else - infinity
  } else if (abs_value == 0.0) {
    // This is a special case because it's not handled well with frexp().
    exp = 0;
    mantissa = 0;
  } else {
    int normal_exp;
    double normal_value = std::frexp(abs_value, &normal_exp);

    // frexp returns numbers in the range [0.5,1) instead of the usual [1,2)
    // range used for the floating point mantissa so we need to offset the
    // exponent by one through the below.

    // Half-precision uses an offset of 15 for the exponent. We already have 1
    // from the frexp so we just increase it by 14.
    exp = 14 + normal_exp;
    if (exp <= 0) {
      // Denormalized numbers. We don't remove the MSB in this case.
      mantissa = std::ldexp(normal_value, 10 + exp);
      exp = 0;
    } else {
      // Handle the normal case. We remove the MSB by subtracting 0.5. Then we
      // use an exponent of 11 here instead of 10 because we're in the [0.5, 1)
      // range and need the full 10 bits of precision.
      mantissa = std::ldexp(normal_value - 0.5, 11);
    }
  }
  uint16_t result = exp << 10 | mantissa;
  if (std::copysign(1, input) < 0) {
    result |= 0x8000;
  }
  return result;
}

}  // namespace cbor
