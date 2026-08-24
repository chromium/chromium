// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use alloc::vec::Vec;

use crate::constants::{MAGIC_BYTES, V2_VERSION_BYTES};
use crate::types::SignatureType;

/// Constructs the CBOR representation of an integrity block with an empty
/// signature array by parsing the pre-encoded attributes and serializing
/// the top-level CBOR structure using the memory-safe CBOR writer.
pub fn create_empty_integrity_block_cbor(attributes_cbor: &[u8]) -> Vec<u8> {
    let attributes_value = match cbor::parse_with_config(attributes_cbor, cbor::Config::default()) {
        Ok(parsed) => parsed.value,
        Err(_) => cbor::Value::Map(Default::default()),
    };

    let top_array = cbor::Value::Array(alloc::vec![
        cbor::Value::Bytestring(MAGIC_BYTES),
        cbor::Value::Bytestring(V2_VERSION_BYTES),
        attributes_value,
        cbor::Value::Array(Vec::new()),
    ]);

    cbor::write(&top_array)
}

fn append_payload_item(payload: &mut Vec<u8>, item: &[u8]) {
    payload.extend_from_slice(&(item.len() as u64).to_be_bytes());
    payload.extend_from_slice(item);
}

/// Constructs the signature payload by concatenating 8-byte big-endian
/// length-prefixed chunks: unsigned_web_bundle_hash, integrity_block_cbor, and
/// attributes_cbor.
pub fn create_signature_payload(
    unsigned_web_bundle_hash: &[u8],
    integrity_block_cbor: &[u8],
    attributes_cbor: &[u8],
) -> Vec<u8> {
    let mut payload = Vec::with_capacity(
        8 + unsigned_web_bundle_hash.len()
            + 8
            + integrity_block_cbor.len()
            + 8
            + attributes_cbor.len(),
    );
    append_payload_item(&mut payload, unsigned_web_bundle_hash);
    append_payload_item(&mut payload, integrity_block_cbor);
    append_payload_item(&mut payload, attributes_cbor);
    payload
}

/// Verifies an Ed25519 signature over `message` using BoringSSL.
#[must_use]
pub fn verify_ed25519_signature(public_key: &[u8], signature: &[u8], message: &[u8]) -> bool {
    let Ok(pk_bytes) = public_key.try_into() else {
        return false;
    };
    let Ok(sig_bytes) = signature.try_into() else {
        return false;
    };
    let pk = bssl_crypto::ed25519::PublicKey::from_bytes(pk_bytes);
    pk.verify(message, sig_bytes).is_ok()
}

/// Verifies an ECDSA P-256 signature (ASN.1 DER format) over `message` using
/// BoringSSL.
#[must_use]
pub fn verify_ecdsa_p256_signature(public_key: &[u8], signature: &[u8], message: &[u8]) -> bool {
    let Some(pk) =
        bssl_crypto::ecdsa::PublicKey::<bssl_crypto::ec::P256>::from_x962_compressed(public_key)
    else {
        return false;
    };
    pk.verify(message, signature).is_ok()
}

/// Dispatches signature verification based on `signature_type`.
#[must_use]
pub fn verify_signature(
    signature_type: SignatureType,
    public_key: &[u8],
    signature: &[u8],
    message: &[u8],
) -> bool {
    match signature_type {
        SignatureType::Ed25519 => verify_ed25519_signature(public_key, signature, message),
        SignatureType::EcdsaP256SHA256 => {
            verify_ecdsa_p256_signature(public_key, signature, message)
        }
        SignatureType::Unknown => true,
    }
}

/// Streaming SHA-512 hasher using BoringSSL.
pub struct Sha512Hasher {
    inner: bssl_crypto::digest::Sha512,
}

impl Default for Sha512Hasher {
    fn default() -> Self {
        Self::new()
    }
}

impl Sha512Hasher {
    pub fn new() -> Self {
        Self { inner: bssl_crypto::digest::Sha512::new() }
    }

    pub fn update(&mut self, chunk: &[u8]) {
        self.inner.update(chunk);
    }

    pub fn finish(self) -> [u8; 64] {
        self.inner.digest()
    }
}
