// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/259749095): When Crubit (`cpp_api_from_rust`) supports generic
// return types like `Result<(Value, usize), Error>` directly across FFI, remove
// `ParseResult` and `parse_with_config_ffi` below, and rely entirely on
// `parse_with_config` propagating a structured `Result` to C++.
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

// LINT.IfChange(ErrorCode)
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ErrorCode {
    Ok = 0,
    UnsupportedMajorType = 1,
    UnknownAdditionalInfo = 2,
    IncompleteCborData = 3,
    IncorrectMapKeyType = 4,
    TooMuchNesting = 5,
    InvalidUtf8 = 6,
    ExtraneousData = 7,
    OutOfOrderKey = 8,
    NonMinimalCborEncoding = 9,
    UnsupportedSimpleValue = 10,
    UnsupportedFloatingPointValue = 11,
    OutOfRangeIntegerValue = 12,
    DuplicateKey = 13,
    UnknownError = 14,
}
// LINT.ThenChange(//components/cbor/reader.h:DecoderError,
// //components/cbor/reader.cc:DecoderErrorAsserts)

impl Error {
    pub fn error_code(&self) -> ErrorCode {
        match self {
            Error::UnsupportedMajorType(_, _) => ErrorCode::UnsupportedMajorType,
            Error::UnsupportedAdditionalInformation(_, _) => ErrorCode::UnknownAdditionalInfo,
            Error::InputTruncated => ErrorCode::IncompleteCborData,
            Error::UnsupportedMapKeyType(_, _) => ErrorCode::IncorrectMapKeyType,
            Error::DepthLimitExceeded(_, _) => ErrorCode::TooMuchNesting,
            Error::InvalidUTF8(_) => ErrorCode::InvalidUtf8,
            Error::TrailingData(_) => ErrorCode::ExtraneousData,
            Error::MapKeysOutOfOrder(_, _) => ErrorCode::OutOfOrderKey,
            Error::NonMinimalAdditionalData(_) => ErrorCode::NonMinimalCborEncoding,
            Error::UnsupportedSimpleValue(_) => ErrorCode::UnsupportedSimpleValue,
            Error::UnsupportedFloatingPointValue(_) => ErrorCode::UnsupportedFloatingPointValue,
            Error::NegativeOutOfRange(_) | Error::UnsignedOutOfRange(_) => {
                ErrorCode::OutOfRangeIntegerValue
            }
            Error::DuplicateMapKey(_, _) => ErrorCode::DuplicateKey,
        }
    }
}

#[repr(C)]
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

#[repr(C)]
#[derive(Debug, PartialEq, Clone)]
pub struct ParseResult {
    pub value: Value,
    pub bytes_consumed: usize,
    pub error_code: ErrorCode,
}

/// Parses CBOR bytes into a `Value`.
///
/// Returns the parsed value and the number of bytes consumed.
pub fn parse_with_config(mut input: &[u8], config: Config) -> Result<(Value, usize), Error> {
    let orig_len = input.len();
    let ret = parse_value(&mut input, 0, &config)?;
    let consumed = orig_len - input.len();
    Ok((ret, consumed))
}

/// FFI wrapper returning `ParseResult` for Crubit C++ consumption.
pub fn parse_with_config_ffi(input: &[u8], config: Config) -> ParseResult {
    match parse_with_config(input, config) {
        Ok((value, bytes_consumed)) => {
            ParseResult { value, bytes_consumed, error_code: ErrorCode::Ok }
        }
        Err(err) => {
            ParseResult { value: Value::Null, bytes_consumed: 0, error_code: err.error_code() }
        }
    }
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
        (MAJOR_TYPE_SIMPLE_VALUE, SIMPLE_VALUE_FLOAT_16) => u64_from_be_bytes::<2, u16>(input),
        (MAJOR_TYPE_SIMPLE_VALUE, SIMPLE_VALUE_FLOAT_32) => u64_from_be_bytes::<4, u32>(input),
        (MAJOR_TYPE_SIMPLE_VALUE, SIMPLE_VALUE_FLOAT_64) => u64_from_be_bytes::<8, u64>(input),
        (_, 0..=23) => Ok(info as u64),
        (_, ADDL_INFO_1_BYTE) => get_argument::<1, u8>(input),
        (_, ADDL_INFO_2_BYTES) => get_argument::<2, u16>(input),
        (_, ADDL_INFO_4_BYTES) => get_argument::<4, u32>(input),
        (_, ADDL_INFO_8_BYTES) => get_argument::<8, u64>(input),
        _ => Err(Error::UnsupportedAdditionalInformation(input.len(), info)),
    }?;
    Ok((major_type, info, arg))
}

// N should really be an associated const, or even replaced with `const {
// core::mem::size_of::<Self>() }`, but those can't be used in generic
// expressions: https://github.com/rust-lang/rust/issues/76560.
trait FromBytes<const N: usize>: Into<u64> {
    fn from_be_bytes(bytes: [u8; N]) -> Self;
}

impl FromBytes<1> for u8 {
    fn from_be_bytes(bytes: [u8; 1]) -> Self {
        Self::from_be_bytes(bytes)
    }
}

impl FromBytes<2> for u16 {
    fn from_be_bytes(bytes: [u8; 2]) -> Self {
        Self::from_be_bytes(bytes)
    }
}

impl FromBytes<4> for u32 {
    fn from_be_bytes(bytes: [u8; 4]) -> Self {
        Self::from_be_bytes(bytes)
    }
}

impl FromBytes<8> for u64 {
    fn from_be_bytes(bytes: [u8; 8]) -> Self {
        Self::from_be_bytes(bytes)
    }
}

fn u64_from_be_bytes<const N: usize, T: FromBytes<N>>(input: &mut &[u8]) -> Result<u64, Error> {
    const {
        assert!(N == core::mem::size_of::<T>());
    }

    let Some((bytes, rest)) = input.split_first_chunk::<N>() else {
        return Err(Error::InputTruncated);
    };
    *input = rest;
    Ok(T::from_be_bytes(*bytes).into())
}

fn get_argument<const N: usize, T: FromBytes<N>>(input: &mut &[u8]) -> Result<u64, Error> {
    let v = u64_from_be_bytes::<N, T>(input)?;
    let (_, expected_num_bytes) = crate::writer::low_bits_and_length(v);
    if N != expected_num_bytes {
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
    let Ok(len) = usize::try_from(len64) else {
        return Err(Error::InputTruncated);
    };
    let bytes = get(input, len)?;
    Ok(Value::Bytestring(bytes.to_vec()))
}

fn to_string(input: &mut &[u8], len64: u64, config: &Config) -> Result<Value, Error> {
    let Ok(len) = usize::try_from(len64) else {
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
    let mut ret: BTreeMap<MapKey, Value> = BTreeMap::new();

    for _ in 0..num_elements {
        // TODO(crbug.com/259749095): Validate key type + order (and possibly return
        // early) before attempting to parse the value.
        let key_value = parse_value(input, depth, config)?;
        let after_key_len = input.len();
        let value = parse_value(input, depth, config)?;

        let key = match MapKey::try_from(key_value) {
            Ok(key) => key,
            Err(Value::InvalidUtf8(_)) => return Err(Error::InvalidUTF8(after_key_len)),
            Err(value) => return Err(Error::UnsupportedMapKeyType(after_key_len, value)),
        };

        if let Some((previous, _)) = ret.last_key_value() {
            match previous.cmp(&key) {
                Ordering::Less => {}
                Ordering::Greater if !ret.contains_key(&key) => {
                    return Err(Error::MapKeysOutOfOrder(after_key_len, Value::from(key)));
                }
                // Covers `Ordering::Equal` and `Ordering::Greater` when the key is already
                // in the map (e.g. an out-of-order duplicate).
                _ => {
                    return Err(Error::DuplicateMapKey(after_key_len, Value::from(key)));
                }
            }
        }

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
