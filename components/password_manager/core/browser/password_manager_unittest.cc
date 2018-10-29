// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/new_password_form_manager.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/browser/stub_credentials_filter.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/ukm/test_ukm_recorder.h"
#include "net/cert/cert_status_flags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::FormData;
using autofill::FormFieldData;
using autofill::PasswordForm;
using base::ASCIIToUTF16;
using base::TestMockTimeTaskRunner;
using testing::_;
using testing::AnyNumber;
using testing::IsNull;
using testing::NotNull;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;
using testing::WithArg;

namespace password_manager {

namespace {

class MockStoreResultFilter : public StubCredentialsFilter {
 public:
  MOCK_CONST_METHOD1(ShouldSave, bool(const autofill::PasswordForm& form));
  MOCK_CONST_METHOD1(ReportFormLoginSuccess,
                     void(const PasswordFormManagerInterface& form_manager));
  MOCK_CONST_METHOD1(IsSyncAccountEmail, bool(const std::string&));
  MOCK_CONST_METHOD1(ShouldSaveGaiaPasswordHash,
                     bool(const autofill::PasswordForm&));
  MOCK_CONST_METHOD1(ShouldSaveEnterprisePasswordHash,
                     bool(const autofill::PasswordForm&));
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() {
    EXPECT_CALL(*this, GetStoreResultFilter())
        .Times(AnyNumber())
        .WillRepeatedly(Return(&filter_));
    ON_CALL(filter_, ShouldSave(_)).WillByDefault(Return(true));
    ON_CALL(filter_, ShouldSaveGaiaPasswordHash(_))
        .WillByDefault(Return(false));
    ON_CALL(filter_, ShouldSaveEnterprisePasswordHash(_))
        .WillByDefault(Return(false));
    ON_CALL(filter_, IsSyncAccountEmail(_)).WillByDefault(Return(false));
  }

  MOCK_CONST_METHOD0(IsSavingAndFillingEnabledForCurrentPage, bool());
  MOCK_CONST_METHOD0(GetMainFrameCertStatus, net::CertStatus());
  MOCK_CONST_METHOD0(GetPasswordStore, PasswordStore*());
  // The code inside EXPECT_CALL for PromptUserToSaveOrUpdatePasswordPtr and
  // ShowManualFallbackForSavingPtr owns the PasswordFormManager* argument.
  MOCK_METHOD1(PromptUserToSaveOrUpdatePasswordPtr,
               void(PasswordFormManagerForUI*));
  MOCK_METHOD3(ShowManualFallbackForSavingPtr,
               void(PasswordFormManagerForUI*, bool, bool));
  MOCK_METHOD0(HideManualFallbackForSaving, void());
  MOCK_METHOD1(NotifySuccessfulLoginWithExistingPassword,
               void(const autofill::PasswordForm&));
  MOCK_METHOD0(AutomaticPasswordSaveIndicator, void());
  MOCK_CONST_METHOD0(GetPrefs, PrefService*());
  MOCK_CONST_METHOD0(GetMainFrameURL, const GURL&());
  MOCK_METHOD0(GetDriver, PasswordManagerDriver*());
  MOCK_CONST_METHOD0(GetStoreResultFilter, const MockStoreResultFilter*());

  // Workaround for std::unique_ptr<> lacking a copy constructor.
  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<PasswordFormManagerForUI> manager,
      bool update_password) override {
    PromptUserToSaveOrUpdatePasswordPtr(manager.release());
    return false;
  }
  void ShowManualFallbackForSaving(
      std::unique_ptr<PasswordFormManagerForUI> manager,
      bool has_generated_password,
      bool is_update) override {
    ShowManualFallbackForSavingPtr(manager.release(), has_generated_password,
                                   is_update);
  }
  void AutomaticPasswordSave(
      std::unique_ptr<PasswordFormManagerForUI> manager) override {
    AutomaticPasswordSaveIndicator();
  }

  void FilterAllResultsForSaving() {
    EXPECT_CALL(filter_, ShouldSave(_)).WillRepeatedly(Return(false));
  }

 private:
  testing::NiceMock<MockStoreResultFilter> filter_;
};

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MOCK_METHOD1(FillPasswordForm, void(const autofill::PasswordFormFillData&));
  MOCK_METHOD1(AutofillDataReceived,
               void(const std::map<autofill::FormData,
                                   autofill::PasswordFormFieldPredictionMap>&));
  MOCK_METHOD0(GetPasswordManager, PasswordManager*());
  MOCK_METHOD0(GetPasswordAutofillManager, PasswordAutofillManager*());
};

// Invokes the password store consumer with a single copy of |form|.
ACTION_P(InvokeConsumer, form) {
  std::vector<std::unique_ptr<PasswordForm>> result;
  result.push_back(std::make_unique<PasswordForm>(form));
  arg0->OnGetPasswordStoreResults(std::move(result));
}

ACTION(InvokeEmptyConsumerWithForms) {
  arg0->OnGetPasswordStoreResults(std::vector<std::unique_ptr<PasswordForm>>());
}

ACTION_P(SaveToScopedPtr, scoped) { scoped->reset(arg0); }

void SanitizeFormData(FormData* form) {
  form->main_frame_origin = url::Origin();
  for (FormFieldData& field : form->fields) {
    field.label.clear();
    field.value.clear();
    field.autocomplete_attribute.clear();
    field.option_values.clear();
    field.option_contents.clear();
    field.placeholder.clear();
    field.css_classes.clear();
    field.id.clear();
  }
}

}  // namespace

class PasswordManagerTest : public testing::Test {
 public:
  PasswordManagerTest()
      : test_url_("https://www.example.com"),
        task_runner_(new TestMockTimeTaskRunner) {}
  ~PasswordManagerTest() override = default;

 protected:
  void SetUp() override {
    store_ = new testing::StrictMock<MockPasswordStore>;
    EXPECT_CALL(*store_, ReportMetrics(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(*store_, GetLoginsForSameOrganizationName(_, _))
        .Times(AnyNumber());
    CHECK(store_->Init(syncer::SyncableService::StartSyncFlare(), nullptr));

    EXPECT_CALL(client_, GetPasswordStore())
        .WillRepeatedly(Return(store_.get()));
    EXPECT_CALL(*store_, GetSiteStatsImpl(_)).Times(AnyNumber());
    EXPECT_CALL(client_, GetDriver()).WillRepeatedly(Return(&driver_));

    manager_.reset(new PasswordManager(&client_));
    password_autofill_manager_.reset(
        new PasswordAutofillManager(client_.GetDriver(), nullptr, &client_));

    EXPECT_CALL(driver_, GetPasswordManager())
        .WillRepeatedly(Return(manager_.get()));
    EXPECT_CALL(driver_, GetPasswordAutofillManager())
        .WillRepeatedly(Return(password_autofill_manager_.get()));
    EXPECT_CALL(client_, GetMainFrameCertStatus()).WillRepeatedly(Return(0));

    EXPECT_CALL(*store_, IsAbleToSavePasswords()).WillRepeatedly(Return(true));

    ON_CALL(client_, GetMainFrameURL()).WillByDefault(ReturnRef(test_url_));
  }

  void TearDown() override {
    store_->ShutdownOnUIThread();
    store_ = nullptr;
  }

  PasswordForm MakeSimpleForm() {
    PasswordForm form;
    form.origin = GURL("http://www.google.com/a/LoginAuth");
    form.action = GURL("http://www.google.com/a/Login");
    form.username_element = ASCIIToUTF16("Email");
    form.password_element = ASCIIToUTF16("Passwd");
    form.username_value = ASCIIToUTF16("googleuser");
    form.password_value = ASCIIToUTF16("p4ssword");
    form.submit_element = ASCIIToUTF16("signIn");
    form.signon_realm = "http://www.google.com/";

    // Fill |form.form_data|.
    form.form_data.origin = form.origin;
    form.form_data.action = form.action;
    form.form_data.name = ASCIIToUTF16("the-form-name");
    form.form_data.unique_renderer_id = 10;

    autofill::FormFieldData field;
    field.name = ASCIIToUTF16("Email");
    field.id = field.name;
    field.value = ASCIIToUTF16("googleuser");
    field.form_control_type = "text";
    field.unique_renderer_id = 2;
    form.form_data.fields.push_back(field);

    field.name = ASCIIToUTF16("Passwd");
    field.id = field.name;
    field.value = ASCIIToUTF16("p4ssword");
    field.form_control_type = "password";
    field.unique_renderer_id = 3;
    form.form_data.fields.push_back(field);

    return form;
  }

  PasswordForm MakeSimpleGAIAForm() {
    PasswordForm form = MakeSimpleForm();
    form.origin = GURL("https://accounts.google.com");
    form.signon_realm = form.origin.spec();
    return form;
  }

  PasswordForm MakeGAIAChangePasswordForm() {
    PasswordForm form;
    form.origin = GURL("https://accounts.google.com");
    form.action = GURL("http://www.google.com/a/Login");
    form.username_element = ASCIIToUTF16("Email");
    form.new_password_element = ASCIIToUTF16("NewPasswd");
    form.username_value = ASCIIToUTF16("googleuser");
    form.new_password_value = ASCIIToUTF16("n3wp4ssword");
    form.submit_element = ASCIIToUTF16("changePassword");
    form.signon_realm = form.origin.spec();
    form.form_data.name = ASCIIToUTF16("the-form-name");
    return form;
  }

  // Create a sign-up form that only has a new password field.
  PasswordForm MakeFormWithOnlyNewPasswordField() {
    PasswordForm form = MakeSimpleForm();
    form.new_password_element.swap(form.password_element);
    form.new_password_value.swap(form.password_value);
    return form;
  }

  PasswordForm MakeAndroidCredential() {
    PasswordForm android_form;
    android_form.origin = GURL("android://hash@google.com");
    android_form.signon_realm = "android://hash@google.com";
    android_form.username_value = ASCIIToUTF16("google");
    android_form.password_value = ASCIIToUTF16("password");
    android_form.is_affiliation_based_match = true;
    return android_form;
  }

  // Reproduction of the form present on twitter's login page.
  PasswordForm MakeTwitterLoginForm() {
    PasswordForm form;
    form.origin = GURL("https://twitter.com/");
    form.action = GURL("https://twitter.com/sessions");
    form.username_element = ASCIIToUTF16("Email");
    form.password_element = ASCIIToUTF16("Passwd");
    form.username_value = ASCIIToUTF16("twitter");
    form.password_value = ASCIIToUTF16("password");
    form.submit_element = ASCIIToUTF16("signIn");
    form.signon_realm = "https://twitter.com/";
    return form;
  }

  // Reproduction of the form present on twitter's failed login page.
  PasswordForm MakeTwitterFailedLoginForm() {
    PasswordForm form;
    form.origin = GURL("https://twitter.com/login/error?redirect_after_login");
    form.action = GURL("https://twitter.com/sessions");
    form.username_element = ASCIIToUTF16("EmailField");
    form.password_element = ASCIIToUTF16("PasswdField");
    form.username_value = ASCIIToUTF16("twitter");
    form.password_value = ASCIIToUTF16("password");
    form.submit_element = ASCIIToUTF16("signIn");
    form.signon_realm = "https://twitter.com/";
    return form;
  }

  PasswordForm MakeSimpleFormWithOnlyPasswordField() {
    PasswordForm form(MakeSimpleForm());
    form.username_element.clear();
    form.username_value.clear();
    return form;
  }

  PasswordManager* manager() { return manager_.get(); }

  void OnPasswordFormSubmitted(const PasswordForm& form) {
    manager()->OnPasswordFormSubmitted(&driver_, form);
  }

  void TurnOnNewParsingForSaving(
      base::test::ScopedFeatureList* scoped_feature_list) {
    scoped_feature_list->InitWithFeatures(
        {features::kNewPasswordFormParsing,
         features::kNewPasswordFormParsingForSaving},
        {});
    manager_.reset(new PasswordManager(&client_));
  }

  const GURL test_url_;
  base::MessageLoop message_loop_;
  scoped_refptr<MockPasswordStore> store_;
  testing::NiceMock<MockPasswordManagerClient> client_;
  MockPasswordManagerDriver driver_;
  std::unique_ptr<PasswordAutofillManager> password_autofill_manager_;
  std::unique_ptr<PasswordManager> manager_;
  scoped_refptr<TestMockTimeTaskRunner> task_runner_;
};

MATCHER_P(FormMatches, form, "") {
  return form.signon_realm == arg.signon_realm && form.origin == arg.origin &&
         form.action == arg.action &&
         form.username_element == arg.username_element &&
         form.username_value == arg.username_value &&
         form.password_element == arg.password_element &&
         form.password_value == arg.password_value &&
         form.new_password_element == arg.new_password_element;
}

TEST_F(PasswordManagerTest, FormSubmitWithOnlyNewPasswordField) {
  // Test that when a form only contains a "new password" field, the form gets
  // saved and in password store, the new password value is saved as a current
  // password value.
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate saving the form, as if the info bar was accepted.
  PasswordForm saved_form;
  EXPECT_CALL(*store_, AddLogin(_)).WillOnce(SaveArg<0>(&saved_form));
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();

  // The value of the new password field should have been promoted to, and saved
  // to the password store as the current password.
  PasswordForm expected_form(form);
  expected_form.password_value.swap(expected_form.new_password_value);
  expected_form.password_element.swap(expected_form.new_password_element);
  EXPECT_THAT(saved_form, FormMatches(expected_form));
}

TEST_F(PasswordManagerTest, GeneratedPasswordFormSubmitEmptyStore) {
  for (bool new_parsing_for_saving : {false, true}) {
    SCOPED_TRACE(testing::Message()
                 << "new_parsing_for_saving = " << new_parsing_for_saving);
    base::test::ScopedFeatureList scoped_feature_list;
    if (new_parsing_for_saving)
      TurnOnNewParsingForSaving(&scoped_feature_list);
    // Test that generated passwords are stored without asking the user.
    std::vector<PasswordForm> observed;
    PasswordForm form(MakeFormWithOnlyNewPasswordField());
    observed.push_back(form);
    EXPECT_CALL(*store_, GetLogins(_, _))
        .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
    manager()->OnPasswordFormsParsed(&driver_, observed);
    manager()->OnPasswordFormsRendered(&driver_, observed, true);

    // Simulate the user generating the password and submitting the form.
    EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*store_, AddLogin(_));
    manager()->OnPresaveGeneratedPassword(&driver_, form);
    OnPasswordFormSubmitted(form);

    // The user should not need to confirm saving as they have already given
    // consent by using the generated password. The form should be saved once
    // navigation occurs. The client will be informed that automatic saving has
    // occurred.
    EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
    PasswordForm form_to_save;
    EXPECT_CALL(*store_, UpdateLoginWithPrimaryKey(_, _))
        .WillOnce(SaveArg<0>(&form_to_save));
    EXPECT_CALL(client_, AutomaticPasswordSaveIndicator());

    // Now the password manager waits for the navigation to complete.
    observed.clear();
    manager()->OnPasswordFormsParsed(&driver_, observed);
    manager()->OnPasswordFormsRendered(&driver_, observed, true);
    EXPECT_EQ(form.username_value, form_to_save.username_value);
    // What was "new password" field in the submitted form, becomes the current
    // password field in the form to save.
    EXPECT_EQ(form.new_password_value, form_to_save.password_value);
  }
}

TEST_F(PasswordManagerTest, FormSubmitNoGoodMatch) {
  // When the password store already contains credentials for a given form, new
  // credentials get still added, as long as they differ in username from the
  // stored ones.
  PasswordForm existing_different(MakeSimpleForm());
  existing_different.username_value = ASCIIToUTF16("google2");

  PasswordForm form(MakeSimpleForm());
  std::vector<PasswordForm> observed = {form};
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(2);
  EXPECT_CALL(*store_, GetLogins(PasswordStore::FormDigest(form), _))
      .WillOnce(WithArg<1>(InvokeConsumer(existing_different)));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form);

  // We still expect an add, since we didn't have a good match.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->DidNavigateMainFrame();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate saving the form.
  EXPECT_CALL(*store_, AddLogin(FormMatches(form)));
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();
}

TEST_F(PasswordManagerTest, BestMatchFormToManager) {
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);

  std::vector<PasswordForm> observed;
  // Observe the form that will be submitted.
  PasswordForm form(MakeSimpleForm());

  // This form is different from the on that will be submitted.
  PasswordForm no_match_form(MakeSimpleForm());
  no_match_form.form_data.name = ASCIIToUTF16("another-name");
  no_match_form.action = GURL("http://www.google.com/somethingelse");
  autofill::FormFieldData field;
  field.name = ASCIIToUTF16("another-field-name");
  no_match_form.form_data.fields.push_back(field);

  observed.push_back(no_match_form);
  observed.push_back(form);

  // Simulate observing forms after navigation the page.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  // The form is modified before being submitted and does not match perfectly.
  // Out of the criteria {name, action, signature}, we keep the signature the
  // same and change the rest.
  PasswordForm changed_form(form);
  changed_form.username_element = ASCIIToUTF16("changed-name");
  changed_form.action = GURL("http://www.google.com/changed-action");
  OnPasswordFormSubmitted(changed_form);
  EXPECT_EQ(CalculateFormSignature(form.form_data),
            CalculateFormSignature(changed_form.form_data));

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Verify that PasswordFormManager to be save owns the correct pair of
  // observed and submitted forms.
  // TODO(https://crbug.com/831123): Implement subsequent expectation with
  // PasswordFormManagerInterface and avoid casting to PasswordFormManager*.
  PasswordFormManager* form_manager =
      static_cast<PasswordFormManager*>(form_manager_to_save.get());
  EXPECT_EQ(form.action, form_manager->observed_form().action);
  EXPECT_EQ(form.form_data.name, form_manager->observed_form().form_data.name);
  EXPECT_EQ(changed_form.action, form_manager->GetSubmittedForm()->action);
  EXPECT_EQ(changed_form.form_data.name,
            form_manager->GetSubmittedForm()->form_data.name);
}

// As long as the is a PasswordFormManager that matches the origin, we should
// not fail to match a submitted PasswordForm to a PasswordFormManager.
TEST_F(PasswordManagerTest, AnyMatchFormToManager) {
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);

  // Observe the form that will be submitted.
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);

  // Simulate observing forms after navigation the page.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  // The form is modified before being submitted and does not match perfectly.
  // We change all of the criteria: {name, action, signature}.
  PasswordForm changed_form(form);
  autofill::FormFieldData field;
  field.name = ASCIIToUTF16("another-field-name");
  changed_form.form_data.fields.push_back(field);
  changed_form.username_element = ASCIIToUTF16("changed-name");
  changed_form.action = GURL("http://www.google.com/changed-action");
  OnPasswordFormSubmitted(changed_form);
  EXPECT_NE(CalculateFormSignature(form.form_data),
            CalculateFormSignature(changed_form.form_data));

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Verify that we matched the form to a PasswordFormManager, although with the
  // worst possible match.
  // TODO(https://crbug.com/831123): Implement subsequent expectation with
  // PasswordFormManagerInterface and avoid casting to PasswordFormManager*.
  PasswordFormManager* form_manager =
      static_cast<PasswordFormManager*>(form_manager_to_save.get());
  EXPECT_EQ(form.action, form_manager->observed_form().action);
  EXPECT_EQ(form.form_data.name, form_manager->observed_form().form_data.name);
  EXPECT_EQ(changed_form.action, form_manager->GetSubmittedForm()->action);
  EXPECT_EQ(changed_form.form_data.name,
            form_manager->GetSubmittedForm()->form_data.name);
}

// Tests that a credential wouldn't be saved if it is already in the store.
TEST_F(PasswordManagerTest, DontSaveAlreadySavedCredential) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));

  PasswordForm form(MakeSimpleForm());
  std::vector<PasswordForm> observed = {form};
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillOnce(WithArg<1>(InvokeConsumer(form)));
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(2);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // The user is typing a credential manually. Till the credential is different
  // from the saved one, the fallback should be available.
  PasswordForm incomplete_match(form);
  incomplete_match.password_value =
      form.password_value.substr(0, form.password_value.length() - 1);
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, false, true))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
  manager()->ShowManualFallbackForSaving(&driver_, incomplete_match);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
              FormMatches(incomplete_match));

  base::UserActionTester user_action_tester;

  // The user completes typing the credential. No fallback should be available,
  // because the credential is already in the store.
  EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, false, true)).Times(0);
  EXPECT_CALL(client_, HideManualFallbackForSaving());
  manager()->ShowManualFallbackForSaving(&driver_, form);

  // The user submits the form. No prompt should pop up. The credential is
  // updated in background.
  OnPasswordFormSubmitted(form);
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  EXPECT_CALL(*store_, UpdateLogin(_));
  observed.clear();
  manager()->DidNavigateMainFrame();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("PasswordManager_LoginPassed"));
}

// Tests that on Chrome sign-in form credentials are not saved.
TEST_F(PasswordManagerTest, DoNotSaveOnChromeSignInForm) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));

  PasswordForm form(MakeSimpleForm());
  form.is_gaia_with_skip_save_password_form = true;
  std::vector<PasswordForm> observed = {form};
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(*client_.GetStoreResultFilter(), ShouldSave(_))
      .WillRepeatedly(Return(false));
  // The user is typing a credential. No fallback should be available.
  PasswordForm typed_credentials(form);
  typed_credentials.password_value = ASCIIToUTF16("pw");
  EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, _, _)).Times(0);
  manager()->ShowManualFallbackForSaving(&driver_, form);

  // The user submits the form. No prompt should pop up.
  OnPasswordFormSubmitted(form);
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  observed.clear();
  manager()->DidNavigateMainFrame();
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

// Tests that a UKM metric "Login Passed" is sent when the submitted credentials
// are already in the store and OnPasswordFormsParsed is called multiple times.
TEST_F(PasswordManagerTest,
       SubmissionMetricsIsPassedWhenDontSaveAlreadySavedCredential) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));

  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeConsumer(form)));
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(4);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // The user submits the form.
  OnPasswordFormSubmitted(form);

  // Another call of OnPasswordFormsParsed happens. In production it happens
  // because of some DOM updates.
  manager()->OnPasswordFormsParsed(&driver_, observed);

  EXPECT_CALL(client_, HideManualFallbackForSaving());
  // The call to manual fallback with |form| equal to already saved should close
  // the fallback, but it should not prevent sending metrics.
  manager()->ShowManualFallbackForSaving(&driver_, form);
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  EXPECT_CALL(*store_, UpdateLogin(_));

  // Simulate successful login. Expect "Login Passed" metric.
  base::UserActionTester user_action_tester;
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("PasswordManager_LoginPassed"));
}

TEST_F(PasswordManagerTest, FormSeenThenLeftPage) {
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // No message from the renderer that a password was submitted. No
  // expected calls.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

TEST_F(PasswordManagerTest, FormSubmit) {
  // Test that a plain form submit results in offering to save passwords.
  PasswordForm form(MakeSimpleForm());
  std::vector<PasswordForm> observed = {form};
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  EXPECT_FALSE(manager()->IsPasswordFieldDetectedOnPage());
  manager()->OnPasswordFormsParsed(&driver_, observed);
  EXPECT_TRUE(manager()->IsPasswordFieldDetectedOnPage());
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate saving the form, as if the info bar was accepted.
  EXPECT_CALL(*store_, AddLogin(FormMatches(form)));
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();
}

TEST_F(PasswordManagerTest, FormSubmitWhenPasswordsCannotBeSaved) {
  // Test that a plain form submit doesn't result in offering to save passwords.
  EXPECT_CALL(*store_, IsAbleToSavePasswords()).WillOnce(Return(false));

  PasswordForm form(MakeSimpleForm());
  std::vector<PasswordForm> observed = {form};
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  EXPECT_FALSE(manager()->IsPasswordFieldDetectedOnPage());
  manager()->OnPasswordFormsParsed(&driver_, observed);
  EXPECT_TRUE(manager()->IsPasswordFieldDetectedOnPage());
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);

  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

// This test verifies a fix for http://crbug.com/236673
TEST_F(PasswordManagerTest, FormSubmitWithFormOnPreviousPage) {
  PasswordForm first_form(MakeSimpleForm());
  first_form.origin = GURL("http://www.nytimes.com/");
  first_form.action = GURL("https://myaccount.nytimes.com/auth/login");
  first_form.signon_realm = "http://www.nytimes.com/";
  PasswordForm second_form(MakeSimpleForm());
  second_form.origin = GURL("https://myaccount.nytimes.com/auth/login");
  second_form.action = GURL("https://myaccount.nytimes.com/auth/login");
  second_form.signon_realm = "https://myaccount.nytimes.com/";

  // Pretend that the form is hidden on the first page.
  std::vector<PasswordForm> observed;
  observed.push_back(first_form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Now navigate to a second page.
  manager()->DidNavigateMainFrame();

  // This page contains a form with the same markup, but on a different
  // URL.
  observed.push_back(second_form);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Now submit this form
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(second_form);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
  // Navigation after form submit, no forms appear.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate saving the form, as if the info bar was accepted and make sure
  // that the saved form matches the second form, not the first.
  EXPECT_CALL(*store_, AddLogin(FormMatches(second_form)));
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();
}

TEST_F(PasswordManagerTest, FormSubmitInvisibleLogin) {
  // Tests fix of http://crbug.com/28911: if the login form reappears on the
  // subsequent page, but is invisible, it shouldn't count as a failed login.
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form);

  // Expect info bar to appear:
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // The form reappears, but is not visible in the layout:
  manager()->OnPasswordFormsParsed(&driver_, observed);
  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate saving the form.
  EXPECT_CALL(*store_, AddLogin(FormMatches(form)));
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();
}

TEST_F(PasswordManagerTest, InitiallyInvisibleForm) {
  // Make sure an invisible login form still gets autofilled.
  PasswordForm form(MakeSimpleForm());
  std::vector<PasswordForm> observed;
  observed.push_back(form);
  EXPECT_CALL(driver_, FillPasswordForm(_));
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillOnce(WithArg<1>(InvokeConsumer(form)));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}


TEST_F(PasswordManagerTest, FillPasswordsOnDisabledManager) {
  // Test fix for http://crbug.com/158296: Passwords must be filled even if the
  // password manager is disabled.
  PasswordForm form(MakeSimpleForm());
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(false));
  std::vector<PasswordForm> observed;
  observed.push_back(form);
  EXPECT_CALL(driver_, FillPasswordForm(_));
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillOnce(WithArg<1>(InvokeConsumer(form)));
  manager()->OnPasswordFormsParsed(&driver_, observed);
}

TEST_F(PasswordManagerTest, PasswordFormReappearance) {
  // If the password form reappears after submit, PasswordManager should deduce
  // that the login failed and not offer saving.
  std::vector<PasswordForm> observed;
  PasswordForm login_form(MakeTwitterLoginForm());
  observed.push_back(login_form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(login_form);

  observed.clear();
  observed.push_back(MakeTwitterFailedLoginForm());
  // A PasswordForm appears, and is visible in the layout:
  // No expected calls to the PasswordStore...
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  EXPECT_CALL(client_, AutomaticPasswordSaveIndicator()).Times(0);
  EXPECT_CALL(*store_, AddLogin(_)).Times(0);
  EXPECT_CALL(*store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

TEST_F(PasswordManagerTest, SyncCredentialsNotSaved) {
  // Simulate loading a simple form with no existing stored password.
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleGAIAForm());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // User should not be prompted and password should not be saved.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  EXPECT_CALL(*store_, AddLogin(_)).Times(0);
#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  ON_CALL(*client_.GetStoreResultFilter(), ShouldSaveGaiaPasswordHash(_))
      .WillByDefault(Return(true));
  ON_CALL(*client_.GetStoreResultFilter(), IsSyncAccountEmail(_))
      .WillByDefault(Return(true));
  EXPECT_CALL(*store_,
              SaveGaiaPasswordHash(
                  "googleuser", form.password_value,
                  metrics_util::SyncPasswordHashChange::SAVED_IN_CONTENT_AREA));
#endif
  // Prefs are needed for failure logging about sync credentials.
  EXPECT_CALL(client_, GetPrefs()).WillRepeatedly(Return(nullptr));
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));

  client_.FilterAllResultsForSaving();

  OnPasswordFormSubmitted(form);
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
TEST_F(PasswordManagerTest, HashSavedOnGaiaFormWithSkipSavePassword) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));

  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleGAIAForm());
  // Simulate that this is Gaia form that should be ignored for saving/filling.
  form.is_gaia_with_skip_save_password_form = true;
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  ON_CALL(*client_.GetStoreResultFilter(), ShouldSaveGaiaPasswordHash(_))
      .WillByDefault(Return(true));
  ON_CALL(*client_.GetStoreResultFilter(), ShouldSave(_))
      .WillByDefault(Return(false));
  ON_CALL(*client_.GetStoreResultFilter(), IsSyncAccountEmail(_))
      .WillByDefault(Return(true));

  EXPECT_CALL(*store_,
              SaveGaiaPasswordHash(
                  "googleuser", form.password_value,
                  metrics_util::SyncPasswordHashChange::SAVED_IN_CONTENT_AREA));

  OnPasswordFormSubmitted(form);
  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}
#endif

// On a successful login with an updated password,
// CredentialsFilter::ReportFormLoginSuccess and CredentialsFilter::ShouldSave
// should be called. The argument of ShouldSave shold be the submitted form.
TEST_F(PasswordManagerTest, ReportFormLoginSuccessAndShouldSaveCalled) {
  PasswordForm stored_form(MakeSimpleForm());

  std::vector<PasswordForm> observed;
  PasswordForm observed_form = stored_form;
  // Different values of |username_element| needed to ensure that it is the
  // |observed_form| and not the |stored_form| what is passed to ShouldSave.
  observed_form.username_element += ASCIIToUTF16("1");
  observed.push_back(observed_form);
  // Simulate that |form| is already in the store, making this an update.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeConsumer(stored_form)));
  EXPECT_CALL(driver_, FillPasswordForm(_));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  EXPECT_CALL(driver_, FillPasswordForm(_));
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Submit form and finish navigation.
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(client_, GetPrefs()).WillRepeatedly(Return(nullptr));

  manager()->ProvisionallySavePassword(observed_form, nullptr);

  // Chrome should recognise the successful login and call
  // ReportFormLoginSuccess.
  EXPECT_CALL(*client_.GetStoreResultFilter(), ReportFormLoginSuccess(_));

  PasswordForm submitted_form = observed_form;
  submitted_form.preferred = true;
  EXPECT_CALL(*client_.GetStoreResultFilter(), ShouldSave(submitted_form));
  EXPECT_CALL(*store_, UpdateLogin(_));
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  // As the clone PasswordFormManager ends up saving the form, it triggers an
  // update of the pending login managers, which in turn triggers new filling.
  EXPECT_CALL(driver_, FillPasswordForm(_));
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

// When there is a sync password saved, and the user successfully uses the
// stored version of it, PasswordManager should not drop that password.
TEST_F(PasswordManagerTest, SyncCredentialsNotDroppedIfUpToDate) {
  PasswordForm form(MakeSimpleGAIAForm());
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeConsumer(form)));

  client_.FilterAllResultsForSaving();

  std::vector<PasswordForm> observed;
  observed.push_back(form);
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(2);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Submit form and finish navigation.
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(client_, GetPrefs()).WillRepeatedly(Return(nullptr));
#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  ON_CALL(*client_.GetStoreResultFilter(), ShouldSaveGaiaPasswordHash(_))
      .WillByDefault(Return(true));
  ON_CALL(*client_.GetStoreResultFilter(), IsSyncAccountEmail(_))
      .WillByDefault(Return(true));
  EXPECT_CALL(*store_,
              SaveGaiaPasswordHash(
                  "googleuser", form.password_value,
                  metrics_util::SyncPasswordHashChange::SAVED_IN_CONTENT_AREA));
#endif
  manager()->ProvisionallySavePassword(form, nullptr);

  // Chrome should not remove the sync credential, because it was successfully
  // used as stored, and therefore is up to date.
  EXPECT_CALL(*store_, RemoveLogin(_)).Times(0);
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

// While sync credentials are not saved, they are still filled to avoid users
// thinking they lost access to their accounts.
TEST_F(PasswordManagerTest, SyncCredentialsStillFilled) {
  PasswordForm form(MakeSimpleForm());
  // Pretend that the password store contains credentials stored for |form|.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeConsumer(form)));

  client_.FilterAllResultsForSaving();

  // Load the page.
  autofill::PasswordFormFillData form_data;
  EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&form_data));
  std::vector<PasswordForm> observed;
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  EXPECT_EQ(form.password_value, form_data.password_field.value);
}

// On failed login attempts, the retry-form can have action scheme changed from
// HTTP to HTTPS (see http://crbug.com/400769). Check that such retry-form is
// considered equal to the original login form, and the attempt recognised as a
// failure.
TEST_F(PasswordManagerTest,
       SeeingFormActionWithOnlyHttpHttpsChangeIsLoginFailure) {
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);

  PasswordForm first_form(MakeSimpleForm());
  first_form.origin = GURL("http://www.xda-developers.com/");
  first_form.action = GURL("http://forum.xda-developers.com/login.php");

  // |second_form|'s action differs only with it's scheme i.e. *https://*.
  PasswordForm second_form(first_form);
  second_form.action = GURL("https://forum.xda-developers.com/login.php");

  std::vector<PasswordForm> observed;
  observed.push_back(first_form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(first_form);

  // Simulate loading a page, which contains |second_form| instead of
  // |first_form|.
  observed.clear();
  observed.push_back(second_form);

  // Verify that no prompt to save the password is shown.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

TEST_F(PasswordManagerTest,
       ShouldBlockPasswordForSameOriginButDifferentSchemeTest) {
  constexpr struct {
    const char* old_origin;
    const char* new_origin;
    bool result;
  } kTestData[] = {
      // Same origin and same scheme.
      {"https://example.com/login", "https://example.com/login", false},
      // Same host and same scheme, different port.
      {"https://example.com:443/login", "https://example.com:444/login", false},
      // Same host but different scheme (https to http).
      {"https://example.com/login", "http://example.com/login", true},
      // Same host but different scheme (http to https).
      {"http://example.com/login", "https://example.com/login", false},
      // Different TLD, same schemes.
      {"https://example.com/login", "https://example.org/login", false},
      // Different TLD, different schemes.
      {"https://example.com/login", "http://example.org/login", false},
      // Different subdomains, same schemes.
      {"https://sub1.example.com/login", "https://sub2.example.org/login",
       false},
  };

  PasswordForm form = MakeSimpleForm();
  for (const auto& test_case : kTestData) {
    SCOPED_TRACE(testing::Message("#test_case = ") << (&test_case - kTestData));
    manager()->main_frame_url_ = GURL(test_case.old_origin);
    form.origin = GURL(test_case.new_origin);
    EXPECT_EQ(
        test_case.result,
        manager()->ShouldBlockPasswordForSameOriginButDifferentScheme(form));
  }
}

// Tests whether two submissions to the same origin but different schemes
// result in only saving the first submission, which has a secure scheme.
TEST_F(PasswordManagerTest, AttemptedSavePasswordSameOriginInsecureScheme) {
  PasswordForm secure_form(MakeSimpleForm());
  secure_form.origin = GURL("https://example.com/login");
  secure_form.action = GURL("https://example.com/login");
  secure_form.signon_realm = secure_form.origin.spec();

  PasswordForm insecure_form(MakeSimpleForm());
  insecure_form.username_element += ASCIIToUTF16("1");
  insecure_form.username_value = ASCIIToUTF16("compromised_user");
  insecure_form.password_value = ASCIIToUTF16("C0mpr0m1s3d_P4ss");
  insecure_form.origin = GURL("http://example.com/home");
  insecure_form.action = GURL("http://example.com/home");
  insecure_form.signon_realm = insecure_form.origin.spec();

  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));

  EXPECT_CALL(client_, GetMainFrameURL())
      .WillRepeatedly(ReturnRef(secure_form.origin));

  // Parse, render and submit the secure form.
  std::vector<PasswordForm> observed = {secure_form};
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
  OnPasswordFormSubmitted(secure_form);

  // Make sure |PromptUserToSaveOrUpdatePassword| gets called, and the resulting
  // form manager is saved.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  EXPECT_CALL(client_, GetMainFrameURL())
      .WillRepeatedly(ReturnRef(insecure_form.origin));

  // Parse, render and submit the insecure form.
  observed = {insecure_form};
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
  OnPasswordFormSubmitted(insecure_form);

  // Expect no further calls to |PromptUserToSaveOrUpdatePassword| due to
  // insecure origin.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);

  // Trigger call to |ProvisionalSavePassword| by rendering a page without
  // forms.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Make sure that the form saved by the user is indeed the secure form.
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
              FormMatches(secure_form));
}

// Create a form with both a new and current password element. Let the current
// password value be non-empty and the new password value be empty and submit
// the form. While normally saving the new password is preferred (on change
// password forms, that would be the reasonable choice), if the new password is
// empty, this is likely just a slightly misunderstood form, and Chrome should
// save the non-empty current password field.
TEST_F(PasswordManagerTest, DoNotSaveWithEmptyNewPasswordAndNonemptyPassword) {
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  ASSERT_FALSE(form.password_value.empty());
  form.new_password_element = ASCIIToUTF16("new_password_element");
  form.new_password_value.clear();
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillOnce(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // Now the password manager waits for the login to complete successfully.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_EQ(form.password_value,
            PasswordFormManager::PasswordToSave(
                form_manager_to_save->GetPendingCredentials())
                .first);
}

TEST_F(PasswordManagerTest, FormSubmitWithOnlyPasswordField) {
  // Test to verify that on submitting the HTML password form without having
  // username input filed shows password save promt and saves the password to
  // store.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
  std::vector<PasswordForm> observed;

  // Loads passsword form without username input field.
  PasswordForm form(MakeSimpleFormWithOnlyPasswordField());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillOnce(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->DidNavigateMainFrame();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate saving the form, as if the info bar was accepted.
  EXPECT_CALL(*store_, AddLogin(FormMatches(form)));
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();
}

// Test that if there are two "similar" forms in different frames, both get
// filled. This means slightly different things depending on whether the
// kNewPasswordFormParsing feature is enabled or not, so it is covered by two
// tests below.

// If kNewPasswordFormParsing is enabled, then "similar" is governed by
// NewPasswordFormManager::DoesManage, which in turn delegates to the unique
// renderer ID of the forms being the same. Note, however, that such ID is only
// unique within one renderer process. If different frames on the page are
// rendered by different processes, two unrelated forms can end up with the same
// ID. The test checks that nevertheless each of them gets assigned its own
// NewPasswordFormManager and filled as expected.
TEST_F(PasswordManagerTest, FillPasswordOnManyFrames_SameId) {
  // Setting task runner is required since NewPasswordFormManager uses
  // PostDelayTask for making filling.
  TestMockTimeTaskRunner::ScopedContext scoped_context_(task_runner_.get());

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kNewPasswordFormParsing);

  // Two unrelated forms...
  FormData form_data;
  form_data.origin = GURL("http://www.google.com/a/LoginAuth");
  form_data.action = GURL("http://www.google.com/a/Login");
  form_data.fields.resize(2);
  form_data.fields[0].name = ASCIIToUTF16("Email");
  form_data.fields[0].value = ASCIIToUTF16("googleuser");
  form_data.fields[0].unique_renderer_id = 1;
  form_data.fields[0].form_control_type = "text";
  form_data.fields[1].name = ASCIIToUTF16("Passwd");
  form_data.fields[1].value = ASCIIToUTF16("p4ssword");
  form_data.fields[1].unique_renderer_id = 2;
  form_data.fields[1].form_control_type = "password";
  PasswordForm first_form;
  first_form.form_data = form_data;

  form_data.origin = GURL("http://www.example.com/");
  form_data.action = GURL("http://www.example.com/");
  form_data.fields[0].name = ASCIIToUTF16("User");
  form_data.fields[0].value = ASCIIToUTF16("exampleuser");
  form_data.fields[0].unique_renderer_id = 3;
  form_data.fields[1].name = ASCIIToUTF16("Pwd");
  form_data.fields[1].value = ASCIIToUTF16("1234");
  form_data.fields[1].unique_renderer_id = 4;
  PasswordForm second_form;
  second_form.form_data = form_data;

  // Make the forms be "similar".
  first_form.form_data.unique_renderer_id =
      second_form.form_data.unique_renderer_id = 7654;

  // The following expectation covers the calls from the old
  // PasswordFormManager.
  EXPECT_CALL(*store_, GetLogins(PasswordStore::FormDigest(PasswordForm()), _))
      .Times(2);

  // Observe the form in the first frame.
  EXPECT_CALL(*store_,
              GetLogins(PasswordStore::FormDigest(first_form.form_data), _))
      .WillOnce(WithArg<1>(InvokeConsumer(first_form)));
  EXPECT_CALL(driver_, FillPasswordForm(_));
  manager()->OnPasswordFormsParsed(&driver_, {first_form});

  // Observe the form in the second frame.
  MockPasswordManagerDriver driver_b;
  EXPECT_CALL(*store_,
              GetLogins(PasswordStore::FormDigest(second_form.form_data), _))
      .WillOnce(WithArg<1>(InvokeConsumer(second_form)));
  EXPECT_CALL(driver_b, FillPasswordForm(_));
  manager()->OnPasswordFormsParsed(&driver_b, {second_form});
  task_runner_->FastForwardUntilNoTasksRemain();
}

// If kNewPasswordFormParsing is disabled, "similar" is governed by
// PasswordFormManager::DoesManage and is related to actual similarity of the
// forms, including having the same signon realm (and hence origin). Should a
// page have two frames with the same origin and a form, and those two forms be
// similar, then it is important to ensure that the single governing
// PasswordFormManager knows about both PasswordManagerDriver instances and
// instructs them to fill.
TEST_F(PasswordManagerTest, FillPasswordOnManyFrames_SameForm) {
  PasswordForm same_form = MakeSimpleForm();

  // Observe the form in the first frame.
  EXPECT_CALL(driver_, FillPasswordForm(_));
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillOnce(WithArg<1>(InvokeConsumer(same_form)));
  manager()->OnPasswordFormsParsed(&driver_, {same_form});

  // Now the form will be seen the second time, in a different frame. The driver
  // for that frame should be told to fill it, but the store should not be asked
  // for it again.
  MockPasswordManagerDriver driver_b;
  EXPECT_CALL(driver_b, FillPasswordForm(_));
  EXPECT_CALL(*store_, GetLogins(_, _)).Times(0);
  manager()->OnPasswordFormsParsed(&driver_b, {same_form});
}

TEST_F(PasswordManagerTest, SameDocumentNavigation) {
  // Test that observing a newly submitted form shows the save password bar on
  // call in page navigation.
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillOnce(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  manager()->OnPasswordFormSubmittedNoChecks(&driver_, form);
  ASSERT_TRUE(form_manager_to_save);

  // Simulate saving the form, as if the info bar was accepted.
  EXPECT_CALL(*store_, AddLogin(FormMatches(form)));
  // The Save() call triggers updating for |pending_login_managers_|, hence the
  // further GetLogins call.
  EXPECT_CALL(*store_, GetLogins(_, _));
  form_manager_to_save->Save();
}

TEST_F(PasswordManagerTest, SameDocumentBlacklistedSite) {
  // Test that observing a newly submitted form on blacklisted site does notify
  // the embedder on call in page navigation.
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  // Simulate that blacklisted form stored in store.
  PasswordForm blacklisted_form(form);
  blacklisted_form.username_value = ASCIIToUTF16("");
  blacklisted_form.blacklisted_by_user = true;
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillOnce(WithArg<1>(InvokeConsumer(blacklisted_form)));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  // Prefs are needed for failure logging about blacklisting.
  EXPECT_CALL(client_, GetPrefs()).WillRepeatedly(Return(nullptr));

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  manager()->OnPasswordFormSubmittedNoChecks(&driver_, form);
  EXPECT_TRUE(form_manager_to_save->IsBlacklisted());
}

TEST_F(PasswordManagerTest, SavingSignupForms_NoHTMLMatch) {
  // Signup forms don't require HTML attributes match in order to save.
  // Verify that we prefer a better match (action + origin vs. origin).
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  PasswordForm wrong_action_form(form);
  wrong_action_form.action = GURL("http://www.google.com/other/action");
  observed.push_back(wrong_action_form);

  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate either form changing or heuristics choosing other fields
  // after the user has entered their information.
  PasswordForm submitted_form(form);
  submitted_form.new_password_element = ASCIIToUTF16("new_password");
  submitted_form.new_password_value = form.password_value;
  submitted_form.password_element.clear();
  submitted_form.password_value.clear();

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(submitted_form);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate saving the form, as if the info bar was accepted.
  PasswordForm form_to_save;
  EXPECT_CALL(*store_, AddLogin(_)).WillOnce(SaveArg<0>(&form_to_save));
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();

  // PasswordManager observed two forms, and should have associate the saved one
  // with the observed form with a matching action.
  EXPECT_EQ(form.action, form_to_save.action);
  // Password values are always saved as the current password value.
  EXPECT_EQ(submitted_form.new_password_value, form_to_save.password_value);
  EXPECT_EQ(submitted_form.new_password_element, form_to_save.password_element);
}

TEST_F(PasswordManagerTest, SavingSignupForms_NoActionMatch) {
  // Signup forms don't require HTML attributes match in order to save.
  // Verify that we prefer a better match (HTML attributes + origin vs. origin).
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  // Change the submit element so we can track which of the two forms is
  // chosen as a better match.
  PasswordForm wrong_submit_form(form);
  wrong_submit_form.submit_element = ASCIIToUTF16("different_signin");
  wrong_submit_form.new_password_element = ASCIIToUTF16("new_password");
  wrong_submit_form.new_password_value = form.password_value;
  wrong_submit_form.password_element.clear();
  wrong_submit_form.password_value.clear();
  observed.push_back(wrong_submit_form);

  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  PasswordForm submitted_form(form);
  submitted_form.action = GURL("http://www.google.com/other/action");

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(submitted_form);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate saving the form, as if the info bar was accepted.
  PasswordForm form_to_save;
  EXPECT_CALL(*store_, AddLogin(_)).WillOnce(SaveArg<0>(&form_to_save));
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();

  // PasswordManager observed two forms, and should have associate the saved one
  // with the observed form with a matching action.
  EXPECT_EQ(form.submit_element, form_to_save.submit_element);

  EXPECT_EQ(submitted_form.password_value, form_to_save.password_value);
  EXPECT_EQ(submitted_form.password_element, form_to_save.password_element);
  EXPECT_EQ(submitted_form.username_value, form_to_save.username_value);
  EXPECT_EQ(submitted_form.username_element, form_to_save.username_element);
  EXPECT_TRUE(form_to_save.new_password_element.empty());
  EXPECT_TRUE(form_to_save.new_password_value.empty());
}

TEST_F(PasswordManagerTest, FormSubmittedChangedWithAutofillResponse) {
  // This tests verifies that if the observed forms and provisionally saved
  // differ in the choice of the username, the saving still succeeds, as long as
  // the changed form is marked "parsed using autofill predictions".
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate that based on autofill server username prediction, the username
  // of the form changed from the default candidate("Email") to something else.
  // Set the parsed_using_autofill_predictions bit to true to make sure that
  // choice of username is accepted by PasswordManager, otherwise the the form
  // will be rejected as not equal to the observed one. Note that during
  // initial parsing we don't have autofill server predictions yet, that's why
  // observed form and submitted form may be different.
  form.username_element = ASCIIToUTF16("Username");
  form.was_parsed_using_autofill_predictions = true;
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate saving the form, as if the info bar was accepted.
  EXPECT_CALL(*store_, AddLogin(FormMatches(form)));
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();
}

TEST_F(PasswordManagerTest, FormSubmittedUnchangedNotifiesClient) {
  // This tests verifies that if the observed forms and provisionally saved
  // forms are the same, then successful submission notifies the client.
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(2);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillOnce(WithArg<1>(InvokeConsumer(form)));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form);

  autofill::PasswordForm updated_form;
  autofill::PasswordForm notified_form;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  EXPECT_CALL(*store_, UpdateLogin(_)).WillOnce(SaveArg<0>(&updated_form));
  EXPECT_CALL(client_, NotifySuccessfulLoginWithExistingPassword(_))
      .WillOnce(SaveArg<0>(&notified_form));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->DidNavigateMainFrame();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_THAT(form, FormMatches(updated_form));
  EXPECT_THAT(form, FormMatches(notified_form));
}

TEST_F(PasswordManagerTest, SaveFormFetchedAfterSubmit) {
  // Test that a password is offered for saving even if the response from the
  // PasswordStore comes after submit.
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);

  // GetLogins calls remain unanswered to emulate that PasswordStore did not
  // fetch a form in time before submission.
  EXPECT_CALL(*store_, GetLogins(_, _));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
  ASSERT_EQ(1u, manager()->pending_login_managers().size());
  PasswordStoreConsumer* store_consumer = nullptr;

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  // This second call is from the new FormFetcher, which is cloned during
  // ProvisionalSavePasswords.
  EXPECT_CALL(*store_, GetLogins(_, _)).WillOnce(SaveArg<1>(&store_consumer));
  OnPasswordFormSubmitted(form);

  // Emulate fetching password form from PasswordStore after submission but
  // before post-navigation load.
  ASSERT_TRUE(store_consumer);
  store_consumer->OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>>());

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate saving the form, as if the info bar was accepted.
  EXPECT_CALL(*store_, AddLogin(FormMatches(form)));
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();
}

TEST_F(PasswordManagerTest, PasswordGeneration_FailedSubmission) {
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, AddLogin(_));
  manager()->OnPresaveGeneratedPassword(&driver_, form);

  // Do not save generated password when the password form reappears.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  EXPECT_CALL(*store_, AddLogin(_)).Times(0);
  EXPECT_CALL(client_, AutomaticPasswordSaveIndicator()).Times(0);

  // Simulate submission failing, with the same form being visible after
  // navigation.
  OnPasswordFormSubmitted(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

// If the user edits the generated password, but does not remove it completely,
// it should stay treated as a generated password.
TEST_F(PasswordManagerTest, PasswordGenerationPasswordEdited_FailedSubmission) {
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, AddLogin(_));
  manager()->OnPresaveGeneratedPassword(&driver_, form);

  // Simulate user editing and submitting a different password. Verify that
  // the edited password is the one that is saved.
  form.new_password_value = ASCIIToUTF16("different_password");
  OnPasswordFormSubmitted(form);

  // Do not save generated password when the password form reappears.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  EXPECT_CALL(*store_, AddLogin(_)).Times(0);
  EXPECT_CALL(client_, AutomaticPasswordSaveIndicator()).Times(0);

  // Simulate submission failing, with the same form being visible after
  // navigation.
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

// Generated password are saved even if it looks like the submit failed (the
// form reappeared). Verify that passwords which are no longer marked as
// generated will not be automatically saved.
TEST_F(PasswordManagerTest,
       PasswordGenerationNoLongerGeneratedPasswordNotForceSaved_FailedSubmit) {
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, AddLogin(_));
  manager()->OnPresaveGeneratedPassword(&driver_, form);

  // Simulate user removing generated password and adding a new one.
  form.new_password_value = ASCIIToUTF16("different_password");
  EXPECT_CALL(*store_, RemoveLogin(_));
  manager()->OnPasswordNoLongerGenerated(&driver_, form);

  OnPasswordFormSubmitted(form);

  // No infobar or prompt is shown if submission fails.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  EXPECT_CALL(client_, AutomaticPasswordSaveIndicator()).Times(0);

  // Simulate submission failing, with the same form being visible after
  // navigation.
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

// Verify that passwords which are no longer generated trigger the confirmation
// dialog when submitted.
TEST_F(PasswordManagerTest,
       PasswordGenerationNoLongerGeneratedPasswordNotForceSaved) {
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, AddLogin(_));
  manager()->OnPresaveGeneratedPassword(&driver_, form);

  // Simulate user removing generated password and adding a new one.
  form.new_password_value = ASCIIToUTF16("different_password");
  EXPECT_CALL(*store_, RemoveLogin(_));
  manager()->OnPasswordNoLongerGenerated(&driver_, form);

  OnPasswordFormSubmitted(form);

  // Verify that a normal prompt is shown instead of the force saving UI.
  std::unique_ptr<PasswordFormManagerForUI> form_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_to_save)));
  EXPECT_CALL(client_, AutomaticPasswordSaveIndicator()).Times(0);

  // Simulate a successful submission.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

TEST_F(PasswordManagerTest, PasswordGenerationUsernameChanged) {
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, AddLogin(_));
  manager()->OnPresaveGeneratedPassword(&driver_, form);

  // Simulate user changing the password and username, without ever completely
  // deleting the password.
  form.new_password_value = ASCIIToUTF16("different_password");
  form.username_value = ASCIIToUTF16("new_username");
  OnPasswordFormSubmitted(form);

  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  PasswordForm form_to_save;
  EXPECT_CALL(*store_, UpdateLoginWithPrimaryKey(_, _))
      .WillOnce(SaveArg<0>(&form_to_save));
  EXPECT_CALL(client_, AutomaticPasswordSaveIndicator());

  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
  EXPECT_EQ(form.username_value, form_to_save.username_value);
  // What was "new password" field in the submitted form, becomes the current
  // password field in the form to save.
  EXPECT_EQ(form.new_password_value, form_to_save.password_value);
}

TEST_F(PasswordManagerTest, PasswordGenerationPresavePassword) {
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  observed.push_back(form);
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  base::HistogramTester histogram_tester;

  // The user accepts a generated password.
  form.password_value = base::ASCIIToUTF16("password");
  PasswordForm sanitized_form(form);
  SanitizeFormData(&sanitized_form.form_data);

  EXPECT_CALL(*store_, AddLogin(sanitized_form)).WillOnce(Return());
  manager()->OnPresaveGeneratedPassword(&driver_, form);

  // The user updates the generated password.
  PasswordForm updated_form(form);
  updated_form.password_value = base::ASCIIToUTF16("password_12345");
  PasswordForm sanitized_updated_form(updated_form);
  SanitizeFormData(&sanitized_updated_form.form_data);
  EXPECT_CALL(*store_,
              UpdateLoginWithPrimaryKey(sanitized_updated_form, sanitized_form))
      .WillOnce(Return());
  manager()->OnPresaveGeneratedPassword(&driver_, updated_form);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.GeneratedFormHasNoFormManager", false, 2);

  // The user removes the generated password.
  EXPECT_CALL(*store_, RemoveLogin(sanitized_updated_form)).WillOnce(Return());
  manager()->OnPasswordNoLongerGenerated(&driver_, updated_form);
}

TEST_F(PasswordManagerTest, PasswordGenerationPresavePassword_NoFormManager) {
  // Checks that GeneratedFormHasNoFormManager metric is sent if there is no
  // corresponding PasswordFormManager for the given form. It should be uncommon
  // case.
  std::vector<PasswordForm> observed;
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  base::HistogramTester histogram_tester;

  // The user accepts a generated password.
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  form.password_value = base::ASCIIToUTF16("password");
  EXPECT_CALL(*store_, AddLogin(_)).Times(0);

  manager()->OnPresaveGeneratedPassword(&driver_, form);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.GeneratedFormHasNoFormManager", true, 1);
}

TEST_F(PasswordManagerTest, PasswordGenerationPresavePasswordAndLogin) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  const bool kFalseTrue[] = {false, true};
  for (bool found_matched_logins_in_store : kFalseTrue) {
    SCOPED_TRACE(testing::Message("found_matched_logins_in_store = ")
                 << found_matched_logins_in_store);
    PasswordForm form(MakeFormWithOnlyNewPasswordField());
    SanitizeFormData(&form.form_data);
    std::vector<PasswordForm> observed = {form};
    if (found_matched_logins_in_store) {
      EXPECT_CALL(*store_, GetLogins(_, _))
          .WillRepeatedly(WithArg<1>(InvokeConsumer(form)));
      EXPECT_CALL(*store_, GetLoginsForSameOrganizationName(_, _));
      EXPECT_CALL(driver_, FillPasswordForm(_)).Times(2);
    } else {
      EXPECT_CALL(*store_, GetLogins(_, _))
          .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
      EXPECT_CALL(*store_, GetLoginsForSameOrganizationName(_, _));
    }
    std::unique_ptr<PasswordFormManagerForUI> form_manager;
    if (found_matched_logins_in_store) {
      EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
          .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager)));
    } else {
      EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
    }
    EXPECT_CALL(client_, AutomaticPasswordSaveIndicator())
        .Times(found_matched_logins_in_store ? 0 : 1);
    manager()->OnPasswordFormsParsed(&driver_, observed);
    manager()->OnPasswordFormsRendered(&driver_, observed, true);

    // The user accepts generated password and makes successful login.
    PasswordForm presaved_form(form);
    if (found_matched_logins_in_store)
      presaved_form.username_value.clear();
    EXPECT_CALL(*store_, AddLogin(presaved_form)).WillOnce(Return());
    manager()->OnPresaveGeneratedPassword(&driver_, form);
    ::testing::Mock::VerifyAndClearExpectations(store_.get());

    EXPECT_CALL(*store_, IsAbleToSavePasswords()).WillRepeatedly(Return(true));
    if (!found_matched_logins_in_store)
      EXPECT_CALL(*store_, UpdateLoginWithPrimaryKey(_, presaved_form));
    OnPasswordFormSubmitted(form);
    observed.clear();
    manager()->DidNavigateMainFrame();
    manager()->OnPasswordFormsParsed(&driver_, observed);
    manager()->OnPasswordFormsRendered(&driver_, observed, true);

    ::testing::Mock::VerifyAndClearExpectations(store_.get());
    EXPECT_CALL(*store_, IsAbleToSavePasswords()).WillRepeatedly(Return(true));
    if (found_matched_logins_in_store) {
      // Credentials should be updated only when the user explicitly chooses.
      ASSERT_TRUE(form_manager);
      EXPECT_CALL(*store_, UpdateLoginWithPrimaryKey(_, presaved_form));
      form_manager->Update(form_manager->GetPendingCredentials());
      ::testing::Mock::VerifyAndClearExpectations(store_.get());
    }
  }
}

TEST_F(PasswordManagerTest,
       PasswordGenerationNoCorrespondingPasswordFormManager) {
  // Verifies that if there is no corresponding password form manager for the
  // given form, new password form manager should fetch data from the password
  // store. Also verifies that |SetGenerationElementAndReasonForForm| doesn't
  // change |has_generated_password_| of new password form manager.
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  std::vector<PasswordForm> observed;
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(*store_, GetLogins(PasswordStore::FormDigest(form), _));
  manager()->SetGenerationElementAndReasonForForm(&driver_, form,
                                                  base::string16(), false);
  ASSERT_EQ(1u, manager()->pending_login_managers().size());
  PasswordFormManager* form_manager =
      manager()->pending_login_managers().front().get();

  EXPECT_FALSE(form_manager->HasGeneratedPassword());
}

TEST_F(PasswordManagerTest, UpdateFormManagers) {
  // Seeing some forms should result in creating PasswordFormManagers and
  // querying PasswordStore. Calling UpdateFormManagers should result in
  // querying the store again.
  EXPECT_CALL(*store_, GetLogins(_, _));

  PasswordForm form;
  std::vector<PasswordForm> observed;
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  ASSERT_EQ(1u, manager()->pending_login_managers().size());
  PasswordFormManager* form_manager =
      manager()->pending_login_managers().front().get();

  // The first GetLogins should have fired, but to unblock the second, we need
  // to first send a response from the store (to be ignored).
  static_cast<FormFetcherImpl*>(form_manager->GetFormFetcher())
      ->OnGetPasswordStoreResults(std::vector<std::unique_ptr<PasswordForm>>());
  EXPECT_CALL(*store_, GetLogins(_, _));
  manager()->UpdateFormManagers();
}

TEST_F(PasswordManagerTest, DropFormManagers) {
  // Interrupt the normal submit flow by DropFormManagers().
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  manager()->DropFormManagers();
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form);

  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

TEST_F(PasswordManagerTest, AutofillingOfAffiliatedCredentials) {
  PasswordForm android_form(MakeAndroidCredential());
  PasswordForm observed_form(MakeSimpleForm());
  std::vector<PasswordForm> observed_forms;
  observed_forms.push_back(observed_form);

  autofill::PasswordFormFillData form_data;
  EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&form_data));
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillOnce(WithArg<1>(InvokeConsumer(android_form)));
  manager()->OnPasswordFormsParsed(&driver_, observed_forms);
  observed_forms.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed_forms, true);

  EXPECT_EQ(android_form.username_value, form_data.username_field.value);
  EXPECT_EQ(android_form.password_value, form_data.password_field.value);
  EXPECT_FALSE(form_data.wait_for_username);
  EXPECT_EQ(android_form.signon_realm, form_data.preferred_realm);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));

  PasswordForm filled_form(observed_form);
  filled_form.username_value = android_form.username_value;
  filled_form.password_value = android_form.password_value;
  OnPasswordFormSubmitted(filled_form);

  PasswordForm saved_form;
  PasswordForm saved_notified_form;
  EXPECT_CALL(*store_, UpdateLogin(_)).WillOnce(SaveArg<0>(&saved_form));
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  EXPECT_CALL(client_, NotifySuccessfulLoginWithExistingPassword(_))
      .WillOnce(SaveArg<0>(&saved_notified_form));
  EXPECT_CALL(*store_, AddLogin(_)).Times(0);
  EXPECT_CALL(*store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);

  observed_forms.clear();
  manager()->DidNavigateMainFrame();
  manager()->OnPasswordFormsParsed(&driver_, observed_forms);
  manager()->OnPasswordFormsRendered(&driver_, observed_forms, true);
  EXPECT_THAT(saved_form, FormMatches(android_form));
  EXPECT_THAT(saved_form, FormMatches(saved_notified_form));
}

// If the manager fills a credential originally saved from an affiliated Android
// application, and the user overwrites the password, they should be prompted if
// they want to update.  If so, the Android credential itself should be updated.
TEST_F(PasswordManagerTest, UpdatePasswordOfAffiliatedCredential) {
  PasswordForm android_form(MakeAndroidCredential());
  PasswordForm observed_form(MakeSimpleForm());
  std::vector<PasswordForm> observed_forms = {observed_form};

  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(2);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillOnce(WithArg<1>(InvokeConsumer(android_form)));
  manager()->OnPasswordFormsParsed(&driver_, observed_forms);
  manager()->OnPasswordFormsRendered(&driver_, observed_forms, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));

  PasswordForm filled_form(observed_form);
  filled_form.username_value = android_form.username_value;
  filled_form.password_value = ASCIIToUTF16("new_password");
  OnPasswordFormSubmitted(filled_form);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  observed_forms.clear();
  manager()->DidNavigateMainFrame();
  manager()->OnPasswordFormsParsed(&driver_, observed_forms);
  manager()->OnPasswordFormsRendered(&driver_, observed_forms, true);

  PasswordForm saved_form;
  EXPECT_CALL(*store_, AddLogin(_)).Times(0);
  EXPECT_CALL(*store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*store_, UpdateLogin(_)).WillOnce(SaveArg<0>(&saved_form));
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();

  PasswordForm expected_form(android_form);
  expected_form.password_value = filled_form.password_value;
  EXPECT_THAT(saved_form, FormMatches(expected_form));
}

TEST_F(PasswordManagerTest, ClearedFieldsSuccessCriteria) {
  // Test that a submission is considered to be successful on a change password
  // form without username when fields valued are cleared.
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  form.username_element.clear();
  form.username_value.clear();
  std::vector<PasswordForm> observed = {form};

  // Emulate page load.
  EXPECT_CALL(*store_, GetLogins(PasswordStore::FormDigest(form), _));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
  ASSERT_EQ(1u, manager()->pending_login_managers().size());
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));

  // Returning result from the store.
  PasswordFormManager* form_manager =
      manager()->pending_login_managers().front().get();
  ASSERT_TRUE(form_manager);
  static_cast<FormFetcherImpl*>(form_manager->GetFormFetcher())
      ->OnGetPasswordStoreResults(std::vector<std::unique_ptr<PasswordForm>>());

  OnPasswordFormSubmitted(form);

  // JavaScript cleared field values.
  observed[0].password_value.clear();
  observed[0].new_password_value.clear();

  // Check success of the submission.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
// Check that no sync password hash is saved when no username is available,
// because we it's not clear whether the submitted credentials are sync
// credentials.
TEST_F(PasswordManagerTest, NotSavingSyncPasswordHash_NoUsername) {
  // Simulate loading a simple form with no existing stored password.
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleGAIAForm());
  // Simulate that no username is found.
  form.username_value.clear();
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, GetPrefs()).WillRepeatedly(Return(nullptr));
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));

  // Simulate that this credentials which is similar to be sync credentials.
  client_.FilterAllResultsForSaving();

  // Check that no Gaia credential password hash is saved.
  EXPECT_CALL(*store_, SaveGaiaPasswordHash(_, _, _)).Times(0);
  OnPasswordFormSubmitted(form);
  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

// Check that no sync password hash is saved when the submitted credentials are
// not qualified as sync credentials.
TEST_F(PasswordManagerTest, NotSavingSyncPasswordHash_NotSyncCredentials) {
  // Simulate loading a simple form with no existing stored password.
  PasswordForm form(MakeSimpleGAIAForm());
  std::vector<PasswordForm> observed = {form};
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, GetPrefs()).WillRepeatedly(Return(nullptr));
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));

  // Check that no Gaia credential password hash is saved since these
  // credentials are eligible for saving.
  EXPECT_CALL(*store_, SaveGaiaPasswordHash(_, _, _)).Times(0);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  OnPasswordFormSubmitted(form);
  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}
#endif

TEST_F(PasswordManagerTest, ManualFallbackForSaving) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));

  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  PasswordForm stored_form = form;
  stored_form.password_value = ASCIIToUTF16("old_password");
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillOnce(WithArg<1>(InvokeConsumer(stored_form)));
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(2);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // The username of the stored form is the same, there should be update bubble.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, false, true))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
  manager()->ShowManualFallbackForSaving(&driver_, form);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(), FormMatches(form));

  // The username of the stored form is different, there should be save bubble.
  PasswordForm new_form = form;
  new_form.username_value = ASCIIToUTF16("another_username");
  EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, false, false))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
  manager()->ShowManualFallbackForSaving(&driver_, new_form);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
              FormMatches(new_form));

  // Hide the manual fallback.
  EXPECT_CALL(client_, HideManualFallbackForSaving());
  manager()->HideManualFallbackForSaving();

  // Two PasswordFormManagers instances hold references to a shared
  // PasswordFormMetrics recorder. These need to be freed to flush the metrics
  // into the test_ukm_recorder.
  manager_.reset();
  form_manager_to_save.reset();

  // Verify that the last state is recorded.
  std::vector<const ukm::mojom::UkmEntry*> ukm_entries =
      test_ukm_recorder.GetEntriesByName(
          ukm::builders::PasswordForm::kEntryName);
  ASSERT_EQ(1u, ukm_entries.size());
  test_ukm_recorder.ExpectEntryMetric(
      ukm_entries[0],
      ukm::builders::PasswordForm::kSaving_ShowedManualFallbackForSavingName,
      1);
}

// Tests that the manual fallback for saving isn't shown if there is no response
// from the password storage. When crbug.com/741537 is fixed, change this test.
TEST_F(PasswordManagerTest, ManualFallbackForSaving_SlowBackend) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));

  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  PasswordStoreConsumer* store_consumer = nullptr;
  EXPECT_CALL(*store_, GetLogins(_, _)).WillOnce(SaveArg<1>(&store_consumer));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // There is no response from the store. Don't show the fallback.
  EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, _, _)).Times(0);
  manager()->ShowManualFallbackForSaving(&driver_, form);

  // The storage responded. The fallback can be shown.
  ASSERT_TRUE(store_consumer);
  store_consumer->OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>>());
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, false, false))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
  manager()->ShowManualFallbackForSaving(&driver_, form);
}

TEST_F(PasswordManagerTest, ManualFallbackForSaving_GeneratedPassword) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));

  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillOnce(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // A user accepts a password generated by Chrome. It triggers password
  // presaving and showing manual fallback.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(*store_, AddLogin(_));
  EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, true, false))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
  manager()->OnPresaveGeneratedPassword(&driver_, form);
  manager()->ShowManualFallbackForSaving(&driver_, form);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(), FormMatches(form));

  // A user edits the generated password. And again it causes password presaving
  // and showing manual fallback.
  EXPECT_CALL(*store_, UpdateLoginWithPrimaryKey(_, _));
  EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, true, false))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
  manager()->OnPresaveGeneratedPassword(&driver_, form);
  manager()->ShowManualFallbackForSaving(&driver_, form);

  // A user removes the generated password. The presaved password is removed,
  // the fallback is disabled.
  EXPECT_CALL(*store_, RemoveLogin(_));
  EXPECT_CALL(client_, HideManualFallbackForSaving());
  manager()->OnPasswordNoLongerGenerated(&driver_, form);
  manager()->HideManualFallbackForSaving();
}

// Tests that Autofill predictions are processed correctly. If at least one of
// these predictions can be converted to a |PasswordFormFieldPredictionMap|, the
// predictions map is updated accordingly.
TEST_F(PasswordManagerTest, ProcessAutofillPredictions) {
  // Create FormData form with two fields.
  autofill::FormData form;
  form.origin = GURL("http://foo.com");
  autofill::FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("password");
  form.fields.push_back(field);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  autofill::AutofillQueryResponseContents response;
  // If there are multiple predictions for the field,
  // |AutofillField::overall_server_type_| will store only autofill vote, but
  // not password vote. |AutofillField::server_predictions_| should store all
  // predictions.
  autofill::AutofillQueryResponseContents_Field* field0 = response.add_field();
  field0->set_overall_type_prediction(autofill::PHONE_HOME_NUMBER);
  autofill::AutofillQueryResponseContents_Field_FieldPrediction*
      field_prediction0 = field0->add_predictions();
  field_prediction0->set_type(autofill::PHONE_HOME_NUMBER);
  autofill::AutofillQueryResponseContents_Field_FieldPrediction*
      field_prediction1 = field0->add_predictions();
  field_prediction1->set_type(autofill::USERNAME);

  autofill::AutofillQueryResponseContents_Field* field1 = response.add_field();
  field1->set_overall_type_prediction(autofill::PASSWORD);
  autofill::AutofillQueryResponseContents_Field_FieldPrediction*
      field_prediction2 = field1->add_predictions();
  field_prediction2->set_type(autofill::PASSWORD);
  autofill::AutofillQueryResponseContents_Field_FieldPrediction*
      field_prediction3 = field1->add_predictions();
  field_prediction3->set_type(autofill::PROBABLY_NEW_PASSWORD);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));
  FormStructure::ParseQueryResponse(response_string, forms, nullptr);

  // Check that Autofill predictions are converted to password related
  // predictions.
  std::map<autofill::FormData, autofill::PasswordFormFieldPredictionMap>
      predictions;
  predictions[form][form.fields[0]] = autofill::PREDICTION_USERNAME;
  predictions[form][form.fields[1]] = autofill::PREDICTION_CURRENT_PASSWORD;
  EXPECT_CALL(driver_, AutofillDataReceived(predictions));

  manager()->ProcessAutofillPredictions(&driver_, forms);
}

// Let the PasswordManager see no password forms. As a default, it should
// suggest the last commited navigation entry to check for being enabled.
TEST_F(PasswordManagerTest, EntryToCheck_Default) {
  EXPECT_EQ(PasswordManager::NavigationEntryToCheck::LAST_COMMITTED,
            manager()->entry_to_check());
  manager()->OnPasswordFormsParsed(nullptr, std::vector<PasswordForm>());
  EXPECT_EQ(PasswordManager::NavigationEntryToCheck::LAST_COMMITTED,
            manager()->entry_to_check());
}

// If the PasswordManager sees HTML password forms, it should suggest the last
// commited navigation entry to check for being enabled.
TEST_F(PasswordManagerTest, EntryToCheck_HTML) {
  PasswordForm html_form;
  html_form.scheme = PasswordForm::SCHEME_HTML;
  html_form.origin = GURL("http://accounts.google.com/");
  html_form.signon_realm = "http://accounts.google.com/";
  EXPECT_CALL(*store_, GetLogins(_, _));
  manager()->OnPasswordFormsParsed(nullptr, {html_form});
  EXPECT_EQ(PasswordManager::NavigationEntryToCheck::LAST_COMMITTED,
            manager()->entry_to_check());
}

// If the PasswordManager sees HTTP auth password forms, it should suggest the
// visible navigation entry to check for being enabled.
TEST_F(PasswordManagerTest, EntryToCheck_HTTP_auth) {
  PasswordForm http_auth_form;
  http_auth_form.scheme = PasswordForm::SCHEME_BASIC;
  http_auth_form.origin = GURL("http://accounts.google.com/");
  http_auth_form.signon_realm = "http://accounts.google.com/";
  EXPECT_CALL(*store_, GetLogins(_, _));
  manager()->OnPasswordFormsParsed(nullptr, {http_auth_form});
  EXPECT_EQ(PasswordManager::NavigationEntryToCheck::VISIBLE,
            manager()->entry_to_check());
}

// Sync password hash should be updated upon submission of change password page.
TEST_F(PasswordManagerTest, SaveSyncPasswordHashOnChangePasswordPage) {
  PasswordForm form(MakeGAIAChangePasswordForm());
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));

  std::vector<PasswordForm> observed;
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Submit form and finish navigation.
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(client_, GetPrefs()).WillRepeatedly(Return(nullptr));
#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  ON_CALL(*client_.GetStoreResultFilter(), ShouldSaveGaiaPasswordHash(_))
      .WillByDefault(Return(true));
  ON_CALL(*client_.GetStoreResultFilter(), IsSyncAccountEmail(_))
      .WillByDefault(Return(true));
  EXPECT_CALL(
      *store_,
      SaveGaiaPasswordHash(
          "googleuser", form.new_password_value,
          metrics_util::SyncPasswordHashChange::CHANGED_IN_CONTENT_AREA));
#endif
  client_.FilterAllResultsForSaving();
  OnPasswordFormSubmitted(form);

  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
// Non-Sync Gaia password hash should be saved upon submission of Gaia login
// page.
TEST_F(PasswordManagerTest, SaveOtherGaiaPasswordHash) {
  PasswordForm form(MakeSimpleGAIAForm());
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));

  std::vector<PasswordForm> observed;
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
  // Submit form and finish navigation.
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(client_, GetPrefs()).WillRepeatedly(Return(nullptr));

  ON_CALL(*client_.GetStoreResultFilter(), ShouldSaveGaiaPasswordHash(_))
      .WillByDefault(Return(true));
  EXPECT_CALL(
      *store_,
      SaveGaiaPasswordHash(
          "googleuser", form.password_value,
          metrics_util::SyncPasswordHashChange::NOT_SYNC_PASSWORD_CHANGE));

  client_.FilterAllResultsForSaving();
  OnPasswordFormSubmitted(form);

  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

// Enterprise password hash should be saved upon submission of enterprise login
// page.
TEST_F(PasswordManagerTest, SaveEnterprisePasswordHash) {
  PasswordForm form(MakeSimpleForm());
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));

  std::vector<PasswordForm> observed;
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Submit form and finish navigation.
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(client_, GetPrefs()).WillRepeatedly(Return(nullptr));
  ON_CALL(*client_.GetStoreResultFilter(), ShouldSaveEnterprisePasswordHash(_))
      .WillByDefault(Return(true));
  ON_CALL(*client_.GetStoreResultFilter(), IsSyncAccountEmail(_))
      .WillByDefault(Return(false));
  EXPECT_CALL(*store_,
              SaveEnterprisePasswordHash("googleuser", form.password_value));
  client_.FilterAllResultsForSaving();
  OnPasswordFormSubmitted(form);

  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}
#endif

// If there are no forms to parse, certificate errors should not be reported.
TEST_F(PasswordManagerTest, CertErrorReported_NoForms) {
  const std::vector<PasswordForm> observed;
  EXPECT_CALL(client_, GetMainFrameCertStatus())
      .WillRepeatedly(Return(net::CERT_STATUS_AUTHORITY_INVALID));
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));

  base::HistogramTester histogram_tester;
  manager()->OnPasswordFormsParsed(&driver_, observed);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.CertificateErrorsWhileSeeingForms", 0);
}

TEST_F(PasswordManagerTest, CertErrorReported) {
  constexpr struct {
    net::CertStatus cert_status;
    metrics_util::CertificateError expected_error;
  } kCases[] = {
      {0, metrics_util::CertificateError::NONE},
      {net::CERT_STATUS_SHA1_SIGNATURE_PRESENT,  // not an error
       metrics_util::CertificateError::NONE},
      {net::CERT_STATUS_COMMON_NAME_INVALID,
       metrics_util::CertificateError::COMMON_NAME_INVALID},
      {net::CERT_STATUS_WEAK_SIGNATURE_ALGORITHM,
       metrics_util::CertificateError::WEAK_SIGNATURE_ALGORITHM},
      {net::CERT_STATUS_DATE_INVALID,
       metrics_util::CertificateError::DATE_INVALID},
      {net::CERT_STATUS_AUTHORITY_INVALID,
       metrics_util::CertificateError::AUTHORITY_INVALID},
      {net::CERT_STATUS_WEAK_KEY, metrics_util::CertificateError::OTHER},
      {net::CERT_STATUS_DATE_INVALID | net::CERT_STATUS_WEAK_KEY,
       metrics_util::CertificateError::DATE_INVALID},
      {net::CERT_STATUS_DATE_INVALID | net::CERT_STATUS_AUTHORITY_INVALID,
       metrics_util::CertificateError::AUTHORITY_INVALID},
      {net::CERT_STATUS_DATE_INVALID | net::CERT_STATUS_AUTHORITY_INVALID |
           net::CERT_STATUS_WEAK_KEY,
       metrics_util::CertificateError::AUTHORITY_INVALID},
  };

  const std::vector<PasswordForm> observed = {PasswordForm()};
  // PasswordStore requested only once for the same form.
  EXPECT_CALL(*store_, GetLogins(_, _));

  for (const auto& test_case : kCases) {
    SCOPED_TRACE(testing::Message("index of test_case = ")
                 << (&test_case - kCases));
    EXPECT_CALL(client_, GetMainFrameCertStatus())
        .WillRepeatedly(Return(test_case.cert_status));
    base::HistogramTester histogram_tester;
    manager()->OnPasswordFormsParsed(&driver_, observed);
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.CertificateErrorsWhileSeeingForms",
        test_case.expected_error, 1);
  }
}

TEST_F(PasswordManagerTest, CreatingFormManagers) {
  // Add the NewPasswordFormParsing feature.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kNewPasswordFormParsing);

  PasswordForm form(MakeSimpleForm());
  std::vector<PasswordForm> observed;
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  // Check that the form manager is created.
  EXPECT_EQ(1u, manager()->form_managers().size());
  EXPECT_TRUE(manager()->form_managers()[0]->DoesManage(form.form_data,
                                                        client_.GetDriver()));

  // Check that receiving the same form the second time does not lead to
  // creating new form manager.
  manager()->OnPasswordFormsParsed(&driver_, observed);
  EXPECT_EQ(1u, manager()->form_managers().size());
}

TEST_F(PasswordManagerTest,
       ShowManualFallbacksDontChangeProvisionalSaveManager) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));

  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeConsumer(form)));
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(2);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_THAT(manager()->provisional_save_manager(), IsNull());
  manager()->ShowManualFallbackForSaving(&driver_, form);
  EXPECT_THAT(manager()->provisional_save_manager(), IsNull());

  // The user submits the form and a provisional save manager is set.
  OnPasswordFormSubmitted(form);

  EXPECT_THAT(manager()->provisional_save_manager(), NotNull());
  const PasswordFormManager* last_provisional_save_manager =
      manager()->provisional_save_manager();

  EXPECT_CALL(client_, HideManualFallbackForSaving());
  // The call to manual fallback with |form| equal to already saved should close
  // the fallback.
  manager()->ShowManualFallbackForSaving(&driver_, form);

  EXPECT_THAT(manager()->provisional_save_manager(), NotNull());
  EXPECT_EQ(last_provisional_save_manager,
            manager()->provisional_save_manager());
}

// Tests that processing normal HTML form submissions works properly with the
// new parsing. For details see scheme 1 in comments before
// |form_managers_| in password_manager.h.
TEST_F(PasswordManagerTest, ProcessingNormalFormSubmission) {
  for (bool skip_old_form_managers_in_tests : {false, true}) {
    for (bool successful_submission : {false, true}) {
      SCOPED_TRACE(testing::Message("skip_old_form_managers_in_tests = ")
                   << skip_old_form_managers_in_tests
                   << "  successful_submission = " << successful_submission);
      base::test::ScopedFeatureList scoped_feature_list;
      TurnOnNewParsingForSaving(&scoped_feature_list);
      manager()->set_skip_old_form_managers_in_tests(
          skip_old_form_managers_in_tests);

      EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
          .WillRepeatedly(Return(true));

      PasswordForm form(MakeSimpleForm());
      EXPECT_CALL(*store_, GetLogins(_, _))
          .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));

      std::vector<PasswordForm> observed;
      observed.push_back(form);
      manager()->OnPasswordFormsParsed(&driver_, observed);
      manager()->OnPasswordFormsRendered(&driver_, observed, true);

      auto submitted_form = form;
      submitted_form.form_data.fields[0].value = ASCIIToUTF16("username");
      submitted_form.form_data.fields[1].value =
          ASCIIToUTF16("strong_password");

      OnPasswordFormSubmitted(submitted_form);
      EXPECT_TRUE(manager()->GetSubmittedManagerForTest());

      std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;

      // Simulate submission.
      if (successful_submission) {
        EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
            .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
        // The form disappeared, so the submission is condered to be successful.
        observed.clear();
      } else {
        EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
      }
      manager()->OnPasswordFormsRendered(&driver_, observed, true);

      // Multiple calls of OnPasswordFormsRendered should be handled gracefully.
      manager()->OnPasswordFormsRendered(&driver_, observed, true);
    }
  }
}

// Tests that processing form submissions without navigations works properly
// with the new parsing. For details see scheme 2 in comments before
// |form_managers_| in password_manager.h.
TEST_F(PasswordManagerTest, ProcessingOtherSubmissionTypes) {
  for (bool skip_old_form_managers_in_tests : {false, true}) {
    SCOPED_TRACE(testing::Message("skip_old_form_managers_in_tests = ")
                 << skip_old_form_managers_in_tests);
    base::test::ScopedFeatureList scoped_feature_list;
    TurnOnNewParsingForSaving(&scoped_feature_list);

    EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
        .WillRepeatedly(Return(true));

    PasswordForm form(MakeSimpleForm());
    EXPECT_CALL(*store_, GetLogins(_, _))
        .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));

    std::vector<PasswordForm> observed;
    observed.push_back(form);
    manager()->OnPasswordFormsParsed(&driver_, observed);
    manager()->OnPasswordFormsRendered(&driver_, observed, true);

    auto submitted_form = form;
    submitted_form.form_data.fields[0].value = ASCIIToUTF16("username");
    submitted_form.form_data.fields[1].value = ASCIIToUTF16("strong_password");

    std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
    EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
        .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
    manager()->OnPasswordFormSubmittedNoChecks(&driver_, submitted_form);
    EXPECT_TRUE(manager()->form_managers().empty());
  }
}

TEST_F(PasswordManagerTest, SubmittedGaiaFormWithoutVisiblePasswordField) {
  // Tests that a submitted GAIA sign-in form which does not contain a visible
  // password field is skipped.
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleGAIAForm());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));

  form.username_value = ASCIIToUTF16("username");
  form.password_value = ASCIIToUTF16("password");
  form.form_data.fields[1].is_focusable = false;

  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  manager()->OnPasswordFormSubmittedNoChecks(&driver_, form);
}

// Tests that PasswordFormManager and NewPasswordFormManager for the same form
// have the same metrics recorder.
TEST_F(PasswordManagerTest, CheckMetricsRecorder) {
  base::test::ScopedFeatureList scoped_feature_list;
  TurnOnNewParsingForSaving(&scoped_feature_list);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));

  PasswordForm form(MakeSimpleForm());
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));

  std::vector<PasswordForm> observed;
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);

  const std::vector<std::unique_ptr<PasswordFormManager>>&
      password_form_managers = manager()->pending_login_managers();

  const std::vector<std::unique_ptr<NewPasswordFormManager>>&
      new_password_form_managers = manager()->form_managers();

  ASSERT_EQ(1u, password_form_managers.size());
  ASSERT_EQ(1u, new_password_form_managers.size());

  EXPECT_TRUE(password_form_managers[0]->GetMetricsRecorder());
  EXPECT_EQ(password_form_managers[0]->GetMetricsRecorder(),
            new_password_form_managers[0]->GetMetricsRecorder());
}

TEST_F(PasswordManagerTest, MetricForSchemeOfSuccessfulLogins) {
  for (bool origin_is_secure : {false, true}) {
    SCOPED_TRACE(testing::Message("origin_is_secure = ") << origin_is_secure);
    PasswordForm form(MakeSimpleForm());
    form.origin =
        GURL(origin_is_secure ? "https://example.com" : "http://example.com");
    std::vector<PasswordForm> observed = {form};
    EXPECT_CALL(*store_, GetLogins(_, _))
        .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
    manager()->OnPasswordFormsParsed(&driver_, observed);
    manager()->OnPasswordFormsRendered(&driver_, observed, true);

    EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
        .WillRepeatedly(Return(true));
    OnPasswordFormSubmitted(form);

    std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
    EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
        .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

    observed.clear();
    base::HistogramTester histogram_tester;
    manager()->OnPasswordFormsParsed(&driver_, observed);
    manager()->OnPasswordFormsRendered(&driver_, observed, true);
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.SuccessfulLoginHappened", origin_is_secure, 1);
  }
}

TEST_F(PasswordManagerTest, ManualFallbackForSavingNewParser) {
  base::test::ScopedFeatureList scoped_feature_list;
  TurnOnNewParsingForSaving(&scoped_feature_list);
  NewPasswordFormManager::set_wait_for_server_predictions_for_filling(false);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));

  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  PasswordForm stored_form = form;
  stored_form.password_value = ASCIIToUTF16("old_password");
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeConsumer(stored_form)));
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(2);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // The username of the stored form is the same, there should be update bubble.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, false, true))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
  manager()->ShowManualFallbackForSaving(&driver_, form);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(), FormMatches(form));

  // The username of the stored form is different, there should be save bubble.
  PasswordForm new_form = form;
  new_form.username_value = ASCIIToUTF16("another_username");
  new_form.form_data.fields[0].value = new_form.username_value;
  EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, false, false))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
  manager()->ShowManualFallbackForSaving(&driver_, new_form);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
              FormMatches(new_form));

  // Hide the manual fallback.
  EXPECT_CALL(client_, HideManualFallbackForSaving());
  manager()->HideManualFallbackForSaving();
}

// Check that some value for the ParsingOnSavingDifference UKM metric is emitted
// on a successful login.
TEST_F(PasswordManagerTest, ParsingOnSavingMetricRecorded) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  base::test::ScopedFeatureList scoped_feature_list;
  TurnOnNewParsingForSaving(&scoped_feature_list);

  EXPECT_CALL(client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));

  PasswordForm form = MakeSimpleForm();
  std::vector<PasswordForm> observed = {form};
  manager()->OnPasswordFormsParsed(&driver_, observed);

  // Provisionally save and simulate a successful landing page load to make
  // manager() believe this password should be saved.
  manager()->ProvisionallySavePassword(form, nullptr);
  manager()->OnPasswordFormsRendered(&driver_, {}, true);

  // Destroy |manager_| to send off UKM metrics.
  manager_.reset();

  std::vector<const ukm::mojom::UkmEntry*> ukm_entries =
      test_ukm_recorder.GetEntriesByName(
          ukm::builders::PasswordForm::kEntryName);
  ASSERT_EQ(1u, ukm_entries.size());
  test_ukm_recorder.EntryHasMetric(
      ukm_entries[0],
      ukm::builders::PasswordForm::kParsingOnSavingDifferenceName);
}

}  // namespace password_manager
