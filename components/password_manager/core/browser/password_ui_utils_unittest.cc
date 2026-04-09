// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_ui_utils.h"

#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace password_manager {

using autofill::FormControlType;
using autofill::FormData;
using autofill::FormFieldData;

TEST(GetShownOriginTest, RemovePrefixes) {
  const struct {
    const std::string input;
    const std::string output;
  } kTestCases[] = {
      {"http://subdomain.example.com:80/login/index.html?h=90&ind=hello#first",
       "subdomain.example.com"},
      {"https://www.example.com", "example.com"},
      {"https://mobile.example.com", "example.com"},
      {"https://m.example.com", "example.com"},
      {"https://m.subdomain.example.net", "subdomain.example.net"},
      {"https://mobile.de", "mobile.de"},
      {"https://www.de", "www.de"},
      {"https://m.de", "m.de"},
      {"https://www.mobile.de", "mobile.de"},
      {"https://m.mobile.de", "mobile.de"},
      {"https://m.www.de", "www.de"},
      {"https://Mobile.example.de", "example.de"},
      {"https://WWW.Example.DE", "example.de"}};

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.output,
              GetShownOrigin(url::Origin::Create(GURL(test_case.input))))
        << "for input " << test_case.input;
  }
}

TEST(GetShownOriginAndLinkUrlTest, OriginFromAndroidForm_NoAppDisplayName) {
  PasswordForm android_form;
  android_form.signon_realm = "android://hash@com.example.android";
  android_form.app_display_name.clear();

  auto shown_origin = GetShownOrigin(CredentialUIEntry(android_form));
  auto shown_url = GetShownUrl(CredentialUIEntry(android_form));

  EXPECT_EQ("android.example.com", shown_origin);
  EXPECT_EQ("https://play.google.com/store/apps/details?id=com.example.android",
            shown_url.spec());
}

TEST(GetShownOriginAndLinkUrlTest, OriginFromAndroidForm_WithAppDisplayName) {
  PasswordForm android_form;
  android_form.signon_realm = "android://hash@com.example.android";
  android_form.app_display_name = "Example Android App";

  auto shown_origin = GetShownOrigin(CredentialUIEntry(android_form));
  auto shown_url = GetShownUrl(CredentialUIEntry(android_form));

  EXPECT_EQ("Example Android App", shown_origin);
  EXPECT_EQ("https://play.google.com/store/apps/details?id=com.example.android",
            shown_url.spec());
}

TEST(GetShownOriginAndLinkUrlTest, OriginFromNonAndroidForm) {
  PasswordForm form;
  form.signon_realm = "https://example.com/";
  form.url = GURL("https://example.com/login?ref=1");

  auto shown_origin = GetShownOrigin(CredentialUIEntry(form));
  auto shown_url = GetShownUrl(CredentialUIEntry(form));

  EXPECT_EQ("example.com", shown_origin);
  EXPECT_EQ(GURL("https://example.com/login?ref=1"), shown_url);
}

TEST(ToUsernameString, NonEmptyUsername) {
  EXPECT_EQ(ToUsernameString("nadeshiko"), u"nadeshiko");
}

TEST(ToUsernameString, EmptyUsername) {
  EXPECT_EQ(ToUsernameString(""),
            l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN));
}

TEST(CalculateTriggerSubmissionTest, CalculateTriggerSubmission) {
  EXPECT_FALSE(
      CalculateTriggerSubmission(SubmissionReadinessState::kNoInformation));
  EXPECT_FALSE(CalculateTriggerSubmission(SubmissionReadinessState::kError));
  EXPECT_FALSE(
      CalculateTriggerSubmission(SubmissionReadinessState::kNoUsernameField));
  EXPECT_FALSE(
      CalculateTriggerSubmission(SubmissionReadinessState::kNoPasswordField));
  EXPECT_FALSE(CalculateTriggerSubmission(
      SubmissionReadinessState::kFieldBetweenUsernameAndPassword));
  EXPECT_FALSE(CalculateTriggerSubmission(
      SubmissionReadinessState::kFieldAfterPasswordField));
  EXPECT_FALSE(
      CalculateTriggerSubmission(SubmissionReadinessState::kLikelyHasCaptcha));

  EXPECT_TRUE(
      CalculateTriggerSubmission(SubmissionReadinessState::kEmptyFields));
  EXPECT_TRUE(
      CalculateTriggerSubmission(SubmissionReadinessState::kMoreThanTwoFields));
  EXPECT_TRUE(CalculateTriggerSubmission(SubmissionReadinessState::kTwoFields));
}

TEST(CalculateSubmissionReadinessTest, Error) {
  FormData form;
  EXPECT_EQ(CalculateSubmissionReadiness(form, 0, 0),
            SubmissionReadinessState::kError);

  FormFieldData field_1;
  form.set_fields({field_1});
  // Indices out of bounds.
  EXPECT_EQ(CalculateSubmissionReadiness(form, 1, 1),
            SubmissionReadinessState::kError);
}

TEST(CalculateSubmissionReadinessTest, NoUsernameField) {
  FormData form;
  FormFieldData field_1;
  form.set_fields({field_1});
  // Username index out of bounds.
  EXPECT_EQ(CalculateSubmissionReadiness(form, 1, 0),
            SubmissionReadinessState::kNoUsernameField);
}

TEST(CalculateSubmissionReadinessTest, NoPasswordField) {
  FormData form;
  FormFieldData field_1;
  form.set_fields({field_1});
  // Password index out of bounds.
  EXPECT_EQ(CalculateSubmissionReadiness(form, 0, 1),
            SubmissionReadinessState::kNoPasswordField);
}

TEST(CalculateSubmissionReadinessTest, FieldBetweenUsernameAndPassword) {
  FormData form;
  FormFieldData field_1;
  FormFieldData field_2;
  FormFieldData field_3;
  form.set_fields({field_1, field_2, field_3});

  EXPECT_EQ(CalculateSubmissionReadiness(form, 0, 2),
            SubmissionReadinessState::kFieldBetweenUsernameAndPassword);
}

TEST(CalculateSubmissionReadinessTest, FieldBetweenUsernameAndPasswordIgnored) {
  FormData form;
  FormFieldData field_1;
  FormFieldData field_2;
  field_2.set_is_focusable(false);
  FormFieldData field_3;
  form.set_fields({field_1, field_2, field_3});

  // Intermediate field is ignored because it is not focusable.
  EXPECT_EQ(CalculateSubmissionReadiness(form, 0, 2),
            SubmissionReadinessState::kTwoFields);
}

TEST(CalculateSubmissionReadinessTest, FieldAfterPasswordField) {
  FormData form;
  FormFieldData field_1;
  FormFieldData field_2;
  FormFieldData field_3;
  form.set_fields({field_1, field_2, field_3});

  EXPECT_EQ(CalculateSubmissionReadiness(form, 0, 1),
            SubmissionReadinessState::kFieldAfterPasswordField);
}

TEST(CalculateSubmissionReadinessTest, FieldAfterPasswordFieldIgnored) {
  FormData form;
  FormFieldData field_1;
  FormFieldData field_2;
  FormFieldData field_3;
  field_3.set_is_focusable(false);
  form.set_fields({field_1, field_2, field_3});

  // Field after password is ignored because it is not focusable.
  EXPECT_EQ(CalculateSubmissionReadiness(form, 0, 1),
            SubmissionReadinessState::kTwoFields);

  FormFieldData field_4;
  field_4.set_form_control_type(FormControlType::kInputCheckbox);
  form.set_fields({field_1, field_2, field_4});
  // Field after password is ignored because it is a checkbox.
  EXPECT_EQ(CalculateSubmissionReadiness(form, 0, 1),
            SubmissionReadinessState::kTwoFields);
}

TEST(CalculateSubmissionReadinessTest, LikelyHasCaptcha) {
  FormData form;
  form.set_likely_contains_captcha(true);
  FormFieldData field_1;
  FormFieldData field_2;
  form.set_fields({field_1, field_2});

  EXPECT_EQ(CalculateSubmissionReadiness(form, 0, 1),
            SubmissionReadinessState::kLikelyHasCaptcha);
}

TEST(CalculateSubmissionReadinessTest, EmptyFields) {
  FormData form;
  FormFieldData field_1;
  FormFieldData field_2;
  FormFieldData field_3;
  form.set_fields({field_1, field_2, field_3});

  // Field 1 is empty and visible.
  EXPECT_EQ(CalculateSubmissionReadiness(form, 0, 2),
            SubmissionReadinessState::kFieldBetweenUsernameAndPassword);
}

TEST(CalculateSubmissionReadinessTest, EmptyFieldsIgnored) {
  FormData form;
  FormFieldData pre_field;  // Index 0
  FormFieldData username;   // Index 1
  FormFieldData password;   // Index 2
  form.set_fields({pre_field, username, password});

  // `pre_field` is empty and visible.
  EXPECT_EQ(CalculateSubmissionReadiness(form, 1, 2),
            SubmissionReadinessState::kEmptyFields);

  // If `pre_field` is filled, we now have 3 visible elements.
  pre_field.set_value(u"val");
  form.set_fields({pre_field, username, password});
  EXPECT_EQ(CalculateSubmissionReadiness(form, 1, 2),
            SubmissionReadinessState::kMoreThanTwoFields);
}

TEST(CalculateSubmissionReadinessTest, TwoFields) {
  FormData form;
  FormFieldData field_1;
  FormFieldData field_2;
  form.set_fields({field_1, field_2});

  EXPECT_EQ(CalculateSubmissionReadiness(form, 0, 1),
            SubmissionReadinessState::kTwoFields);
}

TEST(CalculateSubmissionReadinessTest, MoreThanTwoFields) {
  FormData form;
  FormFieldData field_1;
  field_1.set_value(u"val");
  FormFieldData field_2;
  FormFieldData field_3;
  form.set_fields({field_1, field_2, field_3});

  // field_1 is before username/password (indices 1 and 2).
  EXPECT_EQ(CalculateSubmissionReadiness(form, 1, 2),
            SubmissionReadinessState::kMoreThanTwoFields);
}

TEST(CalculateSubmissionReadinessTest,
     FieldBetweenUsernameAndPassword_TakesPrecedenceOverCaptcha) {
  FormData form;
  form.set_likely_contains_captcha(true);
  FormFieldData field_1;
  FormFieldData field_2;
  FormFieldData field_3;
  form.set_fields({field_1, field_2, field_3});

  // field_2 is between username (0) and password (2).
  // Expect kFieldBetweenUsernameAndPassword, not kLikelyHasCaptcha.
  EXPECT_EQ(CalculateSubmissionReadiness(form, 0, 2),
            SubmissionReadinessState::kFieldBetweenUsernameAndPassword);
}

TEST(CalculateSubmissionReadinessTest,
     FieldAfterPasswordField_TakesPrecedenceOverCaptcha) {
  FormData form;
  form.set_likely_contains_captcha(true);
  FormFieldData field_1;
  FormFieldData field_2;
  FormFieldData field_3;
  form.set_fields({field_1, field_2, field_3});

  // field_3 is after password (1).
  // Expect kFieldAfterPasswordField, not kLikelyHasCaptcha.
  EXPECT_EQ(CalculateSubmissionReadiness(form, 0, 1),
            SubmissionReadinessState::kFieldAfterPasswordField);
}

TEST(CalculateSubmissionReadinessTest,
     LikelyHasCaptcha_TakesPrecedenceOverEmptyFields) {
  FormData form;
  form.set_likely_contains_captcha(true);
  FormFieldData field_1;
  FormFieldData field_2;
  FormFieldData field_3;
  form.set_fields({field_1, field_2, field_3});

  // field_1 is empty and before username (1).
  // This would normally trigger kEmptyFields (or kMoreThanTwoFields if filled).
  // But Captcha check comes first.
  EXPECT_EQ(CalculateSubmissionReadiness(form, 1, 2),
            SubmissionReadinessState::kLikelyHasCaptcha);
}

TEST(CalculateSubmissionReadinessTest,
     FieldBetweenUsernameAndPassword_RadioNotIgnored) {
  FormData form;
  FormFieldData field_1;
  FormFieldData field_2;
  field_2.set_form_control_type(FormControlType::kInputRadio);
  FormFieldData field_3;
  form.set_fields({field_1, field_2, field_3});

  // Radio button between username and password should block submission.
  EXPECT_EQ(CalculateSubmissionReadiness(form, 0, 2),
            SubmissionReadinessState::kFieldBetweenUsernameAndPassword);
}

}  // namespace password_manager
