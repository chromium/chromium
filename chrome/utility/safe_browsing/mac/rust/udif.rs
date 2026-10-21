// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Universal Disk Image Format (UDIF) container parser.
//!
//! Rust port of `chrome/utility/safe_browsing/mac/udif.{h,cc}`.

use crate::{ReadSeek, SharedStream};

/// Errors that can occur when parsing a UDIF image.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum UdifError {
    IoError,
    InvalidTrailer,
    InvalidPlist,
    InvalidBlockData,
    Unimplemented,
}

/// Parses a UDIF disk image, providing access to partition metadata,
/// raw partition streams, and the image-level code signature.
pub struct UdifParser {
    _stream: SharedStream,
    partition_names: Vec<String>,
    signature_blob: Vec<u8>,
}

impl UdifParser {
    /// Parses the UDIF `koly` trailer and `blkx` property list from `stream`.
    pub fn parse(stream: SharedStream) -> Result<Self, UdifError> {
        // TODO(https://crbug.com/575525008): Parse `UDIFResourceFile` (`koly` trailer), `blkx` plist,
        // `UDIFBlockData` (`mish` blocks), and optional code signature blob.
        let _ = stream;
        Err(UdifError::Unimplemented)
    }

    /// Returns the raw DMG code signature bytes (empty if unsigned).
    pub fn code_signature(&self) -> &[u8] {
        &self.signature_blob
    }

    /// Returns the number of partitions in this UDIF image.
    pub fn number_of_partitions(&self) -> usize {
        self.partition_names.len()
    }

    /// Returns the partition name for `part_number`, e.g.
    /// `"disk image (Apple_HFS : 2)"`.
    pub fn partition_name(&self, part_number: usize) -> &str {
        self.partition_names.get(part_number).map_or("", String::as_str)
    }

    /// Extracts the partition type embedded in the partition name, e.g.
    /// `"Apple_HFS"` or `"Apple_Free"`.
    pub fn partition_type(&self, part_number: usize) -> &str {
        // TODO(https://crbug.com/575525008): Parse `"Name (Type : ID)"` format matching
        // `UDIFParser::GetPartitionType`.
        let _ = part_number;
        ""
    }

    /// Returns the logical uncompressed size of `part_number` in bytes.
    pub fn partition_size(&self, part_number: usize) -> usize {
        // TODO(https://crbug.com/575525008): Compute `block.sector_count * block_size`.
        let _ = part_number;
        0
    }

    /// Returns a virtualized stream over the decompressed partition data, or
    /// `None` if `part_number` is out of range.
    pub fn partition_read_stream(&self, part_number: usize) -> Option<Box<dyn ReadSeek>> {
        // TODO(https://crbug.com/575525008): Return `UdifPartitionReadStream` wrapping `self.stream`.
        let _ = part_number;
        None
    }
}
