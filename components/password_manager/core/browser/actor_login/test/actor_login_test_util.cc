// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/test/actor_login_test_util.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"

namespace actor_login {

using autofill::FormData;
using autofill::FormFieldData;
using autofill::test::CreateTestFormField;
using autofill::test::MakeFormRendererId;

Credential CreateTestCredential(const std::u16string& username,
                                const GURL& url) {
  Credential credential;
  credential.username = username;
  credential.source_site_or_app = base::UTF8ToUTF16(url.spec());
  credential.type = CredentialType::kPassword;
  return credential;
}

FormData CreateSigninFormData(const GURL& url) {
  autofill::FormData form_data;
  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Username:", /*name=*/"username",
      /*value=*/"", autofill::FormControlType::kInputEmail));
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

}  // namespace actor_login
