// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use cbor::*;
use rust_gtest_interop::prelude::*;

fn parse_bytes<'a>(input: &'a [u8]) -> Result<Value<'a>, Error> {
    let len = input.len();
    let result = parse_with_config(input, Config::default())?;
    if result.bytes_consumed < len {
        Err(Error::ExtraneousData)
    } else {
        Ok(result.value)
    }
}

#[gtest(CBORReaderRustTest, TestInputs)]
fn test_inputs() {
    let test_cases = [
        ("", Err(Error::IncompleteCborData)),
        ("d0", Err(Error::UnsupportedMajorType)),
        ("1f", Err(Error::UnknownAdditionalInfo)),
        ("01", Ok(Value::Int(1))),
        ("0100", Err(Error::ExtraneousData)),
        ("20", Ok(Value::Int(-1))),
        ("182a", Ok(Value::Int(42))),
        ("191388", Ok(Value::Int(5000))),
        ("1a02faf080", Ok(Value::Int(50000000))),
        ("1b000000746a528800", Ok(Value::Int(500000000000))),
        ("1bffffffffffffffff", Err(Error::OutOfRangeIntegerValue)),
        ("3bffffffffffffffff", Err(Error::OutOfRangeIntegerValue)),
        ("1818", Ok(Value::Int(24))),
        ("1817", Err(Error::NonMinimalCborEncoding)),
        ("190080", Err(Error::NonMinimalCborEncoding)),
        ("60", Ok(Value::String(""))),
        ("6161", Ok(Value::String("a"))),
        ("40", Ok(Value::Bytestring(&[]))),
        ("4100", Ok(Value::Bytestring(&[0x00]))),
        ("61ff", Err(Error::InvalidUtf8)),
        ("80", Ok(Value::Array(Vec::new()))),
        ("8101", Ok(Value::Array(vec![Value::Int(1)]))),
        ("818101", Ok(Value::Array(vec![Value::Array(vec![Value::Int(1)])]))),
        (
            "8181818181818181818181818181818181818180",
            Err(Error::TooMuchNesting),
        ),
        ("a0", Ok(Value::Map(vec![].into()))),
        ("a10101", Ok(Value::Map(vec![(MapKey::Int(1), Value::Int(1)).into()].into()))),
        ("a12101", Ok(Value::Map(vec![(MapKey::Int(-2), Value::Int(1)).into()].into()))),
        (
            "a201010202",
            Ok(Value::Map(
                vec![
                    (MapKey::Int(1), Value::Int(1)).into(),
                    (MapKey::Int(2), Value::Int(2)).into(),
                ]
                .into(),
            )),
        ),
        (
            "a1410a01",
            Ok(Value::Map(
                vec![(
                    MapKey::Bytestring(&[0x0a]),
                    Value::Int(1),
                ).into()]
                .into(),
            )),
        ),
        // This is a COSE key to check that map ordering works correctly.
        (
            "a501020326200121582009ac4af6a4646b5bfe81c37f751769c768c5c41ffea633dad0f48e6e3bc3e9a0225820269fbe132c40bf11f4de4a92bec527901906fdce98bbed52df9b175b6a4f3808",
            Ok(Value::Map(
                vec![
                    (MapKey::Int(1), Value::Int(2)).into(),
                    (MapKey::Int(3), Value::Int(-7)).into(),
                    (MapKey::Int(-1), Value::Int(1)).into(),
                    (
                        MapKey::Int(-2),
                        Value::Bytestring(
                            b"\x09\xac\x4a\xf6\xa4\x64\x6b\x5b\xfe\x81\xc3\x7f\x75\x17\x69\xc7\x68\xc5\xc4\x1f\xfe\xa6\x33\xda\xd0\xf4\x8e\x6e\x3b\xc3\xe9\xa0",
                        ),
                    ).into(),
                    (
                        MapKey::Int(-3),
                        Value::Bytestring(
                            b"\x26\x9f\xbe\x13\x2c\x40\xbf\x11\xf4\xde\x4a\x92\xbe\xc5\x27\x90\x19\x06\xfd\xce\x98\xbb\xed\x52\xdf\x9b\x17\x5b\x6a\x4f\x38\x08",
                        ),
                    ).into(),
                ]
                .into(),
            )),
        ),
        (
            "a101a101a101a101a101a101a101a101a101a101a101a101a101a101a101a101a101a101a101a0",
            Err(Error::TooMuchNesting),
        ),
        ("a202010102", Err(Error::OutOfOrderKey)),
        ("a1810101", Err(Error::IncorrectMapKeyType)),
        ("f4", Ok(Value::Boolean(false))),
        ("f5", Ok(Value::Boolean(true))),
        ("f6", Ok(Value::Null)),
        ("f7", Ok(Value::Undefined)),
        ("a201010102", Err(Error::DuplicateKey)),
        ("f93c00", Err(Error::UnsupportedFloatingPointValue)),
        ("fa3f801000", Err(Error::UnsupportedFloatingPointValue)),
        ("fb3ff0000000000001", Err(Error::UnsupportedFloatingPointValue)),
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
                assert!(result.is_err());
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
        ("fa3f800000", Err(Error::NonMinimalCborEncoding)), // 1.0 in f32
        ("fb3ff0000000000000", Err(Error::NonMinimalCborEncoding)), // 1.0 in f64
        ("fb3ff0020000000000", Err(Error::NonMinimalCborEncoding)), /* 1.00048828125 in f64
                                                      * (fits in f32) */
        ("fa7fc00000", Err(Error::NonMinimalCborEncoding)), // f32 NaN
        ("fb7ff8000000000000", Err(Error::NonMinimalCborEncoding)), // f64 NaN
        ("fa7f800000", Err(Error::NonMinimalCborEncoding)), // f32 Infinity
        ("fb7ff0000000000000", Err(Error::NonMinimalCborEncoding)), // f64 Infinity
        ("faff800000", Err(Error::NonMinimalCborEncoding)), // f32 -Infinity
        ("fbfff0000000000000", Err(Error::NonMinimalCborEncoding)), // f64 -Infinity
    ];

    for test in test_cases {
        let bytes = hex::decode(test.0).unwrap();
        let result = parse_with_config(&bytes, config).map(|result| result.value);

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
        ("5bffffffffffffffff", Err(Error::IncompleteCborData)),
        // C. Illegal / Indefinite Encodings
        ("5f", Err(Error::UnknownAdditionalInfo)),
        ("7f", Err(Error::UnknownAdditionalInfo)),
        ("9f", Err(Error::UnknownAdditionalInfo)),
        ("bf", Err(Error::UnknownAdditionalInfo)),
        ("ff", Err(Error::UnknownAdditionalInfo)),
        // D. Advanced UTF-8 & Unicode Escapes
        ("62c080", Err(Error::InvalidUtf8)),
        ("63eda080", Err(Error::InvalidUtf8)),
        ("626100", Ok(Value::String("a\x00"))),
        // E. Semantic CBOR Tags Rejection
        ("c0", Err(Error::UnsupportedMajorType)),
        ("c2", Err(Error::UnsupportedMajorType)),
    ];

    for test in test_cases {
        let bytes = hex::decode(test.0).unwrap();
        let result = parse_bytes(&bytes);
        assert_eq!(result, test.1, "{}", test.0);
    }
}

#[gtest(CBORReaderRustTest, TestMapKeyIsUnsupportedSimpleValue)]
fn test_map_unsupported_simple_values() {
    let bytes_key = hex::decode("a1f002").unwrap();
    let result_key = parse_bytes(&bytes_key);
    assert_eq!(result_key, Err(Error::UnsupportedSimpleValue));

    let bytes_value = hex::decode("a102f0").unwrap();
    let result_value = parse_bytes(&bytes_value);
    assert_eq!(result_value, Err(Error::UnsupportedSimpleValue));
}
