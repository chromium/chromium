// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::constants::{
    BUNDLE_MAGIC_BYTES, DEPRECATED_B1_TOP_LEVEL_ARRAY_SIZE, MAX_CBOR_ITEM_HEADER_SIZE,
    MAX_SECTION_LENGTHS_CBOR_SIZE, TOP_LEVEL_ARRAY_SIZE, TRAILING_LENGTH_NUM_BYTES,
    VERSION_B1_BYTES, VERSION_B2_BYTES,
};
use crate::types::{MagicAndVersionResult, ParseError};

/// Helper for incremental, sequential CBOR decoding with uniform error
/// reporting.
struct BundleDecoder<'a> {
    decoder: cbor::Decoder,
    slice: &'a [u8],
    orig_len: usize,
}

impl<'a> BundleDecoder<'a> {
    fn new(data: &'a [u8]) -> Self {
        Self { decoder: cbor::Decoder::new(), slice: data, orig_len: data.len() }
    }

    fn bytes_consumed(&self) -> usize {
        self.orig_len - self.slice.len()
    }

    fn expect_bytes_start(&mut self, err_msg: &'static str) -> Result<u64, ParseError> {
        match self.decoder.next_event(&mut self.slice) {
            Ok(cbor::CborEvent::BytesStart(n)) => Ok(n),
            _ => Err(ParseError::format(err_msg)),
        }
    }
}

fn parse_magic_bytes(decoder: &mut BundleDecoder) -> Result<(), ParseError> {
    match decoder.decoder.read_complete_value(&mut decoder.slice) {
        Ok(cbor::Value::Bytestring(BUNDLE_MAGIC_BYTES)) => Ok(()),
        _ => Err(ParseError::format("Wrong magic bytes.")),
    }
}

fn parse_version(decoder: &mut BundleDecoder) -> Result<(), ParseError> {
    match decoder.decoder.read_complete_value(&mut decoder.slice) {
        Ok(cbor::Value::Bytestring(VERSION_B2_BYTES)) => Ok(()),
        Ok(cbor::Value::Bytestring(VERSION_B1_BYTES)) => Err(ParseError::version(
            "Bundle format version is 'b1' which is no longer supported. Currently supported version is: 'b2'",
        )),
        Ok(cbor::Value::Bytestring(_)) => Err(ParseError::version(
            "Version error: bundle format does not correspond to the specifed version. Currently supported version is: 'b2'",
        )),
        _ => Err(ParseError::format("Cannot read version bytes.")),
    }
}

/// Parses the 8-byte big-endian trailing length field at the end of a bundle
/// and returns the bundle's start offset within the file.
///
/// `data` must contain exactly 8 bytes corresponding to the trailing length.
/// Passing a slice of any length other than 8 will result in an error.
///
/// https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-trailing-length
///
/// "Recipients loading the bundle in a random-access context SHOULD start by
/// reading the last 8 bytes and seeking backwards by that many bytes to find
/// the start of the bundle, instead of assuming that the start of the file
/// is also the start of the bundle. This allows the bundle to be appended to
/// another format such as a generic self-extracting executable."
pub fn parse_trailing_length(data: &[u8], file_length: u64) -> Result<u64, ParseError> {
    // The trailing `length` field is itself part of the bundle it measures, so
    // a bundle cannot be shorter than that field (and cannot be empty).
    const MIN_BUNDLE_LENGTH: u64 = TRAILING_LENGTH_NUM_BYTES as u64;
    match <[u8; TRAILING_LENGTH_NUM_BYTES]>::try_from(data).map(u64::from_be_bytes) {
        // The bundle must also fit within the file.
        Ok(len @ MIN_BUNDLE_LENGTH..) if len <= file_length => Ok(file_length - len),
        Ok(_) => Err(ParseError::format("Invalid bundle length.")),
        Err(_) => Err(ParseError::format("Error reading bundle length.")),
    }
}

/// Parses the initial header of the Web Bundle (top-level array header,
/// magic bytes, version bytes, and section-lengths byte string header).
///
/// https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-top-level-structure
///
/// Top-level CDDL structure:
/// ```text
/// webbundle = [
///    magic: h'F0 9F 8C 90 F0 9F 93 A6',
///    version: bytes .size 4,
///    section-lengths: bytes .cbor section-lengths,
///    sections: [* any ],
///    length: bytes .size 8,  ; Big-endian number of bytes in the bundle.
/// ]
/// ```
///
/// Validates:
/// 1. Array size byte is `0x85` (Array of 5 items; `0x86` is also tolerated
///    temporarily to surface a specific deprecation message for `b1` bundles).
/// 2. Magic byte string matches `F0 9F 8C 90 F0 9F 93 A6` ("🌐📦").
/// 3. Version byte string is `b2\0\0`.
/// 4. Section-lengths byte string length is less than 8192 bytes.
pub fn parse_magic_and_version(
    data: &[u8],
    current_offset: u64,
) -> Result<MagicAndVersionResult, ParseError> {
    if data.is_empty() {
        return Err(ParseError::format("Missing CBOR array size byte."));
    }

    let mut decoder = BundleDecoder::new(data);

    let array_size = match decoder.decoder.next_event(&mut decoder.slice) {
        Ok(cbor::CborEvent::ArrayStart(
            size @ (TOP_LEVEL_ARRAY_SIZE | DEPRECATED_B1_TOP_LEVEL_ARRAY_SIZE),
        )) => size,
        Ok(cbor::CborEvent::ArrayStart(_)) => {
            return Err(ParseError::format("Wrong CBOR array size of the top-level structure"));
        }
        _ => return Err(ParseError::format("Wrong magic bytes.")),
    };

    parse_magic_bytes(&mut decoder)?;
    parse_version(&mut decoder)?;

    // The version is 'b2' at this point, so a top-level array of six items
    // (only ever valid for the deprecated 'b1' format) is malformed.
    if array_size != TOP_LEVEL_ARRAY_SIZE {
        return Err(ParseError::format("Wrong CBOR array size of the top-level structure"));
    }

    let section_lengths_len =
        decoder.expect_bytes_start("Cannot parse the size of section-lengths.")?;
    if section_lengths_len >= MAX_SECTION_LENGTHS_CBOR_SIZE {
        return Err(ParseError::format(
            "The section-lengths CBOR must be smaller than 8192 bytes.",
        ));
    }

    let Ok(consumed_bytes) = u64::try_from(decoder.bytes_consumed()) else {
        return Err(ParseError::format("Offset conversion overflow."));
    };
    let Some(next_read_offset) = current_offset.checked_add(consumed_bytes) else {
        return Err(ParseError::format("Integer overflow calculating next read offset."));
    };
    let Some(next_read_length) = section_lengths_len.checked_add(MAX_CBOR_ITEM_HEADER_SIZE) else {
        return Err(ParseError::format("Integer overflow calculating next read length."));
    };

    Ok(MagicAndVersionResult { section_lengths_len, next_read_offset, next_read_length })
}
