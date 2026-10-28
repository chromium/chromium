// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use alloc::vec::Vec;

use crate::constants::*;
use crate::values::{Map, MapEntry, MapKey, Value};

// LINT.IfChange(Error)
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Error {
    UnsupportedMajorType = 1,
    UnknownAdditionalInfo = 2,
    IncompleteCborData = 3,
    IncorrectMapKeyType = 4,
    TooMuchNesting = 5,
    InvalidUtf8 = 6,
    ExtraneousData = 7,
    // 8 was OutOfOrderKey.
    NonMinimalCborEncoding = 9,
    UnsupportedSimpleValue = 10,
    UnsupportedFloatingPointValue = 11,
    OutOfRangeIntegerValue = 12,
    // 13 was DuplicateKey.
    UnknownError = 14,
    OutOfOrderOrDuplicateKey = 15,
}
// LINT.ThenChange(//components/cbor/reader.h:DecoderError,
// //components/cbor/reader.cc:DecoderErrorAsserts)

impl Error {
    pub const fn to_str(self) -> &'static str {
        match self {
            Self::UnsupportedMajorType => "Unsupported major type.",
            Self::UnknownAdditionalInfo => {
                "Unknown additional info format in the first byte."
            }
            Self::IncompleteCborData => {
                "Prematurely terminated CBOR data byte array."
            }
            Self::IncorrectMapKeyType => {
                "Specified map key type is not supported by the current implementation."
            }
            Self::TooMuchNesting => "Too much nesting.",
            Self::InvalidUtf8 => {
                "String encodings other than UTF-8 are not allowed."
            }
            Self::ExtraneousData => "Trailing data bytes are not allowed.",
            Self::NonMinimalCborEncoding => {
                "Unsigned integers must be encoded with minimum number of bytes."
            }
            Self::UnsupportedSimpleValue => {
                "Unsupported or unassigned simple value."
            }
            Self::UnsupportedFloatingPointValue => {
                "Floating point numbers are not supported."
            }
            Self::OutOfRangeIntegerValue => {
                "Integer values must be between INT64_MIN and INT64_MAX."
            }
            Self::UnknownError => "An unknown error occured.",
            Self::OutOfOrderOrDuplicateKey => {
                "Map keys must be strictly monotonically increasing based on major type, byte length and then by byte-wise lexical order."
            }
        }
    }
}

impl core::fmt::Display for Error {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_str(self.to_str())
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Config {
    pub allow_invalid_utf8: bool,
    pub max_nesting_level: usize,
}

impl Default for Config {
    fn default() -> Self {
        Self { allow_invalid_utf8: false, max_nesting_level: 16 }
    }
}

#[repr(C)]
#[derive(Debug, PartialEq, Clone)]
pub struct ParseResult<'a> {
    pub value: Value<'a>,
    pub bytes_consumed: usize,
}

/// Parses CBOR bytes into a `Value`.
///
/// Returns the parsed value and the number of bytes consumed.
pub fn parse_with_config<'a>(
    mut input: &'a [u8],
    config: Config,
) -> Result<ParseResult<'a>, Error> {
    let orig_len = input.len();
    let decoder = Decoder { config };
    let value = decoder.parse_value(&mut input, 0)?;
    let bytes_consumed = orig_len - input.len();
    Ok(ParseResult { value, bytes_consumed })
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

trait SliceExt<'a> {
    fn read_array<const N: usize>(&mut self) -> Option<&'a [u8; N]>;
    fn read_bytes(&mut self, len: usize) -> Option<&'a [u8]>;
    fn read_u8(&mut self) -> Option<u8>;
}

impl<'a> SliceExt<'a> for &'a [u8] {
    fn read_array<const N: usize>(&mut self) -> Option<&'a [u8; N]> {
        let (head, tail) = self.split_first_chunk::<N>()?;
        *self = tail;
        Some(head)
    }

    fn read_bytes(&mut self, len: usize) -> Option<&'a [u8]> {
        self.split_off(..len)
    }

    fn read_u8(&mut self) -> Option<u8> {
        self.split_off_first().copied()
    }
}

enum Header {
    Int(i64),
    ByteString(u64),
    TextString(u64),
    Array(u64),
    Map(u64),
    SimpleValue(u8),
}

struct Decoder {
    config: Config,
}

impl Decoder {
    fn parse_value<'a>(&self, data: &mut &'a [u8], depth: usize) -> Result<Value<'a>, Error> {
        if depth > self.config.max_nesting_level {
            return Err(Error::TooMuchNesting);
        }

        match Self::read_header(data)? {
            Header::Int(val) => Ok(Value::Int(val)),
            Header::ByteString(len) => {
                Self::read_complete_bytestring(data, len).map(Value::Bytestring)
            }
            Header::TextString(len) => {
                let bytes = Self::read_complete_bytestring(data, len)?;
                match core::str::from_utf8(bytes) {
                    Ok(s) => Ok(Value::String(s)),
                    Err(_) if self.config.allow_invalid_utf8 => Ok(Value::InvalidUtf8(bytes)),
                    Err(_) => Err(Error::InvalidUtf8),
                }
            }
            Header::Array(num_elements) => self.parse_array(data, num_elements, depth),
            Header::Map(num_elements) => self.parse_map(data, num_elements, depth),
            Header::SimpleValue(info) => match info {
                SIMPLE_VALUE_FALSE => Ok(Value::Boolean(false)),
                SIMPLE_VALUE_TRUE => Ok(Value::Boolean(true)),
                SIMPLE_VALUE_NULL => Ok(Value::Null),
                SIMPLE_VALUE_UNDEFINED => Ok(Value::Undefined),
                SIMPLE_VALUE_FLOAT_16..=SIMPLE_VALUE_FLOAT_64 => {
                    Err(Error::UnsupportedFloatingPointValue)
                }
                _ => Err(Error::UnsupportedSimpleValue),
            },
        }
    }

    fn parse_array<'a>(
        &self,
        data: &mut &'a [u8],
        num_elements: u64,
        depth: usize,
    ) -> Result<Value<'a>, Error> {
        let mut ret = Vec::new();
        for _ in 0..num_elements {
            ret.push(self.parse_value(data, depth + 1)?);
        }
        Ok(Value::Array(ret))
    }

    fn parse_map<'a>(
        &self,
        data: &mut &'a [u8],
        num_elements: u64,
        depth: usize,
    ) -> Result<Value<'a>, Error> {
        let mut ret: Vec<MapEntry> = Vec::new();

        for _ in 0..num_elements {
            let key = self.parse_map_key(data, depth + 1)?;

            if let Some(previous) = ret.last()
                && previous.key >= key
            {
                return Err(Error::OutOfOrderOrDuplicateKey);
            }

            let value = self.parse_value(data, depth + 1)?;

            ret.push(MapEntry { key, value });
        }
        Ok(Value::Map(Map::from_sorted_vec_unchecked(ret)))
    }

    fn parse_map_key<'a>(&self, data: &mut &'a [u8], depth: usize) -> Result<MapKey<'a>, Error> {
        if depth > self.config.max_nesting_level {
            return Err(Error::TooMuchNesting);
        }

        match Self::read_header(data)? {
            Header::Int(val) => Ok(MapKey::Int(val)),
            Header::ByteString(len) => {
                Self::read_complete_bytestring(data, len).map(MapKey::Bytestring)
            }
            Header::TextString(len) => {
                let bytes = Self::read_complete_bytestring(data, len)?;
                core::str::from_utf8(bytes).map(MapKey::String).map_err(|_| Error::InvalidUtf8)
            }
            Header::Array(_) | Header::Map(_) | Header::SimpleValue(_) => {
                Err(Error::IncorrectMapKeyType)
            }
        }
    }

    fn read_complete_bytestring<'a>(data: &mut &'a [u8], len: u64) -> Result<&'a [u8], Error> {
        let len = usize::try_from(len).map_err(|_| Error::IncompleteCborData)?;
        data.read_bytes(len).ok_or(Error::IncompleteCborData)
    }

    fn read_arg<const N: usize, T: FromBytes<N>>(data: &mut &[u8]) -> Result<u64, Error> {
        let arr = data.read_array::<N>().ok_or(Error::IncompleteCborData)?;
        let v = T::from_be_bytes(*arr).into();
        let (_, expected_num_bytes) = crate::writer::low_bits_and_length(v);
        if N != expected_num_bytes {
            Err(Error::NonMinimalCborEncoding)
        } else {
            Ok(v)
        }
    }

    fn read_header(data: &mut &[u8]) -> Result<Header, Error> {
        let b = data.read_u8().ok_or(Error::IncompleteCborData)?;
        let major_type = b >> 5;
        let info = b & 0x1f;
        let arg = match info {
            0..=ADDL_INFO_DIRECT_MAX => info as u64,
            ADDL_INFO_1_BYTE => Self::read_arg::<1, u8>(data)?,
            ADDL_INFO_2_BYTES => Self::read_arg::<2, u16>(data)?,
            ADDL_INFO_4_BYTES => Self::read_arg::<4, u32>(data)?,
            ADDL_INFO_8_BYTES => Self::read_arg::<8, u64>(data)?,
            _ => return Err(Error::UnknownAdditionalInfo),
        };
        match major_type {
            MAJOR_TYPE_UNSIGNED_INT => {
                i64::try_from(arg).map(Header::Int).map_err(|_| Error::OutOfRangeIntegerValue)
            }
            MAJOR_TYPE_NEGATIVE_INT => i64::try_from(arg)
                .map(|val| Header::Int(!val))
                .map_err(|_| Error::OutOfRangeIntegerValue),
            MAJOR_TYPE_BYTE_STRING => Ok(Header::ByteString(arg)),
            MAJOR_TYPE_TEXT_STRING => Ok(Header::TextString(arg)),
            MAJOR_TYPE_ARRAY => Ok(Header::Array(arg)),
            MAJOR_TYPE_MAP => Ok(Header::Map(arg)),
            MAJOR_TYPE_SIMPLE_VALUE => Ok(Header::SimpleValue(info)),
            _ => Err(Error::UnsupportedMajorType),
        }
    }
}

/// Reads a CBOR array header (major type 4) and returns the number of elements
/// in the array. Leaves `data` unconsumed on error.
pub fn read_array_header(data: &mut &[u8]) -> Result<u64, Error> {
    let mut remaining = *data;
    match Decoder::read_header(&mut remaining)? {
        Header::Array(len) => {
            *data = remaining;
            Ok(len)
        }
        _ => Err(Error::UnsupportedMajorType),
    }
}

/// Reads a CBOR byte string header (major type 2) and returns the byte length.
/// Leaves `data` unconsumed on error.
pub fn read_bytestring_header(data: &mut &[u8]) -> Result<u64, Error> {
    let mut remaining = *data;
    match Decoder::read_header(&mut remaining)? {
        Header::ByteString(len) => {
            *data = remaining;
            Ok(len)
        }
        _ => Err(Error::UnsupportedMajorType),
    }
}

/// Reads `len` payload bytes of a CBOR byte string after its header has been
/// consumed. Leaves `data` unconsumed on error.
pub fn read_complete_bytestring<'a>(data: &mut &'a [u8], len: u64) -> Result<&'a [u8], Error> {
    Decoder::read_complete_bytestring(data, len)
}

/// Reads a complete CBOR byte string (major type 2), including its header and
/// payload bytes. Leaves `data` unconsumed on error.
pub fn read_bytestring<'a>(data: &mut &'a [u8]) -> Result<&'a [u8], Error> {
    let mut remaining = *data;
    let len = read_bytestring_header(&mut remaining)?;
    let bytes = read_complete_bytestring(&mut remaining, len)?;
    *data = remaining;
    Ok(bytes)
}
