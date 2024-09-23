// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/ios/test_helpers.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
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
                             uint32_t form_id,
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
  form_data->form_renderer_id = FormRendererId(form_id);
  form_data->username_element_renderer_id = FieldRendererId(username_unique_id);
  form_data->preferred_login.username_value = base::UTF8ToUTF16(username_value);
  form_data->password_element_renderer_id = FieldRendererId(password_unique_id);
  form_data->preferred_login.password_value = base::UTF8ToUTF16(password_value);
  if (additional_username != nullptr) {
    autofill::PasswordAndMetadata additional_password_data;
    additional_password_data.username_value =
        base::UTF8ToUTF16(additional_username);
    additional_password_data.password_value =
        base::UTF8ToUTF16(additional_password);
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
  form_data->set_url(GURL(origin));
  form_data->set_renderer_id(FormRendererId(form_id));

  FormFieldData field;
  field.set_value(base::UTF8ToUTF16(username_value));
  field.set_form_control_type(autofill::FormControlType::kInputText);
  field.set_renderer_id(FieldRendererId(username_field_id));
  test_api(*form_data).Append(field);

  field.set_value(base::UTF8ToUTF16(password_value));
  field.set_form_control_type(autofill::FormControlType::kInputPassword);
  field.set_renderer_id(FieldRendererId(password_field_id));
  test_api(*form_data).Append(field);
}

autofill::FormData MakeSimpleFormData() {
  autofill::FormData form_data;
  form_data.set_url(GURL("http://www.google.com/a/LoginAuth"));
  form_data.set_action(GURL("http://www.google.com/a/Login"));
  form_data.set_name(u"login_form");

  autofill::FormFieldData field;
  field.set_name(u"Username");
  field.set_id_attribute(field.name());
  field.set_name_attribute(field.name());
  field.set_value(u"googleuser");
  field.set_form_control_type(autofill::FormControlType::kInputText);
  test_api(form_data).Append(field);

  field.set_name(u"Passwd");
  field.set_id_attribute(field.name());
  field.set_name_attribute(field.name());
  field.set_value(u"p4ssword");
  field.set_form_control_type(autofill::FormControlType::kInputPassword);
  test_api(form_data).Append(field);

  return form_data;
}

}  // namespace  test_helpers
