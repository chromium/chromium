// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_utils.h"

#include <memory>
#include <string>

#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

std::unique_ptr<PasswordForm> CreatePasswordFormFromCredentialInfo(
    const CredentialInfo& info,
    const url::Origin& origin) {
  std::unique_ptr<PasswordForm> form;
  if (info.type == CredentialType::CREDENTIAL_TYPE_EMPTY) {
    return form;
  }

  form = std::make_unique<PasswordForm>();
  form->icon_url = info.icon;
  form->display_name = info.name.value_or(std::u16string());
  form->federation_origin = info.federation;
  form->url = origin.GetURL();
  form->password_value = info.password.value_or(std::u16string());
  form->username_value = info.id.value_or(std::u16string());
  form->scheme = PasswordForm::Scheme::kHtml;
  form->type = PasswordForm::Type::kApi;

  form->signon_realm =
      info.type == CredentialType::CREDENTIAL_TYPE_PASSWORD
          ? form->url.spec()
          : "federation://" + origin.host() + "/" + info.federation.host();
  return form;
}

CredentialInfo PasswordFormToCredentialInfo(const PasswordForm& form) {
  return CredentialInfo(form.federation_origin.IsValid()
                            ? CredentialType::CREDENTIAL_TYPE_FEDERATED
                            : CredentialType::CREDENTIAL_TYPE_PASSWORD,
                        form.username_value, form.display_name, form.icon_url,
                        form.password_value, form.federation_origin);
}

}  // namespace password_manager
