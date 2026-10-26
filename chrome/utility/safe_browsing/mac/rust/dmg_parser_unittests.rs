// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//chrome/utility/safe_browsing/mac:dmg_decompress";
    "//chrome/utility/safe_browsing/mac:dmg_decompress_test_support";
    "//chrome/utility/safe_browsing/mac:dmg_parser";
}

use std::io::{self, Cursor, ErrorKind, Read, Seek, SeekFrom};
use std::mem::{size_of, size_of_val};

use base64::engine::general_purpose::STANDARD as BASE64_STANDARD;
use base64::Engine as _;
use bytemuck::Zeroable;
use dmg_decompress::decompress_bz2;
use dmg_decompress_test_support::compress_bz2;
use dmg_parser::hfs::{
    FndrFileInfo, HfsBTreeEntry, HfsBTreeIterator, HfsCatalogRecord, HfsError, HfsForkReadStream,
    HfsIterator, HfsPlusBsdInfo, HfsPlusCatalogFile, HfsPlusCatalogFolder, HfsPlusExtentDescriptor,
    HfsPlusForkData, HfsPlusVolumeHeader, HARD_LINK_FILE_TYPE, HFSX_SIG_WORD, HFS_PLUS_CREATOR,
    HFS_PLUS_SIG_WORD, MAX_EXTENT_BUFFER_SIZE, S_IFLNK, UF_COMPRESSED, VOLUME_HEADER_OFFSET,
    VOLUME_HEADER_SIZE,
};
use dmg_parser::mach_o::{
    MachOError, MachOImage, FAT_ARCH_SIZE, FAT_CIGAM, FAT_HEADER_SIZE, FAT_MAGIC,
    LC_CODE_SIGNATURE, MACH_HEADER_64_SIZE, MACH_HEADER_SIZE, MH_CIGAM, MH_CIGAM_64, MH_MAGIC,
    MH_MAGIC_64,
};
use dmg_parser::udif::{
    parse_partition_type, UdifBlock, UdifBlockChunk, UdifBlockChunkReadStream, UdifBlockChunkType,
    UdifBlockData, UdifError, UdifParser, UdifPartitionReadStream, UdifResourceFile,
    MAX_CHUNK_COMPRESSED_SIZE, MAX_CHUNK_DECOMPRESSED_SIZE, UDIF_BLOCK_DATA_SIGNATURE,
    UDIF_BLOCK_DATA_VERSION, UDIF_RESOURCE_FILE_SIGNATURE, UDIF_RESOURCE_FILE_VERSION,
    UDIF_SECTOR_SIZE,
};
use dmg_parser::SharedStream;
use rust_gtest_interop::prelude::*;

/// Returns the error kind of `result`, or `None` if it is `Ok`. Unlike
/// `unwrap_err()`, this does not panic (and abort the test binary) when the
/// result is unexpectedly `Ok`.
fn err_kind<T>(result: &std::io::Result<T>) -> Option<ErrorKind> {
    result.as_ref().err().map(std::io::Error::kind)
}

/// Compresses `data` into a zlib payload for synthesizing test chunks.
fn compress_zlib(data: &[u8]) -> Vec<u8> {
    use std::io::Write;
    let mut encoder = flate2::write::ZlibEncoder::new(Vec::new(), flate2::Compression::default());
    encoder.write_all(data).expect("zlib compression failed");
    encoder.finish().expect("zlib compression failed")
}

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

fn make_volume_image(signature: u16, block_size: u32) -> Vec<u8> {
    let mut image = vec![0u8; (VOLUME_HEADER_OFFSET as usize) + VOLUME_HEADER_SIZE];
    let header = &mut image[VOLUME_HEADER_OFFSET as usize..];
    header[0..2].copy_from_slice(&signature.to_be_bytes());
    header[40..44].copy_from_slice(&block_size.to_be_bytes());
    // Populate catalogFile (offset 272..352 within HFSPlusVolumeHeader).
    let catalog = &mut header[272..352];
    catalog[0..8].copy_from_slice(&0x0000_0000_0002_0000u64.to_be_bytes());
    // Extent 0: start_block = 10, block_count = 32.
    catalog[16..20].copy_from_slice(&10u32.to_be_bytes());
    catalog[20..24].copy_from_slice(&32u32.to_be_bytes());
    // Extent 7: start_block = 99, block_count = 4.
    catalog[72..76].copy_from_slice(&99u32.to_be_bytes());
    catalog[76..80].copy_from_slice(&4u32.to_be_bytes());
    image
}

fn utf16(s: &str) -> Vec<u16> {
    s.encode_utf16().collect()
}

#[gtest(RustHfsIteratorTest, EmptyOrShortStreamFailsToOpen)]
fn test_empty_or_short_hfs_stream_fails_to_open() {
    let empty_stream = SharedStream::new(Cursor::new(Vec::<u8>::new()));
    expect_eq!(HfsIterator::open(empty_stream).err(), Some(HfsError::IoError));

    let short_stream = SharedStream::new(Cursor::new(vec![
        0u8;
        (VOLUME_HEADER_OFFSET as usize)
            + VOLUME_HEADER_SIZE
            - 1
    ]));
    expect_eq!(HfsIterator::open(short_stream).err(), Some(HfsError::IoError));
}

#[gtest(RustHfsIteratorTest, InvalidVolumeHeaderRejected)]
fn test_invalid_volume_header_rejected() {
    // Classic HFS signature ('BD' = 0x4244) must be rejected.
    let bad_sig_stream = SharedStream::new(Cursor::new(make_volume_image(0x4244, 4096)));
    expect_eq!(HfsIterator::open(bad_sig_stream).err(), Some(HfsError::InvalidVolumeHeader));

    // Zero block_size must be rejected even with a valid HFS+ signature.
    let zero_block_stream = SharedStream::new(Cursor::new(make_volume_image(HFS_PLUS_SIG_WORD, 0)));
    expect_eq!(HfsIterator::open(zero_block_stream).err(), Some(HfsError::InvalidVolumeHeader));
}

#[gtest(RustHfsIteratorTest, ValidVolumeHeaderParsedAndReachesCatalogInit)]
fn test_valid_volume_header_parsed_and_reaches_catalog_init() {
    let image = make_volume_image(HFS_PLUS_SIG_WORD, 4096);
    let header_bytes: &[u8; VOLUME_HEADER_SIZE] = image[VOLUME_HEADER_OFFSET as usize..]
        .first_chunk()
        .expect("`image` has `VOLUME_HEADER_SIZE` bytes starting at `VOLUME_HEADER_OFFSET`");
    let parsed = HfsPlusVolumeHeader::from_big_endian_bytes(header_bytes);
    expect_eq!(parsed.signature, HFS_PLUS_SIG_WORD);
    expect_eq!(parsed.block_size, 4096);
    expect_eq!(parsed.catalog_file.logical_size, 0x20000);
    expect_eq!(
        parsed.catalog_file.extents[0],
        HfsPlusExtentDescriptor { start_block: 10, block_count: 32 }
    );
    expect_eq!(
        parsed.catalog_file.extents[1],
        HfsPlusExtentDescriptor { start_block: 0, block_count: 0 }
    );
    expect_eq!(
        parsed.catalog_file.extents[7],
        HfsPlusExtentDescriptor { start_block: 99, block_count: 4 }
    );

    // Both HFS+ ('H+') and HFSX ('HX') signatures pass volume header validation
    // and reach HfsBTreeIterator::new (currently stubbed as Unimplemented).
    let hfs_plus_stream = SharedStream::new(Cursor::new(image));
    expect_eq!(HfsIterator::open(hfs_plus_stream).err(), Some(HfsError::Unimplemented));

    let hfsx_stream = SharedStream::new(Cursor::new(make_volume_image(HFSX_SIG_WORD, 8192)));
    expect_eq!(HfsIterator::open(hfsx_stream).err(), Some(HfsError::Unimplemented));
}

#[gtest(RustHfsIteratorTest, NextSkipsThreadsAndUnexportedItems)]
fn test_next_skips_threads_and_unexported_items() {
    let entries = vec![
        HfsBTreeEntry { path: utf16(""), unexported: false, record: HfsCatalogRecord::Thread },
        HfsBTreeEntry {
            path: utf16(".HFS+ Private Directory Data\r"),
            unexported: true,
            record: HfsCatalogRecord::Folder(HfsPlusCatalogFolder::default()),
        },
        HfsBTreeEntry {
            path: utf16(".HFS+ Private Directory Data\r/secret"),
            unexported: true,
            record: HfsCatalogRecord::File(HfsPlusCatalogFile::default()),
        },
        HfsBTreeEntry {
            path: utf16("Applications"),
            unexported: false,
            record: HfsCatalogRecord::Folder(HfsPlusCatalogFolder::default()),
        },
        HfsBTreeEntry {
            path: utf16("Applications"),
            unexported: false,
            record: HfsCatalogRecord::Thread,
        },
        HfsBTreeEntry {
            path: utf16("Applications/App.app"),
            unexported: false,
            record: HfsCatalogRecord::File(HfsPlusCatalogFile::default()),
        },
    ];

    let stream = SharedStream::new(Cursor::new(Vec::<u8>::new()));
    let mut iter = HfsIterator::new_for_testing(
        stream,
        4096,
        HfsBTreeIterator::from_entries_for_testing(entries),
    );

    expect_true!(iter.next_item());
    expect_true!(iter.is_directory());
    expect_false!(iter.is_file());
    expect_eq!(iter.path(), utf16("Applications").as_slice());

    expect_true!(iter.next_item());
    expect_false!(iter.is_directory());
    expect_true!(iter.is_file());
    expect_eq!(iter.path(), utf16("Applications/App.app").as_slice());

    expect_false!(iter.next_item());
}

#[gtest(RustHfsIteratorTest, ItemMetadataAndReadStream)]
fn test_item_metadata_and_read_stream() {
    let entries = vec![
        // 1. Regular directory.
        HfsBTreeEntry {
            path: utf16("dir"),
            unexported: false,
            record: HfsCatalogRecord::Folder(HfsPlusCatalogFolder {
                bsd_info: HfsPlusBsdInfo { owner_flags: 0, file_mode: 0o040755 },
            }),
        },
        // 2. Symbolic link directory record.
        HfsBTreeEntry {
            path: utf16("dir_symlink"),
            unexported: false,
            record: HfsCatalogRecord::Folder(HfsPlusCatalogFolder {
                bsd_info: HfsPlusBsdInfo { owner_flags: 0, file_mode: S_IFLNK | 0o755 },
            }),
        },
        // 3. Regular file.
        HfsBTreeEntry {
            path: utf16("dir/regular.txt"),
            unexported: false,
            record: HfsCatalogRecord::File(HfsPlusCatalogFile {
                bsd_info: HfsPlusBsdInfo { owner_flags: 0, file_mode: 0o100644 },
                user_info: FndrFileInfo { fd_type: 0, fd_creator: 0 },
                data_fork: HfsPlusForkData::default(),
            }),
        },
        // 4. Symbolic link file record.
        HfsBTreeEntry {
            path: utf16("dir/file_symlink"),
            unexported: false,
            record: HfsCatalogRecord::File(HfsPlusCatalogFile {
                bsd_info: HfsPlusBsdInfo { owner_flags: 0, file_mode: S_IFLNK | 0o777 },
                user_info: FndrFileInfo::default(),
                data_fork: HfsPlusForkData::default(),
            }),
        },
        // 5. Hard link file record.
        HfsBTreeEntry {
            path: utf16("dir/hardlink"),
            unexported: false,
            record: HfsCatalogRecord::File(HfsPlusCatalogFile {
                bsd_info: HfsPlusBsdInfo { owner_flags: 0, file_mode: 0o100644 },
                user_info: FndrFileInfo {
                    fd_type: HARD_LINK_FILE_TYPE,
                    fd_creator: HFS_PLUS_CREATOR,
                },
                data_fork: HfsPlusForkData::default(),
            }),
        },
        // 6. File with matching fd_type but non-matching fd_creator (not a hard link).
        HfsBTreeEntry {
            path: utf16("dir/not_hardlink"),
            unexported: false,
            record: HfsCatalogRecord::File(HfsPlusCatalogFile {
                bsd_info: HfsPlusBsdInfo { owner_flags: 0, file_mode: 0o100644 },
                user_info: FndrFileInfo { fd_type: HARD_LINK_FILE_TYPE, fd_creator: 0x1234_5678 },
                data_fork: HfsPlusForkData::default(),
            }),
        },
        // 7. decmpfs-compressed file record.
        HfsBTreeEntry {
            path: utf16("dir/compressed"),
            unexported: false,
            record: HfsCatalogRecord::File(HfsPlusCatalogFile {
                bsd_info: HfsPlusBsdInfo { owner_flags: UF_COMPRESSED, file_mode: 0o100644 },
                user_info: FndrFileInfo::default(),
                data_fork: HfsPlusForkData::default(),
            }),
        },
    ];

    let stream = SharedStream::new(Cursor::new(Vec::<u8>::new()));
    let mut iter = HfsIterator::new_for_testing(
        stream,
        4096,
        HfsBTreeIterator::from_entries_for_testing(entries),
    );

    struct Expected {
        path: &'static str,
        is_dir: bool,
        is_file: bool,
        is_symlink: bool,
        is_hardlink: bool,
        is_compressed: bool,
        has_stream: bool,
    }

    let expectations = [
        Expected {
            path: "dir",
            is_dir: true,
            is_file: false,
            is_symlink: false,
            is_hardlink: false,
            is_compressed: false,
            has_stream: false,
        },
        Expected {
            path: "dir_symlink",
            is_dir: true,
            is_file: false,
            is_symlink: true,
            is_hardlink: false,
            is_compressed: false,
            has_stream: false,
        },
        Expected {
            path: "dir/regular.txt",
            is_dir: false,
            is_file: true,
            is_symlink: false,
            is_hardlink: false,
            is_compressed: false,
            has_stream: true,
        },
        Expected {
            path: "dir/file_symlink",
            is_dir: false,
            is_file: true,
            is_symlink: true,
            is_hardlink: false,
            is_compressed: false,
            has_stream: true,
        },
        Expected {
            path: "dir/hardlink",
            is_dir: false,
            is_file: true,
            is_symlink: false,
            is_hardlink: true,
            is_compressed: false,
            has_stream: false,
        },
        Expected {
            path: "dir/not_hardlink",
            is_dir: false,
            is_file: true,
            is_symlink: false,
            is_hardlink: false,
            is_compressed: false,
            has_stream: true,
        },
        Expected {
            path: "dir/compressed",
            is_dir: false,
            is_file: true,
            is_symlink: false,
            is_hardlink: false,
            is_compressed: true,
            has_stream: true,
        },
    ];

    for exp in expectations {
        expect_true!(iter.next_item(), "for path {}", exp.path);
        expect_eq!(iter.path(), utf16(exp.path).as_slice(), "for path {}", exp.path);
        expect_eq!(iter.is_directory(), exp.is_dir, "for path {}", exp.path);
        expect_eq!(iter.is_file(), exp.is_file, "for path {}", exp.path);
        expect_eq!(iter.is_symbolic_link(), exp.is_symlink, "for path {}", exp.path);
        expect_eq!(iter.is_hard_link(), exp.is_hardlink, "for path {}", exp.path);
        expect_eq!(iter.is_decmpfs_compressed(), exp.is_compressed, "for path {}", exp.path);
        expect_eq!(iter.read_stream().is_some(), exp.has_stream, "for path {}", exp.path);
    }

    expect_false!(iter.next_item());
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

    let fork = HfsPlusForkData::from_big_endian_bytes(&raw);
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

#[gtest(RustUdifChunkTest, ZeroFillAndIgnoredChunks)]
fn test_udif_zerofill_and_ignored_chunks() {
    let stream = SharedStream::new(Cursor::new(vec![0xaau8; 1024]));

    let zerofill_chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::ZERO_FILL,
        comment: 0,
        start_sector: 0,
        sector_count: 2, // 1024 bytes (512 block_size)
        compressed_offset: 0,
        compressed_length: 0,
    };

    let mut reader = UdifBlockChunkReadStream::new(stream.clone(), 512, &zerofill_chunk)
        .expect("ZeroFill chunk initialization should succeed");
    expect_eq!(reader.length_in_bytes(), 1024);
    expect_false!(reader.is_at_end());

    let mut buf = [0xffu8; 512];
    expect_eq!(reader.read(&mut buf).ok(), Some(512));
    expect_true!(buf.iter().all(|&b| b == 0));
    expect_eq!(reader.offset(), 512);

    expect_eq!(reader.seek(SeekFrom::Start(100)).ok(), Some(100));
    let mut small_buf = [0xffu8; 10];
    expect_eq!(reader.read(&mut small_buf).ok(), Some(10));
    expect_true!(small_buf.iter().all(|&b| b == 0));
    expect_eq!(reader.offset(), 110);

    expect_eq!(reader.seek(SeekFrom::End(0)).ok(), Some(1024));
    expect_true!(reader.is_at_end());
    expect_eq!(reader.read(&mut buf).ok(), Some(0));

    // Test Ignored chunk behaves identical to ZeroFill
    let ignored_chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::IGNORED,
        comment: 0,
        start_sector: 2,
        sector_count: 1, // 512 bytes
        compressed_offset: 0,
        compressed_length: 0,
    };
    let mut ignored_reader = UdifBlockChunkReadStream::new(stream, 512, &ignored_chunk).unwrap();
    let mut ign_buf = [0xffu8; 512];
    expect_eq!(ignored_reader.read(&mut ign_buf).ok(), Some(512));
    expect_true!(ign_buf.iter().all(|&b| b == 0));
}

#[gtest(RustUdifChunkTest, UncompressedChunkReadAndSeek)]
fn test_udif_uncompressed_chunk() {
    let raw_payload = b"Hello, Chromium Safe Browsing UDIF Chunk Stream!";
    let mut image_data = vec![0u8; 1024];
    let chunk_start_offset = 64u64;
    image_data[chunk_start_offset as usize..chunk_start_offset as usize + raw_payload.len()]
        .copy_from_slice(raw_payload);

    let stream = SharedStream::new(Cursor::new(image_data));
    let chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::UNCOMPRESSED,
        comment: 0,
        start_sector: 0,
        sector_count: 1, // 512 bytes
        compressed_offset: chunk_start_offset,
        compressed_length: 512,
    };

    let mut reader = UdifBlockChunkReadStream::new(stream, 512, &chunk).unwrap();
    let mut read_buf = vec![0u8; raw_payload.len()];
    expect_eq!(reader.read_exact(&mut read_buf).ok(), Some(()));
    expect_eq!(&read_buf, raw_payload);

    // SeekFrom::Current
    expect_eq!(reader.seek(SeekFrom::Current(10)).ok(), Some(raw_payload.len() as u64 + 10));

    // SeekFrom::Start
    expect_eq!(reader.seek(SeekFrom::Start(7)).ok(), Some(7));
    let mut sub_buf = [0u8; 8];
    expect_eq!(reader.read_exact(&mut sub_buf).ok(), Some(()));
    expect_eq!(&sub_buf, &raw_payload[7..15]);

    // SeekFrom::End
    expect_eq!(reader.seek(SeekFrom::End(-12)).ok(), Some(500));
}

#[gtest(RustUdifChunkTest, CompressZlibChunk)]
fn test_udif_zlib_decompression() {
    let mut payload = vec![0u8; 1024];
    for (i, b) in payload.iter_mut().enumerate() {
        *b = ((i * 17 + 5) % 251) as u8;
    }

    let compressed = compress_zlib(&payload);
    let stream = SharedStream::new(Cursor::new(compressed.clone()));

    let chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::COMPRESS_ZLIB,
        comment: 0,
        start_sector: 0,
        sector_count: 2, // 1024 bytes (512 block size)
        compressed_offset: 0,
        compressed_length: compressed.len() as u64,
    };

    let mut reader = UdifBlockChunkReadStream::new(stream, 512, &chunk).unwrap();
    let mut decompressed = vec![0u8; 1024];
    expect_eq!(reader.read_exact(&mut decompressed).ok(), Some(()));
    expect_eq!(&decompressed, &payload);

    // Intra-chunk seeking in cached buffer
    expect_eq!(reader.seek(SeekFrom::Start(250)).ok(), Some(250));
    let mut slice_buf = [0u8; 20];
    expect_eq!(reader.read_exact(&mut slice_buf).ok(), Some(()));
    expect_eq!(&slice_buf, &payload[250..270]);
}

#[gtest(RustUdifChunkTest, CompressBz2Chunk)]
fn test_udif_bz2_decompression() {
    let mut payload = vec![0u8; 1024];
    for (i, b) in payload.iter_mut().enumerate() {
        *b = ((i * 31 + 47) % 251) as u8;
    }

    let compressed = compress_bz2(&payload).expect("bz2 compress should succeed");
    let stream = SharedStream::new(Cursor::new(compressed.clone()));

    let chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::COMPRESS_BZ2,
        comment: 0,
        start_sector: 0,
        sector_count: 2, // 1024 bytes (512 block size)
        compressed_offset: 0,
        compressed_length: compressed.len() as u64,
    };

    let mut reader = UdifBlockChunkReadStream::new(stream, 512, &chunk).unwrap();
    let mut decompressed = vec![0u8; 1024];
    expect_eq!(reader.read_exact(&mut decompressed).ok(), Some(()));
    expect_eq!(&decompressed, &payload);

    // Intra-chunk seeking in cached buffer
    expect_eq!(reader.seek(SeekFrom::Start(100)).ok(), Some(100));
    let mut slice_buf = [0u8; 15];
    expect_eq!(reader.read_exact(&mut slice_buf).ok(), Some(()));
    expect_eq!(&slice_buf, &payload[100..115]);
}

#[gtest(RustUdifChunkTest, DecompressionBombProtection)]
fn test_udif_decompression_bomb_protection() {
    let stream = SharedStream::new(Cursor::new(vec![]));
    let bomb_sectors = (MAX_CHUNK_DECOMPRESSED_SIZE / 512) + 1;

    let bomb_chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::COMPRESS_ZLIB,
        comment: 0,
        start_sector: 0,
        sector_count: bomb_sectors,
        compressed_offset: 0,
        compressed_length: 100,
    };

    let zlib_res = UdifBlockChunkReadStream::new(stream.clone(), 512, &bomb_chunk);
    expect_eq!(err_kind(&zlib_res), Some(ErrorKind::InvalidData));

    let compressed_bomb_chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::COMPRESS_BZ2,
        comment: 0,
        start_sector: 0,
        sector_count: 1,
        compressed_offset: 0,
        compressed_length: MAX_CHUNK_COMPRESSED_SIZE + 1,
    };
    let bz2_res = UdifBlockChunkReadStream::new(stream.clone(), 512, &compressed_bomb_chunk);
    expect_eq!(err_kind(&bz2_res), Some(ErrorKind::InvalidData));

    // Large ZeroFill chunks do NOT allocate decompression buffers, so they must
    // be allowed.
    let zerofill_large = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::ZERO_FILL,
        comment: 0,
        start_sector: 0,
        sector_count: bomb_sectors,
        compressed_offset: 0,
        compressed_length: 0,
    };
    let mut zf_reader =
        UdifBlockChunkReadStream::new(stream, 512, &zerofill_large).expect("ZeroFill can be large");
    let mut small_buf = [0xffu8; 32];
    expect_eq!(zf_reader.read(&mut small_buf).ok(), Some(32));
    expect_true!(small_buf.iter().all(|&b| b == 0));
}

#[gtest(RustUdifChunkTest, UnexpectedEofUncompressed)]
fn test_udif_unexpected_eof_uncompressed() {
    let short_data = b"ShortData";
    let stream = SharedStream::new(Cursor::new(short_data.to_vec()));

    let chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::UNCOMPRESSED,
        comment: 0,
        start_sector: 0,
        sector_count: 1, // 512 bytes expected
        compressed_offset: 0,
        compressed_length: 512,
    };

    let mut reader = UdifBlockChunkReadStream::new(stream, 512, &chunk).unwrap();
    let mut buf = [0u8; 512];
    // First read returns the 9 available bytes
    let n1 = reader.read(&mut buf).unwrap();
    expect_eq!(n1, 9);
    // Next read on underlying EOF returns UnexpectedEof
    expect_eq!(err_kind(&reader.read(&mut buf)), Some(ErrorKind::UnexpectedEof));
}

#[gtest(RustUdifChunkTest, CorruptedCompression)]
fn test_udif_corrupted_compression() {
    let corrupted = b"this is completely corrupted data for compression engines";
    let stream = SharedStream::new(Cursor::new(corrupted.to_vec()));

    let zlib_chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::COMPRESS_ZLIB,
        comment: 0,
        start_sector: 0,
        sector_count: 1,
        compressed_offset: 0,
        compressed_length: corrupted.len() as u64,
    };
    let mut zlib_reader = UdifBlockChunkReadStream::new(stream.clone(), 512, &zlib_chunk).unwrap();
    let mut buf = [0u8; 512];
    expect_eq!(err_kind(&zlib_reader.read(&mut buf)), Some(ErrorKind::InvalidData));

    let bz2_chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::COMPRESS_BZ2,
        comment: 0,
        start_sector: 0,
        sector_count: 1,
        compressed_offset: 0,
        compressed_length: corrupted.len() as u64,
    };
    let mut bz2_reader = UdifBlockChunkReadStream::new(stream, 512, &bz2_chunk).unwrap();
    expect_eq!(err_kind(&bz2_reader.read(&mut buf)), Some(ErrorKind::InvalidData));
}

#[gtest(RustUdifChunkTest, Bz2StallDetection)]
fn test_udif_bz2_stall_detection() {
    // Truncated bz2 header that cannot make decompression progress
    let partial_bz2 = [0x42, 0x5a, 0x68, 0x39]; // "BZh9"
    let mut decompressed = [0u8; 100];
    let res = decompress_bz2(&partial_bz2, &mut decompressed);
    let kind = err_kind(&res);
    expect_true!(kind == Some(ErrorKind::UnexpectedEof) || kind == Some(ErrorKind::InvalidData));
}

#[gtest(RustUdifChunkTest, ChunkTypeFromU32)]
fn test_udif_chunk_type_from_u32() {
    let cases = [
        (0x0000_0000, UdifBlockChunkType::ZERO_FILL),
        (0x0000_0001, UdifBlockChunkType::UNCOMPRESSED),
        (0x0000_0002, UdifBlockChunkType::IGNORED),
        (0x8000_0004, UdifBlockChunkType::COMPRESS_ADC),
        (0x8000_0005, UdifBlockChunkType::COMPRESS_ZLIB),
        (0x8000_0006, UdifBlockChunkType::COMPRESS_BZ2),
        (0x7fff_fffe, UdifBlockChunkType::COMMENT),
        (0xffff_ffff, UdifBlockChunkType::LAST_BLOCK),
        // LZFSE (ULFO) and LZMA (ULMO) are not supported but must not fail parsing.
        (0x8000_0007, UdifBlockChunkType(0x8000_0007)),
        (0x8000_0008, UdifBlockChunkType(0x8000_0008)),
    ];
    for (raw, expected) in cases {
        expect_eq!(UdifBlockChunkType::from(raw), expected, "raw: 0x{raw:08x}");
    }
}

#[gtest(RustUdifChunkTest, UnknownChunkTypeReadUnsupported)]
fn test_udif_unknown_chunk_type_read_unsupported() {
    let stream = SharedStream::new(Cursor::new(vec![0u8; 1024]));
    let chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::from(0x8000_0007),
        comment: 0,
        start_sector: 0,
        sector_count: 1,
        compressed_offset: 0,
        compressed_length: 100,
    };
    let mut reader = UdifBlockChunkReadStream::new(stream, 512, &chunk).unwrap();
    let mut buf = [0u8; 16];
    expect_eq!(err_kind(&reader.read(&mut buf)), Some(ErrorKind::Unsupported));
}

#[gtest(RustUdifChunkTest, UnsupportedAndSpecialChunks)]
fn test_udif_unsupported_and_special_chunks() {
    let stream = SharedStream::new(Cursor::new(vec![0u8; 128]));

    let adc_chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::COMPRESS_ADC,
        comment: 0,
        start_sector: 0,
        sector_count: 1,
        compressed_offset: 0,
        compressed_length: 50,
    };
    let mut adc_reader = UdifBlockChunkReadStream::new(stream.clone(), 512, &adc_chunk).unwrap();
    let mut buf = [0u8; 10];
    expect_eq!(err_kind(&adc_reader.read(&mut buf)), Some(ErrorKind::Unsupported));

    let comment_chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::COMMENT,
        comment: 0,
        start_sector: 0,
        sector_count: 1,
        compressed_offset: 0,
        compressed_length: 0,
    };
    let mut comment_reader =
        UdifBlockChunkReadStream::new(stream.clone(), 512, &comment_chunk).unwrap();
    expect_eq!(err_kind(&comment_reader.read(&mut buf)), Some(ErrorKind::InvalidData));

    let last_block = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::LAST_BLOCK,
        comment: 0,
        start_sector: 0,
        sector_count: 0,
        compressed_offset: 0,
        compressed_length: 0,
    };
    let mut last_reader = UdifBlockChunkReadStream::new(stream, 512, &last_block).unwrap();
    expect_eq!(last_reader.read(&mut buf).ok(), Some(0));
}

#[gtest(RustUdifChunkTest, ArithmeticOverflow)]
fn test_udif_arithmetic_overflow() {
    let stream = SharedStream::new(Cursor::new(vec![]));
    let overflow_chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::ZERO_FILL,
        comment: 0,
        start_sector: 0,
        sector_count: u64::MAX,
        compressed_offset: 0,
        compressed_length: 0,
    };
    let res = UdifBlockChunkReadStream::new(stream.clone(), 512, &overflow_chunk);
    expect_eq!(err_kind(&res), Some(ErrorKind::InvalidData));

    let valid_chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::ZERO_FILL,
        comment: 0,
        start_sector: 0,
        sector_count: 1,
        compressed_offset: 0,
        compressed_length: 0,
    };
    let mut reader = UdifBlockChunkReadStream::new(stream, 512, &valid_chunk).unwrap();
    expect_true!(reader.seek(SeekFrom::Start(u64::MAX)).is_err());
    expect_true!(reader.seek(SeekFrom::Current(i64::MIN)).is_err());
    expect_true!(reader.seek(SeekFrom::End(i64::MIN)).is_err());
}

#[gtest(RustUdifChunkTest, BlockSizeZeroRejected)]
fn test_udif_block_size_zero_rejected() {
    let stream = SharedStream::new(Cursor::new(vec![]));
    let chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::ZERO_FILL,
        comment: 0,
        start_sector: 0,
        sector_count: 1,
        compressed_offset: 0,
        compressed_length: 0,
    };
    let res = UdifBlockChunkReadStream::new(stream, 0, &chunk);
    expect_eq!(err_kind(&res), Some(ErrorKind::InvalidInput));
}

#[gtest(RustUdifChunkTest, CompressedOffsetOverflow)]
fn test_udif_compressed_offset_overflow() {
    let stream = SharedStream::new(Cursor::new(vec![]));
    let compressed_overflow = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::COMPRESS_ZLIB,
        comment: 0,
        start_sector: 0,
        sector_count: 1,
        compressed_offset: u64::MAX - 10,
        compressed_length: 20,
    };
    let res_zlib = UdifBlockChunkReadStream::new(stream.clone(), 512, &compressed_overflow);
    expect_eq!(err_kind(&res_zlib), Some(ErrorKind::InvalidData));

    let uncompressed_overflow = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::UNCOMPRESSED,
        comment: 0,
        start_sector: 0,
        sector_count: 2, // 1024 bytes
        compressed_offset: u64::MAX - 100,
        compressed_length: 1024,
    };
    let res_uncomp = UdifBlockChunkReadStream::new(stream, 512, &uncompressed_overflow);
    expect_eq!(err_kind(&res_uncomp), Some(ErrorKind::InvalidData));
}

#[gtest(RustUdifChunkTest, EmptyCompressedDataRejected)]
fn test_udif_empty_compressed_data_rejected() {
    let mut decompressed = [0u8; 100];
    let bz2_res = decompress_bz2(&[], &mut decompressed);
    expect_eq!(err_kind(&bz2_res), Some(ErrorKind::UnexpectedEof));

    // 0 decompressed bytes into 0 buffer should succeed
    expect_eq!(decompress_bz2(&[], &mut []).ok(), Some(0));

    // Verify chunk stream rejects empty compressed data
    let stream = SharedStream::new(Cursor::new(vec![]));
    let zlib_chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::COMPRESS_ZLIB,
        comment: 0,
        start_sector: 0,
        sector_count: 1,
        compressed_offset: 0,
        compressed_length: 0,
    };
    let mut zlib_reader = UdifBlockChunkReadStream::new(stream.clone(), 512, &zlib_chunk).unwrap();
    let mut buf = [0u8; 512];
    expect_true!(zlib_reader.read(&mut buf).is_err());

    let bz2_chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::COMPRESS_BZ2,
        comment: 0,
        start_sector: 0,
        sector_count: 1,
        compressed_offset: 0,
        compressed_length: 0,
    };
    let mut bz2_reader = UdifBlockChunkReadStream::new(stream, 512, &bz2_chunk).unwrap();
    expect_true!(bz2_reader.read(&mut buf).is_err());
}

#[gtest(RustUdifChunkTest, ZlibShortPayloadZeroPaddedTail)]
fn test_udif_zlib_short_payload_zero_padded_tail() {
    // 300 bytes of non-zero payload decompressed into a 2-sector (1024-byte)
    // chunk.
    let payload = vec![0xABu8; 300];
    let compressed = compress_zlib(&payload);
    let stream = SharedStream::new(Cursor::new(compressed.clone()));
    let chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::COMPRESS_ZLIB,
        comment: 0,
        start_sector: 0,
        sector_count: 2, // 1024 bytes
        compressed_offset: 0,
        compressed_length: compressed.len() as u64,
    };
    let mut reader = UdifBlockChunkReadStream::new(stream, 512, &chunk).unwrap();
    let mut out = vec![0xFFu8; 1024];
    expect_eq!(reader.read_exact(&mut out).ok(), Some(()));
    expect_eq!(&out[..300], &payload[..]);
    expect_true!(out[300..].iter().all(|&b| b == 0));
}

#[gtest(RustUdifChunkTest, Bz2ShortPayloadZeroPaddedTail)]
fn test_udif_bz2_short_payload_zero_padded_tail() {
    // 300 bytes of non-zero payload decompressed into a 2-sector (1024-byte)
    // chunk.
    let payload = vec![0xCDu8; 300];
    let compressed = compress_bz2(&payload).expect("bz2 compress should succeed");
    let stream = SharedStream::new(Cursor::new(compressed.clone()));
    let chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::COMPRESS_BZ2,
        comment: 0,
        start_sector: 0,
        sector_count: 2, // 1024 bytes
        compressed_offset: 0,
        compressed_length: compressed.len() as u64,
    };
    let mut reader = UdifBlockChunkReadStream::new(stream, 512, &chunk).unwrap();
    let mut out = vec![0xFFu8; 1024];
    expect_eq!(reader.read_exact(&mut out).ok(), Some(()));
    expect_eq!(&out[..300], &payload[..]);
    expect_true!(out[300..].iter().all(|&b| b == 0));
}

#[gtest(RustUdifChunkTest, PayloadLargerThanChunkErrors)]
fn test_udif_payload_larger_than_chunk_errors() {
    // 1024 bytes of payload compressed into a 1-sector (512-byte) chunk.
    let payload = vec![0xEFu8; 1024];
    let zlib_compressed = compress_zlib(&payload);
    let stream_zlib = SharedStream::new(Cursor::new(zlib_compressed.clone()));
    let zlib_chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::COMPRESS_ZLIB,
        comment: 0,
        start_sector: 0,
        sector_count: 1, // 512 bytes
        compressed_offset: 0,
        compressed_length: zlib_compressed.len() as u64,
    };
    let mut zlib_reader = UdifBlockChunkReadStream::new(stream_zlib, 512, &zlib_chunk).unwrap();
    let mut buf = [0u8; 512];
    expect_true!(zlib_reader.read(&mut buf).is_err());

    let bz2_compressed = compress_bz2(&payload).expect("bz2 compress should succeed");
    let stream_bz2 = SharedStream::new(Cursor::new(bz2_compressed.clone()));
    let bz2_chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::COMPRESS_BZ2,
        comment: 0,
        start_sector: 0,
        sector_count: 1, // 512 bytes
        compressed_offset: 0,
        compressed_length: bz2_compressed.len() as u64,
    };
    let mut bz2_reader = UdifBlockChunkReadStream::new(stream_bz2, 512, &bz2_chunk).unwrap();
    expect_true!(bz2_reader.read(&mut buf).is_err());
}

#[gtest(RustUdifChunkTest, CompressedDataExceedsStreamLength)]
fn test_udif_compressed_data_exceeds_stream_length() {
    let stream = SharedStream::new(Cursor::new(vec![0u8; 50]));
    let chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::COMPRESS_ZLIB,
        comment: 0,
        start_sector: 0,
        sector_count: 1,
        compressed_offset: 40,
        compressed_length: 20, // 40 + 20 = 60 > 50
    };
    let mut reader = UdifBlockChunkReadStream::new(stream, 512, &chunk).unwrap();
    let mut buf = [0u8; 512];
    let res = reader.read(&mut buf);
    expect_eq!(err_kind(&res), Some(ErrorKind::UnexpectedEof));
}

#[gtest(RustUdifChunkTest, SharedStreamStreamLen)]
fn test_shared_stream_stream_len() {
    let data = b"hello world 12345";
    let stream = SharedStream::new(Cursor::new(data.to_vec()));
    expect_eq!(stream.stream_len().ok(), Some(data.len() as u64));
}

#[gtest(RustUdifChunkTest, ExactBoundary128MB)]
fn test_udif_exact_boundary_128mb() {
    let stream = SharedStream::new(Cursor::new(vec![]));
    let exact_sectors = MAX_CHUNK_DECOMPRESSED_SIZE / 512;

    let exact_chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::COMPRESS_ZLIB,
        comment: 0,
        start_sector: 0,
        sector_count: exact_sectors,
        compressed_offset: 0,
        compressed_length: MAX_CHUNK_COMPRESSED_SIZE,
    };
    // Initialization at the exact boundary must succeed without allocating
    // decompress_buffer yet
    let res = UdifBlockChunkReadStream::new(stream, 512, &exact_chunk);
    expect_true!(res.is_ok());
    let reader = res.unwrap();
    expect_eq!(reader.length_in_bytes(), MAX_CHUNK_DECOMPRESSED_SIZE);
}

#[gtest(RustUdifChunkTest, Bz2ShortPayloadReturnsBytesProduced)]
fn test_udif_bz2_short_payload_returns_bytes_produced() {
    let payload = vec![0x5Au8; 300];
    let compressed = compress_bz2(&payload).expect("bz2 compress should succeed");
    let mut out = vec![0u8; 512];
    expect_eq!(decompress_bz2(&compressed, &mut out).ok(), Some(payload.len()));
    expect_eq!(&out[..payload.len()], &payload[..]);
    expect_true!(out[payload.len()..].iter().all(|&b| b == 0));
}

#[gtest(RustUdifPartitionTest, SeekVariantsAndBoundaries)]
fn test_seek_variants_and_boundaries() {
    let payload = b"0123456789abcdef";
    let cursor = SharedStream::new(Cursor::new(payload.to_vec()));

    let chunks = vec![UdifBlockChunk {
        chunk_type: UdifBlockChunkType::UNCOMPRESSED,
        comment: 0,
        start_sector: 0,
        sector_count: 1,
        compressed_offset: 0,
        compressed_length: 16,
    }];

    let mut stream = UdifPartitionReadStream::new(cursor, 16, &chunks, 1).unwrap();

    // SeekFrom::Start
    expect_eq!(stream.seek(SeekFrom::Start(5)).unwrap(), 5);
    let mut b = [0u8; 3];
    stream.read_exact(&mut b).unwrap();
    expect_eq!(&b, b"567");

    // SeekFrom::Current
    expect_eq!(stream.seek(SeekFrom::Current(2)).unwrap(), 10);
    stream.read_exact(&mut b).unwrap();
    expect_eq!(&b, b"abc");

    // SeekFrom::Current negative
    expect_eq!(stream.seek(SeekFrom::Current(-5)).unwrap(), 8);
    let mut b2 = [0u8; 2];
    stream.read_exact(&mut b2).unwrap();
    expect_eq!(&b2, b"89");

    // SeekFrom::End
    expect_eq!(stream.seek(SeekFrom::End(-4)).unwrap(), 12);
    let mut b4 = [0u8; 4];
    stream.read_exact(&mut b4).unwrap();
    expect_eq!(&b4, b"cdef");

    // Seek to exact end
    expect_eq!(stream.seek(SeekFrom::End(0)).unwrap(), 16);
    let mut eof_buf = [0u8; 10];
    expect_eq!(stream.read(&mut eof_buf).unwrap(), 0);

    // Stream position at EOF and seek backwards from current
    expect_eq!(stream.stream_position().unwrap(), 16);
    expect_eq!(stream.seek(SeekFrom::Current(-4)).unwrap(), 12);
    expect_eq!(stream.stream_position().unwrap(), 12);
    let mut tail = [0u8; 4];
    stream.read_exact(&mut tail).unwrap();
    expect_eq!(&tail, b"cdef");

    // Seek past end errors
    expect_true!(stream.seek(SeekFrom::Start(17)).is_err());
    expect_true!(stream.seek(SeekFrom::End(1)).is_err());

    // Seek before start errors
    expect_true!(stream.seek(SeekFrom::Current(-100)).is_err());
    expect_true!(stream.seek(SeekFrom::End(-20)).is_err());
    expect_true!(stream.seek(SeekFrom::Current(i64::MIN)).is_err());
    expect_true!(stream.seek(SeekFrom::End(i64::MIN)).is_err());

    // Read past EOF multiple times returns 0
    stream.seek(SeekFrom::End(0)).unwrap();
    expect_eq!(stream.read(&mut eof_buf).unwrap(), 0);
    expect_eq!(stream.read(&mut eof_buf).unwrap(), 0);
}

#[gtest(RustUdifPartitionTest, OverlappingChunksRejected)]
fn test_overlapping_chunks_rejected() {
    let cursor = SharedStream::new(Cursor::new(vec![0u8; 1024]));
    let chunks = vec![
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::ZERO_FILL,
            comment: 0,
            start_sector: 0,
            sector_count: 10,
            compressed_offset: 0,
            compressed_length: 0,
        },
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::ZERO_FILL,
            comment: 0,
            start_sector: 5,
            sector_count: 10,
            compressed_offset: 0,
            compressed_length: 0,
        },
    ];

    let res = UdifPartitionReadStream::new(cursor, 512, &chunks, 20);
    expect_eq!(err_kind(&res), Some(ErrorKind::InvalidData));
}

#[gtest(RustUdifPartitionTest, ChunksExceedingTotalSectorCountRejected)]
fn test_chunks_exceeding_total_sector_count_rejected() {
    let cursor = SharedStream::new(Cursor::new(vec![0u8; 1024]));
    let chunks = vec![UdifBlockChunk {
        chunk_type: UdifBlockChunkType::ZERO_FILL,
        comment: 0,
        start_sector: 0,
        sector_count: 10,
        compressed_offset: 0,
        compressed_length: 0,
    }];

    let res = UdifPartitionReadStream::new(cursor, 512, &chunks, 5);
    expect_eq!(err_kind(&res), Some(ErrorKind::InvalidData));
}

#[gtest(RustUdifPartitionTest, ChunkAfterLastBlockRejected)]
fn test_chunk_after_last_block_rejected() {
    let cursor = SharedStream::new(Cursor::new(vec![0u8; 1024]));
    let chunks = vec![
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::LAST_BLOCK,
            comment: 0,
            start_sector: 0,
            sector_count: 0,
            compressed_offset: 0,
            compressed_length: 0,
        },
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::ZERO_FILL,
            comment: 0,
            start_sector: 0,
            sector_count: 1,
            compressed_offset: 0,
            compressed_length: 0,
        },
    ];

    let res = UdifPartitionReadStream::new(cursor, 512, &chunks, 1);
    expect_eq!(err_kind(&res), Some(ErrorKind::InvalidData));
}

#[gtest(RustUdifPartitionTest, UnsortedChunksRejected)]
fn test_unsorted_chunks_rejected() {
    let cursor = SharedStream::new(Cursor::new(vec![0u8; 1024]));
    let chunks = vec![
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::ZERO_FILL,
            comment: 0,
            start_sector: 5,
            sector_count: 1,
            compressed_offset: 0,
            compressed_length: 0,
        },
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::ZERO_FILL,
            comment: 0,
            start_sector: 2,
            sector_count: 1,
            compressed_offset: 0,
            compressed_length: 0,
        },
    ];

    let res = UdifPartitionReadStream::new(cursor, 512, &chunks, 6);
    expect_eq!(err_kind(&res), Some(ErrorKind::InvalidData));
}

#[gtest(RustUdifPartitionTest, UnmappedGapSeekError)]
fn test_unmapped_gap_seek_error() {
    let cursor = SharedStream::new(Cursor::new(vec![0u8; 1024]));
    let chunks = vec![
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::ZERO_FILL,
            comment: 0,
            start_sector: 0,
            sector_count: 1,
            compressed_offset: 0,
            compressed_length: 0,
        },
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::ZERO_FILL,
            comment: 0,
            start_sector: 3,
            sector_count: 1,
            compressed_offset: 0,
            compressed_length: 0,
        },
    ];

    let mut stream = UdifPartitionReadStream::new(cursor, 512, &chunks, 4).unwrap();

    // Seeking into sector 2 (offset 1024) is in an unmapped gap
    let res = stream.seek(SeekFrom::Start(1024));
    expect_eq!(err_kind(&res), Some(ErrorKind::InvalidInput));
}

#[gtest(RustUdifPartitionTest, UnmappedGapReadErrorAndPositionPreservation)]
fn test_unmapped_gap_read_error_and_position_preservation() {
    let payload = b"ChunkZeroData___"; // 16 bytes
    let cursor = SharedStream::new(Cursor::new(payload.to_vec()));

    let chunks = vec![
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::UNCOMPRESSED,
            comment: 0,
            start_sector: 0,
            sector_count: 1,
            compressed_offset: 0,
            compressed_length: 16,
        },
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::ZERO_FILL,
            comment: 0,
            start_sector: 2,
            sector_count: 1,
            compressed_offset: 0,
            compressed_length: 0,
        },
    ];

    let mut stream = UdifPartitionReadStream::new(cursor, 16, &chunks, 3).unwrap();

    // Read chunk 0 completely (16 bytes)
    let mut buf = [0u8; 16];
    stream.read_exact(&mut buf).unwrap();
    expect_eq!(&buf, payload);

    // Verify stream_position is exactly at 16 (end of chunk 0)
    expect_eq!(stream.stream_position().unwrap(), 16);

    // Attempting to read into sector 1 (unmapped gap) must fail cleanly with
    // InvalidData
    let mut next_byte = [0u8; 1];
    expect_eq!(err_kind(&stream.read(&mut next_byte)), Some(ErrorKind::InvalidData));

    // Seeking to sector 2 (offset 32) skips the gap and lands on Chunk 1
    expect_eq!(stream.seek(SeekFrom::Start(32)).unwrap(), 32);
    expect_eq!(stream.stream_position().unwrap(), 32);

    let mut chunk1_buf = [0xffu8; 16];
    stream.read_exact(&mut chunk1_buf).unwrap();
    expect_true!(chunk1_buf.iter().all(|&b| b == 0));
    expect_eq!(stream.stream_position().unwrap(), 48);
}

#[gtest(RustUdifPartitionTest, ReadBufferLargerThanPartition)]
fn test_read_buffer_larger_than_partition() {
    let payload = b"SmallPartition";
    let mut data = vec![0u8; 32];
    data[..payload.len()].copy_from_slice(payload);
    let cursor = SharedStream::new(Cursor::new(data));

    let chunks = vec![UdifBlockChunk {
        chunk_type: UdifBlockChunkType::UNCOMPRESSED,
        comment: 0,
        start_sector: 0,
        sector_count: 1, // 32 bytes
        compressed_offset: 0,
        compressed_length: 32,
    }];

    let mut stream = UdifPartitionReadStream::new(cursor, 32, &chunks, 1).unwrap();
    let mut large_buf = vec![0u8; 1024];
    let n = stream.read(&mut large_buf).unwrap();
    expect_eq!(n, 32);
    expect_eq!(&large_buf[..payload.len()], payload);
    expect_eq!(stream.read(&mut large_buf).unwrap(), 0);
}

#[gtest(RustUdifPartitionTest, UdifParserPartitionStreamWiring)]
fn test_udif_parser_partition_stream_wiring() {
    let payload = b"PartitionData123";
    let cursor = SharedStream::new(Cursor::new(payload.to_vec()));

    let block0 = UdifBlock::new_for_testing(
        1,
        vec![UdifBlockChunk {
            chunk_type: UdifBlockChunkType::UNCOMPRESSED,
            comment: 0,
            start_sector: 0,
            sector_count: 1,
            compressed_offset: 0,
            compressed_length: payload.len() as u64,
        }],
    );

    let parser = UdifParser::with_blocks(cursor, 16, vec![block0]);

    let mut part_stream = parser.partition_read_stream(0).expect("Stream 0 must exist");
    let mut buf = vec![0u8; 16];
    part_stream.read_exact(&mut buf).unwrap();
    expect_eq!(&buf, payload);

    expect_true!(parser.partition_read_stream(1).is_none());
}

#[gtest(RustUdifPartitionTest, ZeroSectorChunks)]
fn test_zero_sector_chunks() {
    let payload = b"Chunk2DataAfterZeroSectorChunk";
    let mut data = vec![0u8; 64];
    data[..payload.len()].copy_from_slice(payload);
    let cursor = SharedStream::new(Cursor::new(data));

    let chunks = vec![
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::ZERO_FILL,
            comment: 0,
            start_sector: 0,
            sector_count: 0, // 0-length chunk
            compressed_offset: 0,
            compressed_length: 0,
        },
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::UNCOMPRESSED,
            comment: 0,
            start_sector: 0,
            sector_count: 1, // 64 bytes
            compressed_offset: 0,
            compressed_length: 64,
        },
    ];

    let mut stream = UdifPartitionReadStream::new(cursor, 64, &chunks, 1).unwrap();
    let mut buf = vec![0u8; 64];
    stream.read_exact(&mut buf).unwrap();
    expect_eq!(&buf[..payload.len()], payload);
}

#[gtest(RustUdifPartitionTest, ZeroBlockSizeRejected)]
fn test_zero_block_size_rejected() {
    let cursor = SharedStream::new(Cursor::new(vec![]));
    let chunks = vec![];
    let res = UdifPartitionReadStream::new(cursor, 0, &chunks, 0);
    expect_eq!(err_kind(&res), Some(ErrorKind::InvalidInput));
}

#[gtest(RustUdifPartitionTest, ArithmeticOverflowProtection)]
fn test_arithmetic_overflow_protection() {
    let cursor = SharedStream::new(Cursor::new(vec![]));
    let overflow_chunks = vec![UdifBlockChunk {
        chunk_type: UdifBlockChunkType::ZERO_FILL,
        comment: 0,
        start_sector: 0,
        sector_count: u64::MAX,
        compressed_offset: 0,
        compressed_length: 0,
    }];

    let res = UdifPartitionReadStream::new(cursor, 512, &overflow_chunks, u64::MAX);
    expect_eq!(err_kind(&res), Some(ErrorKind::InvalidData));
}

#[gtest(RustUdifPartitionTest, LargeUncompressedAndZerofillChunkExceeds128Mb)]
fn test_large_uncompressed_and_zerofill_chunk_exceeds_128mb() {
    let cursor = SharedStream::new(Cursor::new(vec![]));
    // 300,000 sectors * 512 bytes = 153,600,000 bytes > 128 MB
    let large_sector_count = 300_000u64;

    let zerofill_chunk = UdifBlockChunk {
        chunk_type: UdifBlockChunkType::ZERO_FILL,
        comment: 0,
        start_sector: 0,
        sector_count: large_sector_count,
        compressed_offset: 0,
        compressed_length: 0,
    };

    // ZeroFill chunks > 128 MB must succeed because they do not allocate
    // decompression buffers.
    let chunk_stream = UdifBlockChunkReadStream::new(cursor.clone(), 512, &zerofill_chunk);
    expect_true!(chunk_stream.is_ok());

    let chunks = vec![zerofill_chunk];
    let partition_stream =
        UdifPartitionReadStream::new(cursor.clone(), 512, &chunks, large_sector_count);
    expect_true!(partition_stream.is_ok());
    let mut stream = partition_stream.unwrap();

    let mut buf = [0x55u8; 1024];
    expect_eq!(stream.read(&mut buf).unwrap(), 1024);
    expect_true!(buf.iter().all(|&b| b == 0));

    // Seek to near the end (> 128 MB) and read zeros
    let target = (large_sector_count - 1) * 512;
    expect_eq!(stream.seek(SeekFrom::Start(target)).unwrap(), target);
    expect_eq!(stream.read(&mut buf).unwrap(), 512);
    expect_true!(buf[..512].iter().all(|&b| b == 0));
}

#[gtest(RustUdifPartitionTest, TrailingUnmappedGapReadAndSeekError)]
fn test_trailing_unmapped_gap_read_and_seek_error() {
    let payload = b"MappedChunk0____"; // 16 bytes
    let cursor = SharedStream::new(Cursor::new(payload.to_vec()));

    let chunks = vec![
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::UNCOMPRESSED,
            comment: 0,
            start_sector: 0,
            sector_count: 1, // 16 bytes
            compressed_offset: 0,
            compressed_length: 16,
        },
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::LAST_BLOCK,
            comment: 0,
            start_sector: 1,
            sector_count: 0,
            compressed_offset: 0,
            compressed_length: 0,
        },
    ];

    // Total partition has 3 sectors (48 bytes), but only sector 0 is mapped.
    // Sectors 1 and 2 are trailing unmapped gaps!
    let mut stream = UdifPartitionReadStream::new(cursor, 16, &chunks, 3).unwrap();
    expect_eq!(stream.seek(SeekFrom::End(0)).ok(), Some(48));
    expect_eq!(stream.seek(SeekFrom::Start(0)).ok(), Some(0));

    // Read chunk 0 completely
    let mut buf = [0u8; 16];
    stream.read_exact(&mut buf).unwrap();
    expect_eq!(&buf, payload);

    // Attempting to read into sector 1 (trailing unmapped gap) must fail
    // cleanly with InvalidData, NOT return Ok(0) (EOF)
    let mut gap_buf = [0u8; 1];
    expect_eq!(err_kind(&stream.read(&mut gap_buf)), Some(ErrorKind::InvalidData));

    // Seeking into sector 2 (offset 32) must fail with InvalidInput
    expect_eq!(err_kind(&stream.seek(SeekFrom::Start(32))), Some(ErrorKind::InvalidInput));

    // Seeking to exact EOF (offset 48) is valid
    expect_eq!(stream.seek(SeekFrom::End(0)).unwrap(), 48);
    // At exact EOF, read returns 0
    expect_eq!(stream.read(&mut gap_buf).unwrap(), 0);
}

#[gtest(RustUdifPartitionTest, LeadingUnmappedGapReadAndSeekError)]
fn test_leading_unmapped_gap_read_and_seek_error() {
    let payload = b"SectorTwoData___"; // 16 bytes
    let cursor = SharedStream::new(Cursor::new(payload.to_vec()));

    let chunks = vec![UdifBlockChunk {
        chunk_type: UdifBlockChunkType::UNCOMPRESSED,
        comment: 0,
        start_sector: 2, // Sectors 0 and 1 are unmapped!
        sector_count: 1,
        compressed_offset: 0,
        compressed_length: 16,
    }];

    let mut stream = UdifPartitionReadStream::new(cursor, 16, &chunks, 3).unwrap();

    // Reading at offset 0 (leading gap) must fail with InvalidData
    let mut buf = [0u8; 1];
    expect_eq!(err_kind(&stream.read(&mut buf)), Some(ErrorKind::InvalidData));

    // Seeking into sector 0 or 1 must fail with InvalidInput
    expect_true!(stream.seek(SeekFrom::Start(0)).is_err());
    expect_true!(stream.seek(SeekFrom::Start(16)).is_err());

    // Seeking to sector 2 (offset 32) succeeds
    expect_eq!(stream.seek(SeekFrom::Start(32)).unwrap(), 32);
    let mut chunk_buf = [0u8; 16];
    stream.read_exact(&mut chunk_buf).unwrap();
    expect_eq!(&chunk_buf, payload);
}

#[gtest(RustUdifPartitionTest, PartitionWithNoChunksAndNonZeroSectorsErrors)]
fn test_partition_with_no_chunks_and_nonzero_sectors_errors() {
    let cursor = SharedStream::new(Cursor::new(vec![]));
    // 2 sectors declared, but empty chunk table
    let mut stream = UdifPartitionReadStream::new(cursor.clone(), 16, &[], 2).unwrap();
    let mut buf = [0u8; 1];
    expect_eq!(err_kind(&stream.read(&mut buf)), Some(ErrorKind::InvalidData));
    expect_true!(stream.seek(SeekFrom::Start(0)).is_err());

    // Only LastBlock present, but 2 sectors declared
    let last_block_only = vec![UdifBlockChunk {
        chunk_type: UdifBlockChunkType::LAST_BLOCK,
        comment: 0,
        start_sector: 0,
        sector_count: 0,
        compressed_offset: 0,
        compressed_length: 0,
    }];
    let mut stream2 = UdifPartitionReadStream::new(cursor, 16, &last_block_only, 2).unwrap();
    expect_eq!(err_kind(&stream2.read(&mut buf)), Some(ErrorKind::InvalidData));
    expect_true!(stream2.seek(SeekFrom::Start(0)).is_err());
}

#[gtest(RustUdifPartitionTest, EmptyPartitionZeroSectors)]
fn test_empty_partition_zero_sectors() {
    let cursor = SharedStream::new(Cursor::new(vec![]));
    let mut stream = UdifPartitionReadStream::new(cursor, 512, &[], 0).unwrap();
    expect_eq!(stream.seek(SeekFrom::End(0)).ok(), Some(0));
    expect_eq!(stream.seek(SeekFrom::Start(0)).ok(), Some(0));

    let mut buf = [0u8; 10];
    expect_eq!(stream.read(&mut buf).unwrap(), 0);
    expect_eq!(stream.seek(SeekFrom::Start(0)).unwrap(), 0);
    expect_eq!(stream.seek(SeekFrom::End(0)).unwrap(), 0);
    expect_eq!(stream.stream_position().unwrap(), 0);
}

#[gtest(RustUdifPartitionTest, Non512BlockSizes)]
fn test_non_512_block_sizes() {
    // Test with 2048-byte CD/DVD sectors and 4096-byte Advanced Format sectors
    let payload2048 = vec![0x7au8; 2048];
    let cursor = SharedStream::new(Cursor::new(payload2048.clone()));

    let chunks = vec![
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::UNCOMPRESSED,
            comment: 0,
            start_sector: 0,
            sector_count: 1, // 2048 bytes
            compressed_offset: 0,
            compressed_length: 2048,
        },
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::ZERO_FILL,
            comment: 0,
            start_sector: 2, // Sector 1 is a 2048-byte gap!
            sector_count: 1, // 2048 bytes
            compressed_offset: 0,
            compressed_length: 0,
        },
    ];

    let mut stream = UdifPartitionReadStream::new(cursor, 2048, &chunks, 3).unwrap();
    expect_eq!(stream.seek(SeekFrom::End(0)).ok(), Some(6144));
    expect_eq!(stream.seek(SeekFrom::Start(0)).ok(), Some(0));

    let mut read_buf = vec![0u8; 2048];
    stream.read_exact(&mut read_buf).unwrap();
    expect_eq!(&read_buf, &payload2048);

    // Reading into sector 1 (unmapped 2048-byte gap) fails
    let mut single = [0u8; 1];
    expect_eq!(err_kind(&stream.read(&mut single)), Some(ErrorKind::InvalidData));

    // Seeking into sector 1 fails
    expect_true!(stream.seek(SeekFrom::Start(2048)).is_err());
    expect_true!(stream.seek(SeekFrom::Start(4095)).is_err());

    // Seeking to sector 2 (offset 4096) succeeds
    expect_eq!(stream.seek(SeekFrom::Start(4096)).unwrap(), 4096);
    let mut zero_buf = vec![0xffu8; 2048];
    stream.read_exact(&mut zero_buf).unwrap();
    expect_true!(zero_buf.iter().all(|&b| b == 0));
}

#[gtest(RustUdifPartitionTest, SeekTransactionalSafetyOnFailure)]
fn test_seek_transactional_safety_on_failure() {
    let payload = b"InitialChunkData"; // 16 bytes
    let cursor = SharedStream::new(Cursor::new(payload.to_vec()));

    let chunks = vec![
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::UNCOMPRESSED,
            comment: 0,
            start_sector: 0,
            sector_count: 1,
            compressed_offset: 0,
            compressed_length: 16,
        },
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::ZERO_FILL,
            comment: 0,
            start_sector: 2, // Sector 1 is an unmapped gap
            sector_count: 1,
            compressed_offset: 0,
            compressed_length: 0,
        },
    ];

    let mut stream = UdifPartitionReadStream::new(cursor, 16, &chunks, 3).unwrap();

    // Position at byte 5
    expect_eq!(stream.seek(SeekFrom::Start(5)).unwrap(), 5);

    // Attempt invalid seek into unmapped gap (sector 1 = byte 16)
    let err = stream.seek(SeekFrom::Start(16));
    expect_true!(err.is_err());

    // Original position must remain 5
    expect_eq!(stream.stream_position().unwrap(), 5);

    // Subsequent read must read from offset 5, NOT from anywhere else
    let mut buf = [0u8; 4];
    stream.read_exact(&mut buf).unwrap();
    expect_eq!(&buf, &payload[5..9]);
}

#[gtest(RustUdifPartitionTest, MultiChunkFourTypesSequentialRead)]
fn test_multi_chunk_four_types_sequential_read() {
    // 4 chunks: Uncompressed + ZeroFill + CompressZlib + CompressBz2
    let p_uncompressed = b"1111222233334444"; // 16 bytes
    let p_zlib = b"ZZZZYYYYXXXXWWWW"; // 16 bytes
    let p_bz2 = b"BBBBCCCCDDDDEEEE"; // 16 bytes

    let zlib_comp = compress_zlib(p_zlib);
    let bz2_comp = compress_bz2(p_bz2).unwrap();

    let mut backing = Vec::new();
    let off_uncomp = 0u64;
    backing.extend_from_slice(p_uncompressed);
    let off_zlib = backing.len() as u64;
    backing.extend_from_slice(&zlib_comp);
    let off_bz2 = backing.len() as u64;
    backing.extend_from_slice(&bz2_comp);

    let cursor = SharedStream::new(Cursor::new(backing));

    let chunks = vec![
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::UNCOMPRESSED,
            comment: 0,
            start_sector: 0,
            sector_count: 1, // 16 bytes
            compressed_offset: off_uncomp,
            compressed_length: 16,
        },
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::ZERO_FILL,
            comment: 0,
            start_sector: 1,
            sector_count: 1, // 16 bytes
            compressed_offset: 0,
            compressed_length: 0,
        },
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::COMPRESS_ZLIB,
            comment: 0,
            start_sector: 2,
            sector_count: 1, // 16 bytes
            compressed_offset: off_zlib,
            compressed_length: zlib_comp.len() as u64,
        },
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::COMPRESS_BZ2,
            comment: 0,
            start_sector: 3,
            sector_count: 1, // 16 bytes
            compressed_offset: off_bz2,
            compressed_length: bz2_comp.len() as u64,
        },
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::LAST_BLOCK,
            comment: 0,
            start_sector: 4,
            sector_count: 0,
            compressed_offset: 0,
            compressed_length: 0,
        },
    ];

    let mut stream = UdifPartitionReadStream::new(cursor, 16, &chunks, 4).unwrap();
    expect_eq!(stream.seek(SeekFrom::End(0)).ok(), Some(64));
    expect_eq!(stream.seek(SeekFrom::Start(0)).ok(), Some(0));

    // Read entire 64-byte partition spanning all 4 chunk types in a single
    // read_exact
    let mut full = [0u8; 64];
    stream.read_exact(&mut full).unwrap();

    expect_eq!(&full[0..16], p_uncompressed);
    expect_true!(full[16..32].iter().all(|&b| b == 0));
    expect_eq!(&full[32..48], p_zlib);
    expect_eq!(&full[48..64], p_bz2);

    // Intra-chunk and cross-chunk seeking across all 4 types
    // 1. Seeking in Uncompressed chunk
    expect_eq!(stream.seek(SeekFrom::Start(4)).unwrap(), 4);
    let mut b4 = [0u8; 4];
    stream.read_exact(&mut b4).unwrap();
    expect_eq!(&b4, &p_uncompressed[4..8]);

    // 2. Seeking across to ZeroFill chunk
    expect_eq!(stream.seek(SeekFrom::Start(20)).unwrap(), 20);
    stream.read_exact(&mut b4).unwrap();
    expect_true!(b4.iter().all(|&b| b == 0));

    // 3. Seeking into CompressZlib chunk
    expect_eq!(stream.seek(SeekFrom::Start(36)).unwrap(), 36);
    stream.read_exact(&mut b4).unwrap();
    expect_eq!(&b4, &p_zlib[4..8]);

    // 4. Seeking into CompressBz2 chunk
    expect_eq!(stream.seek(SeekFrom::Start(52)).unwrap(), 52);
    stream.read_exact(&mut b4).unwrap();
    expect_eq!(&b4, &p_bz2[4..8]);

    // 5. Seeking back to CompressZlib chunk (cache reload/eviction)
    expect_eq!(stream.seek(SeekFrom::Start(40)).unwrap(), 40);
    stream.read_exact(&mut b4).unwrap();
    expect_eq!(&b4, &p_zlib[8..12]);

    // Read byte by byte across all 4 chunk boundaries
    expect_eq!(stream.seek(SeekFrom::Start(0)).unwrap(), 0);
    let mut byte_by_byte = Vec::new();
    let mut one = [0u8; 1];
    while stream.read(&mut one).unwrap() > 0 {
        byte_by_byte.push(one[0]);
    }
    expect_eq!(&byte_by_byte[..], &full[..]);
}

#[gtest(RustUdifPartitionTest, PartialReadBeforeChunkCreationErrorReturnsBytes)]
fn test_partial_read_before_chunk_creation_error_returns_bytes() {
    // Chunk 0 is a valid 1-sector uncompressed chunk. Chunk 1 passes table
    // validation but fails UdifBlockChunkReadStream::new because its
    // decompressed size exceeds MAX_CHUNK_DECOMPRESSED_SIZE.
    let payload = vec![0xABu8; 512];
    let stream = SharedStream::new(Cursor::new(payload.clone()));
    let oversized_sectors = MAX_CHUNK_DECOMPRESSED_SIZE / 512 + 1;
    let chunks = vec![
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::UNCOMPRESSED,
            comment: 0,
            start_sector: 0,
            sector_count: 1,
            compressed_offset: 0,
            compressed_length: 512,
        },
        UdifBlockChunk {
            chunk_type: UdifBlockChunkType::COMPRESS_ZLIB,
            comment: 0,
            start_sector: 1,
            sector_count: oversized_sectors,
            compressed_offset: 0,
            compressed_length: 16,
        },
    ];
    let mut part =
        UdifPartitionReadStream::new(stream, 512, &chunks, 1 + oversized_sectors).unwrap();

    // The first read returns the bytes from chunk 0 instead of an error.
    let mut buf = vec![0u8; 1024];
    expect_eq!(part.read(&mut buf).unwrap(), 512);
    expect_eq!(&buf[..512], &payload[..]);
    expect_eq!(part.stream_position().unwrap(), 512);

    // The next read surfaces the error without moving the position.
    expect_eq!(err_kind(&part.read(&mut buf)), Some(ErrorKind::InvalidData));
    expect_eq!(part.stream_position().unwrap(), 512);
}

#[gtest(RustMachOImageReaderTest, EmptySliceFailsToParse)]
fn test_empty_mach_o_slice_fails_to_parse() {
    expect_eq!(MachOImage::parse(&[]).err(), Some(MachOError::EmptyImage));
}

#[gtest(RustMachOImageReaderTest, MagicValues)]
fn test_mach_o_magic_values() {
    expect_true!(MachOImage::is_mach_o_magic_value(MH_MAGIC));
    expect_true!(MachOImage::is_mach_o_magic_value(MH_CIGAM));
    expect_true!(MachOImage::is_mach_o_magic_value(MH_MAGIC_64));
    expect_true!(MachOImage::is_mach_o_magic_value(MH_CIGAM_64));
    expect_true!(MachOImage::is_mach_o_magic_value(FAT_MAGIC));
    expect_true!(MachOImage::is_mach_o_magic_value(FAT_CIGAM));
    expect_false!(MachOImage::is_mach_o_magic_value(0xdeadbeef));
}

fn push_ne_u32(buf: &mut Vec<u8>, val: u32) {
    buf.extend_from_slice(&val.to_ne_bytes());
}

fn build_thin_64_with_code_signature(sig_payload: &[u8]) -> Vec<u8> {
    let mut image = Vec::new();
    // mach_header_64 (32 bytes): magic, cputype, cpusubtype, filetype=2
    // (MH_EXECUTE), ncmds=1, sizeofcmds=16, flags=0, reserved=0.
    for val in [MH_MAGIC_64, 0x0100_0007, 3, 2, 1, 16, 0, 0] {
        push_ne_u32(&mut image, val);
    }
    // linkedit_data_command (16 bytes) at offset 32, pointing to signature at
    // offset 48.
    let sig_offset = (MACH_HEADER_64_SIZE + 16) as u32;
    for val in [LC_CODE_SIGNATURE, 16, sig_offset, sig_payload.len() as u32] {
        push_ne_u32(&mut image, val);
    }
    image.extend_from_slice(sig_payload);
    image
}

#[gtest(RustMachOImageReaderTest, Thin64WithCodeSignature)]
fn test_thin_64_with_code_signature() {
    let sig = b"fake-code-signature";
    let raw = build_thin_64_with_code_signature(sig);
    let parsed = MachOImage::parse(&raw).expect("valid 64-bit Mach-O should parse");

    expect_false!(parsed.is_fat());
    expect_true!(parsed.is_64_bit());
    expect_eq!(parsed.file_type(), 2);
    expect_eq!(parsed.mach_header_bytes().len(), MACH_HEADER_SIZE);
    expect_eq!(parsed.mach_header_64_bytes().len(), MACH_HEADER_64_SIZE);
    expect_eq!(parsed.load_commands().len(), 1);
    expect_eq!(parsed.load_commands()[0].cmd(), LC_CODE_SIGNATURE);
    expect_eq!(parsed.load_commands()[0].cmdsize(), 16);
    expect_eq!(parsed.code_signature(), Some(sig.as_slice()));
}

#[gtest(RustMachOImageReaderTest, ByteSwappedThin32AndSmallCmdsize)]
fn test_byte_swapped_thin_32_and_small_cmdsize() {
    let mut image = Vec::new();
    // mach_header (28 bytes) in swapped byte order (`MH_CIGAM`): ncmds = 2.
    push_ne_u32(&mut image, MH_CIGAM);
    for val in [7u32, 3, 6, 2, 16, 0] {
        push_ne_u32(&mut image, val.swap_bytes());
    }
    // Command 0: cmdsize = 5 (< sizeof(load_command) = 8), per https://crbug.com/40432691.
    // Command 1: normal 8-byte load_command immediately following the 8-byte
    // base struct.
    for val in [0x3u32, 5, 0x1, 8] {
        push_ne_u32(&mut image, val.swap_bytes());
    }

    let parsed = MachOImage::parse(&image).expect("byte-swapped 32-bit Mach-O should parse");
    expect_false!(parsed.is_fat());
    expect_false!(parsed.is_64_bit());
    expect_eq!(parsed.mach_header_64_bytes().len(), 0);
    expect_eq!(parsed.load_commands().len(), 2);
    expect_eq!(parsed.load_commands()[0].data.len(), 8);
    expect_eq!(parsed.load_commands()[0].cmdsize(), 5u32.swap_bytes());
    expect_eq!(parsed.load_commands()[1].cmd(), 0x1u32.swap_bytes());
}

fn build_single_arch_fat(magic: u32, arch_offset: u32, arch_size: u32, payload: &[u8]) -> Vec<u8> {
    let maybe_swap = |val: u32| if magic == FAT_CIGAM { val.swap_bytes() } else { val };
    let nfat_arch = 1;
    let cputype = 0;
    let cpusubtype = 0;
    let align = 0;

    let mut image = Vec::new();
    push_ne_u32(&mut image, magic);
    for val in [nfat_arch, cputype, cpusubtype, arch_offset, arch_size, align] {
        push_ne_u32(&mut image, maybe_swap(val));
    }
    image.extend_from_slice(payload);
    image
}

#[gtest(RustMachOImageReaderTest, FatBinary)]
fn test_fat_binary() {
    let thin64 = build_thin_64_with_code_signature(b"sig64");

    // Valid FAT_CIGAM archive with 1 architecture slice immediately after the
    // 28-byte header + fat_arch table.
    let arch_offset = (FAT_HEADER_SIZE + FAT_ARCH_SIZE) as u32;
    let fat = build_single_arch_fat(FAT_CIGAM, arch_offset, thin64.len() as u32, &thin64);

    let mut parsed_fat = MachOImage::parse(&fat).expect("valid fat Mach-O should parse");
    expect_true!(parsed_fat.is_fat());
    expect_false!(parsed_fat.is_64_bit());
    expect_eq!(parsed_fat.load_commands().len(), 0);
    expect_eq!(parsed_fat.code_signature(), None);
    expect_eq!(parsed_fat.fat_images().len(), 1);
    expect_true!(parsed_fat.fat_images()[0].is_64_bit());
    expect_eq!(parsed_fat.fat_images()[0].code_signature(), Some(b"sig64".as_slice()));
    expect_eq!(parsed_fat.take_fat_images().len(), 1);
    expect_true!(parsed_fat.is_fat());
    expect_eq!(parsed_fat.fat_images().len(), 0);
}

#[gtest(RustMachOImageReaderTest, FatBinaryAntiRecursionChecks)]
fn test_fat_binary_anti_recursion_checks() {
    let first_arch_offset = (FAT_HEADER_SIZE + FAT_ARCH_SIZE) as u32;

    // Backward fat_arch offset (< current table offset) must fail with
    // InvalidFatArch.
    let backward_offset = 4;
    let backward_size = first_arch_offset - backward_offset;
    let backward_fat = build_single_arch_fat(FAT_MAGIC, backward_offset, backward_size, &[]);
    expect_eq!(MachOImage::parse(&backward_fat).err(), Some(MachOError::InvalidFatArch));

    // Nested fat-in-fat at a forward offset (>= FAT_HEADER_SIZE +
    // FAT_ARCH_SIZE) must also fail with InvalidFatArch.
    let thin64 = build_thin_64_with_code_signature(b"sig64");
    let inner_fat =
        build_single_arch_fat(FAT_CIGAM, first_arch_offset, thin64.len() as u32, &thin64);
    let nested_fat =
        build_single_arch_fat(FAT_MAGIC, first_arch_offset, inner_fat.len() as u32, &inner_fat);
    expect_eq!(MachOImage::parse(&nested_fat).err(), Some(MachOError::InvalidFatArch));
}

#[gtest(RustMachOImageReaderTest, RejectsInvalidAndTruncatedInputs)]
fn test_rejects_invalid_and_truncated_inputs() {
    expect_eq!(MachOImage::parse(&[0xfe, 0xed]).err(), Some(MachOError::TruncatedHeader));
    expect_eq!(
        MachOImage::parse(&0xdeadbeefu32.to_ne_bytes()).err(),
        Some(MachOError::InvalidMagic)
    );
    expect_eq!(
        MachOImage::parse(&MH_MAGIC_64.to_ne_bytes()).err(),
        Some(MachOError::TruncatedHeader)
    );
    expect_eq!(
        MachOImage::parse(&FAT_MAGIC.to_ne_bytes()).err(),
        Some(MachOError::TruncatedHeader)
    );

    // Fat header claiming nfat_arch = 1 with no fat_arch entry following.
    let mut truncated_fat = Vec::new();
    for val in [FAT_MAGIC, 1] {
        push_ne_u32(&mut truncated_fat, val);
    }
    expect_eq!(MachOImage::parse(&truncated_fat).err(), Some(MachOError::InvalidFatArch));

    // 64-bit header claiming ncmds = 1 with no load command bytes following.
    let mut truncated_cmd = Vec::new();
    for val in [MH_MAGIC_64, 0, 0, 2, 1, 16, 0, 0] {
        push_ne_u32(&mut truncated_cmd, val);
    }
    expect_eq!(MachOImage::parse(&truncated_cmd).err(), Some(MachOError::TruncatedLoadCommand));

    // 64-bit header + 8-byte load_command header claiming cmdsize = 32, but
    // truncated before the remaining 24 command bytes.
    let mut truncated_full_cmd = truncated_cmd.clone();
    for val in [0x1u32, 32] {
        push_ne_u32(&mut truncated_full_cmd, val);
    }
    expect_eq!(
        MachOImage::parse(&truncated_full_cmd).err(),
        Some(MachOError::TruncatedLoadCommand)
    );

    // LC_CODE_SIGNATURE smaller than linkedit_data_command (16 bytes) or with
    // out-of-bounds dataoff/datasize still parses as a valid Mach-O image, but
    // yields `code_signature() == None`.
    let mut short_sig_cmd = truncated_cmd.clone();
    for val in [LC_CODE_SIGNATURE, 8] {
        push_ne_u32(&mut short_sig_cmd, val);
    }
    let parsed_short_sig = MachOImage::parse(&short_sig_cmd).expect("thin Mach-O should parse");
    expect_eq!(parsed_short_sig.code_signature(), None);

    let mut oob_sig_cmd = truncated_cmd.clone();
    for val in [LC_CODE_SIGNATURE, 16, 1024, 64] {
        push_ne_u32(&mut oob_sig_cmd, val);
    }
    let parsed_oob_sig = MachOImage::parse(&oob_sig_cmd).expect("thin Mach-O should parse");
    expect_eq!(parsed_oob_sig.code_signature(), None);
}
