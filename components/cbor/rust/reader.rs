// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec::Vec;
use core::cmp::Ordering;
use core::str;

use crate::constants::*;
use crate::float_conversions::*;
use crate::values::{MapKey, Value};

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
    let (_, expected_num_bytes) = crate::writer::low_bits_and_length(v);
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
