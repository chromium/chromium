// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Pure-Rust parser for thin and fat (universal) macOS Mach-O binary images.
//!
//! Rust port of `chrome/common/safe_browsing/mach_o_image_reader_mac.{h,cc}`.
//! Kept free of `unsafe` code; all C++ FFI glue lives in `ffi.rs`.

pub const MH_MAGIC: u32 = 0xfeedface;
pub const MH_CIGAM: u32 = 0xcefaedfe;
pub const MH_MAGIC_64: u32 = 0xfeedfacf;
pub const MH_CIGAM_64: u32 = 0xcffaedfe;
pub const FAT_MAGIC: u32 = 0xcafebabe;
pub const FAT_CIGAM: u32 = 0xbebafeca;
pub const LC_CODE_SIGNATURE: u32 = 0x1d;

pub const FAT_HEADER_SIZE: usize = 8;
pub const FAT_ARCH_SIZE: usize = 20;
pub const MACH_HEADER_SIZE: usize = 28;
pub const MACH_HEADER_64_SIZE: usize = 32;
pub const LOAD_COMMAND_SIZE: usize = 8;
pub const LINKEDIT_DATA_COMMAND_SIZE: usize = 16;

fn read_ne_u32(data: &[u8], offset: usize) -> Option<u32> {
    let end = offset.checked_add(4)?;
    let bytes: [u8; 4] = data.get(offset..end)?.try_into().ok()?;
    Some(u32::from_ne_bytes(bytes))
}

/// Bounds-checked subslice helper matching C++ `ByteSlice::RangeCheck`.
fn slice_checked(data: &[u8], offset: usize, size: usize) -> Option<&[u8]> {
    // Reject `offset == data.len()` even when `size == 0` to match
    // `ByteSlice::RangeCheck`.
    if offset >= data.len() {
        return None;
    }
    let end = offset.checked_add(size)?;
    data.get(offset..end)
}

/// 4-byte aligned buffer holding `mach_header` (28 bytes) or `mach_header_64`
/// (32 bytes) so C++ callers can safely cast the slice pointer to
/// `const mach_header*` or `const mach_header_64*`.
#[repr(C, align(4))]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
struct AlignedMachHeader {
    bytes: [u8; MACH_HEADER_64_SIZE],
}

/// Represents a single Mach-O `load_command` and its full payload bytes.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct LoadCommand {
    pub data: Vec<u8>,
}

impl LoadCommand {
    pub fn cmd(&self) -> u32 {
        read_ne_u32(&self.data, 0).unwrap_or(0)
    }

    pub fn cmdsize(&self) -> u32 {
        read_ne_u32(&self.data, 4).unwrap_or(0)
    }
}

/// Errors that can occur when parsing a Mach-O image.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum MachOError {
    EmptyImage,
    InvalidMagic,
    TruncatedHeader,
    InvalidFatArch,
    TruncatedLoadCommand,
}

#[derive(Debug, Clone, PartialEq, Eq)]
enum MachOImageKind {
    Fat(Vec<MachOImage>),
    Thin {
        is_64_bit: bool,
        header: AlignedMachHeader,
        commands: Vec<LoadCommand>,
        code_signature: Option<Vec<u8>>,
    },
}

/// Parsed representation of a thin or fat Mach-O binary image.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct MachOImage {
    kind: MachOImageKind,
}

impl MachOImage {
    /// Returns true if `magic` is any thin or fat Mach-O magic constant (in
    /// either little- or big-endian byte order).
    pub fn is_mach_o_magic_value(magic: u32) -> bool {
        matches!(magic, FAT_MAGIC | FAT_CIGAM | MH_MAGIC | MH_CIGAM | MH_MAGIC_64 | MH_CIGAM_64)
    }

    /// Parses and validates a thin or fat Mach-O binary image from `image`.
    pub fn parse(image: &[u8]) -> Result<Self, MachOError> {
        if image.is_empty() {
            return Err(MachOError::EmptyImage);
        }

        let magic = read_ne_u32(image, 0).ok_or(MachOError::TruncatedHeader)?;
        if magic == FAT_MAGIC || magic == FAT_CIGAM {
            Self::parse_fat(image, magic)
        } else {
            Self::parse_thin(image, magic)
        }
    }

    fn parse_fat(image: &[u8], magic: u32) -> Result<Self, MachOError> {
        if slice_checked(image, 0, FAT_HEADER_SIZE).is_none() {
            return Err(MachOError::TruncatedHeader);
        }
        let do_swap = magic == FAT_CIGAM;
        let raw_nfat = read_ne_u32(image, 4).ok_or(MachOError::TruncatedHeader)?;
        let nfat_arch = if do_swap { raw_nfat.swap_bytes() } else { raw_nfat };

        let mut offset = FAT_HEADER_SIZE;
        let mut fat_images = Vec::new();
        for _ in 0..nfat_arch {
            let arch_bytes =
                slice_checked(image, offset, FAT_ARCH_SIZE).ok_or(MachOError::InvalidFatArch)?;
            let raw_arch_offset = read_ne_u32(arch_bytes, 8).ok_or(MachOError::InvalidFatArch)?;
            let raw_arch_size = read_ne_u32(arch_bytes, 12).ok_or(MachOError::InvalidFatArch)?;
            let arch_offset =
                if do_swap { raw_arch_offset.swap_bytes() } else { raw_arch_offset } as usize;
            let arch_size =
                if do_swap { raw_arch_size.swap_bytes() } else { raw_arch_size } as usize;

            // Cannot refer back to headers of previous arches to cause
            // recursive processing (https://crbug.com/40459221).
            if arch_offset < offset {
                return Err(MachOError::InvalidFatArch);
            }

            let slice =
                slice_checked(image, arch_offset, arch_size).ok_or(MachOError::InvalidFatArch)?;
            if slice.is_empty() {
                return Err(MachOError::InvalidFatArch);
            }

            // Fat binaries may only contain thin Mach-O slices, never nested
            // fat binaries.
            let child_magic = read_ne_u32(slice, 0).ok_or(MachOError::TruncatedHeader)?;
            if child_magic == FAT_MAGIC || child_magic == FAT_CIGAM {
                return Err(MachOError::InvalidFatArch);
            }
            let child = Self::parse_thin(slice, child_magic)?;
            fat_images.push(child);

            offset = offset.checked_add(FAT_ARCH_SIZE).ok_or(MachOError::InvalidFatArch)?;
        }

        Ok(Self { kind: MachOImageKind::Fat(fat_images) })
    }

    fn parse_thin(image: &[u8], magic: u32) -> Result<Self, MachOError> {
        let (is_64_bit, do_swap) = match magic {
            MH_MAGIC => (false, false),
            MH_CIGAM => (false, true),
            MH_MAGIC_64 => (true, false),
            MH_CIGAM_64 => (true, true),
            _ => return Err(MachOError::InvalidMagic),
        };

        let header_size = if is_64_bit { MACH_HEADER_64_SIZE } else { MACH_HEADER_SIZE };
        let header_slice =
            slice_checked(image, 0, header_size).ok_or(MachOError::TruncatedHeader)?;
        let mut header = AlignedMachHeader { bytes: [0u8; MACH_HEADER_64_SIZE] };
        header.bytes[..header_size].copy_from_slice(header_slice);

        let raw_ncmds = read_ne_u32(image, 16).ok_or(MachOError::TruncatedHeader)?;
        let num_commands = if do_swap { raw_ncmds.swap_bytes() } else { raw_ncmds };

        let mut offset = header_size;
        let mut commands = Vec::new();
        for _ in 0..num_commands {
            let base_cmd = slice_checked(image, offset, LOAD_COMMAND_SIZE)
                .ok_or(MachOError::TruncatedLoadCommand)?;
            let raw_cmdsize = read_ne_u32(base_cmd, 4).ok_or(MachOError::TruncatedLoadCommand)?;
            let cmdsize = if do_swap { raw_cmdsize.swap_bytes() } else { raw_cmdsize } as usize;

            // If `cmdsize` is smaller than `sizeof(load_command)`, keep only
            // the 8-byte base struct and advance by `sizeof(load_command)`
            // (https://crbug.com/40432691).
            if cmdsize < LOAD_COMMAND_SIZE {
                commands.push(LoadCommand { data: base_cmd.to_vec() });
                offset = offset
                    .checked_add(LOAD_COMMAND_SIZE)
                    .ok_or(MachOError::TruncatedLoadCommand)?;
                continue;
            }

            let full_cmd =
                slice_checked(image, offset, cmdsize).ok_or(MachOError::TruncatedLoadCommand)?;
            commands.push(LoadCommand { data: full_cmd.to_vec() });
            offset = offset.checked_add(cmdsize).ok_or(MachOError::TruncatedLoadCommand)?;
        }

        let code_signature = Self::extract_code_signature(image, &commands);

        Ok(Self { kind: MachOImageKind::Thin { is_64_bit, header, commands, code_signature } })
    }

    fn extract_code_signature(image: &[u8], commands: &[LoadCommand]) -> Option<Vec<u8>> {
        // Matching C++ `MachOImageReader::GetCodeSignatureInfo`, `cmd`,
        // `dataoff`, and `datasize` are read in native byte order without
        // `do_swap`.
        for command in commands {
            if command.cmd() == LC_CODE_SIGNATURE {
                if command.data.len() < LINKEDIT_DATA_COMMAND_SIZE {
                    return None;
                }
                let dataoff = read_ne_u32(&command.data, 8)? as usize;
                let datasize = read_ne_u32(&command.data, 12)? as usize;
                let sig = slice_checked(image, dataoff, datasize)?;
                return Some(sig.to_vec());
            }
        }
        None
    }

    /// Returns true if this image is a fat (universal) Mach-O binary.
    pub fn is_fat(&self) -> bool {
        matches!(self.kind, MachOImageKind::Fat(_))
    }

    /// Returns the architecture slices contained in this fat binary, or an
    /// empty slice for a thin binary.
    pub fn fat_images(&self) -> &[MachOImage] {
        match &self.kind {
            MachOImageKind::Fat(images) => images,
            MachOImageKind::Thin { .. } => &[],
        }
    }

    /// Moves and returns the architecture slices from this fat binary, leaving
    /// `self.fat_images()` empty while `self.is_fat()` remains true.
    pub fn take_fat_images(&mut self) -> Vec<MachOImage> {
        match &mut self.kind {
            MachOImageKind::Fat(images) => std::mem::take(images),
            MachOImageKind::Thin { .. } => Vec::new(),
        }
    }

    pub fn is_64_bit(&self) -> bool {
        match &self.kind {
            MachOImageKind::Thin { is_64_bit, .. } => *is_64_bit,
            MachOImageKind::Fat(_) => false,
        }
    }

    pub fn mach_header_bytes(&self) -> &[u8] {
        match &self.kind {
            MachOImageKind::Thin { header, .. } => &header.bytes[..MACH_HEADER_SIZE],
            MachOImageKind::Fat(_) => &[],
        }
    }

    pub fn mach_header_64_bytes(&self) -> &[u8] {
        match &self.kind {
            MachOImageKind::Thin { is_64_bit: true, header, .. } => {
                &header.bytes[..MACH_HEADER_64_SIZE]
            }
            _ => &[],
        }
    }

    pub fn file_type(&self) -> u32 {
        match &self.kind {
            MachOImageKind::Thin { header, .. } => read_ne_u32(&header.bytes, 12).unwrap_or(0),
            MachOImageKind::Fat(_) => 0,
        }
    }

    pub fn load_commands(&self) -> &[LoadCommand] {
        match &self.kind {
            MachOImageKind::Thin { commands, .. } => commands,
            MachOImageKind::Fat(_) => &[],
        }
    }

    pub fn code_signature(&self) -> Option<&[u8]> {
        match &self.kind {
            MachOImageKind::Thin { code_signature, .. } => code_signature.as_deref(),
            MachOImageKind::Fat(_) => None,
        }
    }
}
