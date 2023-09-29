// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_util.h"

#include <string>

#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager::util {

namespace {

using autofill::FormData;
using autofill::FormFieldData;

FormFieldData CreateFormField(std::string form_control_type,
                              std::string autocomplete_attribute) {
  // TODO(1465839): Use autofill helpers once they are accessible to /common.
  FormFieldData field;
  field.form_control_type =
      autofill::StringToFormControlType(form_control_type);
  field.autocomplete_attribute = std::move(autocomplete_attribute);

  return field;
}

TEST(PasswordsManagerUtilTest,
     IsRendererRecognizedCredentialFormWithPasswordField) {
  FormData form;
  form.fields.push_back(
      CreateFormField("password", /*autocomplete_attribute=*/""));
  EXPECT_TRUE(IsRendererRecognizedCredentialForm(form));
}

TEST(PasswordsManagerUtilTest,
     IsRendererRecognizedCredentialFormWithUsernameAutocompleteAttribute) {
  FormData form;
  form.fields.push_back(CreateFormField("text", /*autocomplete_attribute=*/""));
  form.fields.push_back(
      CreateFormField("text", /*autocomplete_attribute=*/"username"));
  EXPECT_TRUE(IsRendererRecognizedCredentialForm(form));
}

TEST(PasswordsManagerUtilTest,
     IsRendererRecognizedCredentialFormWithFamilyNameAutocompleteAttribute) {
  FormData form;
  form.fields.push_back(CreateFormField("text", /*autocomplete_attribute=*/""));
  form.fields.push_back(
      CreateFormField("text", /*autocomplete_attribute=*/"family-name"));
  EXPECT_FALSE(IsRendererRecognizedCredentialForm(form));
}

}  // namespace

}  // namespace password_manager::util
