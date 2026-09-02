// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Common Privacy Pass framing, framing traits, and token state types.

use athm::{Decodable, Encodable, PublicKey, Token, TokenContext, TokenRequest};

pub const ATHM_TOKEN_TYPE: [u8; 2] = (0xC07Eu16).to_be_bytes();

pub trait TruncatedKeyId {
    fn truncated_key_id(&self) -> u8;
}

impl TruncatedKeyId for PublicKey {
    fn truncated_key_id(&self) -> u8 {
        self.key_id()[31]
    }
}

#[repr(u8)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UnframeError {
    UnexpectedLength,
    UnexpectedType,
    MultipleKeyIds,
    DecodeFailure,
}

/// Set of operations for framing and unframing ATHM messages. `K` is the type
/// of the key_id metadata associated with each framed message.
pub trait Frame<K> {
    fn framed_size() -> usize;
    fn frame(&self, key: &PublicKey) -> Vec<u8>;
    fn unframe(input: &[u8]) -> Result<(Self, K), UnframeError>
    where
        Self: Sized;
}

// Token type + truncated key id + encoded token request body.
pub const TOKEN_REQUEST_FRAMED_SIZE: usize = std::mem::size_of_val(&ATHM_TOKEN_TYPE)
    + std::mem::size_of::<u8>()
    + TokenRequest::encoded_size();

/// Implements the framing of token requests as outlined in https://www.rfc-editor.org/rfc/rfc9578.html#section-5.1-8
impl Frame<u8> for TokenRequest {
    fn framed_size() -> usize {
        TOKEN_REQUEST_FRAMED_SIZE
    }

    fn frame(&self, key: &PublicKey) -> Vec<u8> {
        let mut out: Vec<u8> = Vec::with_capacity(Self::framed_size());
        out.extend_from_slice(&ATHM_TOKEN_TYPE);
        out.push(key.truncated_key_id());
        self.encode(&mut out);
        out
    }

    fn unframe(input: &[u8]) -> Result<(Self, u8), UnframeError> {
        let byte_array: &[u8; TOKEN_REQUEST_FRAMED_SIZE] =
            input.try_into().map_err(|_| UnframeError::UnexpectedLength)?;
        if byte_array[0..2] != ATHM_TOKEN_TYPE {
            return Err(UnframeError::UnexpectedType);
        }
        let truncated_key_id = byte_array[2];
        Self::decode(&byte_array[3..])
            .map(|request| (request, truncated_key_id))
            .map_err(|_| UnframeError::DecodeFailure)
    }
}

// Token type + full SHA256 KeyID + Encoded token body
pub const TOKEN_FRAMED_SIZE: usize =
    std::mem::size_of_val(&ATHM_TOKEN_TYPE) + 32 + Token::encoded_size();

impl Frame<[u8; 32]> for Token {
    fn framed_size() -> usize {
        TOKEN_FRAMED_SIZE
    }

    fn frame(&self, key: &PublicKey) -> Vec<u8> {
        let mut out: Vec<u8> = Vec::with_capacity(Self::framed_size());
        out.extend_from_slice(&ATHM_TOKEN_TYPE);
        out.extend_from_slice(&key.key_id());
        self.encode(&mut out);
        out
    }

    fn unframe(input: &[u8]) -> Result<(Self, [u8; 32]), UnframeError> {
        let byte_array: &[u8; TOKEN_FRAMED_SIZE] =
            input.try_into().map_err(|_| UnframeError::UnexpectedLength)?;
        if byte_array[0..2] != ATHM_TOKEN_TYPE {
            return Err(UnframeError::UnexpectedType);
        }
        let key_id: [u8; 32] = byte_array[2..34].try_into().unwrap();
        Self::decode(&byte_array[34..])
            .map(|token| (token, key_id))
            .map_err(|_| UnframeError::DecodeFailure)
    }
}

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

#[derive(Clone)]
pub struct TokenState {
    pub(crate) context: TokenContext,
    pub request: TokenRequest,
}
