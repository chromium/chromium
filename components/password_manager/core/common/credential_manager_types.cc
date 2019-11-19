// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/credential_manager_types.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/common/password_form.h"

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

CredentialInfo::CredentialInfo() : type(CredentialType::CREDENTIAL_TYPE_EMPTY) {
}

CredentialInfo::CredentialInfo(const autofill::PasswordForm& form,
                               CredentialType form_type)
    : type(form_type),
      id(form.username_value),
      name(form.display_name),
      icon(form.icon_url),
      password(form.password_value),
      federation(form.federation_origin) {
  switch (form_type) {
    case CredentialType::CREDENTIAL_TYPE_EMPTY:
      password = base::string16();
      federation = url::Origin();
      break;
    case CredentialType::CREDENTIAL_TYPE_PASSWORD:
      federation = url::Origin();
      break;
    case CredentialType::CREDENTIAL_TYPE_FEDERATED:
      password = base::string16();
      break;
  }
}

CredentialInfo::CredentialInfo(const CredentialInfo& other) = default;

CredentialInfo::~CredentialInfo() {
}

bool CredentialInfo::operator==(const CredentialInfo& rhs) const {
  return (type == rhs.type && id == rhs.id && name == rhs.name &&
          icon == rhs.icon && password == rhs.password &&
          federation.Serialize() == rhs.federation.Serialize());
}

std::unique_ptr<autofill::PasswordForm> CreatePasswordFormFromCredentialInfo(
    const CredentialInfo& info,
    const GURL& origin) {
  std::unique_ptr<autofill::PasswordForm> form;
  if (info.type == CredentialType::CREDENTIAL_TYPE_EMPTY)
    return form;

  form.reset(new autofill::PasswordForm);
  form->icon_url = info.icon;
  form->display_name = info.name.value_or(base::string16());
  form->federation_origin = info.federation;
  form->origin = origin;
  form->password_value = info.password.value_or(base::string16());
  form->username_value = info.id.value_or(base::string16());
  form->scheme = autofill::PasswordForm::Scheme::kHtml;
  form->type = autofill::PasswordForm::Type::kApi;

  form->signon_realm =
      info.type == CredentialType::CREDENTIAL_TYPE_PASSWORD
          ? origin.GetOrigin().spec()
          : "federation://" + origin.host() + "/" + info.federation.host();
  return form;
}

}  // namespace password_manager
