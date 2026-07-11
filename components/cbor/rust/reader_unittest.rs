// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use cbor::*;
use rust_gtest_interop::prelude::*;
use std::collections::BTreeMap;

fn parse_bytes(input: &[u8]) -> Result<Value, Error> {
    let len = input.len();
    let (val, consumed) = parse_with_config(input, Config::default())?;
    if consumed < len {
        Err(Error::TrailingData(len - consumed))
    } else {
        Ok(val)
    }
}

#[gtest(CBORReaderRustTest, TestInputs)]
fn test_inputs() {
    let test_cases = [
        ("", Err(Error::InputTruncated)),
        ("d0", Err(Error::UnsupportedMajorType(0, 6))),
        ("1f", Err(Error::UnsupportedAdditionalInformation(0, 31))),
        ("01", Ok(Value::Int(1))),
        ("0100", Err(Error::TrailingData(1))),
        ("20", Ok(Value::Int(-1))),
        ("182a", Ok(Value::Int(42))),
        ("191388", Ok(Value::Int(5000))),
        ("1a02faf080", Ok(Value::Int(50000000))),
        ("1b000000746a528800", Ok(Value::Int(500000000000))),
        ("1bffffffffffffffff", Err(Error::UnsignedOutOfRange(0xffffffffffffffff))),
        ("3bffffffffffffffff", Err(Error::NegativeOutOfRange(0xffffffffffffffff))),
        ("1818", Ok(Value::Int(24))),
        ("1817", Err(Error::NonMinimalAdditionalData(0))),
        ("190080", Err(Error::NonMinimalAdditionalData(0))),
        ("60", Ok(Value::String(String::from("")))),
        ("6161", Ok(Value::String(String::from("a")))),
        ("40", Ok(Value::Bytestring(vec![]))),
        ("4100", Ok(Value::Bytestring(vec![0x00]))),
        ("61ff", Err(Error::InvalidUTF8(1))),
        ("80", Ok(Value::Array(Vec::new()))),
        ("8101", Ok(Value::Array(vec![Value::Int(1)]))),
        ("818101", Ok(Value::Array(vec![Value::Array(vec![Value::Int(1)])]))),
        (
            "8181818181818181818181818181818181818180",
            Err(Error::DepthLimitExceeded(3, MAX_DEPTH)),
        ),
        ("a0", Ok(Value::Map(BTreeMap::new()))),
        ("a10101", Ok(Value::Map(BTreeMap::from([(MapKey::Int(1), Value::Int(1))])))),
        ("a12101", Ok(Value::Map(BTreeMap::from([(MapKey::Int(-2), Value::Int(1))])))),
        (
            "a201010202",
            Ok(Value::Map(BTreeMap::from([
                (MapKey::Int(1), Value::Int(1)),
                (MapKey::Int(2), Value::Int(2)),
            ]))),
        ),
        (
            "a1410a01",
            Ok(Value::Map(BTreeMap::from([(
                MapKey::Bytestring(hex::decode("0a").unwrap()),
                Value::Int(1),
            )]))),
        ),
        // This is a COSE key to check that map ordering works correctly.
        (
            "a501020326200121582009ac4af6a4646b5bfe81c37f751769c768c5c41ffea633dad0f48e6e3bc3e9a0225820269fbe132c40bf11f4de4a92bec527901906fdce98bbed52df9b175b6a4f3808",
            Ok(Value::Map(BTreeMap::from([
                (MapKey::Int(1), Value::Int(2)),
                (MapKey::Int(3), Value::Int(-7)),
                (MapKey::Int(-1), Value::Int(1)),
                (
                    MapKey::Int(-2),
                    Value::Bytestring(
                        hex::decode(
                            "09ac4af6a4646b5bfe81c37f751769c768c5c41ffea633dad0f48e6e3bc3e9a0",
                        )
                        .unwrap(),
                    ),
                ),
                (
                    MapKey::Int(-3),
                    Value::Bytestring(
                        hex::decode(
                            "269fbe132c40bf11f4de4a92bec527901906fdce98bbed52df9b175b6a4f3808",
                        )
                        .unwrap(),
                    ),
                ),
            ]))),
        ),
        (
            "a101a101a101a101a101a101a101a101a101a101a101a101a101a101a101a101a101a101a101a0",
            Err(Error::DepthLimitExceeded(6, MAX_DEPTH)),
        ),
        ("a202010102", Err(Error::MapKeysOutOfOrder(1, Value::Int(1)))),
        ("a1810101", Err(Error::UnsupportedMapKeyType(1, Value::Array(vec![Value::Int(1)])))),
        ("f4", Ok(Value::Boolean(false))),
        ("f5", Ok(Value::Boolean(true))),
        ("f6", Ok(Value::Null)),
        ("f7", Ok(Value::Undefined)),
        ("a201010102", Err(Error::DuplicateMapKey(1, Value::Int(1)))),
        ("f93c00", Err(Error::UnsupportedFloatingPointValue(0x3c00))),
        ("fa3f801000", Err(Error::UnsupportedFloatingPointValue(0x3f801000))),
        ("fb3ff0000000000001", Err(Error::UnsupportedFloatingPointValue(0x3ff0000000000001))),
    ];

    for test in test_cases {
        let bytes = hex::decode(test.0).unwrap();
        let result = parse_bytes(&bytes);
        assert_eq!(result, test.1, "{}", test.0);
        if let Ok(value) = result {
            let bytes2 = value.to_bytes();
            assert_eq!(bytes, bytes2, "{}", test.0);
        }
        if !bytes.is_empty() {
            // All truncations of the input should fail to parse.
            for i in 0..bytes.len() - 1 {
                let result = parse_bytes(&bytes[..i]);
                assert!(matches!(result, Result::Err(_)));
            }
        }
    }
}

#[gtest(CBORReaderRustTest, TestFloats)]
fn test_floats() {
    let config = Config { allow_floating_point: true, ..Config::default() };
    let test_cases = [
        ("f93c00", Ok(Value::Float(1.0))),
        ("fa3f801000", Ok(Value::Float(1.00048828125))),
        ("fb3ff0000000000001", Ok(Value::Float(f64::from_bits(0x3ff0000000000001)))),
        ("f97e00", Ok(Value::Float(f64::NAN))),      // f16 NaN
        ("f97c00", Ok(Value::Float(f64::INFINITY))), // f16 Infinity
        ("f9fc00", Ok(Value::Float(f64::NEG_INFINITY))), // f16 -Infinity
        ("fa3f800000", Err(Error::NonMinimalAdditionalData(4))), // 1.0 in f32
        ("fb3ff0000000000000", Err(Error::NonMinimalAdditionalData(8))), // 1.0 in f64
        ("fb3ff0020000000000", Err(Error::NonMinimalAdditionalData(8))), /* 1.00048828125 in f64
                                                      * (fits in f32) */
        ("fa7fc00000", Err(Error::NonMinimalAdditionalData(4))), // f32 NaN
        ("fb7ff8000000000000", Err(Error::NonMinimalAdditionalData(8))), // f64 NaN
        ("fa7f800000", Err(Error::NonMinimalAdditionalData(4))), // f32 Infinity
        ("fb7ff0000000000000", Err(Error::NonMinimalAdditionalData(8))), // f64 Infinity
        ("faff800000", Err(Error::NonMinimalAdditionalData(4))), // f32 -Infinity
        ("fbfff0000000000000", Err(Error::NonMinimalAdditionalData(8))), // f64 -Infinity
    ];

    for test in test_cases {
        let bytes = hex::decode(test.0).unwrap();
        let result = parse_with_config(&bytes, config).map(|(v, _)| v);

        match (&result, &test.1) {
            (Ok(Value::Float(a)), Ok(Value::Float(b))) if a.is_nan() && b.is_nan() => {}
            _ => assert_eq!(result, test.1, "{}", test.0),
        }
    }
}

#[gtest(CBORReaderRustTest, TestCornerCases)]
fn test_corner_cases() {
    let test_cases = [
        // B. Defensive Memory Constraints (OOM / DOS Prevention)
        ("5bffffffffffffffff", Err(Error::InputTruncated)),
        // C. Illegal / Indefinite Encodings
        ("5f", Err(Error::UnsupportedAdditionalInformation(0, 31))),
        ("7f", Err(Error::UnsupportedAdditionalInformation(0, 31))),
        ("9f", Err(Error::UnsupportedAdditionalInformation(0, 31))),
        ("bf", Err(Error::UnsupportedAdditionalInformation(0, 31))),
        ("ff", Err(Error::UnsupportedAdditionalInformation(0, 31))),
        // D. Advanced UTF-8 & Unicode Escapes
        ("62c080", Err(Error::InvalidUTF8(2))),
        ("63eda080", Err(Error::InvalidUTF8(3))),
        ("626100", Ok(Value::String(String::from("a\x00")))),
        // E. Semantic CBOR Tags Rejection
        ("c0", Err(Error::UnsupportedMajorType(0, 6))),
        ("c2", Err(Error::UnsupportedMajorType(0, 6))),
    ];

    for test in test_cases {
        let bytes = hex::decode(test.0).unwrap();
        let result = parse_bytes(&bytes);
        assert_eq!(result, test.1, "{}", test.0);
    }
}
