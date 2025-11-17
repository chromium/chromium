// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO: crbug.com/352295124 - Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "components/password_manager/ios/account_select_fill_data.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/ios/features.h"
#include "components/password_manager/ios/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using autofill::FieldRendererId;
using autofill::FormRendererId;
using autofill::PasswordFormFillData;
using password_manager::AccountSelectFillData;
using password_manager::FillData;
using password_manager::FillDataRetrievalResult;
using password_manager::FillDataRetrievalStatus;
using password_manager::FormInfo;
using password_manager::FormInfoRetrievalError;
using password_manager::FormInfoRetrievalResult;
using password_manager::UsernameAndRealm;
using test_helpers::SetPasswordFormFillData;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::FieldsAre;

namespace {
// Test data.
constexpr char kUrl[] = "http://example.com/";
constexpr const char* kFormNames[] = {"form_name1", "form_name2"};
constexpr uint32_t kFormUniqueIDs[] = {0, 3};
constexpr const char* kUsernameElements[] = {"username1", "username2"};
constexpr uint32_t kUsernameUniqueIDs[] = {1, 4};
constexpr const char* kUsernames[] = {"user0", "u_s_e_r"};
constexpr const char* kPasswordElements[] = {"password1", "password2"};
constexpr uint32_t kPasswordUniqueIDs[] = {2, 5};
constexpr const char* kPasswords[] = {"password0", "secret"};
constexpr char16_t kBackupPassword[] = u"backup_password";
constexpr const char* kAdditionalUsernames[] = {"u$er2", nullptr};
constexpr const char* kAdditionalPasswords[] = {"secret", nullptr};

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

// Tests that the right number of suggestions is created when there's a
// credential that comes with a backup password.
TEST_F(AccountSelectFillDataTest, RetrieveSuggestions_WithBackupPasswords) {
  PasswordFormFillData form_data = form_data_[0];
  form_data.preferred_login.backup_password_value = kBackupPassword;

  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data, /*always_populate_realm=*/false);

  auto retrieve_suggestions = [&]() {
    return account_select_fill_data.RetrieveSuggestions(
        form_data.form_renderer_id, form_data.username_element_renderer_id,
        /*is_password_field=*/false);
  };

  {
    // Enable the iOS backup password feature.
    base::test::ScopedFeatureList scoped_feature_list{
        password_manager::features::kIOSFillRecoveryPassword};

    // There should be three suggestions:
    //   * 1 for the preferred login.
    //   * 1 for the preferred login's backup password.
    //   * 1 for the additional login.
    EXPECT_THAT(
        retrieve_suggestions(),
        ElementsAre(
            AllOf(Field(&UsernameAndRealm::username,
                        base::ASCIIToUTF16(kUsernames[0])),
                  Field(&UsernameAndRealm::is_backup_credential, false)),
            AllOf(Field(&UsernameAndRealm::username,
                        base::ASCIIToUTF16(kUsernames[0])),
                  Field(&UsernameAndRealm::is_backup_credential, true)),
            AllOf(Field(&UsernameAndRealm::username,
                        base::ASCIIToUTF16(kAdditionalUsernames[0])),
                  Field(&UsernameAndRealm::is_backup_credential, false))));
  }
  {
    // Disable the iOS backup password feature.
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(
        password_manager::features::kIOSFillRecoveryPassword);

    // An additional suggestion shouldn't have been created for the backup
    // password.
    EXPECT_THAT(
        retrieve_suggestions(),
        ElementsAre(
            AllOf(Field(&UsernameAndRealm::username,
                        base::ASCIIToUTF16(kUsernames[0])),
                  Field(&UsernameAndRealm::is_backup_credential, false)),
            AllOf(Field(&UsernameAndRealm::username,
                        base::ASCIIToUTF16(kAdditionalUsernames[0])),
                  Field(&UsernameAndRealm::is_backup_credential, false))));
  }
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
  EXPECT_THAT(
      suggestions,
      ElementsAre(AllOf(Field(&UsernameAndRealm::username,
                              base::ASCIIToUTF16(kUsernames[0])),
                        Field(&UsernameAndRealm::realm, kRealm)),
                  AllOf(Field(&UsernameAndRealm::username,
                              base::ASCIIToUTF16(kAdditionalUsernames[0])),
                        Field(&UsernameAndRealm::realm, kAdditionalRealm))));
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
  FillDataRetrievalResult result = account_select_fill_data.GetFillData(
      base::ASCIIToUTF16(kUsernames[0]), /*is_backup_credential=*/false);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(FillDataRetrievalStatus::kNoCredentials, result.error());
}

// Tests that the right status will be returned when there is no form with fill
// data matching the queried form.
TEST_F(AccountSelectFillDataTest, GetFillData_NoResult_BecauseNoForm) {
  AccountSelectFillData account_select_fill_data;

  // GetFillData() when in stateless mode doesn't need to call
  // RetrieveSuggestions() first.
  FillDataRetrievalResult result = account_select_fill_data.GetFillData(
      u"test-user", /*is_backup_credential=*/false,
      form_data_[0].form_renderer_id,
      form_data_[0].username_element_renderer_id,
      /*is_password_field=*/false);

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(FillDataRetrievalStatus::kNoFormMatch, result.error());
}

// Tests that the right status will be returned when there is no field matching
// the queried field.
TEST_F(AccountSelectFillDataTest, GetFillData_NoResult_BecauseNoField) {
  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data_[0], /*always_populate_realm=*/false);

  // GetFillData() when in stateless mode doesn't need to call
  // RetrieveSuggestions() first.
  FillDataRetrievalResult result = account_select_fill_data.GetFillData(
      u"test-user", /*is_backup_credential=*/false,
      form_data_[0].form_renderer_id, UnexistingFieldRendererId(),
      /*is_password_field=*/false);

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(FillDataRetrievalStatus::kNoFieldMatch, result.error());
}

// Tests getting existing form info for an existing username field.
TEST_F(AccountSelectFillDataTest, GetFormInfo_FocusedOnExistingUsernameField) {
  const auto& form_data = form_data_[0];

  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data, /*always_populate_realm=*/false);

  FormInfoRetrievalResult result = account_select_fill_data.GetFormInfo(
      form_data.form_renderer_id, form_data.username_element_renderer_id,
      /*is_password_field=*/false);

  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(
      *result.value(),
      FieldsAre(
          /*origin=*/form_data.url,
          /*form_id=*/form_data.form_renderer_id,
          /*username_element_id=*/form_data.username_element_renderer_id,
          /*password_element_id=*/form_data.password_element_renderer_id));
}

// Tests getting form info for an unexisting username field that was no added to
// the data.
TEST_F(AccountSelectFillDataTest,
       GetFormInfo_FocusedOnUnexistingUsernameField) {
  const auto& form_data = form_data_[0];

  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data, /*always_populate_realm=*/false);

  FormInfoRetrievalResult result = account_select_fill_data.GetFormInfo(
      form_data.form_renderer_id, UnexistingFieldRendererId(),
      /*is_password_field=*/false);

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(FormInfoRetrievalError::kNoFieldMatch, result.error());
}

// Tests getting existing form info when focus on a random password field.
TEST_F(AccountSelectFillDataTest, GetFormInfo_FocusedOnPasswordField) {
  const auto& form_data = form_data_[0];

  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data, /*always_populate_realm=*/false);

  // Get form info for a password field with a unexisting field renderer ID,
  // which should still give a non-null result because any password field should
  // get form info.
  FormInfoRetrievalResult result = account_select_fill_data.GetFormInfo(
      form_data.form_renderer_id, UnexistingFieldRendererId(),
      /*is_password_field=*/true);

  EXPECT_TRUE(result.has_value());
  EXPECT_THAT(*result.value(),
              FieldsAre(/*origin=*/form_data.url,
                        /*form_id=*/form_data.form_renderer_id,
                        /*username_element_id=*/
                        form_data.username_element_renderer_id,
                        /*password_element_id=*/
                        form_data.password_element_renderer_id));
}

// Test getting form info when there is no data for the form.
TEST_F(AccountSelectFillDataTest, GetFormInfo_NoMatch) {
  const auto& form_data = form_data_[0];

  AccountSelectFillData account_select_fill_data;

  FormInfoRetrievalResult result = account_select_fill_data.GetFormInfo(
      form_data.form_renderer_id, form_data.username_element_renderer_id,
      /*is_password_field=*/false);

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(FormInfoRetrievalError::kNoFormMatch, result.error());
}

// This test fixture is parametrized to run tests on a password and a
// non-password field.
class AccountSelectFillDataFieldTypeTest
    : public AccountSelectFillDataTest,
      public testing::WithParamInterface<bool> {
 public:
  bool IsPasswordField() const { return GetParam(); }
};

TEST_P(AccountSelectFillDataFieldTypeTest, RetrieveSuggestionsOneForm) {
  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data_[0],
                               /*always_populate_realm=*/false);

  const bool is_password_field = IsPasswordField();
  const FieldRendererId field_id =
      is_password_field ? form_data_[0].password_element_renderer_id
                        : form_data_[0].username_element_renderer_id;
  std::vector<UsernameAndRealm> suggestions =
      account_select_fill_data.RetrieveSuggestions(
          form_data_[0].form_renderer_id, field_id, is_password_field);
  EXPECT_THAT(
      suggestions,
      ElementsAre(AllOf(Field(&UsernameAndRealm::username,
                              base::ASCIIToUTF16(kUsernames[0])),
                        Field(&UsernameAndRealm::realm, std::string())),
                  AllOf(Field(&UsernameAndRealm::username,
                              base::ASCIIToUTF16(kAdditionalUsernames[0])),
                        Field(&UsernameAndRealm::realm, std::string()))));
}

TEST_P(AccountSelectFillDataFieldTypeTest, GetFillData_WhenNotStateless) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      password_manager::features::kIOSStatelessFillDataFlow);

  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data_[0],
                               /*always_populate_realm=*/false);
  account_select_fill_data.Add(form_data_[1],
                               /*always_populate_realm=*/false);

  const bool is_password_field = IsPasswordField();
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
    FillDataRetrievalResult result = account_select_fill_data.GetFillData(
        base::ASCIIToUTF16(kUsernames[1]), /*is_backup_credential=*/false);
    ASSERT_TRUE(result.has_value());
    EXPECT_THAT(
        *result.value(),
        FieldsAre(/*origin=*/form_data.url,
                  /*form_id=*/form_data.form_renderer_id,
                  /*username_element_id=*/
                  FieldRendererId(kUsernameUniqueIDs[form_i]),
                  /*username_value=*/base::ASCIIToUTF16(kUsernames[1]),
                  /*password_element_id=*/password_field_id,
                  /*password_value=*/base::ASCIIToUTF16(kPasswords[1])));
  }
}

// Tests that the GetFillData() interface to be used when in stateless mode
// works correctly.
TEST_P(AccountSelectFillDataFieldTypeTest, GetFillData) {
  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data_[0],
                               /*always_populate_realm=*/false);
  account_select_fill_data.Add(form_data_[1],
                               /*always_populate_realm=*/false);

  const bool is_password_field = IsPasswordField();
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

    // GetFillData() when in stateless mode doesn't need to call
    // RetrieveSuggestions() first.
    FillDataRetrievalResult result = account_select_fill_data.GetFillData(
        base::ASCIIToUTF16(kUsernames[1]), /*is_backup_credential=*/false,
        form_data.form_renderer_id, clicked_field, is_password_field);
    ASSERT_TRUE(result.has_value());
    EXPECT_THAT(
        *result.value(),
        FieldsAre(/*origin=*/form_data.url,
                  /*form_id=*/form_data.form_renderer_id,
                  /*username_element_id=*/
                  FieldRendererId(kUsernameUniqueIDs[form_i]),
                  /*username_value=*/base::ASCIIToUTF16(kUsernames[1]),
                  /*password_element_id=*/password_field_id,
                  /*password_value=*/base::ASCIIToUTF16(kPasswords[1])));
  }
}

TEST_P(AccountSelectFillDataFieldTypeTest, CrossOriginSuggestionHasRealm) {
  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data_[0],
                               /*always_populate_realm=*/true);

  const bool is_password_field = IsPasswordField();
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

INSTANTIATE_TEST_SUITE_P(All,
                         AccountSelectFillDataFieldTypeTest,
                         testing::Bool());

// This test fixture is parameterized to run tests on password and non-password
// fields, and to verify the behavior for a main credential and its backup.
class AccountSelectFillDataFieldAndCredentialTypeTest
    : public AccountSelectFillDataTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  bool IsPasswordField() const { return std::get<0>(GetParam()); }
  bool IsBackupCredential() const { return std::get<1>(GetParam()); }
};

// Tests that the right fill data is returned when there's a credential that
// comes with a backup password.
TEST_P(AccountSelectFillDataFieldAndCredentialTypeTest,
       GetFillData_WithBackupPasswords) {
  // Enable the iOS backup password feature.
  base::test::ScopedFeatureList scoped_feature_list{
      password_manager::features::kIOSFillRecoveryPassword};

  PasswordFormFillData form_data = form_data_[0];
  form_data.preferred_login.backup_password_value = kBackupPassword;

  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data, /*always_populate_realm=*/false);

  const bool is_password_field = IsPasswordField();
  const bool is_backup_credential = IsBackupCredential();

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
  FillDataRetrievalResult result = account_select_fill_data.GetFillData(
      base::ASCIIToUTF16(kUsernames[0]), is_backup_credential);
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(*result.value(),
              FieldsAre(
                  /*origin=*/form_data.url,
                  /*form_id=*/form_data.form_renderer_id,
                  /*username_element_id=*/
                  FieldRendererId(kUsernameUniqueIDs[0]),
                  /*username_value=*/base::ASCIIToUTF16(kUsernames[0]),
                  /*password_element_id=*/password_field_id,
                  /*password_value=*/
                  is_backup_credential ? kBackupPassword
                                       : base::ASCIIToUTF16(kPasswords[0])));
}

INSTANTIATE_TEST_SUITE_P(All,
                         AccountSelectFillDataFieldAndCredentialTypeTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace
