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

#[gtest(CBORReaderRustTest, TestErrorToString)]
fn test_error_to_string() {
    assert_eq!(Error::UnsupportedMajorType.to_str(), "Unsupported major type.");
    assert_eq!(
        format!("{}", Error::IncompleteCborData),
        "Prematurely terminated CBOR data byte array."
    );
    assert_eq!(
        Error::OutOfOrderKey.to_str(),
        "Map keys must be strictly monotonically increasing based on byte length and then by byte-wise lexical order."
    );
}

#[gtest(CBORReaderRustTest, TestMapLookups)]
fn test_map_lookups() {
    let map = Map::new();
    assert_eq!(map.get(&MapKey::String("foo")), None);
    assert_eq!(map.get(&MapKey::Int(42)), None);

    let bytes = hex::decode("a3010a420102182a63666f6f63626172").unwrap();
    let parsed = parse_with_config(&bytes, Config::default()).unwrap();

    let Value::Map(map) = parsed.value else {
        panic!("expected map");
    };

    assert_eq!(map.get(&MapKey::Int(1)), Some(&Value::Int(10)));
    assert_eq!(map.get(&MapKey::Int(2)), None);

    assert_eq!(map.get(&MapKey::String("foo")), Some(&Value::String("bar")));
    assert_eq!(map.get(&MapKey::String("bar")), None);

    assert_eq!(map.get(&MapKey::Bytestring(&[1, 2])), Some(&Value::Int(42)));
    assert_eq!(map.get(&MapKey::Bytestring(&[1, 3])), None);
}

#[gtest(CBORReaderRustTest, TestStreamingDecoder)]
fn test_streaming_decoder() {
    let mut data: &[u8] = &[0x82, 0x01, 0x02];
    let mut decoder = Decoder::new();
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::ArrayStart(2)));
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::Int(1)));
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::Int(2)));
}

#[gtest(CBORReaderRustTest, TestStreamingDecoderWebBundle)]
fn test_streaming_decoder_webbundle() {
    let mut data: &[u8] =
        &[0x85, 0x48, 0xF0, 0x9F, 0x8C, 0x90, 0xF0, 0x9F, 0x93, 0xA6, 0x44, 0x62, 0x32, 0x00, 0x00];
    let mut decoder = Decoder::new();
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::ArrayStart(5)));

    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::BytesStart(8)));
    assert_eq!(
        decoder.next_event(&mut data),
        Ok(CborEvent::BytesChunk(&[0xF0, 0x9F, 0x8C, 0x90, 0xF0, 0x9F, 0x93, 0xA6]))
    );
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::BytesEnd));

    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::BytesStart(4)));
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::BytesChunk(&[0x62, 0x32, 0x00, 0x00])));
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::BytesEnd));
}

#[gtest(CBORReaderRustTest, TestStreamingDecoderComplex)]
fn test_streaming_decoder_complex() {
    let mut data: &[u8] = &[0xa2, 0x61, b'a', 0x01, 0x61, b'b', 0x82, 0x20, 0x21];
    let mut decoder = Decoder::new();

    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::MapStart(2)));
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::String("a")));
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::Int(1)));
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::String("b")));
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::ArrayStart(2)));
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::Int(-1)));
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::Int(-2)));
}

#[gtest(CBORReaderRustTest, TestStreamingDecoderInvalidUtf8String)]
fn test_streaming_decoder_invalid_utf8_string() {
    let mut data: &[u8] = &[0x61, 0x80]; // Major type 3 (text string) length 1, invalid UTF-8 byte 0x80
    let mut decoder = Decoder::new();
    assert_eq!(decoder.next_event(&mut data), Err(Error::InvalidUtf8));
}

#[gtest(CBORReaderRustTest, TestStreamingDecoderIncomplete)]
fn test_streaming_decoder_incomplete() {
    let mut data: &[u8] = &[0x82, 0x01]; // Array of 2 elements, but only holds 1
    let mut decoder = Decoder::new();
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::ArrayStart(2)));
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::Int(1)));
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::NeedsMoreData(1)));
}

#[gtest(CBORReaderRustTest, TestStreamingDecoderNonMinimal)]
fn test_streaming_decoder_non_minimal() {
    // Int 1 encoded in 2 bytes instead of 1 (0x18 0x01 instead of 0x01)
    let mut data: &[u8] = &[0x18, 0x01];
    let mut decoder = Decoder::new();
    assert_eq!(decoder.next_event(&mut data), Err(Error::NonMinimalCborEncoding));
}

#[gtest(CBORReaderRustTest, TestStreamingDecoderIncrementalChunking)]
fn test_streaming_decoder_incremental_chunking() {
    let mut decoder = Decoder::new();

    let mut chunk1: &[u8] = &[0x44]; // StringStart(4)
    assert_eq!(decoder.next_event(&mut chunk1), Ok(CborEvent::BytesStart(4)));

    let mut chunk2: &[u8] = &[0x62]; // "b"
    assert_eq!(decoder.next_event(&mut chunk2), Ok(CborEvent::BytesChunk(&[0x62])));

    let mut chunk3: &[u8] = &[];
    assert_eq!(decoder.next_event(&mut chunk3), Ok(CborEvent::NeedsMoreData(1)));

    let mut chunk4: &[u8] = &[0x32, 0x00, 0x00]; // "2\0\0"
    assert_eq!(decoder.next_event(&mut chunk4), Ok(CborEvent::BytesChunk(&[0x32, 0x00, 0x00])));
    assert_eq!(decoder.next_event(&mut chunk4), Ok(CborEvent::BytesEnd));
}

#[gtest(CBORReaderRustTest, TestStreamingDecoderIncrementalString)]
fn test_streaming_decoder_incremental_string() {
    let mut decoder = Decoder::new();
    let mut chunk1: &[u8] = &[0x64, b't', b'e'];

    // Since only 2 of 4 bytes are available in the chunk, read_bytes returns None
    // (IncompleteCborData).
    assert_eq!(decoder.next_event(&mut chunk1), Ok(CborEvent::NeedsMoreData(5)));

    // Provide the complete text string chunk:
    let mut chunk2: &[u8] = &[0x64, b't', b'e', b's', b't'];
    assert_eq!(decoder.next_event(&mut chunk2), Ok(CborEvent::String("test")));
}

#[gtest(CBORReaderRustTest, TestStreamingDecoderAllowInvalidUtf8)]
fn test_streaming_decoder_allow_invalid_utf8() {
    let mut data: &[u8] = &[0x61, 0xFF]; // Text string of 1 byte with invalid UTF-8
    let config = Config { allow_invalid_utf8: true, ..Default::default() };
    let mut decoder = Decoder::with_config(config);
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::InvalidUtf8(&[0xFF])));

    // Also test Value conversion:
    let event = CborEvent::InvalidUtf8(&[0xFF]);
    assert_eq!(Value::try_from(event), Ok(Value::InvalidUtf8(&[0xFF])));

    let event_int = CborEvent::Int(42);
    assert_eq!(Value::try_from(event_int), Ok(Value::Int(42)));

    let event_str = CborEvent::String("hello");
    assert_eq!(Value::try_from(event_str), Ok(Value::String("hello")));
}

#[gtest(CBORReaderRustTest, TestStreamingDecoderReadCompleteBytestring)]
fn test_streaming_decoder_read_complete_bytestring() {
    // Array of 2 items: [ h'01020304', 42 ]
    let mut data: &[u8] = &[0x82, 0x44, 0x01, 0x02, 0x03, 0x04, 0x18, 0x2A];
    let mut decoder = Decoder::new();
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::ArrayStart(2)));
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::BytesStart(4)));
    assert_eq!(decoder.read_complete_bytestring(&mut data), Ok(&[0x01, 0x02, 0x03, 0x04][..]));
    // Ensure state correctly transitions back to Value and reads subsequent
    // elements:
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::Int(42)));
}

#[gtest(CBORReaderRustTest, TestDecoderBytestringCompletion)]
fn test_ffi_decoder_bytestring_completion() {
    let mut decoder = Decoder::new();
    let mut data: &[u8] = &[0x44, 0x01, 0x02, 0x03, 0x04];
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::BytesStart(4)));
    assert!(!decoder.is_complete());
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::BytesChunk(&[0x01, 0x02, 0x03, 0x04])));
    assert!(!decoder.is_complete());
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::BytesEnd));
    // Must be complete immediately upon BytesEnd for top-level bytestring!
    assert!(decoder.is_complete());
}

#[gtest(CBORReaderRustTest, TestStreamingDecoderReadCompleteValue)]
fn test_streaming_decoder_read_complete_value() {
    let mut decoder = Decoder::new();
    let original: &[u8] = &[0x18, 0x2A, 0x01, 0x02];
    let mut data = original;
    let val = decoder.read_complete_value(&mut data).unwrap();
    assert_eq!(val, Value::Int(42));
    assert_eq!(data, &[0x01, 0x02]);
}

#[gtest(CBORReaderRustTest, TestStreamingDecoderHugeMapLengthNoOverflow)]
fn test_streaming_decoder_huge_map_length_no_overflow() {
    // Map with length 0x9595959595959595 (> u64::MAX / 2).
    // An array [21] followed by 0xBF (unsupported additional info 31).
    let mut data: &[u8] = &[
        0xBB, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, // Map header
        0x95, // Array of length 21
        0x15, // Uint 21
        0xBF, // Map with additional info 31 (invalid)
        0xBE, 0x3B, 0x4F, 0x2F, 0x9E,
    ];
    let mut decoder = Decoder::new();
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::MapStart(0x9595959595959595)));
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::ArrayStart(21)));
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::Int(21)));
    assert_eq!(decoder.next_event(&mut data), Err(Error::UnknownAdditionalInfo));
}

#[gtest(CBORReaderRustTest, TestStreamingDecoderReadCompleteValueIncompleteBacktrack)]
fn test_streaming_decoder_read_complete_value_incomplete_backtrack() {
    let mut decoder = Decoder::new();
    // Array of 7 elements, but only provides 6 (1 to 6)
    let original: &[u8] = &[0x87, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06];
    let mut data = original;
    let res = decoder.read_complete_value(&mut data);
    assert_eq!(res.unwrap_err(), Error::IncompleteCborData);

    // The data pointer must be fully backtracked to the original!
    assert_eq!(data, original);

    // The decoder's internal nesting stack state must also be reverted,
    // meaning the next_event should interpret the array start cleanly.
    assert_eq!(decoder.next_event(&mut data), Ok(CborEvent::ArrayStart(7)));
}

#[gtest(CBORReaderRustTest, TestOnePassHugeMapLengthNoOverflow)]
fn test_one_pass_huge_map_length_no_overflow() {
    // Map with length 0x9595959595959595 (> u64::MAX / 2).
    // An array [21] followed by 0xBF (unsupported additional info 31).
    let data: &[u8] = &[
        0xBB, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, // Map header
        0x95, // Array of length 21
        0x15, // Uint 21
        0xBF, // Map with additional info 31 (invalid)
        0xBE, 0x3B, 0x4F, 0x2F, 0x9E,
    ];
    let res = parse_with_config(data, Config::default());
    assert_eq!(res, Err(Error::UnknownAdditionalInfo));
}
