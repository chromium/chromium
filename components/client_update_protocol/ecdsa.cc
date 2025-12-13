// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/client_update_protocol/ecdsa.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64url.h"
#include "base/check.h"
#include "base/containers/to_vector.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "crypto/hash.h"
#include "crypto/random.h"
#include "crypto/sign.h"

namespace client_update_protocol {
namespace {

bool ParseETagHeader(std::string_view etag_header_value_in,
                     std::vector<uint8_t>* ecdsa_signature_out,
                     std::vector<uint8_t>* request_hash_out) {
  // The ETag value is a UTF-8 string, formatted as "S:H", where:
  // * S is the ECDSA signature in DER-encoded ASN.1 form, converted to hex.
  // * H is the SHA-256 hash of the observed request body, standard hex format.
  // A Weak ETag is formatted as W/"S:H". This function treats it the same as a
  // strong ETag.
  std::string_view etag_header_value(etag_header_value_in);

  // Remove the weak prefix, then remove the begin and the end quotes.
  const char kWeakETagPrefix[] = "W/";
  if (base::StartsWith(etag_header_value, kWeakETagPrefix)) {
    etag_header_value.remove_prefix(std::size(kWeakETagPrefix) - 1);
  }
  if (etag_header_value.size() >= 2 &&
      base::StartsWith(etag_header_value, "\"") &&
      base::EndsWith(etag_header_value, "\"")) {
    etag_header_value.remove_prefix(1);
    etag_header_value.remove_suffix(1);
  }

  const std::string_view::size_type delim_pos = etag_header_value.find(':');
  if (delim_pos == std::string_view::npos || delim_pos == 0 ||
      delim_pos == etag_header_value.size() - 1) {
    return false;
  }

  const std::string_view sig_hex = etag_header_value.substr(0, delim_pos);
  const std::string_view hash_hex = etag_header_value.substr(delim_pos + 1);

  // Decode the ECDSA signature. Don't bother validating the contents of it;
  // the SignatureValidator class will handle the actual DER decoding and
  // ASN.1 parsing. Check for an expected size range only -- valid ECDSA
  // signatures are between 8 and 72 bytes.
  if (!base::HexStringToBytes(sig_hex, ecdsa_signature_out)) {
    return false;
  }
  if (ecdsa_signature_out->size() < 8 || ecdsa_signature_out->size() > 72) {
    return false;
  }

  // Decode the SHA-256 hash; it should be exactly 32 bytes, no more or less.
  if (!base::HexStringToBytes(hash_hex, request_hash_out)) {
    return false;
  }
  if (request_hash_out->size() != crypto::hash::kSha256Size) {
    return false;
  }

  return true;
}

}  // namespace

Ecdsa::Ecdsa(int key_version, base::span<const uint8_t> public_key)
    : pub_key_version_(key_version),
      // This will fail a CHECK if the public key is malformed. Since the public
      // key is hardcoded, that's fine.
      public_key_(
          *crypto::keypair::PublicKey::FromSubjectPublicKeyInfo(public_key)) {
  CHECK_GT(key_version, 0);
  CHECK(public_key_.IsEc());
}

Ecdsa::~Ecdsa() = default;

void Ecdsa::OverrideNonceForTesting(int key_version, uint32_t nonce) {
  DCHECK(!request_query_cup2key_.empty());
  request_query_cup2key_ = base::StringPrintf("%d:%u", pub_key_version_, nonce);
}

void Ecdsa::SignRequest(std::string_view request_body,
                        std::string* query_params) {
  DCHECK(query_params);
  Ecdsa::RequestParameters request_parameters = SignRequest(request_body);
  *query_params = base::StringPrintf("cup2key=%s&cup2hreq=%s",
                                     request_parameters.query_cup2key.c_str(),
                                     request_parameters.hash_hex.c_str());
}

Ecdsa::RequestParameters Ecdsa::SignRequest(std::string_view request_body) {
  // Generate a random nonce to use for freshness, build the cup2key query
  // string, and compute the SHA-256 hash of the request body. Set these
  // two pieces of data aside to use during ValidateResponse().
  std::array<uint8_t, 32> nonce;
  crypto::RandBytes(nonce);

  // The nonce is an opaque string to the server, so the exact encoding does not
  // matter. Use base64url as it is slightly more compact than hex.
  std::string nonce_b64;
  base::Base64UrlEncode(nonce, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &nonce_b64);

  request_query_cup2key_ =
      base::StringPrintf("%d:%s", pub_key_version_, nonce_b64);
  request_hash_ = crypto::hash::Sha256(base::as_byte_span(request_body));

  // Return the query string for the user to send with the request.
  return {.query_cup2key = request_query_cup2key_,
          .hash_hex = base::HexEncodeLower(request_hash_)};
}

bool Ecdsa::ValidateResponse(std::string_view response_body,
                             std::string_view server_etag) {
  CHECK(!request_hash_.empty());
  CHECK(!request_query_cup2key_.empty());

  if (response_body.empty() || server_etag.empty()) {
    return false;
  }

  // Break the ETag into its two components (the ECDSA signature, and the
  // hash of the request that the server observed) and decode to byte buffers.
  std::vector<uint8_t> signature;
  std::vector<uint8_t> observed_request_hash;
  if (!ParseETagHeader(server_etag, &signature, &observed_request_hash)) {
    return false;
  }

  // Check that the server's observed request hash is equal to the original
  // request hash. (This is a quick rejection test; the signature test is
  // authoritative, but slower.)
  if (!std::ranges::equal(observed_request_hash, request_hash_)) {
    return false;
  }

  // Next, build the buffer that the server will have signed on its end:
  //   hash( hash(request) | hash(response) | cup2key_query_string )
  // When building the client's version of the buffer, it's important to use
  // the original request hash that it attempted to send, and not the observed
  // request hash that the server sent back to us.
  crypto::hash::Hasher hasher(crypto::hash::HashKind::kSha256);
  hasher.Update(request_hash_);
  hasher.Update(crypto::hash::Sha256(base::as_byte_span(response_body)));
  hasher.Update(base::as_byte_span(request_query_cup2key_));
  std::array<uint8_t, crypto::hash::kSha256Size> inner_hash;
  hasher.Finish(inner_hash);

  // If the verification fails, that implies one of two outcomes:
  // * The signature was modified.
  // * The buffer that the server signed does not match the buffer that the
  //   client assembled -- implying that either request body or response body
  //   was modified, or a different nonce value was used.
  //
  // Note that the signature is taken over a hash of inner_hash (hence signature
  // kind ECDSA_SHA256).
  return crypto::sign::Verify(crypto::sign::SignatureKind::ECDSA_SHA256,
                              public_key_, inner_hash, signature);
}

}  // namespace client_update_protocol
