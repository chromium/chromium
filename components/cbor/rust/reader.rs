// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use alloc::vec::Vec;
use core::cmp::Ordering;

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
            Self::OutOfOrderKey => {
                "Map keys must be strictly monotonically increasing based on byte length and then by byte-wise lexical order."
            }
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
            Self::DuplicateKey => "Duplicate map keys are not allowed.",
            Self::UnknownError => "An unknown error occured.",
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
    let mut decoder = Decoder::with_config(config);
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

#[repr(C)]
#[derive(Debug, PartialEq, Clone)]
pub enum CborEvent<'a> {
    /// Integer value (major types 0 and 1) representable as an `i64`.
    Int(i64),
    // CBOR text strings in Chromium (e.g. WebBundle metadata/URLs) are small
    // (capped <= 1MB for metadata, section-lengths <= 8KB). Chunking text strings
    // is not worthwhile because it removes immediate zero-copy UTF-8 validation.
    /// Validated UTF-8 text string slice (major type 3).
    String(&'a str),
    /// Text string slice with invalid UTF-8 (when `allow_invalid_utf8` is
    /// enabled in `Config`).
    InvalidUtf8(&'a [u8]),
    /// Start of a byte string (major type 2). The payload is the total byte
    /// length.
    BytesStart(u64),
    /// Chunk of byte string data yielded while streaming.
    BytesChunk(&'a [u8]),
    /// End of the current byte string.
    BytesEnd,
    /// Start of an array (major type 4). The payload is the number of elements
    /// in the array.
    ArrayStart(u64),
    /// Start of a map (major type 5). The payload is the number of key-value
    /// pairs in the map.
    MapStart(u64),
    /// Boolean simple value (`true` or `false`).
    Boolean(bool),
    /// Null simple value (0x16).
    Null,
    /// Undefined simple value (0x17).
    Undefined,
    /// Parser has successfully completed parsing the root element.
    Done,
    /// The reader requires more data to yield the next event. The payload is
    /// the minimal amount of total bytes (from the start of the current event)
    /// required to proceed.
    NeedsMoreData(usize),
}

impl<'a> TryFrom<CborEvent<'a>> for Value<'a> {
    type Error = CborEvent<'a>;

    fn try_from(event: CborEvent<'a>) -> Result<Self, Self::Error> {
        match event {
            CborEvent::Int(v) => Ok(Value::Int(v)),
            CborEvent::String(s) => Ok(Value::String(s)),
            CborEvent::InvalidUtf8(b) => Ok(Value::InvalidUtf8(b)),
            CborEvent::Boolean(b) => Ok(Value::Boolean(b)),
            CborEvent::Null => Ok(Value::Null),
            CborEvent::Undefined => Ok(Value::Undefined),
            non_scalar @ (CborEvent::BytesStart(_)
            | CborEvent::BytesChunk(_)
            | CborEvent::BytesEnd
            | CborEvent::ArrayStart(_)
            | CborEvent::MapStart(_)
            | CborEvent::Done
            | CborEvent::NeedsMoreData(_)) => Err(non_scalar),
        }
    }
}

#[derive(Clone, Copy, PartialEq)]
enum DecoderState {
    Value,
    ReadingBytes(u64),
    Done,
}

trait SliceExt<'a> {
    fn read_array<const N: usize>(&mut self) -> Option<&'a [u8; N]>;
    fn read_bytes(&mut self, len: usize) -> Option<&'a [u8]>;
    fn read_some_bytes(&mut self, max_len: usize) -> Option<&'a [u8]>;
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

    fn read_some_bytes(&mut self, max_len: usize) -> Option<&'a [u8]> {
        let len = core::cmp::min(self.len(), max_len);
        if len == 0 {
            return None;
        }
        self.split_off(..len)
    }

    fn read_u8(&mut self) -> Option<u8> {
        self.split_off_first().copied()
    }
}

#[derive(Clone, Copy, PartialEq)]
enum OpenContainer {
    /// Number of remaining elements to parse in the array.
    Array(u64),
    /// Currently expecting a map key; holds the number of remaining pairs.
    MapInKey(u64),
    /// Currently expecting a map value; holds the number of remaining pairs.
    MapInValue(u64),
}

impl OpenContainer {
    fn array(num_elements: u64) -> Self {
        Self::Array(num_elements)
    }

    fn map(num_pairs: u64) -> Self {
        Self::MapInKey(num_pairs)
    }

    fn decrement(&mut self) {
        match self {
            Self::Array(count) => *count = count.saturating_sub(1),
            Self::MapInKey(pairs) => *self = Self::MapInValue(*pairs),
            Self::MapInValue(pairs) => *self = Self::MapInKey(pairs.saturating_sub(1)),
        }
    }

    fn is_complete(&self) -> bool {
        match self {
            Self::Array(count) => *count == 0,
            Self::MapInKey(pairs) => *pairs == 0,
            Self::MapInValue(_) => false,
        }
    }
}

fn decrement_top(stack: &mut Vec<OpenContainer>) {
    while stack
        .pop_if(|top| {
            top.decrement();
            top.is_complete()
        })
        .is_some()
    {}
}

/// Incremental, zero-copy CBOR decoder state machine providing the C++
/// `Decoder` interface.
///
/// Returned events (e.g. `String`, `BytesChunk`) borrow directly from the input
/// slice passed to `next_event`. Callers must copy extracted data if the
/// input buffer does not outlive its use. If `next_event` returns
/// `Error::IncompleteCborData`, the input slice is unconsumed; callers
/// must retain the remaining bytes and prepend/concatenate them with the next
/// chunk.
pub struct Decoder {
    state: DecoderState,
    /// Stack tracking open containers and remaining elements.
    nesting_stack: Vec<OpenContainer>,
    config: Config,
}

impl Default for Decoder {
    fn default() -> Self {
        Self::new()
    }
}

impl Decoder {
    fn parse_value<'a>(&mut self, data: &mut &'a [u8], depth: usize) -> Result<Value<'a>, Error> {
        if depth > self.config.max_nesting_level {
            return Err(Error::TooMuchNesting);
        }

        let event = match self.next_event(data) {
            Ok(CborEvent::NeedsMoreData(_)) => return Err(Error::IncompleteCborData),
            Ok(e) => e,
            Err(e) => return Err(e),
        };

        match event {
            CborEvent::BytesStart(_) => self.read_complete_bytestring(data).map(Value::Bytestring),
            CborEvent::ArrayStart(num_elements) => self.parse_array(data, num_elements, depth),
            CborEvent::MapStart(num_elements) => self.parse_map(data, num_elements, depth),
            CborEvent::BytesChunk(_)
            | CborEvent::BytesEnd
            | CborEvent::Done
            | CborEvent::NeedsMoreData(_) => {
                unreachable!("unexpected streaming event in one-pass parser")
            }
            leaf_event @ (CborEvent::Int(_)
            | CborEvent::String(_)
            | CborEvent::InvalidUtf8(_)
            | CborEvent::Boolean(_)
            | CborEvent::Null
            | CborEvent::Undefined) => Value::try_from(leaf_event)
                .map_err(|_| unreachable!("unexpected non-scalar event in leaf handler")),
        }
    }

    fn parse_array<'a>(
        &mut self,
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
        &mut self,
        data: &mut &'a [u8],
        num_elements: u64,
        depth: usize,
    ) -> Result<Value<'a>, Error> {
        let mut ret: Vec<MapEntry> = Vec::new();

        for _ in 0..num_elements {
            // TODO(crbug.com/259749095): Validate key type + order (and possibly return
            // early) before attempting to parse the value.
            let key_value = self.parse_value(data, depth + 1)?;
            let value = self.parse_value(data, depth + 1)?;

            let key = match MapKey::try_from(key_value) {
                Ok(key) => key,
                Err(Value::InvalidUtf8(_)) => return Err(Error::InvalidUtf8),
                Err(_) => return Err(Error::IncorrectMapKeyType),
            };

            if let Some(previous) = ret.last() {
                match previous.key.cmp(&key) {
                    Ordering::Less => {}
                    Ordering::Greater
                        if ret.binary_search_by_key(&&key, |entry| &entry.key).is_err() =>
                    {
                        return Err(Error::OutOfOrderKey);
                    }
                    // Covers `Ordering::Equal` and `Ordering::Greater` when the key is already
                    // in the map (e.g. an out-of-order duplicate).
                    _ => {
                        return Err(Error::DuplicateKey);
                    }
                }
            }

            ret.push(MapEntry { key, value });
        }
        Ok(Value::Map(Map::from_sorted_vec_unchecked(ret)))
    }

    pub fn read_complete_value<'a>(&mut self, data: &mut &'a [u8]) -> Result<Value<'a>, Error> {
        if self.state != DecoderState::Value {
            return Err(Error::UnknownError);
        }

        let original_data = *data;
        let original_state = self.state;
        let original_nesting_stack = self.nesting_stack.clone();

        match self.parse_value(data, 0) {
            Ok(val) => Ok(val),
            Err(e) => {
                *data = original_data;
                self.state = original_state;
                self.nesting_stack = original_nesting_stack;
                Err(e)
            }
        }
    }

    pub fn read_complete_bytestring<'a>(&mut self, data: &mut &'a [u8]) -> Result<&'a [u8], Error> {
        let DecoderState::ReadingBytes(len) = self.state else {
            return Err(Error::UnknownError);
        };
        if len > data.len() as u64 {
            return Err(Error::IncompleteCborData);
        }
        let bytes = data.read_bytes(len as usize).ok_or(Error::IncompleteCborData)?;
        self.state = DecoderState::Value;
        Ok(bytes)
    }

    fn read_arg<const N: usize, T: FromBytes<N>>(
        &mut self,
        data: &mut &[u8],
    ) -> Result<u64, Error> {
        let arr = data.read_array::<N>().ok_or(Error::IncompleteCborData)?;
        let v = T::from_be_bytes(*arr).into();
        let (_, expected_num_bytes) = crate::writer::low_bits_and_length(v);
        if N != expected_num_bytes {
            Err(Error::NonMinimalCborEncoding)
        } else {
            Ok(v)
        }
    }

    pub fn next_event<'a>(&mut self, data: &mut &'a [u8]) -> Result<CborEvent<'a>, Error> {
        if self.state == DecoderState::Done {
            if data.is_empty() {
                return Ok(CborEvent::Done);
            } else {
                return Err(Error::ExtraneousData);
            }
        }

        let mut remaining = *data;
        let original_state = self.state;
        let event = match self.parse_single_event_without_nesting_updates(&mut remaining) {
            Ok(e) => e,
            Err(e) => {
                self.state = original_state;
                return Err(e);
            }
        };

        if let CborEvent::NeedsMoreData(_) = event {
            self.state = original_state;
            return Ok(event);
        }

        if let CborEvent::Done = event {
            self.state = DecoderState::Done;
            *data = remaining;
            return Ok(event);
        }

        self.update_state(&event, self.state)?;

        if !matches!(self.state, DecoderState::ReadingBytes(_)) && self.nesting_stack.is_empty() {
            self.state = DecoderState::Done;
        }

        *data = remaining;
        Ok(event)
    }

    fn parse_single_event_without_nesting_updates<'a>(
        &mut self,
        data: &mut &'a [u8],
    ) -> Result<CborEvent<'a>, Error> {
        match self.state {
            DecoderState::Done => Ok(CborEvent::Done),
            DecoderState::ReadingBytes(0) => {
                self.state = DecoderState::Value;
                Ok(CborEvent::BytesEnd)
            }
            DecoderState::ReadingBytes(remaining) => {
                // On 64-bit systems, `usize::try_from(remaining)` is infallible. On 32-bit
                // systems, clamping to `usize::MAX` allows chunked streaming of byte strings
                // larger than 4GB.
                let to_read = usize::try_from(remaining).unwrap_or(usize::MAX);
                let chunk = match data.read_some_bytes(to_read) {
                    Some(c) => c,
                    None => return Ok(CborEvent::NeedsMoreData(1)),
                };
                let len = chunk.len() as u64;
                if len == 0 {
                    return Ok(CborEvent::NeedsMoreData(1));
                }
                let new_remaining = remaining.checked_sub(len).ok_or(Error::IncompleteCborData)?;
                self.state = DecoderState::ReadingBytes(new_remaining);
                Ok(CborEvent::BytesChunk(chunk))
            }
            DecoderState::Value => {
                let b = match data.read_u8() {
                    Some(b) => b,
                    None => return Ok(CborEvent::NeedsMoreData(1)),
                };
                let major_type = b >> 5;
                let info = b & 0x1f;

                let (arg, arg_len) = match (major_type, info) {
                    (_, 0..=ADDL_INFO_DIRECT_MAX) => (info as u64, 0),
                    (_, ADDL_INFO_1_BYTE) => match self.read_arg::<1, u8>(data) {
                        Ok(v) => (v, 1),
                        Err(Error::IncompleteCborData) => return Ok(CborEvent::NeedsMoreData(2)),
                        Err(e) => return Err(e),
                    },
                    (_, ADDL_INFO_2_BYTES) => match self.read_arg::<2, u16>(data) {
                        Ok(v) => (v, 2),
                        Err(Error::IncompleteCborData) => return Ok(CborEvent::NeedsMoreData(3)),
                        Err(e) => return Err(e),
                    },
                    (_, ADDL_INFO_4_BYTES) => match self.read_arg::<4, u32>(data) {
                        Ok(v) => (v, 4),
                        Err(Error::IncompleteCborData) => return Ok(CborEvent::NeedsMoreData(5)),
                        Err(e) => return Err(e),
                    },
                    (_, ADDL_INFO_8_BYTES) => match self.read_arg::<8, u64>(data) {
                        Ok(v) => (v, 8),
                        Err(Error::IncompleteCborData) => return Ok(CborEvent::NeedsMoreData(9)),
                        Err(e) => return Err(e),
                    },
                    _ => return Err(Error::UnknownAdditionalInfo),
                };

                match major_type {
                    MAJOR_TYPE_UNSIGNED_INT => {
                        let val = i64::try_from(arg).map_err(|_| Error::OutOfRangeIntegerValue)?;
                        Ok(CborEvent::Int(val))
                    }
                    MAJOR_TYPE_NEGATIVE_INT => {
                        let val = i64::try_from(arg).map_err(|_| Error::OutOfRangeIntegerValue)?;
                        Ok(CborEvent::Int(!val))
                    }
                    MAJOR_TYPE_BYTE_STRING => {
                        self.state = DecoderState::ReadingBytes(arg);
                        Ok(CborEvent::BytesStart(arg))
                    }
                    MAJOR_TYPE_TEXT_STRING => {
                        let len = usize::try_from(arg).map_err(|_| Error::IncompleteCborData)?;
                        let bytes = match data.read_bytes(len) {
                            Some(b) => b,
                            None => return Ok(CborEvent::NeedsMoreData(1 + arg_len + len)),
                        };
                        match core::str::from_utf8(bytes) {
                            Ok(s) => Ok(CborEvent::String(s)),
                            Err(_) if self.config.allow_invalid_utf8 => {
                                Ok(CborEvent::InvalidUtf8(bytes))
                            }
                            Err(_) => Err(Error::InvalidUtf8),
                        }
                    }
                    MAJOR_TYPE_ARRAY => Ok(CborEvent::ArrayStart(arg)),
                    MAJOR_TYPE_MAP => Ok(CborEvent::MapStart(arg)),
                    MAJOR_TYPE_SIMPLE_VALUE => match info {
                        SIMPLE_VALUE_FALSE => Ok(CborEvent::Boolean(false)),
                        SIMPLE_VALUE_TRUE => Ok(CborEvent::Boolean(true)),
                        SIMPLE_VALUE_NULL => Ok(CborEvent::Null),
                        SIMPLE_VALUE_UNDEFINED => Ok(CborEvent::Undefined),
                        SIMPLE_VALUE_FLOAT_16..=SIMPLE_VALUE_FLOAT_64 => {
                            Err(Error::UnsupportedFloatingPointValue)
                        }
                        _ => Err(Error::UnsupportedSimpleValue),
                    },
                    _ => Err(Error::UnsupportedMajorType),
                }
            }
        }
    }

    pub fn new() -> Self {
        Self::with_config(Config::default())
    }

    pub fn with_config(config: Config) -> Self {
        Self { state: DecoderState::Value, nesting_stack: Vec::new(), config }
    }

    /// Returns true if the parser has exactly and structurally completed
    /// parsing the top-level CBOR element and its children without
    /// trailing/missing data.
    pub fn is_complete(&self) -> bool {
        self.state == DecoderState::Done
    }

    fn update_state(
        &mut self,
        event: &CborEvent<'_>,
        decoder_state: DecoderState,
    ) -> Result<(), Error> {
        match event {
            CborEvent::BytesChunk(_) => {
                self.state = decoder_state;
                return Ok(());
            }
            CborEvent::BytesEnd => {}
            CborEvent::ArrayStart(size) => {
                if *size > 0 {
                    if self.nesting_stack.len() >= self.config.max_nesting_level {
                        return Err(Error::TooMuchNesting);
                    }
                    self.nesting_stack.push(OpenContainer::array(*size));
                } else {
                    decrement_top(&mut self.nesting_stack);
                }
            }
            CborEvent::MapStart(size) => {
                if *size > 0 {
                    if self.nesting_stack.len() >= self.config.max_nesting_level {
                        return Err(Error::TooMuchNesting);
                    }
                    self.nesting_stack.push(OpenContainer::map(*size));
                } else {
                    decrement_top(&mut self.nesting_stack);
                }
            }
            CborEvent::Int(_)
            | CborEvent::String(_)
            | CborEvent::InvalidUtf8(_)
            | CborEvent::BytesStart(_)
            | CborEvent::Boolean(_)
            | CborEvent::Null
            | CborEvent::Undefined => {
                decrement_top(&mut self.nesting_stack);
            }
            CborEvent::Done | CborEvent::NeedsMoreData(_) => {}
        }

        self.state = if self.nesting_stack.is_empty() && decoder_state == DecoderState::Value {
            DecoderState::Done
        } else {
            decoder_state
        };

        Ok(())
    }
}
