// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Pure-Rust parser for macOS UDIF (`.dmg`) disk images and HFS+/HFSX
//! filesystems.
//!
//! This crate is intentionally free of `unsafe` code; all C++ FFI glue lives in
//! the separate `dmg_ffi_bridge` crate (`ffi.rs`).

use std::cell::RefCell;
use std::io::{Read, Seek};
use std::rc::Rc;

pub mod hfs;
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
pub type SharedStream = Rc<RefCell<dyn ReadSeek>>;

/// Wraps an owned [`ReadSeek`] stream into a [`SharedStream`].
pub fn share_stream<R: ReadSeek + 'static>(reader: R) -> SharedStream {
    Rc::new(RefCell::new(reader))
}
