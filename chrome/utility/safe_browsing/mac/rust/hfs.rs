// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Read-only HFS+ / HFSX filesystem catalog iterator and fork reader.
//!
//! Rust port of `chrome/utility/safe_browsing/mac/hfs.{h,cc}`.

use std::io::{self, Read, Seek, SeekFrom};

use crate::{ReadSeek, SharedStream};

/// Maximum buffer size to allocate for a single extent (64 MiB).
///
/// Safe Browsing limits DMG analysis to 50 MB (see
/// `download_file_types.asciipb`), so an individual extent buffer should never
/// need to exceed 64 MB.
pub const MAX_EXTENT_BUFFER_SIZE: usize = 64 * 1024 * 1024;

/// Errors that can occur when opening or iterating an HFS+/HFSX volume.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum HfsError {
    IoError,
    InvalidVolumeHeader,
    InvalidBTree,
    Unimplemented,
}

/// On-disk HFS+ extent descriptor (`HFSPlusExtentDescriptor` in
/// `<hfs/hfs_format.h>`).
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct HfsPlusExtentDescriptor {
    pub start_block: u32,
    pub block_count: u32,
}

impl HfsPlusExtentDescriptor {
    pub const SIZE: usize = 8;

    /// Parses an extent descriptor from its 8-byte big-endian (`be`) on-disk
    /// representation.
    pub fn from_be_bytes(bytes: &[u8; Self::SIZE]) -> Self {
        Self {
            start_block: u32::from_be_bytes(bytes[0..4].try_into().expect("4-byte slice")),
            block_count: u32::from_be_bytes(bytes[4..8].try_into().expect("4-byte slice")),
        }
    }

    pub fn is_empty(&self) -> bool {
        self.start_block == 0 && self.block_count == 0
    }
}

/// On-disk HFS+ fork descriptor (`HFSPlusForkData` in `<hfs/hfs_format.h>`).
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct HfsPlusForkData {
    pub logical_size: u64,
    pub clump_size: u32,
    pub total_blocks: u32,
    pub extents: [HfsPlusExtentDescriptor; 8],
}

impl HfsPlusForkData {
    pub const SIZE: usize = 80;
    const EXTENTS_OFFSET: usize = 16;

    /// Parses a fork descriptor from its 80-byte big-endian (`be`) on-disk
    /// representation.
    pub fn from_be_bytes(bytes: &[u8; Self::SIZE]) -> Self {
        Self {
            logical_size: u64::from_be_bytes(bytes[0..8].try_into().expect("8-byte slice")),
            clump_size: u32::from_be_bytes(bytes[8..12].try_into().expect("4-byte slice")),
            total_blocks: u32::from_be_bytes(bytes[12..16].try_into().expect("4-byte slice")),
            extents: std::array::from_fn(|i| {
                let start = Self::EXTENTS_OFFSET + i * HfsPlusExtentDescriptor::SIZE;
                let end = start + HfsPlusExtentDescriptor::SIZE;
                HfsPlusExtentDescriptor::from_be_bytes(
                    bytes[start..end].try_into().expect("8-byte extent descriptor slice"),
                )
            }),
        }
    }
}

/// A [`ReadSeek`] stream over the 8 inline extents of an HFS+ fork.
///
/// Rust port of `safe_browsing::dmg::HFSForkReadStream`. This only consults
/// the eight inline fork extents and does not consult the extent overflow file.
pub struct HfsForkReadStream {
    stream: SharedStream,
    block_size: u32,
    fork: HfsPlusForkData,
    current_extent: usize,
    read_current_extent: bool,
    current_extent_data: Vec<u8>,
    current_extent_offset: usize,
    fork_logical_offset: u64,
}

impl HfsForkReadStream {
    pub fn new(stream: SharedStream, block_size: u32, fork: HfsPlusForkData) -> Self {
        Self {
            stream,
            block_size,
            fork,
            current_extent: 0,
            read_current_extent: false,
            current_extent_data: Vec::new(),
            current_extent_offset: 0,
            fork_logical_offset: 0,
        }
    }
}

impl Read for HfsForkReadStream {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        if self.fork_logical_offset == self.fork.logical_size {
            return Ok(0);
        }

        let mut bytes_read = 0usize;

        while self.current_extent < self.fork.extents.len() {
            // If the buffer is out of space, do not attempt any reads. Check
            // this here, so that `current_extent` is advanced by the loop if
            // the last extent was fully read.
            let buffer_space_remaining = buf.len() - bytes_read;
            if buffer_space_remaining == 0 {
                break;
            }

            let extent = self.fork.extents[self.current_extent];

            // A zero-length extent means end-of-fork.
            if extent.is_empty() {
                break;
            }

            let block_count = usize::try_from(extent.block_count).map_err(|_| {
                io::Error::new(io::ErrorKind::InvalidData, "extent block_count overflow")
            })?;
            let block_size = usize::try_from(self.block_size)
                .map_err(|_| io::Error::new(io::ErrorKind::InvalidData, "block_size overflow"))?;
            let extent_size =
                block_count.checked_mul(block_size).filter(|&size| size > 0).ok_or_else(|| {
                    io::Error::new(
                        io::ErrorKind::InvalidData,
                        "extent block_count overflows or is 0",
                    )
                })?;

            // Read the entire extent now, to avoid excessive seeking and
            // re-reading.
            if !self.read_current_extent {
                if extent_size > MAX_EXTENT_BUFFER_SIZE {
                    return Err(io::Error::new(
                        io::ErrorKind::InvalidData,
                        "extent size exceeds MAX_EXTENT_BUFFER_SIZE",
                    ));
                }
                let disk_offset = u64::from(extent.start_block)
                    .checked_mul(u64::from(self.block_size))
                    .ok_or_else(|| {
                        io::Error::new(
                            io::ErrorKind::InvalidData,
                            "extent start_block offset overflow",
                        )
                    })?;
                self.current_extent_data.resize(extent_size, 0);
                self.stream.read_exact_at(disk_offset, &mut self.current_extent_data)?;
                self.read_current_extent = true;
            }

            let remaining_in_fork =
                self.fork.logical_size.checked_sub(self.fork_logical_offset).ok_or_else(|| {
                    io::Error::new(
                        io::ErrorKind::InvalidData,
                        "fork_logical_offset exceeds logical_size",
                    )
                })?;
            let remaining_in_extent =
                extent_size.checked_sub(self.current_extent_offset).ok_or_else(|| {
                    io::Error::new(
                        io::ErrorKind::InvalidData,
                        "current_extent_offset exceeds extent_size",
                    )
                })?;

            let bytes_to_copy = remaining_in_extent
                .min(buffer_space_remaining)
                .min(usize::try_from(remaining_in_fork).unwrap_or(usize::MAX));

            buf[bytes_read..bytes_read + bytes_to_copy].copy_from_slice(
                &self.current_extent_data
                    [self.current_extent_offset..self.current_extent_offset + bytes_to_copy],
            );

            bytes_read += bytes_to_copy;
            self.fork_logical_offset += bytes_to_copy as u64;
            self.current_extent_offset += bytes_to_copy;

            // If the fork's data have been read, then end the loop.
            if self.fork_logical_offset == self.fork.logical_size {
                return Ok(bytes_read);
            }

            // If this extent still has data to be copied out, then the read was
            // partial and the buffer is full. Do not advance to the next
            // extent.
            if self.current_extent_offset < self.current_extent_data.len() {
                break;
            }

            // Advance to the next extent, so reset the state.
            self.current_extent += 1;
            self.read_current_extent = false;
            self.current_extent_offset = 0;
        }

        Ok(bytes_read)
    }
}

impl Seek for HfsForkReadStream {
    fn seek(&mut self, pos: SeekFrom) -> io::Result<u64> {
        let SeekFrom::Start(offset) = pos else {
            return Err(io::Error::new(
                io::ErrorKind::Unsupported,
                "HfsForkReadStream only supports SeekFrom::Start",
            ));
        };

        if self.block_size == 0 {
            return Err(io::Error::new(io::ErrorKind::InvalidInput, "invalid block_size 0"));
        }

        if offset > 0 && offset >= self.fork.logical_size {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "seek offset >= fork logical_size",
            ));
        }

        let block_size_u64 = u64::from(self.block_size);
        let target_block = offset / block_size_u64;
        let mut block_count = 0u64;

        for (idx, extent) in self.fork.extents.iter().enumerate() {
            // An empty extent indicates end-of-fork.
            if extent.is_empty() {
                break;
            }

            let new_block_count = block_count
                .checked_add(u64::from(extent.block_count))
                .filter(|&count| usize::try_from(count).is_ok())
                .ok_or_else(|| {
                    io::Error::new(io::ErrorKind::InvalidData, "seek block count overflow")
                })?;

            if target_block < new_block_count {
                let iterator_block_offset = block_count
                    .checked_mul(block_size_u64)
                    .filter(|&byte_offset| usize::try_from(byte_offset).is_ok())
                    .ok_or_else(|| {
                        io::Error::new(io::ErrorKind::InvalidData, "seek block offset overflow")
                    })?;
                let extent_offset =
                    usize::try_from(offset - iterator_block_offset).map_err(|_| {
                        io::Error::new(io::ErrorKind::InvalidData, "seek extent offset overflow")
                    })?;

                if self.current_extent != idx {
                    self.read_current_extent = false;
                    self.current_extent = idx;
                }
                self.current_extent_offset = extent_offset;
                self.fork_logical_offset = offset;
                return Ok(offset);
            }

            block_count = new_block_count;
        }

        Err(io::Error::new(io::ErrorKind::InvalidInput, "seek offset beyond fork extents"))
    }
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
