// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/session_binding_utils.h"

#include <optional>
#include <string_view>

#include "base/base64url.h"
#include "base/containers/span.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/signin/public/base/hybrid_encryption_key.h"
#include "crypto/sha2.h"
#include "crypto/signature_verifier.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "url/gurl.h"

namespace signin {

namespace {

// Source: JSON Web Signature and Encryption Algorithms
// https://www.iana.org/assignments/jose/jose.xhtml
std::string SignatureAlgorithmToString(
    crypto::SignatureVerifier::SignatureAlgorithm algorithm) {
  switch (algorithm) {
    case crypto::SignatureVerifier::ECDSA_SHA256:
      return "ES256";
    case crypto::SignatureVerifier::RSA_PKCS1_SHA256:
      return "RS256";
    case crypto::SignatureVerifier::RSA_PSS_SHA256:
      return "PS256";
    case crypto::SignatureVerifier::RSA_PKCS1_SHA1:
      return "RS1";
  }
}

std::string Base64UrlEncode(std::string_view data) {
  std::string output;
  base::Base64UrlEncode(data, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &output);
  return output;
}

std::string Base64UrlEncode(base::span<const uint8_t> data) {
  return Base64UrlEncode(std::string_view(
      reinterpret_cast<const char*>(data.data()), data.size()));
}

base::Value::Dict CreatePublicKeyInfo(base::span<const uint8_t> pubkey) {
  return base::Value::Dict()
      .Set("kty",
           "accounts.google.com/.well-known/kty/"
           "SubjectPublicKeyInfo")
      .Set("SubjectPublicKeyInfo", Base64UrlEncode(pubkey));
}

base::Value::Dict CreateHybridPublicKeyInfo(const HybridEncryptionKey& key) {
  return base::Value::Dict()
      .Set("kty",
           "type.googleapis.com/google.crypto.tink.EciesAeadHkdfPublicKey")
      .Set("TinkKeysetPublicKeyInfo", Base64UrlEncode(key.ExportPublicKey()));
}

std::optional<std::string> CreateHeaderAndPayloadWithCustomPayload(
    crypto::SignatureVerifier::SignatureAlgorithm algorithm,
    std::string_view schema,
    const base::Value::Dict& payload) {
  auto header = base::Value::Dict()
                    .Set("alg", SignatureAlgorithmToString(algorithm))
                    .Set("typ", "jwt");
  if (!schema.empty()) {
    header.Set("schema", schema);
  }
  std::optional<std::string> header_serialized = base::WriteJson(header);
  if (!header_serialized) {
    DVLOG(1) << "Unexpected JSONWriter error while serializing a registration "
                "token header";
    return std::nullopt;
  }

  std::optional<std::string> payload_serialized = base::WriteJsonWithOptions(
      payload, base::JSONWriter::OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION);
  if (!payload_serialized) {
    DVLOG(1) << "Unexpected JSONWriter error while serializing a registration "
                "token payload";
    return std::nullopt;
  }

  return base::StrCat({Base64UrlEncode(*header_serialized), ".",
                       Base64UrlEncode(*payload_serialized)});
}

std::optional<std::vector<uint8_t>> ConvertDERSignatureToRaw(
    base::span<const uint8_t> der_signature) {
  bssl::UniquePtr<ECDSA_SIG> ecdsa_sig(
      ECDSA_SIG_from_bytes(der_signature.data(), der_signature.size()));
  if (!ecdsa_sig) {
    DVLOG(1) << "Failed to create ECDSA_SIG";
    return {};
  }

  // TODO(b/301888680): this implicitly depends on a curve used by
  // `crypto::UnexportableKey`. Make this dependency more explicit.
  const size_t kMaxBytesPerBN = 32;
  std::vector<uint8_t> jwt_signature(2 * kMaxBytesPerBN);

  if (!BN_bn2bin_padded(&jwt_signature[0], kMaxBytesPerBN, ecdsa_sig->r) ||
      !BN_bn2bin_padded(&jwt_signature[kMaxBytesPerBN], kMaxBytesPerBN,
                        ecdsa_sig->s)) {
    DVLOG(1) << "Failed to serialize R and S to " << kMaxBytesPerBN << " bytes";
    return {};
  }

  return jwt_signature;
}

}  // namespace

std::optional<crypto::SignatureVerifier::SignatureAlgorithm>
SignatureAlgorithmFromString(std::string_view algorithm) {
  if (base::EqualsCaseInsensitiveASCII(algorithm, "ES256")) {
    return crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256;
  }

  if (base::EqualsCaseInsensitiveASCII(algorithm, "RS256")) {
    return crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256;
  }

  return std::nullopt;
}

std::vector<crypto::SignatureVerifier::SignatureAlgorithm>
ParseSignatureAlgorithmList(std::string_view algorithm_list) {
  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> result;
  for (const auto& algorithm_str : base::SplitStringPiece(
           algorithm_list, " ", base::WhitespaceHandling::TRIM_WHITESPACE,
           base::SplitResult::SPLIT_WANT_NONEMPTY)) {
    std::optional<crypto::SignatureVerifier::SignatureAlgorithm> algorithm =
        signin::SignatureAlgorithmFromString(algorithm_str);
    if (algorithm) {
      result.push_back(*algorithm);
    }
  }
  return result;
}

std::optional<std::string> CreateKeyRegistrationHeaderAndPayloadForTokenBinding(
    std::string_view client_id,
    std::string_view auth_code,
    const GURL& registration_url,
    crypto::SignatureVerifier::SignatureAlgorithm algorithm,
    base::span<const uint8_t> pubkey,
    base::Time timestamp) {
  auto payload =
      base::Value::Dict()
          .Set("sub", client_id)
          .Set("aud", registration_url.spec())
          .Set("jti", Base64UrlEncode(crypto::SHA256HashString(auth_code)))
          // Write out int64_t variable as a double.
          // Note: this may discard some precision, but for `base::Value`
          // there's no other option.
          .Set("iat", static_cast<double>(
                          (timestamp - base::Time::UnixEpoch()).InSeconds()))
          .Set("key", CreatePublicKeyInfo(pubkey));
  return CreateHeaderAndPayloadWithCustomPayload(algorithm, /*schema=*/"",
                                                 payload);
}

std::optional<std::string>
CreateKeyRegistrationHeaderAndPayloadForSessionBinding(
    std::string_view challenge,
    const GURL& registration_url,
    crypto::SignatureVerifier::SignatureAlgorithm algorithm,
    base::span<const uint8_t> pubkey,
    base::Time timestamp) {
  auto payload =
      base::Value::Dict()
          .Set("aud", registration_url.spec())
          .Set("jti", challenge)
          // Write out int64_t variable as a double.
          // Note: this may discard some precision, but for `base::Value`
          // there's no other option.
          .Set("iat", static_cast<double>(
                          (timestamp - base::Time::UnixEpoch()).InSeconds()))
          .Set("key", CreatePublicKeyInfo(pubkey));
  return CreateHeaderAndPayloadWithCustomPayload(algorithm, /*schema=*/"",
                                                 payload);
}

std::optional<std::string> CreateKeyAssertionHeaderAndPayload(
    crypto::SignatureVerifier::SignatureAlgorithm algorithm,
    base::span<const uint8_t> pubkey,
    std::string_view client_id,
    std::string_view challenge,
    const GURL& destination_url,
    std::string_view name_space,
    HybridEncryptionKey* ephemeral_key) {
  auto payload = base::Value::Dict()
                     .Set("sub", client_id)
                     .Set("aud", destination_url.spec())
                     .Set("jti", challenge)
                     .Set("iss", Base64UrlEncode(crypto::SHA256Hash(pubkey)))
                     .Set("namespace", name_space);
  if (ephemeral_key) {
    payload.Set("ephemeral_key", CreateHybridPublicKeyInfo(*ephemeral_key));
  }
  return CreateHeaderAndPayloadWithCustomPayload(
      algorithm, "DEVICE_BOUND_SESSION_CREDENTIALS_ASSERTION", payload);
}

std::optional<std::string> AppendSignatureToHeaderAndPayload(
    std::string_view header_and_payload,
    crypto::SignatureVerifier::SignatureAlgorithm algorithm,
    base::span<const uint8_t> signature) {
  std::optional<std::vector<uint8_t>> signature_holder;
  if (algorithm == crypto::SignatureVerifier::ECDSA_SHA256) {
    signature_holder = ConvertDERSignatureToRaw(signature);
    if (!signature_holder.has_value()) {
      return std::nullopt;
    }
    signature = base::make_span(*signature_holder);
  }

  return base::StrCat({header_and_payload, ".", Base64UrlEncode(signature)});
}

}  // namespace signin
