// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_util.h"

#include <string>

#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/password_manager/core/common/password_manager_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager::util {

namespace {

using autofill::FormData;
using autofill::FormFieldData;

FormFieldData CreateFormField(autofill::FormControlType form_control_type,
                              std::string autocomplete_attribute) {
  // TODO(crbug.com/40276144): Use autofill helpers once they are accessible to
  // /common.
  FormFieldData field;
  field.set_form_control_type(form_control_type);
  field.set_autocomplete_attribute(std::move(autocomplete_attribute));

  return field;
}

TEST(PasswordsManagerUtilTest,
     IsRendererRecognizedCredentialFormWithPasswordField) {
  FormData form;
  form.set_fields({CreateFormField(autofill::FormControlType::kInputPassword,
                                   /*autocomplete_attribute=*/"")});
  EXPECT_TRUE(IsRendererRecognizedCredentialForm(form));
}

TEST(PasswordsManagerUtilTest,
     IsRendererRecognizedCredentialFormWithUsernameAutocompleteAttribute) {
  FormData form;
  form.set_fields({CreateFormField(autofill::FormControlType::kInputText,
                                   /*autocomplete_attribute=*/""),
                   CreateFormField(autofill::FormControlType::kInputText,
                                   /*autocomplete_attribute=*/"username")});
  EXPECT_TRUE(IsRendererRecognizedCredentialForm(form));
}

TEST(PasswordsManagerUtilTest,
     IsRendererRecognizedCredentialFormWithFamilyNameAutocompleteAttribute) {
  FormData form;
  form.set_fields({CreateFormField(autofill::FormControlType::kInputText,
                                   /*autocomplete_attribute=*/""),
                   CreateFormField(autofill::FormControlType::kInputText,
                                   /*autocomplete_attribute=*/"family-name")});
  EXPECT_FALSE(IsRendererRecognizedCredentialForm(form));
}

// Test that a valid username field is considered as such.
TEST(PasswordsManagerUtilTest, CanValueBeConsideredAsSingleUsername_Valid) {
  EXPECT_TRUE(CanValueBeConsideredAsSingleUsername(/*value=*/u"test_username"));
}

TEST(PasswordsManagerUtilTest,
     CanValueBeConsideredAsSingleUsername_ValueHasOtpSize) {
  EXPECT_FALSE(CanValueBeConsideredAsSingleUsername(/*value=*/u"t"));
}

TEST(PasswordsManagerUtilTest,
     CanValueBeConsideredAsSingleUsername_ValueEmpty) {
  EXPECT_FALSE(CanValueBeConsideredAsSingleUsername(/*value=*/u""));
}

TEST(PasswordsManagerUtilTest,
     CanValueBeConsideredAsSingleUsername_ValueTooLarge) {
  EXPECT_FALSE(
      CanValueBeConsideredAsSingleUsername(/*value=*/std::u16string(101, 't')));
}

// Test that a valid username field is considered as such.
TEST(PasswordsManagerUtilTest, CanFieldBeConsideredAsSingleUsername_Valid) {
  EXPECT_TRUE(CanFieldBeConsideredAsSingleUsername(/*name=*/u"username",
                                                   /*id=*/u"username1",
                                                   /*label=*/u"username"));
}

// Test that a field with a too short id and name attribute isn't considered as
// a valid username.
TEST(PasswordsManagerUtilTest,
     CanFieldBeConsideredAsSingleUsername_IdAndNameTooShort) {
  EXPECT_FALSE(CanFieldBeConsideredAsSingleUsername(/*name=*/u"u",
                                                    /*id=*/u"u",
                                                    /*label=*/u"username"));

  // Verify that the rule only applies if the 2 attributes are too short.
  EXPECT_TRUE(CanFieldBeConsideredAsSingleUsername(/*name=*/u"u",
                                                   /*id=*/u"username1",
                                                   /*label=*/u"username"));
  EXPECT_TRUE(CanFieldBeConsideredAsSingleUsername(/*name=*/u"username",
                                                   /*id=*/u"u",
                                                   /*label=*/u"username"));
}

// Test that a field that looks like a search field isn't considered as a valid
// username.
TEST(PasswordsManagerUtilTest,
     CanFieldBeConsideredAsSingleUsername_IsSearchField) {
  EXPECT_FALSE(CanFieldBeConsideredAsSingleUsername(/*name=*/constants::kSearch,
                                                    /*id=*/u"username1",
                                                    /*label=*/u"username"));
  EXPECT_FALSE(CanFieldBeConsideredAsSingleUsername(/*name=*/u"username",
                                                    /*id=*/constants::kSearch,
                                                    /*label=*/u"username"));
  EXPECT_FALSE(
      CanFieldBeConsideredAsSingleUsername(/*name=*/u"username",
                                           /*id=*/u"username1",
                                           /*label=*/constants::kSearch));
}

// Tests that a field that looks like an OTP is considered as such.
TEST(PasswordsManagerUtilTest, IsLikelyOtp_True) {
  EXPECT_TRUE(IsLikelyOtp(/*name=*/u"onetime",
                          /*id=*/u"username1",
                          /*autocomplete=*/"username"));
  EXPECT_TRUE(IsLikelyOtp(
      /*name=*/u"username",
      /*id=*/u"onetime",
      /*autocomplete=*/"username"));
  EXPECT_TRUE(IsLikelyOtp(
      /*name=*/u"username",
      /*id=*/u"username1",
      /*autocomplete=*/constants::kAutocompleteOneTimePassword));
}

// Tests that a field that doesn't look like an OTP isn't considered as
// such.
TEST(PasswordsManagerUtilTest, IsLikelyOtp_False) {
  EXPECT_FALSE(IsLikelyOtp(/*name=*/u"username",
                           /*id=*/u"username1",
                           /*autocomplete=*/"username"));
}

}  // namespace

}  // namespace password_manager::util
