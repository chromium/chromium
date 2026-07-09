// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! `cbor` implements the subset of CBOR used in [CTAP2][1], with optional
//! extensions to support Chromium-specific requirements.
//!
//! Note: The core of this parser was originally imported from
//! `//third_party/cloud_authenticator/cbor`. It was subsequently modified
//! to support Chromium CBOR requirements. It will be wrapped in FFI and
//! eventually replace the current C++ parser.
//!
//! Under the strict CTAP2 configuration (`Config::default()`), parsing is
//! injective, meaning that no two distinct byte strings (that parse
//! successfully) will result in the same value. Reserialising the result of
//! parse will always result in exactly the same byte string. (This is checked
//! by fuzzing.)
//!
//! In the other direction, serialisation is also injective. However, an
//! arbitrary limit is placed on the maximum depth of parsed structures to avoid
//! degenerate inputs from consuming too much stack. Thus structures that are
//! deeper than this will serialise but that result cannot be parsed back to
//! the same value. Aside from this exception, all values should round-trip
//! correctly under the CTAP2 configuration.
//!
//! Note on Relaxations: When `Config::allow_floating_point` or
//! `Config::allow_invalid_utf8` are enabled, the injectivity and exact
//! round-tripping guarantees no longer hold. For example, `f16` floats will
//! parse to `f64` and serialize back as `f64` (changing the byte string).
//!
//! ```
//! let value = cbor::Value::String("hello".to_string());
//! let serialized = value.to_bytes();
//! assert_eq!(serialized, vec![0x65u8, 0x68, 0x65, 0x6c, 0x6c, 0x6f]);
//! assert_eq!(cbor::parse_with_config(serialized, cbor::Config::default()), Ok((value, 1)));
//! ```
//!
//! [1]: https://fidoalliance.org/specs/fido-v2.2-rd-20230321/fido-client-to-authenticator-protocol-v2.2-rd-20230321.html#ctap2-canonical-cbor-encoding-form

#![no_std]
#![forbid(unsafe_code)]

extern crate alloc;

use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec::Vec;
use core::cmp::Ordering;
use core::ops::Deref;

// This code assumes that `usize` fits in a `u64` because it uses `as u64` in a
// couple of places.
const _: () =
    assert!(core::mem::size_of::<usize>() <= core::mem::size_of::<u64>(), "usize too large");

// CBOR Major Types (RFC 8949 Section 3.1)
// https://datatracker.ietf.org/doc/html/rfc8949#section-3.1
const MAJOR_TYPE_UNSIGNED_INT: u8 = 0;
const MAJOR_TYPE_NEGATIVE_INT: u8 = 1;
const MAJOR_TYPE_BYTE_STRING: u8 = 2;
const MAJOR_TYPE_TEXT_STRING: u8 = 3;
const MAJOR_TYPE_ARRAY: u8 = 4;
const MAJOR_TYPE_MAP: u8 = 5;
const MAJOR_TYPE_SIMPLE_VALUE: u8 = 7;

// CBOR Additional Information Values (RFC 8949 Section 3)
// https://datatracker.ietf.org/doc/html/rfc8949#section-3
const ADDL_INFO_1_BYTE: u8 = 24;
const ADDL_INFO_2_BYTES: u8 = 25;
const ADDL_INFO_4_BYTES: u8 = 26;
const ADDL_INFO_8_BYTES: u8 = 27;

// CBOR Simple Values (RFC 8949 Section 3.3)
// https://datatracker.ietf.org/doc/html/rfc8949#section-3.3
const SIMPLE_VALUE_FALSE: u8 = 20;
const SIMPLE_VALUE_TRUE: u8 = 21;
const SIMPLE_VALUE_NULL: u8 = 22;
const SIMPLE_VALUE_UNDEFINED: u8 = 23;
const SIMPLE_VALUE_FLOAT_16: u8 = 25;
const SIMPLE_VALUE_FLOAT_32: u8 = 26;
const SIMPLE_VALUE_FLOAT_64: u8 = 27;

/// MAX_DEPTH is the maximum "depth" of a structure that will be parsed.
/// Each array or map increases the depth by one.
pub const MAX_DEPTH: usize = 16;

/// Error enumerates the different errors that can occur during parsing.
///
/// It is intended for debugging only. Where `usize` values are present, they
/// contain the approximate number of bytes remaining when the error occurred.
#[derive(Debug, PartialEq)]
pub enum Error {
    DepthLimitExceeded(usize, usize),
    InputTruncated,
    InvalidUTF8(usize),
    DuplicateMapKey(usize, Value),
    MapKeysOutOfOrder(usize, Value),
    NegativeOutOfRange(u64),
    NonMinimalAdditionalData(usize),
    TrailingData(usize),
    UnsignedOutOfRange(u64),
    UnsupportedAdditionalInformation(usize, u8),
    UnsupportedMajorType(usize, u8),
    UnsupportedMapKeyType(usize, Value),
    UnsupportedSimpleValue(u64),
    UnsupportedFloatingPointValue(u64),
}

#[derive(Clone, Copy)]
pub struct Config {
    pub allow_invalid_utf8: bool,
    pub allow_floating_point: bool,
    pub max_nesting_level: usize,
}

impl Default for Config {
    fn default() -> Self {
        Self { allow_invalid_utf8: false, allow_floating_point: false, max_nesting_level: 16 }
    }
}

fn get_u8(bytes: &mut &[u8]) -> Result<u8, Error> {
    bytes.split_off_first().copied().ok_or(Error::InputTruncated)
}

fn get<'a>(bytes: &mut &'a [u8], num_bytes: usize) -> Result<&'a [u8], Error> {
    bytes.split_off(..num_bytes).ok_or(Error::InputTruncated)
}

/// Value represents a CBOR structure.
///
/// Integers are mapped to `i64` despite CBOR having 65-bit integers. CBOR
/// integers outside the range of an `i64` result in an error during parsing.
/// Byte strings are returned as `Bytes`s in order to avoid copies.
#[derive(Debug, PartialEq, Clone)]
pub enum Value {
    Int(i64),
    Bytestring(Vec<u8>),
    String(String),
    Array(Vec<Value>),
    Map(BTreeMap<MapKey, Value>),
    Boolean(bool),
    Float(f64),
    Null,
    Undefined,
    InvalidUtf8(Vec<u8>),
}

// low_bits_and_length returns the bottom five bits of the initial byte of a
// CBOR value, and the number of bytes that will need to follow it in order to
// encode an argument `arg`.
fn low_bits_and_length(arg: u64) -> (u8, usize) {
    if arg < ADDL_INFO_1_BYTE as u64 {
        (arg as u8, 0)
    } else if arg < 0x100 {
        (ADDL_INFO_1_BYTE, 1)
    } else if arg < 0x10000 {
        (ADDL_INFO_2_BYTES, 2)
    } else if arg < 0x100000000 {
        (ADDL_INFO_4_BYTES, 4)
    } else {
        (ADDL_INFO_8_BYTES, 8)
    }
}

/// write_header appends the [initial byte and "argument"][1] of a CBOR value.
///
/// [1]: https://datatracker.ietf.org/doc/html/rfc8949#name-specification-of-the-cbor-e
fn write_header(out: &mut Vec<u8>, major_type: u8, arg: u64) {
    let (low_bits, num_bytes) = low_bits_and_length(arg);
    out.push(major_type << 5 | low_bits);
    let value_bytes = arg.to_be_bytes();
    out.extend_from_slice(&value_bytes[8 - num_bytes..]);
}

impl Value {
    // to_bytes serialises `self` to CBOR and returns the result.
    pub fn to_bytes(&self) -> Vec<u8> {
        let mut ret = Vec::new();
        self.append_bytes(&mut ret);
        ret
    }

    // append_bytes appends a serialisation of `self` to `out`.
    pub fn append_bytes(&self, out: &mut Vec<u8>) {
        match self {
            Value::Int(v) if *v >= 0 => write_header(out, MAJOR_TYPE_UNSIGNED_INT, *v as u64),
            Value::Int(v) => write_header(out, MAJOR_TYPE_NEGATIVE_INT, !v as u64),
            Value::Bytestring(s) => {
                write_header(out, MAJOR_TYPE_BYTE_STRING, s.len() as u64);
                out.extend_from_slice(s.deref());
            }
            Value::String(s) => {
                write_header(out, MAJOR_TYPE_TEXT_STRING, s.len() as u64);
                out.extend_from_slice(s.as_bytes());
            }
            Value::Array(a) => {
                write_header(out, MAJOR_TYPE_ARRAY, a.len() as u64);
                for elem in a {
                    elem.append_bytes(out);
                }
            }
            Value::Map(m) => {
                write_header(out, MAJOR_TYPE_MAP, m.len() as u64);
                for (key, value) in m {
                    key.append_bytes(out);
                    value.append_bytes(out);
                }
            }
            Value::Boolean(b) => write_header(
                out,
                MAJOR_TYPE_SIMPLE_VALUE,
                if *b { SIMPLE_VALUE_TRUE as u64 } else { SIMPLE_VALUE_FALSE as u64 },
            ),
            Value::Null => write_header(out, MAJOR_TYPE_SIMPLE_VALUE, SIMPLE_VALUE_NULL as u64),
            Value::Undefined => {
                write_header(out, MAJOR_TYPE_SIMPLE_VALUE, SIMPLE_VALUE_UNDEFINED as u64)
            }
            Value::InvalidUtf8(bytes) => {
                write_header(out, MAJOR_TYPE_TEXT_STRING, bytes.len() as u64);
                out.extend_from_slice(bytes);
            }
            Value::Float(f) => {
                out.push(MAJOR_TYPE_SIMPLE_VALUE << 5 | SIMPLE_VALUE_FLOAT_64);
                out.extend_from_slice(&f.to_bits().to_be_bytes());
            }
        }
    }
}

/// A MapKey is the type of values that can key a CBOR map.
#[derive(Debug, PartialEq, Eq, Clone)]
pub enum MapKey {
    // A separate `MapKey` type is used because we want to exclude things like
    // maps keyed by arrays or other maps. Such structures never appear in
    // CTAP and so we don't need to support them.
    //
    // We expect that a map will always have keys of the same type, which
    // suggests that `Value::Map` could be split into `Value::IntKeyedMap` etc.
    // However, that falls down when a map is empty because the parser can't
    // know what the key type should be, yet calling code will want to expect
    // the right type of map. Thus we end up supporting heterogeneous maps.
    Int(i64),
    Bytestring(Vec<u8>),
    String(String),
}

impl MapKey {
    fn type_arg_and_payload(&self) -> (u8, u64, Option<&[u8]>) {
        match self {
            MapKey::Int(v) if *v >= 0 => (MAJOR_TYPE_UNSIGNED_INT, *v as u64, None),
            MapKey::Int(v) => (MAJOR_TYPE_NEGATIVE_INT, !*v as u64, None),
            MapKey::Bytestring(b) => (MAJOR_TYPE_BYTE_STRING, b.len() as u64, Some(b)),
            MapKey::String(s) => (MAJOR_TYPE_TEXT_STRING, s.len() as u64, Some(s.as_bytes())),
        }
    }

    fn append_bytes(&self, out: &mut Vec<u8>) {
        let (major_type, arg, payload) = self.type_arg_and_payload();
        write_header(out, major_type, arg);
        if let Some(payload) = payload {
            out.extend_from_slice(payload);
        }
    }
}

impl PartialOrd for MapKey {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for MapKey {
    fn cmp(&self, other: &Self) -> Ordering {
        self.type_arg_and_payload().cmp(&other.type_arg_and_payload())
    }
}

/// Parses CBOR bytes into a `Value`.
///
/// Returns the parsed value and the number of bytes consumed.
/// The consumed bytes are used by the C++ FFI (`Reader::Read`) to support
/// parsing a CBOR structure when it's concatenated with other data (by
/// reporting how many bytes the C++ caller should advance its buffer by).
pub fn parse_with_config(mut input: &[u8], config: Config) -> Result<(Value, usize), Error> {
    let orig_len = input.len();
    let ret = parse_value(&mut input, 0, &config)?;
    let consumed = orig_len - input.len();
    Ok((ret, consumed))
}

fn parse_value(input: &mut &[u8], depth: usize, config: &Config) -> Result<Value, Error> {
    if depth > config.max_nesting_level {
        return Err(Error::DepthLimitExceeded(input.len(), config.max_nesting_level));
    }
    let (major_type, info, arg) = parse_header(input)?;
    match major_type {
        MAJOR_TYPE_UNSIGNED_INT => to_int(arg, false),
        MAJOR_TYPE_NEGATIVE_INT => to_int(arg, true),
        MAJOR_TYPE_BYTE_STRING => to_bytestring(input, arg),
        MAJOR_TYPE_TEXT_STRING => to_string(input, arg, config),
        MAJOR_TYPE_ARRAY => to_array(input, arg, depth + 1, config),
        MAJOR_TYPE_MAP => to_map(input, arg, depth + 1, config),
        MAJOR_TYPE_SIMPLE_VALUE => to_simple_value(info, arg, config),
        _ => Err(Error::UnsupportedMajorType(input.len(), major_type)),
    }
}

fn parse_header(input: &mut &[u8]) -> Result<(u8, u8, u64), Error> {
    let b = get_u8(input)?;
    let major_type = b >> 5;
    let info = b & 0x1f;
    let arg = match (major_type, info) {
        (MAJOR_TYPE_SIMPLE_VALUE, SIMPLE_VALUE_FLOAT_16) => get_float_argument(input, 2),
        (MAJOR_TYPE_SIMPLE_VALUE, SIMPLE_VALUE_FLOAT_32) => get_float_argument(input, 4),
        (MAJOR_TYPE_SIMPLE_VALUE, SIMPLE_VALUE_FLOAT_64) => get_float_argument(input, 8),
        (_, 0..=23) => Ok(info as u64),
        (_, ADDL_INFO_1_BYTE) => get_argument(input, 1),
        (_, ADDL_INFO_2_BYTES) => get_argument(input, 2),
        (_, ADDL_INFO_4_BYTES) => get_argument(input, 4),
        (_, ADDL_INFO_8_BYTES) => get_argument(input, 8),
        _ => Err(Error::UnsupportedAdditionalInformation(input.len(), info)),
    }?;
    Ok((major_type, info, arg))
}

fn get_float_argument(input: &mut &[u8], num_bytes: u8) -> Result<u64, Error> {
    let mut v: u64 = 0;
    for _ in 0..num_bytes {
        v <<= 8;
        let b = get_u8(input)?;
        v |= b as u64;
    }
    Ok(v)
}

fn get_argument(input: &mut &[u8], num_bytes: u8) -> Result<u64, Error> {
    let mut v: u64 = 0;
    for _ in 0..num_bytes {
        v <<= 8;
        let b = get_u8(input)?;
        v |= b as u64;
    }
    let (_, expected_num_bytes) = low_bits_and_length(v);
    if num_bytes as usize != expected_num_bytes {
        Err(Error::NonMinimalAdditionalData(input.len()))
    } else {
        Ok(v)
    }
}

fn to_int(arg: u64, is_negative: bool) -> Result<Value, Error> {
    if is_negative {
        if arg > i64::MAX as u64 {
            Err(Error::NegativeOutOfRange(arg))
        } else {
            Ok(Value::Int(!arg as i64))
        }
    } else if arg > i64::MAX as u64 {
        Err(Error::UnsignedOutOfRange(arg))
    } else {
        Ok(Value::Int(arg as i64))
    }
}

fn to_bytestring(input: &mut &[u8], len64: u64) -> Result<Value, Error> {
    let Some(len): Option<usize> = len64.try_into().ok() else {
        return Err(Error::InputTruncated);
    };
    let bytes = get(input, len)?;
    Ok(Value::Bytestring(bytes.to_vec()))
}

fn to_string(input: &mut &[u8], len64: u64, config: &Config) -> Result<Value, Error> {
    let Some(len): Option<usize> = len64.try_into().ok() else {
        return Err(Error::InputTruncated);
    };
    let orig_len = input.len();
    let bytes = get(input, len)?;
    match str::from_utf8(bytes) {
        Ok(string) => Ok(Value::String(String::from(string))),
        Err(_) => {
            if config.allow_invalid_utf8 {
                Ok(Value::InvalidUtf8(bytes.to_vec()))
            } else {
                Err(Error::InvalidUTF8(orig_len))
            }
        }
    }
}

fn to_array(
    input: &mut &[u8],
    num_elements: u64,
    depth: usize,
    config: &Config,
) -> Result<Value, Error> {
    let mut ret = Vec::new();
    for _ in 0..num_elements {
        ret.push(parse_value(input, depth, config)?);
    }
    Ok(Value::Array(ret))
}

fn to_map(
    input: &mut &[u8],
    num_elements: u64,
    depth: usize,
    config: &Config,
) -> Result<Value, Error> {
    let mut ret = BTreeMap::new();
    let mut previous_key: &[u8] = &[];

    for _ in 0..num_elements {
        let key_slice = *input;
        let key_value = parse_value(input, depth, config)?;
        let key_bytes = &key_slice[..key_slice.len() - input.len()];

        // `previous_key` starts empty. The first key always evaluates as
        // `Ordering::Greater` since CBOR values require at least a 1-byte
        // header, meaning `key_bytes` is never empty.
        match key_bytes.cmp(previous_key) {
            Ordering::Equal => return Err(Error::DuplicateMapKey(input.len(), key_value)),
            Ordering::Less => return Err(Error::MapKeysOutOfOrder(input.len(), key_value)),
            Ordering::Greater => {}
        }
        previous_key = key_bytes;

        let key = match key_value {
            Value::Int(i) => MapKey::Int(i),
            Value::Bytestring(b) => MapKey::Bytestring(b),
            Value::String(s) => MapKey::String(s),
            Value::InvalidUtf8(_) => return Err(Error::InvalidUTF8(input.len())),
            _ => return Err(Error::UnsupportedMapKeyType(input.len(), key_value)),
        };
        let value = parse_value(input, depth, config)?;
        ret.insert(key, value);
    }
    Ok(Value::Map(ret))
}

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
fn decode_f16(half: u16) -> f64 {
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
fn encode_f16(input: f64) -> u16 {
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

fn is_f32_minimal(f: f32) -> bool {
    // NaN and infinities are allowed only as f16
    if !f.is_finite() {
        return false;
    }
    decode_f16(encode_f16(f as f64)) != (f as f64)
}

fn is_f64_minimal(f: f64) -> bool {
    // NaN and infinities are allowed only as f16
    if !f.is_finite() {
        return false;
    }
    f != (f as f32) as f64
}

fn to_simple_value(info: u8, arg: u64, config: &Config) -> Result<Value, Error> {
    match info {
        SIMPLE_VALUE_FALSE => Ok(Value::Boolean(false)),
        SIMPLE_VALUE_TRUE => Ok(Value::Boolean(true)),
        SIMPLE_VALUE_NULL => Ok(Value::Null),
        SIMPLE_VALUE_UNDEFINED => Ok(Value::Undefined),
        SIMPLE_VALUE_FLOAT_16..=SIMPLE_VALUE_FLOAT_64 if !config.allow_floating_point => {
            Err(Error::UnsupportedFloatingPointValue(arg))
        }
        SIMPLE_VALUE_FLOAT_16 => Ok(Value::Float(decode_f16(arg as u16))),
        SIMPLE_VALUE_FLOAT_32 if is_f32_minimal(f32::from_bits(arg as u32)) => {
            Ok(Value::Float(f32::from_bits(arg as u32) as f64))
        }
        SIMPLE_VALUE_FLOAT_32 => Err(Error::NonMinimalAdditionalData(4)),
        SIMPLE_VALUE_FLOAT_64 if is_f64_minimal(f64::from_bits(arg)) => {
            Ok(Value::Float(f64::from_bits(arg)))
        }
        SIMPLE_VALUE_FLOAT_64 => Err(Error::NonMinimalAdditionalData(8)),
        _ => Err(Error::UnsupportedSimpleValue(arg)),
    }
}
