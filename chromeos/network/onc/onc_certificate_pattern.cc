// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_certificate_pattern.h"

#include <stddef.h>

#include <utility>

#include "base/logging.h"
#include "base/values.h"
#include "components/onc/onc_constants.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"

namespace chromeos {

namespace {

bool GetAsListOfStrings(const base::Value& value,
                        std::vector<std::string>* result) {
  if (!value.is_list())
    return false;

  result->clear();
  result->reserve(value.GetList().size());
  for (const auto& entry : value.GetList()) {
    if (!entry.is_string())
      return false;
    result->push_back(entry.GetString());
  }

  return true;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// OncCertificatePattern

OncCertificatePattern::OncCertificatePattern() = default;

OncCertificatePattern::OncCertificatePattern(
    const OncCertificatePattern& other) = default;

OncCertificatePattern::OncCertificatePattern(OncCertificatePattern&& other) =
    default;

OncCertificatePattern::~OncCertificatePattern() = default;

OncCertificatePattern& OncCertificatePattern::operator=(
    const OncCertificatePattern& rhs) = default;
OncCertificatePattern& OncCertificatePattern::operator=(
    OncCertificatePattern&& rhs) = default;

bool OncCertificatePattern::Empty() const {
  return pem_encoded_issuer_cas_.empty() && issuer_pattern_.Empty() &&
         subject_pattern_.Empty();
}

bool OncCertificatePattern::Matches(
    const net::X509Certificate& certificate,
    const std::string& pem_encoded_issuer_ca) const {
  if (!issuer_pattern_.Empty() || !subject_pattern_.Empty()) {
    if (!issuer_pattern_.Empty() &&
        !issuer_pattern_.Matches(certificate.issuer()))
      return false;
    if (!subject_pattern_.Empty() &&
        !subject_pattern_.Matches(certificate.subject()))
      return false;
  }

  if (!pem_encoded_issuer_cas_.empty() &&
      !base::Contains(pem_encoded_issuer_cas_, pem_encoded_issuer_ca)) {
    return false;
  }
  return true;
}

// static
base::Optional<OncCertificatePattern>
OncCertificatePattern::ReadFromONCDictionary(const base::Value& dict) {
  // All of these are optional.
  const base::Value* pem_encoded_issuer_cas_value = dict.FindKeyOfType(
      onc::client_cert::kIssuerCAPEMs, base::Value::Type::LIST);
  std::vector<std::string> pem_encoded_issuer_cas;
  if (pem_encoded_issuer_cas_value &&
      !GetAsListOfStrings(*pem_encoded_issuer_cas_value,
                          &pem_encoded_issuer_cas)) {
    return base::nullopt;
  }

  const base::Value* enrollment_uri_list_value = dict.FindKeyOfType(
      onc::client_cert::kEnrollmentURI, base::Value::Type::LIST);
  std::vector<std::string> enrollment_uri_list;
  if (enrollment_uri_list_value &&
      !GetAsListOfStrings(*enrollment_uri_list_value, &enrollment_uri_list)) {
    return base::nullopt;
  }

  auto issuer_pattern =
      certificate_matching::CertificatePrincipalPattern::ParseFromOptionalDict(
          dict.FindKeyOfType(onc::client_cert::kIssuer,
                             base::Value::Type::DICTIONARY),
          onc::client_cert::kCommonName, onc::client_cert::kLocality,
          onc::client_cert::kOrganization,
          onc::client_cert::kOrganizationalUnit);
  auto subject_pattern =
      certificate_matching::CertificatePrincipalPattern::ParseFromOptionalDict(
          dict.FindKeyOfType(onc::client_cert::kSubject,
                             base::Value::Type::DICTIONARY),
          onc::client_cert::kCommonName, onc::client_cert::kLocality,
          onc::client_cert::kOrganization,
          onc::client_cert::kOrganizationalUnit);

  return OncCertificatePattern(pem_encoded_issuer_cas, issuer_pattern,
                               subject_pattern, enrollment_uri_list);
}

OncCertificatePattern::OncCertificatePattern(
    std::vector<std::string> pem_encoded_issuer_cas,
    certificate_matching::CertificatePrincipalPattern issuer_pattern,
    certificate_matching::CertificatePrincipalPattern subject_pattern,
    std::vector<std::string> enrollment_uri_list)
    : pem_encoded_issuer_cas_(std::move(pem_encoded_issuer_cas)),
      issuer_pattern_(std::move(issuer_pattern)),
      subject_pattern_(std::move(subject_pattern)),
      enrollment_uri_list_(std::move(enrollment_uri_list)) {}

}  // namespace chromeos
