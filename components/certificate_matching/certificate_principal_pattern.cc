// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_matching/certificate_principal_pattern.h"

#include <string>
#include <string_view>

#include "base/containers/contains.h"
#include "base/values.h"
#include "net/cert/x509_cert_types.h"
#include "net/cert/x509_certificate.h"

namespace certificate_matching {
namespace {

std::string GetOptionalStringKey(const base::Value::Dict& dictionary,
                                 std::string_view key) {
  auto* value = dictionary.FindString(key);
  return value ? *value : std::string();
}

}  // namespace

CertificatePrincipalPattern::CertificatePrincipalPattern() = default;

CertificatePrincipalPattern::CertificatePrincipalPattern(
    std::string common_name,
    std::string locality,
    std::string organization,
    std::string organization_unit)
    : common_name_(std::move(common_name)),
      locality_(std::move(locality)),
      organization_(std::move(organization)),
      organization_unit_(std::move(organization_unit)) {}

CertificatePrincipalPattern::CertificatePrincipalPattern(
    const CertificatePrincipalPattern& rhs) = default;

CertificatePrincipalPattern::CertificatePrincipalPattern(
    CertificatePrincipalPattern&& rhs) = default;

CertificatePrincipalPattern::~CertificatePrincipalPattern() = default;

CertificatePrincipalPattern& CertificatePrincipalPattern::operator=(
    const CertificatePrincipalPattern& rhs) = default;
CertificatePrincipalPattern& CertificatePrincipalPattern::operator=(
    CertificatePrincipalPattern&& rhs) = default;

bool CertificatePrincipalPattern::Empty() const {
  return common_name_.empty() && locality_.empty() && organization_.empty() &&
         organization_unit_.empty();
}

bool CertificatePrincipalPattern::Matches(
    const net::CertPrincipal& principal) const {
  if (!common_name_.empty() && common_name_ != principal.common_name) {
    return false;
  }

  if (!locality_.empty() && locality_ != principal.locality_name) {
    return false;
  }

  if (!organization_.empty()) {
    if (!base::Contains(principal.organization_names, organization_)) {
      return false;
    }
  }

  if (!organization_unit_.empty()) {
    if (!base::Contains(principal.organization_unit_names,
                        organization_unit_)) {
      return false;
    }
  }

  return true;
}

// static
CertificatePrincipalPattern CertificatePrincipalPattern::ParseFromOptionalDict(
    const base::Value::Dict* dict,
    std::string_view key_common_name,
    std::string_view key_locality,
    std::string_view key_organization,
    std::string_view key_organization_unit) {
  if (!dict)
    return CertificatePrincipalPattern();
  return CertificatePrincipalPattern(
      GetOptionalStringKey(*dict, key_common_name),
      GetOptionalStringKey(*dict, key_locality),
      GetOptionalStringKey(*dict, key_organization),
      GetOptionalStringKey(*dict, key_organization_unit));
}

}  // namespace certificate_matching
