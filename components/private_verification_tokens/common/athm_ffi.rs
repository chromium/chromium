// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! cxx FFI bridge over the Anonymous Tokens with Hidden Metadata (ATHM) crate
//! (`//third_party/anonymous_tokens:athm`), exposing the issuer- and
//! client-side protocol operations to C++. The elliptic-curve and hashing
//! primitives run on Chromium's in-tree BoringSSL via the `bssl_sys` bindings.
//!
//! All values cross the boundary as the serialized ATHM wire encodings (see the
//! ATHM spec for the encoding). The issuer-side functions hold the secret key;
//! the client-side functions use only public material.
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
    Params, PrivateKey, PublicKey, PublicKeyProof, Token, TokenContext, TokenRequest,
    TokenResponse,
};

#[cxx::bridge(namespace = "private_verification_tokens")]
mod ffi {
    /// Outcome of a bridge operation.
    enum AthmStatus {
        /// The operation succeeded; the accompanying payload is valid.
        Ok,
        /// One of the serialized inputs failed to decode.
        InvalidInput,
        /// Inputs decoded, but the cryptographic operation failed (e.g. invalid
        /// proof, out-of-range metadata, or a token that did not verify).
        OperationFailed,
    }

    // Client params. Field is the serialized wire encoding of the
    // ATHM params object. Valid only when `status` is `OK`.
    struct AthmClientParams {
        status: AthmStatus,
        params: Vec<u8>,
    }

    /// Server (issuer) key material. Each field is the serialized wire encoding
    /// of the corresponding ATHM object. Valid only when `status` is `Ok`.
    struct AthmKeyMaterial {
        status: AthmStatus,
        params: Vec<u8>,
        private_key: Vec<u8>,
        public_key: Vec<u8>,
        public_key_proof: Vec<u8>,
    }

    /// A serialized-bytes result (used for issuance responses and tokens). The
    /// `bytes` are meaningful only when `status` is `Ok`.
    struct AthmBytesResult {
        status: AthmStatus,
        bytes: Vec<u8>,
    }

    /// Result of the client building a token request. `context` is the client's
    /// secret state that it must retain until it finalizes the token.
    struct AthmClientRequest {
        status: AthmStatus,
        context: Vec<u8>,
        request: Vec<u8>,
    }

    /// Result of verifying a token. `metadata` is meaningful only when `status`
    /// is `Ok`.
    struct AthmVerifyResult {
        status: AthmStatus,
        metadata: u8,
    }

    extern "Rust" {
        // --- Issuer side (holds the secret key) ---

        /// Generates issuer key material for `n_buckets` metadata buckets under
        /// `deployment_id`.
        fn athm_key_gen(n_buckets: u8, deployment_id: &[u8]) -> AthmKeyMaterial;

        /// Signs a client `request`, embedding `hidden_metadata` (which must be
        /// `< n_buckets`). `private_key` and `public_key` must come from the
        /// same `athm_key_gen` call.
        fn athm_issue(
            private_key: &[u8],
            public_key: &[u8],
            request: &[u8],
            hidden_metadata: u8,
            params: &[u8],
        ) -> AthmBytesResult;

        /// Verifies a finalized `token` and recovers the hidden metadata.
        fn athm_verify(private_key: &[u8], token: &[u8], params: &[u8]) -> AthmVerifyResult;

        // --- Client side (only public material) ---

        /// Generates client params for `n_buckets` metadata buckets under
        /// `deployment_id`.
        fn athm_client_params(n_buckets: u8, deployment_id: &[u8]) -> AthmClientParams;

        /// Builds a blinded token request from the issuer's public material.
        fn athm_client_request(
            public_key: &[u8],
            public_key_proof: &[u8],
            params: &[u8],
        ) -> AthmClientRequest;

        /// Unblinds the issuer `response` into a finalized token.
        fn athm_client_finalize(
            context: &[u8],
            public_key: &[u8],
            request: &[u8],
            response: &[u8],
            params: &[u8],
        ) -> AthmBytesResult;
    }
}

use ffi::AthmStatus;

fn encode<T: Encodable>(value: &T) -> Vec<u8> {
    let mut out = Vec::new();
    value.encode(&mut out);
    out
}

// Error-result constructors: one per result struct, so each struct's empty
// "error" shape lives in a single place.
fn err_key_material(status: AthmStatus) -> ffi::AthmKeyMaterial {
    ffi::AthmKeyMaterial {
        status,
        params: Vec::new(),
        private_key: Vec::new(),
        public_key: Vec::new(),
        public_key_proof: Vec::new(),
    }
}

fn err_bytes(status: AthmStatus) -> ffi::AthmBytesResult {
    ffi::AthmBytesResult { status, bytes: Vec::new() }
}

fn err_request(status: AthmStatus) -> ffi::AthmClientRequest {
    ffi::AthmClientRequest { status, context: Vec::new(), request: Vec::new() }
}

fn err_verify(status: AthmStatus) -> ffi::AthmVerifyResult {
    ffi::AthmVerifyResult { status, metadata: 0 }
}

fn err_client_params(status: AthmStatus) -> ffi::AthmClientParams {
    ffi::AthmClientParams { status, params: Vec::new() }
}

fn athm_client_params(n_buckets: u8, deployment_id: &[u8]) -> ffi::AthmClientParams {
    let params = match Params::new(n_buckets, deployment_id.to_vec()) {
        Ok(params) => params,
        Err(_) => return err_client_params(AthmStatus::InvalidInput),
    };
    ffi::AthmClientParams { status: AthmStatus::Ok, params: encode(&params) }
}

fn athm_key_gen(n_buckets: u8, deployment_id: &[u8]) -> ffi::AthmKeyMaterial {
    let params = match Params::new(n_buckets, deployment_id.to_vec()) {
        Ok(params) => params,
        Err(_) => return err_key_material(AthmStatus::InvalidInput),
    };
    let (private_key, public_key, proof) = key_gen(&params);
    ffi::AthmKeyMaterial {
        status: AthmStatus::Ok,
        params: encode(&params),
        private_key: encode(&private_key),
        public_key: encode(&public_key),
        public_key_proof: encode(&proof),
    }
}

fn athm_issue(
    private_key: &[u8],
    public_key: &[u8],
    request: &[u8],
    hidden_metadata: u8,
    params: &[u8],
) -> ffi::AthmBytesResult {
    let (Ok(params), Ok(private_key), Ok(public_key), Ok(request)) = (
        Params::decode(params),
        PrivateKey::decode(private_key),
        PublicKey::decode(public_key),
        TokenRequest::decode(request),
    ) else {
        return err_bytes(AthmStatus::InvalidInput);
    };
    match token_response(&private_key, &public_key, &request, hidden_metadata, &params) {
        Ok(response) => ffi::AthmBytesResult { status: AthmStatus::Ok, bytes: encode(&response) },
        Err(_) => err_bytes(AthmStatus::OperationFailed),
    }
}

fn athm_verify(private_key: &[u8], token: &[u8], params: &[u8]) -> ffi::AthmVerifyResult {
    let (Ok(params), Ok(private_key), Ok(token)) =
        (Params::decode(params), PrivateKey::decode(private_key), Token::decode(token))
    else {
        return err_verify(AthmStatus::InvalidInput);
    };
    match verify_token(&private_key, &token, &params).into_option() {
        Some(metadata) => ffi::AthmVerifyResult { status: AthmStatus::Ok, metadata },
        None => err_verify(AthmStatus::OperationFailed),
    }
}

fn athm_client_request(
    public_key: &[u8],
    public_key_proof: &[u8],
    params: &[u8],
) -> ffi::AthmClientRequest {
    let (Ok(params), Ok(public_key), Ok(proof)) = (
        Params::decode(params),
        PublicKey::decode(public_key),
        PublicKeyProof::decode(public_key_proof),
    ) else {
        return err_request(AthmStatus::InvalidInput);
    };
    match token_request(&public_key, &proof, &params) {
        Ok((context, request)) => ffi::AthmClientRequest {
            status: AthmStatus::Ok,
            context: encode(&context),
            request: encode(&request),
        },
        Err(_) => err_request(AthmStatus::OperationFailed),
    }
}

fn athm_client_finalize(
    context: &[u8],
    public_key: &[u8],
    request: &[u8],
    response: &[u8],
    params: &[u8],
) -> ffi::AthmBytesResult {
    // `TokenResponse` decodes against `params` (it is sized by `n_buckets`), so
    // decode `params` first and reuse it.
    let Ok(params) = Params::decode(params) else {
        return err_bytes(AthmStatus::InvalidInput);
    };
    let (Ok(context), Ok(public_key), Ok(request), Ok(response)) = (
        TokenContext::decode(context),
        PublicKey::decode(public_key),
        TokenRequest::decode(request),
        TokenResponse::decode(response, &params),
    ) else {
        return err_bytes(AthmStatus::InvalidInput);
    };
    match finalize_token(&context, &public_key, &request, &response, &params) {
        Ok(token) => ffi::AthmBytesResult { status: AthmStatus::Ok, bytes: encode(&token) },
        Err(_) => err_bytes(AthmStatus::OperationFailed),
    }
}
