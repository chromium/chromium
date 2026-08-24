// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use cbor::{Map, MapEntry, MapKey, Value};
use rust_gtest_interop::prelude::*;
use signed_web_bundles_rust::*;

const VALID_ECDSA_P256_PK: &[u8; 33] =
    b"\x02\x6b\x17\xd1\xf2\xe1\x2c\x42\x47\xf8\xbc\xe6\xe5\x63\xa4\x40\xf2\x77\x03\x7d\x81\x2d\xeb\x33\xa0\xf4\xa1\x39\x45\xd8\x98\xc2\x96";

fn make_top_attributes(bundle_id: &str) -> Value<'_> {
    Value::Map(Map::from(vec![MapEntry::from((
        MapKey::String("webBundleId"),
        Value::String(bundle_id),
    ))]))
}

fn make_ed25519_sig_entry<'a>(pk: &'a [u8], sig: &'a [u8]) -> Value<'a> {
    Value::Array(vec![
        Value::Map(Map::from(vec![MapEntry::from((
            MapKey::String("ed25519PublicKey"),
            Value::Bytestring(pk),
        ))])),
        Value::Bytestring(sig),
    ])
}

fn make_valid_ib_cbor() -> Vec<u8> {
    let top_array = Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        make_top_attributes("test-bundle-id"),
        Value::Array(vec![make_ed25519_sig_entry(&[0x42; 32], &[0x24; 64])]),
    ]);
    cbor::write(&top_array)
}

#[gtest(IntegrityBlockRustTest, TestInvalidCbor)]
fn test_invalid_cbor() {
    let result = parse_integrity_block(&[0xff]);
    assert!(result.is_err());
    let err = result.unwrap_err();
    assert!(!err.is_version_error);
    assert!(err.message.as_str().starts_with("Error parsing integrity block as CBOR:"));
}

#[gtest(IntegrityBlockRustTest, TestNotACborArray)]
fn test_not_a_cbor_array() {
    let data = cbor::write(&Value::Int(42));
    let err = parse_integrity_block(&data).unwrap_err();
    assert_eq!(err.message.as_str(), "Integrity block is not a CBOR array.");
    assert!(!err.is_version_error);
}

#[gtest(IntegrityBlockRustTest, TestInvalidArrayLength)]
fn test_invalid_array_length() {
    let data = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        make_top_attributes("test-bundle-id"),
    ]));
    let err = parse_integrity_block(&data).unwrap_err();
    assert_eq!(err.message.as_str(), "Invalid integrity block array length: expected 4, got 3.");
}

#[gtest(IntegrityBlockRustTest, TestUnexpectedMagicBytes)]
fn test_unexpected_magic_bytes() {
    let data = cbor::write(&Value::Array(vec![
        Value::Bytestring(b"badmagic"),
        Value::Bytestring(V2_VERSION_BYTES),
        make_top_attributes("test-bundle-id"),
        Value::Array(vec![make_ed25519_sig_entry(&[0x42; 32], &[0x24; 64])]),
    ]));
    let err = parse_integrity_block(&data).unwrap_err();
    assert_eq!(err.message.as_str(), "Unexpected magic bytes.");
    assert!(!err.is_version_error);
}

#[gtest(IntegrityBlockRustTest, TestUnexpectedVersionBytes)]
fn test_unexpected_version_bytes() {
    let data = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(b"3b\0\0"),
        make_top_attributes("test-bundle-id"),
        Value::Array(vec![make_ed25519_sig_entry(&[0x42; 32], &[0x24; 64])]),
    ]));
    let err = parse_integrity_block(&data).unwrap_err();
    assert_eq!(err.message.as_str(), "Unexpected version bytes.");
    assert!(err.is_version_error);
}

#[gtest(IntegrityBlockRustTest, TestAttributesNotAMap)]
fn test_attributes_not_a_map() {
    let data = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        Value::Array(vec![]),
        Value::Array(vec![make_ed25519_sig_entry(&[0x42; 32], &[0x24; 64])]),
    ]));
    let err = parse_integrity_block(&data).unwrap_err();
    assert_eq!(err.message.as_str(), "Integrity block attributes must be a map.");
}

#[gtest(IntegrityBlockRustTest, TestMissingOrMalformedWebBundleId)]
fn test_missing_or_malformed_web_bundle_id() {
    // Missing webBundleId
    let data_missing = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        Value::Map(Map::new()),
        Value::Array(vec![make_ed25519_sig_entry(&[0x42; 32], &[0x24; 64])]),
    ]));
    let err = parse_integrity_block(&data_missing).unwrap_err();
    assert_eq!(
        err.message.as_str(),
        "`webBundleId` integrity block attribute is missing or malformed."
    );

    // Malformed webBundleId (int instead of string)
    let data_malformed = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        Value::Map(Map::from(vec![MapEntry::from((
            MapKey::String("webBundleId"),
            Value::Int(123),
        ))])),
        Value::Array(vec![make_ed25519_sig_entry(&[0x42; 32], &[0x24; 64])]),
    ]));
    let err2 = parse_integrity_block(&data_malformed).unwrap_err();
    assert_eq!(
        err2.message.as_str(),
        "`webBundleId` integrity block attribute is missing or malformed."
    );
}

#[gtest(IntegrityBlockRustTest, TestSignatureStackNotAnArray)]
fn test_signature_stack_not_an_array() {
    let data = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        make_top_attributes("test-bundle-id"),
        Value::Int(123),
    ]));
    let err = parse_integrity_block(&data).unwrap_err();
    assert_eq!(err.message.as_str(), "Signature stack must be an array.");
}

#[gtest(IntegrityBlockRustTest, TestSignatureStackEmpty)]
fn test_signature_stack_empty() {
    let data = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        make_top_attributes("test-bundle-id"),
        Value::Array(vec![]),
    ]));
    let err = parse_integrity_block(&data).unwrap_err();
    assert_eq!(err.message.as_str(), "The signature stack must contain at least one signature.");
}

#[gtest(IntegrityBlockRustTest, TestSignatureStackEntryNotAnArray)]
fn test_signature_stack_entry_not_an_array() {
    let data = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        make_top_attributes("test-bundle-id"),
        Value::Array(vec![Value::Int(123)]),
    ]));
    let err = parse_integrity_block(&data).unwrap_err();
    assert_eq!(err.message.as_str(), "Each signature stack entry must be a CBOR array.");

    let data_map = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        make_top_attributes("test-bundle-id"),
        Value::Array(vec![Value::Map(Map::new())]),
    ]));
    let err_map = parse_integrity_block(&data_map).unwrap_err();
    assert_eq!(err_map.message.as_str(), "Each signature stack entry must be a CBOR array.");
}

#[gtest(IntegrityBlockRustTest, TestSignatureStackEntryWrongLength)]
fn test_signature_stack_entry_wrong_length() {
    // Entry array with 1 item
    let data_1 = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        make_top_attributes("test-bundle-id"),
        Value::Array(vec![Value::Array(vec![Value::Map(Map::new())])]),
    ]));
    let err_1 = parse_integrity_block(&data_1).unwrap_err();
    assert_eq!(
        err_1.message.as_str(),
        "Each signature stack entry must contain exactly two elements."
    );

    // Entry array with 3 items
    let data_3 = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        make_top_attributes("test-bundle-id"),
        Value::Array(vec![Value::Array(vec![
            Value::Map(Map::new()),
            Value::Bytestring(&[0x24; 64]),
            Value::Int(3),
        ])]),
    ]));
    let err_3 = parse_integrity_block(&data_3).unwrap_err();
    assert_eq!(
        err_3.message.as_str(),
        "Each signature stack entry must contain exactly two elements."
    );
}

#[gtest(IntegrityBlockRustTest, TestSignatureStackEntryMalformedTypes)]
fn test_signature_stack_entry_malformed_types() {
    let data = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        make_top_attributes("test-bundle-id"),
        Value::Array(vec![Value::Array(vec![
            Value::Bytestring(b"not-a-map"),
            Value::Bytestring(&[0x24; 64]),
        ])]),
    ]));
    let err = parse_integrity_block(&data).unwrap_err();
    assert_eq!(err.message.as_str(), "Malformed signature stack entry.");
}

#[gtest(IntegrityBlockRustTest, TestMultipleKeyTypes)]
fn test_multiple_key_types() {
    let data = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        make_top_attributes("test-bundle-id"),
        Value::Array(vec![Value::Array(vec![
            Value::Map(Map::from(vec![
                MapEntry::from((
                    MapKey::String("ecdsaP256SHA256PublicKey"),
                    Value::Bytestring(VALID_ECDSA_P256_PK),
                )),
                MapEntry::from((
                    MapKey::String("ed25519PublicKey"),
                    Value::Bytestring(&[0x42; 32]),
                )),
            ])),
            Value::Bytestring(&[0x24; 64]),
        ])]),
    ]));
    let err = parse_integrity_block(&data).unwrap_err();
    assert_eq!(err.message.as_str(), "Multiple key types for one signature.");
}

#[gtest(IntegrityBlockRustTest, TestInvalidKeyAttributeTypes)]
fn test_invalid_key_attribute_types() {
    // ed25519PublicKey is integer instead of bytestring
    let data_ed = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        make_top_attributes("test-bundle-id"),
        Value::Array(vec![Value::Array(vec![
            Value::Map(Map::from(vec![MapEntry::from((
                MapKey::String("ed25519PublicKey"),
                Value::Int(123),
            ))])),
            Value::Bytestring(&[0x24; 64]),
        ])]),
    ]));
    let err_ed = parse_integrity_block(&data_ed).unwrap_err();
    assert_eq!(err_ed.message.as_str(), "Invalid ED25519 key.");

    // ecdsaP256SHA256PublicKey is string instead of bytestring
    let data_ec = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        make_top_attributes("test-bundle-id"),
        Value::Array(vec![Value::Array(vec![
            Value::Map(Map::from(vec![MapEntry::from((
                MapKey::String("ecdsaP256SHA256PublicKey"),
                Value::String("not-bytes"),
            ))])),
            Value::Bytestring(&[0x24; 64]),
        ])]),
    ]));
    let err_ec = parse_integrity_block(&data_ec).unwrap_err();
    assert_eq!(err_ec.message.as_str(), "Invalid ECDSA key.");
}

#[gtest(IntegrityBlockRustTest, TestFirstSignatureUnknown)]
fn test_first_signature_unknown() {
    let data = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        make_top_attributes("test-bundle-id"),
        Value::Array(vec![Value::Array(vec![
            Value::Map(Map::from(vec![MapEntry::from((
                MapKey::String("otherCipherPublicKey"),
                Value::Bytestring(&[0x42; 32]),
            ))])),
            Value::Bytestring(&[0x24; 64]),
        ])]),
    ]));
    let err = parse_integrity_block(&data).unwrap_err();
    assert_eq!(err.message.as_str(), "Unknown cipher type of the first signature.");
}

#[gtest(IntegrityBlockRustTest, TestEd25519KeyAndSignatureLengthValidation)]
fn test_ed25519_key_and_signature_length_validation() {
    // Wrong public key length (31 instead of 32)
    let data_pk = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        make_top_attributes("test-bundle-id"),
        Value::Array(vec![make_ed25519_sig_entry(&[0x42; 31], &[0x24; 64])]),
    ]));
    let err_pk = parse_integrity_block(&data_pk).unwrap_err();
    assert_eq!(
        err_pk.message.as_str(),
        "The Ed25519 public key does not have the correct length. Expected 32 bytes, but received 31 bytes."
    );

    // Wrong signature length (65 instead of 64)
    let data_sig = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        make_top_attributes("test-bundle-id"),
        Value::Array(vec![make_ed25519_sig_entry(&[0x42; 32], &[0x24; 65])]),
    ]));
    let err_sig = parse_integrity_block(&data_sig).unwrap_err();
    assert_eq!(
        err_sig.message.as_str(),
        "The signature has the wrong length. Expected 64, but got 65 bytes."
    );
}

#[gtest(IntegrityBlockRustTest, TestEcdsaKeyAndSignatureValidation)]
fn test_ecdsa_key_and_signature_validation() {
    // Wrong public key length (32 instead of 33)
    let data_pk_len = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        make_top_attributes("test-bundle-id"),
        Value::Array(vec![Value::Array(vec![
            Value::Map(Map::from(vec![MapEntry::from((
                MapKey::String("ecdsaP256SHA256PublicKey"),
                Value::Bytestring(&VALID_ECDSA_P256_PK[..32]),
            ))])),
            Value::Bytestring(&[0x24; 64]),
        ])]),
    ]));
    let err_pk_len = parse_integrity_block(&data_pk_len).unwrap_err();
    assert_eq!(
        err_pk_len.message.as_str(),
        "The ECDSA P-256 public key does not have the correct length. Expected 33 bytes, but received 32 bytes."
    );

    // 33-byte public key that is not a valid curve point (invalid format prefix)
    let invalid_curve_pk = vec![0x00; 33];
    let data_pk_invalid = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        make_top_attributes("test-bundle-id"),
        Value::Array(vec![Value::Array(vec![
            Value::Map(Map::from(vec![MapEntry::from((
                MapKey::String("ecdsaP256SHA256PublicKey"),
                Value::Bytestring(&invalid_curve_pk),
            ))])),
            Value::Bytestring(&[0x24; 64]),
        ])]),
    ]));
    let err_pk_invalid = parse_integrity_block(&data_pk_invalid).unwrap_err();
    assert_eq!(
        err_pk_invalid.message.as_str(),
        "Unable to parse a valid ECDSA P-256 key from the given bytes."
    );

    // Signature length too short (63 bytes)
    let data_sig_short = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        make_top_attributes("test-bundle-id"),
        Value::Array(vec![Value::Array(vec![
            Value::Map(Map::from(vec![MapEntry::from((
                MapKey::String("ecdsaP256SHA256PublicKey"),
                Value::Bytestring(VALID_ECDSA_P256_PK),
            ))])),
            Value::Bytestring(&[0x24; 63]),
        ])]),
    ]));
    let err_sig_short = parse_integrity_block(&data_sig_short).unwrap_err();
    assert_eq!(
        err_sig_short.message.as_str(),
        "The ECDSA P-256 SHA-256 signature does not have the correct length. Expected from 64 to 72 bytes, but received 63 bytes."
    );

    // Signature length too long (73 bytes)
    let data_sig_long = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        make_top_attributes("test-bundle-id"),
        Value::Array(vec![Value::Array(vec![
            Value::Map(Map::from(vec![MapEntry::from((
                MapKey::String("ecdsaP256SHA256PublicKey"),
                Value::Bytestring(VALID_ECDSA_P256_PK),
            ))])),
            Value::Bytestring(&[0x24; 73]),
        ])]),
    ]));
    let err_sig_long = parse_integrity_block(&data_sig_long).unwrap_err();
    assert_eq!(
        err_sig_long.message.as_str(),
        "The ECDSA P-256 SHA-256 signature does not have the correct length. Expected from 64 to 72 bytes, but received 73 bytes."
    );
}

#[gtest(IntegrityBlockRustTest, TestValidEd25519IntegrityBlock)]
fn test_valid_ed25519_integrity_block() {
    let data = make_valid_ib_cbor();
    let ib = parse_integrity_block(&data).expect("valid parse");
    assert_eq!(ib.size, data.len());
    assert_eq!(ib.web_bundle_id, "test-bundle-id");
    assert_eq!(ib.signature_stack.len(), 1);
    assert_eq!(ib.signature_stack[0].signature_type, SignatureType::Ed25519);
    assert_eq!(ib.signature_stack[0].public_key, &[0x42; 32]);
    assert_eq!(ib.signature_stack[0].signature, &[0x24; 64]);
}

#[gtest(IntegrityBlockRustTest, TestValidEcdsaIntegrityBlock)]
fn test_valid_ecdsa_integrity_block() {
    let data = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        make_top_attributes("test-bundle-id-ecdsa"),
        Value::Array(vec![Value::Array(vec![
            Value::Map(Map::from(vec![MapEntry::from((
                MapKey::String("ecdsaP256SHA256PublicKey"),
                Value::Bytestring(VALID_ECDSA_P256_PK),
            ))])),
            Value::Bytestring(&[0x24; 70]),
        ])]),
    ]));
    let ib = parse_integrity_block(&data).expect("valid parse");
    assert_eq!(ib.size, data.len());
    assert_eq!(ib.web_bundle_id, "test-bundle-id-ecdsa");
    assert_eq!(ib.signature_stack.len(), 1);
    assert_eq!(ib.signature_stack[0].signature_type, SignatureType::EcdsaP256SHA256);
    assert_eq!(ib.signature_stack[0].public_key, VALID_ECDSA_P256_PK.as_slice());
    assert_eq!(ib.signature_stack[0].signature, &[0x24; 70]);
}

#[gtest(IntegrityBlockRustTest, TestValidMultiSignatureWithUnknownTrailing)]
fn test_valid_multi_signature_with_unknown_trailing() {
    let data = cbor::write(&Value::Array(vec![
        Value::Bytestring(MAGIC_BYTES),
        Value::Bytestring(V2_VERSION_BYTES),
        make_top_attributes("multi-sig-bundle"),
        Value::Array(vec![
            make_ed25519_sig_entry(&[0x42; 32], &[0x24; 64]),
            Value::Array(vec![
                Value::Map(Map::from(vec![MapEntry::from((
                    MapKey::String("unknownFutureCipherKey"),
                    Value::Bytestring(&[0x99; 32]),
                ))])),
                Value::Bytestring(&[0x88; 64]),
            ]),
        ]),
    ]));
    let ib = parse_integrity_block(&data).expect("valid parse");
    assert_eq!(ib.signature_stack.len(), 2);
    assert_eq!(ib.signature_stack[0].signature_type, SignatureType::Ed25519);
    assert_eq!(ib.signature_stack[1].signature_type, SignatureType::Unknown);
    assert_eq!(ib.signature_stack[1].public_key, &[]);
    assert_eq!(ib.signature_stack[1].signature, &[0x88; 64]);
}
