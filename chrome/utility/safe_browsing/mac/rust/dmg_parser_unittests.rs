// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//chrome/utility/safe_browsing/mac:dmg_parser";
}

use std::io::Cursor;
use std::mem::{size_of, size_of_val};

use base64::engine::general_purpose::STANDARD as BASE64_STANDARD;
use base64::Engine as _;
use bytemuck::Zeroable;
use dmg_parser::hfs::{HfsError, HfsIterator};
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
