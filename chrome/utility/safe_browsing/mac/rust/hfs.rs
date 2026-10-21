// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Read-only HFS+ / HFSX filesystem catalog iterator and fork reader.
//!
//! Rust port of `chrome/utility/safe_browsing/mac/hfs.{h,cc}`.

use crate::{ReadSeek, SharedStream};

/// Errors that can occur when opening or iterating an HFS+/HFSX volume.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum HfsError {
    IoError,
    InvalidVolumeHeader,
    InvalidBTree,
    Unimplemented,
}

/// Iterates over files and directories in an HFS+ or HFSX volume.
pub struct HfsIterator {
    _stream: SharedStream,
    current_path: Vec<u16>,
}

impl HfsIterator {
    /// Reads the `HFSPlusVolumeHeader` at byte offset 1024 and initializes the
    /// catalog B-Tree iterator.
    pub fn open(stream: SharedStream) -> Result<Self, HfsError> {
        // TODO(https://crbug.com/575525008): Seek to 1024, parse `HFSPlusVolumeHeader`, open
        // `catalogFile` fork stream, and initialize `HfsBTreeIterator`.
        let _ = stream;
        Err(HfsError::Unimplemented)
    }

    /// Advances the iterator to the next exported file or folder record.
    /// Returns `false` at end-of-catalog or on corrupt record data.
    pub fn next_item(&mut self) -> bool {
        // TODO(https://crbug.com/575525008): Advance `HfsBTreeIterator`, skipping thread records and
        // unexported HFS+ private metadata entries.
        false
    }

    /// Returns true if the current item is a directory.
    pub fn is_directory(&self) -> bool {
        false
    }

    /// Returns true if the current item is a file.
    pub fn is_file(&self) -> bool {
        false
    }

    /// Returns true if the current item is a symbolic link.
    pub fn is_symbolic_link(&self) -> bool {
        false
    }

    /// Returns true if the current item is a hard link.
    pub fn is_hard_link(&self) -> bool {
        false
    }

    /// Returns true if the current item is `decmpfs`-compressed
    /// (`UF_COMPRESSED`).
    pub fn is_decmpfs_compressed(&self) -> bool {
        false
    }

    /// Returns the full UTF-16 path of the current catalog item.
    pub fn path(&self) -> &[u16] {
        &self.current_path
    }

    /// Returns a reader for the data fork of the current file item, or `None`
    /// if the current item is a directory or hard link.
    pub fn read_stream(&self) -> Option<Box<dyn ReadSeek>> {
        // TODO(https://crbug.com/575525008): Return `Box::new(HfsForkReadStream::new(...))`.
        None
    }
}
