// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![no_std]
#![forbid(unsafe_code)]

extern crate alloc;

mod constants;
mod parser;
mod types;

pub use constants::TRAILING_LENGTH_NUM_BYTES;
pub use parser::parse_trailing_length;
pub use types::ParseError;
