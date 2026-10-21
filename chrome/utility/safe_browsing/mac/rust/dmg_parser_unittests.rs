// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//chrome/utility/safe_browsing/mac:dmg_parser";
}

use std::io::Cursor;

use dmg_parser::hfs::{HfsError, HfsIterator};
use dmg_parser::udif::{UdifError, UdifParser};
use dmg_parser::SharedStream;
use rust_gtest_interop::prelude::*;

#[gtest(RustHfsIteratorTest, EmptyStreamFailsToOpen)]
fn test_empty_hfs_stream_fails_to_open() {
    let stream = SharedStream::new(Cursor::new(Vec::<u8>::new()));
    expect_eq!(HfsIterator::open(stream).err(), Some(HfsError::Unimplemented));
}

#[gtest(RustUdifParserTest, EmptyStreamFailsToParse)]
fn test_empty_udif_stream_fails_to_parse() {
    let stream = SharedStream::new(Cursor::new(Vec::<u8>::new()));
    expect_eq!(UdifParser::parse(stream).err(), Some(UdifError::Unimplemented));
}

#[gtest(RustSharedStreamTest, InterleavedPositionalReads)]
fn test_shared_stream_interleaved_positional_reads() {
    let stream_a = SharedStream::new(Cursor::new(b"0123456789abcdef".to_vec()));
    let stream_b = stream_a.clone();

    let mut buf_a = [0u8; 4];
    let mut buf_b = [0u8; 4];

    expect_eq!(stream_a.read_at(2, &mut buf_a).ok(), Some(4));
    expect_eq!(&buf_a, b"2345");

    expect_eq!(stream_b.read_exact_at(10, &mut buf_b).ok(), Some(()));
    expect_eq!(&buf_b, b"abcd");

    expect_eq!(stream_a.read_exact_from_end(-4, &mut buf_a).ok(), Some(12));
    expect_eq!(&buf_a, b"cdef");
}
