// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Audited FFI wrapper around system libbz2 for bzip2 decompression.
//!
//! TODO(crbug.com/575525008): Replace with a vendored pure-Rust bzip2
//! implementation (e.g. libbz2-rs-sys) to remove the last C dependency.

use std::ffi::{c_char, c_int, c_uint, c_void};
use std::io;

// These declarations mirror `bz_stream` and the decompression API from the
// system `<bzlib.h>` by hand rather than via `rust_bindgen`: the header comes
// from the platform SDK, is not present in Chromium's Linux sysroot, and there
// is no bundled bzip2, so a bindgen target would break non-Mac builds of this
// crate. The layout is checked below, and this FFI is slated for replacement
// by a pure-Rust bzip2 implementation (see the TODO above).
#[repr(C)]
struct BzStream {
    next_in: *mut c_char,
    avail_in: c_uint,
    total_in_lo32: c_uint,
    total_in_hi32: c_uint,

    next_out: *mut c_char,
    avail_out: c_uint,
    total_out_lo32: c_uint,
    total_out_hi32: c_uint,

    state: *mut c_void,

    bzalloc: Option<unsafe extern "C" fn(*mut c_void, c_int, c_int) -> *mut c_void>,
    bzfree: Option<unsafe extern "C" fn(*mut c_void, *mut c_void)>,
    opaque: *mut c_void,
}

// Matches `sizeof(bz_stream)` and field offsets in `<bzlib.h>` on 64-bit
// platforms (the only ones this Mac-only code targets).
#[cfg(target_pointer_width = "64")]
const _: () = {
    assert!(std::mem::size_of::<BzStream>() == 80);
    assert!(std::mem::offset_of!(BzStream, next_in) == 0);
    assert!(std::mem::offset_of!(BzStream, avail_in) == 8);
    assert!(std::mem::offset_of!(BzStream, next_out) == 24);
    assert!(std::mem::offset_of!(BzStream, avail_out) == 32);
    assert!(std::mem::offset_of!(BzStream, state) == 48);
    assert!(std::mem::offset_of!(BzStream, opaque) == 72);
};

const BZ_OK: c_int = 0;
const BZ_STREAM_END: c_int = 4;

unsafe extern "C" {
    fn BZ2_bzDecompressInit(strm: *mut BzStream, verbosity: c_int, small: c_int) -> c_int;
    fn BZ2_bzDecompress(strm: *mut BzStream) -> c_int;
    fn BZ2_bzDecompressEnd(strm: *mut BzStream) -> c_int;
}

/// Owns an initialized, heap-allocated `BzStream` for its entire lifetime.
///
/// libbz2 records the stream address in its internal state at init time and
/// dereferences it on every call, so the stream must never move and every
/// access (from Rust and C) must derive from the same pointer. The stream is
/// therefore allocated with `Box::into_raw` and only ever accessed through
/// `self.strm`; no Rust reference to it is created.
struct BzDecompressor {
    strm: *mut BzStream,
}

impl BzDecompressor {
    fn new() -> io::Result<Self> {
        let strm = Box::into_raw(Box::new(BzStream {
            next_in: std::ptr::null_mut(),
            avail_in: 0,
            total_in_lo32: 0,
            total_in_hi32: 0,
            next_out: std::ptr::null_mut(),
            avail_out: 0,
            total_out_lo32: 0,
            total_out_hi32: 0,
            state: std::ptr::null_mut(),
            bzalloc: None,
            bzfree: None,
            opaque: std::ptr::null_mut(),
        }));

        // SAFETY: `strm` points to a valid, zero-initialized `BzStream` that is
        // never moved until it is freed in `Drop` (or below on failure).
        let ret = unsafe { BZ2_bzDecompressInit(strm, 0, 0) };
        if ret != BZ_OK {
            // SAFETY: `strm` came from `Box::into_raw` above and libbz2 did not
            // retain it because initialization failed.
            drop(unsafe { Box::from_raw(strm) });
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                format!("BZ2_bzDecompressInit failed with error code {ret}"),
            ));
        }
        Ok(Self { strm })
    }

    /// Runs one `BZ2_bzDecompress` step from `input` into `output`. Returns
    /// `(return_code, bytes_consumed, bytes_produced)`.
    fn step(&mut self, input: &[u8], output: &mut [u8]) -> (c_int, usize, usize) {
        let avail_in = std::cmp::min(input.len(), c_uint::MAX as usize) as c_uint;
        let avail_out = std::cmp::min(output.len(), c_uint::MAX as usize) as c_uint;

        // SAFETY: `self.strm` is valid and initialized (see `new`), and every
        // access goes through this single pointer. `next_in`/`next_out` point
        // into `input`/`output` with matching lengths, and both slices outlive
        // the `BZ2_bzDecompress` call. The pointers are cleared before
        // returning so no dangling slice pointer is left in the stream.
        unsafe {
            (*self.strm).next_in = input.as_ptr() as *mut c_char;
            (*self.strm).avail_in = avail_in;
            (*self.strm).next_out = output.as_mut_ptr() as *mut c_char;
            (*self.strm).avail_out = avail_out;

            let rv = BZ2_bzDecompress(self.strm);

            let consumed = (avail_in - (*self.strm).avail_in) as usize;
            let produced = (avail_out - (*self.strm).avail_out) as usize;

            (*self.strm).next_in = std::ptr::null_mut();
            (*self.strm).avail_in = 0;
            (*self.strm).next_out = std::ptr::null_mut();
            (*self.strm).avail_out = 0;

            (rv, consumed, produced)
        }
    }
}

impl Drop for BzDecompressor {
    fn drop(&mut self) {
        // SAFETY: `self.strm` was initialized by `BZ2_bzDecompressInit` in
        // `new` and must be released by `BZ2_bzDecompressEnd`. It was allocated
        // by `Box::into_raw` and is freed exactly once here.
        unsafe {
            BZ2_bzDecompressEnd(self.strm);
            drop(Box::from_raw(self.strm));
        }
    }
}

/// Decompresses raw bzip2 compressed bytes into the `decompressed` buffer.
/// Returns `Ok(bytes_produced)` on success, or an `io::Error` on failure.
pub fn decompress_bz2(compressed: &[u8], decompressed: &mut [u8]) -> io::Result<usize> {
    if decompressed.is_empty() {
        return Ok(0);
    }

    if compressed.is_empty() {
        return Err(io::Error::new(io::ErrorKind::UnexpectedEof, "Compressed bzip2 data is empty"));
    }

    let mut decompressor = BzDecompressor::new()?;

    let total_out_len = decompressed.len();
    let mut remaining_in = compressed;
    let mut remaining_out = decompressed;

    let mut rv = BZ_OK;
    while rv == BZ_OK && (!remaining_in.is_empty() || !remaining_out.is_empty()) {
        let (step_rv, bytes_consumed, bytes_produced) =
            decompressor.step(remaining_in, remaining_out);
        rv = step_rv;

        if bytes_consumed == 0 && bytes_produced == 0 && rv == BZ_OK {
            return Err(io::Error::new(
                io::ErrorKind::UnexpectedEof,
                "BZ2 decompress stalled: no progress made while input or output remaining",
            ));
        }

        remaining_in = &remaining_in[bytes_consumed..];
        remaining_out = &mut remaining_out[bytes_produced..];
    }

    if rv == BZ_OK {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "BZ2 stream did not end within the chunk (truncated or larger than expected)",
        ));
    }

    if rv != BZ_STREAM_END {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            format!("BZ2_bzDecompress failed with error code {rv}"),
        ));
    }

    Ok(total_out_len - remaining_out.len())
}
