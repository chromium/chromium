// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Client-side operations for Privacy Pass ATHM, including batch request
//! management.

use athm::{Decodable, Params, PublicKey, PublicKeyProof, Token, TokenResponse};

use crate::privacy_pass_common::{
    AthmStatus, Frame, TokenState, UnframeError, TOKEN_REQUEST_FRAMED_SIZE,
};
use crate::privacy_pass_issuer::PrivacyPassAthmIssuer;

/// Client key material and cryptographic parameters.
#[derive(Clone)]
pub struct PrivacyPassAthmClient {
    public_key: PublicKey,
    public_key_proof: PublicKeyProof,
    params: Params,
}

impl PrivacyPassAthmClient {
    pub fn try_new(
        public_key: &[u8],
        public_key_proof: &[u8],
        num_buckets: u8,
        deployment_id: &[u8],
    ) -> Result<Self, AthmStatus> {
        let params = Params::new(num_buckets, deployment_id.to_vec())
            .map_err(|_| AthmStatus::InvalidInput)?;
        let public_key = PublicKey::decode(public_key).map_err(|_| AthmStatus::InvalidInput)?;
        let public_key_proof =
            PublicKeyProof::decode(public_key_proof).map_err(|_| AthmStatus::InvalidInput)?;
        Ok(Self { public_key, public_key_proof, params })
    }

    pub fn new(issuer: &PrivacyPassAthmIssuer) -> Self {
        PrivacyPassAthmClient {
            public_key: issuer.public_key.clone(),
            public_key_proof: issuer.public_key_proof.clone(),
            params: issuer.params.clone(),
        }
    }

    pub fn new_token_state(&self) -> Result<TokenState, AthmStatus> {
        athm::token_request(&self.public_key, &self.public_key_proof, &self.params)
            .map(|(context, request)| TokenState { context, request })
            .map_err(|_| AthmStatus::OperationFailed)
    }

    pub fn finalize_request(
        &self,
        state: TokenState,
        response: TokenResponse,
    ) -> Result<Token, AthmStatus> {
        athm::finalize_token_generic(
            &state.context,
            &self.public_key,
            &state.request,
            &response,
            &self.params,
        )
        .map_err(|_| AthmStatus::OperationFailed)
    }
}

/// Parsed token responses as seen by the client.
#[derive(Clone)]
pub struct TokenResponseInboundBatch {
    pub responses: Vec<TokenResponse>,
}

impl TokenResponseInboundBatch {
    pub fn from_bytes(
        input: &[u8],
        client: &PrivacyPassAthmClient,
    ) -> Result<TokenResponseInboundBatch, UnframeError> {
        let single_response_size = TokenResponse::encoded_size_for_buckets(client.params.n_buckets);
        if single_response_size == 0
            || input.is_empty()
            || !input.len().is_multiple_of(single_response_size)
        {
            return Err(UnframeError::UnexpectedLength);
        }
        let responses = input
            .chunks_exact(single_response_size)
            .map(|chunk| {
                TokenResponse::decode(chunk, &client.params)
                    .map_err(|_| UnframeError::DecodeFailure)
            })
            .collect::<Result<Vec<_>, _>>()?;
        Ok(TokenResponseInboundBatch { responses })
    }

    pub fn finalize_batch(
        self,
        states: Vec<TokenState>,
        client: &PrivacyPassAthmClient,
    ) -> Result<Vec<Token>, AthmStatus> {
        if states.len() != self.responses.len() {
            return Err(AthmStatus::InvalidInput);
        }
        states
            .into_iter()
            .zip(self.responses)
            .map(|(state, response)| client.finalize_request(state, response))
            .collect()
    }
}

#[derive(Clone)]
pub struct PrivacyPassBatchClient {
    client: PrivacyPassAthmClient,
    states: Vec<TokenState>,
    request_body: Vec<u8>,
}

impl PrivacyPassBatchClient {
    pub fn try_new(
        public_key: &[u8],
        public_key_proof: &[u8],
        num_buckets: u8,
        deployment_id: &[u8],
        version: u32,
        batch_size: usize,
    ) -> Result<Self, AthmStatus> {
        if batch_size == 0 {
            return Err(AthmStatus::InvalidInput);
        }
        let client = PrivacyPassAthmClient::try_new(
            public_key,
            public_key_proof,
            num_buckets,
            deployment_id,
        )?;
        let mut states = Vec::with_capacity(batch_size);
        let mut request_body = Vec::with_capacity(batch_size * TOKEN_REQUEST_FRAMED_SIZE + 4);
        for _ in 0..batch_size {
            let state = client.new_token_state()?;
            request_body.extend_from_slice(&state.request.frame(&client.public_key));
            states.push(state);
        }
        request_body.extend_from_slice(&version.to_be_bytes());
        Ok(PrivacyPassBatchClient { client, states, request_body })
    }

    pub fn request_body(&self) -> &[u8] {
        &self.request_body
    }

    pub fn finalize(&mut self, response_body: &[u8]) -> Result<Vec<Vec<u8>>, AthmStatus> {
        let states = std::mem::take(&mut self.states);
        if states.is_empty() {
            return Err(AthmStatus::OperationFailed);
        }
        let inbound = TokenResponseInboundBatch::from_bytes(response_body, &self.client)
            .map_err(|_| AthmStatus::InvalidInput)?;
        let tokens = inbound.finalize_batch(states, &self.client)?;
        Ok(tokens.iter().map(|t| t.frame(&self.client.public_key)).collect())
    }
}
