// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CERTIFICATE_MATCHING_CERTIFICATE_PRINCIPAL_PATTERN_H_
#define COMPONENTS_CERTIFICATE_MATCHING_CERTIFICATE_PRINCIPAL_PATTERN_H_

#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/values.h"

namespace net {
struct CertPrincipal;
}  // namespace net

namespace certificate_matching {

// Class to represent fields of a principal (issuer or subject) of a X509
// certificate and compare them.
class COMPONENT_EXPORT(CERTIFICATE_MATCHING) CertificatePrincipalPattern {
 public:
  // Creates a pattern that matches every certificate principal.
  CertificatePrincipalPattern();
  // Creates a pattern that requires an exact equality with the specified
  // |common_name|, |locality|, |organization| and |organization_unit| for a
  // certificate to match. If one of these is empty, no constraint is put on the
  // corresponding principal field.
  CertificatePrincipalPattern(std::string common_name,
                              std::string locality,
                              std::string organization,
                              std::string organization_unit);
  CertificatePrincipalPattern(const CertificatePrincipalPattern& rhs);
  CertificatePrincipalPattern(CertificatePrincipalPattern&& rhs);
  ~CertificatePrincipalPattern();

  CertificatePrincipalPattern& operator=(
      const CertificatePrincipalPattern& rhs);
  CertificatePrincipalPattern& operator=(CertificatePrincipalPattern&& rhs);

  // Returns true if all fields in the pattern are empty. A return value of true
  // means that this pattern will match every |CertPrincipal|.
  bool Empty() const;

  // Returns true if this pattern matches |principal|.
  bool Matches(const net::CertPrincipal& principal) const;

  const std::string& common_name() const { return common_name_; }
  const std::string& locality() const { return locality_; }
  const std::string& organization() const { return organization_; }
  const std::string& organization_unit() const { return organization_unit_; }

  // Parses |value| to create a |CertificatePrincipalPattern|. If |value| is
  // present and a dictionary, the |key_*| parameters will be used to fill
  // corresponding fields of the resulting |CertificatePrincipalPattern|. If a
  // key is not present in the dictionary, the corresponding field will be left
  // empty (putting no constraint on the principal field). If |value| is nullptr
  // or not a dictionary, returns an empty pattern.
  static CertificatePrincipalPattern ParseFromOptionalDict(
      const base::Value::Dict* dict,
      std::string_view key_common_name,
      std::string_view key_locality,
      std::string_view key_organization,
      std::string_view key_organization_unit);

 private:
  std::string common_name_;
  std::string locality_;
  std::string organization_;
  std::string organization_unit_;
};

}  // namespace certificate_matching

#endif  // COMPONENTS_CERTIFICATE_MATCHING_CERTIFICATE_PRINCIPAL_PATTERN_H_
