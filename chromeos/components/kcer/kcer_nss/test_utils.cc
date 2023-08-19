// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/kcer/kcer_nss/test_utils.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/signature_verifier.h"

using SignatureAlgorithm = crypto::SignatureVerifier::SignatureAlgorithm;

namespace kcer {

TokenHolder::TokenHolder(Token token, bool initialize) {
  io_token_ = std::make_unique<internal::KcerTokenImplNss>(token);
  io_token_->SetAttributeTranslationForTesting(/*is_enabled=*/true);
  weak_ptr_ = io_token_->GetWeakPtr();
  // After this point `io_token_` should only be used on the IO thread.

  if (initialize) {
    Initialize();
  }
}

TokenHolder::~TokenHolder() {
  weak_ptr_.reset();
  content::GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                                 std::move(io_token_));
}

void TokenHolder::Initialize() {
  CHECK(!is_initialized_);
  is_initialized_ = true;

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &internal::KcerTokenImplNss::Initialize, weak_ptr_,
          crypto::ScopedPK11Slot(PK11_ReferenceSlot(nss_slot_.slot()))));
}

void TokenHolder::FailInitialization() {
  CHECK(!is_initialized_);
  is_initialized_ = true;

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&internal::KcerTokenImplNss::Initialize, weak_ptr_,
                     /*nss_slot=*/nullptr));
}

//==============================================================================

bool KeyPermissionsEqual(const absl::optional<chaps::KeyPermissions>& a,
                         const absl::optional<chaps::KeyPermissions>& b) {
  if (!a.has_value() || !b.has_value()) {
    return (a.has_value() == b.has_value());
  }
  return (a->SerializeAsString() == b->SerializeAsString());
}

bool VerifySignature(SigningScheme signing_scheme,
                     PublicKeySpki spki,
                     DataToSign data_to_sign,
                     Signature signature,
                     bool strict) {
  SignatureAlgorithm signature_algo = SignatureAlgorithm::RSA_PKCS1_SHA1;
  switch (signing_scheme) {
    case SigningScheme::kRsaPkcs1Sha1:
      signature_algo = SignatureAlgorithm::RSA_PKCS1_SHA1;
      break;
    case SigningScheme::kRsaPkcs1Sha256:
      signature_algo = SignatureAlgorithm::RSA_PKCS1_SHA256;
      break;
    case SigningScheme::kRsaPssRsaeSha256:
      signature_algo = SignatureAlgorithm::RSA_PSS_SHA256;
      break;
    case SigningScheme::kEcdsaSecp256r1Sha256:
      signature_algo = SignatureAlgorithm::ECDSA_SHA256;
      break;
    default:
      return !strict;
  }

  crypto::SignatureVerifier signature_verifier;
  if (!signature_verifier.VerifyInit(signature_algo, signature.value(),
                                     spki.value())) {
    LOG(ERROR) << "Failed to initialize signature verifier";
    return false;
  }
  signature_verifier.VerifyUpdate(data_to_sign.value());
  return signature_verifier.VerifyFinal();
}

std::vector<uint8_t> PrependSHA256DigestInfo(base::span<const uint8_t> hash) {
  // DER-encoded PKCS#1 DigestInfo "prefix" with
  // AlgorithmIdentifier=id-sha256.
  // The encoding is taken from https://tools.ietf.org/html/rfc3447#page-43
  const std::vector<uint8_t> kDigestInfoSha256DerData = {
      0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
      0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20};

  std::vector<uint8_t> result;
  result.reserve(kDigestInfoSha256DerData.size() + hash.size());

  result.insert(result.end(), kDigestInfoSha256DerData.begin(),
                kDigestInfoSha256DerData.end());
  result.insert(result.end(), hash.begin(), hash.end());
  return result;
}

}  // namespace kcer
