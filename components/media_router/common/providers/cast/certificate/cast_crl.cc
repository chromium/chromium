// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/media_router/common/providers/cast/certificate/cast_crl.h"

#include <unordered_map>
#include <unordered_set>

#include <memory>

#include "base/build_time.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/time/time.h"
#include "components/media_router/common/providers/cast/certificate/cast_fallback_crl.h"
#include "crypto/sha2.h"
#include "net/cert/time_conversions.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/input.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"
#include "third_party/boringssl/src/pki/parse_values.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"
#include "third_party/boringssl/src/pki/path_builder.h"
#include "third_party/boringssl/src/pki/simple_path_builder_delegate.h"
#include "third_party/boringssl/src/pki/trust_store_in_memory.h"
#include "third_party/boringssl/src/pki/verify_certificate_chain.h"
#include "third_party/openscreen/src/cast/common/certificate/proto/revocation.pb.h"

namespace cast_certificate {

using openscreen::cast::proto::Crl;
using openscreen::cast::proto::CrlBundle;
using openscreen::cast::proto::TbsCrl;

namespace {

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
// During fuzz testing, we won't have valid hashes for certificate revocation,
// so we use the empty string as a placeholder where a hash code is needed in
// production.  This allows us to test the revocation logic without needing the
// fuzzing engine to produce a valid hash code.
constexpr char kFakeHashForFuzzing[] = "fake_hash_code";
#endif

enum CrlVersion {
  // version 0: Spki Hash Algorithm = SHA-256
  //            Signature Algorithm = RSA-PKCS1 V1.5 with SHA-256
  CRL_VERSION_0 = 0,
};

// -------------------------------------------------------------------------
// Cast CRL trust anchors.
// -------------------------------------------------------------------------

// There is one trusted root for Cast CRL certificate chains:
//
//   (1) CN=Cast CRL Root CA    (kCastCRLRootCaDer)
//
// These constants are defined by the file included next:

#include "components/media_router/common/providers/cast/certificate/cast_crl_root_ca_cert_der-inc.h"

// When the fallback Cast CRL is used, ignore the back-up Cast CRL’s validity
// range. Instead, use the Build Time as the not_before date and Build Time + 20
// weeks as the not_after time. This is a constant as 20 weeks in seconds
constexpr static int kFallbackCrlValidityInSeconds = 20 * 7 * 24 * 60 * 60;

// Singleton for the Cast CRL trust store.
class CastCRLTrustStore {
 public:
  static CastCRLTrustStore* GetInstance() {
    return base::Singleton<CastCRLTrustStore, base::LeakySingletonTraits<
                                                  CastCRLTrustStore>>::get();
  }

  CastCRLTrustStore(const CastCRLTrustStore&) = delete;
  CastCRLTrustStore& operator=(const CastCRLTrustStore&) = delete;

  static bssl::TrustStore& Get() { return GetInstance()->store_; }

 private:
  friend struct base::DefaultSingletonTraits<CastCRLTrustStore>;

  CastCRLTrustStore() {
    // Initialize the trust store with the root certificate.
    bssl::CertErrors errors;
    std::shared_ptr<const bssl::ParsedCertificate> cert =
        bssl::ParsedCertificate::Create(
            net::x509_util::CreateCryptoBufferFromStaticDataUnsafe(
                kCastCRLRootCaDer),
            {}, &errors);
    CHECK(cert) << errors.ToDebugString();
    // Enforce pathlen constraints and policies defined on the root certificate.
    store_.AddTrustAnchorWithConstraints(std::move(cert));
  }

  bssl::TrustStoreInMemory store_;
};

// Converts a uint64_t unix timestamp to net::der::GeneralizedTime.
bool ConvertTimeSeconds(uint64_t seconds,
                        bssl::der::GeneralizedTime* generalized_time) {
  base::Time unix_timestamp =
      base::Time::UnixEpoch() +
      base::Seconds(base::saturated_cast<int64_t>(seconds));
  return net::EncodeTimeAsGeneralizedTime(unix_timestamp, generalized_time);
}

// Verifies the CRL is signed by a trusted CRL authority at the time the CRL
// was issued. Verifies the signature of |tbs_crl| is valid based on the
// certificate and signature in |crl|. The validity of |tbs_crl| is verified
// at |time|. The validity period of the CRL is adjusted to be the earliest
// of the issuer certificate chain's expiration and the CRL's expiration and
// the result is stored in |overall_not_after|.
// |is_fallback_crl| states whether a fallback CRL is used. When true,
// ignore the validity range of the fallback CRL's issuer certificate.
bool VerifyCRL(const Crl& crl,
               const TbsCrl& tbs_crl,
               const base::Time& time,
               bssl::TrustStore* trust_store,
               bssl::der::GeneralizedTime* overall_not_after,
               bool is_fallback_crl) {
  if (!crl.has_signature() || !crl.has_signer_cert()) {
    VLOG(2) << "CRL - Missing fields";
    return false;
  }

  bssl::CertErrors parse_errors;
  std::shared_ptr<const bssl::ParsedCertificate> parsed_cert =
      bssl::ParsedCertificate::Create(
          net::x509_util::CreateCryptoBuffer(crl.signer_cert()), {},
          &parse_errors);
  if (parsed_cert == nullptr) {
    VLOG(2) << "CRL - Issuer certificate parsing failed:\n"
            << parse_errors.ToDebugString();
    return false;
  }

  CBS spki;
  CBS_init(&spki, parsed_cert->tbs().spki_tlv.data(),
           parsed_cert->tbs().spki_tlv.size());
  bssl::UniquePtr<EVP_PKEY> pubkey(EVP_parse_public_key(&spki));
  if (!pubkey || CBS_len(&spki) != 0) {
    VLOG(2) << "CRL - Parsing public key failed";
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    return false;
#endif
  }

  // Verify the signature in the CRL. It should be signed with RSASSA-PKCS1-v1_5
  // and SHA-256.
  auto signature_bytes = base::as_bytes(base::make_span(crl.signature()));
  auto tbs_crl_bytes = base::as_bytes(base::make_span(crl.tbs_crl()));
  bssl::ScopedEVP_MD_CTX ctx;
  if (EVP_PKEY_id(pubkey.get()) != EVP_PKEY_RSA ||
      !EVP_DigestVerifyInit(ctx.get(), nullptr, EVP_sha256(), nullptr,
                            pubkey.get()) ||
      !EVP_DigestVerify(ctx.get(), signature_bytes.data(),
                        signature_bytes.size(), tbs_crl_bytes.data(),
                        tbs_crl_bytes.size())) {
    VLOG(2) << "CRL - Signature verification failed";
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    return false;
#endif
  }

  // Verify the issuer certificate.
  bssl::der::GeneralizedTime issuer_verification_time;
  base::Time time_issuer_check = time;
  if (is_fallback_crl) {
    time_issuer_check = base::Time::FromTimeT(kCastFallbackCRLTimestamp);
    VLOG(2) << "CRL - Issuer certifcate's verification time overridden for the "
               "fallback CRL.";
  }
  if (!net::EncodeTimeAsGeneralizedTime(time_issuer_check,
                                        &issuer_verification_time)) {
    VLOG(2) << "CRL - Unable to parse verification time for issuer certificate "
               "check.";
    return false;
  }

  // SimplePathBuilderDelegate will enforce required signature algorithms of
  // RSASSA PKCS#1 v1.5 with SHA-256, and RSA keys 2048-bits or longer.
  bssl::SimplePathBuilderDelegate path_builder_delegate(
      2048, bssl::SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1);

  // Verify the trust of the CRL authority.
  bssl::CertPathBuilder path_builder(
      parsed_cert, trust_store, &path_builder_delegate,
      issuer_verification_time, bssl::KeyPurpose::ANY_EKU,
      bssl::InitialExplicitPolicy::kFalse,
      {bssl::der::Input(bssl::kAnyPolicyOid)},
      bssl::InitialPolicyMappingInhibit::kFalse,
      bssl::InitialAnyPolicyInhibit::kFalse);
  bssl::CertPathBuilder::Result result = path_builder.Run();
  if (!result.HasValidPath()) {
    VLOG(2) << "CRL - Issuer certificate verification failed.";
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // TODO(crbug.com/41267838): Log the error information.
    return false;
#endif
  }
  // There are no requirements placed on the leaf certificate having any
  // particular KeyUsages. Leaf certificate checks are bypassed.

  // Verify the CRL is still valid.
  bssl::der::GeneralizedTime crl_verification_time;
  if (!net::EncodeTimeAsGeneralizedTime(time, &crl_verification_time)) {
    VLOG(2)
        << "CRL - Unable to parse verification time for CRL validity check.";
    return false;
  }
  bssl::der::GeneralizedTime not_before;
  if (!ConvertTimeSeconds(tbs_crl.not_before_seconds(), &not_before)) {
    VLOG(2) << "CRL - Unable to parse not_before.";
    return false;
  }
  bssl::der::GeneralizedTime not_after;
  if (!ConvertTimeSeconds(tbs_crl.not_after_seconds(), &not_after)) {
    VLOG(2) << "CRL - Unable to parse not_after.";
    return false;
  }
  if ((crl_verification_time < not_before) ||
      (crl_verification_time > not_after)) {
    VLOG(2) << "CRL - Not time-valid.";
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    return false;
#endif
  }

  // Set CRL expiry to the earliest of the cert chain expiry and CRL expiry.
  // Note that the trust anchor is not part of this loop.
  // "expiration" of the trust anchor is handled instead by its
  // presence in the trust store.
  *overall_not_after = not_after;
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
  // We don't expect to have a valid path during fuzz testing, so just use a
  // single cert.
  const bssl::ParsedCertificateList path_certs = {parsed_cert};
#else
  const bssl::ParsedCertificateList& path_certs =
      result.GetBestValidPath()->certs;
#endif
  if (!is_fallback_crl) {
    for (const auto& cert : path_certs) {
      bssl::der::GeneralizedTime cert_not_after =
          cert->tbs().validity_not_after;
      if (cert_not_after < *overall_not_after) {
        *overall_not_after = cert_not_after;
      }
    }
  }

  // Perform sanity check on serial numbers.
  for (const auto& range : tbs_crl.revoked_serial_number_ranges()) {
    uint64_t first_serial_number = range.first_serial_number();
    uint64_t last_serial_number = range.last_serial_number();
    if (last_serial_number < first_serial_number) {
      VLOG(2) << "CRL - Malformed serial number range.";
      return false;
    }
  }
  return true;
}

class CastCRLImpl : public CastCRL {
 public:
  CastCRLImpl(const TbsCrl& tbs_crl,
              const bssl::der::GeneralizedTime& overall_not_after);

  CastCRLImpl(const CastCRLImpl&) = delete;
  CastCRLImpl& operator=(const CastCRLImpl&) = delete;

  ~CastCRLImpl() override;

  bool CheckRevocation(const bssl::ParsedCertificateList& trusted_chain,
                       const base::Time& time) const override;

 private:
  struct SerialNumberRange {
    uint64_t first_serial;
    uint64_t last_serial;
  };

  bssl::der::GeneralizedTime not_before_;
  bssl::der::GeneralizedTime not_after_;

  // Revoked public key hashes.
  // The values consist of the SHA256 hash of the SubjectPublicKeyInfo.
  std::unordered_set<std::string> revoked_hashes_;

  // Revoked serial number ranges indexed by issuer public key hash.
  // The key is the SHA256 hash of issuer's SubjectPublicKeyInfo.
  // The value is a list of revoked serial number ranges.
  std::unordered_map<std::string, std::vector<SerialNumberRange>>
      revoked_serial_numbers_;
};

CastCRLImpl::CastCRLImpl(const TbsCrl& tbs_crl,
                         const bssl::der::GeneralizedTime& overall_not_after) {
  // Parse the validity information.
  // Assume ConvertTimeSeconds will succeed. Successful call to VerifyCRL
  // means that these calls were successful.
  ConvertTimeSeconds(tbs_crl.not_before_seconds(), &not_before_);
  ConvertTimeSeconds(tbs_crl.not_after_seconds(), &not_after_);
  if (overall_not_after < not_after_)
    not_after_ = overall_not_after;

  // Parse the revoked hashes.
  for (const auto& hash : tbs_crl.revoked_public_key_hashes()) {
    revoked_hashes_.insert(hash);
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // Save fake hash code for later lookups.
    revoked_hashes_.insert(kFakeHashForFuzzing);
#endif
  }

  // Parse the revoked serial ranges.
  for (const auto& range : tbs_crl.revoked_serial_number_ranges()) {
    std::string issuer_hash = range.issuer_public_key_hash();
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // Save range under fake hash code for later lookups.
    issuer_hash = kFakeHashForFuzzing;
#endif

    uint64_t first_serial_number = range.first_serial_number();
    uint64_t last_serial_number = range.last_serial_number();
    auto& serial_number_range = revoked_serial_numbers_[issuer_hash];
    serial_number_range.push_back({first_serial_number, last_serial_number});
  }
}

CastCRLImpl::~CastCRLImpl() = default;

// Verifies the revocation status of the certificate chain, at the specified
// time.
bool CastCRLImpl::CheckRevocation(
    const bssl::ParsedCertificateList& trusted_chain,
    const base::Time& time) const {
  if (trusted_chain.empty())
    return false;

  // Check the validity of the CRL at the specified time.
  bssl::der::GeneralizedTime verification_time;
  if (!net::EncodeTimeAsGeneralizedTime(time, &verification_time)) {
    VLOG(2) << "CRL verification time malformed.";
    return false;
  }
  if ((verification_time < not_before_) || (verification_time > not_after_)) {
    VLOG(2) << "CRL not time-valid. Perform hard fail.";
    return false;
  }

  // Check revocation. This loop iterates over both certificates AND then the
  // trust anchor after exhausting the certs.
  for (size_t i = 0; i < trusted_chain.size(); ++i) {
    const bssl::der::Input& spki_tlv = trusted_chain[i]->tbs().spki_tlv;

    // Calculate the public key's hash to check for revocation.
    std::string spki_hash = crypto::SHA256HashString(spki_tlv.AsString());
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // Revocation data (if any) was saved in the constructor using this fake
    // hash code.
    spki_hash = kFakeHashForFuzzing;
#endif
    if (revoked_hashes_.find(spki_hash) != revoked_hashes_.end()) {
      VLOG(2) << "Public key is revoked.";
      return false;
    }

    // Check if the subordinate certificate was revoked by serial number.
    if (i > 0) {
      auto issuer_iter = revoked_serial_numbers_.find(spki_hash);
      if (issuer_iter != revoked_serial_numbers_.end()) {
        const auto& subordinate = trusted_chain[i - 1];
        uint64_t serial_number;
        // Only Google generated device certificates will be revoked by range.
        // These will always be less than 64 bits in length.
        if (!bssl::der::ParseUint64(subordinate->tbs().serial_number,
                                    &serial_number)) {
          continue;
        }
        for (const auto& revoked_serial : issuer_iter->second) {
          if (revoked_serial.first_serial <= serial_number &&
              revoked_serial.last_serial >= serial_number) {
            VLOG(2) << "Serial number is revoked";
            return false;
          }
        }
      }
    }
  }
  return true;
}

}  // namespace

std::unique_ptr<CastCRL> ParseAndVerifyCRL(const std::string& crl_proto,
                                           const base::Time& time,
                                           const bool is_fallback_crl) {
  return ParseAndVerifyCRLUsingCustomTrustStore(
      crl_proto, time, &CastCRLTrustStore::Get(), is_fallback_crl);
}

std::unique_ptr<CastCRL> ParseAndVerifyCRLUsingCustomTrustStore(
    const std::string& crl_proto,
    const base::Time& time,
    bssl::TrustStore* trust_store,
    const bool is_fallback_crl) {
  if (!trust_store)
    return ParseAndVerifyCRL(crl_proto, time, is_fallback_crl);

  CrlBundle crl_bundle;
  if (!crl_bundle.ParseFromString(crl_proto)) {
    LOG(ERROR) << "CRL - Binary could not be parsed.";
    return nullptr;
  }
  for (auto const& crl : crl_bundle.crls()) {
    TbsCrl tbs_crl;
    if (!tbs_crl.ParseFromString(crl.tbs_crl())) {
      LOG(WARNING) << "Binary TBS CRL could not be parsed.";
      continue;
    }
    if (tbs_crl.version() != CRL_VERSION_0) {
      continue;
    }

    if (is_fallback_crl) {
      tbs_crl.set_not_before_seconds(base::GetBuildTime().ToTimeT());
      tbs_crl.set_not_after_seconds(base::GetBuildTime().ToTimeT() +
                                    kFallbackCrlValidityInSeconds);
    }

    bssl::der::GeneralizedTime overall_not_after;
    if (!VerifyCRL(crl, tbs_crl, time, trust_store, &overall_not_after,
                   is_fallback_crl)) {
      LOG(ERROR) << "CRL - Verification failed.";
      return nullptr;
    }
    return std::make_unique<CastCRLImpl>(tbs_crl, overall_not_after);
  }
  LOG(ERROR) << "No supported version of revocation data.";
  return nullptr;
}

std::unique_ptr<CastCRL> ParseAndVerifyFallbackCRLUsingCustomTrustStore(
    const base::Time& time,
    bssl::TrustStore* trust_store) {
  std::string fallback_serialized_crl(
      kCastFallbackCRLs, kCastFallbackCRLs + sizeof kCastFallbackCRLs /
                                                 sizeof kCastFallbackCRLs[0]);
  return ParseAndVerifyCRLUsingCustomTrustStore(
      fallback_serialized_crl, time, trust_store, true /* is_fallback_crl */);
}

}  // namespace cast_certificate
