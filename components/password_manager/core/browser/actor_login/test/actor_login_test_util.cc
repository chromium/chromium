// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/test/actor_login_test_util.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "url/origin.h"

namespace actor_login {

using autofill::FormData;
using autofill::FormFieldData;
using autofill::test::CreateTestFormField;
using autofill::test::MakeFormRendererId;
using password_manager::PasswordForm;

Credential CreateTestCredential(const std::u16string& username,
                                const GURL& url,
                                const url::Origin& request_origin) {
  Credential credential;
  credential.username = username;
  credential.source_site_or_app = base::UTF8ToUTF16(url.spec());
  credential.request_origin = request_origin;
  credential.type = CredentialType::kPassword;
  return credential;
}

PasswordForm CreateSavedPasswordForm(const GURL& url,
                                     const std::u16string& username,
                                     const std::u16string& password) {
  PasswordForm form;
  form.url = url;
  form.signon_realm = password_manager_util::GetSignonRealm(url);
  form.username_value = username;
  form.password_value = password;
  form.match_type = PasswordForm::MatchType::kExact;
  form.in_store = password_manager::PasswordForm::Store::kAccountStore;
  return form;
}

FormData CreateSigninFormData(const GURL& url) {
  autofill::FormData form_data;
  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Username:", /*name=*/"username",
      /*value=*/"", autofill::FormControlType::kInputEmail,
      /*autocomplete=*/"username"));
  fields.push_back(CreateTestFormField(
      /*label=*/"Password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  form_data.set_renderer_id(autofill::test::MakeFormRendererId());
  form_data.set_url(url);
  form_data.set_fields(std::move(fields));
  return form_data;
}

FormData CreateChangePasswordFormData(const GURL& url) {
  autofill::FormData form_data;
  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"New password:", /*name=*/"new_password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.push_back(CreateTestFormField(
      /*label=*/"Confirm new password:", /*name=*/"confirm_password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  form_data.set_renderer_id(autofill::test::MakeFormRendererId());
  form_data.set_url(url);
  form_data.set_fields(std::move(fields));
  return form_data;
}

// Implementation for a username-only form.
FormData CreateUsernameOnlyFormData(const GURL& url) {
  autofill::FormData form_data;
  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Username:", /*name=*/"username",
      /*value=*/"", autofill::FormControlType::kInputEmail,
      /*autocomplete=*/"username"));
  form_data.set_renderer_id(autofill::test::MakeFormRendererId());
  form_data.set_url(url);
  form_data.set_fields(std::move(fields));
  return form_data;
}

// Implementation for a password-only form.
FormData CreatePasswordOnlyFormData(const GURL& url) {
  autofill::FormData form_data;
  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  form_data.set_renderer_id(autofill::test::MakeFormRendererId());
  form_data.set_url(url);
  form_data.set_fields(std::move(fields));
  return form_data;
}

}  // namespace actor_login
