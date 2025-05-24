// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_CAST_CRL_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_CAST_CRL_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"

namespace bssl {
class TrustStore;
}

namespace cast_certificate {

// This class represents the CRL information parsed from the binary proto.
class CastCRL {
 public:
  virtual ~CastCRL() = default;

  // Verifies the revocation status of a cast device certificate given a chain
  // of X.509 certificates.
  //
  // Inputs:
  // * |trusted_chain| the chain of verified certificates, including trust
  //   anchor.
  //
  // * |time| is the unix timestamp to use for determining if the certificate
  //   is revoked.
  //
  // Output:
  // Returns true if no certificate in the chain was revoked.
  virtual bool CheckRevocation(const bssl::ParsedCertificateList& trusted_chain,
                               const base::Time& time) const = 0;
};

// Parses and verifies the CRL used to verify the revocation status of
// Cast device certificates, using the built-in Cast CRL trust anchors.
//
// Inputs:
// * |crl_proto| is a serialized cast_certificate.CrlBundle proto.
// * |time| is the unix timestamp to use for determining if the CRL is valid.
//
// Output:
// Returns the CRL object if success, nullptr otherwise.
std::unique_ptr<CastCRL> ParseAndVerifyCRL(const std::string& crl_proto,
                                           const base::Time& time,
                                           const bool is_fallback_crl);

// This is an overloaded version of ParseAndVerifyCRL that allows
// the input of a custom TrustStore.
//
// For production use pass |trust_store| as nullptr to use the production trust
// store.
std::unique_ptr<CastCRL> ParseAndVerifyCRLUsingCustomTrustStore(
    const std::string& crl_proto,
    const base::Time& time,
    bssl::TrustStore* trust_store,
    const bool is_fallback_crl);

std::unique_ptr<CastCRL> ParseAndVerifyFallbackCRLUsingCustomTrustStore(
    const base::Time& time,
    bssl::TrustStore* trust_store);

}  // namespace cast_certificate

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_CAST_CRL_H_
