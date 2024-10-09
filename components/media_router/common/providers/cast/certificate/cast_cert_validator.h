// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_CAST_CERT_VALIDATOR_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_CAST_CERT_VALIDATOR_H_

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/time/time.h"

namespace bssl {
class TrustStore;
enum class DigestAlgorithm;
}  // namespace bssl
namespace cast_certificate {

class CastCRL;

// Describes the policy for a Device certificate.
enum class CastDeviceCertPolicy {
  // The device certificate is unrestricted.
  NONE,

  // The device certificate is for an audio-only device.
  AUDIO_ONLY,
};

enum class CRLPolicy {
  // Revocation is checked if a CRL is provided. If CRL is not provided,
  // revocation is checked by fallback CRL.
  CRL_OPTIONAL_WITH_FALLBACK,

  // Revocation is only checked if a CRL is provided.
  CRL_OPTIONAL,

  // Revocation is always checked. If CRL is not provided, revocation is checked
  // by fallback CRL.
  CRL_REQUIRED_WITH_FALLBACK,

  // Revocation is always checked. A missing CRL results in failure.
  CRL_REQUIRED,
};

enum class CastCertError {
  OK,
  // Certificates were not provided for verification.
  ERR_CERTS_MISSING,
  // The certificates provided could not be parsed.
  ERR_CERTS_PARSE,
  // Key usage is missing or is not set to Digital Signature.
  // This error could also be thrown if the CN is missing.
  ERR_CERTS_RESTRICTIONS,
  // The current date is before the notBefore date or after the notAfter date.
  ERR_CERTS_DATE_INVALID,
  // The certificate failed to chain to a trusted root.
  ERR_CERTS_VERIFY_GENERIC,
  // The CRL is missing or failed to verify.
  ERR_CRL_INVALID,
  // One of the certificates in the chain is revoked.
  ERR_CERTS_REVOKED,
  // When verification is successful only after using the fallback CRL.
  OK_FALLBACK_CRL,
  // When the flag to use a fallback CRL is enabled but the build is too old and
  // the fallback already expired.
  ERR_FALLBACK_CRL_INVALID,
  // When verification shows the certificate is revoked after using the fallback
  // CRL.
  ERR_CERTS_REVOKED_BY_FALLBACK_CRL,
  // An internal coding error.
  ERR_UNEXPECTED,
};

// The digest algorithms supported with CertVerificationContext.
enum class CastDigestAlgorithm {
  SHA1,
  SHA256,
};

// An object of this type is returned by the VerifyDeviceCert function, and can
// be used for additional certificate-related operations, using the verified
// certificate.
class CertVerificationContext {
 public:
  CertVerificationContext();

  CertVerificationContext(const CertVerificationContext&) = delete;
  CertVerificationContext& operator=(const CertVerificationContext&) = delete;

  virtual ~CertVerificationContext();

  // Use the public key from the verified certificate to verify an
  // RSASSA-PKCS1-v1_5 |signature| over arbitrary |data|, with the specified
  // |digest_algorithm|. Both |signature| and |data| hold raw binary data.
  // Returns true if the signature was correct.
  virtual bool VerifySignatureOverData(
      std::string_view signature,
      std::string_view data,
      CastDigestAlgorithm digest_algorithm) const = 0;

  // Retrieve the Common Name attribute of the subject's distinguished name from
  // the verified certificate, if present.  Returns an empty string if no Common
  // Name is found.
  virtual std::string GetCommonName() const = 0;
};

// These provide access for tests to change the CastTrustStoreSingleton.
void CastTrustStoreAddDefaultCertificatesForTesting();
void CastTrustStoreAddBuiltInCertificatesForTesting();
void CastTrustStoreAddCertificateFromPathForTesting(base::FilePath cert_path);
void CastTrustStoreClearForTesting();

// Verifies a cast device certificate given a chain of DER-encoded certificates,
// using the built-in Cast trust anchors.
//
// Inputs:
//
// * |certs| is a chain of DER-encoded certificates:
//   * |certs[0]| is the target certificate (i.e. the device certificate).
//   * |certs[1..n-1]| are intermediates certificates to use in path building.
//     Their ordering does not matter.
//
// * |time| is the unix timestamp to use for determining if the certificate
//   is expired.
//
// * |crl| is the CRL to check for certificate revocation status.
//   If this is a nullptr, then revocation checking is currently disabled.
//
// * |crl_policy| is for choosing how to handle the absence of a CRL.
//   If CRL_REQUIRED is passed, then an empty |crl| input would result
//   in a failed verification. Otherwise, |crl| is ignored if it is absent.
//
// Outputs:
//
// Returns CastCertError::OK on success. Otherwise, the corresponding
// CastCertError. On success, the output parameters are filled with more
// details:
//
//   * |context| is filled with an object that can be used to verify signatures
//     using the device certificate's public key, as well as to extract other
//     properties from the device certificate (Common Name).
//   * |policy| is filled with an indication of the device certificate's policy
//     (i.e. is it for audio-only devices or is it unrestricted?)
[[nodiscard]] CastCertError VerifyDeviceCert(
    const std::vector<std::string>& certs,
    const base::Time& time,
    std::unique_ptr<CertVerificationContext>* context,
    CastDeviceCertPolicy* policy,
    const CastCRL* crl,
    const CastCRL* fallback_crl,
    CRLPolicy crl_policy);

// This is an overloaded version of VerifyDeviceCert that allows
// the input of a custom TrustStore.
//
// For production use pass |trust_store| as nullptr to use the production trust
// store.
[[nodiscard]] CastCertError VerifyDeviceCertUsingCustomTrustStore(
    const std::vector<std::string>& certs,
    const base::Time& time,
    std::unique_ptr<CertVerificationContext>* context,
    CastDeviceCertPolicy* policy,
    const CastCRL* crl,
    const CastCRL* fallback_crl,
    CRLPolicy crl_policy,
    bssl::TrustStore* trust_store);

// Returns a string status messages for the CastCertError provided.
std::string CastCertErrorToString(CastCertError error);

}  // namespace cast_certificate

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_CAST_CERT_VALIDATOR_H_
