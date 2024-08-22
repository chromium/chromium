// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO: crbug.com/352295124 - Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "components/password_manager/ios/account_select_fill_data.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/ios/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using autofill::FieldRendererId;
using autofill::FormRendererId;
using autofill::PasswordFormFillData;
using password_manager::AccountSelectFillData;
using password_manager::FillData;
using password_manager::UsernameAndRealm;
using test_helpers::SetPasswordFormFillData;

namespace {
// Test data.
const char* kUrl = "http://example.com/";
const char* kFormNames[] = {"form_name1", "form_name2"};
const uint32_t kFormUniqueIDs[] = {0, 3};
const char* kUsernameElements[] = {"username1", "username2"};
const uint32_t kUsernameUniqueIDs[] = {1, 4};
const char* kUsernames[] = {"user0", "u_s_e_r"};
const char* kPasswordElements[] = {"password1", "password2"};
const uint32_t kPasswordUniqueIDs[] = {2, 5};
const char* kPasswords[] = {"password0", "secret"};
const char* kAdditionalUsernames[] = {"u$er2", nullptr};
const char* kAdditionalPasswords[] = {"secret", nullptr};

// Returns a field renderer ID that isn't used in any testing data, which
// represents an unexisting field renderer ID.
autofill::FieldRendererId UnexistingFieldRendererId() {
  return autofill::FieldRendererId(1000);
}

// Returns form data for a single username form with credentials eligible for
// filling that has at least one non-empty username.
PasswordFormFillData EligibleSingleUsernameFormData() {
  // Set fill data with 1 non-empty username and 1 empty username.
  PasswordFormFillData form_data;
  test_helpers::SetPasswordFormFillData(
      kUrl, /*form_name=*/"", /*renderer_id=*/1, /*username_field=*/"",
      /*username_field_id=*/1,
      /*username_value=*/"", /*password_field=*/"", /*password_field_id=*/0,
      /*password_value=*/"secret1", /*additional_username=*/"username",
      /*addition_password=*/"secret2", &form_data);
  return form_data;
}

// Returns form data with only empty usernames.
PasswordFormFillData FormDataWithEmptyUsernamesOnly() {
  // Set fill data with 2 empty usernames.
  PasswordFormFillData form_data;
  test_helpers::SetPasswordFormFillData(
      kUrl, /*form_name=*/"", /*renderer_id=*/1, /*username_field=*/"",
      /*username_field_id=*/2,
      /*username_value=*/"", /*password_field=*/"", /*password_field_id=*/3,
      /*password_value=*/"secret1", /*additional_username=*/"",
      /*addition_password=*/"secret2", &form_data);
  return form_data;
}

// Returns form data for a single username form with credentials ineligible for
// filling that only consist of empty usernames.
PasswordFormFillData IneligibleSingleUsernameFormData() {
  PasswordFormFillData form_data = FormDataWithEmptyUsernamesOnly();
  // Set the default field id for the password field to make the form a single
  // username form.
  form_data.password_element_renderer_id = autofill::FieldRendererId(0);
  return form_data;
}

class AccountSelectFillDataTest : public PlatformTest {
 public:
  AccountSelectFillDataTest() {
    for (size_t i = 0; i < std::size(form_data_); ++i) {
      SetPasswordFormFillData(
          kUrl, kFormNames[i], kFormUniqueIDs[i], kUsernameElements[i],
          kUsernameUniqueIDs[i], kUsernames[i], kPasswordElements[i],
          kPasswordUniqueIDs[i], kPasswords[i], kAdditionalUsernames[i],
          kAdditionalPasswords[i], &form_data_[i]);
    }
  }

 protected:
  PasswordFormFillData form_data_[2];
};

TEST_F(AccountSelectFillDataTest, EmptyReset) {
  AccountSelectFillData account_select_fill_data;
  EXPECT_TRUE(account_select_fill_data.Empty());

  account_select_fill_data.Add(form_data_[0],
                               /*always_populate_realm=*/false);
  EXPECT_FALSE(account_select_fill_data.Empty());

  account_select_fill_data.Reset();
  EXPECT_TRUE(account_select_fill_data.Empty());
}

TEST_F(AccountSelectFillDataTest, IsSuggestionsAvailableOneForm) {
  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data_[0],
                               /*always_populate_realm=*/false);

  // Suggestions are available for the correct form and field ids.
  EXPECT_TRUE(account_select_fill_data.IsSuggestionsAvailable(
      form_data_[0].form_renderer_id,
      form_data_[0].username_element_renderer_id, false));
  // Suggestion should be available to any password field.
  EXPECT_TRUE(account_select_fill_data.IsSuggestionsAvailable(
      form_data_[0].form_renderer_id, FieldRendererId(404), true));

  // Suggestions are not available for different form renderer_id.
  EXPECT_FALSE(account_select_fill_data.IsSuggestionsAvailable(
      FormRendererId(404), form_data_[0].username_element_renderer_id, false));
  EXPECT_FALSE(account_select_fill_data.IsSuggestionsAvailable(
      FormRendererId(404), form_data_[0].password_element_renderer_id, true));

  // Suggestions are not available for different field id.
  EXPECT_FALSE(account_select_fill_data.IsSuggestionsAvailable(
      form_data_[0].form_renderer_id, FieldRendererId(404), false));
}

TEST_F(AccountSelectFillDataTest, IsSuggestionsAvailableTwoForms) {
  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data_[0],
                               /*always_populate_realm=*/false);
  account_select_fill_data.Add(form_data_[1],
                               /*always_populate_realm=*/false);

  // Suggestions are available for the correct form and field names.
  EXPECT_TRUE(account_select_fill_data.IsSuggestionsAvailable(
      form_data_[0].form_renderer_id,
      form_data_[0].username_element_renderer_id, false));
  // Suggestions are available for the correct form and field names.
  EXPECT_TRUE(account_select_fill_data.IsSuggestionsAvailable(
      form_data_[1].form_renderer_id,
      form_data_[1].username_element_renderer_id, false));
  // Suggestions are not available for different form id.
  EXPECT_FALSE(account_select_fill_data.IsSuggestionsAvailable(
      FormRendererId(404), form_data_[0].username_element_renderer_id, false));
}

// Test that, when sign-in uff is disabled, IsSuggestionsAvailable() returns
// true on password forms when there are only empty usernames, as the
// suggestions with empty usernames still hold passwords that can be filled.
// This test makes sure that there is no regression in password filling with
// sign-in uff disabled.
TEST_F(AccountSelectFillDataTest, IsSuggestionsAvailable_EmptyUsernames) {
  PasswordFormFillData form_data = FormDataWithEmptyUsernamesOnly();

  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data, /*always_populate_realm=*/false);

  EXPECT_TRUE(account_select_fill_data.IsSuggestionsAvailable(
      form_data.form_renderer_id, form_data.username_element_renderer_id,
      false));
}

// Test that, when sign-in uff is enabled, IsSuggestionsAvailable() still
// returns true on password forms when there are only empty usernames, as the
// suggestions with empty usernames still hold passwords that can be filled.
// Sign-in uff should only target single username forms.
TEST_F(AccountSelectFillDataTest,
       IsSuggestionsAvailable_EmptyUsernames_WhenSigninUffEnabled) {
  PasswordFormFillData form_data = FormDataWithEmptyUsernamesOnly();

  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data, /*always_populate_realm=*/false);

  EXPECT_TRUE(account_select_fill_data.IsSuggestionsAvailable(
      form_data.form_renderer_id, form_data.username_element_renderer_id,
      false));
}

TEST_F(AccountSelectFillDataTest,
       IsSuggestionsAvailable_OnSingleUsernameForm_WhenEligible) {
  PasswordFormFillData form_data = EligibleSingleUsernameFormData();

  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data, /*always_populate_realm=*/false);

  EXPECT_TRUE(account_select_fill_data.IsSuggestionsAvailable(
      form_data.form_renderer_id, form_data.username_element_renderer_id,
      false));
}

TEST_F(AccountSelectFillDataTest,
       IsSuggestionsAvailable_OnSingleUsernameForm_WhenIneligible) {
  PasswordFormFillData form_data = IneligibleSingleUsernameFormData();

  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data, /*always_populate_realm=*/false);

  EXPECT_FALSE(account_select_fill_data.IsSuggestionsAvailable(
      form_data.form_renderer_id, form_data.username_element_renderer_id,
      false));
}

TEST_F(AccountSelectFillDataTest, RetrieveSuggestionsOneForm) {
  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data_[0],
                               /*always_populate_realm=*/false);

  for (bool is_password_field : {false, true}) {
    const FieldRendererId field_id =
        is_password_field ? form_data_[0].password_element_renderer_id
                          : form_data_[0].username_element_renderer_id;
    std::vector<UsernameAndRealm> suggestions =
        account_select_fill_data.RetrieveSuggestions(
            form_data_[0].form_renderer_id, field_id, is_password_field);
    EXPECT_EQ(2u, suggestions.size());
    EXPECT_EQ(base::ASCIIToUTF16(kUsernames[0]), suggestions[0].username);
    EXPECT_EQ(std::string(), suggestions[0].realm);
    EXPECT_EQ(base::ASCIIToUTF16(kAdditionalUsernames[0]),
              suggestions[1].username);
    EXPECT_EQ(std::string(), suggestions[1].realm);
  }
}

TEST_F(AccountSelectFillDataTest, RetrieveSuggestionsTwoForm) {
  // Test that after adding two PasswordFormFillData, suggestions for both forms
  // are shown, with credentials from the second PasswordFormFillData. That
  // emulates the case when credentials in the Password Store were changed
  // between load the first and the second forms.
  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data_[0],
                               /*always_populate_realm=*/false);
  account_select_fill_data.Add(form_data_[1],
                               /*always_populate_realm=*/false);

  std::vector<UsernameAndRealm> suggestions =
      account_select_fill_data.RetrieveSuggestions(
          form_data_[0].form_renderer_id,
          form_data_[0].username_element_renderer_id, false);
  EXPECT_EQ(1u, suggestions.size());
  EXPECT_EQ(base::ASCIIToUTF16(kUsernames[1]), suggestions[0].username);

  suggestions = account_select_fill_data.RetrieveSuggestions(
      form_data_[1].form_renderer_id,
      form_data_[1].username_element_renderer_id, false);
  EXPECT_EQ(1u, suggestions.size());
  EXPECT_EQ(base::ASCIIToUTF16(kUsernames[1]), suggestions[0].username);
}

// Test that, when sign-in uff is disabled, RetrieveSuggestions() returns all
// suggestions on password forms when there are only empty usernames, as the
// suggestions with empty usernames still hold passwords that can be filled.
// This test makes sure that there is no regression in password filling with
// sign-in uff disabled.
TEST_F(AccountSelectFillDataTest, RetrieveSuggestions_EmptyUsernames) {
  PasswordFormFillData form_data = FormDataWithEmptyUsernamesOnly();

  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data, /*always_populate_realm=*/false);

  EXPECT_THAT(account_select_fill_data.RetrieveSuggestions(
                  form_data.form_renderer_id,
                  form_data.username_element_renderer_id, false),
              testing::SizeIs(2));
}

// Test that, when sign-in uff is enabled, RetrieveSuggestions() still returns
// all suggestions on password forms when there are only empty usernames, as the
// suggestions with empty usernames still hold passwords that can be filled.
// This test makes sure that there is no regression in password filling with
// sign-in uff disabled. Sign-in uff should only target single username forms.
TEST_F(AccountSelectFillDataTest,
       RetrieveSuggestions_EmptyUsernames_WhenSigninUffEnabled) {
  PasswordFormFillData form_data = FormDataWithEmptyUsernamesOnly();

  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data, /*always_populate_realm=*/false);

  EXPECT_THAT(account_select_fill_data.RetrieveSuggestions(
                  form_data.form_renderer_id,
                  form_data.username_element_renderer_id, false),
              testing::SizeIs(2));
}

TEST_F(AccountSelectFillDataTest,
       RetrieveSuggestions_OnSingleUsernameForm_WhenEligible) {
  PasswordFormFillData form_data = EligibleSingleUsernameFormData();

  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data, /*always_populate_realm=*/false);

  EXPECT_THAT(account_select_fill_data.RetrieveSuggestions(
                  form_data.form_renderer_id,
                  form_data.username_element_renderer_id, false),
              testing::SizeIs(2));
}

TEST_F(AccountSelectFillDataTest,
       RetrieveSuggestions_OnSingleUsernameForm_WhenIneligible) {
  PasswordFormFillData form_data = IneligibleSingleUsernameFormData();

  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data, /*always_populate_realm=*/false);

  EXPECT_THAT(account_select_fill_data.RetrieveSuggestions(
                  form_data.form_renderer_id,
                  form_data.username_element_renderer_id, false),
              testing::IsEmpty());
}

TEST_F(AccountSelectFillDataTest, RetrievePSLMatchedSuggestions) {
  AccountSelectFillData account_select_fill_data;
  const char* kRealm = "http://a.example.com/";
  std::string kAdditionalRealm = "http://b.example.com/";

  // Make logins to be PSL matched by setting non-empy realm.
  form_data_[0].preferred_login.realm = kRealm;
  form_data_[0].additional_logins.begin()->realm = kAdditionalRealm;

  account_select_fill_data.Add(form_data_[0],
                               /*always_populate_realm=*/false);
  std::vector<UsernameAndRealm> suggestions =
      account_select_fill_data.RetrieveSuggestions(
          form_data_[0].form_renderer_id,
          form_data_[0].username_element_renderer_id, false);
  EXPECT_EQ(2u, suggestions.size());
  EXPECT_EQ(base::ASCIIToUTF16(kUsernames[0]), suggestions[0].username);
  EXPECT_EQ(kRealm, suggestions[0].realm);
  EXPECT_EQ(base::ASCIIToUTF16(kAdditionalUsernames[0]),
            suggestions[1].username);
  EXPECT_EQ(kAdditionalRealm, suggestions[1].realm);
}

TEST_F(AccountSelectFillDataTest, GetFillData) {
  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data_[0],
                               /*always_populate_realm=*/false);
  account_select_fill_data.Add(form_data_[1],
                               /*always_populate_realm=*/false);

  for (bool is_password_field : {false, true}) {
    for (size_t form_i = 0; form_i < std::size(form_data_); ++form_i) {
      const auto& form_data = form_data_[form_i];
      // Suggestions should be shown on any password field on the form. So in
      // case of clicking on a password field it is taken an id different from
      // existing field ids.
      const FieldRendererId password_field_id =
          is_password_field ? FieldRendererId(1000)
                            : form_data.password_element_renderer_id;
      const FieldRendererId clicked_field =
          is_password_field ? password_field_id
                            : form_data.username_element_renderer_id;

      // GetFillData() doesn't have form identifier in arguments, it should be
      // provided in RetrieveSuggestions().
      account_select_fill_data.RetrieveSuggestions(
          form_data.form_renderer_id, clicked_field, is_password_field);
      std::unique_ptr<FillData> fill_data =
          account_select_fill_data.GetFillData(
              base::ASCIIToUTF16(kUsernames[1]));

      ASSERT_TRUE(fill_data);
      EXPECT_EQ(form_data.url, fill_data->origin);
      EXPECT_EQ(form_data.form_renderer_id.value(), fill_data->form_id.value());
      EXPECT_EQ(kUsernameUniqueIDs[form_i],
                fill_data->username_element_id.value());
      EXPECT_EQ(base::ASCIIToUTF16(kUsernames[1]), fill_data->username_value);
      EXPECT_EQ(password_field_id, fill_data->password_element_id);
      EXPECT_EQ(base::ASCIIToUTF16(kPasswords[1]), fill_data->password_value);
    }
  }
}

TEST_F(AccountSelectFillDataTest, GetFillDataOldCredentials) {
  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data_[0],
                               /*always_populate_realm=*/false);
  account_select_fill_data.Add(form_data_[1],
                               /*always_populate_realm=*/false);

  // GetFillData() doesn't have form identifier in arguments, it should be
  // provided in RetrieveSuggestions().
  account_select_fill_data.RetrieveSuggestions(
      form_data_[0].form_renderer_id,
      form_data_[0].username_element_renderer_id, false);

  // AccountSelectFillData should keep only last credentials. Check that in
  // request of old credentials nothing is returned.
  std::unique_ptr<FillData> fill_data =
      account_select_fill_data.GetFillData(base::ASCIIToUTF16(kUsernames[0]));
  EXPECT_FALSE(fill_data);
}

TEST_F(AccountSelectFillDataTest, CrossOriginSuggestionHasRealm) {
  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data_[0],
                               /*always_populate_realm=*/true);

  for (bool is_password_field : {false, true}) {
    const FieldRendererId field_id =
        is_password_field ? form_data_[0].password_element_renderer_id
                          : form_data_[0].username_element_renderer_id;
    std::vector<UsernameAndRealm> suggestions =
        account_select_fill_data.RetrieveSuggestions(
            form_data_[0].form_renderer_id, field_id, is_password_field);
    EXPECT_EQ(2u, suggestions.size());
    EXPECT_EQ(kUrl, suggestions[0].realm);
    EXPECT_EQ(kUrl, suggestions[1].realm);
  }
}

// Tests getting existing form info for an existing username field.
TEST_F(AccountSelectFillDataTest, GetFormInfo_FocusedOnExistingUsernameField) {
  const auto& form_data = form_data_[0];

  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data, /*always_populate_realm=*/false);

  const password_manager::FormInfo* form_info =
      account_select_fill_data.GetFormInfo(
          form_data.form_renderer_id, form_data.username_element_renderer_id,
          /*is_password_field=*/false);

  ASSERT_TRUE(form_info);

  EXPECT_EQ(form_data.url, form_info->origin);
  EXPECT_EQ(form_data.form_renderer_id, form_info->form_id);
  EXPECT_EQ(form_data.username_element_renderer_id,
            form_info->username_element_id);
  EXPECT_EQ(form_data.password_element_renderer_id,
            form_info->password_element_id);
}

// Tests getting form info for an unexisting username field that was no added to
// the data.
TEST_F(AccountSelectFillDataTest,
       GetFormInfo_FocusedOnUnexistingUsernameField) {
  const auto& form_data = form_data_[0];

  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data, /*always_populate_realm=*/false);

  const password_manager::FormInfo* form_info =
      account_select_fill_data.GetFormInfo(form_data.form_renderer_id,
                                           UnexistingFieldRendererId(),
                                           /*is_password_field=*/false);

  EXPECT_FALSE(form_info);
}

// Tests getting existing form info when focus on a random password field.
TEST_F(AccountSelectFillDataTest, GetFormInfo_FocusedOnPasswordField) {
  const auto& form_data = form_data_[0];

  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data, /*always_populate_realm=*/false);

  // Get form info for a password field with a unexisting field renderer ID,
  // which should still give a non-null result because any password field should
  // get form info.
  const password_manager::FormInfo* form_info =
      account_select_fill_data.GetFormInfo(form_data.form_renderer_id,
                                           UnexistingFieldRendererId(),
                                           /*is_password_field=*/true);

  ASSERT_TRUE(form_info);

  EXPECT_EQ(form_data.url, form_info->origin);
  EXPECT_EQ(form_data.form_renderer_id, form_info->form_id);
  EXPECT_EQ(form_data.username_element_renderer_id,
            form_info->username_element_id);
  EXPECT_EQ(form_data.password_element_renderer_id,
            form_info->password_element_id);
}

// Test getting form info when there is no data for the form.
TEST_F(AccountSelectFillDataTest, GetFormInfo_NoMatch) {
  const auto& form_data = form_data_[0];

  AccountSelectFillData account_select_fill_data;

  const password_manager::FormInfo* form_info =
      account_select_fill_data.GetFormInfo(
          form_data.form_renderer_id, form_data.username_element_renderer_id,
          /*is_password_field=*/false);

  EXPECT_FALSE(form_info);
}

}  // namespace
