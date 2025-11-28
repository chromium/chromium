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

optimization_guide::proto::ActorLoginQuality_FormData CreateExpectedFormData(
    const PasswordForm& form) {
  optimization_guide::proto::ActorLoginQuality_FormData form_data_proto;

  form_data_proto.set_form_signature(
      autofill::CalculateFormSignature(form.form_data).value());

  for (const auto& field : form.form_data.fields()) {
    optimization_guide::proto::ActorLoginQuality_FormData_FieldData field_data;
    field_data.set_signature(
        autofill::CalculateFieldSignatureForField(field).value());

    if (field.renderer_id() == form.username_element_renderer_id) {
      field_data.set_field_type(
          optimization_guide::proto::ActorLoginQuality_FormData_FieldData::
              USERNAME);
    } else if (field.renderer_id() == form.password_element_renderer_id) {
      field_data.set_field_type(
          optimization_guide::proto::ActorLoginQuality_FormData_FieldData::
              PASSWORD);
    } else if (field.renderer_id() == form.new_password_element_renderer_id) {
      field_data.set_field_type(
          optimization_guide::proto::ActorLoginQuality_FormData_FieldData::
              NEW_PASSWORD);
    } else if (field.renderer_id() ==
               form.confirmation_password_element_renderer_id) {
      field_data.set_field_type(
          optimization_guide::proto::ActorLoginQuality_FormData_FieldData::
              CONFIRMATION_PASSWORD);
    } else {
      field_data.set_field_type(
          optimization_guide::proto::ActorLoginQuality_FormData_FieldData::
              UNKNOWN);
    }
    *form_data_proto.add_field_data() = field_data;
  }

  return form_data_proto;
}

optimization_guide::proto::ActorLoginQuality_ParsedFormDetails
CreateExpectedLoginFormDetails(const PasswordForm& form,
                               bool is_username_visible,
                               bool is_password_visible,
                               std::optional<int> async_check_time_ms) {
  optimization_guide::proto::ActorLoginQuality_ParsedFormDetails details;
  *details.mutable_form_data() = CreateExpectedFormData(form);

  details.set_is_username_field_visible(is_username_visible);
  details.set_is_password_field_visible(is_password_visible);
  details.set_is_new_password_visible(false);
  details.set_is_valid_frame_and_origin(true);
  details.set_async_check_time_ms(async_check_time_ms.value_or(0));

  return details;
}

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
