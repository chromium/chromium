// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//chrome/utility/safe_browsing/mac:dmg_parser";
}

use std::io::{self, Cursor, Read, Seek, SeekFrom};
use std::mem::{size_of, size_of_val};

use base64::engine::general_purpose::STANDARD as BASE64_STANDARD;
use base64::Engine as _;
use bytemuck::Zeroable;
use dmg_parser::hfs::{
    HfsError, HfsForkReadStream, HfsIterator, HfsPlusExtentDescriptor, HfsPlusForkData,
    MAX_EXTENT_BUFFER_SIZE,
};
use dmg_parser::udif::{
    parse_partition_type, UdifBlock, UdifBlockChunk, UdifBlockChunkType, UdifBlockData, UdifError,
    UdifParser, UdifResourceFile, UDIF_BLOCK_DATA_SIGNATURE, UDIF_BLOCK_DATA_VERSION,
    UDIF_RESOURCE_FILE_SIGNATURE, UDIF_RESOURCE_FILE_VERSION, UDIF_SECTOR_SIZE,
};
use dmg_parser::SharedStream;
use rust_gtest_interop::prelude::*;

fn build_mish_block(
    signature: u32,
    version: u32,
    start_sector: u64,
    sector_count: u64,
    reported_chunk_count: u32,
    chunks: &[UdifBlockChunk],
) -> Vec<u8> {
    let mut header = UdifBlockData::zeroed();
    header.signature = signature.to_be();
    header.version = version.to_be();
    header.start_sector = start_sector.to_be();
    header.sector_count = sector_count.to_be();
    header.chunk_count = reported_chunk_count.to_be();

    let mut bytes = Vec::with_capacity(size_of::<UdifBlockData>() + size_of_val(chunks));
    bytes.extend_from_slice(bytemuck::bytes_of(&header));
    for chunk in chunks {
        let raw_chunk = UdifBlockChunk {
            chunk_type: bytemuck::cast(bytemuck::cast::<_, u32>(chunk.chunk_type).to_be()),
            comment: chunk.comment.to_be(),
            start_sector: chunk.start_sector.to_be(),
            sector_count: chunk.sector_count.to_be(),
            compressed_offset: chunk.compressed_offset.to_be(),
            compressed_length: chunk.compressed_length.to_be(),
        };
        bytes.extend_from_slice(bytemuck::bytes_of(&raw_chunk));
    }
    bytes
}

fn build_koly_trailer(
    signature: u32,
    version: u32,
    plist_offset: u64,
    plist_length: u64,
    code_signature_offset: u64,
    code_signature_length: u64,
) -> [u8; size_of::<UdifResourceFile>()] {
    let mut trailer = UdifResourceFile::zeroed();
    trailer.signature = signature.to_be();
    trailer.version = version.to_be();
    trailer.plist_offset = plist_offset.to_be();
    trailer.plist_length = plist_length.to_be();
    trailer.code_signature_offset = code_signature_offset.to_be();
    trailer.code_signature_length = code_signature_length.to_be();
    bytemuck::cast(trailer)
}

#[gtest(RustHfsIteratorTest, EmptyStreamFailsToOpen)]
fn test_empty_hfs_stream_fails_to_open() {
    let stream = SharedStream::new(Cursor::new(Vec::<u8>::new()));
    expect_eq!(HfsIterator::open(stream).err(), Some(HfsError::Unimplemented));
}

#[gtest(RustSharedStreamTest, InterleavedPositionalReads)]
fn test_shared_stream_interleaved_positional_reads() {
    let stream_a = SharedStream::new(Cursor::new(b"0123456789abcdef".to_vec()));
    let stream_b = stream_a.clone();

    let mut buf_a = [0u8; 4];
    let mut buf_b = [0u8; 4];

    expect_eq!(stream_a.read_at(2, &mut buf_a).ok(), Some(4));
    expect_eq!(&buf_a, b"2345");

    expect_eq!(stream_b.read_exact_at(10, &mut buf_b).ok(), Some(()));
    expect_eq!(&buf_b, b"abcd");

    expect_eq!(stream_a.read_exact_from_end(-4, &mut buf_a).ok(), Some(12));
    expect_eq!(&buf_a, b"cdef");
}

#[gtest(RustUdifParserTest, EmptyStreamFailsToParse)]
fn test_empty_udif_stream_fails_to_parse() {
    let stream = SharedStream::new(Cursor::new(Vec::<u8>::new()));
    expect_eq!(UdifParser::try_new(stream).err(), Some(UdifError::IoError));
}

#[gtest(RustUdifParserTest, ParsePartitionType)]
fn test_parse_partition_type() {
    let cases = [
        ("disk image (Apple_HFS : 2)", "Apple_HFS"),
        ("MBR (MBR : 0)", "MBR"),
        ("GPT Header (Primary GPT Header : 1)", "Primary GPT Header"),
        ("(Apple_Free : 3)", "Apple_Free"),
        ("Name (with parens) (Apple_HFSX : 4)", "Apple_HFSX"),
        // Malformed partition names return an empty string.
        ("", ""),
        ("Apple_HFS", ""),
        ("disk image (Apple_HFS)", ""),
        ("disk image Apple_HFS : 2)", ""),
        ("disk image (Apple_HFS : 2", ""),
        ("disk image (Apple_HFS : 2) ", ""),
        ("disk image (Apple_HFS :2)", ""),
        ("disk image ( : 2)", ""),
        ("disk image (: 2)", ""),
        ("disk image ()", ""),
        (": ( )", ""),
    ];
    for (input, expected) in cases {
        expect_eq!(parse_partition_type(input), expected, "input: {input:?}");
    }
}

#[gtest(RustUdifBlockTest, ParseValidBlockData)]
fn test_parse_valid_block_data() {
    let chunks = [
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::ZERO_FILL,
            comment: 0,
            start_sector: 0,
            sector_count: 4,
            compressed_offset: 0,
            compressed_length: 0,
        },
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::COMPRESS_ZLIB,
            comment: 0,
            start_sector: 4,
            sector_count: 6,
            compressed_offset: 512,
            compressed_length: 1024,
        },
    ];
    let bytes = build_mish_block(
        UDIF_BLOCK_DATA_SIGNATURE,
        UDIF_BLOCK_DATA_VERSION,
        2,
        10,
        chunks.len() as u32,
        &chunks,
    );

    let block = UdifBlock::try_new(&bytes, UDIF_SECTOR_SIZE).expect("valid mish block");
    expect_eq!(block.signature(), UDIF_BLOCK_DATA_SIGNATURE);
    expect_eq!(block.version(), UDIF_BLOCK_DATA_VERSION);
    expect_eq!(block.start_sector(), 2);
    expect_eq!(block.sector_count(), 10);
    expect_eq!(block.chunk_count(), 2);
    expect_eq!(block.chunk(0), Some(&chunks[0]));
    expect_eq!(block.chunk(1), Some(&chunks[1]));
    expect_eq!(block.chunk(2), None);
}

#[gtest(RustUdifBlockTest, ParseInvalidBlockDataFails)]
fn test_parse_invalid_block_data_fails() {
    // Truncated header (< 204 bytes).
    expect_eq!(
        UdifBlock::try_new(&[0u8; size_of::<UdifBlockData>() - 1], UDIF_SECTOR_SIZE).err(),
        Some(UdifError::InvalidBlockData)
    );

    // Header claims 2 chunks, but buffer only contains 1 chunk.
    let single_chunk = [UdifBlockChunk {
        chunk_type: UdifBlockChunkType::ZERO_FILL,
        comment: 0,
        start_sector: 0,
        sector_count: 1,
        compressed_offset: 0,
        compressed_length: 0,
    }];
    let truncated_chunks = build_mish_block(
        UDIF_BLOCK_DATA_SIGNATURE,
        UDIF_BLOCK_DATA_VERSION,
        0,
        10,
        2,
        &single_chunk,
    );
    expect_eq!(
        UdifBlock::try_new(&truncated_chunks, UDIF_SECTOR_SIZE).err(),
        Some(UdifError::InvalidBlockData)
    );

    // `sector_count * sector_size` overflow.
    let overflow_sectors =
        build_mish_block(UDIF_BLOCK_DATA_SIGNATURE, UDIF_BLOCK_DATA_VERSION, 0, u64::MAX, 0, &[]);
    expect_eq!(
        UdifBlock::try_new(&overflow_sectors, UDIF_SECTOR_SIZE).err(),
        Some(UdifError::InvalidBlockData)
    );

    // Sum of chunk `sector_count`s exceeds block `sector_count`.
    let too_many_chunk_sectors = build_mish_block(
        UDIF_BLOCK_DATA_SIGNATURE,
        UDIF_BLOCK_DATA_VERSION,
        0,
        5,
        2,
        &[
            UdifBlockChunk {
                chunk_type: UdifBlockChunkType::ZERO_FILL,
                comment: 0,
                start_sector: 0,
                sector_count: 3,
                compressed_offset: 0,
                compressed_length: 0,
            },
            UdifBlockChunk {
                chunk_type: UdifBlockChunkType::ZERO_FILL,
                comment: 0,
                start_sector: 3,
                sector_count: 3,
                compressed_offset: 0,
                compressed_length: 0,
            },
        ],
    );
    expect_eq!(
        UdifBlock::try_new(&too_many_chunk_sectors, UDIF_SECTOR_SIZE).err(),
        Some(UdifError::InvalidBlockData)
    );

    // `compressed_offset + compressed_length` overflow.
    let overflow_compressed_end = build_mish_block(
        UDIF_BLOCK_DATA_SIGNATURE,
        UDIF_BLOCK_DATA_VERSION,
        0,
        4,
        1,
        &[UdifBlockChunk {
            chunk_type: UdifBlockChunkType::UNCOMPRESSED,
            comment: 0,
            start_sector: 0,
            sector_count: 4,
            compressed_offset: u64::MAX - 10,
            compressed_length: 20,
        }],
    );
    expect_eq!(
        UdifBlock::try_new(&overflow_compressed_end, UDIF_SECTOR_SIZE).err(),
        Some(UdifError::InvalidBlockData)
    );

    // `compressed_length > block_size`.
    let compressed_too_large = build_mish_block(
        UDIF_BLOCK_DATA_SIGNATURE,
        UDIF_BLOCK_DATA_VERSION,
        0,
        2,
        1,
        &[UdifBlockChunk {
            chunk_type: UdifBlockChunkType::UNCOMPRESSED,
            comment: 0,
            start_sector: 0,
            sector_count: 2,
            compressed_offset: 0,
            compressed_length: u64::from(UDIF_SECTOR_SIZE) * 2 + 1,
        }],
    );
    expect_eq!(
        UdifBlock::try_new(&compressed_too_large, UDIF_SECTOR_SIZE).err(),
        Some(UdifError::InvalidBlockData)
    );
}

#[gtest(RustUdifParserTest, ParseValidUdifImageAndSkipsNonMatchingBlkxEntries)]
fn test_parse_valid_udif_image() {
    let block0_chunks = [UdifBlockChunk {
        chunk_type: UdifBlockChunkType::ZERO_FILL,
        comment: 0,
        start_sector: 0,
        sector_count: 1,
        compressed_offset: 0,
        compressed_length: 0,
    }];
    let block0_b64 = BASE64_STANDARD.encode(build_mish_block(
        UDIF_BLOCK_DATA_SIGNATURE,
        UDIF_BLOCK_DATA_VERSION,
        0,
        1,
        1,
        &block0_chunks,
    ));

    let block1_chunks = [UdifBlockChunk {
        chunk_type: UdifBlockChunkType::UNCOMPRESSED,
        comment: 0,
        start_sector: 0,
        sector_count: 8,
        compressed_offset: 0,
        compressed_length: 4096,
    }];
    let block1_b64 = BASE64_STANDARD.encode(build_mish_block(
        UDIF_BLOCK_DATA_SIGNATURE,
        UDIF_BLOCK_DATA_VERSION,
        1,
        8,
        1,
        &block1_chunks,
    ));

    // A block with an unrecognized `mish` version should be skipped rather than
    // failing the entire parse (matching C++ `UDIFParser::ParseBlkx`).
    let bad_version_b64 =
        BASE64_STANDARD.encode(build_mish_block(UDIF_BLOCK_DATA_SIGNATURE, 99, 0, 1, 0, &[]));

    let plist_xml = format!(
        r#"<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>resource-fork</key>
    <dict>
        <key>blkx</key>
        <array>
            <dict>
                <key>Attributes</key>
                <string>0x0050</string>
                <key>Data</key>
                <data>{block0_b64}</data>
                <key>ID</key>
                <string>-1</string>
                <key>Name</key>
                <string>Master Boot Record (MBR : 0)</string> <!-- nocheck -->
            </dict>
            <dict>
                <key>Data</key>
                <data>{bad_version_b64}</data>
                <key>Name</key>
                <string>Skipped Version (Ignored : 1)</string>
            </dict>
            <dict>
                <key>Data</key>
                <data>{block1_b64}</data>
                <key>Name</key>
                <string>disk image (Apple_HFS : 2)</string>
            </dict>
        </array>
    </dict>
</dict>
</plist>"#
    );

    let mut image = vec![0xAAu8; 4096];
    let sig_offset = image.len() as u64;
    let sig_bytes = b"mock-code-signature";
    image.extend_from_slice(sig_bytes);

    let plist_offset = image.len() as u64;
    let plist_length = plist_xml.len() as u64;
    image.extend_from_slice(plist_xml.as_bytes());

    let trailer = build_koly_trailer(
        UDIF_RESOURCE_FILE_SIGNATURE,
        UDIF_RESOURCE_FILE_VERSION,
        plist_offset,
        plist_length,
        sig_offset,
        sig_bytes.len() as u64,
    );
    image.extend_from_slice(&trailer);

    let stream = SharedStream::new(Cursor::new(image));
    let parser = UdifParser::try_new(stream).expect("valid UDIF image should parse");

    expect_eq!(parser.number_of_partitions(), 2);
    expect_eq!(parser.partition_name(0), "Master Boot Record (MBR : 0)"); // nocheck
    expect_eq!(parser.partition_type(0), "MBR");
    expect_eq!(parser.partition_size(0), usize::from(UDIF_SECTOR_SIZE));

    expect_eq!(parser.partition_name(1), "disk image (Apple_HFS : 2)");
    expect_eq!(parser.partition_type(1), "Apple_HFS");
    expect_eq!(parser.partition_size(1), 8 * usize::from(UDIF_SECTOR_SIZE));

    expect_eq!(parser.partition_name(2), "");
    expect_eq!(parser.partition_type(2), "");
    expect_eq!(parser.partition_size(2), 0);

    expect_eq!(parser.code_signature(), sig_bytes);
}

#[gtest(RustUdifParserTest, ParseRejectsInvalidTrailersAndPlists)]
fn test_parse_rejects_invalid_trailers_and_plists() {
    // Wrong trailer magic.
    let bad_magic = build_koly_trailer(u32::from_be_bytes(*b"nope"), 4, 0, 10, 0, 0);
    expect_eq!(
        UdifParser::try_new(SharedStream::new(Cursor::new(bad_magic.to_vec()))).err(),
        Some(UdifError::InvalidTrailer)
    );

    // Wrong trailer version.
    let bad_version = build_koly_trailer(UDIF_RESOURCE_FILE_SIGNATURE, 3, 0, 10, 0, 0);
    expect_eq!(
        UdifParser::try_new(SharedStream::new(Cursor::new(bad_version.to_vec()))).err(),
        Some(UdifError::InvalidTrailer)
    );

    // Zero plist length.
    let zero_plist =
        build_koly_trailer(UDIF_RESOURCE_FILE_SIGNATURE, UDIF_RESOURCE_FILE_VERSION, 0, 0, 0, 0);
    expect_eq!(
        UdifParser::try_new(SharedStream::new(Cursor::new(zero_plist.to_vec()))).err(),
        Some(UdifError::InvalidTrailer)
    );

    // Plist range extends past trailer start.
    let mut oob_plist_image = vec![0u8; 16];
    oob_plist_image.extend_from_slice(&build_koly_trailer(
        UDIF_RESOURCE_FILE_SIGNATURE,
        UDIF_RESOURCE_FILE_VERSION,
        8,
        16,
        0,
        0,
    ));
    expect_eq!(
        UdifParser::try_new(SharedStream::new(Cursor::new(oob_plist_image))).err(),
        Some(UdifError::InvalidTrailer)
    );

    // Malformed XML plist.
    let bad_xml = b"<plist><dict><unclosed></plist>";
    let mut bad_xml_image = bad_xml.to_vec();
    bad_xml_image.extend_from_slice(&build_koly_trailer(
        UDIF_RESOURCE_FILE_SIGNATURE,
        UDIF_RESOURCE_FILE_VERSION,
        0,
        bad_xml.len() as u64,
        0,
        0,
    ));
    expect_eq!(
        UdifParser::try_new(SharedStream::new(Cursor::new(bad_xml_image))).err(),
        Some(UdifError::InvalidPlist)
    );

    // Valid XML plist missing `resource-fork` -> `blkx`.
    let missing_blkx_xml = b"<plist version=\"1.0\"><dict></dict></plist>";
    let mut missing_blkx_image = missing_blkx_xml.to_vec();
    missing_blkx_image.extend_from_slice(&build_koly_trailer(
        UDIF_RESOURCE_FILE_SIGNATURE,
        UDIF_RESOURCE_FILE_VERSION,
        0,
        missing_blkx_xml.len() as u64,
        0,
        0,
    ));
    expect_eq!(
        UdifParser::try_new(SharedStream::new(Cursor::new(missing_blkx_image))).err(),
        Some(UdifError::InvalidPlist)
    );
}

/// Test reader whose `seek` clamps offsets past EOF to `data.len()`, mirroring
/// C++ `MemoryReadStream::Seek`.
struct ClampingMemoryStream {
    data: Vec<u8>,
    pos: usize,
}

impl Read for ClampingMemoryStream {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let remaining = &self.data[self.pos..];
        let to_copy = remaining.len().min(buf.len());
        buf[..to_copy].copy_from_slice(&remaining[..to_copy]);
        self.pos += to_copy;
        Ok(to_copy)
    }
}

impl Seek for ClampingMemoryStream {
    fn seek(&mut self, pos: SeekFrom) -> io::Result<u64> {
        let SeekFrom::Start(offset) = pos else {
            return Err(io::Error::new(io::ErrorKind::Unsupported, "only SeekFrom::Start"));
        };
        let clamped = usize::try_from(offset).unwrap_or(self.data.len()).min(self.data.len());
        self.pos = clamped;
        Ok(clamped as u64)
    }
}

#[gtest(RustSharedStreamTest, ClampedSeekPastEofReturnsUnexpectedEof)]
fn test_shared_stream_clamped_seek_past_eof() {
    let stream = SharedStream::new(ClampingMemoryStream { data: b"01234567".to_vec(), pos: 0 });
    let mut buf = [0u8; 4];

    // Reading within bounds succeeds.
    expect_eq!(stream.read_at(2, &mut buf).ok(), Some(4));
    expect_eq!(&buf, b"2345");
    expect_eq!(stream.read_exact_at(4, &mut buf).ok(), Some(()));
    expect_eq!(&buf, b"4567");

    // Seeking past EOF clamps to `data.len()` (8 != 16), so both `read_at` and
    // `read_exact_at` reject the clamped seek with `UnexpectedEof`.
    expect_eq!(
        stream.read_at(16, &mut buf).expect_err("clamped seek should fail").kind(),
        io::ErrorKind::UnexpectedEof
    );
    expect_eq!(
        stream.read_exact_at(16, &mut buf).expect_err("clamped seek should fail").kind(),
        io::ErrorKind::UnexpectedEof
    );
}

#[gtest(RustHfsForkReadStreamTest, ParseForkDataAndExtentDescriptorsFromBigEndian)]
fn test_parse_fork_data_from_big_endian() {
    let mut raw = [0u8; HfsPlusForkData::SIZE];
    raw[0..8].copy_from_slice(&0x0000_0000_0001_2345u64.to_be_bytes());
    raw[8..12].copy_from_slice(&0x0000_4000u32.to_be_bytes());
    raw[12..16].copy_from_slice(&36u32.to_be_bytes());
    // Extent 0: start_block = 10, block_count = 32.
    raw[16..20].copy_from_slice(&10u32.to_be_bytes());
    raw[20..24].copy_from_slice(&32u32.to_be_bytes());
    // Extent 7: start_block = 99, block_count = 4.
    raw[72..76].copy_from_slice(&99u32.to_be_bytes());
    raw[76..80].copy_from_slice(&4u32.to_be_bytes());

    let fork = HfsPlusForkData::from_be_bytes(&raw);
    expect_eq!(fork.logical_size, 0x1_2345);
    expect_eq!(fork.clump_size, 0x4000);
    expect_eq!(fork.total_blocks, 36);
    expect_eq!(fork.extents[0], HfsPlusExtentDescriptor { start_block: 10, block_count: 32 });
    expect_false!(fork.extents[0].is_empty());
    expect_eq!(fork.extents[1], HfsPlusExtentDescriptor { start_block: 0, block_count: 0 });
    expect_true!(fork.extents[1].is_empty());
    expect_eq!(fork.extents[7], HfsPlusExtentDescriptor { start_block: 99, block_count: 4 });
}

#[gtest(RustHfsForkReadStreamTest, ReadAndSeekAcrossAllEightExtents)]
fn test_read_and_seek_across_all_eight_extents() {
    const BLOCK_SIZE: u32 = 64;
    const TOTAL_DISK_BLOCKS: usize = 40;

    // 8 non-contiguous extents with varying block counts (total = 13 blocks =
    // 832 bytes), with logical_size = 800 bytes so the 8th extent is
    // partially filled.
    let extents = [
        HfsPlusExtentDescriptor { start_block: 2, block_count: 1 },
        HfsPlusExtentDescriptor { start_block: 7, block_count: 2 },
        HfsPlusExtentDescriptor { start_block: 4, block_count: 1 },
        HfsPlusExtentDescriptor { start_block: 15, block_count: 3 },
        HfsPlusExtentDescriptor { start_block: 11, block_count: 1 },
        HfsPlusExtentDescriptor { start_block: 25, block_count: 2 },
        HfsPlusExtentDescriptor { start_block: 20, block_count: 1 },
        HfsPlusExtentDescriptor { start_block: 30, block_count: 2 },
    ];
    let logical_size: usize = 800;

    let mut fake_disk = vec![0xFFu8; TOTAL_DISK_BLOCKS * (BLOCK_SIZE as usize)];
    let mut expected_fork = Vec::new();

    for (extent_idx, ext) in extents.iter().enumerate() {
        let disk_start = (ext.start_block as usize) * (BLOCK_SIZE as usize);
        let extent_len = (ext.block_count as usize) * (BLOCK_SIZE as usize);
        for b in 0..extent_len {
            let byte_val = ((extent_idx * 31 + b) & 0xFF) as u8;
            fake_disk[disk_start + b] = byte_val;
            if expected_fork.len() < logical_size {
                expected_fork.push(byte_val);
            }
        }
    }
    expect_eq!(expected_fork.len(), logical_size);

    let fork = HfsPlusForkData {
        logical_size: logical_size as u64,
        clump_size: 0,
        total_blocks: 13,
        extents,
    };

    let disk_stream = SharedStream::new(Cursor::new(fake_disk));
    let mut fork_stream = HfsForkReadStream::new(disk_stream, BLOCK_SIZE, fork);

    // 1. Read the entire 8-extent fork in 37-byte chunks that straddle extent
    //    boundaries.
    let mut read_chunked = Vec::new();
    let mut chunk = [0u8; 37];
    loop {
        let n = fork_stream.read(&mut chunk).expect("chunked read should succeed");
        if n == 0 {
            break;
        }
        read_chunked.extend_from_slice(&chunk[..n]);
    }
    expect_eq!(read_chunked, expected_fork);

    // Subsequent read at EOF returns 0.
    expect_eq!(fork_stream.read(&mut chunk).ok(), Some(0));

    // 2. Seek back to 0 and read the entire fork in a single call.
    expect_eq!(fork_stream.seek(SeekFrom::Start(0)).ok(), Some(0));
    let mut read_all = vec![0u8; logical_size + 32];
    let n = fork_stream.read(&mut read_all).expect("full read should succeed");
    expect_eq!(n, logical_size);
    expect_eq!(&read_all[..n], expected_fork.as_slice());

    // 3. Seek into the middle of extent 3 (logical offset 300) and read across
    //    into extent 5.
    expect_eq!(fork_stream.seek(SeekFrom::Start(300)).ok(), Some(300));
    let mut mid_buf = vec![0u8; 250];
    expect_eq!(fork_stream.read(&mut mid_buf).ok(), Some(250));
    expect_eq!(&mid_buf[..], &expected_fork[300..550]);

    // 4. Seek within the same cached extent (extent 5 spans logical bytes
    //    512..640)
    // and verify intra-extent backward/forward seeks return the right slice.
    expect_eq!(fork_stream.seek(SeekFrom::Start(520)).ok(), Some(520));
    let mut intra_buf = [0u8; 16];
    expect_eq!(fork_stream.read(&mut intra_buf).ok(), Some(16));
    expect_eq!(&intra_buf[..], &expected_fork[520..536]);

    // 5. Seek near the end of extent 7 (logical offset 790) and verify reading
    //    stops at
    // logical_size (800).
    expect_eq!(fork_stream.seek(SeekFrom::Start(790)).ok(), Some(790));
    let mut tail_buf = [0u8; 64];
    expect_eq!(fork_stream.read(&mut tail_buf).ok(), Some(10));
    expect_eq!(&tail_buf[..10], &expected_fork[790..800]);
}

#[gtest(RustHfsForkReadStreamTest, ZeroLogicalSizeAndTrailingEmptyExtents)]
fn test_zero_logical_size_and_trailing_empty_extents() {
    let disk = SharedStream::new(Cursor::new(vec![0xAAu8; 2048]));

    // Empty fork (logical_size == 0).
    let empty_fork = HfsPlusForkData::default();
    let mut empty_stream = HfsForkReadStream::new(disk.clone(), 512, empty_fork);
    let mut buf = [0u8; 16];
    expect_eq!(empty_stream.read(&mut buf).ok(), Some(0));
    // Seeking to 0 on an empty fork with 0 extents fails (matching C++).
    expect_true!(empty_stream.seek(SeekFrom::Start(0)).is_err());

    // Fork with 1 populated extent followed by empty extents, where
    // logical_size > extent bytes.
    let mut extents = [HfsPlusExtentDescriptor::default(); 8];
    extents[0] = HfsPlusExtentDescriptor { start_block: 1, block_count: 1 };
    let fork = HfsPlusForkData { logical_size: 1024, clump_size: 0, total_blocks: 1, extents };
    let mut stream = HfsForkReadStream::new(disk, 512, fork);
    let mut out = vec![0u8; 1024];
    expect_eq!(stream.read(&mut out).ok(), Some(512));
    expect_eq!(stream.read(&mut out).ok(), Some(0));
}

#[gtest(RustHfsForkReadStreamTest, ExtentExceedingMaxBufferSizeRejected)]
fn test_extent_exceeding_max_buffer_size_rejected() {
    const BLOCK_SIZE: u32 = 512;
    let huge_blocks = (MAX_EXTENT_BUFFER_SIZE / (BLOCK_SIZE as usize)) as u32 + 1;

    let mut extents = [HfsPlusExtentDescriptor::default(); 8];
    extents[0] = HfsPlusExtentDescriptor { start_block: 1, block_count: huge_blocks };
    let fork = HfsPlusForkData {
        logical_size: u64::from(huge_blocks) * u64::from(BLOCK_SIZE),
        clump_size: 0,
        total_blocks: huge_blocks,
        extents,
    };

    // Backing disk only needs a few bytes; the 64 MiB cap rejects the extent
    // before reading.
    let disk = SharedStream::new(Cursor::new(vec![0u8; 2048]));
    let mut stream = HfsForkReadStream::new(disk, BLOCK_SIZE, fork);
    let mut buf = [0u8; 16];
    expect_true!(stream.read(&mut buf).is_err());
}

#[gtest(RustHfsForkReadStreamTest, InvalidSeekAndOverflowRejected)]
fn test_invalid_seek_and_overflow_rejected() {
    let mut extents = [HfsPlusExtentDescriptor::default(); 8];
    extents[0] = HfsPlusExtentDescriptor { start_block: 1, block_count: 2 };
    let fork = HfsPlusForkData { logical_size: 1000, clump_size: 0, total_blocks: 2, extents };

    let disk = SharedStream::new(Cursor::new(vec![0u8; 4096]));
    let mut stream = HfsForkReadStream::new(disk.clone(), 512, fork);

    // SeekFrom::Current (including stream_position) and SeekFrom::End are
    // unsupported.
    expect_true!(stream.seek(SeekFrom::Current(1)).is_err());
    expect_true!(stream.stream_position().is_err());
    expect_true!(stream.seek(SeekFrom::End(0)).is_err());

    // Seeking to >= logical_size is rejected.
    expect_true!(stream.seek(SeekFrom::Start(1000)).is_err());
    expect_true!(stream.seek(SeekFrom::Start(5000)).is_err());

    // Invalid block_size == 0 is rejected on both read and seek.
    let mut zero_bs_stream = HfsForkReadStream::new(disk.clone(), 0, fork);
    let mut buf = [0u8; 8];
    expect_true!(zero_bs_stream.seek(SeekFrom::Start(0)).is_err());
    expect_true!(zero_bs_stream.read(&mut buf).is_err());

    // Extent with start_block != 0 and block_count == 0 is rejected on read.
    let mut zero_bc_extents = [HfsPlusExtentDescriptor::default(); 8];
    zero_bc_extents[0] = HfsPlusExtentDescriptor { start_block: 5, block_count: 0 };
    let zero_bc_fork = HfsPlusForkData {
        logical_size: 512,
        clump_size: 0,
        total_blocks: 0,
        extents: zero_bc_extents,
    };
    let mut zero_bc_stream = HfsForkReadStream::new(disk.clone(), 512, zero_bc_fork);
    expect_true!(zero_bc_stream.read(&mut buf).is_err());

    // Extent with start_block * block_size overflowing u64 is rejected on read.
    let mut overflow_start_extents = [HfsPlusExtentDescriptor::default(); 8];
    overflow_start_extents[0] = HfsPlusExtentDescriptor { start_block: u32::MAX, block_count: 1 };
    let overflow_start_fork = HfsPlusForkData {
        logical_size: u64::MAX,
        clump_size: 0,
        total_blocks: 1,
        extents: overflow_start_extents,
    };
    let mut overflow_start_stream = HfsForkReadStream::new(disk, u32::MAX, overflow_start_fork);
    expect_true!(overflow_start_stream.read(&mut buf).is_err());
}
