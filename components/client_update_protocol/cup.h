// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CLIENT_UPDATE_PROTOCOL_CUP_H_
#define COMPONENTS_CLIENT_UPDATE_PROTOCOL_CUP_H_

#include <stdint.h>

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"

namespace client_update_protocol {

// Client Update Protocol v2 (CUP) is used by Google Update (Omaha) servers to
// ensure freshness and authenticity of server responses over HTTP, without the
// overhead of HTTPS -- namely, no PKI, no guarantee of privacy, and no request
// replay protection.
//
// CUP supports single signing operations using ML-DSA-44.
//
// Each `Cup` object represents a single network ping in flight -- a call to
// PrepareRequestParameters() generates internal state that will be used by
// ValidateResponse().
class Cup {
 public:
  Cup() = delete;
  Cup(const Cup&) = delete;
  Cup& operator=(const Cup&) = delete;

  ~Cup();

  // Initializes this instance of `Cup` with a versioned public key.
  // `key_version` must be positive. `spki` is expected to be a
  // DER-encoded ASN.1 SubjectPublicKeyInfo containing an ML-DSA-44 public key.
  Cup(int key_version, base::span<const uint8_t> spki);

  // Generates freshness/authentication data for an outgoing ping.
  // `request_body` contains the body of the ping in UTF-8. Returns a set of
  // query parameters (in UTF-8) to be appended to the URL.
  //
  // This method will store internal state in this instance used by calls to
  // ValidateResponse(); if you need to have multiple pings in flight,
  // initialize a separate `Cup` instance for each one.
  std::string PrepareRequestParameters(std::string_view request_body);

  // Validates a response given to a ping previously prepared with
  // PrepareRequestParameters(). `response_body` contains the body of the
  // response in UTF-8. `signature` contains the signature and observed
  // request hash. Returns true if the response is valid and the observed
  // request hash matches the sent hash.  This method uses internal state that
  // is set by a prior PrepareRequestParameters() call.
  bool ValidateResponse(std::string_view response_body,
                        std::string_view signature);

  // Sets the key and nonce that were used to generate a signature that is baked
  // into a unit test. Note this function encodes `nonce` in decimal, while
  // non-test paths use a base64url-encoded, 256-bit string.
  void OverrideNonceForTesting(int key_version, uint32_t nonce);

 private:
  bool ParseETagHeader(std::string_view etag_header_value_in,
                       std::vector<uint8_t>* signature_out,
                       std::vector<uint8_t>* request_hash_out) const;

  // The server keeps multiple signing keys; a version must be sent so that
  // the correct signing key is used to sign the assembled message.
  const int key_version_;

  // The public key to use for verifying response signatures.
  const crypto::keypair::PublicKey public_key_;

  // The SHA-256 hash of the XML request.  This is modified on each call to
  // PrepareRequestParameters(), and checked by ValidateResponse().
  std::array<uint8_t, crypto::hash::kSha256Size> request_hash_ = {};

  // The query string containing key version and nonce in UTF-8 form.  This is
  // modified on each call to PrepareRequestParameters(), and checked by
  // ValidateResponse().
  std::string request_query_cup2key_;
};

}  // namespace client_update_protocol

#endif  // COMPONENTS_CLIENT_UPDATE_PROTOCOL_CUP_H_
