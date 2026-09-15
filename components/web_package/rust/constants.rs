// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// The number of bytes used to specify the length of the web bundle.
/// https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-trailing-length
pub const TRAILING_LENGTH_NUM_BYTES: usize = 8;

/// The maximum size of the section-lengths CBOR item.
/// https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-top-level-structure
/// "The section-lengths array is embedded in a byte string to facilitate
/// reading it from a network. This byte string MUST be less than 8192
/// (8*1024) bytes long, and parsers MUST NOT load any data from a
/// section-lengths item longer than this."
pub(crate) const MAX_SECTION_LENGTHS_CBOR_SIZE: u64 = 8192;

/// The maximum size of a metadata section allowed in this implementation.
pub const MAX_METADATA_SECTION_SIZE: u64 = 1024 * 1024;

/// The maximum size of the response header CBOR.
/// https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-responses
/// "The length of the headers byte string in a response MUST be less than
/// 524288 (512*1024) bytes, and recipients MUST fail to load a response with
/// longer headers"
pub const MAX_RESPONSE_HEADER_LENGTH: u64 = 512 * 1024;

/// The number of items in a response array (headers byte string, payload byte
/// string). https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-responses
pub(crate) const RESPONSE_ARRAY_SIZE: u64 = 2;

/// The initial buffer size for reading an item from the response section.
pub const INITIAL_BUFFER_SIZE_FOR_RESPONSE: u64 = 4096;

/// Initial buffer size for reading magic bytes and top-level headers (24
/// bytes): 1 byte (array header) + 9 bytes (magic byte string) + 5 bytes
/// (version byte string) + 9 bytes (max section-lengths CBOR header).
pub const INITIAL_BUNDLE_HEADER_BUFFER_SIZE: u64 = 24;

/// The maximum number of bytes needed for a CBOR item type and length header.
pub(crate) const MAX_CBOR_ITEM_HEADER_SIZE: u64 = 9;

/// The number of items in the top-level WebBundle array for version b2.
/// https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-top-level-structure
pub(crate) const TOP_LEVEL_ARRAY_SIZE: u64 = 5;

/// The number of items in the top-level WebBundle array for deprecated version
/// b1. Tolerated temporarily during initial header parsing to provide a
/// specific deprecation error message for b1 bundles.
pub(crate) const DEPRECATED_B1_TOP_LEVEL_ARRAY_SIZE: u64 = 6;

/// Byte contents of the magic string "🌐📦" (8 bytes in UTF-8).
///
/// The first 10 bytes of the web bundle format are:
/// ```text
///   85                             -- Array of length 5
///      48                          -- Byte string of length 8
///         F0 9F 8C 90 F0 9F 93 A6  -- "🌐📦" in UTF-8
/// ```
pub(crate) const BUNDLE_MAGIC_BYTES: &[u8] = "🌐📦".as_bytes();

/// CBOR payload of the version string "b2\0\0" (4 bytes).
/// ```text
///   44               -- Byte string of length 4
///       62 32 00 00  -- "b2\0\0"
/// ```
pub(crate) const VERSION_B2_BYTES: &[u8] = b"b2\0\0";

/// CBOR payload of the version string "b1\0\0" (4 bytes).
/// ```text
///   44               -- Byte string of length 4
///       62 31 00 00  -- "b1\0\0"
/// ```
pub(crate) const VERSION_B1_BYTES: &[u8] = b"b1\0\0";

/// Section names.
/// https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-bundle-sections
pub const CRITICAL_SECTION: &str = "critical";
pub const INDEX_SECTION: &str = "index";
pub const PRIMARY_SECTION: &str = "primary";
pub const RESPONSES_SECTION: &str = "responses";
