// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;
use web_package_rust::{parse_magic_and_version, parse_trailing_length};

#[gtest(WebPackageRustTest, TestTrailingLength)]
fn test_trailing_length() {
    let data = 100u64.to_be_bytes();

    let res = parse_trailing_length(&data, 150);
    expect_true!(res.is_ok());
    expect_eq!(res.unwrap(), 50);

    // Boundary case: bundle_length == file_length (bundle offset is 0).
    let res = parse_trailing_length(&data, 100);
    expect_true!(res.is_ok());
    expect_eq!(res.unwrap(), 0);

    let res = parse_trailing_length(&data, 50);
    expect_true!(res.is_err());
    let err = res.unwrap_err();
    expect_eq!(err.message, "Invalid bundle length.");

    // A bundle cannot be empty: it always contains at least its own trailing
    // `length` field.
    let empty_bundle = 0u64.to_be_bytes();
    let res = parse_trailing_length(&empty_bundle, 150);
    expect_true!(res.is_err());
    let err = res.unwrap_err();
    expect_eq!(err.message, "Invalid bundle length.");

    // Boundary case: one byte short of the trailing `length` field itself.
    let too_short_bundle = 7u64.to_be_bytes();
    let res = parse_trailing_length(&too_short_bundle, 150);
    expect_true!(res.is_err());
    let err = res.unwrap_err();
    expect_eq!(err.message, "Invalid bundle length.");

    // Boundary case: a bundle consisting of exactly the trailing `length`
    // field is still accepted here; later parsing steps reject it.
    let minimal_bundle = 8u64.to_be_bytes();
    let res = parse_trailing_length(&minimal_bundle, 150);
    expect_true!(res.is_ok());
    expect_eq!(res.unwrap(), 142);

    let short_data = [0u8; 4];
    let res = parse_trailing_length(&short_data, 150);
    expect_true!(res.is_err());
    let err = res.unwrap_err();
    expect_eq!(err.message, "Error reading bundle length.");

    let long_data = [0u8; 12];
    let res = parse_trailing_length(&long_data, 150);
    expect_true!(res.is_err());
    let err = res.unwrap_err();
    expect_eq!(err.message, "Error reading bundle length.");
}

#[gtest(WebPackageRustTest, TestMagicAndVersionValid)]
fn test_magic_and_version_valid() {
    // Top-level array (5 items), magic bytes (8), version bytes (4),
    // section-lengths byte string (e.g. 100 bytes)
    let mut data = [0u8; 117];
    data[..17].copy_from_slice(&[
        0x85, // Array of 5
        0x48, 0xF0, 0x9F, 0x8C, 0x90, 0xF0, 0x9F, 0x93, 0xA6, // Magic
        0x44, 0x62, 0x32, 0x00, 0x00, // "b2\0\0"
        0x58, 0x64, // Byte string of length 100
    ]);

    let res = parse_magic_and_version(&data, 10).unwrap();
    expect_eq!(res.section_lengths_len, 100);
    // Consumed: 1 (array) + 9 (magic) + 5 (version) + 2 (byte string header 0x58
    // 0x64) = 17 bytes
    expect_eq!(res.next_read_offset, 27);
    expect_eq!(res.next_read_length, 109);
}

#[gtest(WebPackageRustTest, TestMagicAndVersionErrors)]
fn test_magic_and_version_errors() {
    // Empty data
    let err_empty = parse_magic_and_version(&[], 0).unwrap_err();
    expect_eq!(err_empty.message, "Missing CBOR array size byte.");
    expect_false!(err_empty.is_version_error);

    // Wrong array header
    // Non-array header
    let not_an_array = [0x00, 0x48, 0xF0, 0x9F, 0x8C, 0x90, 0xF0, 0x9F, 0x93, 0xA6];
    let err_not_array = parse_magic_and_version(&not_an_array, 0).unwrap_err();
    expect_eq!(err_not_array.message, "Wrong magic bytes.");
    expect_false!(err_not_array.is_version_error);

    // Wrong array header size (e.g. array of 4)
    let wrong_head = [0x84, 0x48, 0xF0, 0x9F, 0x8C, 0x90, 0xF0, 0x9F, 0x93, 0xA6];
    let err_wrong_head = parse_magic_and_version(&wrong_head, 0).unwrap_err();
    expect_eq!(err_wrong_head.message, "Wrong CBOR array size of the top-level structure");
    expect_false!(err_wrong_head.is_version_error);

    // Wrong magic bytes
    let wrong_magic = [
        0x85, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x62, 0x32, 0x00, 0x00,
        0x44,
    ];
    let err_wrong_magic = parse_magic_and_version(&wrong_magic, 0).unwrap_err();
    expect_eq!(err_wrong_magic.message, "Wrong magic bytes.");
    expect_false!(err_wrong_magic.is_version_error);

    // Version b1 (array of 6)
    let b1_data = [
        0x86, 0x48, 0xF0, 0x9F, 0x8C, 0x90, 0xF0, 0x9F, 0x93, 0xA6, 0x44, 0x62, 0x31, 0x00, 0x00,
        0x44,
    ];
    let err_b1 = parse_magic_and_version(&b1_data, 0).unwrap_err();
    expect_true!(err_b1.is_version_error);
    expect_eq!(
        err_b1.message,
        "Bundle format version is 'b1' which is no longer supported. Currently supported version is: 'b2'"
    );

    // An array of 6 is only tolerated to diagnose 'b1' bundles: the magic bytes
    // are still validated first.
    let b1_size_wrong_magic = [
        0x86, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x62, 0x31, 0x00, 0x00,
        0x44,
    ];
    let err_b1_size_wrong_magic = parse_magic_and_version(&b1_size_wrong_magic, 0).unwrap_err();
    expect_false!(err_b1_size_wrong_magic.is_version_error);
    expect_eq!(err_b1_size_wrong_magic.message, "Wrong magic bytes.");

    // An array of 6 holding a 'b2' bundle is not a 'b1' bundle, so it must be
    // reported as a malformed top-level array instead.
    let b1_size_b2_version = [
        0x86, 0x48, 0xF0, 0x9F, 0x8C, 0x90, 0xF0, 0x9F, 0x93, 0xA6, 0x44, 0x62, 0x32, 0x00, 0x00,
        0x58, 0x64,
    ];
    let err_b1_size_b2_version = parse_magic_and_version(&b1_size_b2_version, 0).unwrap_err();
    expect_false!(err_b1_size_b2_version.is_version_error);
    expect_eq!(err_b1_size_b2_version.message, "Wrong CBOR array size of the top-level structure");

    // Unknown version
    let unknown_ver = [
        0x85, 0x48, 0xF0, 0x9F, 0x8C, 0x90, 0xF0, 0x9F, 0x93, 0xA6, 0x44, 0x71, 0x32, 0x00, 0x00,
        0x44,
    ];
    let err_unknown = parse_magic_and_version(&unknown_ver, 0).unwrap_err();
    expect_true!(err_unknown.is_version_error);
    expect_eq!(
        err_unknown.message,
        "Version error: bundle format does not correspond to the specifed version. Currently supported version is: 'b2'"
    );

    // Top-level array count 7
    let array_count_7 = [
        0x87, 0x48, 0xF0, 0x9F, 0x8C, 0x90, 0xF0, 0x9F, 0x93, 0xA6, 0x44, 0x62, 0x32, 0x00, 0x00,
        0x58, 0x64,
    ];
    let err_count_7 = parse_magic_and_version(&array_count_7, 0).unwrap_err();
    expect_false!(err_count_7.is_version_error);
    expect_eq!(err_count_7.message, "Wrong CBOR array size of the top-level structure");

    // Section lengths >= 8192
    let large_section_lengths = [
        0x85, 0x48, 0xF0, 0x9F, 0x8C, 0x90, 0xF0, 0x9F, 0x93, 0xA6, 0x44, 0x62, 0x32, 0x00, 0x00,
        0x59, 0x20, 0x00,
    ];
    let err_large_sl = parse_magic_and_version(&large_section_lengths, 0).unwrap_err();
    expect_false!(err_large_sl.is_version_error);
    expect_eq!(err_large_sl.message, "The section-lengths CBOR must be smaller than 8192 bytes.");

    // Truncated section-lengths header
    let truncated_sl =
        [0x85, 0x48, 0xF0, 0x9F, 0x8C, 0x90, 0xF0, 0x9F, 0x93, 0xA6, 0x44, 0x62, 0x32, 0x00, 0x00];
    let err_truncated_sl = parse_magic_and_version(&truncated_sl, 0).unwrap_err();
    expect_false!(err_truncated_sl.is_version_error);
    expect_eq!(err_truncated_sl.message, "Cannot parse the size of section-lengths.");
}
