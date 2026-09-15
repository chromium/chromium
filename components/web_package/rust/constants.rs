// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// The number of bytes used to specify the length of the web bundle.
/// https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-trailing-length
pub const TRAILING_LENGTH_NUM_BYTES: usize = 8;

/// The maximum size of the section-lengths CBOR item.
/// https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-top-level-structure
pub(crate) const MAX_SECTION_LENGTHS_CBOR_SIZE: u64 = 8192;

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
