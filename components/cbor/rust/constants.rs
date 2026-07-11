// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CBOR Major Types (RFC 8949 Section 3.1)
// https://datatracker.ietf.org/doc/html/rfc8949#section-3.1
pub(crate) const MAJOR_TYPE_UNSIGNED_INT: u8 = 0;
pub(crate) const MAJOR_TYPE_NEGATIVE_INT: u8 = 1;
pub(crate) const MAJOR_TYPE_BYTE_STRING: u8 = 2;
pub(crate) const MAJOR_TYPE_TEXT_STRING: u8 = 3;
pub(crate) const MAJOR_TYPE_ARRAY: u8 = 4;
pub(crate) const MAJOR_TYPE_MAP: u8 = 5;
pub(crate) const MAJOR_TYPE_SIMPLE_VALUE: u8 = 7;

// CBOR Additional Information Values (RFC 8949 Section 3)
// https://datatracker.ietf.org/doc/html/rfc8949#section-3
pub(crate) const ADDL_INFO_1_BYTE: u8 = 24;
pub(crate) const ADDL_INFO_2_BYTES: u8 = 25;
pub(crate) const ADDL_INFO_4_BYTES: u8 = 26;
pub(crate) const ADDL_INFO_8_BYTES: u8 = 27;

// CBOR Simple Values (RFC 8949 Section 3.3)
// https://datatracker.ietf.org/doc/html/rfc8949#section-3.3
pub(crate) const SIMPLE_VALUE_FALSE: u8 = 20;
pub(crate) const SIMPLE_VALUE_TRUE: u8 = 21;
pub(crate) const SIMPLE_VALUE_NULL: u8 = 22;
pub(crate) const SIMPLE_VALUE_UNDEFINED: u8 = 23;
pub(crate) const SIMPLE_VALUE_FLOAT_16: u8 = 25;
pub(crate) const SIMPLE_VALUE_FLOAT_32: u8 = 26;
pub(crate) const SIMPLE_VALUE_FLOAT_64: u8 = 27;

/// MAX_DEPTH is the maximum "depth" of a structure that will be parsed.
/// Each array or map increases the depth by one.
pub const MAX_DEPTH: usize = 16;
