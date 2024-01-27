// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/challenge_response/cert_utils.h"

#include <string>
#include <string_view>

#include "base/logging.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace ash {

namespace {

bool GetSubjectPublicKeyInfo(const net::X509Certificate& certificate,
                             std::string* spki_der) {
  std::string_view spki_der_piece;
  if (!net::asn1::ExtractSPKIFromDERCert(
          net::x509_util::CryptoBufferAsStringPiece(certificate.cert_buffer()),
          &spki_der_piece)) {
    return false;
  }
  *spki_der = std::string(spki_der_piece);
  return !spki_der->empty();
}

}  // namespace

std::optional<ChallengeResponseKey::SignatureAlgorithm>
GetChallengeResponseKeyAlgorithmFromSsl(uint16_t ssl_algorithm) {
  switch (ssl_algorithm) {
    case SSL_SIGN_RSA_PKCS1_SHA1:
      return ChallengeResponseKey::SignatureAlgorithm::kRsassaPkcs1V15Sha1;
    case SSL_SIGN_RSA_PKCS1_SHA256:
      return ChallengeResponseKey::SignatureAlgorithm::kRsassaPkcs1V15Sha256;
    case SSL_SIGN_RSA_PKCS1_SHA384:
      return ChallengeResponseKey::SignatureAlgorithm::kRsassaPkcs1V15Sha384;
    case SSL_SIGN_RSA_PKCS1_SHA512:
      return ChallengeResponseKey::SignatureAlgorithm::kRsassaPkcs1V15Sha512;
    default:
      // This algorithm is unsupported by ChallengeResponseKey.
      return {};
  }
}

bool ExtractChallengeResponseKeyFromCert(
    const net::X509Certificate& certificate,
    const std::vector<ChallengeResponseKey::SignatureAlgorithm>&
        signature_algorithms,
    ChallengeResponseKey* challenge_response_key) {
  if (signature_algorithms.empty()) {
    LOG(ERROR)
        << "No signature algorithms provided for the challenge-response key";
    return false;
  }
  std::string spki_der;
  if (!GetSubjectPublicKeyInfo(certificate, &spki_der)) {
    LOG(ERROR) << "Failed to extract Subject Public Key Information from the "
                  "given certificate";
    return false;
  }
  challenge_response_key->set_public_key_spki_der(spki_der);
  challenge_response_key->set_signature_algorithms(signature_algorithms);
  return true;
}

}  // namespace ash
