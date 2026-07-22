// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_ATHM_TEST_ISSUER_H_
#define COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_ATHM_TEST_ISSUER_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/containers/span.h"

namespace private_verification_tokens {

// A self-contained test issuer for Anonymous Tokens with Hidden Metadata
// (ATHM), analogous to network::TestTrustTokenIssuer for Private State Tokens.
//
// It holds the (secret) issuer key material and exposes the issuer-side
// operations -- Issue() and Verify() -- plus the public material a client needs
// to participate (public key, the public-key proof, and the public protocol
// params). It also provides client-side helpers (CreateClientRequest,
// FinalizeToken) that use only public material, so tests can drive the issuer
// and client as separate parties. All inputs/outputs are the serialized ATHM
// wire encodings (see the ATHM spec for the encoding).
//
// The crypto runs through the cxx bridge in athm_ffi.rs on Chromium's in-tree
// BoringSSL. This is a test/feasibility utility, not production code.
class AthmTestIssuer {
 public:
  // The client state produced by CreateClientRequest(). Holding one of these
  // means it is valid; CreateClientRequest() returns nullopt on failure.
  struct ClientRequest {
    // Client-secret state that must be retained until FinalizeToken().
    std::vector<uint8_t> context;
    // The blinded request to send to the issuer.
    std::vector<uint8_t> request;
  };

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

  // Issuer side: signs `request`, embedding `hidden_metadata`. Returns the
  // serialized token response, or nullopt on failure.
  std::optional<std::vector<uint8_t>> Issue(base::span<const uint8_t> request,
                                            uint8_t hidden_metadata) const;

  // Issuer side: verifies `token` and recovers the embedded hidden metadata, or
  // nullopt if the token does not verify.
  std::optional<uint8_t> Verify(base::span<const uint8_t> token) const;

  // Client side: builds a blinded token request from this issuer's public
  // material. Uses no secret state. Returns nullopt on failure.
  std::optional<ClientRequest> CreateClientRequest() const;

  // Client side: unblinds the issuer `response` into a finalized token, using
  // the `client_request` returned by CreateClientRequest(). Returns the
  // serialized token, or nullopt on failure.
  std::optional<std::vector<uint8_t>> FinalizeToken(
      const ClientRequest& client_request,
      base::span<const uint8_t> response) const;

 private:
  AthmTestIssuer(std::vector<uint8_t> params,
                 std::vector<uint8_t> private_key,
                 std::vector<uint8_t> public_key,
                 std::vector<uint8_t> public_key_proof);

  std::vector<uint8_t> params_;
  std::vector<uint8_t> private_key_;
  std::vector<uint8_t> public_key_;
  std::vector<uint8_t> public_key_proof_;
};

}  // namespace private_verification_tokens

#endif  // COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_ATHM_TEST_ISSUER_H_
