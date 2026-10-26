// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Pure-Rust parser for macOS UDIF (`.dmg`) disk images, HFS+/HFSX
//! filesystems, and Mach-O binary images.
//!
//! This crate is intentionally free of `unsafe` code; all C++ FFI glue lives in
//! the separate `dmg_ffi_bridge` crate (`ffi.rs`), and bzip2 decompression FFI
//! lives in the separate `dmg_decompress` crate.

#![forbid(unsafe_code)]

use std::cell::RefCell;
use std::io::{self, Read, Seek, SeekFrom};
use std::rc::Rc;

pub mod hfs;
pub mod mach_o;
pub mod udif;

/// Combined `Read + Seek` trait equivalent to C++
/// `safe_browsing::dmg::ReadStream`.
pub trait ReadSeek: Read + Seek {}

impl<T: Read + Seek> ReadSeek for T {}

/// Shared, interiorly mutable handle to a [`ReadSeek`] stream.
///
/// Both UDIF partition streams and HFS+ fork streams perform explicit `Seek`
/// calls before every `Read`, allowing multiple virtual streams (such as the
/// HFS+ catalog file and an extracted file's data fork) to share a single
/// underlying reader without lifetime coupling.
#[derive(Clone)]
pub struct SharedStream {
    inner: Rc<RefCell<dyn ReadSeek>>,
}

impl SharedStream {
    /// Wraps an owned [`ReadSeek`] stream into a [`SharedStream`].
    pub fn new<R: ReadSeek + 'static>(reader: R) -> Self {
        Self { inner: Rc::new(RefCell::new(reader)) }
    }

    /// Seeks to `offset` from the start of the stream and reads up to
    /// `buf.len()` bytes into `buf`.
    pub fn read_at(&self, offset: u64, buf: &mut [u8]) -> io::Result<usize> {
        // `borrow_mut` cannot panic here because borrows of `self.inner` never
        // escape across calls. Therefore, the only way for a panic to occur is
        // via reentrancy, which would also indicate a reference cycle.
        let mut stream = self.inner.borrow_mut();
        seek_to_offset(&mut *stream, offset)?;
        stream.read(buf)
    }

    /// Seeks to `offset` from the start of the stream and reads exactly
    /// `buf.len()` bytes into `buf`.
    pub fn read_exact_at(&self, offset: u64, buf: &mut [u8]) -> io::Result<()> {
        // `borrow_mut` cannot panic here because borrows of `self.inner` never
        // escape across calls. Therefore, the only way for a panic to occur is
        // via reentrancy, which would also indicate a reference cycle.
        let mut stream = self.inner.borrow_mut();
        seek_to_offset(&mut *stream, offset)?;
        stream.read_exact(buf)
    }

    /// Seeks to `offset` relative to the end of the stream, reads exactly
    /// `buf.len()` bytes into `buf`, and returns the starting byte offset from
    /// the beginning of the stream.
    pub fn read_exact_from_end(&self, offset: i64, buf: &mut [u8]) -> io::Result<u64> {
        // `borrow_mut` cannot panic here because borrows of `self.inner` never
        // escape across calls. Therefore, the only way for a panic to occur is
        // via reentrancy, which would also indicate a reference cycle.
        let mut stream = self.inner.borrow_mut();
        let start = stream.seek(SeekFrom::End(offset))?;
        stream.read_exact(buf)?;
        Ok(start)
    }

    /// Returns the total length of the underlying stream in bytes by seeking to
    /// the end.
    pub fn stream_len(&self) -> io::Result<u64> {
        let mut stream = self.inner.borrow_mut();
        stream.seek(SeekFrom::End(0))
    }
}

fn seek_to_offset(stream: &mut dyn ReadSeek, offset: u64) -> io::Result<()> {
    let pos = stream.seek(SeekFrom::Start(offset))?;
    if pos != offset {
        return Err(io::Error::new(
            io::ErrorKind::UnexpectedEof,
            "seek did not reach target offset",
        ));
    }
    Ok(())
}
