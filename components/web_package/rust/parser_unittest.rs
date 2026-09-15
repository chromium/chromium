// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use cbor::{Map, MapEntry, MapKey, Value};
use rust_gtest_interop::prelude::*;
use web_package_rust::{
    parse_bundle_header, parse_critical_section, parse_index_section, parse_magic_and_version,
    parse_primary_section, parse_response, parse_trailing_length,
};

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

#[gtest(WebPackageRustTest, TestBundleHeader)]
fn test_bundle_header() {
    // section-lengths: ["primary", 50, "index", 100, "responses", 200]
    let section_lengths_cbor = cbor::write(&Value::Array(vec![
        Value::String("primary"),
        Value::Int(50),
        Value::String("index"),
        Value::Int(100),
        Value::String("responses"),
        Value::Int(200),
    ]));

    let mut data = section_lengths_cbor.clone();
    // Sections array header (array of 3 elements)
    data.push(0x83);

    let res = parse_bundle_header(&data, section_lengths_cbor.len() as u64, 1000);
    expect_true!(res.is_ok());
    let res = res.unwrap();
    expect_eq!(res.metadata_sections.len(), 2usize);
    // sections_start = 1000 + section_lengths_cbor.len() + 1
    let start = 1000 + section_lengths_cbor.len() as u64 + 1;
    expect_eq!(res.metadata_sections[0].name, "primary");
    expect_eq!(res.metadata_sections[0].offset, start);
    expect_eq!(res.metadata_sections[0].length, 50);

    expect_eq!(res.metadata_sections[1].name, "index");
    expect_eq!(res.metadata_sections[1].offset, start + 50);
    expect_eq!(res.metadata_sections[1].length, 100);

    expect_eq!(res.responses_offset, start + 150);
    expect_eq!(res.responses_length, 200);
}

#[gtest(WebPackageRustTest, TestBundleHeaderErrors)]
fn test_bundle_header_errors() {
    // 1. Truncated section-lengths data.
    let data = [0u8; 10];
    let res = parse_bundle_header(&data, 20, 0);
    expect_true!(res.is_err());
    let err = res.unwrap_err();
    expect_eq!(err.message, "Cannot read section-lengths.");
    expect_false!(err.is_version_error);

    // 32-bit truncation / overflow protection in section_lengths_len.
    let res_overflow = parse_bundle_header(&data, u64::MAX, 0);
    expect_true!(res_overflow.is_err());
    expect_eq!(res_overflow.unwrap_err().message, "Cannot read section-lengths.");

    // 2. Responses section not last.
    // Case 2a: responses is not the last section (e.g. index follows responses).
    let not_last_cbor = cbor::write(&Value::Array(vec![
        Value::String("responses"),
        Value::Int(100),
        Value::String("index"),
        Value::Int(200),
    ]));
    let mut not_last_data = not_last_cbor.clone();
    not_last_data.push(0x82); // Array of 2 sections
    let res_not_last = parse_bundle_header(&not_last_data, not_last_cbor.len() as u64, 0);
    expect_true!(res_not_last.is_err());
    let err_not_last = res_not_last.unwrap_err();
    expect_eq!(err_not_last.message, "Responses section is not the last in section-lengths.");
    expect_false!(err_not_last.is_version_error);

    // Case 2b: missing responses entirely.
    let missing_resp_cbor =
        cbor::write(&Value::Array(vec![Value::String("index"), Value::Int(100)]));
    let mut missing_resp_data = missing_resp_cbor.clone();
    missing_resp_data.push(0x81); // Array of 1 section
    let res_missing = parse_bundle_header(&missing_resp_data, missing_resp_cbor.len() as u64, 0);
    expect_true!(res_missing.is_err());
    let err_missing = res_missing.unwrap_err();
    expect_eq!(err_missing.message, "Responses section is not the last in section-lengths.");
    expect_false!(err_missing.is_version_error);

    // 3. Duplicated section names.
    let dup_cbor = cbor::write(&Value::Array(vec![
        Value::String("index"),
        Value::Int(50),
        Value::String("index"),
        Value::Int(100),
        Value::String("responses"),
        Value::Int(200),
    ]));
    let mut dup_data = dup_cbor.clone();
    dup_data.push(0x83); // Array of 3 sections
    let res_dup = parse_bundle_header(&dup_data, dup_cbor.len() as u64, 0);
    expect_true!(res_dup.is_err());
    let err_dup = res_dup.unwrap_err();
    expect_eq!(err_dup.message, "Duplicated section.");
    expect_false!(err_dup.is_version_error);

    // Negative section length
    let neg_len_cbor = cbor::write(&Value::Array(vec![
        Value::String("index"),
        Value::Int(-1),
        Value::String("responses"),
        Value::Int(100),
    ]));
    let mut neg_len_data = neg_len_cbor.clone();
    neg_len_data.push(0x82);
    let res_neg = parse_bundle_header(&neg_len_data, neg_len_cbor.len() as u64, 0);
    expect_true!(res_neg.is_err());
    expect_eq!(res_neg.unwrap_err().message, "Cannot parse section-lengths.");
}

#[gtest(WebPackageRustTest, TestIndexSection)]
fn test_index_section() {
    let index_cbor = cbor::write(&Value::Map(Map::from(vec![
        MapEntry::from((
            MapKey::String("https://example.com/a"),
            Value::Array(vec![Value::Int(0), Value::Int(50)]),
        )),
        MapEntry::from((
            MapKey::String("https://example.com/b"),
            Value::Array(vec![Value::Int(50), Value::Int(150)]),
        )),
    ])));

    let entries = parse_index_section(&index_cbor, 2000, 200).unwrap();
    expect_eq!(entries.len(), 2usize);
    expect_eq!(entries[0].url, "https://example.com/a");
    expect_eq!(entries[0].offset, 2000);
    expect_eq!(entries[0].length, 50);

    expect_eq!(entries[1].url, "https://example.com/b");
    expect_eq!(entries[1].offset, 2050);
    expect_eq!(entries[1].length, 150);

    // Response out of range
    let invalid_cbor = cbor::write(&Value::Map(Map::from(vec![MapEntry::from((
        MapKey::String("https://example.com/c"),
        Value::Array(vec![Value::Int(100), Value::Int(150)]),
    ))])));
    let err = parse_index_section(&invalid_cbor, 2000, 200).unwrap_err();
    expect_eq!(err.message, "Index section: response out of range.");

    // Not a map
    let not_a_map = cbor::write(&Value::Array(vec![Value::Int(1)]));
    let err = parse_index_section(&not_a_map, 0, 100).unwrap_err();
    expect_eq!(err.message, "Index section must be a map.");

    // Value not an array
    let val_not_array = cbor::write(&Value::Map(Map::from(vec![MapEntry::from((
        MapKey::String("https://example.com/a"),
        Value::Int(1),
    ))])));
    let err = parse_index_section(&val_not_array, 0, 100).unwrap_err();
    expect_eq!(err.message, "Index section: value must be an array.");

    // Array size != 2
    let array_len_3 = cbor::write(&Value::Map(Map::from(vec![MapEntry::from((
        MapKey::String("https://example.com/a"),
        Value::Array(vec![Value::Int(0), Value::Int(50), Value::Int(100)]),
    ))])));
    let err = parse_index_section(&array_len_3, 0, 100).unwrap_err();
    expect_eq!(
        err.message,
        "Index section: the size of a response array per URL should be exactly 2."
    );

    // Negative offset
    let neg_offset = cbor::write(&Value::Map(Map::from(vec![MapEntry::from((
        MapKey::String("https://example.com/a"),
        Value::Array(vec![Value::Int(-1), Value::Int(50)]),
    ))])));
    let err = parse_index_section(&neg_offset, 0, 100).unwrap_err();
    expect_eq!(err.message, "Index section: offset and length values must be unsigned.");

    // Non-string key
    let non_str_key = cbor::write(&Value::Map(Map::from(vec![MapEntry::from((
        MapKey::Int(42),
        Value::Array(vec![Value::Int(0), Value::Int(50)]),
    ))])));
    let err = parse_index_section(&non_str_key, 0, 100).unwrap_err();
    expect_eq!(err.message, "Index section: key must be a string.");
}

#[gtest(WebPackageRustTest, TestCriticalSection)]
fn test_critical_section() {
    let valid_cbor =
        cbor::write(&Value::Array(vec![Value::String("index"), Value::String("responses")]));
    expect_true!(parse_critical_section(&valid_cbor).is_ok());

    let invalid_cbor = cbor::write(&Value::Array(vec![Value::String("unknown_sec")]));
    let err = parse_critical_section(&invalid_cbor).unwrap_err();
    expect_eq!(err.message, "Unknown critical section.");
}

#[gtest(WebPackageRustTest, TestCriticalSectionErrors)]
fn test_critical_section_errors() {
    let not_an_array = cbor::write(&Value::Int(42));
    let err = parse_critical_section(&not_an_array).unwrap_err();
    expect_eq!(err.message, "Critical section must be an array.");

    let non_string_elem = cbor::write(&Value::Array(vec![Value::Int(42)]));
    let err = parse_critical_section(&non_string_elem).unwrap_err();
    expect_eq!(err.message, "Non-string element in the critical section.");
}

#[gtest(WebPackageRustTest, TestPrimarySection)]
fn test_primary_section() {
    let valid_cbor = cbor::write(&Value::String("https://example.com/primary"));
    let url = parse_primary_section(&valid_cbor).unwrap();
    expect_eq!(url, "https://example.com/primary");

    let invalid_cbor = cbor::write(&Value::Int(42));
    let err = parse_primary_section(&invalid_cbor).unwrap_err();
    expect_eq!(err.message, "Primary section must be a string.");
}

#[gtest(WebPackageRustTest, TestResponseParsing)]
fn test_response_parsing() {
    let headers_cbor = cbor::write(&Value::Map(Map::from(vec![
        MapEntry::from((MapKey::Bytestring(b":status"), Value::Bytestring(b"200"))),
        MapEntry::from((MapKey::Bytestring(b"content-type"), Value::Bytestring(b"text/plain"))),
    ])));

    // response: [ headers: bstr, payload: bstr ]
    let response_cbor = cbor::write(&Value::Array(vec![
        Value::Bytestring(&headers_cbor),
        Value::Bytestring(b"Hello World"),
    ]));

    let res = parse_response(&response_cbor, 5000, response_cbor.len() as u64).unwrap();
    expect_false!(res.needs_more_data);
    expect_eq!(res.response_code, 200);
    expect_eq!(res.headers.len(), 1usize);
    expect_eq!(res.headers[0].name, b"content-type");
    expect_eq!(res.headers[0].value, b"text/plain");
    expect_eq!(res.payload_length, 11);
    expect_eq!(res.payload_offset, 5000 + (response_cbor.len() as u64 - 11));
}

/// RFC 9110 Section 5.5 (https://www.rfc-editor.org/rfc/rfc9110.html#section-5.5)
/// allows `obs-text` (`%x80-FF`) in a field value and directs recipients to
/// treat those octets as opaque data, so a value that is not valid UTF-8 must
/// still be accepted.
#[gtest(WebPackageRustTest, TestResponseNonUtf8HeaderValue)]
fn test_response_non_utf8_header_value() {
    // "café.pdf" encoded as ISO-8859-1, which is not valid UTF-8.
    let latin1_value = b"attachment; filename=\"caf\xe9.pdf\"";
    let headers_cbor = cbor::write(&Value::Map(Map::from(vec![
        MapEntry::from((MapKey::Bytestring(b":status"), Value::Bytestring(b"200"))),
        MapEntry::from((
            MapKey::Bytestring(b"content-disposition"),
            Value::Bytestring(latin1_value),
        )),
        MapEntry::from((MapKey::Bytestring(b"content-type"), Value::Bytestring(b"text/plain"))),
    ])));

    let response_cbor = cbor::write(&Value::Array(vec![
        Value::Bytestring(&headers_cbor),
        Value::Bytestring(b"Hello World"),
    ]));

    let res = parse_response(&response_cbor, 0, response_cbor.len() as u64).unwrap();
    expect_eq!(res.response_code, 200);
    expect_eq!(res.headers.len(), 2usize);
    // CBOR map keys are ordered by length first, then bytewise, so the shorter
    // "content-type" key precedes "content-disposition".
    expect_eq!(res.headers[0].name, b"content-type");
    expect_eq!(res.headers[0].value, b"text/plain");
    expect_eq!(res.headers[1].name, b"content-disposition");
    expect_eq!(res.headers[1].value, latin1_value);
}

#[gtest(WebPackageRustTest, TestResponseMissingStatus)]
fn test_response_missing_status() {
    let headers_cbor = cbor::write(&Value::Map(Map::from(vec![MapEntry::from((
        MapKey::Bytestring(b"content-type"),
        Value::Bytestring(b"text/plain"),
    ))])));

    let response_cbor = cbor::write(&Value::Array(vec![
        Value::Bytestring(&headers_cbor),
        Value::Bytestring(b"payload"),
    ]));

    let err = parse_response(&response_cbor, 0, response_cbor.len() as u64).unwrap_err();
    expect_eq!(err.message, "Response headers map must have exactly one pseudo-header, :status.");
}

#[gtest(WebPackageRustTest, TestResponseExtraPseudoHeader)]
fn test_response_extra_pseudo_header() {
    for extra_pseudo in
        [b":path".as_slice(), b":method".as_slice(), b":foo".as_slice(), b":Status".as_slice()]
    {
        let headers_cbor = cbor::write(&Value::Map(Map::from(vec![
            MapEntry::from((MapKey::Bytestring(b":status"), Value::Bytestring(b"200"))),
            MapEntry::from((MapKey::Bytestring(extra_pseudo), Value::Bytestring(b""))),
            MapEntry::from((MapKey::Bytestring(b"content-type"), Value::Bytestring(b"text/plain"))),
        ])));

        let response_cbor = cbor::write(&Value::Array(vec![
            Value::Bytestring(&headers_cbor),
            Value::Bytestring(b"payload"),
        ]));

        let err = parse_response(&response_cbor, 0, response_cbor.len() as u64).unwrap_err();
        expect_eq!(
            err.message,
            "Response headers map must have exactly one pseudo-header, :status."
        );
    }
}

#[gtest(WebPackageRustTest, TestResponseNeedsMoreData)]
fn test_response_needs_more_data() {
    let headers_cbor = cbor::write(&Value::Map(Map::from(vec![
        MapEntry::from((MapKey::Bytestring(b":status"), Value::Bytestring(b"200"))),
        MapEntry::from((MapKey::Bytestring(b"content-type"), Value::Bytestring(b"text/plain"))),
    ])));

    let response_cbor = cbor::write(&Value::Array(vec![
        Value::Bytestring(&headers_cbor),
        Value::Bytestring(b"payload"),
    ]));

    // Pass only a truncated prefix (e.g. 5 bytes)
    let truncated = &response_cbor[..5];
    let res = parse_response(truncated, 0, response_cbor.len() as u64).unwrap();
    expect_true!(res.needs_more_data);
    expect_true!(res.required_buffer_size > 5);
}

#[gtest(WebPackageRustTest, TestResponseEmptyPayloadWithoutContentType)]
fn test_response_empty_payload_without_content_type() {
    let headers_cbor = cbor::write(&Value::Map(Map::from(vec![MapEntry::from((
        MapKey::Bytestring(b":status"),
        Value::Bytestring(b"204"),
    ))])));

    let response_cbor =
        cbor::write(&Value::Array(vec![Value::Bytestring(&headers_cbor), Value::Bytestring(b"")]));

    let res = parse_response(&response_cbor, 5000, response_cbor.len() as u64).unwrap();
    expect_false!(res.needs_more_data);
    expect_eq!(res.response_code, 204);
    expect_eq!(res.headers.len(), 0usize);
    expect_eq!(res.payload_length, 0);
    expect_eq!(res.payload_offset, 5000 + response_cbor.len() as u64);
}

#[gtest(WebPackageRustTest, TestResponseNonEmptyPayloadWithoutContentType)]
fn test_response_non_empty_payload_without_content_type() {
    let headers_cbor = cbor::write(&Value::Map(Map::from(vec![MapEntry::from((
        MapKey::Bytestring(b":status"),
        Value::Bytestring(b"200"),
    ))])));

    let response_cbor = cbor::write(&Value::Array(vec![
        Value::Bytestring(&headers_cbor),
        Value::Bytestring(b"some non-empty payload"),
    ]));

    let err = parse_response(&response_cbor, 0, response_cbor.len() as u64).unwrap_err();
    expect_eq!(err.message, "Non-empty response must have a content-type header.");
}

#[gtest(WebPackageRustTest, TestResponseHeaderTooBig)]
fn test_response_header_too_big() {
    // CBOR: Array of 2 elements (0x82), followed by a byte string of length 512KB
    // (524288 bytes). In CBOR: 0x5a followed by 4 bytes: 0x00, 0x08, 0x00,
    // 0x00.
    let response_cbor = [0x82, 0x5A, 0x00, 0x08, 0x00, 0x00];
    let err = parse_response(&response_cbor, 0, 1_000_000).unwrap_err();
    expect_eq!(err.message, "Response header is too big.");
}

#[gtest(WebPackageRustTest, TestResponseUppercaseHeaderName)]
fn test_response_uppercase_header_name() {
    let headers_cbor = cbor::write(&Value::Map(Map::from(vec![
        MapEntry::from((MapKey::Bytestring(b":status"), Value::Bytestring(b"200"))),
        MapEntry::from((MapKey::Bytestring(b"Content-Type"), Value::Bytestring(b"text/plain"))),
    ])));

    let response_cbor = cbor::write(&Value::Array(vec![
        Value::Bytestring(&headers_cbor),
        Value::Bytestring(b"Hello"),
    ]));

    let err = parse_response(&response_cbor, 0, response_cbor.len() as u64).unwrap_err();
    expect_eq!(err.message, "Cannot parse response headers.");
}

#[gtest(WebPackageRustTest, TestResponseDisallowedHeaderValueChars)]
fn test_response_disallowed_header_value_chars() {
    for invalid_val in
        [b"text/\0plain".as_slice(), b"text/\rplain".as_slice(), b"text/\nplain".as_slice()]
    {
        let headers_cbor = cbor::write(&Value::Map(Map::from(vec![
            MapEntry::from((MapKey::Bytestring(b":status"), Value::Bytestring(b"200"))),
            MapEntry::from((MapKey::Bytestring(b"content-type"), Value::Bytestring(invalid_val))),
        ])));

        let response_cbor = cbor::write(&Value::Array(vec![
            Value::Bytestring(&headers_cbor),
            Value::Bytestring(b"Hello"),
        ]));

        let err = parse_response(&response_cbor, 0, response_cbor.len() as u64).unwrap_err();
        expect_eq!(err.message, "Cannot parse response headers.");
    }
}

#[gtest(WebPackageRustTest, TestResponsePayloadLengthMismatchAndOverflow)]
fn test_response_payload_length_mismatch_and_overflow() {
    let headers_cbor = cbor::write(&Value::Map(Map::from(vec![
        MapEntry::from((MapKey::Bytestring(b":status"), Value::Bytestring(b"200"))),
        MapEntry::from((MapKey::Bytestring(b"content-type"), Value::Bytestring(b"text/plain"))),
    ])));

    let response_cbor = cbor::write(&Value::Array(vec![
        Value::Bytestring(&headers_cbor),
        Value::Bytestring(b"Hello World"),
    ]));

    // Mismatch: response_length greater than actual length.
    let err = parse_response(&response_cbor, 0, (response_cbor.len() as u64) + 1).unwrap_err();
    expect_eq!(err.message, "Unexpected payload length.");

    // Mismatch: response_length smaller than actual length.
    let err = parse_response(&response_cbor, 0, (response_cbor.len() as u64) - 1).unwrap_err();
    expect_eq!(err.message, "Unexpected payload length.");

    // Overflow in payload_offset calculation: response_offset +
    // consumed_header_bytes overflows.
    let err = parse_response(&response_cbor, u64::MAX, response_cbor.len() as u64).unwrap_err();
    expect_eq!(err.message, "Unexpected payload length.");

    // Overflow in total_len calculation: consumed_header_bytes + payload_len
    // overflows. Construct response with valid headers bytestring followed by
    // 0x5B (8-byte length) with u64::MAX.
    let mut overflow_cbor = Vec::new();
    overflow_cbor.push(0x82); // Array of 2
    let headers_bstr = cbor::write(&Value::Bytestring(&headers_cbor));
    overflow_cbor.extend_from_slice(&headers_bstr);
    // Payload byte string with u64::MAX length: 0x5B followed by 8 0xFF bytes.
    overflow_cbor.push(0x5B);
    overflow_cbor.extend_from_slice(&[0xFF; 8]);
    let err = parse_response(&overflow_cbor, 0, overflow_cbor.len() as u64).unwrap_err();
    expect_eq!(err.message, "Unexpected payload length.");
}

#[gtest(WebPackageRustTest, TestResponseArraySizeNotTwo)]
fn test_response_array_size_not_two() {
    let resp_3 = cbor::write(&Value::Array(vec![
        Value::Bytestring(b""),
        Value::Bytestring(b""),
        Value::Bytestring(b""),
    ]));
    let err = parse_response(&resp_3, 0, resp_3.len() as u64).unwrap_err();
    expect_eq!(err.message, "Array size of response must be 2.");

    let resp_1 = cbor::write(&Value::Array(vec![Value::Bytestring(b"")]));
    let err = parse_response(&resp_1, 0, resp_1.len() as u64).unwrap_err();
    expect_eq!(err.message, "Array size of response must be 2.");
}

#[gtest(WebPackageRustTest, TestResponseStatusInvalidDigits)]
fn test_response_status_invalid_digits() {
    for bad_status in [b"20".as_slice(), b"2000".as_slice(), b"20a".as_slice()] {
        let headers_cbor = cbor::write(&Value::Map(Map::from(vec![
            MapEntry::from((MapKey::Bytestring(b":status"), Value::Bytestring(bad_status))),
            MapEntry::from((MapKey::Bytestring(b"content-type"), Value::Bytestring(b"text/plain"))),
        ])));
        let response_cbor = cbor::write(&Value::Array(vec![
            Value::Bytestring(&headers_cbor),
            Value::Bytestring(b"payload"),
        ]));
        let err = parse_response(&response_cbor, 0, response_cbor.len() as u64).unwrap_err();
        expect_eq!(err.message, ":status must be 3 ASCII decimal digits.");
    }
}
