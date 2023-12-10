// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/onc/certificate_scope.h"

#include <tuple>

#include "base/values.h"
#include "components/onc/onc_constants.h"

namespace chromeos::onc {

CertificateScope::CertificateScope(const CertificateScope& other) = default;
CertificateScope::CertificateScope(CertificateScope&& other) = default;
CertificateScope::~CertificateScope() = default;

CertificateScope::CertificateScope(const std::string& extension_id)
    : extension_id_(extension_id) {}

// static
CertificateScope CertificateScope::ForExtension(
    const std::string& extension_id) {
  return CertificateScope(/*extension_id=*/extension_id);
}

// static
CertificateScope CertificateScope::Default() {
  return CertificateScope(/*extension_id=*/std::string());
}

// static
std::optional<CertificateScope> CertificateScope::ParseFromOncValue(
    const base::Value::Dict& scope_dict) {
  const std::string* scope_type_str =
      scope_dict.FindString(::onc::scope::kType);
  const std::string* scope_id_str = scope_dict.FindString(::onc::scope::kId);

  if (!scope_type_str || !scope_id_str)
    return std::nullopt;

  if (*scope_type_str == ::onc::scope::kDefault)
    return Default();
  if (*scope_type_str == ::onc::scope::kExtension)
    return ForExtension(*scope_id_str);

  return std::nullopt;
}

CertificateScope& CertificateScope::operator=(const CertificateScope& other) =
    default;

bool CertificateScope::operator<(const CertificateScope& other) const {
  return extension_id_ < other.extension_id_;
}
bool CertificateScope::operator==(const CertificateScope& other) const {
  return extension_id_ == other.extension_id_;
}

bool CertificateScope::operator!=(const CertificateScope& other) const {
  return !(*this == other);
}

}  // namespace chromeos::onc
