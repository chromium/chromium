// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![no_std]
#![forbid(unsafe_code)]

extern crate alloc;

mod constants;
mod parser;
mod types;

pub use constants::{INITIAL_BUNDLE_HEADER_BUFFER_SIZE, TRAILING_LENGTH_NUM_BYTES};
pub use parser::{parse_bundle_header, parse_magic_and_version, parse_trailing_length};
pub use types::{BundleHeaderResult, MagicAndVersionResult, ParseError, SectionOffsetEntry};
