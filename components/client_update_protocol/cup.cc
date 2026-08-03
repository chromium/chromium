// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/client_update_protocol/cup.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"
#include "crypto/random.h"
#include "crypto/sign.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace client_update_protocol {

class SigningStrategy {
 public:
  SigningStrategy(int key_version, crypto::keypair::PublicKey public_key)
      : key_version_(key_version), public_key_(std::move(public_key)) {}
  SigningStrategy(const SigningStrategy&) = delete;
  SigningStrategy& operator=(const SigningStrategy&) = delete;

  virtual ~SigningStrategy() = default;

  std::string GetKeyId() const { return GetKeyIdForVersion(key_version_); }

  std::string GetKeyIdForVersion(int version) const {
    return GetKeyIdForVersionImpl(version);
  }

  bool HasValidSignatureLength(base::span<const uint8_t> signature) const {
    return HasValidSignatureLengthImpl(signature);
  }

  bool VerifySignature(base::span<const uint8_t> digest,
                       base::span<const uint8_t> signature) const {
    return crypto::sign::Verify(GetSignatureKind(), public_key_, digest,
                                signature);
  }

 private:
  virtual std::string GetKeyIdForVersionImpl(int version) const = 0;
  virtual crypto::sign::SignatureKind GetSignatureKind() const = 0;
  virtual bool HasValidSignatureLengthImpl(
      base::span<const uint8_t> signature) const = 0;

  // The server keeps multiple signing keys; a version must be sent so that
  // the correct signing key is used to sign the assembled message.
  const int key_version_;

  // The public key (ECDSA or ML-DSA-44) to use for verifying response
  // signatures.
  const crypto::keypair::PublicKey public_key_;
};

namespace {

class EcdsaSigningStrategy : public SigningStrategy {
 public:
  using SigningStrategy::SigningStrategy;

 private:
  std::string GetKeyIdForVersionImpl(int version) const override {
    return base::NumberToString(version);
  }

  crypto::sign::SignatureKind GetSignatureKind() const override {
    return crypto::sign::SignatureKind::ECDSA_SHA256;
  }

  bool HasValidSignatureLengthImpl(
      base::span<const uint8_t> signature) const override {
    // Ensure this is a valid ECDSA signature, which must have a length between
    // 8 and 72 bytes, inclusive.
    return signature.size() >= 8 && signature.size() <= 72;
  }
};

class Mldsa44SigningStrategy : public SigningStrategy {
 public:
  using SigningStrategy::SigningStrategy;

 private:
  std::string GetKeyIdForVersionImpl(int version) const override {
    return absl::StrFormat("ML-DSA-44-%d", version);
  }

  crypto::sign::SignatureKind GetSignatureKind() const override {
    return crypto::sign::SignatureKind::MLDSA_44;
  }

  bool HasValidSignatureLengthImpl(
      base::span<const uint8_t> signature) const override {
    // Valid ML-DSA-44 signatures are exactly 2,420 bytes.
    return signature.size() == 2420;
  }
};

}  // namespace

std::unique_ptr<const SigningStrategy> Cup::CreateSigningStrategy(
    int key_version,
    base::span<const uint8_t> public_key) {
  std::optional<crypto::keypair::PublicKey> key =
      crypto::keypair::PublicKey::FromSubjectPublicKeyInfo(public_key);
  if (!key) {
    return nullptr;
  }
  if (key->IsMldsa44()) {
    return std::make_unique<Mldsa44SigningStrategy>(key_version,
                                                    std::move(*key));
  }
  if (key->IsEc()) {
    return std::make_unique<EcdsaSigningStrategy>(key_version, std::move(*key));
  }
  return nullptr;
}

Cup::Cup(int key_version, base::span<const uint8_t> public_key)
    : strategy_(CreateSigningStrategy(key_version, public_key)) {
  CHECK_GT(key_version, 0);
  CHECK(strategy_);
}

Cup::~Cup() = default;

bool Cup::ParseETagHeader(std::string_view etag_header_value_in,
                          std::vector<uint8_t>* signature_out,
                          std::vector<uint8_t>* request_hash_out) const {
  // The ETag value is a UTF-8 string, formatted as "S:H", where:
  // * S is the signature in DER-encoded ASN.1 or raw format, converted to hex.
  // * H is the SHA-256 hash of the observed request body, standard hex format.
  // A Weak ETag is formatted as W/"S:H". This function treats it the same as a
  // strong ETag.
  std::string_view etag_header_value(etag_header_value_in);

  // Remove the weak prefix, then remove the begin and the end quotes.
  static constexpr std::string_view kWeakETagPrefix = "W/";
  if (etag_header_value.starts_with(kWeakETagPrefix)) {
    etag_header_value.remove_prefix(kWeakETagPrefix.size());
  }
  if (etag_header_value.size() >= 2 && etag_header_value.starts_with('"') &&
      etag_header_value.ends_with('"')) {
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

  if (!base::HexStringToBytes(sig_hex, signature_out)) {
    return false;
  }
  if (!strategy_->HasValidSignatureLength(*signature_out)) {
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

void Cup::OverrideNonceForTesting(int key_version, uint32_t nonce) {
  CHECK(!request_query_cup2key_.empty());
  request_query_cup2key_ = absl::StrFormat(
      "%s:%u", strategy_->GetKeyIdForVersion(key_version), nonce);
}

std::string Cup::PrepareRequestParameters(std::string_view request_body) {
  // Generate a random nonce to use for freshness, build the cup2key query
  // string, and compute the SHA-256 hash of the request body. Set these
  // two pieces of data aside to use during ValidateResponse().
  std::array<uint8_t, 32> nonce = {};
  crypto::RandBytes(nonce);

  // The nonce is an opaque string to the server, so the exact encoding does not
  // matter. Use base64url as it is slightly more compact than hex.
  std::string nonce_b64;
  base::Base64UrlEncode(nonce, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &nonce_b64);

  request_query_cup2key_ =
      absl::StrFormat("%s:%s", strategy_->GetKeyId(), nonce_b64);
  request_hash_ = crypto::hash::Sha256(base::as_byte_span(request_body));

  return absl::StrFormat("cup2key=%s&cup2hreq=%s", request_query_cup2key_,
                         base::HexEncodeLower(request_hash_));
}

bool Cup::ValidateResponse(std::string_view response_body,
                           std::string_view server_etag) {
  CHECK(!request_hash_.empty());
  CHECK(!request_query_cup2key_.empty());

  if (response_body.empty() || server_etag.empty()) {
    return false;
  }

  // Break the ETag into its two components (the signature, and the
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
  std::array<uint8_t, crypto::hash::kSha256Size> inner_hash = {};
  hasher.Finish(inner_hash);

  // If the verification fails, that implies one of two outcomes:
  // * The signature was modified.
  // * The buffer that the server signed does not match the buffer that the
  //   client assembled -- implying that either request body or response body
  //   was modified, or a different nonce value was used.
  //
  // Note that the signature is taken over a hash of inner_hash.
  return strategy_->VerifySignature(inner_hash, signature);
}

}  // namespace client_update_protocol
