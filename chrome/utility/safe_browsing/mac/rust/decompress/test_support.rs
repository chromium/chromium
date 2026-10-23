// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Test-only bzip2 compressor used to synthesize test payloads.

use std::ffi::{c_char, c_int, c_uint};
use std::io;

const BZ_OK: c_int = 0;

unsafe extern "C" {
    fn BZ2_bzBuffToBuffCompress(
        dest: *mut c_char,
        dest_len: *mut c_uint,
        source: *const c_char,
        source_len: c_uint,
        block_size_100k: c_int,
        verbosity: c_int,
        work_factor: c_int,
    ) -> c_int;
}

/// Compresses `input` into a bzip2 payload. Used for synthesis in unit tests.
pub fn compress_bz2(input: &[u8]) -> io::Result<Vec<u8>> {
    let too_large = || io::Error::new(io::ErrorKind::InvalidInput, "bz2 input too large");
    let input_len = c_uint::try_from(input.len()).map_err(|_| too_large())?;
    // libbz2 documents this as a safe upper bound for the compressed size.
    let mut dest_len = input
        .len()
        .checked_add(input.len() / 100 + 600)
        .and_then(|n| c_uint::try_from(n).ok())
        .ok_or_else(too_large)?;
    let mut output = vec![0u8; dest_len as usize];

    // SAFETY: `output` and `input` are valid, non-overlapping slices of lengths
    // `dest_len` and `input_len` respectively, and `&mut dest_len` is a valid
    // mutable pointer. On `BZ_OK`, `BZ2_bzBuffToBuffCompress` guarantees that
    // the updated `dest_len` is `<= output.len()`.
    let ret = unsafe {
        BZ2_bzBuffToBuffCompress(
            output.as_mut_ptr() as *mut c_char,
            &mut dest_len,
            input.as_ptr() as *const c_char,
            input_len,
            9,
            0,
            30,
        )
    };
    if ret != BZ_OK {
        return Err(io::Error::other(format!(
            "BZ2_bzBuffToBuffCompress failed: {ret}",
        )));
    }
    output.truncate(dest_len as usize);
    Ok(output)
}
