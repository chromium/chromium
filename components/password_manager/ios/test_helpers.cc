// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/ios/test_helpers.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/ios/account_select_fill_data.h"
#include "url/gurl.h"

using autofill::PasswordFormFillData;
using password_manager::FillData;

namespace test_helpers {

void SetPasswordFormFillData(const std::string& origin,
                             const std::string& action,
                             const char* username_field,
                             const char* username_value,
                             const char* password_field,
                             const char* password_value,
                             const char* additional_username,
                             const char* additional_password,
                             bool wait_for_username,
                             PasswordFormFillData* form_data) {
  form_data->origin = GURL(origin);
  form_data->action = GURL(action);
  autofill::FormFieldData username;
  username.name = base::UTF8ToUTF16(username_field);
  username.unique_id = base::UTF8ToUTF16(username_field);
  username.value = base::UTF8ToUTF16(username_value);
  form_data->username_field = username;
  autofill::FormFieldData password;
  password.name = base::UTF8ToUTF16(password_field);
  password.unique_id = base::UTF8ToUTF16(password_field);
  password.value = base::UTF8ToUTF16(password_value);
  form_data->password_field = password;
  if (additional_username) {
    autofill::PasswordAndMetadata additional_password_data;
    additional_password_data.password = base::UTF8ToUTF16(additional_password);
    additional_password_data.realm.clear();
    form_data->additional_logins.insert(
        std::pair<base::string16, autofill::PasswordAndMetadata>(
            base::UTF8ToUTF16(additional_username), additional_password_data));
  }
  form_data->wait_for_username = wait_for_username;
}

void SetFillData(const std::string& origin,
                 const std::string& action,
                 const char* username_field,
                 const char* username_value,
                 const char* password_field,
                 const char* password_value,
                 FillData* fill_data) {
  DCHECK(fill_data);
  fill_data->origin = GURL(origin);
  fill_data->action = GURL(action);
  fill_data->username_element = base::UTF8ToUTF16(username_field);
  fill_data->username_value = base::UTF8ToUTF16(username_value);
  fill_data->password_element = base::UTF8ToUTF16(password_field);
  fill_data->password_value = base::UTF8ToUTF16(password_value);
}

}  // namespace  test_helpers
