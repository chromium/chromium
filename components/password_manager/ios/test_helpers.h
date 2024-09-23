// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_TEST_HELPERS_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_TEST_HELPERS_H_

#include <string>

namespace autofill {
class FormData;
struct PasswordFormFillData;
}  // namespace autofill

namespace password_manager {
struct FillData;
}  // namespace password_manager

namespace test_helpers {

// Populates |form_data| with test values.
void SetPasswordFormFillData(const std::string& origin,
                             const char* form_name,
                             uint32_t form_id,
                             const char* username_field,
                             uint32_t username_field_id,
                             const char* username_value,
                             const char* password_field,
                             uint32_t password_field_id,
                             const char* password_value,
                             const char* additional_username,
                             const char* additional_password,
                             autofill::PasswordFormFillData* form_data);

// Populates |fill_data| with test values.
void SetFillData(const std::string& origin,
                 uint32_t form_id,
                 uint32_t username_field_id,
                 const char* username_value,
                 uint32_t password_field_id,
                 const char* password_value,
                 password_manager::FillData* fill_data);

// Populates |form_data| with test values.
void SetFormData(const std::string& origin,
                 uint32_t form_id,
                 uint32_t username_field_id,
                 const char* username_value,
                 uint32_t password_field_id,
                 const char* password_value,
                 autofill::FormData* form_data);

// Returns a simple FormData with test values.
autofill::FormData MakeSimpleFormData();

}  // namespace test_helpers

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_TEST_HELPERS_H_
