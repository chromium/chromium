// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Universal Disk Image Format (UDIF) container parser.
//!
//! Rust port of `chrome/utility/safe_browsing/mac/udif.{h,cc}`.

use std::collections::BTreeMap;
use std::mem::size_of;

use base64::engine::general_purpose::STANDARD as BASE64_STANDARD;
use base64::Engine as _;
use bytemuck::{Pod, Zeroable};
use static_assertions::const_assert_eq;
use xml::reader::{EventReader, ParserConfig, XmlEvent};

use crate::{ReadSeek, SharedStream};

/// Default UDIF sector size in bytes.
pub const UDIF_SECTOR_SIZE: u16 = 512;

/// Size in bytes of the `UDIFResourceFile` (`koly`) trailer.
const UDIF_RESOURCE_FILE_SIZE: usize = 512;

/// Magic signature (`'koly'`) for `UDIFResourceFile`.
pub const UDIF_RESOURCE_FILE_SIGNATURE: u32 = u32::from_be_bytes(*b"koly");

/// Expected version for `UDIFResourceFile`.
pub const UDIF_RESOURCE_FILE_VERSION: u32 = 4;

/// Size in bytes of the `UDIFBlockData` (`mish`) header.
const UDIF_BLOCK_DATA_SIZE: usize = 204;

/// Magic signature (`'mish'`) for `UDIFBlockData`.
pub const UDIF_BLOCK_DATA_SIGNATURE: u32 = u32::from_be_bytes(*b"mish");

/// Expected version for `UDIFBlockData`.
pub const UDIF_BLOCK_DATA_VERSION: u32 = 1;

/// Size in bytes of a single `UDIFBlockChunk` entry.
const UDIF_BLOCK_CHUNK_SIZE: usize = 40;

/// Maximum XML nesting depth permitted when parsing the `blkx` plist.
const MAX_PLIST_DEPTH: usize = 32;

/// Errors that can occur when parsing a UDIF image.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum UdifError {
    IoError,
    InvalidTrailer,
    InvalidPlist,
    InvalidBlockData,
}

/// Packed UDIF checksum structure (`UDIFChecksum` in `udif.cc`).
#[repr(C, packed)]
#[derive(Copy, Clone, Pod, Zeroable)]
struct UdifChecksum {
    checksum_type: u32,
    size: u32,
    data: [u32; 32],
}
const_assert_eq!(size_of::<UdifChecksum>(), 136);

impl UdifChecksum {
    /// Converts all fields from big-endian to host byte order
    /// (`ConvertToHost`).
    fn convert_from_big_endian(self) -> Self {
        Self {
            checksum_type: u32::from_be(self.checksum_type),
            size: u32::from_be(self.size),
            data: self.data.map(u32::from_be),
        }
    }
}

/// Packed 512-byte UDIF `koly` trailer (`UDIFResourceFile` in `udif.cc`).
#[repr(C, packed)]
#[derive(Copy, Clone, Pod, Zeroable)]
pub struct UdifResourceFile {
    pub signature: u32,
    pub version: u32,
    header_size: u32,
    flags: u32,
    running_data_fork_offset: u64,
    data_fork_offset: u64,
    data_fork_length: u64,
    rsrc_fork_offset: u64,
    rsrc_fork_length: u64,
    segment_number: u32,
    segment_count: u32,
    segment_id: [u8; 16],
    data_checksum: UdifChecksum,
    pub plist_offset: u64,
    pub plist_length: u64,
    reserved1: [u8; 64],
    pub code_signature_offset: u64,
    pub code_signature_length: u64,
    reserved2: [u8; 40],
    main_checksum: UdifChecksum,
    image_variant: u32,
    sector_count: u64,
    reserved3: u32,
    reserved4: u32,
    reserved5: u32,
}
const_assert_eq!(size_of::<UdifResourceFile>(), UDIF_RESOURCE_FILE_SIZE);

impl UdifResourceFile {
    /// Converts all fields from big-endian to host byte order
    /// (`ConvertToHost`).
    fn convert_from_big_endian(self) -> Self {
        Self {
            signature: u32::from_be(self.signature),
            version: u32::from_be(self.version),
            header_size: u32::from_be(self.header_size),
            flags: u32::from_be(self.flags),
            running_data_fork_offset: u64::from_be(self.running_data_fork_offset),
            data_fork_offset: u64::from_be(self.data_fork_offset),
            data_fork_length: u64::from_be(self.data_fork_length),
            rsrc_fork_offset: u64::from_be(self.rsrc_fork_offset),
            rsrc_fork_length: u64::from_be(self.rsrc_fork_length),
            segment_number: u32::from_be(self.segment_number),
            segment_count: u32::from_be(self.segment_count),
            segment_id: self.segment_id,
            data_checksum: self.data_checksum.convert_from_big_endian(),
            plist_offset: u64::from_be(self.plist_offset),
            plist_length: u64::from_be(self.plist_length),
            reserved1: self.reserved1,
            code_signature_offset: u64::from_be(self.code_signature_offset),
            code_signature_length: u64::from_be(self.code_signature_length),
            reserved2: self.reserved2,
            main_checksum: self.main_checksum.convert_from_big_endian(),
            image_variant: u32::from_be(self.image_variant),
            sector_count: u64::from_be(self.sector_count),
            reserved3: u32::from_be(self.reserved3),
            reserved4: u32::from_be(self.reserved4),
            reserved5: u32::from_be(self.reserved5),
        }
    }
}

/// Chunk compression/layout type (`UDIFBlockChunk::Type` in `udif.cc`).
///
/// Represented as a `#[repr(transparent)]` newtype over `u32` rather than a
/// Rust `enum` so that all 32-bit bit patterns (including unswapped big-endian
/// values and unrecognized chunk types) are valid for `bytemuck::Pod`.
#[repr(transparent)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Pod, Zeroable)]
pub struct UdifBlockChunkType(u32);

impl UdifBlockChunkType {
    pub const ZERO_FILL: Self = Self(0x0000_0000);
    pub const UNCOMPRESSED: Self = Self(0x0000_0001);
    pub const IGNORED: Self = Self(0x0000_0002);
    pub const COMPRESS_ADC: Self = Self(0x8000_0004);
    pub const COMPRESS_ZLIB: Self = Self(0x8000_0005);
    pub const COMPRESS_BZ2: Self = Self(0x8000_0006);
    pub const COMMENT: Self = Self(0x7fff_fffe);
    pub const LAST_BLOCK: Self = Self(0xffff_ffff);

    fn convert_from_big_endian(self) -> Self {
        Self(u32::from_be(self.0))
    }
}

/// A single 40-byte chunk descriptor within a UDIF `mish` block table
/// (`UDIFBlockChunk` in `udif.cc`).
#[repr(C, packed)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Pod, Zeroable)]
pub struct UdifBlockChunk {
    pub chunk_type: UdifBlockChunkType,
    pub comment: u32,
    pub start_sector: u64,
    pub sector_count: u64,
    pub compressed_offset: u64,
    pub compressed_length: u64,
}
const_assert_eq!(size_of::<UdifBlockChunk>(), UDIF_BLOCK_CHUNK_SIZE);

impl UdifBlockChunk {
    /// Converts all fields from big-endian to host byte order
    /// (`ConvertToHost`).
    fn convert_from_big_endian(self) -> Self {
        Self {
            chunk_type: self.chunk_type.convert_from_big_endian(),
            comment: u32::from_be(self.comment),
            start_sector: u64::from_be(self.start_sector),
            sector_count: u64::from_be(self.sector_count),
            compressed_offset: u64::from_be(self.compressed_offset),
            compressed_length: u64::from_be(self.compressed_length),
        }
    }
}

/// Packed 204-byte UDIF `mish` block header (`UDIFBlockData` in `udif.cc`).
#[repr(C, packed)]
#[derive(Copy, Clone, Pod, Zeroable)]
pub struct UdifBlockData {
    pub signature: u32,
    pub version: u32,
    pub start_sector: u64,
    pub sector_count: u64,
    data_offset: u64,
    buffers_needed: u32,
    block_descriptors: u32,
    reserved1: u32,
    reserved2: u32,
    reserved3: u32,
    reserved4: u32,
    reserved5: u32,
    reserved6: u32,
    checksum: UdifChecksum,
    pub chunk_count: u32,
}
const_assert_eq!(size_of::<UdifBlockData>(), UDIF_BLOCK_DATA_SIZE);

impl UdifBlockData {
    /// Converts all fields from big-endian to host byte order
    /// (`ConvertToHost`).
    fn convert_from_big_endian(self) -> Self {
        Self {
            signature: u32::from_be(self.signature),
            version: u32::from_be(self.version),
            start_sector: u64::from_be(self.start_sector),
            sector_count: u64::from_be(self.sector_count),
            data_offset: u64::from_be(self.data_offset),
            buffers_needed: u32::from_be(self.buffers_needed),
            block_descriptors: u32::from_be(self.block_descriptors),
            reserved1: u32::from_be(self.reserved1),
            reserved2: u32::from_be(self.reserved2),
            reserved3: u32::from_be(self.reserved3),
            reserved4: u32::from_be(self.reserved4),
            reserved5: u32::from_be(self.reserved5),
            reserved6: u32::from_be(self.reserved6),
            checksum: self.checksum.convert_from_big_endian(),
            chunk_count: u32::from_be(self.chunk_count),
        }
    }
}

/// Parsed UDIF `mish` block header and chunk table for a single partition.
///
/// Rust equivalent of `safe_browsing::dmg::UDIFBlock`.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct UdifBlock {
    signature: u32,
    version: u32,
    start_sector: u64,
    sector_count: u64,
    chunks: Vec<UdifBlockChunk>,
}

impl UdifBlock {
    /// Parses a big-endian `UDIFBlockData` header and its trailing
    /// `UDIFBlockChunk` entries from `block_data`.
    ///
    /// Rust equivalent of `UDIFBlock::ParseBlockData`.
    pub fn try_new(block_data: &[u8], sector_size: u16) -> Result<Self, UdifError> {
        let Some((header_bytes, rest)) = block_data.split_at_checked(size_of::<UdifBlockData>())
        else {
            log::debug!("UDIF block data is smaller than expected");
            return Err(UdifError::InvalidBlockData);
        };
        let header = bytemuck::try_from_bytes::<UdifBlockData>(header_bytes)
            .map_err(|_| UdifError::InvalidBlockData)?
            .convert_from_big_endian();

        // Make sure the number of sectors doesn't overflow.
        let Some(block_size) = usize::try_from(header.sector_count)
            .ok()
            .and_then(|sc| sc.checked_mul(usize::from(sector_size)))
        else {
            log::debug!("UDIF block size overflows");
            return Err(UdifError::InvalidBlockData);
        };

        // Make sure the block data contains the reported number of chunks.
        let chunk_layout = usize::try_from(header.chunk_count).ok().and_then(|cc| {
            let chunks_byte_len = cc.checked_mul(size_of::<UdifBlockChunk>())?;
            let expected_total = size_of::<UdifBlockData>().checked_add(chunks_byte_len)?;
            Some((cc, chunks_byte_len, expected_total))
        });
        let Some((chunk_count, chunks_byte_len, expected_total)) = chunk_layout else {
            log::debug!(
                "UDIF block does not contain reported number of chunks (chunk_count={} overflows), got {} bytes",
                { header.chunk_count },
                block_data.len()
            );
            return Err(UdifError::InvalidBlockData);
        };
        let Some((chunks_bytes, _)) = rest.split_at_checked(chunks_byte_len) else {
            log::debug!(
                "UDIF block does not contain reported number of chunks, {expected_total} bytes expected, got {}",
                block_data.len()
            );
            return Err(UdifError::InvalidBlockData);
        };
        let raw_chunks = bytemuck::try_cast_slice::<u8, UdifBlockChunk>(chunks_bytes)
            .map_err(|_| UdifError::InvalidBlockData)?;

        // Make sure that the chunk data isn't larger than the block reports.
        let mut chunks = Vec::with_capacity(chunk_count);
        let mut chunk_sectors: usize = 0;
        for (i, raw_chunk) in raw_chunks.iter().enumerate() {
            let chunk = raw_chunk.convert_from_big_endian();

            let Some(next_chunk_sectors) = usize::try_from(chunk.sector_count)
                .ok()
                .and_then(|sc| chunk_sectors.checked_add(sc))
            else {
                log::debug!("Total chunk sectors larger than reported block sectors");
                return Err(UdifError::InvalidBlockData);
            };
            chunk_sectors = next_chunk_sectors;
            if (chunk_sectors as u64) > header.sector_count {
                log::debug!("Total chunk sectors larger than reported block sectors");
                return Err(UdifError::InvalidBlockData);
            }

            let valid_compressed_range = usize::try_from(chunk.compressed_offset)
                .ok()
                .zip(usize::try_from(chunk.compressed_length).ok())
                .is_some_and(|(offset, length)| {
                    length <= block_size && offset.checked_add(length).is_some()
                });
            if !valid_compressed_range {
                log::debug!("UDIF chunk data length {i} overflows");
                return Err(UdifError::InvalidBlockData);
            }

            chunks.push(chunk);
        }

        Ok(Self {
            signature: header.signature,
            version: header.version,
            start_sector: header.start_sector,
            sector_count: header.sector_count,
            chunks,
        })
    }

    pub fn signature(&self) -> u32 {
        self.signature
    }

    pub fn version(&self) -> u32 {
        self.version
    }

    pub fn start_sector(&self) -> u64 {
        self.start_sector
    }

    pub fn sector_count(&self) -> u64 {
        self.sector_count
    }

    pub fn chunk_count(&self) -> usize {
        self.chunks.len()
    }

    pub fn chunk(&self, i: usize) -> Option<&UdifBlockChunk> {
        self.chunks.get(i)
    }

    pub fn chunks(&self) -> &[UdifBlockChunk] {
        self.chunks.as_slice()
    }

    fn log_debug(&self, partition_name: &str) {
        if !log::log_enabled!(log::Level::Debug) {
            return;
        }
        log::debug!("Name: {partition_name}");
        log::debug!(
            "StartSector = {}, SectorCount = {}, ChunkCount = {}",
            self.start_sector(),
            self.sector_count(),
            self.chunk_count()
        );
        for (j, chunk) in self.chunks.iter().enumerate() {
            log::debug!(
                "Chunk#{j} type = {:#x}, StartSector = {}, SectorCount = {}, CompressOffset = {}, CompressLen = {}",
                { chunk.chunk_type.0 },
                { chunk.start_sector },
                { chunk.sector_count },
                { chunk.compressed_offset },
                { chunk.compressed_length }
            );
        }
    }
}

/// Extracts the partition type embedded in a UDIF partition `Name` field of the
/// form `"Partition-Name (Partition-Type : Partition-ID)"`.
///
/// Rust equivalent of `UDIFParser::GetPartitionType`.
pub fn parse_partition_type(name: &str) -> &str {
    let Some((open, separator)) = name.rfind('(').zip(name.rfind(':')) else {
        return "";
    };

    // Name does not end in ')' or no space after ':'.
    if !name.ends_with(')') || name.as_bytes().get(separator + 1) != Some(&b' ') {
        return "";
    }

    if separator <= 1 {
        return "";
    }
    name.get((open + 1)..(separator - 1)).unwrap_or("")
}

/// Minimal representation of an Apple XML property list value needed for UDIF
/// `blkx` parsing.
enum PlistValue {
    Dict(BTreeMap<String, PlistValue>),
    Array(Vec<PlistValue>),
    String(String),
    Data(Vec<u8>),
    Other,
}

/// Pulls the next XML event, skipping comments, processing instructions,
/// doctypes, and pure whitespace. Fails if non-whitespace character data
/// appears outside a text element.
fn next_structural_xml_event(reader: &mut EventReader<&[u8]>) -> Result<XmlEvent, UdifError> {
    loop {
        let event = reader.next().map_err(|_| UdifError::InvalidPlist)?;
        match event {
            XmlEvent::StartDocument { .. }
            | XmlEvent::Doctype { .. }
            | XmlEvent::ProcessingInstruction { .. }
            | XmlEvent::Comment(_)
            | XmlEvent::Whitespace(_) => continue,
            XmlEvent::Characters(ref s) if s.chars().all(char::is_whitespace) => continue,
            XmlEvent::StartElement { .. } | XmlEvent::EndElement { .. } | XmlEvent::EndDocument => {
                return Ok(event);
            }
            XmlEvent::Characters(_) | XmlEvent::CData(_) => {
                return Err(UdifError::InvalidPlist);
            }
        }
    }
}

/// Reads character content inside a leaf XML element until its matching
/// closing tag. Fails if nested elements are encountered.
fn read_xml_text_element(
    reader: &mut EventReader<&[u8]>,
    expected_tag: &str,
) -> Result<String, UdifError> {
    let mut text = String::new();
    loop {
        let event = reader.next().map_err(|_| UdifError::InvalidPlist)?;
        match event {
            XmlEvent::Characters(s) | XmlEvent::Whitespace(s) | XmlEvent::CData(s) => {
                text.push_str(&s);
            }
            XmlEvent::Comment(_) | XmlEvent::ProcessingInstruction { .. } => {}
            XmlEvent::EndElement { name } if name.local_name == expected_tag => {
                return Ok(text);
            }
            _ => return Err(UdifError::InvalidPlist),
        }
    }
}

fn parse_plist_value(
    reader: &mut EventReader<&[u8]>,
    tag_name: &str,
    depth: usize,
) -> Result<PlistValue, UdifError> {
    if depth > MAX_PLIST_DEPTH {
        return Err(UdifError::InvalidPlist);
    }

    match tag_name {
        "dict" => {
            let mut map = BTreeMap::new();
            loop {
                match next_structural_xml_event(reader)? {
                    XmlEvent::EndElement { name } if name.local_name == "dict" => {
                        return Ok(PlistValue::Dict(map));
                    }
                    XmlEvent::StartElement { name, .. } if name.local_name == "key" => {
                        let key = read_xml_text_element(reader, "key")?;
                        let XmlEvent::StartElement { name: val_name, .. } =
                            next_structural_xml_event(reader)?
                        else {
                            return Err(UdifError::InvalidPlist);
                        };
                        let value = parse_plist_value(reader, &val_name.local_name, depth + 1)?;
                        map.insert(key, value);
                    }
                    _ => return Err(UdifError::InvalidPlist),
                }
            }
        }
        "array" => {
            let mut items = Vec::new();
            loop {
                match next_structural_xml_event(reader)? {
                    XmlEvent::EndElement { name } if name.local_name == "array" => {
                        return Ok(PlistValue::Array(items));
                    }
                    XmlEvent::StartElement { name, .. } => {
                        let value = parse_plist_value(reader, &name.local_name, depth + 1)?;
                        items.push(value);
                    }
                    _ => return Err(UdifError::InvalidPlist),
                }
            }
        }
        "string" => {
            let s = read_xml_text_element(reader, "string")?;
            Ok(PlistValue::String(s))
        }
        "data" => {
            let mut encoded = read_xml_text_element(reader, "data")?;
            encoded.retain(|b| !b.is_ascii_whitespace());
            let decoded = BASE64_STANDARD.decode(&encoded).map_err(|_| UdifError::InvalidPlist)?;
            Ok(PlistValue::Data(decoded))
        }
        "integer" | "real" | "date" => {
            let _ = read_xml_text_element(reader, tag_name)?;
            Ok(PlistValue::Other)
        }
        "true" | "false" => {
            let text = read_xml_text_element(reader, tag_name)?;
            if !text.trim().is_empty() {
                return Err(UdifError::InvalidPlist);
            }
            Ok(PlistValue::Other)
        }
        _ => Err(UdifError::InvalidPlist),
    }
}

impl PlistValue {
    fn try_new(bytes: &[u8]) -> Result<Self, UdifError> {
        let config = ParserConfig::new().allow_multiple_root_elements(false);
        let mut reader = EventReader::new_with_config(bytes, config);

        let XmlEvent::StartElement { name: root_name, .. } =
            next_structural_xml_event(&mut reader)?
        else {
            return Err(UdifError::InvalidPlist);
        };
        if root_name.local_name != "plist" {
            return Err(UdifError::InvalidPlist);
        }

        let XmlEvent::StartElement { name: value_name, .. } =
            next_structural_xml_event(&mut reader)?
        else {
            return Err(UdifError::InvalidPlist);
        };
        let root_value = parse_plist_value(&mut reader, &value_name.local_name, 1)?;

        let XmlEvent::EndElement { name: end_name } = next_structural_xml_event(&mut reader)?
        else {
            return Err(UdifError::InvalidPlist);
        };
        if end_name.local_name != "plist" {
            return Err(UdifError::InvalidPlist);
        }

        let XmlEvent::EndDocument = next_structural_xml_event(&mut reader)? else {
            return Err(UdifError::InvalidPlist);
        };

        Ok(root_value)
    }

    fn into_dict(self) -> Option<BTreeMap<String, PlistValue>> {
        match self {
            Self::Dict(map) => Some(map),
            _ => None,
        }
    }

    fn into_array(self) -> Option<Vec<PlistValue>> {
        match self {
            Self::Array(items) => Some(items),
            _ => None,
        }
    }

    fn into_string(self) -> Option<String> {
        match self {
            Self::String(s) => Some(s),
            _ => None,
        }
    }

    fn into_data(self) -> Option<Vec<u8>> {
        match self {
            Self::Data(data) => Some(data),
            _ => None,
        }
    }
}

/// Verifies that `[offset, offset + length)` fits within `0..limit` and that
/// `length` fits in `usize`.
fn checked_slice_len(offset: u64, length: u64, limit: u64) -> Option<usize> {
    let end = offset.checked_add(length)?;
    if end > limit {
        return None;
    }
    usize::try_from(length).ok()
}

/// Parses a UDIF disk image, providing access to partition metadata,
/// raw partition streams, and the image-level code signature.
pub struct UdifParser {
    stream: SharedStream,
    partition_names: Vec<String>,
    blocks: Vec<UdifBlock>,
    block_size: u16,
    signature_blob: Vec<u8>,
}

impl UdifParser {
    /// Parses the UDIF `koly` trailer and `blkx` property list from `stream`.
    ///
    /// Rust equivalent of `UDIFParser::Parse` and `UDIFParser::ParseBlkx`.
    pub fn try_new(stream: SharedStream) -> Result<Self, UdifError> {
        let mut parser = Self {
            stream,
            partition_names: Vec::new(),
            blocks: Vec::new(),
            block_size: UDIF_SECTOR_SIZE,
            signature_blob: Vec::new(),
        };
        parser.parse_blkx()?;
        Ok(parser)
    }

    /// Parses the `koly` trailer, `blkx` XML property list, `mish` block
    /// tables, and optional code signature blob.
    fn parse_blkx(&mut self) -> Result<(), UdifError> {
        let (trailer, trailer_start) = self.read_trailer()?;
        let blkx = self.read_blkx_array(&trailer, trailer_start)?;

        for (i, item) in blkx.into_iter().enumerate() {
            if let Some((name, block)) = self.parse_blkx_entry(i, item)? {
                self.blocks.push(block);
                self.partition_names.push(name);
            }
        }

        self.signature_blob = self.read_code_signature(&trailer, trailer_start)?;
        Ok(())
    }

    /// Reads and validates the 512-byte `koly` trailer at the end of `stream`,
    /// returning the parsed trailer and its starting byte offset.
    fn read_trailer(&self) -> Result<(UdifResourceFile, u64), UdifError> {
        let mut trailer_bytes = [0u8; UDIF_RESOURCE_FILE_SIZE];
        let trailer_start = self
            .stream
            .read_exact_from_end(-(UDIF_RESOURCE_FILE_SIZE as i64), &mut trailer_bytes)
            .map_err(|e| {
                log::debug!("Failed to read UDIFResourceFile: {e}");
                UdifError::IoError
            })?;

        let trailer =
            bytemuck::from_bytes::<UdifResourceFile>(&trailer_bytes).convert_from_big_endian();
        if trailer.signature != UDIF_RESOURCE_FILE_SIGNATURE {
            log::debug!("blkx signature does not match, is {:#x}", { trailer.signature });
            return Err(UdifError::InvalidTrailer);
        }
        if trailer.version != UDIF_RESOURCE_FILE_VERSION {
            log::debug!("blkx version does not match, is {}", { trailer.version });
            return Err(UdifError::InvalidTrailer);
        }

        Ok((trailer, trailer_start))
    }

    /// Reads the XML property list referenced by `trailer` and extracts its
    /// `resource-fork` -> `blkx` array.
    fn read_blkx_array(
        &self,
        trailer: &UdifResourceFile,
        trailer_start: u64,
    ) -> Result<Vec<PlistValue>, UdifError> {
        let Some(plist_length) =
            checked_slice_len(trailer.plist_offset, trailer.plist_length, trailer_start)
        else {
            log::debug!("blkx plist extends past UDIF trailer");
            return Err(UdifError::InvalidTrailer);
        };
        if plist_length == 0 {
            log::debug!("Failed to read blkx plist data");
            return Err(UdifError::InvalidTrailer);
        }

        let mut plist_bytes = vec![0u8; plist_length];
        self.stream.read_exact_at(trailer.plist_offset, &mut plist_bytes).map_err(|e| {
            log::debug!("Failed to read blkx plist data: {e}");
            UdifError::IoError
        })?;

        let plist = PlistValue::try_new(&plist_bytes).inspect_err(|_| {
            log::debug!("Failed to parse XML plist");
        })?;
        let mut plist_dict = plist.into_dict().ok_or_else(|| {
            log::debug!("Plist is not a dictionary");
            UdifError::InvalidPlist
        })?;
        let mut resource_fork = plist_dict
            .remove("resource-fork")
            .and_then(PlistValue::into_dict)
            .ok_or_else(|| {
                log::debug!("No resource-fork entry in plist");
                UdifError::InvalidPlist
            })?;
        resource_fork.remove("blkx").and_then(PlistValue::into_array).ok_or_else(|| {
            log::debug!("No blkx entry in resource-fork");
            UdifError::InvalidPlist
        })
    }

    /// Parses a single dictionary entry from the `blkx` array. Returns
    /// `Ok(None)` for entries that should be skipped, matching
    /// `UDIFParser::ParseBlkx`.
    fn parse_blkx_entry(
        &self,
        index: usize,
        item: PlistValue,
    ) -> Result<Option<(String, UdifBlock)>, UdifError> {
        let Some(mut block_dict) = item.into_dict() else {
            log::debug!("Skipping block {index} because it is not a dictionary");
            return Ok(None);
        };
        let Some(data) = block_dict.remove("Data").and_then(PlistValue::into_data) else {
            log::debug!("Skipping block {index} because it has no Data section");
            return Ok(None);
        };

        let block = UdifBlock::try_new(&data, self.block_size).inspect_err(|_| {
            log::debug!("Failed to parse UDIF block data");
        })?;
        if block.signature() != UDIF_BLOCK_DATA_SIGNATURE {
            log::debug!(
                "Skipping block {index} because its signature does not match, is {:#x}",
                block.signature()
            );
            return Ok(None);
        }
        if block.version() != UDIF_BLOCK_DATA_VERSION {
            log::debug!(
                "Skipping block {index} because its version does not match, is {}",
                block.version()
            );
            return Ok(None);
        }

        let Some(partition_name) = block_dict.remove("Name").and_then(PlistValue::into_string)
        else {
            log::debug!("Skipping block {index} because it has no name");
            return Ok(None);
        };

        block.log_debug(&partition_name);
        Ok(Some((partition_name, block)))
    }

    /// Reads the raw code signature blob if the trailer specifies a valid,
    /// non-empty range preceding `trailer_start`.
    fn read_code_signature(
        &self,
        trailer: &UdifResourceFile,
        trailer_start: u64,
    ) -> Result<Vec<u8>, UdifError> {
        // The offsets in the trailer could be garbage in DMGs that aren't
        // signed. Need a sanity check that the DMG has legit values for
        // these fields.
        let Some(sig_length) = checked_slice_len(
            trailer.code_signature_offset,
            trailer.code_signature_length,
            trailer_start,
        )
        .filter(|&len| len > 0) else {
            return Ok(Vec::new());
        };

        let mut signature_blob = vec![0u8; sig_length];
        self.stream.read_exact_at(trailer.code_signature_offset, &mut signature_blob).map_err(
            |e| {
                log::debug!("Failed to read raw signature bytes: {e}");
                UdifError::IoError
            },
        )?;
        Ok(signature_blob)
    }

    /// Returns the raw DMG code signature bytes (empty if unsigned).
    pub fn code_signature(&self) -> &[u8] {
        &self.signature_blob
    }

    /// Returns the number of partitions in this UDIF image.
    pub fn number_of_partitions(&self) -> usize {
        self.blocks.len()
    }

    /// Returns the partition name for `part_number`, e.g.
    /// `"disk image (Apple_HFS : 2)"`.
    pub fn partition_name(&self, part_number: usize) -> &str {
        self.partition_names.get(part_number).map_or("", String::as_str)
    }

    /// Extracts the partition type embedded in the partition name, e.g.
    /// `"Apple_HFS"` or `"Apple_Free"`.
    pub fn partition_type(&self, part_number: usize) -> &str {
        parse_partition_type(self.partition_name(part_number))
    }

    /// Returns the logical uncompressed size of `part_number` in bytes.
    pub fn partition_size(&self, part_number: usize) -> usize {
        self.blocks
            .get(part_number)
            .and_then(|block| {
                usize::try_from(block.sector_count())
                    .ok()
                    .and_then(|sc| sc.checked_mul(usize::from(self.block_size)))
            })
            .unwrap_or(0)
    }

    /// Returns a virtualized stream over the decompressed partition data, or
    /// `None` if `part_number` is out of range.
    pub fn partition_read_stream(&self, part_number: usize) -> Option<Box<dyn ReadSeek>> {
        // TODO(https://crbug.com/575525008): Return `UdifPartitionReadStream` wrapping `self.stream`.
        let _ = part_number;
        None
    }
}
