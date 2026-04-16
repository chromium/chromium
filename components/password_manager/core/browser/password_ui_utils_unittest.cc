// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_ui_utils.h"

#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace password_manager {
namespace {

using ::autofill::FieldGlobalId;
using ::autofill::FormControlType;
using ::autofill::FormData;
using ::autofill::FormFieldData;

FormData CreateForm(size_t num_fields) {
  FormData form;
  for (size_t i = 0; i < num_fields; ++i) {
    FormFieldData field = autofill::test::CreateTestFormField(
        "", "", "", autofill::FormControlType::kInputText);
    test_api(form).Append(field);
  }
  return form;
}

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

class CalculateSubmissionReadinessTest : public testing::Test {
 private:
  autofill::test::AutofillUnitTestEnvironment autofill_environment_;
};

TEST_F(CalculateSubmissionReadinessTest, Error) {
  EXPECT_EQ(CalculateSubmissionReadiness(CreateForm(0), FieldGlobalId(),
                                         FieldGlobalId()),
            SubmissionReadinessState::kError);
  EXPECT_EQ(CalculateSubmissionReadiness(CreateForm(0),
                                         autofill::test::MakeFieldGlobalId(),
                                         autofill::test::MakeFieldGlobalId()),
            SubmissionReadinessState::kError);

  // Unknown field IDs.
  EXPECT_EQ(CalculateSubmissionReadiness(CreateForm(1), FieldGlobalId(),
                                         FieldGlobalId()),
            SubmissionReadinessState::kError);
  EXPECT_EQ(CalculateSubmissionReadiness(CreateForm(1),
                                         autofill::test::MakeFieldGlobalId(),
                                         autofill::test::MakeFieldGlobalId()),
            SubmissionReadinessState::kError);
}

TEST_F(CalculateSubmissionReadinessTest, NoUsernameField) {
  FormData form = CreateForm(1);
  // Username index out of bounds.
  EXPECT_EQ(
      CalculateSubmissionReadiness(form, autofill::test::MakeFieldGlobalId(),
                                   form.fields()[0].global_id()),
      SubmissionReadinessState::kNoUsernameField);
}

TEST_F(CalculateSubmissionReadinessTest, NoPasswordField) {
  FormData form = CreateForm(1);
  // Password index out of bounds.
  EXPECT_EQ(CalculateSubmissionReadiness(form, form.fields()[0].global_id(),
                                         autofill::test::MakeFieldGlobalId()),
            SubmissionReadinessState::kNoPasswordField);
}

TEST_F(CalculateSubmissionReadinessTest, FieldBetweenUsernameAndPassword) {
  FormData form = CreateForm(3);
  EXPECT_EQ(CalculateSubmissionReadiness(form, form.fields()[0].global_id(),
                                         form.fields()[2].global_id()),
            SubmissionReadinessState::kFieldBetweenUsernameAndPassword);
}

TEST_F(CalculateSubmissionReadinessTest,
       FieldBetweenUsernameAndPasswordIgnored) {
  FormData form = CreateForm(3);
  test_api(form).field(1).set_is_focusable(false);

  // Intermediate field is ignored because it is not focusable.
  EXPECT_EQ(CalculateSubmissionReadiness(form, form.fields()[0].global_id(),
                                         form.fields()[2].global_id()),
            SubmissionReadinessState::kTwoFields);
}

TEST_F(CalculateSubmissionReadinessTest, FieldAfterPasswordField) {
  FormData form = CreateForm(3);

  EXPECT_EQ(CalculateSubmissionReadiness(form, form.fields()[0].global_id(),
                                         form.fields()[1].global_id()),
            SubmissionReadinessState::kFieldAfterPasswordField);
}

TEST_F(CalculateSubmissionReadinessTest, FieldAfterPasswordFieldIgnored) {
  FormData form = CreateForm(3);
  test_api(form).field(2).set_is_focusable(false);

  // Field after password is ignored because it is not focusable.
  EXPECT_EQ(CalculateSubmissionReadiness(form, form.fields()[0].global_id(),
                                         form.fields()[1].global_id()),
            SubmissionReadinessState::kTwoFields);

  form = CreateForm(3);
  test_api(form).field(2).set_form_control_type(
      FormControlType::kInputCheckbox);

  // Field after password is ignored because it is a checkbox.
  EXPECT_EQ(CalculateSubmissionReadiness(form, form.fields()[0].global_id(),
                                         form.fields()[1].global_id()),
            SubmissionReadinessState::kTwoFields);
}

TEST_F(CalculateSubmissionReadinessTest, LikelyHasCaptcha) {
  FormData form = CreateForm(2);
  form.set_likely_contains_captcha(true);

  EXPECT_EQ(CalculateSubmissionReadiness(form, form.fields()[0].global_id(),
                                         form.fields()[1].global_id()),
            SubmissionReadinessState::kLikelyHasCaptcha);
}

TEST_F(CalculateSubmissionReadinessTest, EmptyFields) {
  FormData form = CreateForm(3);

  // Field 1 is empty and visible.
  EXPECT_EQ(CalculateSubmissionReadiness(form, form.fields()[0].global_id(),
                                         form.fields()[2].global_id()),
            SubmissionReadinessState::kFieldBetweenUsernameAndPassword);
}

TEST_F(CalculateSubmissionReadinessTest, EmptyFieldsIgnored) {
  // Field 0 is a preceding field.
  // Field 1 is a username field.
  // Field 2 is a password field.
  FormData form = CreateForm(3);

  // `pre_field` is empty and visible.
  EXPECT_EQ(CalculateSubmissionReadiness(form, form.fields()[1].global_id(),
                                         form.fields()[2].global_id()),
            SubmissionReadinessState::kEmptyFields);

  // If the preceding field is filled, we now have 3 visible elements.
  test_api(form).field(0).set_value(u"val");
  EXPECT_EQ(CalculateSubmissionReadiness(form, form.fields()[1].global_id(),
                                         form.fields()[2].global_id()),
            SubmissionReadinessState::kMoreThanTwoFields);
}

TEST_F(CalculateSubmissionReadinessTest, TwoFields) {
  FormData form = CreateForm(2);

  EXPECT_EQ(CalculateSubmissionReadiness(form, form.fields()[0].global_id(),
                                         form.fields()[1].global_id()),
            SubmissionReadinessState::kTwoFields);
}

TEST_F(CalculateSubmissionReadinessTest, MoreThanTwoFields) {
  FormData form = CreateForm(3);
  test_api(form).field(0).set_value(u"val");

  // field_1 is before username/password (indices 1 and 2).
  EXPECT_EQ(CalculateSubmissionReadiness(form, form.fields()[1].global_id(),
                                         form.fields()[2].global_id()),
            SubmissionReadinessState::kMoreThanTwoFields);
}

TEST_F(CalculateSubmissionReadinessTest,
       FieldBetweenUsernameAndPassword_TakesPrecedenceOverCaptcha) {
  FormData form = CreateForm(3);
  form.set_likely_contains_captcha(true);

  // field_2 is between username (0) and password (2).
  // Expect kFieldBetweenUsernameAndPassword, not kLikelyHasCaptcha.
  EXPECT_EQ(CalculateSubmissionReadiness(form, form.fields()[0].global_id(),
                                         form.fields()[2].global_id()),
            SubmissionReadinessState::kFieldBetweenUsernameAndPassword);
}

TEST_F(CalculateSubmissionReadinessTest,
       FieldAfterPasswordField_TakesPrecedenceOverCaptcha) {
  FormData form = CreateForm(3);
  form.set_likely_contains_captcha(true);

  // field_3 is after password (1).
  // Expect kFieldAfterPasswordField, not kLikelyHasCaptcha.
  EXPECT_EQ(CalculateSubmissionReadiness(form, form.fields()[0].global_id(),
                                         form.fields()[1].global_id()),
            SubmissionReadinessState::kFieldAfterPasswordField);
}

TEST_F(CalculateSubmissionReadinessTest,
       LikelyHasCaptcha_TakesPrecedenceOverEmptyFields) {
  FormData form = CreateForm(3);
  form.set_likely_contains_captcha(true);

  // field_1 is empty and before username (1).
  // This would normally trigger kEmptyFields (or kMoreThanTwoFields if filled).
  // But Captcha check comes first.
  EXPECT_EQ(CalculateSubmissionReadiness(form, form.fields()[1].global_id(),
                                         form.fields()[2].global_id()),
            SubmissionReadinessState::kLikelyHasCaptcha);
}

TEST_F(CalculateSubmissionReadinessTest,
       FieldBetweenUsernameAndPassword_RadioNotIgnored) {
  FormData form = CreateForm(3);
  test_api(form).field(1).set_form_control_type(FormControlType::kInputRadio);

  // Radio button between username and password should block submission.
  EXPECT_EQ(CalculateSubmissionReadiness(form, form.fields()[0].global_id(),
                                         form.fields()[2].global_id()),
            SubmissionReadinessState::kFieldBetweenUsernameAndPassword);
}

}  // namespace
}  // namespace password_manager
