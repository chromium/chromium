// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CBOR_FLOAT_CONVERSIONS_H_
#define COMPONENTS_CBOR_FLOAT_CONVERSIONS_H_

#include <cstdint>

namespace cbor {

// Convert the half-precision float in the provided `value` to a double
// precision floating point number.
double DecodeHalfPrecisionFloat(uint16_t value);

// Convert the double precision float in the provided `value` to a
// half-precision floating point number. The output is only meaningful if the
// value can be represented in half-precision.
uint16_t EncodeHalfPrecisionFloat(double value);

}  // namespace cbor

#endif  // COMPONENTS_CBOR_FLOAT_CONVERSIONS_H_
