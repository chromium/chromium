// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use athm::{Params, PrivateKey, PublicKey, PublicKeyProof, Token};

use crate::types::{AthmStatus, TokenRequest};

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
    pub fn try_new(n_buckets: u8, deployment_id: &[u8]) -> Result<Self, AthmStatus> {
        let params =
            Params::new(n_buckets, deployment_id.to_vec()).map_err(|_| AthmStatus::InvalidInput)?;
        let (private_key, public_key, public_key_proof) = athm::key_gen(&params);
        Ok(AthmKeyMaterial { params, private_key, public_key, public_key_proof })
    }

    /// Returns the serialized public key bytes.
    #[must_use]
    pub fn public_key(&self) -> Vec<u8> {
        crate::types::encode(&self.public_key)
    }

    /// Returns the serialized public key proof bytes.
    #[must_use]
    pub fn public_key_proof(&self) -> Vec<u8> {
        crate::types::encode(&self.public_key_proof)
    }

    /// Signs a client `request`, embedding `hidden_metadata` (which must be
    /// `< n_buckets`).
    pub fn issue(
        &self,
        request: &TokenRequest,
        hidden_metadata: u8,
    ) -> Result<Vec<u8>, AthmStatus> {
        athm::token_response(
            &self.private_key,
            &self.public_key,
            &request.0,
            hidden_metadata,
            &self.params,
        )
        .map(|response| crate::types::encode(&response))
        .map_err(|_| AthmStatus::OperationFailed)
    }

    /// Verifies a finalized `token` and recovers the hidden metadata.
    pub fn verify(&self, token: &[u8]) -> Result<u8, AthmStatus> {
        use athm::Decodable;
        let token = Token::decode(token).map_err(|_| AthmStatus::InvalidInput)?;
        athm::verify_token(&self.private_key, &token, &self.params)
            .into_option()
            .ok_or(AthmStatus::OperationFailed)
    }
}
