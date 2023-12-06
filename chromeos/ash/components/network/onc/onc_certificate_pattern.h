// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_ONC_CERTIFICATE_PATTERN_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_ONC_CERTIFICATE_PATTERN_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/values.h"
#include "components/certificate_matching/certificate_principal_pattern.h"

namespace net {
class X509Certificate;
}

namespace ash {

// A class to contain a certificate pattern and find existing matches to the
// pattern in the certificate database.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) OncCertificatePattern {
 public:
  OncCertificatePattern();
  OncCertificatePattern(const OncCertificatePattern& other);
  OncCertificatePattern(OncCertificatePattern&& other);
  ~OncCertificatePattern();

  OncCertificatePattern& operator=(const OncCertificatePattern& rhs);
  OncCertificatePattern& operator=(OncCertificatePattern&& rhs);

  // Returns true if this pattern has nothing set (and so would match all
  // certs). Ignores enrollment_uri_;
  bool Empty() const;

  bool Matches(const net::X509Certificate& certificate,
               const std::string& pem_encoded_issuer_ca) const;

  const std::vector<std::string>& pem_encoded_issuer_cas() const {
    return pem_encoded_issuer_cas_;
  }
  const certificate_matching::CertificatePrincipalPattern& issuer_pattern()
      const {
    return issuer_pattern_;
  }
  const certificate_matching::CertificatePrincipalPattern& subject_pattern()
      const {
    return subject_pattern_;
  }
  const std::vector<std::string>& enrollment_uri_list() const {
    return enrollment_uri_list_;
  }

  // Reads a |OncCertificatePattern| from an ONC dictionary.
  static std::optional<OncCertificatePattern> ReadFromONCDictionary(
      const base::Value::Dict& dictionary);

 private:
  OncCertificatePattern(
      std::vector<std::string> pem_encoded_issuer_cas,
      certificate_matching::CertificatePrincipalPattern issuer_pattern,
      certificate_matching::CertificatePrincipalPattern subject_pattern,
      std::vector<std::string> enrollment_uri_list_);

  std::vector<std::string> pem_encoded_issuer_cas_;
  certificate_matching::CertificatePrincipalPattern issuer_pattern_;
  certificate_matching::CertificatePrincipalPattern subject_pattern_;
  std::vector<std::string> enrollment_uri_list_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_ONC_CERTIFICATE_PATTERN_H_
