// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

pub const MAGIC_BYTES: &[u8] = b"\xF0\x9F\x96\x8B\xF0\x9F\x93\xA6";
pub const V2_VERSION_BYTES: &[u8] = b"2b\0\0";
pub const WEB_BUNDLE_ID_ATTRIBUTE_NAME: &str = "webBundleId";
pub const ED25519_PUBLIC_KEY_ATTRIBUTE_NAME: &str = "ed25519PublicKey";
pub const ECDSA_P256_PUBLIC_KEY_ATTRIBUTE_NAME: &str = "ecdsaP256SHA256PublicKey";
pub const TOP_LEVEL_ARRAY_LENGTH: usize = 4;

/// Length in bytes of a compressed ECDSA P-256 public key (SEC 1 / X9.62
/// format: 0x02 or 0x03 tag prefix followed by the 32-byte X coordinate).
pub const ECDSA_P256_PUBLIC_KEY_LEN: usize = 33;

/// Minimum and maximum valid byte lengths of an ASN.1 DER-encoded ECDSA P-256
/// signature sequence containing integers (r, s).
pub const ECDSA_P256_SIGNATURE_MIN_LEN: usize = 64;
pub const ECDSA_P256_SIGNATURE_MAX_LEN: usize = 72;
