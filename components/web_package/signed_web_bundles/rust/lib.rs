// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![no_std]
#![forbid(unsafe_code)]

extern crate alloc;

mod constants;
mod integrity_block;
mod signature_verifier;
mod types;

pub use constants::{
    ECDSA_P256_PUBLIC_KEY_ATTRIBUTE_NAME, ECDSA_P256_PUBLIC_KEY_LEN, ECDSA_P256_SIGNATURE_MAX_LEN,
    ECDSA_P256_SIGNATURE_MIN_LEN, ED25519_PUBLIC_KEY_ATTRIBUTE_NAME, MAGIC_BYTES,
    TOP_LEVEL_ARRAY_LENGTH, V2_VERSION_BYTES, WEB_BUNDLE_ID_ATTRIBUTE_NAME,
};
pub use integrity_block::parse_integrity_block;
pub use signature_verifier::{
    create_empty_integrity_block_cbor, create_signature_payload, verify_ecdsa_p256_signature,
    verify_ed25519_signature, verify_signature, Sha512Hasher,
};
pub use types::{IntegrityBlock, ParseError, SignatureStackEntry, SignatureType};
