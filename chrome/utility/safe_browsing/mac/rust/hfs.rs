// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Read-only HFS+ / HFSX filesystem catalog iterator and fork reader.
//!
//! Rust port of `chrome/utility/safe_browsing/mac/hfs.{h,cc}`.

use std::collections::{HashMap, HashSet};
use std::io::{self, Read, Seek, SeekFrom};

use bytemuck::{Pod, Zeroable};
use static_assertions::const_assert_eq;

use crate::{ReadSeek, SharedStream};

/// Maximum buffer size to allocate for a single extent (64 MiB).
///
/// Safe Browsing limits DMG analysis to 50 MB (see
/// `download_file_types.asciipb`), so an individual extent buffer should never
/// need to exceed 64 MB.
pub const MAX_EXTENT_BUFFER_SIZE: usize = 64 * 1024 * 1024;

/// Byte offset of the `HFSPlusVolumeHeader` from the start of an HFS+ volume.
pub const VOLUME_HEADER_OFFSET: u64 = 1024;

/// Size in bytes of `HFSPlusVolumeHeader` (`sizeof(HFSPlusVolumeHeader)`).
pub const VOLUME_HEADER_SIZE: usize = 512;

/// HFS+ volume signature (`'H+'` in ASCII, `kHFSPlusSigWord`).
pub const HFS_PLUS_SIG_WORD: u16 = 0x482B;

/// HFSX volume signature (`'HX'` in ASCII, `kHFSXSigWord`).
pub const HFSX_SIG_WORD: u16 = 0x4858;

/// Finder file type for HFS+ hard links (`'hlnk'`, `kHardLinkFileType`).
pub const HARD_LINK_FILE_TYPE: u32 = 0x686C_6E6B;

/// Finder creator code for HFS+ hard links (`'hfs+'`, `kHFSPlusCreator`).
pub const HFS_PLUS_CREATOR: u32 = 0x6866_732B;

/// BSD `ownerFlags` bit indicating `decmpfs` compression (`UF_COMPRESSED`).
pub const UF_COMPRESSED: u8 = 0x20;

/// POSIX file type mask (`S_IFMT` in `<sys/stat.h>`).
const S_IFMT: u16 = 0o170000;

/// POSIX symbolic link file type (`S_IFLNK` in `<sys/stat.h>`).
pub const S_IFLNK: u16 = 0o120000;

/// UTF-16 path separator (`'/'`).
const FILE_PATH_SEPARATOR: u16 = b'/' as u16;

/// B-Tree leaf node kind (`kBTLeafNode` in `<hfs/hfs_format.h>`).
pub const BT_LEAF_NODE: i8 = -1;

/// B-Tree header node kind (`kBTHeaderNode` in `<hfs/hfs_format.h>`).
pub const BT_HEADER_NODE: i8 = 1;

/// HFS+ catalog folder record type (`kHFSPlusFolderRecord` in
/// `<hfs/hfs_format.h>`).
pub const HFS_PLUS_FOLDER_RECORD: i16 = 1;

/// HFS+ catalog file record type (`kHFSPlusFileRecord` in
/// `<hfs/hfs_format.h>`).
pub const HFS_PLUS_FILE_RECORD: i16 = 2;

/// HFS+ catalog folder thread record type (`kHFSPlusFolderThreadRecord` in
/// `<hfs/hfs_format.h>`).
pub const HFS_PLUS_FOLDER_THREAD_RECORD: i16 = 3;

/// HFS+ catalog file thread record type (`kHFSPlusFileThreadRecord` in
/// `<hfs/hfs_format.h>`).
pub const HFS_PLUS_FILE_THREAD_RECORD: i16 = 4;

/// Errors that can occur when opening or iterating an HFS+/HFSX volume.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum HfsError {
    IoError,
    InvalidVolumeHeader,
    InvalidBTree,
}

/// On-disk HFS+ extent descriptor (`HFSPlusExtentDescriptor` in
/// `<hfs/hfs_format.h>`).
#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Pod, Zeroable)]
pub struct HfsPlusExtentDescriptor {
    pub start_block: u32,
    pub block_count: u32,
}
const_assert_eq!(size_of::<HfsPlusExtentDescriptor>(), HfsPlusExtentDescriptor::SIZE);

impl HfsPlusExtentDescriptor {
    pub const SIZE: usize = 8;

    /// Parses an extent descriptor from its 8-byte big-endian on-disk
    /// representation.
    pub fn from_big_endian_bytes(bytes: &[u8; Self::SIZE]) -> Self {
        bytemuck::pod_read_unaligned::<Self>(bytes).convert_from_big_endian()
    }

    /// Converts all fields from big-endian to host byte order.
    fn convert_from_big_endian(self) -> Self {
        Self {
            start_block: u32::from_be(self.start_block),
            block_count: u32::from_be(self.block_count),
        }
    }

    pub fn is_empty(&self) -> bool {
        self.start_block == 0 && self.block_count == 0
    }
}

/// On-disk HFS+ fork descriptor (`HFSPlusForkData` in `<hfs/hfs_format.h>`).
#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Pod, Zeroable)]
pub struct HfsPlusForkData {
    pub logical_size: u64,
    pub clump_size: u32,
    pub total_blocks: u32,
    pub extents: [HfsPlusExtentDescriptor; 8],
}
const_assert_eq!(size_of::<HfsPlusForkData>(), HfsPlusForkData::SIZE);

impl HfsPlusForkData {
    pub const SIZE: usize = 80;

    /// Parses a fork descriptor from its 80-byte big-endian on-disk
    /// representation.
    pub fn from_big_endian_bytes(bytes: &[u8; Self::SIZE]) -> Self {
        bytemuck::pod_read_unaligned::<Self>(bytes).convert_from_big_endian()
    }

    /// Converts all fields from big-endian to host byte order.
    fn convert_from_big_endian(self) -> Self {
        Self {
            logical_size: u64::from_be(self.logical_size),
            clump_size: u32::from_be(self.clump_size),
            total_blocks: u32::from_be(self.total_blocks),
            extents: self.extents.map(HfsPlusExtentDescriptor::convert_from_big_endian),
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

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct HfsPlusVolumeHeader {
    pub signature: u16,
    pub block_size: u32,
    pub catalog_file: HfsPlusForkData,
}

impl HfsPlusVolumeHeader {
    const BLOCK_SIZE_OFFSET: usize = 40;
    const CATALOG_FILE_OFFSET: usize = 272;
    const SIGNATURE_SIZE_IN_BYTES: usize = 2;
    const BLOCK_SIZE_SIZE_IN_BYTES: usize = 4;

    pub fn from_big_endian_bytes(bytes: &[u8; VOLUME_HEADER_SIZE]) -> Self {
        Self {
            // The `unwrap` calls are safe because the input is a fixed size.
            signature: u16::from_be_bytes(
                *bytes.first_chunk::<{ Self::SIGNATURE_SIZE_IN_BYTES }>().unwrap(),
            ),
            block_size: u32::from_be_bytes(
                *bytes[Self::BLOCK_SIZE_OFFSET..]
                    .first_chunk::<{ Self::BLOCK_SIZE_SIZE_IN_BYTES }>()
                    .unwrap(),
            ),
            catalog_file: HfsPlusForkData::from_big_endian_bytes(
                bytes[Self::CATALOG_FILE_OFFSET..]
                    .first_chunk::<{ HfsPlusForkData::SIZE }>()
                    .unwrap(),
            ),
        }
    }
}

/// BSD file ownership and permissions (`HFSPlusBSDInfo` in
/// `<hfs/hfs_format.h>`).
#[repr(C, packed)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Pod, Zeroable)]
pub struct HfsPlusBsdInfo {
    pub owner_id: u32,
    pub group_id: u32,
    pub admin_flags: u8,
    pub owner_flags: u8,
    pub file_mode: u16,
    /// Union of `iNodeNum`, `linkCount` and `rawDevice`.
    pub special: u32,
}
const_assert_eq!(size_of::<HfsPlusBsdInfo>(), HfsPlusBsdInfo::SIZE);

impl HfsPlusBsdInfo {
    pub const SIZE: usize = 16;

    /// Converts all fields from big-endian to host byte order.
    fn convert_from_big_endian(self) -> Self {
        Self {
            owner_id: u32::from_be(self.owner_id),
            group_id: u32::from_be(self.group_id),
            admin_flags: self.admin_flags,
            owner_flags: self.owner_flags,
            file_mode: u16::from_be(self.file_mode),
            special: u32::from_be(self.special),
        }
    }
}

/// Finder file type and creator metadata (`FndrFileInfo` in
/// `<hfs/hfs_format.h>`).
#[repr(C, packed)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Pod, Zeroable)]
pub struct FinderFileInfo {
    pub fd_type: u32,
    pub fd_creator: u32,
    pub fd_flags: u16,
    /// `Point` (`v`, `h`) of the file's icon in its window.
    pub fd_location: [i16; 2],
    pub opaque: u16,
}
const_assert_eq!(size_of::<FinderFileInfo>(), FinderFileInfo::SIZE);

impl FinderFileInfo {
    pub const SIZE: usize = 16;

    /// Converts all fields from big-endian to host byte order.
    fn convert_from_big_endian(self) -> Self {
        Self {
            fd_type: u32::from_be(self.fd_type),
            fd_creator: u32::from_be(self.fd_creator),
            fd_flags: u16::from_be(self.fd_flags),
            fd_location: self.fd_location.map(i16::from_be),
            opaque: u16::from_be(self.opaque),
        }
    }
}

/// Catalog folder record (`HFSPlusCatalogFolder` in `<hfs/hfs_format.h>`).
#[repr(C, packed)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Pod, Zeroable)]
pub struct HfsPlusCatalogFolder {
    pub record_type: i16,
    pub flags: u16,
    pub valence: u32,
    pub folder_id: u32,
    pub create_date: u32,
    pub content_mod_date: u32,
    pub attribute_mod_date: u32,
    pub access_date: u32,
    pub backup_date: u32,
    pub bsd_info: HfsPlusBsdInfo,
    /// Opaque `FndrDirInfo`.
    pub user_info: [u8; 16],
    /// Opaque `FndrOpaqueInfo`.
    pub finder_info: [u8; 16],
    pub text_encoding: u32,
    pub folder_count: u32,
}
const_assert_eq!(size_of::<HfsPlusCatalogFolder>(), HfsPlusCatalogFolder::SIZE);

impl HfsPlusCatalogFolder {
    pub const SIZE: usize = 88;

    /// Parses a catalog folder record from its big-endian on-disk
    /// representation.
    pub fn from_big_endian_bytes(bytes: &[u8; Self::SIZE]) -> Self {
        bytemuck::pod_read_unaligned::<Self>(bytes).convert_from_big_endian()
    }

    /// Converts all fields from big-endian to host byte order.
    fn convert_from_big_endian(self) -> Self {
        Self {
            record_type: i16::from_be(self.record_type),
            flags: u16::from_be(self.flags),
            valence: u32::from_be(self.valence),
            folder_id: u32::from_be(self.folder_id),
            create_date: u32::from_be(self.create_date),
            content_mod_date: u32::from_be(self.content_mod_date),
            attribute_mod_date: u32::from_be(self.attribute_mod_date),
            access_date: u32::from_be(self.access_date),
            backup_date: u32::from_be(self.backup_date),
            bsd_info: self.bsd_info.convert_from_big_endian(),
            user_info: self.user_info,
            finder_info: self.finder_info,
            text_encoding: u32::from_be(self.text_encoding),
            folder_count: u32::from_be(self.folder_count),
        }
    }
}

/// Catalog file record (`HFSPlusCatalogFile` in `<hfs/hfs_format.h>`).
#[repr(C, packed)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Pod, Zeroable)]
pub struct HfsPlusCatalogFile {
    pub record_type: i16,
    pub flags: u16,
    pub reserved1: u32,
    pub file_id: u32,
    pub create_date: u32,
    pub content_mod_date: u32,
    pub attribute_mod_date: u32,
    pub access_date: u32,
    pub backup_date: u32,
    pub bsd_info: HfsPlusBsdInfo,
    pub user_info: FinderFileInfo,
    /// Opaque `FndrOpaqueInfo`.
    pub finder_info: [u8; 16],
    pub text_encoding: u32,
    pub reserved2: u32,
    pub data_fork: HfsPlusForkData,
    pub resource_fork: HfsPlusForkData,
}
const_assert_eq!(size_of::<HfsPlusCatalogFile>(), HfsPlusCatalogFile::SIZE);

impl HfsPlusCatalogFile {
    pub const SIZE: usize = 248;

    /// Parses a catalog file record from its big-endian on-disk
    /// representation.
    pub fn from_big_endian_bytes(bytes: &[u8; Self::SIZE]) -> Self {
        bytemuck::pod_read_unaligned::<Self>(bytes).convert_from_big_endian()
    }

    /// Converts all fields from big-endian to host byte order.
    fn convert_from_big_endian(self) -> Self {
        Self {
            record_type: i16::from_be(self.record_type),
            flags: u16::from_be(self.flags),
            reserved1: u32::from_be(self.reserved1),
            file_id: u32::from_be(self.file_id),
            create_date: u32::from_be(self.create_date),
            content_mod_date: u32::from_be(self.content_mod_date),
            attribute_mod_date: u32::from_be(self.attribute_mod_date),
            access_date: u32::from_be(self.access_date),
            backup_date: u32::from_be(self.backup_date),
            bsd_info: self.bsd_info.convert_from_big_endian(),
            user_info: self.user_info.convert_from_big_endian(),
            finder_info: self.finder_info,
            text_encoding: u32::from_be(self.text_encoding),
            reserved2: u32::from_be(self.reserved2),
            data_fork: self.data_fork.convert_from_big_endian(),
            resource_fork: self.resource_fork.convert_from_big_endian(),
        }
    }
}

/// B-Tree node descriptor (`BTNodeDescriptor` in `<hfs/hfs_format.h>`).
#[repr(C, packed)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Pod, Zeroable)]
pub struct BtNodeDescriptor {
    pub f_link: u32,
    pub b_link: u32,
    pub kind: i8,
    pub height: u8,
    pub num_records: u16,
    pub reserved: u16,
}
const_assert_eq!(size_of::<BtNodeDescriptor>(), BtNodeDescriptor::SIZE);

impl BtNodeDescriptor {
    pub const SIZE: usize = 14;

    /// Parses a B-Tree node descriptor from its big-endian on-disk
    /// representation.
    pub fn from_big_endian_bytes(bytes: &[u8; Self::SIZE]) -> Self {
        bytemuck::pod_read_unaligned::<Self>(bytes).convert_from_big_endian()
    }

    /// Converts all fields from big-endian to host byte order.
    fn convert_from_big_endian(self) -> Self {
        Self {
            f_link: u32::from_be(self.f_link),
            b_link: u32::from_be(self.b_link),
            kind: self.kind,
            height: self.height,
            num_records: u16::from_be(self.num_records),
            reserved: u16::from_be(self.reserved),
        }
    }
}

/// B-Tree header record (`BTHeaderRec` in `<hfs/hfs_format.h>`).
#[repr(C, packed)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Pod, Zeroable)]
pub struct BtHeaderRec {
    pub tree_depth: u16,
    pub root_node: u32,
    pub leaf_records: u32,
    pub first_leaf_node: u32,
    pub last_leaf_node: u32,
    pub node_size: u16,
    pub max_key_length: u16,
    pub total_nodes: u32,
    pub free_nodes: u32,
    pub reserved1: u16,
    pub clump_size: u32,
    pub btree_type: u8,
    pub key_compare_type: u8,
    pub attributes: u32,
    pub reserved3: [u32; 16],
}
const_assert_eq!(size_of::<BtHeaderRec>(), BtHeaderRec::SIZE);

impl BtHeaderRec {
    pub const SIZE: usize = 106;

    /// Parses a B-Tree header record from its big-endian on-disk
    /// representation.
    pub fn from_big_endian_bytes(bytes: &[u8; Self::SIZE]) -> Self {
        bytemuck::pod_read_unaligned::<Self>(bytes).convert_from_big_endian()
    }

    /// Converts all fields from big-endian to host byte order.
    fn convert_from_big_endian(self) -> Self {
        Self {
            tree_depth: u16::from_be(self.tree_depth),
            root_node: u32::from_be(self.root_node),
            leaf_records: u32::from_be(self.leaf_records),
            first_leaf_node: u32::from_be(self.first_leaf_node),
            last_leaf_node: u32::from_be(self.last_leaf_node),
            node_size: u16::from_be(self.node_size),
            max_key_length: u16::from_be(self.max_key_length),
            total_nodes: u32::from_be(self.total_nodes),
            free_nodes: u32::from_be(self.free_nodes),
            reserved1: u16::from_be(self.reserved1),
            clump_size: u32::from_be(self.clump_size),
            btree_type: self.btree_type,
            key_compare_type: self.key_compare_type,
            attributes: u32::from_be(self.attributes),
            reserved3: self.reserved3.map(u32::from_be),
        }
    }
}

/// Parsed payload of a catalog B-Tree leaf record.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum HfsCatalogRecord {
    Folder(HfsPlusCatalogFolder),
    File(HfsPlusCatalogFile),
    Thread,
}

impl HfsCatalogRecord {
    fn from_be_slice(data_bytes: &[u8]) -> Option<Self> {
        let (record_type_bytes, _) = data_bytes.split_first_chunk::<2>()?;
        match i16::from_be_bytes(*record_type_bytes) {
            HFS_PLUS_FOLDER_RECORD => {
                let (bytes, _) =
                    data_bytes.split_first_chunk::<{ HfsPlusCatalogFolder::SIZE }>()?;
                Some(Self::Folder(HfsPlusCatalogFolder::from_big_endian_bytes(bytes)))
            }
            HFS_PLUS_FILE_RECORD => {
                let (bytes, _) = data_bytes.split_first_chunk::<{ HfsPlusCatalogFile::SIZE }>()?;
                Some(Self::File(HfsPlusCatalogFile::from_big_endian_bytes(bytes)))
            }
            HFS_PLUS_FOLDER_THREAD_RECORD | HFS_PLUS_FILE_THREAD_RECORD => Some(Self::Thread),
            _ => None,
        }
    }
}

/// A single entry yielded by [`HfsBTreeIterator`].
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct HfsBTreeEntry {
    pub path: Vec<u16>,
    pub unexported: bool,
    pub record: HfsCatalogRecord,
}

/// Converts an ASCII byte string to UTF-16 code units at compile time.
const fn ascii_to_utf16<const N: usize>(ascii: &[u8; N]) -> [u16; N] {
    let mut utf16 = [0u16; N];
    let mut i = 0;
    while i < N {
        utf16[i] = ascii[i] as u16;
        i += 1;
    }
    utf16
}

/// Decodes big-endian `u16` values from `bytes`. A trailing odd byte is
/// ignored.
fn be_u16s(bytes: &[u8]) -> impl DoubleEndedIterator<Item = u16> + '_ {
    bytes.as_chunks::<2>().0.iter().map(|&chunk| u16::from_be_bytes(chunk))
}

/// Reserved HFS+ catalog key for private metadata folder
/// (`"\0\0\0\0HFS+ Private Data"`).
const HFS_METADATA_FOLDER: [u16; 21] = ascii_to_utf16(b"\0\0\0\0HFS+ Private Data");

/// Reserved HFS+ catalog key for private directory metadata folder
/// (`".HFS+ Private Directory Data\r"`).
const HFS_DIR_METADATA_FOLDER: [u16; 29] = ascii_to_utf16(b".HFS+ Private Directory Data\r");

/// Key of a catalog B-Tree leaf record (`HFSPlusCatalogKey` in
/// `<hfs/hfs_format.h>`), excluding the leading `keyLength`.
struct CatalogKey {
    parent_id: u32,
    name: Vec<u16>,
}

impl CatalogKey {
    fn from_be_slice(key_bytes: &[u8]) -> Option<Self> {
        let (parent_id_bytes, rest) = key_bytes.split_first_chunk::<4>()?;
        let (name_len_bytes, rest) = rest.split_first_chunk::<2>()?;
        let name_byte_len = usize::from(u16::from_be_bytes(*name_len_bytes)) * 2;
        let name_bytes = rest.get(..name_byte_len)?;
        Some(Self {
            parent_id: u32::from_be_bytes(*parent_id_bytes),
            name: be_u16s(name_bytes).collect(),
        })
    }

    /// Checks if this is a Mac OS X reserved key that should not have it or
    /// its contents iterated over.
    fn is_unexported(&self) -> bool {
        self.name == HFS_DIR_METADATA_FOLDER || self.name == HFS_METADATA_FOLDER
    }
}

/// A loaded and validated B-Tree leaf node.
struct LeafNode {
    descriptor: BtNodeDescriptor,
    /// Per-record byte offsets into the node data, taken from the offset
    /// table at the end of the node. There are `num_records + 1` entries; the
    /// last entry marks the start of the node's free space.
    record_offsets: Vec<u16>,
    /// The number of records read from this node.
    records_read: usize,
}

impl LeafNode {
    /// Parses and validates the node descriptor and record offset table of
    /// the leaf node in `node_data`.
    fn parse(node_data: &[u8]) -> Option<Self> {
        let (descriptor_bytes, _) = node_data.split_first_chunk::<{ BtNodeDescriptor::SIZE }>()?;
        let descriptor = BtNodeDescriptor::from_big_endian_bytes(descriptor_bytes);
        if descriptor.kind != BT_LEAF_NODE {
            return None;
        }

        // Each B-Tree node ends with a table of `numRecords + 1` big-endian
        // `u16` offsets stored in reverse order at the end of the node. Entry
        // `i` gives the byte offset of record `i` within the node and the final
        // entry marks the start of free space. Records must lie between the
        // node descriptor and the offset table itself.
        let num_offsets = usize::from(descriptor.num_records) + 1;
        let table_size = num_offsets * 2;
        let records_end = node_data.len().checked_sub(table_size)?;
        if records_end < BtNodeDescriptor::SIZE {
            return None;
        }

        let record_offsets: Vec<u16> = be_u16s(&node_data[records_end..]).rev().collect();

        let offsets_in_bounds = record_offsets
            .iter()
            .all(|&offset| (BtNodeDescriptor::SIZE..=records_end).contains(&usize::from(offset)));
        if !offsets_in_bounds || !record_offsets.is_sorted() {
            return None;
        }

        Some(Self { descriptor, record_offsets, records_read: 0 })
    }

    /// Parses the key and catalog record of the next unread record in
    /// `node_data`.
    fn parse_current_record(
        &self,
        node_data: &[u8],
        max_key_length: u16,
    ) -> Option<(CatalogKey, HfsCatalogRecord)> {
        let start = usize::from(*self.record_offsets.get(self.records_read)?);
        let end = usize::from(*self.record_offsets.get(self.records_read + 1)?);
        let record_bytes = node_data.get(start..end)?;

        let (key_len_bytes, rest) = record_bytes.split_first_chunk::<2>()?;
        let key_length = u16::from_be_bytes(*key_len_bytes);
        if key_length > max_key_length {
            return None;
        }

        // The data portion of a keyed record begins immediately after the key,
        // at `record_offset + sizeof(keyLength) + keyLength`.
        let (key_bytes, data_bytes) = rest.split_at_checked(usize::from(key_length))?;
        let key = CatalogKey::from_be_slice(key_bytes)?;
        let record = HfsCatalogRecord::from_be_slice(data_bytes)?;
        Some((key, record))
    }

    fn is_fully_read(&self) -> bool {
        self.records_read >= usize::from(self.descriptor.num_records)
    }
}

/// Iterates over leaf records in the HFS+ catalog B-Tree.
///
/// Rust port of `safe_browsing::dmg::HFSBTreeIterator`.
pub struct HfsBTreeIterator {
    stream: HfsForkReadStream,
    header: BtHeaderRec,
    folder_cnid_map: HashMap<u32, Vec<u16>>,
    unexported_parents: HashSet<u32>,
    /// The total number of leaf records read from all the leaf nodes.
    leaf_records_read: u32,
    /// The node ID of the leaf being read.
    current_leaf_number: u32,
    /// The raw data of the leaf node at `current_leaf_number`. Reused across
    /// nodes to avoid reallocating.
    leaf_data: Vec<u8>,
    /// The leaf node at `current_leaf_number`, or `None` if it has not been
    /// read yet.
    current_leaf: Option<LeafNode>,
}

impl HfsBTreeIterator {
    pub fn try_new(mut stream: HfsForkReadStream) -> Result<Self, HfsError> {
        if stream.seek(SeekFrom::Start(0)).map_err(|_| HfsError::IoError)? != 0 {
            return Err(HfsError::IoError);
        }

        let mut node_bytes = [0u8; BtNodeDescriptor::SIZE];
        stream.read_exact(&mut node_bytes).map_err(|_| HfsError::IoError)?;
        let node = BtNodeDescriptor::from_big_endian_bytes(&node_bytes);
        if node.kind != BT_HEADER_NODE {
            return Err(HfsError::InvalidBTree);
        }

        let mut header_bytes = [0u8; BtHeaderRec::SIZE];
        stream.read_exact(&mut header_bytes).map_err(|_| HfsError::IoError)?;
        let header = BtHeaderRec::from_big_endian_bytes(&header_bytes);
        let node_size = usize::from(header.node_size);
        if node_size < BtNodeDescriptor::SIZE {
            return Err(HfsError::InvalidBTree);
        }

        let current_leaf_number = header.first_leaf_node;
        Ok(Self {
            stream,
            header,
            folder_cnid_map: HashMap::new(),
            unexported_parents: HashSet::new(),
            leaf_records_read: 0,
            current_leaf_number,
            leaf_data: vec![0u8; node_size],
            current_leaf: None,
        })
    }

    fn build_entry(&mut self, key: CatalogKey, record: HfsCatalogRecord) -> HfsBTreeEntry {
        match record {
            HfsCatalogRecord::Folder(folder) => {
                let unexported =
                    key.is_unexported() || self.unexported_parents.contains(&key.parent_id);
                if unexported {
                    self.unexported_parents.insert(folder.folder_id);
                }

                let path = if key.parent_id != 0
                    && let Some(parent_path) = self.folder_cnid_map.get(&key.parent_id)
                {
                    Self::join_path(parent_path, &key.name)
                } else {
                    key.name
                };
                self.folder_cnid_map.insert(folder.folder_id, path.clone());
                HfsBTreeEntry { path, unexported, record }
            }
            HfsCatalogRecord::File(_) => {
                // Matching the C++ implementation, an unseen parent CNID is
                // registered with an empty path, so later folders under it get
                // a leading separator.
                let parent_path = self.folder_cnid_map.entry(key.parent_id).or_default();
                let path = Self::join_path(parent_path, &key.name);
                let unexported = self.unexported_parents.contains(&key.parent_id);
                HfsBTreeEntry { path, unexported, record }
            }
            HfsCatalogRecord::Thread => {
                // Thread records are used to quickly locate a file or folder by
                // CNID. They are not needed during catalog iteration; the next
                // record is located via the node's offset table.
                HfsBTreeEntry { path: Vec::new(), unexported: false, record }
            }
        }
    }

    fn join_path(parent: &[u16], name: &[u16]) -> Vec<u16> {
        let mut path = Vec::with_capacity(parent.len() + 1 + name.len());
        path.extend_from_slice(parent);
        path.push(FILE_PATH_SEPARATOR);
        path.extend_from_slice(name);
        path
    }

    /// Attempts to seek to the node at `self.current_leaf_number`. Returns
    /// `true` on success and `false` on failure.
    fn seek_to_current_leaf(&mut self) -> bool {
        if self.current_leaf_number >= self.header.total_nodes {
            return false;
        }
        let offset = u64::from(self.current_leaf_number) * u64::from(self.header.node_size);
        self.stream.seek(SeekFrom::Start(offset)).is_ok()
    }

    /// Reads the node at `self.current_leaf_number` into `self.leaf_data` and
    /// parses it as a leaf node.
    fn read_current_leaf(&mut self) -> Option<LeafNode> {
        if !self.seek_to_current_leaf() || self.stream.read_exact(&mut self.leaf_data).is_err() {
            return None;
        }
        LeafNode::parse(&self.leaf_data)
    }
}

impl Iterator for HfsBTreeIterator {
    type Item = HfsBTreeEntry;

    fn next(&mut self) -> Option<Self::Item> {
        if self.leaf_records_read >= self.header.leaf_records {
            return None;
        }
        if self.current_leaf.is_none() {
            self.current_leaf = self.read_current_leaf();
        }
        let leaf = self.current_leaf.as_mut()?;
        let (key, record) =
            leaf.parse_current_record(&self.leaf_data, self.header.max_key_length)?;

        self.leaf_records_read += 1;
        leaf.records_read += 1;

        // If all records from this leaf have been read, follow the forward link
        // to the next B-Tree leaf node.
        if leaf.is_fully_read() {
            self.current_leaf_number = leaf.descriptor.f_link;
            self.current_leaf = None;
        }

        Some(self.build_entry(key, record))
    }
}

/// Iterates over files and directories in an HFS+ or HFSX volume.
pub struct HfsIterator {
    stream: SharedStream,
    volume_header: HfsPlusVolumeHeader,
    catalog: HfsBTreeIterator,
    current_item: Option<HfsBTreeEntry>,
}

impl HfsIterator {
    /// Reads the `HFSPlusVolumeHeader` at byte offset 1024 and initializes the
    /// catalog B-Tree iterator.
    pub fn open(stream: SharedStream) -> Result<Self, HfsError> {
        let mut header_bytes = [0u8; VOLUME_HEADER_SIZE];
        stream
            .read_exact_at(VOLUME_HEADER_OFFSET, &mut header_bytes)
            .map_err(|_| HfsError::IoError)?;

        let volume_header = HfsPlusVolumeHeader::from_big_endian_bytes(&header_bytes);
        if volume_header.signature != HFS_PLUS_SIG_WORD && volume_header.signature != HFSX_SIG_WORD
        {
            return Err(HfsError::InvalidVolumeHeader);
        }
        if volume_header.block_size == 0 {
            return Err(HfsError::InvalidVolumeHeader);
        }

        let catalog_file = HfsForkReadStream::new(
            stream.clone(),
            volume_header.block_size,
            volume_header.catalog_file,
        );
        let catalog = HfsBTreeIterator::try_new(catalog_file)?;

        Ok(Self { stream, volume_header, catalog, current_item: None })
    }

    /// Advances the iterator to the next exported file or folder record.
    /// Returns `false` at end-of-catalog or on corrupt record data.
    pub fn next_item(&mut self) -> bool {
        self.current_item = self.catalog.find(|entry| {
            !entry.unexported
                && matches!(entry.record, HfsCatalogRecord::Folder(_) | HfsCatalogRecord::File(_))
        });
        self.current_item.is_some()
    }

    /// Returns the catalog record of the current item, if any.
    fn current_record(&self) -> Option<&HfsCatalogRecord> {
        self.current_item.as_ref().map(|entry| &entry.record)
    }

    /// Returns true if the current item is a directory.
    pub fn is_directory(&self) -> bool {
        matches!(self.current_record(), Some(HfsCatalogRecord::Folder(_)))
    }

    /// Returns true if the current item is a file.
    pub fn is_file(&self) -> bool {
        matches!(self.current_record(), Some(HfsCatalogRecord::File(_)))
    }

    /// Returns true if the current item is a symbolic link.
    pub fn is_symbolic_link(&self) -> bool {
        let file_mode = match self.current_record() {
            Some(HfsCatalogRecord::Folder(folder)) => folder.bsd_info.file_mode,
            Some(HfsCatalogRecord::File(file)) => file.bsd_info.file_mode,
            _ => return false,
        };
        (file_mode & S_IFMT) == S_IFLNK
    }

    /// Returns true if the current item is a hard link.
    pub fn is_hard_link(&self) -> bool {
        match self.current_record() {
            Some(HfsCatalogRecord::File(file)) => {
                file.user_info.fd_type == HARD_LINK_FILE_TYPE
                    && file.user_info.fd_creator == HFS_PLUS_CREATOR
            }
            _ => false,
        }
    }

    /// Returns true if the current item is `decmpfs`-compressed
    /// (`UF_COMPRESSED`).
    pub fn is_decmpfs_compressed(&self) -> bool {
        match self.current_record() {
            Some(HfsCatalogRecord::File(file)) => (file.bsd_info.owner_flags & UF_COMPRESSED) != 0,
            _ => false,
        }
    }

    /// Returns the full UTF-16 path of the current catalog item.
    pub fn path(&self) -> &[u16] {
        self.current_item.as_ref().map_or(&[], |entry| entry.path.as_slice())
    }

    /// Returns a reader for the data fork of the current file item, or `None`
    /// if the current item is a directory or hard link.
    pub fn read_stream(&self) -> Option<Box<dyn ReadSeek>> {
        if self.is_hard_link() {
            return None;
        }
        match self.current_record() {
            Some(HfsCatalogRecord::File(file)) => Some(Box::new(HfsForkReadStream::new(
                self.stream.clone(),
                self.volume_header.block_size,
                file.data_fork,
            ))),
            _ => None,
        }
    }
}
