// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use athm::{PublicKey, PublicKeyProof, TokenContext, TokenResponse};

use crate::types::{AthmParameters, AthmStatus, TokenRequest};

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
    pub fn try_new(
        public_key: &[u8],
        public_key_proof: &[u8],
        params: &AthmParameters,
    ) -> Result<Self, AthmStatus> {
        use athm::Decodable;
        let public_key = PublicKey::decode(public_key).map_err(|_| AthmStatus::InvalidInput)?;
        let proof = PublicKeyProof::decode(public_key_proof).map_err(|_| AthmStatus::InvalidInput)?;
        athm::token_request(&public_key, &proof, &params.0)
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
        use athm::Decodable;
        let public_key = PublicKey::decode(public_key).map_err(|_| AthmStatus::InvalidInput)?;
        let response = TokenResponse::decode(response, &params.0).map_err(|_| AthmStatus::InvalidInput)?;
        athm::finalize_token(&self.context, &public_key, &self.request.0, &response, &params.0)
            .map(|token| crate::types::encode(&token))
            .map_err(|_| AthmStatus::OperationFailed)
    }

    /// Serialized wire encoding of the blinded token request to send to the
    /// issuer.
    #[must_use]
    pub fn request(&self) -> Vec<u8> {
        self.request.to_bytes()
    }
}
