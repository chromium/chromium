// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//components/private_verification_tokens/common/athm_ffi:athm_ffi";
}

use athm::{Decodable, Encodable, Token, TokenRequest};
use athm_ffi::{
    AthmStatus, Frame, PrivacyPassAthmClient, PrivacyPassAthmIssuer, PrivacyPassBatchClient,
    TokenRequestInboundBatch, TokenResponseInboundBatch, TruncatedKeyId, UnframeError,
};
use rust_gtest_interop::prelude::*;

const DEPLOYMENT_ID: &[u8] = b"privacy-pass-test-deployment";

#[gtest(PrivacyPassTest, IssuerAndClientCreationSuccess)]
fn test_issuer_and_client_creation_success() {
    let issuer = PrivacyPassAthmIssuer::try_new(4, DEPLOYMENT_ID);
    let issuer = issuer.unwrap();

    // Verify public key and proof can be encoded.
    let mut pk_bytes = Vec::new();
    issuer.public_key.encode(&mut pk_bytes);
    expect_false!(pk_bytes.is_empty());

    let mut proof_bytes = Vec::new();
    issuer.public_key_proof.encode(&mut proof_bytes);
    expect_false!(proof_bytes.is_empty());

    // Create client using try_new.
    let client = PrivacyPassAthmClient::try_new(&pk_bytes, &proof_bytes, 4, DEPLOYMENT_ID);
    expect_true!(client.is_ok());

    // Create client using convenience constructor from issuer.
    let _client_from_issuer = PrivacyPassAthmClient::new(&issuer);
}

#[gtest(PrivacyPassTest, IssuerCreationInvalidParams)]
fn test_issuer_creation_invalid_params() {
    // 0 buckets is invalid.
    let res_zero = PrivacyPassAthmIssuer::try_new(0, DEPLOYMENT_ID);
    expect_eq!(res_zero.err(), Some(AthmStatus::InvalidInput));

    // Deployment ID > 255 bytes is invalid in ATHM parameters.
    let long_id = vec![0x42; 256];
    let res_long = PrivacyPassAthmIssuer::try_new(4, &long_id);
    expect_eq!(res_long.err(), Some(AthmStatus::InvalidInput));
}

#[gtest(PrivacyPassTest, ClientCreationInvalidParams)]
fn test_client_creation_invalid_params() {
    let issuer = PrivacyPassAthmIssuer::try_new(4, DEPLOYMENT_ID).unwrap();
    let mut pk_bytes = Vec::new();
    issuer.public_key.encode(&mut pk_bytes);
    let mut proof_bytes = Vec::new();
    issuer.public_key_proof.encode(&mut proof_bytes);

    // Invalid public key bytes.
    let res_bad_pk = PrivacyPassAthmClient::try_new(b"invalid_pk", &proof_bytes, 4, DEPLOYMENT_ID);
    expect_eq!(res_bad_pk.err(), Some(AthmStatus::InvalidInput));

    // Invalid proof bytes.
    let res_bad_proof =
        PrivacyPassAthmClient::try_new(&pk_bytes, b"invalid_proof", 4, DEPLOYMENT_ID);
    expect_eq!(res_bad_proof.err(), Some(AthmStatus::InvalidInput));

    // 0 buckets.
    let res_zero_buckets =
        PrivacyPassAthmClient::try_new(&pk_bytes, &proof_bytes, 0, DEPLOYMENT_ID);
    expect_eq!(res_zero_buckets.err(), Some(AthmStatus::InvalidInput));

    // Overlong deployment ID.
    let long_id = vec![0x42; 256];
    let res_long_id = PrivacyPassAthmClient::try_new(&pk_bytes, &proof_bytes, 4, &long_id);
    expect_eq!(res_long_id.err(), Some(AthmStatus::InvalidInput));
}

#[gtest(PrivacyPassTest, IssuanceAndVerificationRoundtrip)]
fn test_issuance_and_verification_roundtrip() {
    let num_buckets = 4;
    let issuer = PrivacyPassAthmIssuer::try_new(num_buckets, DEPLOYMENT_ID).unwrap();
    let client = PrivacyPassAthmClient::new(&issuer);

    for metadata in 0..num_buckets {
        let state = client.new_token_state();
        let state = state.unwrap();

        let response = issuer.issue(state.request.clone(), metadata);
        let response = response.unwrap();

        let token = client.finalize_request(state, response);
        let token = token.unwrap();

        let recovered_metadata = issuer.verify(token);
        expect_eq!(recovered_metadata, Ok(metadata));
    }
}

#[gtest(PrivacyPassTest, MultipleBucketConfigurations)]
fn test_multiple_bucket_configurations() {
    for num_buckets in [1, 2, 8, 16] {
        let issuer = PrivacyPassAthmIssuer::try_new(num_buckets, DEPLOYMENT_ID).unwrap();
        let client = PrivacyPassAthmClient::new(&issuer);

        let test_metadata = num_buckets - 1;
        let state = client.new_token_state().unwrap();
        let response = issuer.issue(state.request.clone(), test_metadata).unwrap();
        let token = client.finalize_request(state, response).unwrap();

        let recovered = issuer.verify(token);
        expect_eq!(recovered, Ok(test_metadata));
    }
}

#[gtest(PrivacyPassTest, RequestBlindingRandomness)]
fn test_request_blinding_randomness() {
    let issuer = PrivacyPassAthmIssuer::try_new(4, DEPLOYMENT_ID).unwrap();
    let client = PrivacyPassAthmClient::new(&issuer);

    let state1 = client.new_token_state().unwrap();
    let state2 = client.new_token_state().unwrap();

    let mut req1_bytes = Vec::new();
    state1.request.encode(&mut req1_bytes);
    let mut req2_bytes = Vec::new();
    state2.request.encode(&mut req2_bytes);

    expect_ne!(req1_bytes, req2_bytes);
}

#[gtest(PrivacyPassTest, IssueWithOutOfRangeMetadataFails)]
fn test_issue_with_out_of_range_metadata_fails() {
    let num_buckets = 4;
    let issuer = PrivacyPassAthmIssuer::try_new(num_buckets, DEPLOYMENT_ID).unwrap();
    let client = PrivacyPassAthmClient::new(&issuer);

    let state = client.new_token_state().unwrap();

    // Metadata == num_buckets is out of range (valid range is 0..num_buckets).
    let response = issuer.issue(state.request.clone(), num_buckets);
    expect_eq!(response.err(), Some(AthmStatus::OperationFailed));

    // Higher out-of-range metadata.
    let response_high = issuer.issue(state.request, num_buckets + 10);
    expect_eq!(response_high.err(), Some(AthmStatus::OperationFailed));
}

#[gtest(PrivacyPassTest, WrongIssuerKeyDoesNotVerify)]
fn test_wrong_issuer_key_does_not_verify() {
    let issuer_a = PrivacyPassAthmIssuer::try_new(4, DEPLOYMENT_ID).unwrap();
    let issuer_b = PrivacyPassAthmIssuer::try_new(4, DEPLOYMENT_ID).unwrap();
    let client_a = PrivacyPassAthmClient::new(&issuer_a);

    let state = client_a.new_token_state().unwrap();
    let response = issuer_a.issue(state.request.clone(), 2).unwrap();
    let token = client_a.finalize_request(state, response).unwrap();

    expect_eq!(issuer_a.verify(token.clone()), Ok(2));
    expect_eq!(issuer_b.verify(token).err(), Some(AthmStatus::OperationFailed));
}

#[gtest(PrivacyPassTest, TamperedTokenDoesNotVerify)]
fn test_tampered_token_does_not_verify() {
    let issuer = PrivacyPassAthmIssuer::try_new(4, DEPLOYMENT_ID).unwrap();
    let client = PrivacyPassAthmClient::new(&issuer);

    let state = client.new_token_state().unwrap();
    let response = issuer.issue(state.request.clone(), 1).unwrap();
    let token = client.finalize_request(state, response).unwrap();

    let mut token_bytes = Vec::new();
    token.encode(&mut token_bytes);
    expect_eq!(token_bytes.len(), 98);

    // Corrupt a coordinate byte.
    token_bytes[40] ^= 0xFF;

    let tampered_token = Token::decode(&token_bytes);
    if let Ok(t) = tampered_token {
        expect_eq!(issuer.verify(t).err(), Some(AthmStatus::OperationFailed));
    }
}

#[gtest(PrivacyPassTest, TokenRequestFramingAndUnframing)]
fn test_token_request_framing_and_unframing() {
    let issuer = PrivacyPassAthmIssuer::try_new(4, DEPLOYMENT_ID).unwrap();
    let client = PrivacyPassAthmClient::new(&issuer);

    let state = client.new_token_state().unwrap();
    let original_request = state.request;

    expect_eq!(<TokenRequest as Frame<u8>>::framed_size(), 36);

    let framed = original_request.frame(&issuer.public_key);
    expect_eq!(framed.len(), 36);
    expect_eq!(&framed[0..2], &[0xC0, 0x7E]);
    expect_eq!(framed[2], issuer.public_key.key_id()[31]);

    let (unframed, truncated_key_id) = <TokenRequest as Frame<u8>>::unframe(&framed).unwrap();
    expect_eq!(truncated_key_id, issuer.public_key.key_id()[31]);

    let mut original_bytes = Vec::new();
    original_request.encode(&mut original_bytes);
    let mut unframed_bytes = Vec::new();
    unframed.encode(&mut unframed_bytes);
    expect_eq!(original_bytes, unframed_bytes);
}

#[gtest(PrivacyPassTest, TokenRequestUnframeErrors)]
fn test_token_request_unframe_errors() {
    let issuer = PrivacyPassAthmIssuer::try_new(4, DEPLOYMENT_ID).unwrap();
    let client = PrivacyPassAthmClient::new(&issuer);
    let state = client.new_token_state().unwrap();
    let framed = state.request.frame(&issuer.public_key);

    // Unexpected length: too short.
    let short = &framed[..35];
    expect_eq!(
        <TokenRequest as Frame<u8>>::unframe(short).err(),
        Some(UnframeError::UnexpectedLength)
    );

    // Unexpected length: too long.
    let mut long = framed.clone();
    long.push(0x00);
    expect_eq!(
        <TokenRequest as Frame<u8>>::unframe(&long).err(),
        Some(UnframeError::UnexpectedLength)
    );

    // Unexpected token type.
    let mut bad_type = framed.clone();
    bad_type[0] = 0x00;
    bad_type[1] = 0x01;
    expect_eq!(
        <TokenRequest as Frame<u8>>::unframe(&bad_type).err(),
        Some(UnframeError::UnexpectedType)
    );

    // Decode failure: corrupt curve point.
    let mut bad_point = framed;
    bad_point[3] = 0xFF; // Invalid compressed curve point header
    expect_eq!(
        <TokenRequest as Frame<u8>>::unframe(&bad_point).err(),
        Some(UnframeError::DecodeFailure)
    );
}

#[gtest(PrivacyPassTest, TokenFramingAndUnframing)]
fn test_token_framing_and_unframing() {
    let issuer = PrivacyPassAthmIssuer::try_new(4, DEPLOYMENT_ID).unwrap();
    let client = PrivacyPassAthmClient::new(&issuer);

    let state = client.new_token_state().unwrap();
    let response = issuer.issue(state.request.clone(), 3).unwrap();
    let token = client.finalize_request(state, response).unwrap();

    expect_eq!(<Token as Frame<[u8; 32]>>::framed_size(), 132);

    let framed = token.frame(&issuer.public_key);
    expect_eq!(framed.len(), 132);
    expect_eq!(&framed[0..2], &[0xC0, 0x7E]);
    expect_eq!(&framed[2..34], &issuer.public_key.key_id());

    let (unframed_token, key_id) = <Token as Frame<[u8; 32]>>::unframe(&framed).unwrap();
    expect_eq!(key_id, issuer.public_key.key_id());

    // Verified unframed token recovers the correct metadata.
    let recovered = issuer.verify(unframed_token);
    expect_eq!(recovered, Ok(3));
}

#[gtest(PrivacyPassTest, TokenUnframeErrors)]
fn test_token_unframe_errors() {
    let issuer = PrivacyPassAthmIssuer::try_new(4, DEPLOYMENT_ID).unwrap();
    let client = PrivacyPassAthmClient::new(&issuer);

    let state = client.new_token_state().unwrap();
    let response = issuer.issue(state.request.clone(), 0).unwrap();
    let token = client.finalize_request(state, response).unwrap();
    let framed = token.frame(&issuer.public_key);

    // Unexpected length: too short.
    let short = &framed[..131];
    expect_eq!(
        <Token as Frame<[u8; 32]>>::unframe(short).err(),
        Some(UnframeError::UnexpectedLength)
    );

    // Unexpected length: too long.
    let mut long = framed.clone();
    long.push(0x00);
    expect_eq!(
        <Token as Frame<[u8; 32]>>::unframe(&long).err(),
        Some(UnframeError::UnexpectedLength)
    );

    // Unexpected token type.
    let mut bad_type = framed.clone();
    bad_type[0] = 0x12;
    bad_type[1] = 0x34;
    expect_eq!(
        <Token as Frame<[u8; 32]>>::unframe(&bad_type).err(),
        Some(UnframeError::UnexpectedType)
    );

    // Decode failure: corrupt compressed EC point header in token.
    let mut bad_token = framed;
    bad_token[66] = 0xFF; // Corrupt first byte of compressed point P (starts at offset 34 + 32 = 66)
    expect_eq!(
        <Token as Frame<[u8; 32]>>::unframe(&bad_token).err(),
        Some(UnframeError::DecodeFailure)
    );
}

#[gtest(PrivacyPassTest, EndToEndFramedProtocolWorkflow)]
fn test_end_to_end_framed_protocol_workflow() {
    let issuer = PrivacyPassAthmIssuer::try_new(4, DEPLOYMENT_ID).unwrap();
    let client = PrivacyPassAthmClient::new(&issuer);
    let expected_metadata = 2;

    // 1. Client creates request state using PrivacyPassAthmClient and frames the
    //    request.
    let state = client.new_token_state().unwrap();
    let framed_request = state.request.frame(&issuer.public_key);
    expect_eq!(framed_request.len(), 36);

    // 2. Issuer receives framed request, unframes and verifies truncated key ID.
    let (unframed_request, truncated_key_id) =
        <TokenRequest as Frame<u8>>::unframe(&framed_request).unwrap();
    expect_eq!(truncated_key_id, issuer.public_key.key_id()[31]);

    // 3. Issuer issues response with metadata.
    let response = issuer.issue(unframed_request, expected_metadata).unwrap();

    // 4. Client finalizes token using PrivacyPassAthmClient and frames it for
    //    redemption.
    let token = client.finalize_request(state, response).unwrap();
    let framed_token = token.frame(&issuer.public_key);
    expect_eq!(framed_token.len(), 132);

    // 5. Issuer receives framed token, unframes, checks key ID, and verifies
    //    metadata.
    let (unframed_token, key_id) = <Token as Frame<[u8; 32]>>::unframe(&framed_token).unwrap();
    expect_eq!(key_id, issuer.public_key.key_id());
    let recovered_metadata = issuer.verify(unframed_token);
    expect_eq!(recovered_metadata, Ok(expected_metadata));
}

#[gtest(PrivacyPassTest, TokenRequestInboundBatchFromBytesSingleRequest)]
fn test_token_request_inbound_batch_from_bytes_single_request() {
    let issuer = PrivacyPassAthmIssuer::try_new(4, DEPLOYMENT_ID).unwrap();
    let client = PrivacyPassAthmClient::new(&issuer);

    let state = client.new_token_state().unwrap();
    let mut batch_bytes = state.request.frame(&issuer.public_key);
    expect_eq!(batch_bytes.len(), 36);

    let version = 1u32;
    batch_bytes.extend_from_slice(&version.to_be_bytes());
    expect_eq!(batch_bytes.len(), 40);

    let batch = TokenRequestInboundBatch::from_bytes(&batch_bytes);
    let batch = batch.unwrap();

    expect_eq!(batch.version, 1);
    expect_eq!(batch.truncated_key_id, issuer.public_key.key_id()[31]);
    expect_eq!(batch.requests.len(), 1);
}

#[gtest(PrivacyPassTest, TokenRequestInboundBatchFromBytesMultipleRequests)]
fn test_token_request_inbound_batch_from_bytes_multiple_requests() {
    let issuer = PrivacyPassAthmIssuer::try_new(4, DEPLOYMENT_ID).unwrap();
    let client = PrivacyPassAthmClient::new(&issuer);

    let state1 = client.new_token_state().unwrap();
    let state2 = client.new_token_state().unwrap();
    let state3 = client.new_token_state().unwrap();

    let mut batch_bytes = Vec::new();
    batch_bytes.extend_from_slice(&state1.request.frame(&issuer.public_key));
    batch_bytes.extend_from_slice(&state2.request.frame(&issuer.public_key));
    batch_bytes.extend_from_slice(&state3.request.frame(&issuer.public_key));
    expect_eq!(batch_bytes.len(), 36 * 3);

    let version = 1u32;
    batch_bytes.extend_from_slice(&version.to_be_bytes());
    expect_eq!(batch_bytes.len(), 36 * 3 + 4);

    let batch = TokenRequestInboundBatch::from_bytes(&batch_bytes).unwrap();
    expect_eq!(batch.version, 1);
    expect_eq!(batch.truncated_key_id, issuer.public_key.key_id()[31]);
    expect_eq!(batch.requests.len(), 3);
}

#[gtest(PrivacyPassTest, TokenRequestInboundBatchFromBytesErrors)]
fn test_token_request_inbound_batch_from_bytes_errors() {
    let issuer = PrivacyPassAthmIssuer::try_new(4, DEPLOYMENT_ID).unwrap();
    let client = PrivacyPassAthmClient::new(&issuer);

    // Empty input: UnexpectedLength.
    expect_eq!(
        TokenRequestInboundBatch::from_bytes(&[]).err(),
        Some(UnframeError::UnexpectedLength)
    );

    // Input shorter than 4 bytes: UnexpectedLength.
    expect_eq!(
        TokenRequestInboundBatch::from_bytes(&[0, 1, 2]).err(),
        Some(UnframeError::UnexpectedLength)
    );

    // Only version (4 bytes, 0 requests): UnexpectedLength.
    expect_eq!(
        TokenRequestInboundBatch::from_bytes(&1u32.to_be_bytes()).err(),
        Some(UnframeError::UnexpectedLength)
    );

    let state = client.new_token_state().unwrap();
    let framed = state.request.frame(&issuer.public_key);

    // Incomplete request chunk: 35 bytes + 4 bytes version = 39 bytes.
    let mut bad_len = framed[..35].to_vec();
    bad_len.extend_from_slice(&1u32.to_be_bytes());
    expect_eq!(
        TokenRequestInboundBatch::from_bytes(&bad_len).err(),
        Some(UnframeError::UnexpectedLength)
    );

    // Extra trailing byte: 37 bytes + 4 bytes version = 41 bytes.
    let mut bad_len2 = framed.clone();
    bad_len2.push(0x00);
    bad_len2.extend_from_slice(&1u32.to_be_bytes());
    expect_eq!(
        TokenRequestInboundBatch::from_bytes(&bad_len2).err(),
        Some(UnframeError::UnexpectedLength)
    );

    // Unexpected token type in request chunk.
    let mut bad_type = framed.clone();
    bad_type[0] = 0x00;
    bad_type[1] = 0x01;
    bad_type.extend_from_slice(&1u32.to_be_bytes());
    expect_eq!(
        TokenRequestInboundBatch::from_bytes(&bad_type).err(),
        Some(UnframeError::UnexpectedType)
    );

    // Decode failure in request chunk (corrupt curve point).
    let mut bad_point = framed.clone();
    bad_point[3] = 0xFF;
    bad_point.extend_from_slice(&1u32.to_be_bytes());
    expect_eq!(
        TokenRequestInboundBatch::from_bytes(&bad_point).err(),
        Some(UnframeError::DecodeFailure)
    );

    // Inconsistent key IDs in batch (MultipleKeyIds).
    let issuer_b = PrivacyPassAthmIssuer::try_new(4, b"deployment-b").unwrap();
    let client_b = PrivacyPassAthmClient::new(&issuer_b);
    let state_b = client_b.new_token_state().unwrap();

    let mut multi_key_batch = Vec::new();
    multi_key_batch.extend_from_slice(&framed); // Key ID from issuer A
    multi_key_batch.extend_from_slice(&state_b.request.frame(&issuer_b.public_key)); // Key ID from issuer B
    multi_key_batch.extend_from_slice(&1u32.to_be_bytes());
    expect_eq!(
        TokenRequestInboundBatch::from_bytes(&multi_key_batch).err(),
        Some(UnframeError::MultipleKeyIds)
    );
}

#[gtest(PrivacyPassTest, IssueBatchAndTokenResponseInboundBatchSuccess)]
fn test_issue_batch_and_token_response_inbound_batch_success() {
    let issuer = PrivacyPassAthmIssuer::try_new(4, DEPLOYMENT_ID).unwrap();
    let client = PrivacyPassAthmClient::new(&issuer);
    let expected_metadata = 3;

    let state1 = client.new_token_state().unwrap();
    let state2 = client.new_token_state().unwrap();
    let state3 = client.new_token_state().unwrap();

    let mut batch_bytes = Vec::new();
    batch_bytes.extend_from_slice(&state1.request.frame(&issuer.public_key));
    batch_bytes.extend_from_slice(&state2.request.frame(&issuer.public_key));
    batch_bytes.extend_from_slice(&state3.request.frame(&issuer.public_key));
    batch_bytes.extend_from_slice(&1u32.to_be_bytes());

    let request_batch = TokenRequestInboundBatch::from_bytes(&batch_bytes).unwrap();
    let response_bytes = issuer.issue_batch(request_batch, expected_metadata);
    let response_bytes = response_bytes.unwrap();

    // Client side: parses the concatenated token responses using
    // TokenResponseInboundBatch.
    let response_batch = TokenResponseInboundBatch::from_bytes(&response_bytes, &client);
    let response_batch = response_batch.unwrap();
    expect_eq!(response_batch.responses.len(), 3);

    // Client side: finalizes the entire batch via response_batch.finalize_batch.
    let states = vec![state1, state2, state3];
    let tokens = response_batch.finalize_batch(states, &client);
    let tokens = tokens.unwrap();
    expect_eq!(tokens.len(), 3);

    for token in tokens {
        let recovered = issuer.verify(token);
        expect_eq!(recovered, Ok(expected_metadata));
    }
}

#[gtest(PrivacyPassTest, TokenResponseInboundBatchFromBytesErrors)]
fn test_token_response_inbound_batch_from_bytes_errors() {
    let issuer = PrivacyPassAthmIssuer::try_new(4, DEPLOYMENT_ID).unwrap();
    let client = PrivacyPassAthmClient::new(&issuer);

    // Empty input: UnexpectedLength.
    expect_eq!(
        TokenResponseInboundBatch::from_bytes(&[], &client).err(),
        Some(UnframeError::UnexpectedLength)
    );

    let state = client.new_token_state().unwrap();
    let mut req_batch_bytes = state.request.frame(&issuer.public_key);
    req_batch_bytes.extend_from_slice(&1u32.to_be_bytes());
    let req_batch = TokenRequestInboundBatch::from_bytes(&req_batch_bytes).unwrap();

    let valid_response_bytes = issuer.issue_batch(req_batch, 1).unwrap();
    let single_resp_len = valid_response_bytes.len();

    // Partial response length: UnexpectedLength.
    let short_resp = &valid_response_bytes[..single_resp_len - 1];
    expect_eq!(
        TokenResponseInboundBatch::from_bytes(short_resp, &client).err(),
        Some(UnframeError::UnexpectedLength)
    );

    // Extra trailing byte: UnexpectedLength.
    let mut long_resp = valid_response_bytes.clone();
    long_resp.push(0x00);
    expect_eq!(
        TokenResponseInboundBatch::from_bytes(&long_resp, &client).err(),
        Some(UnframeError::UnexpectedLength)
    );

    // Decode failure: corrupt curve point data in response.
    let mut corrupt_resp = valid_response_bytes;
    corrupt_resp[0] = 0xFF; // Invalid compressed point header for big_u
    expect_eq!(
        TokenResponseInboundBatch::from_bytes(&corrupt_resp, &client).err(),
        Some(UnframeError::DecodeFailure)
    );
}

#[gtest(PrivacyPassTest, TokenResponseInboundBatchFinalizeErrors)]
fn test_token_response_inbound_batch_finalize_errors() {
    let issuer = PrivacyPassAthmIssuer::try_new(4, DEPLOYMENT_ID).unwrap();
    let client = PrivacyPassAthmClient::new(&issuer);

    let state1 = client.new_token_state().unwrap();
    let state2 = client.new_token_state().unwrap();

    let mut req_batch_bytes = state1.request.frame(&issuer.public_key);
    req_batch_bytes.extend_from_slice(&1u32.to_be_bytes());
    let req_batch = TokenRequestInboundBatch::from_bytes(&req_batch_bytes).unwrap();

    let valid_response_bytes = issuer.issue_batch(req_batch, 1).unwrap();
    let response_batch =
        TokenResponseInboundBatch::from_bytes(&valid_response_bytes, &client).unwrap();

    // State count mismatch: 2 states provided for 1 response in batch.
    let state1_again = client.new_token_state().unwrap();
    expect_true!(response_batch.finalize_batch(vec![state1_again, state2], &client).is_err());
}

#[gtest(PrivacyPassTest, IssueBatchKeyIdMismatchReturnsNone)]
fn test_issue_batch_key_id_mismatch_returns_none() {
    let issuer_a = PrivacyPassAthmIssuer::try_new(4, DEPLOYMENT_ID).unwrap();
    let issuer_b = PrivacyPassAthmIssuer::try_new(4, b"other-deployment").unwrap();
    let client_b = PrivacyPassAthmClient::new(&issuer_b);

    let state = client_b.new_token_state().unwrap();
    let mut batch_bytes = state.request.frame(&issuer_b.public_key);
    batch_bytes.extend_from_slice(&1u32.to_be_bytes());

    let batch = TokenRequestInboundBatch::from_bytes(&batch_bytes).unwrap();
    // Batch is framed with issuer_b's truncated key ID; sending to issuer_a must
    // return Err.
    let response = issuer_a.issue_batch(batch, 1);
    expect_true!(response.is_err());
}

#[gtest(PrivacyPassTest, IssueBatchOutOfRangeMetadataReturnsNone)]
fn test_issue_batch_out_of_range_metadata_returns_none() {
    let issuer = PrivacyPassAthmIssuer::try_new(4, DEPLOYMENT_ID).unwrap();
    let client = PrivacyPassAthmClient::new(&issuer);

    let state = client.new_token_state().unwrap();
    let mut batch_bytes = state.request.frame(&issuer.public_key);
    batch_bytes.extend_from_slice(&1u32.to_be_bytes());

    let batch = TokenRequestInboundBatch::from_bytes(&batch_bytes).unwrap();
    // Metadata == 4 is out of range for 4 buckets (0..4).
    let response = issuer.issue_batch(batch, 4);
    expect_true!(response.is_err());
}

#[gtest(PrivacyPassTest, BatchRequestTryNewSuccessAndRequestBody)]
fn test_batch_request_try_new_success_and_request_body() {
    let issuer = PrivacyPassAthmIssuer::try_new(4, DEPLOYMENT_ID).unwrap();
    let mut pk_bytes = Vec::new();
    issuer.public_key.encode(&mut pk_bytes);
    let mut proof_bytes = Vec::new();
    issuer.public_key_proof.encode(&mut proof_bytes);

    let batch_size = 5;
    let version = 1;
    let batch_req = PrivacyPassBatchClient::try_new(
        &pk_bytes,
        &proof_bytes,
        4,
        DEPLOYMENT_ID,
        version,
        batch_size,
    );
    let batch_req = batch_req.unwrap();

    let body = batch_req.request_body();
    expect_eq!(body.len(), batch_size * 36 + 4);
}

#[gtest(PrivacyPassTest, BatchRequestTryNewInvalidParameters)]
fn test_batch_request_try_new_invalid_parameters() {
    let issuer = PrivacyPassAthmIssuer::try_new(4, DEPLOYMENT_ID).unwrap();
    let mut pk_bytes = Vec::new();
    issuer.public_key.encode(&mut pk_bytes);
    let mut proof_bytes = Vec::new();
    issuer.public_key_proof.encode(&mut proof_bytes);

    // 0 batch size is invalid.
    expect_eq!(
        PrivacyPassBatchClient::try_new(&pk_bytes, &proof_bytes, 4, DEPLOYMENT_ID, 1, 0).err(),
        Some(AthmStatus::InvalidInput)
    );

    // 0 buckets is invalid.
    expect_eq!(
        PrivacyPassBatchClient::try_new(&pk_bytes, &proof_bytes, 0, DEPLOYMENT_ID, 1, 2).err(),
        Some(AthmStatus::InvalidInput)
    );

    // Invalid public key.
    expect_eq!(
        PrivacyPassBatchClient::try_new(b"invalid_pk", &proof_bytes, 4, DEPLOYMENT_ID, 1, 2).err(),
        Some(AthmStatus::InvalidInput)
    );

    // Invalid proof.
    expect_eq!(
        PrivacyPassBatchClient::try_new(&pk_bytes, b"invalid_proof", 4, DEPLOYMENT_ID, 1, 2).err(),
        Some(AthmStatus::InvalidInput)
    );

    // Overlong deployment ID (> 255 bytes).
    let long_id = vec![0x42; 256];
    expect_eq!(
        PrivacyPassBatchClient::try_new(&pk_bytes, &proof_bytes, 4, &long_id, 1, 2).err(),
        Some(AthmStatus::InvalidInput)
    );
}

#[gtest(PrivacyPassTest, BatchRequestRoundtripIssuanceAndFinalization)]
fn test_batch_request_roundtrip_issuance_and_finalization() {
    let issuer = PrivacyPassAthmIssuer::try_new(4, DEPLOYMENT_ID).unwrap();
    let mut pk_bytes = Vec::new();
    issuer.public_key.encode(&mut pk_bytes);
    let mut proof_bytes = Vec::new();
    issuer.public_key_proof.encode(&mut proof_bytes);

    let batch_size = 3;
    let mut batch_req =
        PrivacyPassBatchClient::try_new(&pk_bytes, &proof_bytes, 4, DEPLOYMENT_ID, 1, batch_size)
            .unwrap();

    let request_body = batch_req.request_body();
    let inbound_batch = TokenRequestInboundBatch::from_bytes(request_body).unwrap();
    expect_eq!(inbound_batch.version, 1);

    let metadata = 2;
    let response_body = issuer.issue_batch(inbound_batch, metadata).unwrap();

    let finalized_tokens = batch_req.finalize(&response_body);
    let finalized_tokens = finalized_tokens.unwrap();
    expect_eq!(finalized_tokens.len(), batch_size);

    for token_bytes in finalized_tokens {
        expect_eq!(token_bytes.len(), 132);
        let (unframed, _key_id) = Token::unframe(&token_bytes).unwrap();
        let recovered = issuer.verify(unframed);
        expect_eq!(recovered, Ok(metadata));
    }
}

#[gtest(PrivacyPassTest, BatchRequestFinalizeErrorHandling)]
fn test_batch_request_finalize_error_handling() {
    let issuer = PrivacyPassAthmIssuer::try_new(4, DEPLOYMENT_ID).unwrap();
    let mut pk_bytes = Vec::new();
    issuer.public_key.encode(&mut pk_bytes);
    let mut proof_bytes = Vec::new();
    issuer.public_key_proof.encode(&mut proof_bytes);

    let batch_size = 2;
    let mut batch_req =
        PrivacyPassBatchClient::try_new(&pk_bytes, &proof_bytes, 4, DEPLOYMENT_ID, 1, batch_size)
            .unwrap();

    // Invalid response body (truncated).
    let bad_response = vec![0u8; 10];
    let res = batch_req.finalize(&bad_response);
    expect_eq!(res.err(), Some(AthmStatus::InvalidInput));

    // Calling finalize again on a consumed/failed request returns
    // AthmStatus::OperationFailed.
    let res_second = batch_req.finalize(&bad_response);
    expect_eq!(res_second.err(), Some(AthmStatus::OperationFailed));
}

#[gtest(PrivacyPassTest, IssuerWireMethodsRoundtrip)]
fn test_issuer_wire_methods_roundtrip() {
    let issuer = PrivacyPassAthmIssuer::try_new(2, b"1").unwrap();
    let pk_bytes = issuer.public_key_bytes();
    let proof_bytes = issuer.public_key_proof_bytes();
    expect_false!(pk_bytes.is_empty());
    expect_false!(proof_bytes.is_empty());
    expect_eq!(issuer.public_key.truncated_key_id(), issuer.public_key.key_id()[31]);

    let batch_size = 2;
    let mut batch_req =
        PrivacyPassBatchClient::try_new(&pk_bytes, &proof_bytes, 2, b"1", 1, batch_size).unwrap();

    let request_body = batch_req.request_body();
    let metadata = 0;
    let response_body = issuer.issue_batch_from_bytes(request_body, metadata).unwrap();

    let finalized_tokens = batch_req.finalize(&response_body).unwrap();
    expect_eq!(finalized_tokens.len(), batch_size);

    for token_bytes in &finalized_tokens {
        let recovered = issuer.verify_wire_token(token_bytes).unwrap();
        expect_eq!(recovered, metadata);
    }

    // Error handling on issue_batch_from_bytes
    expect_eq!(issuer.issue_batch_from_bytes(&[], metadata).err(), Some(AthmStatus::InvalidInput));
    expect_eq!(
        issuer.issue_batch_from_bytes(&[0u8; 10], metadata).err(),
        Some(AthmStatus::InvalidInput)
    );

    // Error handling on verify_wire_token
    expect_eq!(issuer.verify_wire_token(&[0u8; 10]).err(), Some(AthmStatus::InvalidInput));
    let mut bad_token = finalized_tokens[0].clone();
    bad_token[0] = 0xFF; // Bad token type
    expect_eq!(issuer.verify_wire_token(&bad_token).err(), Some(AthmStatus::InvalidInput));
}
