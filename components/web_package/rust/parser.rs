// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use alloc::collections::BTreeSet;
use alloc::string::ToString;
use alloc::vec::Vec;

use crate::constants::{
    BUNDLE_MAGIC_BYTES, CRITICAL_SECTION, DEPRECATED_B1_TOP_LEVEL_ARRAY_SIZE, INDEX_SECTION,
    MAX_CBOR_ITEM_HEADER_SIZE, MAX_RESPONSE_HEADER_LENGTH, MAX_SECTION_LENGTHS_CBOR_SIZE,
    PRIMARY_SECTION, RESPONSES_SECTION, RESPONSE_ARRAY_SIZE, TOP_LEVEL_ARRAY_SIZE,
    TRAILING_LENGTH_NUM_BYTES, VERSION_B1_BYTES, VERSION_B2_BYTES,
};
use crate::http::{is_valid_header_name, is_valid_header_value};
use crate::types::{
    BundleHeaderResult, HeaderEntry, MagicAndVersionResult, ParseError, ParsedIndexEntry,
    ResponseParseResult, SectionOffsetEntry,
};

fn is_metadata_section(name: &str) -> bool {
    matches!(name, CRITICAL_SECTION | INDEX_SECTION | PRIMARY_SECTION)
}

fn is_known_section(name: &str) -> bool {
    is_metadata_section(name) || name == RESPONSES_SECTION
}

/// Helper for incremental, sequential CBOR decoding with uniform error
/// reporting.
///
/// Encapsulates `cbor::Decoder` and a data slice, providing combinators that
/// match expected CBOR events while mapping both decoding errors and unexpected
/// event types to a single, consistent `ParseError`.
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

    fn expect_array_start(&mut self, err_msg: &'static str) -> Result<u64, ParseError> {
        match self.decoder.next_event(&mut self.slice) {
            Ok(cbor::CborEvent::ArrayStart(n)) => Ok(n),
            _ => Err(ParseError::format(err_msg)),
        }
    }

    fn expect_exact_array_start(
        &mut self,
        count: u64,
        err_msg: &'static str,
    ) -> Result<(), ParseError> {
        match self.decoder.next_event(&mut self.slice) {
            Ok(cbor::CborEvent::ArrayStart(n)) if n == count => Ok(()),
            _ => Err(ParseError::format(err_msg)),
        }
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

/// Helper that parses a single complete CBOR value from `data`, ensuring all
/// bytes are consumed.
fn parse_exact_cbor<'a>(data: &'a [u8]) -> Result<cbor::Value<'a>, cbor::Error> {
    match cbor::parse_with_config(data, cbor::Config::default()) {
        Ok(res) if res.bytes_consumed == data.len() => Ok(res.value),
        Ok(_) => Err(cbor::Error::ExtraneousData),
        Err(err) => Err(err),
    }
}

/// Parses a metadata section's CBOR, formatting CBOR decoder errors with the
/// section error prefix.
fn parse_section_cbor<'a, T>(
    data: &'a [u8],
    matcher: impl FnOnce(cbor::Value<'a>) -> Result<T, ParseError>,
) -> Result<T, ParseError> {
    parse_exact_cbor(data)
        .map_err(|_| ParseError::format("Cannot parse section contents as CBOR."))
        .and_then(matcher)
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

fn parse_section_lengths<'a>(bytes: &'a [u8]) -> Result<Vec<cbor::Value<'a>>, ParseError> {
    match parse_exact_cbor(bytes) {
        // Each section is represented as a (name, length) pair, so the array length must be even.
        Ok(cbor::Value::Array(arr)) if arr.len() % 2 == 0 => Ok(arr),
        _ => Err(ParseError::format("Cannot parse section-lengths.")),
    }
}

fn parse_sections_start(
    remaining: &[u8],
    expected_count: u64,
    current_offset: u64,
    section_lengths_len: u64,
) -> Result<u64, ParseError> {
    let mut decoder = BundleDecoder::new(remaining);
    if expected_count != decoder.expect_array_start("Cannot parse the number of sections.")? {
        return Err(ParseError::format("Unexpected number of sections."));
    }
    let Ok(consumed_bytes) = u64::try_from(decoder.bytes_consumed()) else {
        return Err(ParseError::format("Offset conversion overflow."));
    };
    let Some(start) = current_offset
        .checked_add(section_lengths_len)
        .and_then(|off| off.checked_add(consumed_bytes))
    else {
        return Err(ParseError::format("Integer overflow calculating section offsets."));
    };
    Ok(start)
}

fn parse_section_entry<'a>(chunk: &[cbor::Value<'a>]) -> Result<(&'a str, u64), ParseError> {
    match chunk {
        &[cbor::Value::String(name), cbor::Value::Int(len @ 0..)] => Ok((name, len as u64)),
        _ => Err(ParseError::format("Cannot parse section-lengths.")),
    }
}

fn parse_section_offsets(
    raw_sections: &[cbor::Value],
    mut cur_offset: u64,
) -> Result<(Vec<SectionOffsetEntry>, u64, u64), ParseError> {
    let mut seen_names = BTreeSet::new();
    let mut metadata_sections = Vec::new();
    let mut responses_info = None;

    for chunk in raw_sections.chunks_exact(2) {
        let (name, len) = parse_section_entry(chunk)?;

        if !seen_names.insert(name) {
            return Err(ParseError::format("Duplicated section."));
        }

        match name {
            RESPONSES_SECTION => responses_info = Some((cur_offset, len)),
            _ if is_metadata_section(name) => {
                metadata_sections.push(SectionOffsetEntry {
                    name: name.to_string(),
                    offset: cur_offset,
                    length: len,
                });
            }
            // Unknown or extension sections are not read as metadata sections;
            // if any are unrecognized but listed in "critical", parse_critical_section will fail
            // later.
            _ => {}
        }

        cur_offset = cur_offset
            .checked_add(len)
            .ok_or_else(|| ParseError::format("Integer overflow calculating section offsets."))?;
    }

    let (responses_offset, responses_length) =
        match raw_sections.chunks_exact(2).last().map(parse_section_entry) {
            Some(Ok((RESPONSES_SECTION, _))) => responses_info.ok_or_else(|| {
                ParseError::format("Responses section is not the last in section-lengths.")
            })?,
            Some(Err(err)) => return Err(err),
            _ => {
                return Err(ParseError::format(
                    "Responses section is not the last in section-lengths.",
                ))
            }
        };

    Ok((metadata_sections, responses_offset, responses_length))
}

/// Parses the `section-lengths` payload and the `sections` array header,
/// validating sections and computing their offsets.
///
/// https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-bundle-sections
///
/// CDDL structure:
/// ```text
/// section-lengths = [* (section-name: tstr, length: uint) ]
/// sections: [* any ]
/// ```
///
/// Validates:
/// 1. Section lengths array has an even number of elements (`[name, length,
///    ...]`).
/// 2. `sections` array count in CBOR matches exactly the number of sections
///    specified in `section-lengths`: "The sections array contains the
///    sections' content. The length of this array MUST be exactly half the
///    length of the section-lengths array, and parsers MUST NOT load any data
///    if that is not the case."
/// 3. Section names are not duplicated.
/// 4. Section offsets do not experience integer overflow.
/// 5. The "responses" section is the last section in `section-lengths`: "The
///    'responses' section MUST appear after the other three sections defined
///    here, and parsers MUST NOT load any data if that is not the case."
pub fn parse_bundle_header(
    data: &[u8],
    section_lengths_len: u64,
    current_offset: u64,
) -> Result<BundleHeaderResult, ParseError> {
    let (section_lengths_bytes, remaining) = match usize::try_from(section_lengths_len) {
        Ok(len) if len <= data.len() => data.split_at(len),
        _ => return Err(ParseError::format("Cannot read section-lengths.")),
    };

    let section_lengths = parse_section_lengths(section_lengths_bytes)?;
    let num_sections = (section_lengths.len() / 2) as u64;
    let sections_start =
        parse_sections_start(remaining, num_sections, current_offset, section_lengths_len)?;

    let (metadata_sections, responses_offset, responses_length) =
        parse_section_offsets(&section_lengths, sections_start)?;

    Ok(BundleHeaderResult { metadata_sections, responses_offset, responses_length })
}

fn parse_response_location(value: &cbor::Value<'_>) -> Result<(u64, u64), ParseError> {
    let cbor::Value::Array(arr) = value else {
        return Err(ParseError::format("Index section: value must be an array."));
    };
    match arr.as_slice() {
        [cbor::Value::Int(offset @ 0..), cbor::Value::Int(length @ 0..)] => {
            Ok((*offset as u64, *length as u64))
        }
        [_, _] => {
            Err(ParseError::format("Index section: offset and length values must be unsigned."))
        }
        _ => Err(ParseError::format(
            "Index section: the size of a response array per URL should be exactly 2.",
        )),
    }
}

fn parse_index_entry<'a>(
    key: cbor::MapKey<'a>,
    value: &cbor::Value<'_>,
    responses_offset: u64,
    responses_length: u64,
) -> Result<ParsedIndexEntry<'a>, ParseError> {
    let cbor::MapKey::String(url) = key else {
        return Err(ParseError::format("Index section: key must be a string."));
    };
    let (offset, length) = parse_response_location(value)?;
    match (offset.checked_add(length), responses_offset.checked_add(offset)) {
        (Some(end), Some(offset_within_stream)) if end <= responses_length => {
            Ok(ParsedIndexEntry { url, offset: offset_within_stream, length })
        }
        _ => Err(ParseError::format("Index section: response out of range.")),
    }
}

/// Parses the `index` metadata section, returning entries with absolute
/// payload offsets into the responses section.
///
/// https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-the-index-section
///
/// The index section has the following structure:
///   index = {* whatwg-url => [ location-in-responses ] }
///   location-in-responses = (offset: uint, length: uint)
pub fn parse_index_section<'a>(
    data: &'a [u8],
    responses_offset: u64,
    responses_length: u64,
) -> Result<Vec<ParsedIndexEntry<'a>>, ParseError> {
    parse_section_cbor(data, |val| {
        let cbor::Value::Map(map) = val else {
            return Err(ParseError::format("Index section must be a map."));
        };
        map.into_iter()
            .map(|cbor::MapEntry { key, value }| {
                parse_index_entry(key, &value, responses_offset, responses_length)
            })
            .collect()
    })
}

fn validate_critical_element(elem: &cbor::Value<'_>) -> Result<(), ParseError> {
    match elem {
        cbor::Value::String(name) if is_known_section(name) => Ok(()),
        cbor::Value::String(_) => Err(ParseError::format("Unknown critical section.")),
        _ => Err(ParseError::format("Non-string element in the critical section.")),
    }
}

/// Parses and validates the `critical` metadata section.
///
/// https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#critical-section
///
///   critical = [*tstr]
///
/// "If the client has not implemented a section named by one of the items in
/// this list, the client MUST fail to parse the bundle as a whole."
// Uses `Result<bool, ...>` instead of `Result<(), ...>` because Crubit does not
// yet support generating bindings for the unit type `()` (b/259749095).
pub fn parse_critical_section(data: &[u8]) -> Result<bool, ParseError> {
    parse_section_cbor(data, |val| {
        let cbor::Value::Array(arr) = val else {
            return Err(ParseError::format("Critical section must be an array."));
        };
        for elem in &arr {
            validate_critical_element(elem)?;
        }
        Ok(true)
    })
}

/// Parses the `primary` metadata section, returning the primary URL string.
///
/// https://github.com/WICG/webpackage/blob/main/extensions/primary-section.md
///
///   primary = whatwg-url
pub fn parse_primary_section(data: &[u8]) -> Result<&str, ParseError> {
    parse_section_cbor(data, |val| {
        let cbor::Value::String(url) = val else {
            return Err(ParseError::format("Primary section must be a string."));
        };
        Ok(url)
    })
}

fn err_cannot_parse_headers() -> ParseError {
    ParseError::format("Cannot parse response headers.")
}

fn err_invalid_pseudo_header() -> ParseError {
    ParseError::format("Response headers map must have exactly one pseudo-header, :status.")
}

fn err_unexpected_payload_length() -> ParseError {
    ParseError::format("Unexpected payload length.")
}

/// Parses the `:status` pseudo-header value into an HTTP status code integer.
/// The value must consist of exactly 3 ASCII decimal digits.
fn parse_status_code(v: &[u8]) -> Result<i32, ParseError> {
    match v {
        &[d0 @ b'0'..=b'9', d1 @ b'0'..=b'9', d2 @ b'0'..=b'9'] => {
            // Convert 3 ASCII decimal digit bytes into a 3-digit integer status code.
            Ok((d0 - b'0') as i32 * 100 + (d1 - b'0') as i32 * 10 + (d2 - b'0') as i32)
        }
        _ => Err(ParseError::format(":status must be 3 ASCII decimal digits.")),
    }
}

/// Parses a response from the responses section, checking buffer adequacy,
/// extracting and validating HTTP headers, and computing payload boundaries.
///
/// https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-responses
///
/// CDDL structure:
/// ```text
/// responses = [*response]
/// response = [headers: bstr .cbor headers, payload: bstr]
/// headers = {* bstr => bstr}
/// ```
///
/// Validates:
/// 1. Array size of response is exactly 2 (headers byte string, payload byte
///    string).
/// 2. Header byte string length is less than 512KB (524,288 bytes): "The length
///    of the headers byte string in a response MUST be less than 524288
///    (512*1024) bytes, and recipients MUST fail to load a response with longer
///    headers"
/// 3. Verifies if the provided buffer contains enough data for the headers and
///    the CBOR header of the payload. If insufficient, returns
///    `ResponseParseResult::needs_more`.
/// 4. Headers map contains exactly one `:status` pseudo-header consisting of 3
///    ASCII digits: "Each response's headers MUST include a :status
///    pseudo-header with exactly 3 ASCII decimal digits and MUST NOT include
///    any other pseudo-headers."
/// 5. Validates header field names and values (per RFC 9110, RFC 9112, and RFC
///    9113; see `http.rs`).
/// 6. Non-empty payload requires a `content-type` header: "If a response's
///    payload is not empty, its headers MUST include a Content-Type header
///    (Section 8.3 of RFC 9110, https://www.rfc-editor.org/rfc/rfc9110.html#section-8.3)."
/// 7. Consumed header bytes + payload length equals `response_length`.
pub fn parse_response<'a>(
    data: &'a [u8],
    response_offset: u64,
    response_length: u64,
) -> Result<ResponseParseResult<'a>, ParseError> {
    let mut decoder = BundleDecoder::new(data);
    decoder.expect_exact_array_start(RESPONSE_ARRAY_SIZE, "Array size of response must be 2.")?;

    let header_len = decoder.expect_bytes_start("Cannot parse response header length.")?;
    if header_len >= MAX_RESPONSE_HEADER_LENGTH {
        return Err(ParseError::format("Response header is too big."));
    }

    let consumed_so_far = decoder.bytes_consumed();
    let required_buffer_size = (consumed_so_far as u64)
        .saturating_add(header_len)
        .saturating_add(MAX_CBOR_ITEM_HEADER_SIZE)
        .min(response_length);
    if (data.len() as u64) < required_buffer_size {
        return Ok(ResponseParseResult::needs_more(required_buffer_size));
    }

    let header_bytes = decoder
        .decoder
        .read_complete_bytestring(&mut decoder.slice)
        .map_err(|_| ParseError::format("Cannot read response headers."))?;

    let Ok(cbor::Value::Map(map)) = parse_exact_cbor(header_bytes) else {
        return Err(err_cannot_parse_headers());
    };

    let mut status_code = None;
    let mut headers = Vec::with_capacity(map.len().saturating_sub(1));
    let mut has_content_type = false;

    for cbor::MapEntry { key, value } in map {
        let cbor::MapKey::Bytestring(k) = key else {
            return Err(err_cannot_parse_headers());
        };
        let cbor::Value::Bytestring(v) = value else {
            return Err(err_cannot_parse_headers());
        };

        match k {
            // HTTP/2 pseudo-header fields start with ':' per RFC 9113 Section 8.3.
            // Each response's headers MUST include exactly one :status pseudo-header
            // and MUST NOT include any other pseudo-headers.
            b":status" if status_code.is_none() => {
                status_code = Some(parse_status_code(v)?);
            }
            [b':', ..] => return Err(err_invalid_pseudo_header()),
            _ => {
                if !is_valid_header_name(k) || !is_valid_header_value(v) {
                    return Err(err_cannot_parse_headers());
                }
                has_content_type |= k == b"content-type";
                headers.push(HeaderEntry { name: k, value: v });
            }
        }
    }

    let Some(status_code) = status_code else {
        return Err(err_invalid_pseudo_header());
    };

    let payload_len = decoder.expect_bytes_start("Cannot parse response payload length.")?;
    if payload_len > 0 && !has_content_type {
        return Err(ParseError::format("Non-empty response must have a content-type header."));
    }

    let consumed_header_bytes = decoder.bytes_consumed() as u64;
    if consumed_header_bytes.checked_add(payload_len) != Some(response_length) {
        return Err(err_unexpected_payload_length());
    }

    let Some(payload_offset) = response_offset.checked_add(consumed_header_bytes) else {
        return Err(err_unexpected_payload_length());
    };

    Ok(ResponseParseResult::success(status_code, headers, payload_offset, payload_len))
}
