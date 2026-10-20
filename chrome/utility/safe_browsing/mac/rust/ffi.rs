// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! `cxx` FFI bridge between C++ `safe_browsing::dmg` (`UDIFParser`,
//! `HFSIterator`, and `ReadStream`) and the pure-Rust `dmg_parser` crate.
//!
//! This is the only Rust target in `chrome/utility/safe_browsing/mac` with
//! `allow_unsafe = true` (required by `#[cxx::bridge]`).

chromium::import! {
    "//chrome/utility/safe_browsing/mac:dmg_parser";
}

use std::io::{self, Cursor, Read, Seek, SeekFrom};

use dmg_parser::hfs::HfsIterator;
use dmg_parser::udif::UdifParser;
use dmg_parser::{share_stream, ReadSeek, SharedReadStream, SharedStream};

#[cxx::bridge(namespace = "safe_browsing::dmg::rust_ffi")]
mod ffi {
    unsafe extern "C++" {
        include!("chrome/utility/safe_browsing/mac/rust/read_stream_bridge.h");

        type ReadStream;

        #[cxx_name = "RustBridgeRead"]
        unsafe fn rust_bridge_read(
            stream: *mut ReadStream,
            buf: &mut [u8],
            bytes_read: &mut usize,
        ) -> bool;

        #[cxx_name = "RustBridgeSeek"]
        unsafe fn rust_bridge_seek(stream: *mut ReadStream, offset: i64, whence: i32) -> i64;
    }

    extern "Rust" {
        type RustReadStream;
        fn read_stream(self: &mut RustReadStream, buf: &mut [u8], bytes_read: &mut usize) -> bool;
        fn seek_stream(self: &mut RustReadStream, offset: i64, whence: i32) -> i64;

        type RustUdifParser;
        unsafe fn new_udif_parser(stream: *mut ReadStream) -> Box<RustUdifParser>;
        fn parse(self: &mut RustUdifParser) -> bool;
        fn code_signature(self: &RustUdifParser) -> &[u8];
        fn number_of_partitions(self: &RustUdifParser) -> usize;
        fn partition_name(self: &RustUdifParser, part_number: usize) -> String;
        fn partition_type(self: &RustUdifParser, part_number: usize) -> String;
        fn partition_size(self: &RustUdifParser, part_number: usize) -> usize;
        fn partition_read_stream(self: &RustUdifParser, part_number: usize) -> Box<RustReadStream>;

        type RustHfsIterator;
        unsafe fn new_hfs_iterator(stream: *mut ReadStream) -> Box<RustHfsIterator>;
        fn open(self: &mut RustHfsIterator) -> bool;
        fn next_item(self: &mut RustHfsIterator) -> bool;
        fn is_directory(self: &RustHfsIterator) -> bool;
        fn is_file(self: &RustHfsIterator) -> bool;
        fn is_symbolic_link(self: &RustHfsIterator) -> bool;
        fn is_hard_link(self: &RustHfsIterator) -> bool;
        fn is_decmpfs_compressed(self: &RustHfsIterator) -> bool;
        fn path_utf16(self: &RustHfsIterator) -> &[u16];
        fn has_read_stream(self: &RustHfsIterator) -> bool;
        fn read_stream(self: &RustHfsIterator) -> Box<RustReadStream>;
    }
}

/// Adapts a non-owning C++ `ReadStream*` into a Rust [`Read`] + [`Seek`]
/// implementation.
struct CppStreamReader {
    stream: *mut ffi::ReadStream,
}

impl Read for CppStreamReader {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let mut bytes_read = 0usize;
        // SAFETY: `self.stream` points to the C++ `ReadStream` passed to
        // `UDIFParser` or `HFSIterator`, which outlives the parser/iterator.
        let ok = unsafe { ffi::rust_bridge_read(self.stream, buf, &mut bytes_read) };
        if ok {
            Ok(bytes_read)
        } else {
            Err(io::Error::other("C++ ReadStream::Read failed"))
        }
    }
}

impl Seek for CppStreamReader {
    fn seek(&mut self, pos: SeekFrom) -> io::Result<u64> {
        let (offset, whence) = match pos {
            SeekFrom::Start(o) => {
                let o = i64::try_from(o)
                    .map_err(|_| io::Error::new(io::ErrorKind::InvalidInput, "seek overflow"))?;
                (o, 0)
            }
            SeekFrom::Current(o) => (o, 1),
            SeekFrom::End(o) => (o, 2),
        };
        // SAFETY: `self.stream` points to the C++ `ReadStream` passed to
        // `UDIFParser` or `HFSIterator`, which outlives the parser/iterator.
        let rv = unsafe { ffi::rust_bridge_seek(self.stream, offset, whence) };
        u64::try_from(rv).map_err(|_| io::Error::other("C++ ReadStream::Seek failed"))
    }
}

/// Boxed Rust stream exposed back to C++ via `RustReadStreamAdapter`.
pub struct RustReadStream {
    inner: Box<dyn ReadSeek>,
}

impl RustReadStream {
    fn read_stream(&mut self, buf: &mut [u8], bytes_read: &mut usize) -> bool {
        match self.inner.read(buf) {
            Ok(n) => {
                *bytes_read = n;
                true
            }
            Err(_) => {
                *bytes_read = 0;
                false
            }
        }
    }

    fn seek_stream(&mut self, offset: i64, whence: i32) -> i64 {
        let pos = match whence {
            0 => u64::try_from(offset).ok().map(SeekFrom::Start),
            1 => Some(SeekFrom::Current(offset)),
            2 => Some(SeekFrom::End(offset)),
            _ => None,
        };
        let Some(pos) = pos else {
            return -1;
        };
        match self.inner.seek(pos) {
            Ok(new_pos) => i64::try_from(new_pos).unwrap_or(-1),
            Err(_) => -1,
        }
    }
}

/// FFI wrapper around [`UdifParser`] backing C++
/// `safe_browsing::dmg::UDIFParser`.
pub struct RustUdifParser {
    stream: SharedStream,
    parsed: Option<UdifParser>,
}

unsafe fn new_udif_parser(stream: *mut ffi::ReadStream) -> Box<RustUdifParser> {
    Box::new(RustUdifParser { stream: share_stream(CppStreamReader { stream }), parsed: None })
}

impl RustUdifParser {
    fn parse(&mut self) -> bool {
        match UdifParser::parse(self.stream.clone()) {
            Ok(parser) => {
                self.parsed = Some(parser);
                true
            }
            Err(_) => false,
        }
    }

    fn code_signature(&self) -> &[u8] {
        self.parsed.as_ref().map_or(&[], UdifParser::code_signature)
    }

    fn number_of_partitions(&self) -> usize {
        self.parsed.as_ref().map_or(0, UdifParser::number_of_partitions)
    }

    fn partition_name(&self, part_number: usize) -> String {
        self.parsed.as_ref().map_or_else(String::new, |p| p.partition_name(part_number).to_owned())
    }

    fn partition_type(&self, part_number: usize) -> String {
        self.parsed.as_ref().map_or_else(String::new, |p| p.partition_type(part_number).to_owned())
    }

    fn partition_size(&self, part_number: usize) -> usize {
        self.parsed.as_ref().map_or(0, |p| p.partition_size(part_number))
    }

    fn partition_read_stream(&self, part_number: usize) -> Box<RustReadStream> {
        let inner: Box<dyn ReadSeek> = match self.parsed.as_ref() {
            Some(p) => Box::new(SharedReadStream(p.partition_read_stream(part_number))),
            None => Box::new(Cursor::new(Vec::<u8>::new())),
        };
        Box::new(RustReadStream { inner })
    }
}

/// FFI wrapper around [`HfsIterator`] backing C++
/// `safe_browsing::dmg::HFSIterator`.
pub struct RustHfsIterator {
    stream: SharedStream,
    opened: Option<HfsIterator>,
}

unsafe fn new_hfs_iterator(stream: *mut ffi::ReadStream) -> Box<RustHfsIterator> {
    Box::new(RustHfsIterator { stream: share_stream(CppStreamReader { stream }), opened: None })
}

impl RustHfsIterator {
    fn open(&mut self) -> bool {
        match HfsIterator::open(self.stream.clone()) {
            Ok(iter) => {
                self.opened = Some(iter);
                true
            }
            Err(_) => false,
        }
    }

    fn next_item(&mut self) -> bool {
        self.opened.as_mut().is_some_and(HfsIterator::next_item)
    }

    fn is_directory(&self) -> bool {
        self.opened.as_ref().is_some_and(HfsIterator::is_directory)
    }

    fn is_file(&self) -> bool {
        self.opened.as_ref().is_some_and(HfsIterator::is_file)
    }

    fn is_symbolic_link(&self) -> bool {
        self.opened.as_ref().is_some_and(HfsIterator::is_symbolic_link)
    }

    fn is_hard_link(&self) -> bool {
        self.opened.as_ref().is_some_and(HfsIterator::is_hard_link)
    }

    fn is_decmpfs_compressed(&self) -> bool {
        self.opened.as_ref().is_some_and(HfsIterator::is_decmpfs_compressed)
    }

    fn path_utf16(&self) -> &[u16] {
        self.opened.as_ref().map_or(&[], HfsIterator::path)
    }

    fn has_read_stream(&self) -> bool {
        self.opened.as_ref().is_some_and(|h| !h.is_directory() && !h.is_hard_link())
    }

    fn read_stream(&self) -> Box<RustReadStream> {
        let inner = self
            .opened
            .as_ref()
            .and_then(HfsIterator::read_stream)
            .unwrap_or_else(|| Box::new(Cursor::new(Vec::<u8>::new())));
        Box::new(RustReadStream { inner })
    }
}
