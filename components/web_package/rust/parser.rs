// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::constants::TRAILING_LENGTH_NUM_BYTES;
use crate::types::ParseError;

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
