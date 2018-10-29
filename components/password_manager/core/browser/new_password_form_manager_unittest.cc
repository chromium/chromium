// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/new_password_form_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::FormData;
using autofill::FormStructure;
using autofill::FormFieldData;
using autofill::PasswordForm;
using autofill::PasswordFormFillData;
using base::ASCIIToUTF16;
using base::TestMockTimeTaskRunner;
using testing::_;
using testing::Mock;
using testing::NiceMock;
using testing::SaveArg;
using testing::SaveArgPointee;

namespace password_manager {

namespace {

// Indices of username and password fields in the observed form.
const int kUsernameFieldIndex = 1;
const int kPasswordFieldIndex = 2;

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MockPasswordManagerDriver() {}

  ~MockPasswordManagerDriver() override {}

  MOCK_METHOD1(FillPasswordForm, void(const PasswordFormFillData&));
  MOCK_METHOD1(AllowPasswordGenerationForForm, void(const PasswordForm&));
};

void CheckPendingCredentials(const PasswordForm& expected,
                             const PasswordForm& actual) {
  EXPECT_EQ(expected.signon_realm, actual.signon_realm);
  EXPECT_EQ(expected.origin, actual.origin);
  EXPECT_EQ(expected.action, actual.action);
  EXPECT_EQ(expected.username_value, actual.username_value);
  EXPECT_EQ(expected.password_value, actual.password_value);
  EXPECT_EQ(expected.username_element, actual.username_element);
  EXPECT_EQ(expected.password_element, actual.password_element);
  EXPECT_EQ(expected.blacklisted_by_user, actual.blacklisted_by_user);
  EXPECT_EQ(expected.form_data, actual.form_data);
}

struct ExpectedGenerationUKM {
  base::Optional<int64_t> generation_popup_shown;
  int64_t has_generated_password;
  base::Optional<int64_t> generated_password_modified;
};

// Check that UKM |metric_name| in |entry| is equal to |expected|. |expected| ==
// null means that no metric recording is expected.
void CheckMetric(const int64_t* expected,
                 const ukm::mojom::UkmEntry* entry,
                 const char* metric_name) {
  SCOPED_TRACE(testing::Message("Checking UKM metric ") << metric_name);

  const int64_t* actual =
      ukm::TestUkmRecorder::GetEntryMetric(entry, metric_name);

  ASSERT_EQ(!!expected, !!actual);
  if (expected)
    EXPECT_EQ(*expected, *actual);
}

// Check that |recorder| records metrics |expected_metrics|.
void CheckPasswordGenerationUKM(const ukm::TestAutoSetUkmRecorder& recorder,
                                const ExpectedGenerationUKM& expected_metrics) {
  auto entries =
      recorder.GetEntriesByName(ukm::builders::PasswordForm::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const int64_t* expected_popup_shown = nullptr;
  if (expected_metrics.generation_popup_shown)
    expected_popup_shown = &expected_metrics.generation_popup_shown.value();
  CheckMetric(expected_popup_shown, entries[0],
              ukm::builders::PasswordForm::kGeneration_PopupShownName);

  CheckMetric(&expected_metrics.has_generated_password, entries[0],
              ukm::builders::PasswordForm::kGeneration_GeneratedPasswordName);

  const int64_t* expected_password_modified = nullptr;
  if (expected_metrics.generated_password_modified)
    expected_password_modified =
        &expected_metrics.generated_password_modified.value();
  CheckMetric(
      expected_password_modified, entries[0],
      ukm::builders::PasswordForm::kGeneration_GeneratedPasswordModifiedName);
}

class MockFormSaver : public StubFormSaver {
 public:
  MockFormSaver() = default;

  ~MockFormSaver() override = default;

  // FormSaver:
  MOCK_METHOD1(PermanentlyBlacklist, void(autofill::PasswordForm* observed));
  MOCK_METHOD2(
      Save,
      void(const autofill::PasswordForm& pending,
           const std::map<base::string16, const PasswordForm*>& best_matches));
  MOCK_METHOD4(
      Update,
      void(const autofill::PasswordForm& pending,
           const std::map<base::string16, const PasswordForm*>& best_matches,
           const std::vector<autofill::PasswordForm>* credentials_to_update,
           const autofill::PasswordForm* old_primary_key));
  MOCK_METHOD1(PresaveGeneratedPassword,
               void(const autofill::PasswordForm& generated));
  MOCK_METHOD0(RemovePresavedPassword, void());

  std::unique_ptr<FormSaver> Clone() override {
    return std::make_unique<MockFormSaver>();
  }

  // Convenience downcasting method.
  static MockFormSaver& Get(NewPasswordFormManager* form_manager) {
    return *static_cast<MockFormSaver*>(form_manager->form_saver());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFormSaver);
};

// TODO(https://crbug.com/831123): Test sending metrics.
// TODO(https://crbug.com/831123): Test create pending credentials when
// generation happened.
// TODO(https://crbug.com/831123): Test create pending credentials with
// Credential API.
class NewPasswordFormManagerTest : public testing::Test {
 public:
  NewPasswordFormManagerTest() : task_runner_(new TestMockTimeTaskRunner) {
    GURL origin = GURL("https://accounts.google.com/a/ServiceLoginAuth");
    GURL action = GURL("https://accounts.google.com/a/ServiceLogin");
    GURL psl_origin = GURL("https://myaccounts.google.com/a/ServiceLoginAuth");
    GURL psl_action = GURL("https://myaccounts.google.com/a/ServiceLogin");

    observed_form_.origin = origin;
    observed_form_.action = action;
    observed_form_.name = ASCIIToUTF16("sign-in");
    observed_form_.unique_renderer_id = 1;
    observed_form_.is_form_tag = true;

    observed_form_only_password_fields_ = observed_form_;

    FormFieldData field;
    field.name = ASCIIToUTF16("firstname");
    field.form_control_type = "text";
    field.unique_renderer_id = 1;
    observed_form_.fields.push_back(field);

    field.name = ASCIIToUTF16("username");
    field.id = field.name;
    field.form_control_type = "text";
    field.unique_renderer_id = 2;
    observed_form_.fields.push_back(field);

    field.name = ASCIIToUTF16("password");
    field.id = field.name;
    field.form_control_type = "password";
    field.unique_renderer_id = 3;
    observed_form_.fields.push_back(field);
    observed_form_only_password_fields_.fields.push_back(field);

    field.name = ASCIIToUTF16("password2");
    field.id = field.name;
    field.form_control_type = "password";
    field.unique_renderer_id = 5;
    observed_form_only_password_fields_.fields.push_back(field);

    submitted_form_ = observed_form_;
    submitted_form_.fields[kUsernameFieldIndex].value = ASCIIToUTF16("user1");
    submitted_form_.fields[kPasswordFieldIndex].value = ASCIIToUTF16("secret1");

    saved_match_.origin = origin;
    saved_match_.action = action;
    saved_match_.signon_realm = "https://accounts.google.com/";
    saved_match_.preferred = true;
    saved_match_.username_value = ASCIIToUTF16("test@gmail.com");
    saved_match_.username_element = ASCIIToUTF16("field1");
    saved_match_.password_value = ASCIIToUTF16("test1");
    saved_match_.password_element = ASCIIToUTF16("field2");
    saved_match_.is_public_suffix_match = false;
    saved_match_.scheme = PasswordForm::SCHEME_HTML;

    psl_saved_match_ = saved_match_;
    psl_saved_match_.origin = psl_origin;
    psl_saved_match_.action = psl_action;
    psl_saved_match_.signon_realm = "https://myaccounts.google.com/";
    psl_saved_match_.is_public_suffix_match = true;

    parsed_observed_form_ = saved_match_;
    parsed_observed_form_.form_data = observed_form_;
    parsed_observed_form_.username_element =
        observed_form_.fields[kUsernameFieldIndex].name;
    parsed_observed_form_.password_element =
        observed_form_.fields[kPasswordFieldIndex].name;

    parsed_submitted_form_ = parsed_observed_form_;
    parsed_submitted_form_.username_value =
        submitted_form_.fields[kUsernameFieldIndex].value;
    parsed_submitted_form_.password_value =
        submitted_form_.fields[kPasswordFieldIndex].value;

    blacklisted_match_ = saved_match_;
    blacklisted_match_.blacklisted_by_user = true;

    CreateFormManager(observed_form_);
  }

 protected:
  FormData observed_form_;
  FormData submitted_form_;
  FormData observed_form_only_password_fields_;
  PasswordForm saved_match_;
  PasswordForm psl_saved_match_;
  PasswordForm blacklisted_match_;
  PasswordForm parsed_observed_form_;
  PasswordForm parsed_submitted_form_;
  StubPasswordManagerClient client_;
  MockPasswordManagerDriver driver_;
  scoped_refptr<TestMockTimeTaskRunner> task_runner_;
  // Define |fetcher_| before |form_manager_|, because the former needs to
  // outlive the latter.
  std::unique_ptr<FakeFormFetcher> fetcher_;
  std::unique_ptr<NewPasswordFormManager> form_manager_;

  // Creates NewPasswordFormManager and sets it to |form_manager_|. Along the
  // way a new |fetcher_| is created.
  void CreateFormManager(const FormData& observed_form) {
    fetcher_.reset(new FakeFormFetcher());
    fetcher_->Fetch();
    form_manager_.reset(new NewPasswordFormManager(
        &client_, driver_.AsWeakPtr(), observed_form, fetcher_.get(),
        std::make_unique<NiceMock<MockFormSaver>>(), nullptr));
  }
};

TEST_F(NewPasswordFormManagerTest, DoesManage) {
  EXPECT_TRUE(form_manager_->DoesManage(observed_form_, &driver_));
  // Forms on other drivers are not considered managed.
  EXPECT_FALSE(form_manager_->DoesManage(observed_form_, nullptr));
  FormData another_form = observed_form_;
  another_form.is_form_tag = false;
  EXPECT_FALSE(form_manager_->DoesManage(another_form, &driver_));

  another_form = observed_form_;
  another_form.unique_renderer_id = observed_form_.unique_renderer_id + 1;
  EXPECT_FALSE(form_manager_->DoesManage(another_form, &driver_));
}

TEST_F(NewPasswordFormManagerTest, DoesManageNoFormTag) {
  observed_form_.is_form_tag = false;
  CreateFormManager(observed_form_);

  FormData another_form = observed_form_;
  // Simulate that new input was added by JavaScript.
  another_form.fields.push_back(FormFieldData());
  EXPECT_TRUE(form_manager_->DoesManage(another_form, &driver_));
  // Forms on other drivers are not considered managed.
  EXPECT_FALSE(form_manager_->DoesManage(another_form, nullptr));
}

TEST_F(NewPasswordFormManagerTest, Autofill) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());

  CreateFormManager(observed_form_);
  EXPECT_CALL(driver_, AllowPasswordGenerationForForm(_));
  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&fill_data));
  CreateFormManager(observed_form_);
  fetcher_->SetNonFederated({&saved_match_}, 0u);

  task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_EQ(observed_form_.origin, fill_data.origin);
  EXPECT_FALSE(fill_data.wait_for_username);
  EXPECT_EQ(observed_form_.fields[1].name, fill_data.username_field.name);
  EXPECT_EQ(saved_match_.username_value, fill_data.username_field.value);
  EXPECT_EQ(observed_form_.fields[2].name, fill_data.password_field.name);
  EXPECT_EQ(saved_match_.password_value, fill_data.password_field.value);
}

TEST_F(NewPasswordFormManagerTest, AutofillNotMoreThan5Times) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());

  EXPECT_CALL(driver_, FillPasswordForm(_));
  fetcher_->SetNonFederated({&saved_match_}, 0u);

  task_runner_->FastForwardUntilNoTasksRemain();
  Mock::VerifyAndClearExpectations(&driver_);

  for (size_t i = 0; i < NewPasswordFormManager::kMaxTimesAutofill - 1; ++i) {
    EXPECT_CALL(driver_, FillPasswordForm(_));
    form_manager_->Fill();
    Mock::VerifyAndClearExpectations(&driver_);
  }

  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
  form_manager_->Fill();
}

// NewPasswordFormManager should always send fill data to renderer, even for
// sign-up forms (no "current-password" field, i.e., no password field to fill
// into). However, for sign-up forms, no particular password field should be
// identified for filling. That way, Chrome won't disturb the user by filling
// the sign-up form, but will be able to offer a manual fallback for filling if
// the form was misclassified.
TEST_F(NewPasswordFormManagerTest, AutofillSignUpForm) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  // Make |observed_form_| to be sign-up form.
  observed_form_.fields.back().autocomplete_attribute = "new-password";

  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&fill_data));
  CreateFormManager(observed_form_);
  fetcher_->SetNonFederated({&saved_match_}, 0u);

  task_runner_->FastForwardUntilNoTasksRemain();
  constexpr uint32_t kNoID = FormFieldData::kNotSetFormControlRendererId;
  EXPECT_EQ(kNoID, fill_data.password_field.unique_renderer_id);
  EXPECT_EQ(saved_match_.password_value, fill_data.password_field.value);
}

TEST_F(NewPasswordFormManagerTest, AutofillWithBlacklistedMatch) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&fill_data));
  fetcher_->SetNonFederated({&saved_match_, &blacklisted_match_}, 0u);

  task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_EQ(observed_form_.origin, fill_data.origin);
  EXPECT_EQ(saved_match_.username_value, fill_data.username_field.value);
  EXPECT_EQ(saved_match_.password_value, fill_data.password_field.value);
}

TEST_F(NewPasswordFormManagerTest, SetSubmitted) {
  EXPECT_FALSE(form_manager_->is_submitted());
  EXPECT_TRUE(
      form_manager_->SetSubmittedFormIfIsManaged(submitted_form_, &driver_));
  EXPECT_TRUE(form_manager_->is_submitted());

  FormData another_form = submitted_form_;
  another_form.name += ASCIIToUTF16("1");
  // |another_form| is managed because the same |unique_renderer_id| as
  // |observed_form_|.
  EXPECT_TRUE(
      form_manager_->SetSubmittedFormIfIsManaged(another_form, &driver_));
  EXPECT_TRUE(form_manager_->is_submitted());

  form_manager_->set_not_submitted();
  EXPECT_FALSE(form_manager_->is_submitted());

  another_form.unique_renderer_id = observed_form_.unique_renderer_id + 1;
  EXPECT_FALSE(
      form_manager_->SetSubmittedFormIfIsManaged(another_form, &driver_));
  EXPECT_FALSE(form_manager_->is_submitted());

  // An identical form but in a different frame (represented here by a null
  // driver) is also not considered managed.
  EXPECT_FALSE(
      form_manager_->SetSubmittedFormIfIsManaged(observed_form_, nullptr));
  EXPECT_FALSE(form_manager_->is_submitted());

  // Check if the subbmitted form can not be parsed then form manager does not
  // became submitted.
  FormData malformed_form = submitted_form_;
  malformed_form.fields.clear();
  EXPECT_FALSE(
      form_manager_->SetSubmittedFormIfIsManaged(malformed_form, &driver_));
  EXPECT_FALSE(form_manager_->is_submitted());
}

// Tests that when NewPasswordFormManager receives saved matches it waits for
// server predictions and fills on receving them.
TEST_F(NewPasswordFormManagerTest, ServerPredictionsWithinDelay) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());

  // Expects no filling on save matches receiving.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
  fetcher_->SetNonFederated({&saved_match_}, 0u);
  Mock::VerifyAndClearExpectations(&driver_);

  FormStructure form_structure(observed_form_);
  form_structure.field(2)->set_server_type(autofill::PASSWORD);
  std::vector<FormStructure*> predictions{&form_structure};

  // Expect filling without delay on receiving server predictions.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(1);
  form_manager_->ProcessServerPredictions(predictions);
}

// Tests that NewPasswordFormManager fills after some delay even without
// server predictions.
TEST_F(NewPasswordFormManagerTest, ServerPredictionsAfterDelay) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());

  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(1);
  fetcher_->SetNonFederated({&saved_match_}, 0u);
  // Expect filling after passing filling delay.

  // Simulate passing filling delay.
  task_runner_->FastForwardUntilNoTasksRemain();
  Mock::VerifyAndClearExpectations(&driver_);

  FormStructure form_structure(observed_form_);
  form_structure.field(2)->set_server_type(autofill::PASSWORD);
  std::vector<FormStructure*> predictions{&form_structure};

  // Expect filling on receiving server predictions because it was less than
  // kMaxTimesAutofill attempts to fill.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(1);
  form_manager_->ProcessServerPredictions(predictions);
  task_runner_->FastForwardUntilNoTasksRemain();
}

// Tests that filling happens immediately if server predictions are received
// before saved matches.
TEST_F(NewPasswordFormManagerTest, ServerPredictionsBeforeFetcher) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  // Expect no filling after receiving saved matches from |fetcher_|, since
  // |form_manager| is waiting for server-side predictions.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
  CreateFormManager(observed_form_);
  FormStructure form_structure(observed_form_);
  form_structure.field(2)->set_server_type(autofill::PASSWORD);
  std::vector<FormStructure*> predictions{&form_structure};
  form_manager_->ProcessServerPredictions(predictions);
  Mock::VerifyAndClearExpectations(&driver_);

  // Expect filling without delay on receiving server predictions.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(1);
  fetcher_->SetNonFederated({&saved_match_}, 0u);
}

// Tests creating pending credentials when the password store is empty.
TEST_F(NewPasswordFormManagerTest, CreatePendingCredentialsEmptyStore) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->SetNonFederated({}, 0u);

  EXPECT_TRUE(
      form_manager_->SetSubmittedFormIfIsManaged(submitted_form_, &driver_));
  CheckPendingCredentials(parsed_submitted_form_,
                          form_manager_->GetPendingCredentials());
}

// Tests creating pending credentials when new credentials are submitted and the
// store has another credentials saved.
TEST_F(NewPasswordFormManagerTest, CreatePendingCredentialsNewCredentials) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->SetNonFederated({&saved_match_}, 0u);

  EXPECT_TRUE(
      form_manager_->SetSubmittedFormIfIsManaged(submitted_form_, &driver_));
  CheckPendingCredentials(parsed_submitted_form_,
                          form_manager_->GetPendingCredentials());
}

// Tests that when submitted credentials are equal to already saved one then
// pending credentials equal to saved match.
TEST_F(NewPasswordFormManagerTest, CreatePendingCredentialsAlreadySaved) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->SetNonFederated({&saved_match_}, 0u);

  submitted_form_.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  submitted_form_.fields[kPasswordFieldIndex].value =
      saved_match_.password_value;
  EXPECT_TRUE(
      form_manager_->SetSubmittedFormIfIsManaged(submitted_form_, &driver_));
  CheckPendingCredentials(/* expected */ saved_match_,
                          form_manager_->GetPendingCredentials());
}

// Tests that when submitted credentials are equal to already saved PSL
// credentials.
TEST_F(NewPasswordFormManagerTest, CreatePendingCredentialsPSLMatchSaved) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  PasswordForm expected = saved_match_;

  saved_match_.origin = GURL("https://m.accounts.google.com/auth");
  saved_match_.signon_realm = "https://m.accounts.google.com/";
  saved_match_.is_public_suffix_match = true;

  fetcher_->SetNonFederated({&saved_match_}, 0u);

  submitted_form_.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  submitted_form_.fields[kPasswordFieldIndex].value =
      saved_match_.password_value;

  EXPECT_TRUE(
      form_manager_->SetSubmittedFormIfIsManaged(submitted_form_, &driver_));
  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
}

// Tests creating pending credentials when new credentials are different only in
// password with already saved one.
TEST_F(NewPasswordFormManagerTest, CreatePendingCredentialsPasswordOverriden) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->SetNonFederated({&saved_match_}, 0u);

  PasswordForm expected = saved_match_;
  expected.password_value += ASCIIToUTF16("1");

  submitted_form_.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  submitted_form_.fields[kPasswordFieldIndex].value = expected.password_value;
  EXPECT_TRUE(
      form_manager_->SetSubmittedFormIfIsManaged(submitted_form_, &driver_));
  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
}

// Tests that when submitted credentials are equal to already saved one then
// pending credentials equal to saved match.
TEST_F(NewPasswordFormManagerTest, CreatePendingCredentialsUpdate) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->SetNonFederated({&saved_match_}, 0u);

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = ASCIIToUTF16("strongpassword");
  submitted_form.fields[1].value = ASCIIToUTF16("verystrongpassword");

  PasswordForm expected = saved_match_;
  expected.password_value = ASCIIToUTF16("verystrongpassword");

  EXPECT_TRUE(
      form_manager_->SetSubmittedFormIfIsManaged(submitted_form, &driver_));
  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
}

// Tests creating pending credentials when a change password form is submitted
// and there are multipe saved forms.
TEST_F(NewPasswordFormManagerTest,
       CreatePendingCredentialsUpdateMultipleSaved) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  PasswordForm another_saved_match = saved_match_;
  another_saved_match.username_value += ASCIIToUTF16("1");
  fetcher_->SetNonFederated({&saved_match_, &another_saved_match}, 0u);

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = ASCIIToUTF16("strongpassword");
  submitted_form.fields[1].value = ASCIIToUTF16("verystrongpassword");

  PasswordForm expected;
  expected.origin = observed_form_.origin;
  expected.action = observed_form_.action;
  expected.password_value = ASCIIToUTF16("verystrongpassword");

  EXPECT_TRUE(
      form_manager_->SetSubmittedFormIfIsManaged(submitted_form, &driver_));
  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
}

// Tests that there is no crash even when the observed form is a not password
// form and the submitted form is password form.
TEST_F(NewPasswordFormManagerTest, NoCrashOnNonPasswordForm) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  FormData form_without_password_fields = observed_form_;
  // Remove the password field.
  form_without_password_fields.fields.resize(kPasswordFieldIndex);
  CreateFormManager(form_without_password_fields);
  fetcher_->SetNonFederated({}, 0u);

  FormData submitted_form = observed_form_;
  submitted_form.fields[kUsernameFieldIndex].value = ASCIIToUTF16("username");
  submitted_form.fields[kPasswordFieldIndex].value = ASCIIToUTF16("password");

  // Expect no crash.
  form_manager_->SetSubmittedFormIfIsManaged(submitted_form, &driver_);
}

TEST_F(NewPasswordFormManagerTest, IsEqualToSubmittedForm) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->SetNonFederated({}, 0u);

  FormData submitted_form = observed_form_;
  submitted_form.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  submitted_form.fields[kPasswordFieldIndex].value =
      saved_match_.password_value;

  // No submitted form yet.
  EXPECT_FALSE(form_manager_->IsEqualToSubmittedForm(submitted_form));

  ASSERT_TRUE(
      form_manager_->SetSubmittedFormIfIsManaged(submitted_form, &driver_));

  observed_form_.unique_renderer_id += 10;
  observed_form_.fields.clear();

  EXPECT_TRUE(form_manager_->IsEqualToSubmittedForm(observed_form_));

  observed_form_.action = GURL("https://example.com");
  EXPECT_FALSE(form_manager_->IsEqualToSubmittedForm(observed_form_));
}

// Tests that when credentials with a new username (i.e. not saved yet) is
// successfully submitted, then they are saved correctly.
TEST_F(NewPasswordFormManagerTest, SaveNewCredentials) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  fetcher_->SetNonFederated({&saved_match_}, 0u);

  FormData submitted_form = observed_form_;
  base::string16 new_username = saved_match_.username_value + ASCIIToUTF16("1");
  base::string16 new_password = saved_match_.password_value + ASCIIToUTF16("1");
  submitted_form.fields[kUsernameFieldIndex].value = new_username;
  submitted_form.fields[kPasswordFieldIndex].value = new_password;

  EXPECT_TRUE(
      form_manager_->SetSubmittedFormIfIsManaged(submitted_form, &driver_));
  EXPECT_TRUE(form_manager_->IsNewLogin());

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());
  PasswordForm saved_form;
  std::map<base::string16, const PasswordForm*> best_matches;
  EXPECT_CALL(form_saver, Save(_, _))
      .WillOnce(DoAll(SaveArg<0>(&saved_form), SaveArg<1>(&best_matches)));

  form_manager_->Save();

  std::string expected_signon_realm = submitted_form.origin.GetOrigin().spec();
  EXPECT_EQ(submitted_form.origin, saved_form.origin);
  EXPECT_EQ(expected_signon_realm, saved_form.signon_realm);
  EXPECT_EQ(new_username, saved_form.username_value);
  EXPECT_EQ(new_password, saved_form.password_value);
  EXPECT_TRUE(saved_form.preferred);

  EXPECT_EQ(submitted_form.fields[kUsernameFieldIndex].name,
            saved_form.username_element);
  EXPECT_EQ(submitted_form.fields[kPasswordFieldIndex].name,
            saved_form.password_element);
  EXPECT_EQ(1u, best_matches.size());
  base::string16 saved_username = saved_match_.username_value;
  ASSERT_TRUE(best_matches.find(saved_username) != best_matches.end());
  EXPECT_EQ(saved_match_, *best_matches[saved_username]);

  // Check UKM metrics.
  form_manager_.reset();
  ExpectedGenerationUKM expected_metrics = {
      {} /* shown manually */,
      0 /* password generated */,
      {} /* generated password is not modified */};

  CheckPasswordGenerationUKM(test_ukm_recorder, expected_metrics);
}

// Check that if there is saved PSL matched credentials with the same
// username/password as in submitted form, then the saved form is the same
// already saved only with origin and signon_realm from the submitted form.
TEST_F(NewPasswordFormManagerTest, SavePSLToAlreadySaved) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->SetNonFederated({&psl_saved_match_}, 0u);

  FormData submitted_form = observed_form_;
  // Change
  submitted_form.fields[kUsernameFieldIndex].value =
      psl_saved_match_.username_value;
  submitted_form.fields[kPasswordFieldIndex].value =
      psl_saved_match_.password_value;

  EXPECT_TRUE(
      form_manager_->SetSubmittedFormIfIsManaged(submitted_form, &driver_));
  EXPECT_TRUE(form_manager_->IsNewLogin());
  EXPECT_TRUE(form_manager_->IsPendingCredentialsPublicSuffixMatch());

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());
  PasswordForm saved_form;
  std::map<base::string16, const PasswordForm*> best_matches;
  EXPECT_CALL(form_saver, Save(_, _))
      .WillOnce(DoAll(SaveArg<0>(&saved_form), SaveArg<1>(&best_matches)));

  form_manager_->Save();

  EXPECT_EQ(submitted_form.origin, saved_form.origin);
  EXPECT_EQ(GetSignonRealm(submitted_form.origin), saved_form.signon_realm);
  EXPECT_EQ(saved_form.username_value, psl_saved_match_.username_value);
  EXPECT_EQ(saved_form.password_value, psl_saved_match_.password_value);
  EXPECT_EQ(saved_form.username_element, psl_saved_match_.username_element);
  EXPECT_EQ(saved_form.password_element, psl_saved_match_.password_element);

  EXPECT_TRUE(saved_form.preferred);

  EXPECT_EQ(1u, best_matches.size());
  base::string16 saved_username = psl_saved_match_.username_value;
  ASSERT_TRUE(best_matches.find(saved_username) != best_matches.end());
  EXPECT_EQ(psl_saved_match_, *best_matches[saved_username]);
}

// Tests that when credentials with already saved username but with a new
// password are submitted, then the saved password is updated.
TEST_F(NewPasswordFormManagerTest, OverridePassword) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->SetNonFederated({&saved_match_}, 0u);

  FormData submitted_form = observed_form_;
  base::string16 username = saved_match_.username_value;
  base::string16 new_password = saved_match_.password_value + ASCIIToUTF16("1");
  submitted_form.fields[kUsernameFieldIndex].value = username;
  submitted_form.fields[kPasswordFieldIndex].value = new_password;

  EXPECT_TRUE(
      form_manager_->SetSubmittedFormIfIsManaged(submitted_form, &driver_));
  EXPECT_FALSE(form_manager_->IsNewLogin());
  EXPECT_TRUE(form_manager_->IsPasswordOverridden());

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());
  PasswordForm updated_form;
  std::map<base::string16, const PasswordForm*> best_matches;
  std::vector<PasswordForm> credentials_to_update;
  EXPECT_CALL(form_saver, Update(_, _, _, nullptr))
      .WillOnce(DoAll(SaveArg<0>(&updated_form), SaveArg<1>(&best_matches),
                      SaveArgPointee<2>(&credentials_to_update)));

  form_manager_->Save();

  EXPECT_TRUE(ArePasswordFormUniqueKeyEqual(saved_match_, updated_form));
  EXPECT_TRUE(updated_form.preferred);
  EXPECT_EQ(new_password, updated_form.password_value);
  EXPECT_EQ(1u, best_matches.size());
  ASSERT_TRUE(best_matches.find(username) != best_matches.end());
  EXPECT_EQ(saved_match_, *best_matches[username]);
  EXPECT_TRUE(credentials_to_update.empty());
}

// Tests that when the user changes password on a change password form then the
// saved password is updated.
TEST_F(NewPasswordFormManagerTest, UpdatePasswordOnChangePasswordForm) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  CreateFormManager(observed_form_only_password_fields_);
  PasswordForm not_best_saved_match = saved_match_;
  not_best_saved_match.preferred = false;
  PasswordForm saved_match_another_username = saved_match_;
  saved_match_another_username.username_value += ASCIIToUTF16("1");

  fetcher_->SetNonFederated(
      {&saved_match_, &not_best_saved_match, &saved_match_another_username},
      0u);

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = saved_match_.password_value;
  base::string16 new_password = saved_match_.password_value + ASCIIToUTF16("1");
  submitted_form.fields[1].value = new_password;

  EXPECT_TRUE(
      form_manager_->SetSubmittedFormIfIsManaged(submitted_form, &driver_));
  EXPECT_FALSE(form_manager_->IsNewLogin());
  EXPECT_FALSE(form_manager_->IsPasswordOverridden());

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());
  PasswordForm updated_form;
  std::map<base::string16, const PasswordForm*> best_matches;
  std::vector<PasswordForm> credentials_to_update;
  EXPECT_CALL(form_saver, Update(_, _, _, nullptr))
      .WillOnce(DoAll(SaveArg<0>(&updated_form), SaveArg<1>(&best_matches),
                      SaveArgPointee<2>(&credentials_to_update)));

  form_manager_->Save();

  EXPECT_TRUE(ArePasswordFormUniqueKeyEqual(saved_match_, updated_form));
  EXPECT_TRUE(updated_form.preferred);
  EXPECT_EQ(new_password, updated_form.password_value);

  EXPECT_EQ(2u, best_matches.size());
  base::string16 username = saved_match_.username_value;
  ASSERT_TRUE(best_matches.find(username) != best_matches.end());
  EXPECT_EQ(saved_match_, *best_matches[username]);
  base::string16 another_username = saved_match_another_username.username_value;
  ASSERT_TRUE(best_matches.find(another_username) != best_matches.end());
  EXPECT_EQ(saved_match_another_username, *best_matches[another_username]);

  ASSERT_EQ(1u, credentials_to_update.size());
  not_best_saved_match.password_value = new_password;
  EXPECT_EQ(not_best_saved_match, credentials_to_update[0]);
}

TEST_F(NewPasswordFormManagerTest, UpdateUsernameEmptyStore) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->SetNonFederated({}, 0u);

  form_manager_->SetSubmittedFormIfIsManaged(submitted_form_, &driver_);

  base::string16 new_username =
      parsed_submitted_form_.username_value + ASCIIToUTF16("1");
  PasswordForm expected = parsed_submitted_form_;
  expected.username_value = new_username;
  expected.username_element.clear();

  form_manager_->UpdateUsername(new_username);

  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
  EXPECT_TRUE(form_manager_->IsNewLogin());
}

TEST_F(NewPasswordFormManagerTest, UpdateUsernameToAlreadyExisting) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->SetNonFederated({&saved_match_}, 0u);

  form_manager_->SetSubmittedFormIfIsManaged(submitted_form_, &driver_);

  base::string16 new_username = saved_match_.username_value;
  base::string16 expected_password = parsed_submitted_form_.password_value;
  PasswordForm expected = saved_match_;
  expected.password_value = expected_password;

  form_manager_->UpdateUsername(new_username);

  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
  EXPECT_FALSE(form_manager_->IsNewLogin());
  EXPECT_TRUE(form_manager_->IsPasswordOverridden());
}

TEST_F(NewPasswordFormManagerTest, UpdatePasswordEmptyStore) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->SetNonFederated({}, 0u);

  form_manager_->SetSubmittedFormIfIsManaged(submitted_form_, &driver_);

  base::string16 new_password =
      parsed_submitted_form_.password_value + ASCIIToUTF16("1");
  PasswordForm expected = parsed_submitted_form_;
  expected.password_value = new_password;
  expected.password_element.clear();

  form_manager_->UpdatePasswordValue(new_password);

  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
  EXPECT_TRUE(form_manager_->IsNewLogin());
}

TEST_F(NewPasswordFormManagerTest, UpdatePasswordToAlreadyExisting) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->SetNonFederated({&saved_match_}, 0u);

  // Emulate submitting form with known username and different password.
  submitted_form_.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  form_manager_->SetSubmittedFormIfIsManaged(submitted_form_, &driver_);

  // The user changes password to already saved one.
  base::string16 password = saved_match_.password_value;
  form_manager_->UpdatePasswordValue(password);

  CheckPendingCredentials(saved_match_, form_manager_->GetPendingCredentials());
  EXPECT_FALSE(form_manager_->IsNewLogin());
  EXPECT_FALSE(form_manager_->IsPasswordOverridden());
}

TEST_F(NewPasswordFormManagerTest, PermanentlyBlacklist) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->SetNonFederated({}, 0u);

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

  PasswordForm* new_blacklisted_form = nullptr;
  EXPECT_CALL(form_saver, PermanentlyBlacklist(_))
      .WillOnce(SaveArg<0>(&new_blacklisted_form));

  form_manager_->PermanentlyBlacklist();
  ASSERT_TRUE(new_blacklisted_form);
  EXPECT_EQ(observed_form_.origin, new_blacklisted_form->origin);
  EXPECT_EQ(GetSignonRealm(observed_form_.origin),
            new_blacklisted_form->signon_realm);
}

TEST_F(NewPasswordFormManagerTest, Clone) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->SetNonFederated({}, 0u);

  std::unique_ptr<NewPasswordFormManager> cloned_manager =
      form_manager_->Clone();

  EXPECT_TRUE(cloned_manager->DoesManage(observed_form_, nullptr));
  EXPECT_TRUE(cloned_manager->GetFormFetcher());
  // Check that |form_fetcher| was cloned.
  EXPECT_NE(form_manager_->GetFormFetcher(), cloned_manager->GetFormFetcher());

  EXPECT_EQ(form_manager_->metrics_recorder(),
            cloned_manager->metrics_recorder());
}

// Extracts the information whether parsing was successful from a metric
// specified by |metric_name| stored in |entry|. The metric name should be one
// of ukm::builders::PasswordForm::kReadonlyWhenSavingName and
// ukm::builders::PasswordForm::kReadonlyWhenFillingName.
bool ParsingSuccessReported(const ukm::mojom::UkmEntry* entry,
                            base::StringPiece metric_name) {
  const int64_t* value =
      ukm::TestUkmRecorder::GetEntryMetric(entry, metric_name);
  EXPECT_TRUE(value);
  // Ideally, an ASSERT_TRUE above would prevent the test suite from crashing on
  // dereferencing |value| below. But ASSERT_* is not available in non-void
  // returning functions, so the null value is handled explicitly.
  if (!value)
    return false;  // Value does not matter, the test already failed.
  return 1 == (1 & *value);
}

// Test that an attempt to log to ReadonlyWhenFilling UKM is made when filling.
TEST_F(NewPasswordFormManagerTest, RecordReadonlyWhenFilling) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  EXPECT_CALL(driver_, AllowPasswordGenerationForForm(_));
  EXPECT_CALL(driver_, FillPasswordForm(_));
  fetcher_->SetNonFederated({&saved_match_}, 0u);

  task_runner_->FastForwardUntilNoTasksRemain();

  // Destroy the form manager to destroy the UKM recorder it owns. The recorder
  // only records metrics in its destructor.
  form_manager_.reset();

  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::PasswordForm::kEntryName);
  ASSERT_EQ(1u, entries.size());

  EXPECT_TRUE(ParsingSuccessReported(
      entries[0], ukm::builders::PasswordForm::kReadonlyWhenFillingName));
}

// Test that an attempt to log to ReadonlyWhenFilling UKM is made when filling,
// even when the parsing itself is unsuccessful.
TEST_F(NewPasswordFormManagerTest, RecordReadonlyWhenFilling_ParsingFailed) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());

  FormData malformed_form = observed_form_;
  malformed_form.fields.clear();
  CreateFormManager(malformed_form);
  // Only create the recorder after the current form manager is created,
  // otherwise the destruction of the previous one will add unwanted UKM entries
  // in it.
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  fetcher_->SetNonFederated({&saved_match_}, 0u);

  task_runner_->FastForwardUntilNoTasksRemain();

  // Destroy the form manager to destroy the UKM recorder it owns. The recorder
  // only records metrics in its destructor.
  form_manager_.reset();

  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::PasswordForm::kEntryName);
  ASSERT_EQ(1u, entries.size());

  EXPECT_FALSE(ParsingSuccessReported(
      entries[0], ukm::builders::PasswordForm::kReadonlyWhenFillingName));
}

// Test that an attempt to log to ReadonlyWhenSaving UKM is made when creating
// pending credentials.
TEST_F(NewPasswordFormManagerTest, RecordReadonlyWhenSaving) {
  // The scoped context is needed for the UKM recorder.
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  fetcher_->SetNonFederated({&saved_match_}, 0u);

  EXPECT_TRUE(
      form_manager_->SetSubmittedFormIfIsManaged(submitted_form_, &driver_));

  // Destroy the form manager to destroy the UKM recorder it owns. The recorder
  // only records metrics in its destructor.
  form_manager_.reset();

  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::PasswordForm::kEntryName);
  ASSERT_EQ(1u, entries.size());

  EXPECT_TRUE(ParsingSuccessReported(
      entries[0], ukm::builders::PasswordForm::kReadonlyWhenSavingName));
}

// Test that an attempt to log to ReadonlyWhenSaving UKM is made when creating
// pending credentials, even when their parsing itself is unsuccessful.
TEST_F(NewPasswordFormManagerTest, RecordReadonlyWhenSaving_ParsingFailed) {
  // The scoped context is needed for the UKM recorder.
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  fetcher_->SetNonFederated({&saved_match_}, 0u);

  FormData malformed_form = submitted_form_;
  malformed_form.fields.clear();
  EXPECT_FALSE(
      form_manager_->SetSubmittedFormIfIsManaged(malformed_form, &driver_));

  // Destroy the form manager to destroy the UKM recorder it owns. The recorder
  // only records metrics in its destructor.
  form_manager_.reset();

  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::PasswordForm::kEntryName);
  ASSERT_EQ(1u, entries.size());

  EXPECT_FALSE(ParsingSuccessReported(
      entries[0], ukm::builders::PasswordForm::kReadonlyWhenSavingName));
}

TEST_F(NewPasswordFormManagerTest, PresaveGeneratedPasswordEmptyStore) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  fetcher_->SetNonFederated({}, 0u);

  EXPECT_FALSE(form_manager_->HasGeneratedPassword());

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

  form_manager_->SetGenerationPopupWasShown(
      true /* generation_popup_was_shown */, false /* is_manual_generation */);

  // Check that the generated password is presaved.
  PasswordForm saved_form;
  EXPECT_CALL(form_saver, PresaveGeneratedPassword(_))
      .WillOnce(SaveArg<0>(&saved_form));

  PasswordForm form_with_generated_password;
  form_with_generated_password.form_data = submitted_form_;
  FormData& form_data = form_with_generated_password.form_data;
  form_manager_->PresaveGeneratedPassword(form_with_generated_password);

  EXPECT_TRUE(form_manager_->HasGeneratedPassword());
  EXPECT_EQ(saved_form.username_value,
            form_data.fields[kUsernameFieldIndex].value);
  EXPECT_EQ(saved_form.password_value,
            form_data.fields[kPasswordFieldIndex].value);

  Mock::VerifyAndClearExpectations(&form_saver);

  // Check that when the generated password is edited, then it's presaved.
  form_data.fields[kPasswordFieldIndex].value += ASCIIToUTF16("1");
  EXPECT_CALL(form_saver, PresaveGeneratedPassword(_))
      .WillOnce(SaveArg<0>(&saved_form));

  form_manager_->PresaveGeneratedPassword(form_with_generated_password);

  EXPECT_TRUE(form_manager_->HasGeneratedPassword());
  EXPECT_EQ(saved_form.username_value,
            form_data.fields[kUsernameFieldIndex].value);
  EXPECT_EQ(saved_form.password_value,
            form_data.fields[kPasswordFieldIndex].value);

  Mock::VerifyAndClearExpectations(&form_saver);

  // Check UKM metrics.
  form_manager_.reset();
  ExpectedGenerationUKM expected_metrics = {
      base::make_optional(1u) /* shown automatically */,
      1 /* password generated */,
      base::make_optional(1u) /* password modified */};

  CheckPasswordGenerationUKM(test_ukm_recorder, expected_metrics);
}

TEST_F(NewPasswordFormManagerTest, PasswordNoLongerGenerated) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  fetcher_->SetNonFederated({}, 0u);

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());
  form_manager_->SetGenerationPopupWasShown(
      true /* generation_popup_was_shown */, true /* is_manual_generation */);

  EXPECT_CALL(form_saver, PresaveGeneratedPassword(_));

  PasswordForm form;
  form.form_data = submitted_form_;
  form_manager_->PresaveGeneratedPassword(form);
  Mock::VerifyAndClearExpectations(&form_saver);

  EXPECT_TRUE(form_manager_->HasGeneratedPassword());

  // Check when the user removes the generated password on the page, it is
  // removed from the store.
  EXPECT_CALL(form_saver, RemovePresavedPassword());
  form_manager_->PasswordNoLongerGenerated();

  EXPECT_FALSE(form_manager_->HasGeneratedPassword());

  // Check UKM metrics.
  form_manager_.reset();
  ExpectedGenerationUKM expected_metrics = {
      base::make_optional(2u) /* shown manually */,
      0 /* password generated */,
      {} /* generated password is not modified */};

  CheckPasswordGenerationUKM(test_ukm_recorder, expected_metrics);
}

TEST_F(NewPasswordFormManagerTest, PresaveGeneratedPasswordExistingCredential) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  fetcher_->SetNonFederated({&saved_match_}, 0u);

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

  form_manager_->SetGenerationPopupWasShown(
      true /* generation_popup_was_shown */, false /* is_manual_generation */);

  // Check that the generated password is presaved.
  PasswordForm saved_form;
  EXPECT_CALL(form_saver, PresaveGeneratedPassword(_))
      .WillOnce(SaveArg<0>(&saved_form));

  PasswordForm form_with_generated_password;
  form_with_generated_password.form_data = submitted_form_;
  FormData& form_data = form_with_generated_password.form_data;

  // Check that the generated password is saved with the empty username when
  // there is already a saved credetial with the same username.
  form_data.fields[kUsernameFieldIndex].value = saved_match_.username_value;
  form_manager_->PresaveGeneratedPassword(form_with_generated_password);

  EXPECT_TRUE(form_manager_->HasGeneratedPassword());
  EXPECT_TRUE(saved_form.username_value.empty());
  EXPECT_EQ(saved_form.password_value,
            form_data.fields[kPasswordFieldIndex].value);
}

TEST_F(NewPasswordFormManagerTest, UserEventsForGeneration) {
  using GeneratedPasswordStatus =
      PasswordFormMetricsRecorder::GeneratedPasswordStatus;

  PasswordForm submitted_form(parsed_observed_form_);
  submitted_form.form_data = submitted_form_;
  FormData& form_data = submitted_form.form_data;

  {  // User accepts a generated password.
    base::HistogramTester histogram_tester;
    CreateFormManager(observed_form_);
    form_manager_->PresaveGeneratedPassword(submitted_form);
    form_manager_.reset();
    histogram_tester.ExpectUniqueSample(
        "PasswordGeneration.UserDecision",
        GeneratedPasswordStatus::kPasswordAccepted, 1);
  }

  {  // User edits the generated password.
    base::HistogramTester histogram_tester;
    CreateFormManager(observed_form_);
    form_manager_->PresaveGeneratedPassword(submitted_form);
    form_data.fields[kPasswordFieldIndex].value += ASCIIToUTF16("1");
    form_manager_->PresaveGeneratedPassword(submitted_form);
    form_manager_.reset();
    histogram_tester.ExpectUniqueSample(
        "PasswordGeneration.UserDecision",
        GeneratedPasswordStatus::kPasswordEdited, 1);
  }

  {  // User clears the generated password.
    base::HistogramTester histogram_tester;
    CreateFormManager(observed_form_);
    form_manager_->PresaveGeneratedPassword(submitted_form);
    form_data.fields[kPasswordFieldIndex].value += ASCIIToUTF16("2");
    form_manager_->PresaveGeneratedPassword(submitted_form);
    form_manager_->PasswordNoLongerGenerated();
    form_manager_.reset();
    histogram_tester.ExpectUniqueSample(
        "PasswordGeneration.UserDecision",
        GeneratedPasswordStatus::kPasswordDeleted, 1);
  }
}

}  // namespace

}  // namespace  password_manager
