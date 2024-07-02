// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/credential_manager_types.h"

#include <memory>

#include "base/strings/string_number_conversions.h"

namespace password_manager {

std::string CredentialTypeToString(CredentialType value) {
  switch (value) {
    case CredentialType::CREDENTIAL_TYPE_EMPTY:
      return "CredentialType::CREDENTIAL_TYPE_EMPTY";
    case CredentialType::CREDENTIAL_TYPE_PASSWORD:
      return "CredentialType::CREDENTIAL_TYPE_PASSWORD";
    case CredentialType::CREDENTIAL_TYPE_FEDERATED:
      return "CredentialType::CREDENTIAL_TYPE_FEDERATED";
  }
  return "Unknown CredentialType value: " +
         base::NumberToString(static_cast<int>(value));
}

std::ostream& operator<<(std::ostream& os, CredentialType value) {
  return os << CredentialTypeToString(value);
}

CredentialInfo::CredentialInfo() = default;

CredentialInfo::CredentialInfo(CredentialType type,
                               std::optional<std::u16string> id,
                               std::optional<std::u16string> name,
                               GURL icon,
                               std::optional<std::u16string> password,
                               url::SchemeHostPort federation)
    : type(type),
      id(std::move(id)),
      name(std::move(name)),
      icon(std::move(icon)),
      password(std::move(password)),
      federation(std::move(federation)) {
  switch (type) {
    case CredentialType::CREDENTIAL_TYPE_EMPTY:
      password = std::u16string();
      federation = url::SchemeHostPort();
      break;
    case CredentialType::CREDENTIAL_TYPE_PASSWORD:
      federation = url::SchemeHostPort();
      break;
    case CredentialType::CREDENTIAL_TYPE_FEDERATED:
      password = std::u16string();
      break;
  }
}

CredentialInfo::CredentialInfo(const CredentialInfo& other) = default;

CredentialInfo::~CredentialInfo() = default;

bool CredentialInfo::operator==(const CredentialInfo& rhs) const {
  return (type == rhs.type && id == rhs.id && name == rhs.name &&
          icon == rhs.icon && password == rhs.password &&
          federation.Serialize() == rhs.federation.Serialize());
}

}  // namespace password_manager
