// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![no_std]
#![forbid(unsafe_code)]

extern crate alloc;

mod constants;
pub mod http;
mod parser;
mod types;

pub use constants::{
    CRITICAL_SECTION, INDEX_SECTION, INITIAL_BUFFER_SIZE_FOR_RESPONSE,
    INITIAL_BUNDLE_HEADER_BUFFER_SIZE, MAX_METADATA_SECTION_SIZE, PRIMARY_SECTION,
    RESPONSES_SECTION, TRAILING_LENGTH_NUM_BYTES,
};
pub use parser::{
    parse_bundle_header, parse_critical_section, parse_index_section, parse_magic_and_version,
    parse_primary_section, parse_response, parse_trailing_length,
};
pub use types::{
    BundleHeaderResult, HeaderEntry, MagicAndVersionResult, ParseError, ParsedIndexEntry,
    ResponseParseResult, SectionOffsetEntry,
};
