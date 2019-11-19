// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/certificate_scope.h"

#include <base/values.h>
#include <tuple>

#include "components/onc/onc_constants.h"

namespace chromeos {
namespace onc {

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
base::Optional<CertificateScope> CertificateScope::ParseFromOncValue(
    const base::Value& scope_dict) {
  const std::string* scope_type_str =
      scope_dict.FindStringKey(::onc::scope::kType);
  const std::string* scope_id_str = scope_dict.FindStringKey(::onc::scope::kId);

  if (!scope_type_str || !scope_id_str)
    return base::nullopt;

  if (*scope_type_str == ::onc::scope::kDefault)
    return Default();
  if (*scope_type_str == ::onc::scope::kExtension)
    return ForExtension(*scope_id_str);

  return base::nullopt;
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

}  // namespace onc
}  // namespace chromeos
