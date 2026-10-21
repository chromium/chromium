// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//chrome/utility/safe_browsing/mac:dmg_parser";
}

use std::io::Cursor;

use dmg_parser::hfs::{HfsError, HfsIterator};
use dmg_parser::share_stream;
use dmg_parser::udif::{UdifError, UdifParser};
use rust_gtest_interop::prelude::*;

#[gtest(RustHfsIteratorTest, EmptyStreamFailsToOpen)]
fn test_empty_hfs_stream_fails_to_open() {
    let stream = share_stream(Cursor::new(Vec::<u8>::new()));
    expect_eq!(HfsIterator::open(stream).err(), Some(HfsError::Unimplemented));
}

#[gtest(RustUdifParserTest, EmptyStreamFailsToParse)]
fn test_empty_udif_stream_fails_to_parse() {
    let stream = share_stream(Cursor::new(Vec::<u8>::new()));
    expect_eq!(UdifParser::parse(stream).err(), Some(UdifError::Unimplemented));
}
