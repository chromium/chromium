// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Issuer-side operations for Privacy Pass ATHM.

use athm::{
    Encodable, Params, PrivateKey, PublicKey, PublicKeyProof, Token, TokenRequest, TokenResponse,
};

use crate::privacy_pass_common::{
    AthmStatus, Frame, TruncatedKeyId, UnframeError, TOKEN_REQUEST_FRAMED_SIZE,
};

/// Parsed results from a token issuance request as seen by the server.
#[derive(Clone)]
pub struct TokenRequestInboundBatch {
    pub version: u32,
    pub truncated_key_id: u8,
    pub requests: Vec<TokenRequest>,
}

impl TokenRequestInboundBatch {
    pub fn from_bytes(input: &[u8]) -> Result<TokenRequestInboundBatch, UnframeError> {
        let (request_batch_bytes, version_bytes) =
            input.split_last_chunk::<4>().ok_or(UnframeError::UnexpectedLength)?;
        let version = u32::from_be_bytes(*version_bytes);
        let (request_byte_chunks, remainder) =
            request_batch_bytes.as_chunks::<TOKEN_REQUEST_FRAMED_SIZE>();
        if !remainder.is_empty() || request_byte_chunks.is_empty() {
            return Err(UnframeError::UnexpectedLength);
        }
        let requests_and_key_ids: Vec<(TokenRequest, u8)> = request_byte_chunks
            .iter()
            .map(|request_bytes| TokenRequest::unframe(request_bytes))
            .collect::<Result<_, _>>()?;
        let (requests, key_ids): (Vec<TokenRequest>, Vec<u8>) =
            requests_and_key_ids.into_iter().unzip();
        let truncated_key_id = *key_ids.first().ok_or(UnframeError::UnexpectedLength)?;
        if !key_ids.into_iter().all(|x| x == truncated_key_id) {
            return Err(UnframeError::MultipleKeyIds);
        }

        Ok(TokenRequestInboundBatch { version, truncated_key_id, requests })
    }
}

#[derive(Clone)]
pub struct PrivacyPassAthmIssuer {
    private_key: PrivateKey,
    pub public_key: PublicKey,
    pub public_key_proof: PublicKeyProof,
    pub params: Params,
}

impl PrivacyPassAthmIssuer {
    pub fn try_new(num_buckets: u8, deployment_id: &[u8]) -> Result<Self, AthmStatus> {
        let params = Params::new(num_buckets, deployment_id.to_vec())
            .map_err(|_| AthmStatus::InvalidInput)?;
        let (private_key, public_key, public_key_proof) = athm::key_gen(&params);
        Ok(PrivacyPassAthmIssuer { params, private_key, public_key, public_key_proof })
    }

    pub fn public_key_bytes(&self) -> Vec<u8> {
        let mut out = Vec::new();
        self.public_key.encode(&mut out);
        out
    }

    pub fn public_key_proof_bytes(&self) -> Vec<u8> {
        let mut out = Vec::new();
        self.public_key_proof.encode(&mut out);
        out
    }

    pub fn issue(
        &self,
        request: TokenRequest,
        hidden_metadata: u8,
    ) -> Result<TokenResponse, AthmStatus> {
        athm::token_response(
            &self.private_key,
            &self.public_key,
            &request,
            hidden_metadata,
            &self.params,
        )
        .map_err(|_| AthmStatus::OperationFailed)
    }

    pub fn issue_batch(
        &self,
        batch: TokenRequestInboundBatch,
        hidden_metadata: u8,
    ) -> Result<Vec<u8>, AthmStatus> {
        if batch.truncated_key_id != self.public_key.truncated_key_id() {
            return Err(AthmStatus::InvalidInput);
        }
        let single_response_size = TokenResponse::encoded_size_for_buckets(self.params.n_buckets);
        let mut out = Vec::with_capacity(batch.requests.len() * single_response_size);
        for request in batch.requests {
            let response = self.issue(request, hidden_metadata)?;
            response.encode(&mut out);
        }
        Ok(out)
    }

    pub fn issue_batch_from_bytes(
        &self,
        request_body: &[u8],
        hidden_metadata: u8,
    ) -> Result<Vec<u8>, AthmStatus> {
        let inbound = TokenRequestInboundBatch::from_bytes(request_body)
            .map_err(|_| AthmStatus::InvalidInput)?;
        self.issue_batch(inbound, hidden_metadata)
    }

    pub fn verify(&self, token: Token) -> Result<u8, AthmStatus> {
        Option::<u8>::from(athm::verify_token(&self.private_key, &token, &self.params))
            .ok_or(AthmStatus::OperationFailed)
    }

    pub fn verify_wire_token(&self, wire_token: &[u8]) -> Result<u8, AthmStatus> {
        let (token, key_id) = Token::unframe(wire_token).map_err(|_| AthmStatus::InvalidInput)?;
        if key_id != self.public_key.key_id() {
            return Err(AthmStatus::InvalidInput);
        }
        self.verify(token)
    }
}
