// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Crubit FFI over the Anonymous Tokens with Hidden Metadata (ATHM) crate
//! (`//third_party/anonymous_tokens:athm`), exposing the issuer- and
//! client-side protocol operations to C++. The elliptic-curve and hashing
//! primitives run on Chromium's in-tree BoringSSL via the `bssl_sys` bindings.
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
//! Malformed serialized inputs are rejected gracefully: the BoringSSL backend's
//! point decoder validates each compressed point and surfaces `InvalidInput`
//! rather than aborting (verified empirically against tampered, truncated, and
//! garbage tokens). The constant-time arithmetic path does `assert!` on a
//! non-curve point, but it only runs on points that already decoded
//! successfully, so it is not reachable from untrusted boundary input.
//!
//! TODO: These entry points decode untrusted serialized input and are prime
//! fuzz targets; add cargo-fuzz/libfuzzer coverage for them in a follow-up.

use athm::{
    finalize_token, key_gen, token_request, token_response, verify_token, Decodable, Encodable,
    Params, PrivateKey, PublicKey, PublicKeyProof, Token, TokenContext, TokenResponse,
};

/// Error outcome of a bridge operation.
#[repr(u8)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum AthmStatus {
    /// One of the serialized inputs failed to decode.
    InvalidInput = 1,
    /// Inputs decoded, but the cryptographic operation failed (e.g. invalid
    /// proof, out-of-range metadata, or a token that did not verify).
    OperationFailed = 2,
}

/// Blinded token request from the client.
#[derive(Clone)]
pub struct TokenRequest(pub(crate) athm::TokenRequest);

impl TokenRequest {
    /// Decodes a serialized token request from wire bytes.
    pub fn decode(bytes: &[u8]) -> Result<Self, AthmStatus> {
        athm::TokenRequest::decode(bytes).map(TokenRequest).map_err(|_| AthmStatus::InvalidInput)
    }

    /// Serializes the token request to wire bytes.
    pub fn to_bytes(&self) -> Vec<u8> {
        encode(&self.0)
    }
}

/// Public parameters for the ATHM protocol, derived deterministically by each
/// party from `(n_buckets, deployment_id)`. Parameters are never sent over the
/// wire.
#[derive(Clone)]
pub struct AthmParameters(pub(crate) Params);

impl AthmParameters {
    /// Creates parameters for `n_buckets` metadata buckets under
    /// `deployment_id`.
    pub fn create(n_buckets: u8, deployment_id: &[u8]) -> Result<Self, AthmStatus> {
        Params::new(n_buckets, deployment_id.to_vec())
            .map(AthmParameters)
            .map_err(|_| AthmStatus::InvalidInput)
    }

    /// Returns the expected wire size in bytes of a single `TokenResponse`.
    pub fn token_response_size(&self) -> usize {
        token_response_size_for_buckets(self.0.n_buckets)
    }
}

/// Server (issuer) key material.
#[derive(Clone)]
pub struct AthmKeyMaterial {
    pub(crate) params: Params,
    pub(crate) private_key: PrivateKey,
    pub(crate) public_key: PublicKey,
    pub(crate) public_key_proof: PublicKeyProof,
}

impl AthmKeyMaterial {
    /// Generates issuer key material for `n_buckets` metadata buckets under
    /// `deployment_id`.
    pub fn key_gen(n_buckets: u8, deployment_id: &[u8]) -> Result<Self, AthmStatus> {
        let params =
            Params::new(n_buckets, deployment_id.to_vec()).map_err(|_| AthmStatus::InvalidInput)?;
        let (private_key, public_key, public_key_proof) = key_gen(&params);
        Ok(AthmKeyMaterial { params, private_key, public_key, public_key_proof })
    }

    pub fn public_key(&self) -> Vec<u8> {
        encode(&self.public_key)
    }

    pub fn public_key_proof(&self) -> Vec<u8> {
        encode(&self.public_key_proof)
    }

    /// Signs a client `request`, embedding `hidden_metadata` (which must be
    /// `< n_buckets`).
    pub fn issue(
        &self,
        request: &TokenRequest,
        hidden_metadata: u8,
    ) -> Result<Vec<u8>, AthmStatus> {
        token_response(
            &self.private_key,
            &self.public_key,
            &request.0,
            hidden_metadata,
            &self.params,
        )
        .map(|response| encode(&response))
        .map_err(|_| AthmStatus::OperationFailed)
    }

    /// Verifies a finalized `token` and recovers the hidden metadata.
    pub fn verify(&self, token: &[u8]) -> Result<u8, AthmStatus> {
        let Ok(token) = Token::decode(token) else {
            return Err(AthmStatus::InvalidInput);
        };
        verify_token(&self.private_key, &token, &self.params)
            .into_option()
            .ok_or(AthmStatus::OperationFailed)
    }
}

/// Result of the client building a token request. `context` is the client's
/// secret state that it must retain until it finalizes the token.
#[derive(Clone)]
pub struct AthmClientRequest {
    pub(crate) context: TokenContext,
    pub(crate) request: TokenRequest,
}

impl AthmClientRequest {
    /// Builds a blinded token request from the issuer's public material and
    /// protocol parameters.
    pub fn create(
        public_key: &[u8],
        public_key_proof: &[u8],
        params: &AthmParameters,
    ) -> Result<Self, AthmStatus> {
        let (Ok(public_key), Ok(proof)) =
            (PublicKey::decode(public_key), PublicKeyProof::decode(public_key_proof))
        else {
            return Err(AthmStatus::InvalidInput);
        };
        token_request(&public_key, &proof, &params.0)
            .map(|(context, request)| AthmClientRequest { context, request: TokenRequest(request) })
            .map_err(|_| AthmStatus::OperationFailed)
    }

    /// Unblinds the issuer `response` into a finalized token using the stored
    /// context and request.
    pub fn finalize(
        &self,
        public_key: &[u8],
        response: &[u8],
        params: &AthmParameters,
    ) -> Result<Vec<u8>, AthmStatus> {
        let (Ok(public_key), Ok(response)) =
            (PublicKey::decode(public_key), TokenResponse::decode(response, &params.0))
        else {
            return Err(AthmStatus::InvalidInput);
        };
        finalize_token(&self.context, &public_key, &self.request.0, &response, &params.0)
            .map(|token| encode(&token))
            .map_err(|_| AthmStatus::OperationFailed)
    }

    /// Serialized wire encoding of the blinded token request to send to the
    /// issuer.
    pub fn request(&self) -> Vec<u8> {
        self.request.to_bytes()
    }
}

fn encode<T: Encodable>(value: &T) -> Vec<u8> {
    let mut out = Vec::new();
    value.encode(&mut out);
    out
}

// Wire sizes in bytes for P-256 field elements and points in the BoringSSL
// backend.
const SCALAR_SIZE: usize = 32;
const POINT_SIZE: usize = 33; // SEC-1 compressed format: 1-byte header + 32-byte X-coordinate

// Number of dynamic challenge/response scalars per metadata bucket in
// IssuanceProof (e_vec and a_vec).
const SCALARS_PER_METADATA_BUCKET: usize = 2;

/// Computes the wire size of an ATHM `IssuanceProof` for `n_buckets`:
/// - `big_c`: 1 Point
/// - `a_d`, `a_rho`, `a_w`: 3 fixed Scalars
/// - `e_vec`, `a_vec`: `n_buckets` Scalars each (2 * `n_buckets` Scalars)
const fn issuance_proof_size(n_buckets: u8) -> usize {
    POINT_SIZE + (3 + (n_buckets as usize) * SCALARS_PER_METADATA_BUCKET) * SCALAR_SIZE
}

/// Computes the wire size of an ATHM `TokenResponse` for `n_buckets`:
/// - `big_u`, `big_v`: 2 Points
/// - `ts`: 1 Scalar
/// - `issuance_proof`: `issuance_proof_size(n_buckets)`
const fn token_response_size_for_buckets(n_buckets: u8) -> usize {
    2 * POINT_SIZE + SCALAR_SIZE + issuance_proof_size(n_buckets)
}
