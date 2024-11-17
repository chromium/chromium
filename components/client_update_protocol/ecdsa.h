// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CLIENT_UPDATE_PROTOCOL_ECDSA_H_
#define COMPONENTS_CLIENT_UPDATE_PROTOCOL_ECDSA_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace client_update_protocol {

// Client Update Protocol v2, or CUP-ECDSA, is used by Google Update (Omaha)
// servers to ensure freshness and authenticity of server responses over HTTP,
// without the overhead of HTTPS -- namely, no PKI, no guarantee of privacy, and
// no request replay protection.
//
// CUP-ECDSA relies on a single signing operation using ECDSA with SHA-256,
// instead of the original CUP which used HMAC-SHA1 with a random signing key
// encrypted using RSA.
//
// Each |Ecdsa| object represents a single network ping in flight -- a call to
// SignRequest() generates internal state that will be used by
// ValidateResponse().
class Ecdsa {
 public:
  Ecdsa() = delete;
  Ecdsa(const Ecdsa&) = delete;
  Ecdsa& operator=(const Ecdsa&) = delete;

  ~Ecdsa();

  struct RequestParameters {
    std::string query_cup2key;
    std::string hash_hex;
  };

  // Initializes this instance of CUP-ECDSA with a versioned public key.
  // |key_version| must be non-negative. |public_key| is expected to be a
  // DER-encoded ASN.1 SubjectPublicKeyInfo containing an ECDSA public key.
  // Returns a NULL pointer on failure.
  static std::unique_ptr<Ecdsa> Create(int key_version,
                                       std::string_view public_key);

  // Generates freshness/authentication data for an outgoing ping.
  // |request_body| contains the body of the ping in UTF-8.  On return,
  // |query_params| contains a set of query parameters (in UTF-8) to be appended
  // to the URL.
  //
  // This method will store internal state in this instance used by calls to
  // ValidateResponse(); if you need to have multiple pings in flight,
  // initialize a separate CUP-ECDSA instance for each one.
  void SignRequest(std::string_view request_body, std::string* query_params);

  // Generates freshness/authentication data for an outgoing ping.
  // |request_body| contains the body of the ping in UTF-8. Returns the
  // parameters that must be sent to the server for ECDSA authentication.
  //
  // This method will store internal state in this instance used by calls to
  // ValidateResponse(); if you need to have multiple pings in flight,
  // initialize a separate CUP-ECDSA instance for each one.
  RequestParameters SignRequest(std::string_view request_body);

  // Validates a response given to a ping previously signed with
  // SignRequest(). |response_body| contains the body of the response in
  // UTF-8. |signature| contains the ECDSA signature and observed request
  // hash. Returns true if the response is valid and the observed request hash
  // matches the sent hash.  This method uses internal state that is set by a
  // prior SignRequest() call.
  bool ValidateResponse(std::string_view response_body,
                        std::string_view signature);

  // Sets the key and nonce that were used to generate a signature that is baked
  // into a unit test. Note this function encodes |nonce| in decimal, while
  // non-test paths use a base64url-encoded, 256-bit string.
  void OverrideNonceForTesting(int key_version, uint32_t nonce);

 private:
  Ecdsa(int key_version, std::string_view public_key);

  // The server keeps multiple signing keys; a version must be sent so that
  // the correct signing key is used to sign the assembled message.
  const int pub_key_version_;

  // The ECDSA public key to use for verifying response signatures.
  const std::vector<uint8_t> public_key_;

  // The SHA-256 hash of the XML request.  This is modified on each call to
  // SignRequest(), and checked by ValidateResponse().
  std::vector<uint8_t> request_hash_;

  // The query string containing key version and nonce in UTF-8 form.  This is
  // modified on each call to SignRequest(), and checked by ValidateResponse().
  std::string request_query_cup2key_;
};

}  // namespace client_update_protocol

#endif  // COMPONENTS_CLIENT_UPDATE_PROTOCOL_ECDSA_H_
