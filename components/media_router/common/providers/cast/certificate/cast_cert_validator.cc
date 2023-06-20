// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/certificate/cast_cert_validator.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/media_router/common/providers/cast/certificate/cast_crl.h"
#include "net/cert/pki/cert_issuer_source_static.h"
#include "net/cert/pki/certificate_policies.h"
#include "net/cert/pki/common_cert_errors.h"
#include "net/cert/pki/parse_certificate.h"
#include "net/cert/pki/parse_name.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/pki/path_builder.h"
#include "net/cert/pki/simple_path_builder_delegate.h"
#include "net/cert/pki/trust_store_in_memory.h"
#include "net/cert/time_conversions.h"
#include "net/cert/x509_util.h"
#include "net/der/input.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

// Used specifically when CAST_ALLOW_DEVELOPER_CERTIFICATE is true:
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "components/media_router/common/providers/cast/certificate/cast_cert_reader.h"
#include "components/media_router/common/providers/cast/certificate/switches.h"

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

#include "components/media_router/common/providers/cast/certificate/cast_root_ca_cert_der-inc.h"
#include "components/media_router/common/providers/cast/certificate/eureka_root_ca_der-inc.h"

class CastTrustStore {
 public:
  using AccessCallback = base::OnceCallback<void(net::TrustStore*)>;

  CastTrustStore(const CastTrustStore&) = delete;
  CastTrustStore& operator=(const CastTrustStore&) = delete;

  static void AccessInstance(AccessCallback callback) {
    CastTrustStore* instance = GetInstance();
    const base::AutoLock guard(instance->lock_);
    std::move(callback).Run(&instance->store_);
  }

 private:
  friend class base::NoDestructor<CastTrustStore>;

  static CastTrustStore* GetInstance() {
    static base::NoDestructor<CastTrustStore> instance;
    return instance.get();
  }

  CastTrustStore() {
    AddAnchor(kCastRootCaDer);
    AddAnchor(kEurekaRootCaDer);

    // Adding developer certificates must be done off of the IO thread due
    // to blocking file access.
#if defined(CAST_ALLOW_DEVELOPER_CERTIFICATE)
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        // NOTE: the singleton instance is never destroyed, so we can use
        // Unretained here instead of a weak pointer.
        base::BindOnce(&CastTrustStore::AddDeveloperCertificates,
                       base::Unretained(this)));
  }

  // Check for custom root developer certificate and create a trust store
  // from it if present and enabled.
  void AddDeveloperCertificates() {
    base::AutoLock guard(lock_);
    auto* command_line = base::CommandLine::ForCurrentProcess();
    std::string cert_path_arg = command_line->GetSwitchValueASCII(
        switches::kCastDeveloperCertificatePath);
    if (!cert_path_arg.empty()) {
      base::FilePath cert_path(cert_path_arg);
      if (!cert_path.IsAbsolute()) {
        base::FilePath path;
        base::PathService::Get(base::DIR_CURRENT, &path);
        cert_path = path.Append(cert_path);
      }
      VLOG(1) << "Using cast developer certificate path " << cert_path;
      if (!PopulateStoreWithCertsFromPath(&store_, cert_path)) {
        LOG(WARNING) << "No developer certs added to store, only official"
                        "Google root CA certificates will work.";
      }
    }
#endif
  }

  // Adds a trust anchor given a DER-encoded certificate from static
  // storage.
  template <size_t N>
  void AddAnchor(const uint8_t (&data)[N]) {
    net::CertErrors errors;
    std::shared_ptr<const net::ParsedCertificate> cert =
        net::ParsedCertificate::Create(
            net::x509_util::CreateCryptoBufferFromStaticDataUnsafe(data), {},
            &errors);
    CHECK(cert) << errors.ToDebugString();
    // Enforce pathlen constraints and policies defined on the root certificate.
    base::AutoLock guard(lock_);
    store_.AddTrustAnchorWithConstraints(std::move(cert));
  }

  base::Lock lock_;
  net::TrustStoreInMemory store_ GUARDED_BY(lock_);
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
  // Save a copy of the passed in public key and common name (text).
  CertVerificationContextImpl(bssl::UniquePtr<EVP_PKEY> key,
                              base::StringPiece common_name)
      : key_(std::move(key)), common_name_(common_name) {}

  bool VerifySignatureOverData(
      const base::StringPiece& signature,
      const base::StringPiece& data,
      CastDigestAlgorithm digest_algorithm) const override {
    const EVP_MD* digest = nullptr;
    switch (digest_algorithm) {
      case CastDigestAlgorithm::SHA1:
        digest = EVP_sha1();
        break;
      case CastDigestAlgorithm::SHA256:
        digest = EVP_sha256();
        break;
    };

    // Verify with RSASSA-PKCS1-v1_5 and |digest|.
    auto signature_bytes = base::as_bytes(base::make_span(signature));
    auto data_bytes = base::as_bytes(base::make_span(data));
    bssl::ScopedEVP_MD_CTX ctx;
    return EVP_PKEY_id(key_.get()) == EVP_PKEY_RSA &&
           EVP_DigestVerifyInit(ctx.get(), nullptr, digest, nullptr,
                                key_.get()) &&
           EVP_DigestVerify(ctx.get(), signature_bytes.data(),
                            signature_bytes.size(), data_bytes.data(),
                            data_bytes.size());
  }

  std::string GetCommonName() const override { return common_name_; }

 private:
  bssl::UniquePtr<EVP_PKEY> key_;
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
      if (atv.type == net::der::Input(net::kTypeCommonNameOid)) {
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
[[nodiscard]] bool CheckTargetCertificate(
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

  // Get the public key for the certificate.
  CBS spki;
  CBS_init(&spki, cert->tbs().spki_tlv.UnsafeData(),
           cert->tbs().spki_tlv.Length());
  bssl::UniquePtr<EVP_PKEY> key(EVP_parse_public_key(&spki));
  if (!key || CBS_len(&spki) != 0)
    return false;

  *context = std::make_unique<CertVerificationContextImpl>(std::move(key),
                                                           common_name);
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
  CastCertError verification_result;
  CastTrustStore::AccessInstance(base::BindOnce(
      [](const std::vector<std::string>& certs, const base::Time& time,
         std::unique_ptr<CertVerificationContext>* context,
         CastDeviceCertPolicy* policy, const CastCRL* crl, CRLPolicy crl_policy,
         CastCertError* result, net::TrustStore* store) {
        *result = VerifyDeviceCertUsingCustomTrustStore(
            certs, time, context, policy, crl, crl_policy, store);
      },
      certs, time, context, policy, crl, crl_policy, &verification_result));
  return verification_result;
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
  std::shared_ptr<const net::ParsedCertificate> target_cert;
  net::CertIssuerSourceStatic intermediate_cert_issuer_source;
  for (size_t i = 0; i < certs.size(); ++i) {
    std::shared_ptr<const net::ParsedCertificate> cert(
        net::ParsedCertificate::Create(
            net::x509_util::CreateCryptoBuffer(certs[i]),
            GetCertParsingOptions(), &errors));
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
  if (!net::EncodeTimeAsGeneralizedTime(time, &verification_time)) {
    return CastCertError::ERR_UNEXPECTED;
  }
  net::CertPathBuilder path_builder(
      target_cert, trust_store, &path_builder_delegate, verification_time,
      net::KeyPurpose::CLIENT_AUTH, net::InitialExplicitPolicy::kFalse,
      {net::der::Input(net::kAnyPolicyOid)},
      net::InitialPolicyMappingInhibit::kFalse,
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
