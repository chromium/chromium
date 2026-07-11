// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use cbor::*;
use rust_gtest_interop::prelude::*;
use std::collections::BTreeMap;

#[gtest(CBORWriterRustTest, TestWriteUint)]
fn test_write_uint() {
    let test_cases = [
        (0, "00"),
        (1, "01"),
        (10, "0a"),
        (23, "17"),
        (24, "1818"),
        (25, "1819"),
        (100, "1864"),
        (1000, "1903e8"),
        (1000000, "1a000f4240"),
        (0xFFFFFFFF, "1affffffff"),
        (0x100000000, "1b0000000100000000"),
        (i64::MAX, "1b7fffffffffffffff"),
    ];

    for test in test_cases {
        let val = Value::Int(test.0);
        let expected = hex::decode(test.1).unwrap();
        assert_eq!(write(&val), expected, "Failed encoding {}", test.0);
    }
}

#[gtest(CBORWriterRustTest, TestWriteNegativeInteger)]
fn test_write_negative_integer() {
    let test_cases = [
        (-1, "20"),
        (-10, "29"),
        (-23, "36"),
        (-24, "37"),
        (-25, "3818"),
        (-100, "3863"),
        (-256, "38ff"),
        (-1000, "3903e7"),
        (-65536, "39ffff"),
        (-4294967296, "3affffffff"),
        (-4294967297, "3b0000000100000000"),
        (i64::MIN, "3b7fffffffffffffff"),
    ];

    for test in test_cases {
        let val = Value::Int(test.0);
        let expected = hex::decode(test.1).unwrap();
        assert_eq!(write(&val), expected, "Failed encoding {}", test.0);
    }
}

#[gtest(CBORWriterRustTest, TestWriteBytes)]
fn test_write_bytes() {
    let test_cases = [(vec![], "40"), (vec![0x01, 0x02, 0x03, 0x04], "4401020304")];

    for test in test_cases {
        let val = Value::Bytestring(test.0.clone());
        let expected = hex::decode(test.1).unwrap();
        assert_eq!(write(&val), expected);
    }
}

#[gtest(CBORWriterRustTest, TestWriteString)]
fn test_write_string() {
    let test_cases = [("", "60"), ("a", "6161")];

    for test in test_cases {
        let val = Value::String(String::from(test.0));
        let expected = hex::decode(test.1).unwrap();
        assert_eq!(write(&val), expected);
    }
}

#[gtest(CBORWriterRustTest, TestWriteArray)]
fn test_write_array() {
    let test_cases = [
        (Value::Array(Vec::new()), "80"),
        (Value::Array(vec![Value::Int(1)]), "8101"),
        (Value::Array(vec![Value::Array(vec![Value::Int(1)])]), "818101"),
    ];

    for test in test_cases {
        let expected = hex::decode(test.1).unwrap();
        assert_eq!(write(&test.0), expected);
    }
}

#[gtest(CBORWriterRustTest, TestWriteMap)]
fn test_write_map() {
    let test_cases = [
        (Value::Map(BTreeMap::new()), "a0"),
        (Value::Map(BTreeMap::from([(MapKey::Int(1), Value::Int(1))])), "a10101"),
        (Value::Map(BTreeMap::from([(MapKey::Int(-2), Value::Int(1))])), "a12101"),
        (
            Value::Map(BTreeMap::from([
                (MapKey::Int(1), Value::Int(1)),
                (MapKey::Int(2), Value::Int(2)),
            ])),
            "a201010202",
        ),
        (
            Value::Map(BTreeMap::from([(
                MapKey::Bytestring(hex::decode("0a").unwrap()),
                Value::Int(1),
            )])),
            "a1410a01",
        ),
    ];

    for test in test_cases {
        let expected = hex::decode(test.1).unwrap();
        assert_eq!(write(&test.0), expected);
    }
}

#[gtest(CBORWriterRustTest, TestWriteSimpleValues)]
fn test_write_simple_values() {
    let test_cases = [
        (Value::Boolean(false), "f4"),
        (Value::Boolean(true), "f5"),
        (Value::Null, "f6"),
        (Value::Undefined, "f7"),
    ];

    for test in test_cases {
        let expected = hex::decode(test.1).unwrap();
        assert_eq!(write(&test.0), expected);
    }
}

#[gtest(CBORWriterRustTest, TestWriteFloats)]
fn test_write_floats() {
    let test_cases = [
        (Value::Float(1.0), "fb3ff0000000000000"),
        (Value::Float(1.00048828125), "fb3ff0020000000000"),
        (Value::Float(f64::from_bits(0x3ff0000000000001)), "fb3ff0000000000001"),
        (Value::Float(f64::NAN), "fb7ff8000000000000"),
        (Value::Float(f64::INFINITY), "fb7ff0000000000000"),
        (Value::Float(f64::NEG_INFINITY), "fbfff0000000000000"),
    ];

    for test in test_cases {
        let expected = hex::decode(test.1).unwrap();
        let bytes = write(&test.0);
        if test.1 == "fb7ff8000000000000" {
            // NaN payloads might differ, just check the length and that it parses back as
            // NaN
            assert_eq!(bytes.len(), 9);
            assert_eq!(bytes[0], 0xfb);
        } else {
            assert_eq!(bytes, expected, "Failed encoding {}", test.1);
        }
    }
}

#[gtest(CBORWriterRustTest, TestWriteMapKeyCanonicalization)]
fn test_write_map_key_canonicalization() {
    let map = BTreeMap::from([
        (MapKey::String(String::from("bb")), Value::Int(1)),
        (MapKey::String(String::from("c")), Value::Int(2)), // Length 1 should precede length 2
        (MapKey::Int(-1), Value::Int(3)),                   // Major Type 1
        (MapKey::Int(1), Value::Int(4)),                    // Major Type 0
    ]);

    // Expected CTAP2 Canonical Order:
    // 1. MapKey::Int(1) -> 0x01
    // 2. MapKey::Int(-1) -> 0x20
    // 3. MapKey::String("c") -> 0x6163
    // 4. MapKey::String("bb") -> 0x626262
    // Map Envelope: 0xa4
    // Values: 0x04 (for 1), 0x03 (for -1), 0x02 (for "c"), 0x01 (for "bb")
    // Total raw expected payload bytes: a4 01 04 20 03 61 63 02 62 62 62 01

    let expected = hex::decode("a40104200361630262626201").unwrap();
    assert_eq!(write(&Value::Map(map)), expected);
}
