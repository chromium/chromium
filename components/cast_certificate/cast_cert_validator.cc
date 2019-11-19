// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_certificate/cast_cert_validator.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "components/cast_certificate/cast_crl.h"
#include "net/cert/internal/cert_issuer_source_static.h"
#include "net/cert/internal/certificate_policies.h"
#include "net/cert/internal/common_cert_errors.h"
#include "net/cert/internal/extended_key_usage.h"
#include "net/cert/internal/parse_certificate.h"
#include "net/cert/internal/parse_name.h"
#include "net/cert/internal/parsed_certificate.h"
#include "net/cert/internal/path_builder.h"
#include "net/cert/internal/signature_algorithm.h"
#include "net/cert/internal/simple_path_builder_delegate.h"
#include "net/cert/internal/trust_store_in_memory.h"
#include "net/cert/internal/verify_signed_data.h"
#include "net/cert/x509_util.h"
#include "net/der/encode_values.h"
#include "net/der/input.h"

namespace cast_certificate {
namespace {

#define RETURN_STRING_LITERAL(x) \
  case x:                        \
    return #x;

// -------------------------------------------------------------------------
// Cast trust anchors.
// -------------------------------------------------------------------------

// There are two trusted roots for Cast certificate chains:
//
//   (1) CN=Cast Root CA    (kCastRootCaDer)
//   (2) CN=Eureka Root CA  (kEurekaRootCaDer)
//
// These constants are defined by the files included next:

#include "components/cast_certificate/cast_root_ca_cert_der-inc.h"
#include "components/cast_certificate/eureka_root_ca_der-inc.h"

// Singleton for the Cast trust store.
class CastTrustStore {
 public:
  static CastTrustStore* GetInstance() {
    return base::Singleton<CastTrustStore,
                           base::LeakySingletonTraits<CastTrustStore>>::get();
  }

  static net::TrustStore& Get() { return GetInstance()->store_; }

 private:
  friend struct base::DefaultSingletonTraits<CastTrustStore>;

  CastTrustStore() {
    AddAnchor(kCastRootCaDer);
    AddAnchor(kEurekaRootCaDer);
  }

  // Adds a trust anchor given a DER-encoded certificate from static
  // storage.
  template <size_t N>
  void AddAnchor(const uint8_t (&data)[N]) {
    net::CertErrors errors;
    scoped_refptr<net::ParsedCertificate> cert =
        net::ParsedCertificate::CreateWithoutCopyingUnsafe(data, N, {},
                                                           &errors);
    CHECK(cert) << errors.ToDebugString();
    // Enforce pathlen constraints and policies defined on the root certificate.
    store_.AddTrustAnchorWithConstraints(std::move(cert));
  }

  net::TrustStoreInMemory store_;
  DISALLOW_COPY_AND_ASSIGN(CastTrustStore);
};

// Returns the OID for the Audio-Only Cast policy
// (1.3.6.1.4.1.11129.2.5.2) in DER form.
net::der::Input AudioOnlyPolicyOid() {
  static const uint8_t kAudioOnlyPolicy[] = {0x2B, 0x06, 0x01, 0x04, 0x01,
                                             0xD6, 0x79, 0x02, 0x05, 0x02};
  return net::der::Input(kAudioOnlyPolicy);
}

// Cast certificates rely on RSASSA-PKCS#1 v1.5 with SHA-1 for signatures.
//
// The following delegate will allow signature algorithms of:
//
//   * ECDSA, RSA-SSA, and RSA-PSS
//   * Supported EC curves: P-256, P-384, P-521.
//   * Hashes: All SHA hashes including SHA-1 (despite being known weak).
//
// It will also require RSA keys have a modulus at least 2048-bits long.
class CastPathBuilderDelegate : public net::SimplePathBuilderDelegate {
 public:
  CastPathBuilderDelegate()
      : SimplePathBuilderDelegate(
            2048,
            SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1) {}
};

class CertVerificationContextImpl : public CertVerificationContext {
 public:
  // Save a copy of the passed in public key (DER) and common name (text).
  CertVerificationContextImpl(const net::der::Input& spki,
                              const base::StringPiece& common_name)
      : spki_(spki.AsString()), common_name_(common_name.as_string()) {}

  bool VerifySignatureOverData(
      const base::StringPiece& signature,
      const base::StringPiece& data,
      net::DigestAlgorithm digest_algorithm) const override {
    // This code assumes the signature algorithm was RSASSA PKCS#1 v1.5 with
    // |digest_algorithm|.
    auto signature_algorithm =
        net::SignatureAlgorithm::CreateRsaPkcs1(digest_algorithm);

    return net::VerifySignedData(
        *signature_algorithm, net::der::Input(data),
        net::der::BitString(net::der::Input(signature), 0),
        net::der::Input(&spki_));
  }

  std::string GetCommonName() const override { return common_name_; }

 private:
  std::string spki_;
  std::string common_name_;
};

// Helper that extracts the Common Name from a certificate's subject field. On
// success |common_name| contains the text for the attribute (UTF-8, but for
// Cast device certs it should be ASCII).
bool GetCommonNameFromSubject(const net::der::Input& subject_tlv,
                              std::string* common_name) {
  net::RDNSequence rdn_sequence;
  if (!net::ParseName(subject_tlv, &rdn_sequence))
    return false;

  for (const net::RelativeDistinguishedName& rdn : rdn_sequence) {
    for (const auto& atv : rdn) {
      if (atv.type == net::TypeCommonNameOid()) {
        return atv.ValueAsString(common_name);
      }
    }
  }
  return false;
}

// Cast device certificates use the policy 1.3.6.1.4.1.11129.2.5.2 to indicate
// it is *restricted* to an audio-only device whereas the absence of a policy
// means it is unrestricted.
//
// This is somewhat different than RFC 5280's notion of policies, so policies
// are checked separately outside of path building.
//
// See the unit-tests VerifyCastDeviceCertTest.Policies* for some
// concrete examples of how this works.
void DetermineDeviceCertificatePolicy(
    const net::CertPathBuilderResultPath* result_path,
    CastDeviceCertPolicy* policy) {
  // Iterate over all the certificates, including the root certificate. If any
  // certificate contains the audio-only policy, the whole chain is considered
  // constrained to audio-only device certificates.
  //
  // Policy mappings are not accounted for. The expectation is that top-level
  // intermediates issued with audio-only will have no mappings. If subsequent
  // certificates in the chain do, it won't matter as the chain is already
  // restricted to being audio-only.
  bool audio_only = false;
  for (const auto& cert : result_path->certs) {
    if (cert->has_policy_oids()) {
      const std::vector<net::der::Input>& policies = cert->policy_oids();
      if (base::Contains(policies, AudioOnlyPolicyOid())) {
        audio_only = true;
        break;
      }
    }
  }

  *policy = audio_only ? CastDeviceCertPolicy::AUDIO_ONLY
                       : CastDeviceCertPolicy::NONE;
}

// Checks properties on the target certificate.
//
//   * The Key Usage must include Digital Signature
WARN_UNUSED_RESULT bool CheckTargetCertificate(
    const net::ParsedCertificate* cert,
    std::unique_ptr<CertVerificationContext>* context) {
  // Get the Key Usage extension.
  if (!cert->has_key_usage())
    return false;

  // Ensure Key Usage contains digitalSignature.
  if (!cert->key_usage().AssertsBit(net::KEY_USAGE_BIT_DIGITAL_SIGNATURE))
    return false;

  // Get the Common Name for the certificate.
  std::string common_name;
  if (!GetCommonNameFromSubject(cert->tbs().subject_tlv, &common_name))
    return false;

  context->reset(
      new CertVerificationContextImpl(cert->tbs().spki_tlv, common_name));
  return true;
}

// Returns the parsing options used for Cast certificates.
net::ParseCertificateOptions GetCertParsingOptions() {
  net::ParseCertificateOptions options;

  // Some cast intermediate certificates contain serial numbers that are
  // 21 octets long, and might also not use valid DER encoding for an
  // INTEGER (non-minimal encoding).
  //
  // Allow these sorts of serial numbers.
  //
  // TODO(eroman): At some point in the future this workaround will no longer be
  // necessary. Should revisit this for removal in 2017 if not earlier.
  options.allow_invalid_serial_numbers = true;
  return options;
}

// Returns the CastCertError for the failed path building.
// This function must only be called if path building failed.
CastCertError MapToCastError(const net::CertPathBuilder::Result& result) {
  DCHECK(!result.HasValidPath());
  if (result.paths.empty())
    return CastCertError::ERR_CERTS_VERIFY_GENERIC;
  const net::CertPathErrors& path_errors =
      result.paths.at(result.best_result_index)->errors;
  if (path_errors.ContainsError(net::cert_errors::kValidityFailedNotAfter) ||
      path_errors.ContainsError(net::cert_errors::kValidityFailedNotBefore)) {
    return CastCertError::ERR_CERTS_DATE_INVALID;
  }
  return CastCertError::ERR_CERTS_VERIFY_GENERIC;
}

}  // namespace

CastCertError VerifyDeviceCert(
    const std::vector<std::string>& certs,
    const base::Time& time,
    std::unique_ptr<CertVerificationContext>* context,
    CastDeviceCertPolicy* policy,
    const CastCRL* crl,
    CRLPolicy crl_policy) {
  return VerifyDeviceCertUsingCustomTrustStore(
      certs, time, context, policy, crl, crl_policy, &CastTrustStore::Get());
}

CastCertError VerifyDeviceCertUsingCustomTrustStore(
    const std::vector<std::string>& certs,
    const base::Time& time,
    std::unique_ptr<CertVerificationContext>* context,
    CastDeviceCertPolicy* policy,
    const CastCRL* crl,
    CRLPolicy crl_policy,
    net::TrustStore* trust_store) {
  if (!trust_store)
    return VerifyDeviceCert(certs, time, context, policy, crl, crl_policy);

  if (certs.empty())
    return CastCertError::ERR_CERTS_MISSING;

  // Fail early if CRL is required but not provided.
  if (!crl && crl_policy == CRLPolicy::CRL_REQUIRED)
    return CastCertError::ERR_CRL_INVALID;

  net::CertErrors errors;
  scoped_refptr<net::ParsedCertificate> target_cert;
  net::CertIssuerSourceStatic intermediate_cert_issuer_source;
  for (size_t i = 0; i < certs.size(); ++i) {
    scoped_refptr<net::ParsedCertificate> cert(net::ParsedCertificate::Create(
        net::x509_util::CreateCryptoBuffer(certs[i]), GetCertParsingOptions(),
        &errors));
    if (!cert)
      return CastCertError::ERR_CERTS_PARSE;

    if (i == 0)
      target_cert = std::move(cert);
    else
      intermediate_cert_issuer_source.AddCert(std::move(cert));
  }

  CastPathBuilderDelegate path_builder_delegate;

  // Do path building and RFC 5280 compatible certificate verification using the
  // two Cast trust anchors and Cast signature policy.
  net::der::GeneralizedTime verification_time;
  if (!net::der::EncodeTimeAsGeneralizedTime(time, &verification_time))
    return CastCertError::ERR_UNEXPECTED;
  net::CertPathBuilder path_builder(
      target_cert.get(), trust_store, &path_builder_delegate, verification_time,
      net::KeyPurpose::CLIENT_AUTH, net::InitialExplicitPolicy::kFalse,
      {net::AnyPolicy()}, net::InitialPolicyMappingInhibit::kFalse,
      net::InitialAnyPolicyInhibit::kFalse);
  path_builder.AddCertIssuerSource(&intermediate_cert_issuer_source);
  net::CertPathBuilder::Result result = path_builder.Run();
  if (!result.HasValidPath())
    return MapToCastError(result);

  // Determine whether this device certificate is restricted to audio-only.
  DetermineDeviceCertificatePolicy(result.GetBestValidPath(), policy);

  // Check properties of the leaf certificate not already verified by path
  // building (key usage), and construct a CertVerificationContext that uses
  // its public key.
  if (!CheckTargetCertificate(target_cert.get(), context))
    return CastCertError::ERR_CERTS_RESTRICTIONS;

  // Check for revocation.
  if (crl && !crl->CheckRevocation(result.GetBestValidPath()->certs, time))
    return CastCertError::ERR_CERTS_REVOKED;

  return CastCertError::OK;
}

std::unique_ptr<CertVerificationContext> CertVerificationContextImplForTest(
    const base::StringPiece& spki) {
  // Use a bogus CommonName, since this is just exposed for testing signature
  // verification by unittests.
  return std::make_unique<CertVerificationContextImpl>(net::der::Input(spki),
                                                       "CommonName");
}

std::string CastCertErrorToString(CastCertError error) {
  switch (error) {
    RETURN_STRING_LITERAL(CastCertError::ERR_CERTS_MISSING);
    RETURN_STRING_LITERAL(CastCertError::ERR_CERTS_PARSE);
    RETURN_STRING_LITERAL(CastCertError::ERR_CERTS_DATE_INVALID);
    RETURN_STRING_LITERAL(CastCertError::ERR_CERTS_VERIFY_GENERIC);
    RETURN_STRING_LITERAL(CastCertError::ERR_CERTS_RESTRICTIONS);
    RETURN_STRING_LITERAL(CastCertError::ERR_CRL_INVALID);
    RETURN_STRING_LITERAL(CastCertError::ERR_CERTS_REVOKED);
    RETURN_STRING_LITERAL(CastCertError::ERR_UNEXPECTED);
    RETURN_STRING_LITERAL(CastCertError::OK);
  }
  return "CastCertError::UNKNOWN";
}

}  // namespace cast_certificate
