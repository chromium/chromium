// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_ATHM_TEST_ISSUER_H_
#define COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_ATHM_TEST_ISSUER_H_

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "crypto/hash.h"

namespace private_verification_tokens {

// A self-contained test issuer for Anonymous Tokens with Hidden Metadata
// (ATHM), analogous to network::TestTrustTokenIssuer for Private State Tokens.
//
// It holds the (secret) issuer key material and exposes the issuer-side
// operations -- Issue() and Verify() -- plus the public material a client needs
// to participate (public key, the public-key proof, and the public protocol
// params). All inputs/outputs are serialized ATHM wire encodings.
//
// The crypto runs through the cxx bridge in athm_ffi.rs on Chromium's in-tree
// BoringSSL. This is a test utility, not production code.
class AthmTestIssuer {
 public:
  // Creates an issuer for `num_buckets` metadata buckets (valid hidden metadata
  // is then `0..num_buckets`) bound to `deployment_id`. Returns nullopt if key
  // generation fails, which happens when `num_buckets == 0` or when
  // `deployment_id` exceeds the 255-byte limit the ATHM parameters enforce.
  static std::optional<AthmTestIssuer> Create(
      uint8_t num_buckets,
      base::span<const uint8_t> deployment_id);

  AthmTestIssuer(AthmTestIssuer&&);
  AthmTestIssuer& operator=(AthmTestIssuer&&);
  ~AthmTestIssuer();

  AthmTestIssuer(const AthmTestIssuer&) = delete;
  AthmTestIssuer& operator=(const AthmTestIssuer&) = delete;

  // Public material the client/browser needs (the "verification key" analog).
  const std::vector<uint8_t>& public_key() const { return public_key_; }
  const std::vector<uint8_t>& public_key_proof() const {
    return public_key_proof_;
  }
  const std::vector<uint8_t>& params() const { return params_; }
  // The 32-byte SHA-256 digest of `public_key_`.
  const std::array<uint8_t, crypto::hash::kSha256Size>& key_id() const {
    return key_id_;
  }
  // The 1-byte truncated key ID (last byte of `key_id_`) used in Privacy Pass /
  // ATHM token request and redemption framing.
  uint8_t truncated_key_id() const { return key_id_.back(); }

  // Issuer side: signs `request`, embedding `hidden_metadata`. Returns the
  // serialized token response, or nullopt on failure.
  std::optional<std::vector<uint8_t>> Issue(base::span<const uint8_t> request,
                                            uint8_t hidden_metadata) const;

  // Issuer side: processes a batch of wire-formatted `AthmTokenRequest`s,
  // unmarshaling each request, validating that the token type is
  // `kAthmTokenType` and the truncated key ID matches `truncated_key_id()`, and
  // signing each extracted blinded request with `hidden_metadata` via Issue().
  // Returns a vector of serialized token responses, or nullopt if `requests` is
  // empty, if any request fails validation/unmarshaling, or if token signing
  // fails.
  std::optional<std::vector<std::vector<uint8_t>>> BatchIssue(
      const std::vector<std::vector<uint8_t>>& requests,
      uint8_t hidden_metadata) const;

  // Issuer side: processes a concatenated batch request body `request_body`
  // (sliced into single wire-formatted request chunks according to Version 1
  // parameters) by delegating to the vector BatchIssue() overload. Returns the
  // concatenated serialized token responses as a string, or nullopt if
  // `request_body` has an invalid/empty size, or if any request fails
  // validation or signing.
  std::optional<std::string> BatchIssue(std::string_view request_body,
                                        uint8_t hidden_metadata) const;

  // Issuer side: verifies `token` and recovers the embedded hidden metadata, or
  // nullopt if the token does not verify.
  std::optional<uint8_t> Verify(base::span<const uint8_t> token) const;

  // Issuer side: verifies `marshaled_token` (132-byte wire-formatted token),
  // validating that the token type is `kAthmTokenType` and the issuer key ID
  // matches `key_id()`, and verifies the inner 98-byte unblinded token via
  // Verify(). Returns the recovered hidden metadata on success, or nullopt if
  // unmarshaling, header validation, or cryptographic verification fails.
  std::optional<uint8_t> VerifyWithCheck(
      base::span<const uint8_t> marshaled_token) const;

 private:
  AthmTestIssuer(std::vector<uint8_t> params,
                 std::vector<uint8_t> private_key,
                 std::vector<uint8_t> public_key,
                 std::vector<uint8_t> public_key_proof);

  std::vector<uint8_t> params_;
  std::vector<uint8_t> private_key_;
  std::vector<uint8_t> public_key_;
  std::vector<uint8_t> public_key_proof_;
  std::array<uint8_t, crypto::hash::kSha256Size> key_id_;
};

// A self-contained test client for Anonymous Tokens with Hidden Metadata
// (ATHM).
//
// It holds only public material (public_key, public_key_proof, params) and
// exposes client-side operations (CreateClientRequest, FinalizeToken) so tests
// can drive the issuer and client as separate parties.
class AthmTestClient {
 public:
  // The client state produced by CreateClientRequest(). Holding one of these
  // means it is valid; CreateClientRequest() returns nullopt on failure.
  struct ClientRequest {
    // Client-secret state that must be retained until FinalizeToken().
    std::vector<uint8_t> context;
    // The blinded request to send to the issuer.
    std::vector<uint8_t> request;
  };

  static std::optional<AthmTestClient> Create(
      base::span<const uint8_t> public_key,
      base::span<const uint8_t> public_key_proof,
      uint8_t num_buckets,
      base::span<const uint8_t> deployment_id);
  ~AthmTestClient();

  AthmTestClient(const AthmTestClient&);
  AthmTestClient& operator=(const AthmTestClient&);
  AthmTestClient(AthmTestClient&&);
  AthmTestClient& operator=(AthmTestClient&&);

  // Client side: builds a blinded token request from public material.
  std::optional<ClientRequest> CreateClientRequest() const;

  // Client side: unblinds the issuer `response` into a finalized token, using
  // the `client_request` returned by CreateClientRequest(). Returns the
  // serialized token, or nullopt on failure.
  std::optional<std::vector<uint8_t>> FinalizeToken(
      const ClientRequest& client_request,
      base::span<const uint8_t> response) const;

 private:
  AthmTestClient(std::vector<uint8_t> public_key,
                 std::vector<uint8_t> public_key_proof,
                 std::vector<uint8_t> params);

  std::vector<uint8_t> public_key_;
  std::vector<uint8_t> public_key_proof_;
  std::vector<uint8_t> params_;
};

}  // namespace private_verification_tokens

#endif  // COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_ATHM_TEST_ISSUER_H_
