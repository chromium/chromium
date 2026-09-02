// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Crubit FFI over the Anonymous Tokens with Hidden Metadata (ATHM) crate
//! (`//third_party/anonymous_tokens:athm`), exposing the issuer- and
//! client-side protocol operations to C++. The elliptic-curve and hashing
//! primitives run on Chromium's in-tree `BoringSSL` via the `bssl_sys`
//! bindings.
//!
//! Public keys, blinded requests, responses, and finalized tokens cross the
//! boundary as serialized ATHM wire encodings or rich types (`TokenRequest`,
//! `AthmClientRequest`). Protocol parameters (`AthmParameters`) are derived
//! locally by each party and never transmitted over the wire.
//!
//! This is the building block intended for a real Private Verification Tokens
//! issuance/redemption flow, so the contract distinguishes input-decoding
//! failures (`InvalidInput`) from cryptographic-operation failures
//! (`OperationFailed`) rather than collapsing both into one sentinel.
//!
//! Malformed serialized inputs are rejected gracefully: the `BoringSSL`
//! backend's point decoder validates each compressed point and surfaces
//! `InvalidInput` rather than aborting (verified empirically against tampered,
//! truncated, and garbage tokens). The constant-time arithmetic path does
//! `assert!` on a non-curve point, but it only runs on points that already
//! decoded successfully, so it is not reachable from untrusted boundary input.
//!
//! TODO: These entry points decode untrusted serialized input and are prime
//! fuzz targets; add cargo-fuzz/libfuzzer coverage for them in a follow-up.

pub mod privacy_pass_client;
pub mod privacy_pass_common;
pub mod privacy_pass_issuer;

pub use privacy_pass_client::*;
pub use privacy_pass_common::*;
pub use privacy_pass_issuer::*;
