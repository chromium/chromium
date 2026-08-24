// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use alloc::format;
use alloc::vec::Vec;

use crate::constants::{
    ECDSA_P256_PUBLIC_KEY_ATTRIBUTE_NAME, ECDSA_P256_PUBLIC_KEY_LEN, ECDSA_P256_SIGNATURE_MAX_LEN,
    ECDSA_P256_SIGNATURE_MIN_LEN, ED25519_PUBLIC_KEY_ATTRIBUTE_NAME, MAGIC_BYTES,
    TOP_LEVEL_ARRAY_LENGTH, V2_VERSION_BYTES, WEB_BUNDLE_ID_ATTRIBUTE_NAME,
};
use crate::types::{IntegrityBlock, ParseError, SignatureStackEntry, SignatureType};

fn validate_keys_and_signature(
    signature_type: SignatureType,
    public_key: &[u8],
    signature: &[u8],
) -> Result<(), ParseError> {
    match signature_type {
        SignatureType::Ed25519 => {
            if public_key.len() != bssl_crypto::ed25519::PUBLIC_KEY_LEN {
                return Err(ParseError::format(format!(
                    "The Ed25519 public key does not have the correct length. Expected {} bytes, but received {} bytes.",
                    bssl_crypto::ed25519::PUBLIC_KEY_LEN,
                    public_key.len()
                )));
            }
            if signature.len() != bssl_crypto::ed25519::SIGNATURE_LEN {
                return Err(ParseError::format(format!(
                    "The signature has the wrong length. Expected {}, but got {} bytes.",
                    bssl_crypto::ed25519::SIGNATURE_LEN,
                    signature.len()
                )));
            }
        }
        SignatureType::EcdsaP256SHA256 => {
            if public_key.len() != ECDSA_P256_PUBLIC_KEY_LEN {
                return Err(ParseError::format(format!(
                    "The ECDSA P-256 public key does not have the correct length. Expected {} bytes, but received {} bytes.",
                    ECDSA_P256_PUBLIC_KEY_LEN,
                    public_key.len()
                )));
            }
            if bssl_crypto::ecdsa::PublicKey::<bssl_crypto::ec::P256>::from_x962_compressed(
                public_key,
            )
            .is_none()
            {
                return Err(ParseError::format(
                    "Unable to parse a valid ECDSA P-256 key from the given bytes.",
                ));
            }
            if signature.len() < ECDSA_P256_SIGNATURE_MIN_LEN
                || signature.len() > ECDSA_P256_SIGNATURE_MAX_LEN
            {
                return Err(ParseError::format(format!(
                    "The ECDSA P-256 SHA-256 signature does not have the correct length. Expected from {} to {} bytes, but received {} bytes.",
                    ECDSA_P256_SIGNATURE_MIN_LEN,
                    ECDSA_P256_SIGNATURE_MAX_LEN,
                    signature.len()
                )));
            }
        }
        SignatureType::Unknown => {}
    }
    Ok(())
}

fn parse_signature_info<'a>(
    entry: &cbor::Value<'a>,
) -> Result<SignatureStackEntry<'a>, ParseError> {
    let cbor::Value::Array(sig_array) = entry else {
        return Err(ParseError::format("Each signature stack entry must be a CBOR array."));
    };

    let [cbor::Value::Map(attributes_map), cbor::Value::Bytestring(signature_bytes)] =
        sig_array.as_slice()
    else {
        if sig_array.len() != 2 {
            return Err(ParseError::format(
                "Each signature stack entry must contain exactly two elements.",
            ));
        }
        return Err(ParseError::format("Malformed signature stack entry."));
    };

    let ed25519_key = attributes_map.get(&cbor::MapKey::String(ED25519_PUBLIC_KEY_ATTRIBUTE_NAME));
    let ecdsa_key = attributes_map.get(&cbor::MapKey::String(ECDSA_P256_PUBLIC_KEY_ATTRIBUTE_NAME));

    let (signature_type, public_key) = match (ed25519_key, ecdsa_key) {
        (Some(_), Some(_)) => {
            return Err(ParseError::format("Multiple key types for one signature."));
        }
        (Some(cbor::Value::Bytestring(pk)), None) => (SignatureType::Ed25519, *pk),
        (Some(_), None) => return Err(ParseError::format("Invalid ED25519 key.")),
        (None, Some(cbor::Value::Bytestring(pk))) => (SignatureType::EcdsaP256SHA256, *pk),
        (None, Some(_)) => return Err(ParseError::format("Invalid ECDSA key.")),
        (None, None) => (SignatureType::Unknown, &[][..]),
    };

    validate_keys_and_signature(signature_type, public_key, signature_bytes)?;

    let attributes_cbor = cbor::write(&sig_array[0]);

    Ok(SignatureStackEntry {
        attributes_cbor,
        signature_type,
        public_key,
        signature: signature_bytes,
    })
}

pub fn parse_integrity_block<'a>(input: &'a [u8]) -> Result<IntegrityBlock<'a>, ParseError> {
    let parse_result = cbor::parse_with_config(input, cbor::Config::default())
        .map_err(|e| ParseError::format(format!("Error parsing integrity block as CBOR: {}", e)))?;

    let cbor::Value::Array(top_array) = &parse_result.value else {
        return Err(ParseError::format("Integrity block is not a CBOR array."));
    };

    if top_array.len() != TOP_LEVEL_ARRAY_LENGTH {
        return Err(ParseError::format(format!(
            "Invalid integrity block array length: expected {}, got {}.",
            TOP_LEVEL_ARRAY_LENGTH,
            top_array.len()
        )));
    }

    // 1. Magic bytes
    let cbor::Value::Bytestring(MAGIC_BYTES) = &top_array[0] else {
        return Err(ParseError::format("Unexpected magic bytes."));
    };

    // 2. Version bytes
    let cbor::Value::Bytestring(V2_VERSION_BYTES) = &top_array[1] else {
        return Err(ParseError::version("Unexpected version bytes."));
    };

    // 3. Attributes map
    let cbor::Value::Map(attributes_map) = &top_array[2] else {
        return Err(ParseError::format("Integrity block attributes must be a map."));
    };

    let Some(cbor::Value::String(web_bundle_id)) =
        attributes_map.get(&cbor::MapKey::String(WEB_BUNDLE_ID_ATTRIBUTE_NAME))
    else {
        return Err(ParseError::format(
            "`webBundleId` integrity block attribute is missing or malformed.",
        ));
    };

    // 4. Signature Stack
    let cbor::Value::Array(signatures) = &top_array[3] else {
        return Err(ParseError::format("Signature stack must be an array."));
    };

    if signatures.is_empty() {
        return Err(ParseError::format("The signature stack must contain at least one signature."));
    }

    let mut signature_stack = Vec::with_capacity(signatures.len());
    for (i, sig_entry) in signatures.iter().enumerate() {
        let parsed = parse_signature_info(sig_entry)?;
        if i == 0 && parsed.signature_type == SignatureType::Unknown {
            return Err(ParseError::format("Unknown cipher type of the first signature."));
        }
        signature_stack.push(parsed);
    }

    let attributes_cbor = cbor::write(&top_array[2]);

    Ok(IntegrityBlock {
        size: parse_result.bytes_consumed,
        web_bundle_id,
        attributes_cbor,
        signature_stack,
    })
}
