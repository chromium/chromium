// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/onc/onc_certificate_pattern.h"

#include <stddef.h>

#include <utility>

#include "base/containers/contains.h"
#include "base/values.h"
#include "components/onc/onc_constants.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"

namespace ash {

namespace {

bool GetAsListOfStrings(const base::Value::List& value,
                        std::vector<std::string>* result) {
  result->clear();
  result->reserve(value.size());
  for (const auto& entry : value) {
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
  if (!issuer_pattern_.Empty() &&
      !issuer_pattern_.Matches(certificate.issuer())) {
    return false;
  }
  if (!subject_pattern_.Empty() &&
      !subject_pattern_.Matches(certificate.subject())) {
    return false;
  }
  if (!pem_encoded_issuer_cas_.empty() &&
      !base::Contains(pem_encoded_issuer_cas_, pem_encoded_issuer_ca)) {
    return false;
  }
  return true;
}

// static
std::optional<OncCertificatePattern>
OncCertificatePattern::ReadFromONCDictionary(const base::Value::Dict& dict) {
  // All of these are optional.
  const base::Value::List* pem_encoded_issuer_cas_value =
      dict.FindList(onc::client_cert::kIssuerCAPEMs);
  std::vector<std::string> pem_encoded_issuer_cas;
  if (pem_encoded_issuer_cas_value &&
      !GetAsListOfStrings(*pem_encoded_issuer_cas_value,
                          &pem_encoded_issuer_cas)) {
    return std::nullopt;
  }

  const base::Value::List* enrollment_uri_list_value =
      dict.FindList(onc::client_cert::kEnrollmentURI);
  std::vector<std::string> enrollment_uri_list;
  if (enrollment_uri_list_value &&
      !GetAsListOfStrings(*enrollment_uri_list_value, &enrollment_uri_list)) {
    return std::nullopt;
  }

  auto issuer_pattern =
      certificate_matching::CertificatePrincipalPattern::ParseFromOptionalDict(
          dict.FindDict(onc::client_cert::kIssuer),
          onc::client_cert::kCommonName, onc::client_cert::kLocality,
          onc::client_cert::kOrganization,
          onc::client_cert::kOrganizationalUnit);
  auto subject_pattern =
      certificate_matching::CertificatePrincipalPattern::ParseFromOptionalDict(
          dict.FindDict(onc::client_cert::kSubject),
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

}  // namespace ash
