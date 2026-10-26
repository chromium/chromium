// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! `cxx` FFI bridge between C++ `safe_browsing::dmg` (`UDIFParser`,
//! `HFSIterator`, and `ReadStream`) / `safe_browsing::MachOImageReader` and
//! the pure-Rust `dmg_parser` crate.
//!
//! This is the only Rust target in `chrome/utility/safe_browsing/mac` with
//! `allow_unsafe = true` (required by `#[cxx::bridge]`).

chromium::import! {
    "//chrome/utility/safe_browsing/mac:dmg_parser";
}

use std::io::{self, Read, Seek, SeekFrom};

use dmg_parser::hfs::HfsIterator;
use dmg_parser::mach_o::MachOImage;
use dmg_parser::udif::UdifParser;
use dmg_parser::{ReadSeek, SharedStream};

#[cxx::bridge(namespace = "safe_browsing::dmg::rust_ffi")]
mod ffi {
    extern "C++" {
        include!("chrome/utility/safe_browsing/mac/rust/read_stream_bridge.h");

        type ReadStream;

        /// Reads up to `buf.len()` bytes from `stream` into `buf`.
        ///
        /// # Safety
        ///
        /// `stream` must be a valid, non-null pointer to a live C++
        /// `safe_browsing::dmg::ReadStream` with exclusive mutable access for
        /// the duration of this call.
        #[cxx_name = "RustBridgeRead"]
        unsafe fn rust_bridge_read(
            stream: *mut ReadStream,
            buf: &mut [u8],
            bytes_read: &mut usize,
        ) -> bool;

        /// Seeks `stream` to `offset` relative to `whence` (`0` = `SEEK_SET`,
        /// `1` = `SEEK_CUR`, `2` = `SEEK_END`).
        ///
        /// # Safety
        ///
        /// `stream` must be a valid, non-null pointer to a live C++
        /// `safe_browsing::dmg::ReadStream` with exclusive mutable access for
        /// the duration of this call.
        #[cxx_name = "RustBridgeSeek"]
        unsafe fn rust_bridge_seek(stream: *mut ReadStream, offset: i64, whence: i32) -> i64;
    }

    extern "Rust" {
        type RustReadStream;
        fn is_valid(self: &RustReadStream) -> bool;
        fn read_stream(self: &mut RustReadStream, buf: &mut [u8], bytes_read: &mut usize) -> bool;
        fn seek_stream(self: &mut RustReadStream, offset: i64, whence: i32) -> i64;

        type RustUdifParser;
        /// # Safety
        ///
        /// `stream` must be a valid, non-null pointer to a C++ `ReadStream`
        /// that outlives the returned `RustUdifParser` and any `RustReadStream`
        /// obtained from `partition_read_stream()`.
        unsafe fn new_udif_parser(stream: *mut ReadStream) -> Box<RustUdifParser>;
        fn parse(self: &mut RustUdifParser) -> bool;
        /// The returned slice borrows from `self` and is only valid while this
        /// `RustUdifParser` is alive and not mutated.
        fn code_signature(self: &RustUdifParser) -> &[u8];
        fn number_of_partitions(self: &RustUdifParser) -> usize;
        /// The returned string slice borrows from `self` and is only valid
        /// while this `RustUdifParser` is alive and not mutated.
        fn partition_name(self: &RustUdifParser, part_number: usize) -> &str;
        /// The returned string slice borrows from `self` and is only valid
        /// while this `RustUdifParser` is alive and not mutated.
        fn partition_type(self: &RustUdifParser, part_number: usize) -> &str;
        fn partition_size(self: &RustUdifParser, part_number: usize) -> usize;
        fn partition_read_stream(self: &RustUdifParser, part_number: usize) -> Box<RustReadStream>;

        type RustHfsIterator;
        /// # Safety
        ///
        /// `stream` must be a valid, non-null pointer to a C++ `ReadStream`
        /// that outlives the returned `RustHfsIterator` and any
        /// `RustReadStream` obtained from `read_stream()`.
        unsafe fn new_hfs_iterator(stream: *mut ReadStream) -> Box<RustHfsIterator>;
        fn open(self: &mut RustHfsIterator) -> bool;
        fn next_item(self: &mut RustHfsIterator) -> bool;
        fn is_directory(self: &RustHfsIterator) -> bool;
        fn is_file(self: &RustHfsIterator) -> bool;
        fn is_symbolic_link(self: &RustHfsIterator) -> bool;
        fn is_hard_link(self: &RustHfsIterator) -> bool;
        fn is_decmpfs_compressed(self: &RustHfsIterator) -> bool;
        /// The returned slice borrows from `self` and is only valid while this
        /// `RustHfsIterator` is alive and not mutated.
        fn path_utf16(self: &RustHfsIterator) -> &[u16];
        fn read_stream(self: &RustHfsIterator) -> Box<RustReadStream>;

        type RustMachOImageReader;
        fn is_mach_o_magic_value(magic: u32) -> bool;
        /// Parses `image` as a thin or fat Mach-O binary. Callers must check
        /// `is_valid()` on the returned reader before calling other methods.
        fn new_mach_o_image_reader(image: &[u8]) -> Box<RustMachOImageReader>;
        fn is_valid(self: &RustMachOImageReader) -> bool;
        fn is_fat(self: &RustMachOImageReader) -> bool;
        fn num_fat_images(self: &RustMachOImageReader) -> usize;
        /// Transfers ownership of the fat architecture slice at `index` to the
        /// caller. Subsequent calls with the same `index` return an invalid
        /// (`!is_valid()`) reader.
        fn take_fat_image(
            self: &mut RustMachOImageReader,
            index: usize,
        ) -> Box<RustMachOImageReader>;
        fn is_64_bit(self: &RustMachOImageReader) -> bool;
        /// The returned 28-byte slice borrows from `self` and is valid while
        /// this `RustMachOImageReader` is alive and not mutated.
        fn mach_header_bytes(self: &RustMachOImageReader) -> &[u8];
        /// The returned 32-byte slice borrows from `self` and is valid while
        /// this `RustMachOImageReader` is alive and not mutated.
        fn mach_header_64_bytes(self: &RustMachOImageReader) -> &[u8];
        fn file_type(self: &RustMachOImageReader) -> u32;
        fn num_load_commands(self: &RustMachOImageReader) -> usize;
        /// The returned slice borrows from `self` and is valid while this
        /// `RustMachOImageReader` is alive and not mutated.
        fn load_command_data(self: &RustMachOImageReader, index: usize) -> &[u8];
        fn has_code_signature(self: &RustMachOImageReader) -> bool;
        /// The returned slice borrows from `self` and is valid while this
        /// `RustMachOImageReader` is alive and not mutated.
        fn code_signature_info(self: &RustMachOImageReader) -> &[u8];
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
        // SAFETY: By the safety contract of `new_udif_parser` and
        // `new_hfs_iterator`, `self.stream` is a valid, non-null pointer to a
        // live C++ `ReadStream` that outlives this `CppStreamReader` (including
        // any `SharedStream` clones held by child `RustReadStream`s), and
        // `&mut self` guarantees exclusive access from Rust during this call.
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
        // SAFETY: By the safety contract of `new_udif_parser` and
        // `new_hfs_iterator`, `self.stream` is a valid, non-null pointer to a
        // live C++ `ReadStream` that outlives this `CppStreamReader` (including
        // any `SharedStream` clones held by child `RustReadStream`s), and
        // `&mut self` guarantees exclusive access from Rust during this call.
        let rv = unsafe { ffi::rust_bridge_seek(self.stream, offset, whence) };
        u64::try_from(rv).map_err(|_| io::Error::other("C++ ReadStream::Seek failed"))
    }
}

/// Boxed Rust stream exposed back to C++ via `RustReadStreamAdapter`.
pub struct RustReadStream {
    inner: Option<Box<dyn ReadSeek>>,
}

impl RustReadStream {
    fn is_valid(&self) -> bool {
        self.inner.is_some()
    }

    fn read_stream(&mut self, buf: &mut [u8], bytes_read: &mut usize) -> bool {
        let Some(inner) = self.inner.as_mut() else {
            *bytes_read = 0;
            return false;
        };
        match inner.read(buf) {
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
        let Some(inner) = self.inner.as_mut() else {
            return -1;
        };
        let pos = match whence {
            0 => u64::try_from(offset).ok().map(SeekFrom::Start),
            1 => Some(SeekFrom::Current(offset)),
            2 => Some(SeekFrom::End(offset)),
            _ => None,
        };
        let Some(pos) = pos else {
            return -1;
        };
        match inner.seek(pos) {
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

/// # Safety
///
/// `stream` must be a valid, non-null pointer to a C++ `ReadStream` that
/// outlives the returned `RustUdifParser` and any `RustReadStream` obtained
/// from `partition_read_stream()`.
unsafe fn new_udif_parser(stream: *mut ffi::ReadStream) -> Box<RustUdifParser> {
    Box::new(RustUdifParser { stream: SharedStream::new(CppStreamReader { stream }), parsed: None })
}

impl RustUdifParser {
    fn parse(&mut self) -> bool {
        match UdifParser::try_new(self.stream.clone()) {
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

    fn partition_name(&self, part_number: usize) -> &str {
        self.parsed.as_ref().map_or("", |p| p.partition_name(part_number))
    }

    fn partition_type(&self, part_number: usize) -> &str {
        self.parsed.as_ref().map_or("", |p| p.partition_type(part_number))
    }

    fn partition_size(&self, part_number: usize) -> usize {
        self.parsed.as_ref().map_or(0, |p| p.partition_size(part_number))
    }

    fn partition_read_stream(&self, part_number: usize) -> Box<RustReadStream> {
        let inner = self.parsed.as_ref().and_then(|p| p.partition_read_stream(part_number));
        Box::new(RustReadStream { inner })
    }
}

/// FFI wrapper around [`HfsIterator`] backing C++
/// `safe_browsing::dmg::HFSIterator`.
pub struct RustHfsIterator {
    stream: SharedStream,
    opened: Option<HfsIterator>,
}

/// # Safety
///
/// `stream` must be a valid, non-null pointer to a C++ `ReadStream` that
/// outlives the returned `RustHfsIterator` and any `RustReadStream` obtained
/// from `read_stream()`.
unsafe fn new_hfs_iterator(stream: *mut ffi::ReadStream) -> Box<RustHfsIterator> {
    Box::new(RustHfsIterator {
        stream: SharedStream::new(CppStreamReader { stream }),
        opened: None,
    })
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

    fn read_stream(&self) -> Box<RustReadStream> {
        let inner = self.opened.as_ref().and_then(HfsIterator::read_stream);
        Box::new(RustReadStream { inner })
    }
}

fn is_mach_o_magic_value(magic: u32) -> bool {
    MachOImage::is_mach_o_magic_value(magic)
}

/// Validated Mach-O reader state.
///
/// Separated from [`RustMachOImageReader`] so that invalid states are
/// unrepresentable on this type (`parsed` is always a valid [`MachOImage`]),
/// while [`RustMachOImageReader`] provides the `Option` wrapper required at the
/// `#[cxx::bridge]` boundary (since `cxx` does not support `Option<Box<T>>`,
/// and `cxx`'s `Result<T>` requires C++ exceptions).
struct ValidMachOImageReader {
    parsed: MachOImage,
    /// Validated thin architecture slices for a fat binary, wrapped in `Option`
    /// slots so `take_fat_image(index)` can move each slice out once in O(1)
    /// time without shifting the indices of remaining slices.
    fat_images: Vec<Option<MachOImage>>,
}

impl ValidMachOImageReader {
    fn try_new(image: &[u8]) -> Option<Self> {
        MachOImage::parse(image).ok().map(Self::new)
    }

    fn new(mut parsed: MachOImage) -> Self {
        // Every slice returned by `MachOImage::take_fat_images()` was already
        // parsed and validated during `MachOImage::parse()` (which fails the
        // entire fat binary if any architecture slice is invalid, matching C++
        // `MachOImageReader::Initialize()`). Wrap each valid `MachOImage` in
        // `Some` so `take_fat_image(index)` can move ownership out via
        // `Option::take()` without shifting indices.
        let fat_images = parsed.take_fat_images().into_iter().map(Some).collect();
        Self { parsed, fat_images }
    }

    fn take_fat_image(&mut self, index: usize) -> Option<Self> {
        let child = self.fat_images.get_mut(index)?.take()?;
        Some(Self::new(child))
    }
}

/// FFI wrapper around [`ValidMachOImageReader`] backing C++
/// `safe_browsing::MachOImageReader`.
pub struct RustMachOImageReader(Option<ValidMachOImageReader>);

fn new_mach_o_image_reader(image: &[u8]) -> Box<RustMachOImageReader> {
    Box::new(RustMachOImageReader(ValidMachOImageReader::try_new(image)))
}

impl RustMachOImageReader {
    fn is_valid(&self) -> bool {
        self.0.is_some()
    }

    fn is_fat(&self) -> bool {
        self.0.as_ref().is_some_and(|r| r.parsed.is_fat())
    }

    fn num_fat_images(&self) -> usize {
        self.0.as_ref().map_or(0, |r| r.fat_images.len())
    }

    /// Transfers ownership of the fat architecture slice at `index` to the
    /// caller. Subsequent calls with the same `index` return an invalid
    /// (`!is_valid()`) reader.
    fn take_fat_image(&mut self, index: usize) -> Box<RustMachOImageReader> {
        let child = self.0.as_mut().and_then(|r| r.take_fat_image(index));
        Box::new(Self(child))
    }

    fn is_64_bit(&self) -> bool {
        self.0.as_ref().is_some_and(|r| r.parsed.is_64_bit())
    }

    fn mach_header_bytes(&self) -> &[u8] {
        self.0.as_ref().map_or(&[], |r| r.parsed.mach_header_bytes())
    }

    fn mach_header_64_bytes(&self) -> &[u8] {
        self.0.as_ref().map_or(&[], |r| r.parsed.mach_header_64_bytes())
    }

    fn file_type(&self) -> u32 {
        self.0.as_ref().map_or(0, |r| r.parsed.file_type())
    }

    fn num_load_commands(&self) -> usize {
        self.0.as_ref().map_or(0, |r| r.parsed.load_commands().len())
    }

    fn load_command_data(&self, index: usize) -> &[u8] {
        self.0
            .as_ref()
            .and_then(|r| r.parsed.load_commands().get(index))
            .map_or(&[], |cmd| cmd.data.as_slice())
    }

    fn has_code_signature(&self) -> bool {
        self.0.as_ref().and_then(|r| r.parsed.code_signature()).is_some()
    }

    fn code_signature_info(&self) -> &[u8] {
        self.0.as_ref().and_then(|r| r.parsed.code_signature()).unwrap_or(&[])
    }
}
