// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/kcer/kcer_utils.h"

#include "chromeos/ash/components/kcer/kcer.h"

namespace kcer {

std::vector<SigningScheme> GetSupportedSigningSchemes(bool supports_pss,
                                                      KeyType key_type) {
  std::vector<SigningScheme> result;

  switch (key_type) {
      // Supported signing schemes for RSA also depend on the key length, but
      // NSS doesn't seem to provide a convenient interface to read it. 2048 bit
      // keys are big enough for all RSA signatures, smaller keys are not really
      // used in practice nowadays and the TLS stack is expected to also double
      // check and shrink the list.
    case KeyType::kRsa:
      result.insert(result.end(), {
                                      SigningScheme::kRsaPkcs1Sha1,
                                      SigningScheme::kRsaPkcs1Sha256,
                                      SigningScheme::kRsaPkcs1Sha384,
                                      SigningScheme::kRsaPkcs1Sha512,
                                  });
      if (supports_pss) {
        result.insert(result.end(), {
                                        SigningScheme::kRsaPssRsaeSha256,
                                        SigningScheme::kRsaPssRsaeSha384,
                                        SigningScheme::kRsaPssRsaeSha512,
                                    });
      }
      break;
    case KeyType::kEcc:
      result.insert(result.end(), {
                                      SigningScheme::kEcdsaSecp256r1Sha256,
                                      SigningScheme::kEcdsaSecp384r1Sha384,
                                      SigningScheme::kEcdsaSecp521r1Sha512,
                                  });
  }

  return result;
}

base::expected<std::vector<uint8_t>, Error> ReencodeEcSignatureAsAsn1(
    base::span<const uint8_t> signature) {
  if (signature.size() % 2 != 0) {
    return base::unexpected(Error::kFailedToSignBadSignatureLength);
  }
  size_t order_size_bytes = signature.size() / 2;
  base::span<const uint8_t> r_bytes = signature.first(order_size_bytes);
  base::span<const uint8_t> s_bytes = signature.subspan(order_size_bytes);

  // Convert the RAW ECDSA signature to a DER-encoded ECDSA-Sig-Value.
  bssl::UniquePtr<ECDSA_SIG> sig(ECDSA_SIG_new());
  if (!sig || !BN_bin2bn(r_bytes.data(), r_bytes.size(), sig->r) ||
      !BN_bin2bn(s_bytes.data(), s_bytes.size(), sig->s)) {
    return base::unexpected(Error::kFailedToDerEncode);
  }

  std::vector<uint8_t> result_signature;

  {
    const int len = i2d_ECDSA_SIG(sig.get(), nullptr);
    if (len <= 0) {
      return base::unexpected(Error::kFailedToSignBadSignatureLength);
    }
    result_signature.resize(len);
  }

  {
    uint8_t* ptr = result_signature.data();
    const int len = i2d_ECDSA_SIG(sig.get(), &ptr);
    if (len <= 0) {
      return base::unexpected(Error::kFailedToDerEncode);
    }
  }

  return result_signature;
}

}  // namespace kcer
