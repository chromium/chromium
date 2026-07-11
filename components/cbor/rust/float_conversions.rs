// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// Decodes a 16-bit IEEE 754 half-precision float (`f16`) into a 64-bit double
/// (`f64`).
///
/// Note: These functions are implemented manually because `f16` is still an
/// unstable experimental feature in Rust (tracking issue #116909).
///
/// TODO(crbug.com/460501344): Remove float support from all CBOR parsers after
/// the Protected Audience API is gone.
///
/// IEEE 754 float bit layouts:
/// - `f16`: 1 sign bit | 5 exponent bits (bias 15)   | 10 mantissa bits Format:
///   `S EEEEE MMMMMMMMMM`
/// - `f64`: 1 sign bit | 11 exponent bits (bias 1023)| 52 mantissa bits Format:
///   `S EEEEEEEEEEE MMM...MMM` (52 bits)
///
/// This function unpacks the sign, exponent, and mantissa of the `f16` and
/// shifts them into the appropriate positions for an `f64`. It gracefully
/// handles subnormal numbers using standard bit manipulations.
pub(crate) fn decode_f16(half: u16) -> f64 {
    let exp = (half >> 10) & 0x1f;
    let mant = half & 0x3ff;
    let sign = (half as u64 & 0x8000) << 48;

    match exp {
        0 if mant == 0 => f64::from_bits(sign),
        0 => {
            // Normalize the subnormal mantissa
            let e = mant.leading_zeros() - 5;
            let m = mant << e;
            f64::from_bits(sign | (((1023 - 14 - e) as u64) << 52) | (((m & 0x3ff) as u64) << 42))
        }
        31 => f64::from_bits(sign | 0x7ff0000000000000 | ((mant as u64) << 42)),
        _ => f64::from_bits(sign | ((exp as u64 + 1008) << 52) | ((mant as u64) << 42)),
    }
}

/// Encodes a 64-bit double (`f64`) into a 16-bit IEEE 754 half-precision float
/// (`f16`).
///
/// Note: These functions are implemented manually because `f16` is still an
/// unstable experimental feature in Rust (tracking issue #116909).
///
/// TODO(crbug.com/460501344): Remove float support from all CBOR parsers after
/// the Protected Audience API is gone.
///
/// This performs the reverse of `decode_f16`, repacking the components of an
/// `f64`:   Format: `S EEEEEEEEEEE MMM...MMM` (64 bits)
/// into the `f16` format:
///   Format: `S EEEEE MMMMMMMMMM` (16 bits)
///
/// The exponent is adjusted from the `f64` bias (1023) to the `f16` bias (15),
/// and it gracefully handles edge cases like `NaN`, infinity, and subnormal
/// numbers using bitwise logic without floating-point math overhead.
pub(crate) fn encode_f16(input: f64) -> u16 {
    let bits = input.to_bits();
    let sign = ((bits >> 48) & 0x8000) as u16;
    use core::num::FpCategory;

    match input.classify() {
        FpCategory::Nan => sign | 0x7e00,
        FpCategory::Infinite => sign | 0x7c00,
        FpCategory::Zero => sign,
        _ => {
            let e = ((bits >> 52) & 0x7ff) as i32;
            let m = bits & 0xfffffffffffff;

            // Convert f64 exponent to f16 exponent (bias 1023 -> 15)
            let f16_e = e - 1023 + 15;

            // Extract the unrounded f16 bits, the shift amount, and the explicit f64
            // mantissa
            let (f16_unrounded, shift, explicit_m) = match f16_e {
                ..=0 => {
                    // Subnormal or underflow. Shift mantissa right by `1 - f16_e`.
                    let shift = (42 + (1 - f16_e)) as u32;
                    if shift >= 53 {
                        return sign; // Underflow to zero
                    }

                    // We know input is NOT an f64 subnormal here (they all underflow above),
                    // so the implicit 1 is guaranteed to be present.
                    let explicit_m = m | (1 << 52);
                    (sign | ((explicit_m >> shift) as u16), shift, explicit_m)
                }
                1..=30 => {
                    // Normal
                    (sign | ((f16_e as u16) << 10) | ((m >> 42) as u16), 42, m)
                }
                31.. => return sign | 0x7c00, // Overflow to infinity
            };

            // IEEE 754 Round to Nearest, Ties to Even
            let mask = (1u64 << shift) - 1;
            let discarded = explicit_m & mask;
            let half = 1u64 << (shift - 1);

            // Round up if we discarded > 0.5, OR if we discarded exactly 0.5 and the
            // resulting mantissa is odd
            let round_up = discarded > half || (discarded == half && (f16_unrounded & 1) == 1);

            if round_up {
                // Adding 1 naturally ripples a subnormal up to a normal, or max_normal up to
                // infinity!
                f16_unrounded + 1
            } else {
                f16_unrounded
            }
        }
    }
}

pub(crate) fn is_f32_minimal(f: f32) -> bool {
    // NaN and infinities are allowed only as f16
    if !f.is_finite() {
        return false;
    }
    decode_f16(encode_f16(f as f64)) != (f as f64)
}

pub(crate) fn is_f64_minimal(f: f64) -> bool {
    // NaN and infinities are allowed only as f16
    if !f.is_finite() {
        return false;
    }
    f != (f as f32) as f64
}
