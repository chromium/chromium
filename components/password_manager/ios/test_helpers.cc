// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/ios/test_helpers.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/ios/account_select_fill_data.h"
#include "url/gurl.h"

using autofill::FieldRendererId;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormRendererId;
using autofill::PasswordFormFillData;
using password_manager::FillData;

namespace test_helpers {

void SetPasswordFormFillData(const std::string& url,
                             const char* form_name,
                             uint32_t unique_renderer_id,
                             const char* username_field,
                             uint32_t username_unique_id,
                             const char* username_value,
                             const char* password_field,
                             uint32_t password_unique_id,
                             const char* password_value,
                             const char* additional_username,
                             const char* additional_password,
                             PasswordFormFillData* form_data) {
  form_data->url = GURL(url);
  form_data->name = base::UTF8ToUTF16(form_name);
  form_data->form_renderer_id = FormRendererId(unique_renderer_id);
  autofill::FormFieldData username;
  username.name = base::UTF8ToUTF16(username_field);
  username.unique_renderer_id = FieldRendererId(username_unique_id);
  username.value = base::UTF8ToUTF16(username_value);
  form_data->username_field = username;
  autofill::FormFieldData password;
  password.name = base::UTF8ToUTF16(password_field);
  password.unique_renderer_id = FieldRendererId(password_unique_id);
  password.value = base::UTF8ToUTF16(password_value);
  form_data->password_field = password;
  if (additional_username) {
    autofill::PasswordAndMetadata additional_password_data;
    additional_password_data.username = base::UTF8ToUTF16(additional_username);
    additional_password_data.password = base::UTF8ToUTF16(additional_password);
    additional_password_data.realm.clear();
    form_data->additional_logins.push_back(additional_password_data);
  }
  form_data->wait_for_username = true;
}

void SetFillData(const std::string& origin,
                 uint32_t form_id,
                 uint32_t username_field_id,
                 const char* username_value,
                 uint32_t password_field_id,
                 const char* password_value,
                 FillData* fill_data) {
  DCHECK(fill_data);
  fill_data->origin = GURL(origin);
  fill_data->form_id = FormRendererId(form_id);
  fill_data->username_element_id = FieldRendererId(username_field_id);
  fill_data->username_value = base::UTF8ToUTF16(username_value);
  fill_data->password_element_id = FieldRendererId(password_field_id);
  fill_data->password_value = base::UTF8ToUTF16(password_value);
}

void SetFormData(const std::string& origin,
                 uint32_t form_id,
                 uint32_t username_field_id,
                 const char* username_value,
                 uint32_t password_field_id,
                 const char* password_value,
                 FormData* form_data) {
  DCHECK(form_data);
  form_data->url = GURL(origin);
  form_data->unique_renderer_id = FormRendererId(form_id);

  FormFieldData field;
  field.value = base::UTF8ToUTF16(username_value);
  field.form_control_type = "text";
  field.unique_renderer_id = FieldRendererId(username_field_id);
  form_data->fields.push_back(field);

  field.value = base::UTF8ToUTF16(password_value);
  field.form_control_type = "password";
  field.unique_renderer_id = FieldRendererId(password_field_id);
  form_data->fields.push_back(field);
}

autofill::FormData MakeSimpleFormData() {
  autofill::FormData form_data;
  form_data.url = GURL("http://www.google.com/a/LoginAuth");
  form_data.action = GURL("http://www.google.com/a/Login");
  form_data.name = u"login_form";

  autofill::FormFieldData field;
  field.name = u"Username";
  field.id_attribute = field.name;
  field.name_attribute = field.name;
  field.value = u"googleuser";
  field.form_control_type = "text";
  form_data.fields.push_back(field);

  field.name = u"Passwd";
  field.id_attribute = field.name;
  field.name_attribute = field.name;
  field.value = u"p4ssword";
  field.form_control_type = "password";
  form_data.fields.push_back(field);

  return form_data;
}

}  // namespace  test_helpers
