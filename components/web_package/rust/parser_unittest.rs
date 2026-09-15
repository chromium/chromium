// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;
use web_package_rust::*;

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
