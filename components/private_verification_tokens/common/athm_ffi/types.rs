// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use athm::{Params, PublicKey, PublicKeyProof};

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
        use athm::Decodable;
        athm::TokenRequest::decode(bytes).map(TokenRequest).map_err(|_| AthmStatus::InvalidInput)
    }

    /// Serializes the token request to wire bytes.
    #[must_use]
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
    pub fn try_new(n_buckets: u8, deployment_id: &[u8]) -> Result<Self, AthmStatus> {
        Params::new(n_buckets, deployment_id.to_vec())
            .map(AthmParameters)
            .map_err(|_| AthmStatus::InvalidInput)
    }

    /// Returns the expected wire size in bytes of a single `TokenResponse`.
    #[must_use]
    pub fn token_response_size(&self) -> usize {
        core::mem::size_of::<PublicKey>()
            + (2 + self.0.n_buckets as usize) * core::mem::size_of::<PublicKeyProof>()
    }
}

pub(crate) fn encode<T: athm::Encodable>(value: &T) -> Vec<u8> {
    let mut out = Vec::new();
    value.encode(&mut out);
    out
}
