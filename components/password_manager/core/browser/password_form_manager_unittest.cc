// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_manager.h"

#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/possible_username_data.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/browser/vote_uploads_test_matchers.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::AutofillUploadContents;
using autofill::FieldPropertiesFlags;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormSignature;
using autofill::FormStructure;
using autofill::PasswordForm;
using autofill::PasswordFormFillData;
using autofill::PasswordFormGenerationData;
using autofill::ServerFieldType;
using base::ASCIIToUTF16;
using base::TestMockTimeTaskRunner;
using testing::_;
using testing::AllOf;
using testing::Contains;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Mock;
using testing::NiceMock;
using testing::Pointee;
using testing::Return;
using testing::SaveArg;
using testing::SaveArgPointee;
using testing::UnorderedElementsAre;

namespace password_manager {

namespace {

// Indices of username and password fields in the observed form.
const int kUsernameFieldIndex = 1;
const int kPasswordFieldIndex = 2;

MATCHER_P(FormHasUniqueKey, key, "") {
  return ArePasswordFormUniqueKeysEqual(arg, key);
}

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MockPasswordManagerDriver() {}

  ~MockPasswordManagerDriver() override {}

  MOCK_METHOD1(FillPasswordForm, void(const PasswordFormFillData&));
  MOCK_METHOD1(AllowPasswordGenerationForForm, void(const PasswordForm&));
  MOCK_METHOD1(FormEligibleForGenerationFound,
               void(const autofill::PasswordFormGenerationData&));
};

class MockAutofillDownloadManager : public autofill::AutofillDownloadManager {
 public:
  MockAutofillDownloadManager()
      : AutofillDownloadManager(nullptr, &fake_observer) {}

  MOCK_METHOD6(StartUploadRequest,
               bool(const FormStructure&,
                    bool,
                    const autofill::ServerFieldTypeSet&,
                    const std::string&,
                    bool,
                    PrefService*));

 private:
  class StubObserver : public AutofillDownloadManager::Observer {
    void OnLoadedServerPredictions(
        std::string response,
        const std::vector<std::string>& form_signatures) override {}
  };

  StubObserver fake_observer;
  DISALLOW_COPY_AND_ASSIGN(MockAutofillDownloadManager);
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  ~MockPasswordManagerClient() override = default;

  MOCK_CONST_METHOD0(IsIncognito, bool());

  MOCK_METHOD0(GetAutofillDownloadManager,
               autofill::AutofillDownloadManager*());

  MOCK_METHOD0(UpdateFormManagers, void());

  MOCK_METHOD2(AutofillHttpAuth,
               void(const PasswordForm&, const PasswordFormManagerForUI*));

  MOCK_CONST_METHOD0(IsMainFrameSecure, bool());
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
  FormData::IdentityComparator less;
  EXPECT_FALSE(less(expected.form_data, actual.form_data));
  EXPECT_FALSE(less(actual.form_data, expected.form_data));
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

// Create predictions for |form| using field predictions |field_predictions|.
std::map<FormSignature, FormPredictions> CreatePredictions(
    const FormData& form,
    std::vector<std::pair<int, ServerFieldType>> field_predictions) {
  FormPredictions predictions;
  for (const auto& index_prediction : field_predictions) {
    uint32_t renderer_id =
        form.fields[index_prediction.first].unique_renderer_id;
    ServerFieldType server_type = index_prediction.second;
    predictions.fields.emplace_back();

    predictions.fields.back().renderer_id = renderer_id;
    predictions.fields.back().type = server_type;
  }
  FormSignature form_signature = CalculateFormSignature(form);
  return {{form_signature, predictions}};
}

class MockFormSaver : public StubFormSaver {
 public:
  MockFormSaver() = default;

  ~MockFormSaver() override = default;

  // FormSaver:
  MOCK_METHOD1(PermanentlyBlacklist, PasswordForm(PasswordStore::FormDigest));
  MOCK_METHOD3(Save,
               void(PasswordForm pending,
                    const std::vector<const PasswordForm*>& matches,
                    const base::string16& old_password));
  MOCK_METHOD3(Update,
               void(PasswordForm pending,
                    const std::vector<const PasswordForm*>& matches,
                    const base::string16& old_password));
  MOCK_METHOD4(UpdateReplace,
               void(PasswordForm pending,
                    const std::vector<const PasswordForm*>& matches,
                    const base::string16& old_password,
                    const PasswordForm& old_unique_key));
  MOCK_METHOD1(Remove, void(const PasswordForm&));

  std::unique_ptr<FormSaver> Clone() override {
    return std::make_unique<MockFormSaver>();
  }

  // Convenience downcasting method.
  static MockFormSaver& Get(PasswordFormManager* form_manager) {
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
class PasswordFormManagerTest : public testing::Test {
 public:
  PasswordFormManagerTest() : task_runner_(new TestMockTimeTaskRunner) {
    GURL origin = GURL("https://accounts.google.com/a/ServiceLoginAuth");
    GURL action = GURL("https://accounts.google.com/a/ServiceLogin");
    GURL psl_origin = GURL("https://myaccounts.google.com/a/ServiceLoginAuth");
    GURL psl_action = GURL("https://myaccounts.google.com/a/ServiceLogin");

    observed_form_.url = origin;
    observed_form_.action = action;
    observed_form_.name = ASCIIToUTF16("sign-in");
    observed_form_.unique_renderer_id = 1;
    observed_form_.is_form_tag = true;

    observed_form_only_password_fields_ = observed_form_;

    FormFieldData field;
    field.name = ASCIIToUTF16("firstname");
    field.id_attribute = field.name;
    field.name_attribute = field.name;
    field.form_control_type = "text";
    field.unique_renderer_id = 1;
    observed_form_.fields.push_back(field);

    field.name = ASCIIToUTF16("username");
    field.id_attribute = field.name;
    field.name_attribute = field.name;
    field.form_control_type = "text";
    field.unique_renderer_id = 2;
    observed_form_.fields.push_back(field);

    field.name = ASCIIToUTF16("password");
    field.id_attribute = field.name;
    field.name_attribute = field.name;
    field.form_control_type = "password";
    field.unique_renderer_id = 3;
    observed_form_.fields.push_back(field);
    observed_form_only_password_fields_.fields.push_back(field);

    field.name = ASCIIToUTF16("password2");
    field.id_attribute = field.name;
    field.name_attribute = field.name;
    field.form_control_type = "password";
    field.unique_renderer_id = 5;
    observed_form_only_password_fields_.fields.push_back(field);

// On iOS the unique_id member uniquely addresses this field in the DOM.
// This is an ephemeral value which is not guaranteed to be stable across
// page loads. It serves to allow a given field to be found during the
// current navigation.
// TODO(crbug.com/896689): Expand the logic/application of this to other
// platforms and/or merge this concept with |unique_renderer_id|.
#if defined(OS_IOS)
    for (auto& f : observed_form_.fields) {
      f.unique_id = f.id_attribute;
    }
    for (auto& f : observed_form_only_password_fields_.fields) {
      f.unique_id = f.id_attribute;
    }
#endif

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
    saved_match_.scheme = PasswordForm::Scheme::kHtml;

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
    parsed_submitted_form_.form_data = submitted_form_;
    parsed_submitted_form_.username_value =
        submitted_form_.fields[kUsernameFieldIndex].value;
    parsed_submitted_form_.password_value =
        submitted_form_.fields[kPasswordFieldIndex].value;

    EXPECT_CALL(client_, GetAutofillDownloadManager())
        .WillRepeatedly(Return(&mock_autofill_download_manager_));
    ON_CALL(client_, IsMainFrameSecure()).WillByDefault(Return(true));
    ON_CALL(mock_autofill_download_manager_,
            StartUploadRequest(_, _, _, _, _, _))
        .WillByDefault(Return(true));

    fetcher_.reset(new FakeFormFetcher());
    fetcher_->Fetch();

    CreateFormManager(observed_form_);
  }

 protected:
  MockAutofillDownloadManager mock_autofill_download_manager_;
  FormData observed_form_;
  FormData submitted_form_;
  FormData observed_form_only_password_fields_;
  PasswordForm saved_match_;
  PasswordForm psl_saved_match_;
  PasswordForm parsed_observed_form_;
  PasswordForm parsed_submitted_form_;
  MockPasswordManagerClient client_;
  MockPasswordManagerDriver driver_;
  scoped_refptr<TestMockTimeTaskRunner> task_runner_;

  // Define |fetcher_| before |form_manager_|, because the former needs to
  // outlive the latter.
  std::unique_ptr<FakeFormFetcher> fetcher_;
  std::unique_ptr<PasswordFormManager> form_manager_;

  // Creates PasswordFormManager and sets it to |form_manager_|. Along the
  // way a new |fetcher_| is created.
  void CreateFormManager(const FormData& observed_form) {
    form_manager_.reset(new PasswordFormManager(
        &client_, driver_.AsWeakPtr(), observed_form, fetcher_.get(),
        std::make_unique<NiceMock<MockFormSaver>>(), nullptr));
  }

  // Creates PasswordFormManager and sets it to |form_manager_| for
  // |base_auth_observed_form|. Along the way a new |fetcher_| is created.
  void CreateFormManagerForNonWebForm(
      const PasswordForm& base_auth_observed_form) {
    fetcher_->set_scheme(
        PasswordStore::FormDigest(base_auth_observed_form).scheme);
    form_manager_.reset(new PasswordFormManager(
        &client_, PasswordStore::FormDigest(base_auth_observed_form),
        fetcher_.get(), std::make_unique<NiceMock<MockFormSaver>>()));
  }

  void SetNonFederatedAndNotifyFetchCompleted(
      const std::vector<const autofill::PasswordForm*>& non_federated) {
    fetcher_->SetNonFederated(non_federated);
    fetcher_->NotifyFetchCompleted();
  }
};

TEST_F(PasswordFormManagerTest, DoesManage) {
  EXPECT_TRUE(form_manager_->DoesManage(observed_form_, &driver_));
  // Forms on other drivers are not considered managed.
  EXPECT_FALSE(form_manager_->DoesManage(observed_form_, nullptr));
  FormData another_form = observed_form_;
  another_form.is_form_tag = false;
  EXPECT_FALSE(form_manager_->DoesManage(another_form, &driver_));

  // On non-iOS platforms unique_renderer_id is the form identifier.
  another_form = observed_form_;
  another_form.unique_renderer_id = observed_form_.unique_renderer_id + 1;
#if defined(OS_IOS)
  EXPECT_TRUE(form_manager_->DoesManage(another_form, &driver_));
#else
  EXPECT_FALSE(form_manager_->DoesManage(another_form, &driver_));
#endif

  // On iOS platforms form name is the form identifier.
  another_form = observed_form_;
  another_form.name = observed_form_.name + ASCIIToUTF16("1");
#if defined(OS_IOS)
  EXPECT_FALSE(form_manager_->DoesManage(another_form, &driver_));
#else
  EXPECT_TRUE(form_manager_->DoesManage(another_form, &driver_));
#endif
}

TEST_F(PasswordFormManagerTest, DoesManageNoFormTag) {
  observed_form_.is_form_tag = false;
  CreateFormManager(observed_form_);

  FormData another_form = observed_form_;
  // Simulate that new input was added by JavaScript.
  another_form.fields.push_back(FormFieldData());
  EXPECT_TRUE(form_manager_->DoesManage(another_form, &driver_));
  // Forms on other drivers are not considered managed.
  EXPECT_FALSE(form_manager_->DoesManage(another_form, nullptr));
}

TEST_F(PasswordFormManagerTest, Autofill) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());

  CreateFormManager(observed_form_);
  EXPECT_CALL(driver_, FormEligibleForGenerationFound(_)).Times(0);
  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&fill_data));
  CreateFormManager(observed_form_);
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_EQ(observed_form_.url, fill_data.origin);
  EXPECT_FALSE(fill_data.wait_for_username);
  EXPECT_EQ(observed_form_.fields[1].name, fill_data.username_field.name);
  EXPECT_EQ(saved_match_.username_value, fill_data.username_field.value);
  EXPECT_EQ(observed_form_.fields[2].name, fill_data.password_field.name);
  EXPECT_EQ(saved_match_.password_value, fill_data.password_field.value);
}

TEST_F(PasswordFormManagerTest, AutofillNotMoreThan5Times) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());

  EXPECT_CALL(driver_, FillPasswordForm(_));
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  task_runner_->FastForwardUntilNoTasksRemain();
  Mock::VerifyAndClearExpectations(&driver_);

  for (size_t i = 0; i < PasswordFormManager::kMaxTimesAutofill - 1; ++i) {
    EXPECT_CALL(driver_, FillPasswordForm(_));
    form_manager_->Fill();
    Mock::VerifyAndClearExpectations(&driver_);
  }

  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
  form_manager_->Fill();
}

// PasswordFormManager should always send fill data to renderer, even for
// sign-up forms (no "current-password" field, i.e., no password field to fill
// into). However, for sign-up forms, no particular password field should be
// identified for filling. That way, Chrome won't disturb the user by filling
// the sign-up form, but will be able to offer a manual fallback for filling if
// the form was misclassified.
TEST_F(PasswordFormManagerTest, AutofillSignUpForm) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  // Make |observed_form_| to be sign-up form.
  observed_form_.fields.back().autocomplete_attribute = "new-password";

  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&fill_data));

  PasswordFormGenerationData generation_data;
  EXPECT_CALL(driver_, FormEligibleForGenerationFound(_))
      .WillOnce(SaveArg<0>(&generation_data));

  CreateFormManager(observed_form_);
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  task_runner_->FastForwardUntilNoTasksRemain();
  constexpr uint32_t kNoID = FormFieldData::kNotSetFormControlRendererId;
  EXPECT_EQ(kNoID, fill_data.password_field.unique_renderer_id);
  EXPECT_EQ(saved_match_.password_value, fill_data.password_field.value);
#if defined(OS_IOS)
  EXPECT_EQ(ASCIIToUTF16("sign-in"), generation_data.form_name);
  EXPECT_EQ(ASCIIToUTF16("password"), generation_data.new_password_element);
  EXPECT_EQ(base::string16(), generation_data.confirmation_password_element);
#else
  EXPECT_EQ(observed_form_.fields.back().unique_renderer_id,
            generation_data.new_password_renderer_id);
  EXPECT_EQ(kNoID, generation_data.confirmation_password_renderer_id);
#endif
}

// Check that generation signal is sent the the renderer when new password
// fields are marked with autocomplete attribute.
TEST_F(PasswordFormManagerTest, GenerationOnNewAndConfirmPasswordFields) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  // Make |observed_form_| to be sign-up form.
  observed_form_.fields.back().autocomplete_attribute = "new-password";
  const uint32_t new_password_render_id =
      observed_form_.fields.back().unique_renderer_id;
  // Add a confirmation field.
  FormFieldData field;
  const uint32_t confirm_password_render_id = new_password_render_id + 1;
  field.unique_renderer_id = confirm_password_render_id;
  field.form_control_type = "password";
  field.autocomplete_attribute = "new-password";
  observed_form_.fields.push_back(field);

  PasswordFormGenerationData generation_data;
  EXPECT_CALL(driver_, FormEligibleForGenerationFound(_))
      .WillOnce(SaveArg<0>(&generation_data));

  CreateFormManager(observed_form_);
  fetcher_->NotifyFetchCompleted();

  task_runner_->FastForwardUntilNoTasksRemain();
#if defined(OS_IOS)
  EXPECT_EQ(ASCIIToUTF16("sign-in"), generation_data.form_name);
  EXPECT_EQ(ASCIIToUTF16("password"), generation_data.new_password_element);
  EXPECT_EQ(base::string16(), generation_data.confirmation_password_element);
#else
  EXPECT_EQ(new_password_render_id, generation_data.new_password_renderer_id);
  EXPECT_EQ(confirm_password_render_id,
            generation_data.confirmation_password_renderer_id);
#endif
}

TEST_F(PasswordFormManagerTest, AutofillWithBlacklistedMatch) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&fill_data));
  fetcher_->SetNonFederated({&saved_match_});
  fetcher_->SetBlacklisted(true);
  fetcher_->NotifyFetchCompleted();

  task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_EQ(observed_form_.url, fill_data.origin);
  EXPECT_EQ(saved_match_.username_value, fill_data.username_field.value);
  EXPECT_EQ(saved_match_.password_value, fill_data.password_field.value);
}

TEST_F(PasswordFormManagerTest, SetSubmitted) {
  EXPECT_FALSE(form_manager_->is_submitted());
  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));
  EXPECT_TRUE(form_manager_->is_submitted());

  FormData another_form = submitted_form_;
  another_form.name += ASCIIToUTF16("1");
#if !defined(OS_IOS)
  // |another_form| is managed because the same |unique_renderer_id| as
  // |observed_form_|.
  EXPECT_TRUE(
      form_manager_->ProvisionallySave(another_form, &driver_, nullptr));
  EXPECT_TRUE(form_manager_->is_submitted());
#endif
}

TEST_F(PasswordFormManagerTest, SetSubmittedMultipleTimes) {
  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));
  EXPECT_TRUE(form_manager_->is_submitted());

  // Make the submitted form to be invalid password form.
  submitted_form_.fields.clear();

  // Expect that |form_manager_| is still in submitted state because the first
  // time the submited form was valid.
  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));
  EXPECT_TRUE(form_manager_->is_submitted());
  EXPECT_TRUE(form_manager_->GetSubmittedForm());
}

// Tests that when PasswordFormManager receives saved matches it waits for
// server predictions and fills on receving them.
TEST_F(PasswordFormManagerTest, ServerPredictionsWithinDelay) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());

  // Expects no filling on save matches receiving.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});
  Mock::VerifyAndClearExpectations(&driver_);

  std::map<FormSignature, FormPredictions> predictions = CreatePredictions(
      observed_form_, {std::make_pair(2, autofill::PASSWORD)});

  // Expect filling without delay on receiving server predictions.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(1);
  form_manager_->ProcessServerPredictions(predictions);
  Mock::VerifyAndClearExpectations(&driver_);

  // Expect no filling on receving predictions again.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
  form_manager_->ProcessServerPredictions(predictions);
}

// Tests that PasswordFormManager fills after some delay even without
// server predictions.
TEST_F(PasswordFormManagerTest, ServerPredictionsAfterDelay) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());

  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(1);
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});
  // Expect filling after passing filling delay.

  // Simulate passing filling delay.
  task_runner_->FastForwardUntilNoTasksRemain();
  Mock::VerifyAndClearExpectations(&driver_);

  std::map<FormSignature, FormPredictions> predictions = CreatePredictions(
      observed_form_, {std::make_pair(2, autofill::PASSWORD)});

  // Expect filling on receiving server predictions because it was less than
  // kMaxTimesAutofill attempts to fill.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(1);
  form_manager_->ProcessServerPredictions(predictions);
  task_runner_->FastForwardUntilNoTasksRemain();
}

// Tests that filling happens immediately if server predictions are received
// before saved matches.
TEST_F(PasswordFormManagerTest, ServerPredictionsBeforeFetcher) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  // Expect no filling after receiving saved matches from |fetcher_|, since
  // |form_manager| is waiting for server-side predictions.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
  CreateFormManager(observed_form_);

  std::map<FormSignature, FormPredictions> predictions = CreatePredictions(
      observed_form_, {std::make_pair(2, autofill::PASSWORD)});
  form_manager_->ProcessServerPredictions(predictions);
  Mock::VerifyAndClearExpectations(&driver_);

  // Expect filling without delay on receiving server predictions.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(1);
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});
}

// Tests creating pending credentials when the password store is empty.
TEST_F(PasswordFormManagerTest, CreatePendingCredentialsEmptyStore) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->NotifyFetchCompleted();

  const base::Time kNow = base::Time::Now();

  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));

  const PasswordForm& pending_credentials =
      form_manager_->GetPendingCredentials();
  CheckPendingCredentials(parsed_submitted_form_, pending_credentials);
  EXPECT_GE(pending_credentials.date_last_used, kNow);
  EXPECT_EQ(UserAction::kOverrideUsernameAndPassword,
            form_manager_->GetMetricsRecorder()->GetUserAction());
}

// Tests creating pending credentials when new credentials are submitted and the
// store has another credentials saved.
TEST_F(PasswordFormManagerTest, CreatePendingCredentialsNewCredentials) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));
  CheckPendingCredentials(parsed_submitted_form_,
                          form_manager_->GetPendingCredentials());
  EXPECT_EQ(UserAction::kOverrideUsernameAndPassword,
            form_manager_->GetMetricsRecorder()->GetUserAction());
}

// Tests that when submitted credentials are equal to already saved one then
// pending credentials equal to saved match.
TEST_F(PasswordFormManagerTest, CreatePendingCredentialsAlreadySaved) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  submitted_form_.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  submitted_form_.fields[kPasswordFieldIndex].value =
      saved_match_.password_value;

  // Tests that depending on whether we fill on page load or account select that
  // correct user action is recorded. Fill on account select is simulated by
  // pretending we are in incognito mode.
  for (bool is_incognito : {false, true}) {
    EXPECT_CALL(client_, IsIncognito).WillOnce(Return(is_incognito));
    form_manager_->Fill();
    EXPECT_TRUE(
        form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));
    CheckPendingCredentials(/* expected */ saved_match_,
                            form_manager_->GetPendingCredentials());
    EXPECT_EQ(is_incognito ? UserAction::kChoose : UserAction::kNone,
              form_manager_->GetMetricsRecorder()->GetUserAction());
  }
}

// Tests that when submitted credentials are equal to already saved PSL
// credentials.
TEST_F(PasswordFormManagerTest, CreatePendingCredentialsPSLMatchSaved) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  PasswordForm expected = saved_match_;

  saved_match_.origin = GURL("https://m.accounts.google.com/auth");
  saved_match_.signon_realm = "https://m.accounts.google.com/";
  saved_match_.is_public_suffix_match = true;

  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  submitted_form_.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  submitted_form_.fields[kPasswordFieldIndex].value =
      saved_match_.password_value;

  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));
  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
  EXPECT_EQ(UserAction::kChoosePslMatch,
            form_manager_->GetMetricsRecorder()->GetUserAction());
}

// Tests creating pending credentials when new credentials are different only in
// password with already saved one.
TEST_F(PasswordFormManagerTest, CreatePendingCredentialsPasswordOverriden) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  PasswordForm expected = saved_match_;
  expected.password_value += ASCIIToUTF16("1");

  submitted_form_.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  submitted_form_.fields[kPasswordFieldIndex].value = expected.password_value;
  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));
  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
  EXPECT_EQ(UserAction::kOverridePassword,
            form_manager_->GetMetricsRecorder()->GetUserAction());
}

// Tests that when submitted credentials are equal to already saved one then
// pending credentials equal to saved match.
TEST_F(PasswordFormManagerTest, CreatePendingCredentialsUpdate) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = ASCIIToUTF16("strongpassword");
  submitted_form.fields[1].value = ASCIIToUTF16("verystrongpassword");

  PasswordForm expected = saved_match_;
  expected.password_value = ASCIIToUTF16("verystrongpassword");

  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form, &driver_, nullptr));
  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
  EXPECT_EQ(UserAction::kOverridePassword,
            form_manager_->GetMetricsRecorder()->GetUserAction());
}

// Tests creating pending credentials when a change password form is submitted
// and there are multipe saved forms.
TEST_F(PasswordFormManagerTest, CreatePendingCredentialsUpdateMultipleSaved) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  PasswordForm another_saved_match = saved_match_;
  another_saved_match.username_value += ASCIIToUTF16("1");
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_, &another_saved_match});

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = ASCIIToUTF16("strongpassword");
  submitted_form.fields[1].value = ASCIIToUTF16("verystrongpassword");

  PasswordForm expected = saved_match_;
  expected.password_value = ASCIIToUTF16("verystrongpassword");

  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form, &driver_, nullptr));
  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
}

// Tests creating pending credentials when the password field has an empty name.
TEST_F(PasswordFormManagerTest, CreatePendingCredentialsEmptyName) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->NotifyFetchCompleted();

  FormData anonymous_signup = observed_form_;
  // There is an anonymous password field.
  anonymous_signup.fields[2].name.clear();
  anonymous_signup.fields[2].value = ASCIIToUTF16("a password");
  // Mark the password field as new-password.
  std::map<FormSignature, FormPredictions> predictions = CreatePredictions(
      observed_form_, {std::make_pair(2, autofill::ACCOUNT_CREATION_PASSWORD)});

  form_manager_->ProcessServerPredictions(predictions);

  EXPECT_TRUE(
      form_manager_->ProvisionallySave(anonymous_signup, &driver_, nullptr));
  EXPECT_EQ(ASCIIToUTF16("a password"),
            form_manager_->GetPendingCredentials().password_value);
}

// Tests that there is no crash even when the observed form is a not password
// form and the submitted form is password form.
TEST_F(PasswordFormManagerTest, NoCrashOnNonPasswordForm) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  FormData form_without_password_fields = observed_form_;
  // Remove the password field.
  form_without_password_fields.fields.resize(kPasswordFieldIndex);
  CreateFormManager(form_without_password_fields);
  fetcher_->NotifyFetchCompleted();

  FormData submitted_form = observed_form_;
  submitted_form.fields[kUsernameFieldIndex].value = ASCIIToUTF16("username");
  submitted_form.fields[kPasswordFieldIndex].value = ASCIIToUTF16("password");

  // Expect no crash.
  form_manager_->ProvisionallySave(submitted_form, &driver_, nullptr);
}

TEST_F(PasswordFormManagerTest, IsEqualToSubmittedForm) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->NotifyFetchCompleted();

  FormData submitted_form = observed_form_;
  submitted_form.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  submitted_form.fields[kPasswordFieldIndex].value =
      saved_match_.password_value;

  // No submitted form yet.
  EXPECT_FALSE(form_manager_->IsEqualToSubmittedForm(submitted_form));

  ASSERT_TRUE(
      form_manager_->ProvisionallySave(submitted_form, &driver_, nullptr));

  observed_form_.unique_renderer_id += 10;
  observed_form_.fields.clear();

  EXPECT_TRUE(form_manager_->IsEqualToSubmittedForm(observed_form_));

  observed_form_.action = GURL("https://example.com");
  EXPECT_FALSE(form_manager_->IsEqualToSubmittedForm(observed_form_));
}

// Tests that when credentials with a new username (i.e. not saved yet) is
// successfully submitted, then they are saved correctly.
TEST_F(PasswordFormManagerTest, SaveNewCredentials) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  FormData submitted_form = observed_form_;
  base::string16 new_username = saved_match_.username_value + ASCIIToUTF16("1");
  base::string16 new_password = saved_match_.password_value + ASCIIToUTF16("1");
  submitted_form.fields[kUsernameFieldIndex].value = new_username;
  submitted_form.fields[kPasswordFieldIndex].value = new_password;

  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form, &driver_, nullptr));
  EXPECT_TRUE(form_manager_->IsNewLogin());

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());
  PasswordForm saved_form;
  std::vector<const PasswordForm*> best_matches;
  EXPECT_CALL(form_saver, Save(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&saved_form), SaveArg<1>(&best_matches)));
  EXPECT_CALL(client_, UpdateFormManagers());

  form_manager_->Save();

  std::string expected_signon_realm = submitted_form.url.GetOrigin().spec();
  EXPECT_EQ(submitted_form.url, saved_form.origin);
  EXPECT_EQ(expected_signon_realm, saved_form.signon_realm);
  EXPECT_EQ(new_username, saved_form.username_value);
  EXPECT_EQ(new_password, saved_form.password_value);
  EXPECT_TRUE(saved_form.preferred);

  EXPECT_EQ(submitted_form.fields[kUsernameFieldIndex].name,
            saved_form.username_element);
  EXPECT_EQ(submitted_form.fields[kPasswordFieldIndex].name,
            saved_form.password_element);
  EXPECT_EQ(std::vector<const PasswordForm*>{&saved_match_}, best_matches);

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
TEST_F(PasswordFormManagerTest, SavePSLToAlreadySaved) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  SetNonFederatedAndNotifyFetchCompleted({&psl_saved_match_});

  FormData submitted_form = observed_form_;
  // Change
  submitted_form.fields[kUsernameFieldIndex].value =
      psl_saved_match_.username_value;
  submitted_form.fields[kPasswordFieldIndex].value =
      psl_saved_match_.password_value;

  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form, &driver_, nullptr));
  EXPECT_TRUE(form_manager_->IsNewLogin());
  EXPECT_TRUE(form_manager_->IsPendingCredentialsPublicSuffixMatch());

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());
  PasswordForm saved_form;
  std::vector<const PasswordForm*> best_matches;
  EXPECT_CALL(form_saver, Save(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&saved_form), SaveArg<1>(&best_matches)));

  form_manager_->Save();

  EXPECT_EQ(submitted_form.url, saved_form.origin);
  EXPECT_EQ(GetSignonRealm(submitted_form.url), saved_form.signon_realm);
  EXPECT_EQ(saved_form.username_value, psl_saved_match_.username_value);
  EXPECT_EQ(saved_form.password_value, psl_saved_match_.password_value);
  EXPECT_EQ(saved_form.username_element, psl_saved_match_.username_element);
  EXPECT_EQ(saved_form.password_element, psl_saved_match_.password_element);

  EXPECT_TRUE(saved_form.preferred);

  EXPECT_EQ(std::vector<const PasswordForm*>{&psl_saved_match_}, best_matches);
}

// Tests that when credentials with already saved username but with a new
// password are submitted, then the saved password is updated.
TEST_F(PasswordFormManagerTest, OverridePassword) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  FormData submitted_form = observed_form_;
  base::string16 username = saved_match_.username_value;
  base::string16 new_password = saved_match_.password_value + ASCIIToUTF16("1");
  submitted_form.fields[kUsernameFieldIndex].value = username;
  submitted_form.fields[kPasswordFieldIndex].value = new_password;

  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form, &driver_, nullptr));
  EXPECT_FALSE(form_manager_->IsNewLogin());
  EXPECT_TRUE(form_manager_->IsPasswordUpdate());

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());
  PasswordForm updated_form;
  EXPECT_CALL(form_saver, Update(_, ElementsAre(Pointee(saved_match_)),
                                 saved_match_.password_value))
      .WillOnce(SaveArg<0>(&updated_form));

  form_manager_->Save();

  EXPECT_TRUE(ArePasswordFormUniqueKeysEqual(saved_match_, updated_form));
  EXPECT_TRUE(updated_form.preferred);
  EXPECT_EQ(new_password, updated_form.password_value);
}

// Tests that when the user changes password on a change password form then the
// saved password is updated.
TEST_F(PasswordFormManagerTest, UpdatePasswordOnChangePasswordForm) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  CreateFormManager(observed_form_only_password_fields_);
  PasswordForm not_best_saved_match = saved_match_;
  not_best_saved_match.preferred = false;
  PasswordForm saved_match_another_username = saved_match_;
  saved_match_another_username.username_value += ASCIIToUTF16("1");

  SetNonFederatedAndNotifyFetchCompleted(
      {&saved_match_, &not_best_saved_match, &saved_match_another_username});

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = saved_match_.password_value;
  base::string16 new_password = saved_match_.password_value + ASCIIToUTF16("1");
  submitted_form.fields[1].value = new_password;

  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form, &driver_, nullptr));
  EXPECT_FALSE(form_manager_->IsNewLogin());
  EXPECT_TRUE(form_manager_->IsPasswordUpdate());

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());
  PasswordForm updated_form;
  EXPECT_CALL(form_saver,
              Update(_,
                     UnorderedElementsAre(
                         Pointee(saved_match_), Pointee(not_best_saved_match),
                         Pointee(saved_match_another_username)),
                     saved_match_.password_value))
      .WillOnce(SaveArg<0>(&updated_form));

  form_manager_->Save();

  EXPECT_TRUE(ArePasswordFormUniqueKeysEqual(saved_match_, updated_form));
  EXPECT_TRUE(updated_form.preferred);
  EXPECT_EQ(new_password, updated_form.password_value);
}

TEST_F(PasswordFormManagerTest, VotesUploadingOnPasswordUpdate) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());

  for (auto expected_vote :
       {autofill::NEW_PASSWORD, autofill::PROBABLY_NEW_PASSWORD,
        autofill::NOT_NEW_PASSWORD}) {
    SCOPED_TRACE(testing::Message("expected_vote=") << expected_vote);
    CreateFormManager(observed_form_only_password_fields_);
    SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

    FormData submitted_form = observed_form_only_password_fields_;
    submitted_form.fields[0].value = saved_match_.password_value;
    auto new_password = saved_match_.password_value + ASCIIToUTF16("1");
    submitted_form.fields[1].value = new_password;

    EXPECT_TRUE(
        form_manager_->ProvisionallySave(submitted_form, &driver_, nullptr));

    std::map<base::string16, autofill::ServerFieldType> expected_types;
    expected_types[ASCIIToUTF16("password")] = autofill::PASSWORD;
    expected_types[ASCIIToUTF16("password2")] = expected_vote;

    testing::InSequence in_sequence;
    EXPECT_CALL(mock_autofill_download_manager_,
                StartUploadRequest(UploadedAutofillTypesAre(expected_types),
                                   false, _, _, true, nullptr));
    if (expected_vote == autofill::NEW_PASSWORD) {
      // An unrelated |FIRST_USE| vote.
      EXPECT_CALL(mock_autofill_download_manager_,
                  StartUploadRequest(_, _, _, _, _, _));
    }

    if (expected_vote == autofill::NEW_PASSWORD)
      form_manager_->Save();
    else if (expected_vote == autofill::PROBABLY_NEW_PASSWORD)
      form_manager_->OnNoInteraction(true /* is_update */);
    else
      form_manager_->OnNopeUpdateClicked();
    Mock::VerifyAndClearExpectations(&mock_autofill_download_manager_);
  }
}

TEST_F(PasswordFormManagerTest, UpdateUsernameEmptyStore) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->NotifyFetchCompleted();

  form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr);

  base::string16 new_username =
      parsed_submitted_form_.username_value + ASCIIToUTF16("1");
  PasswordForm expected = parsed_submitted_form_;
  expected.username_value = new_username;
  expected.username_element.clear();

  form_manager_->OnUpdateUsernameFromPrompt(new_username);

  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
  EXPECT_TRUE(form_manager_->IsNewLogin());
}

TEST_F(PasswordFormManagerTest, UpdateUsernameToAnotherFieldValue) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->NotifyFetchCompleted();

  base::string16 user_chosen_username = ASCIIToUTF16("user_chosen_username");
  base::string16 automatically_chosen_username =
      ASCIIToUTF16("automatically_chosen_username");
  submitted_form_.fields[0].value = user_chosen_username;
  submitted_form_.fields[1].value = automatically_chosen_username;
  form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr);

  EXPECT_EQ(automatically_chosen_username,
            form_manager_->GetPendingCredentials().username_value);

  form_manager_->OnUpdateUsernameFromPrompt(user_chosen_username);

  EXPECT_EQ(user_chosen_username,
            form_manager_->GetPendingCredentials().username_value);

  FieldTypeMap expected_types = {
      {ASCIIToUTF16("firstname"), autofill::USERNAME},
      {ASCIIToUTF16("password"), autofill::PASSWORD}};
  VoteTypeMap expected_vote_types = {
      {ASCIIToUTF16("firstname"),
       AutofillUploadContents::Field::USERNAME_EDITED}};
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(
          AllOf(UploadedAutofillTypesAre(expected_types),
                HasGenerationVote(false), VoteTypesAre(expected_vote_types)),
          _, Contains(autofill::USERNAME), _, _, nullptr));
  form_manager_->Save();
}

TEST_F(PasswordFormManagerTest, UpdateUsernameToAlreadyExisting) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr);

  base::string16 new_username = saved_match_.username_value;
  base::string16 expected_password = parsed_submitted_form_.password_value;
  PasswordForm expected = saved_match_;
  expected.password_value = expected_password;

  form_manager_->OnUpdateUsernameFromPrompt(new_username);

  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
  EXPECT_FALSE(form_manager_->IsNewLogin());
  EXPECT_TRUE(form_manager_->IsPasswordUpdate());
}

TEST_F(PasswordFormManagerTest, UpdatePasswordValueEmptyStore) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->NotifyFetchCompleted();

  form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr);

  base::string16 new_password =
      parsed_submitted_form_.password_value + ASCIIToUTF16("1");
  PasswordForm expected = parsed_submitted_form_;
  expected.password_value = new_password;
  expected.password_element.clear();

  form_manager_->OnUpdatePasswordFromPrompt(new_password);

  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
  EXPECT_TRUE(form_manager_->IsNewLogin());

  // TODO(https://crbug.com/928690): implement not sending incorrect votes and
  // check that StartUploadRequest is not called.
  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(_, _, _, _, _, _))
      .Times(1);
  form_manager_->Save();
}

TEST_F(PasswordFormManagerTest, UpdatePasswordValueToAlreadyExisting) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  // Emulate submitting form with known username and different password.
  submitted_form_.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr);

  // The user changes password to already saved one.
  base::string16 password = saved_match_.password_value;
  form_manager_->OnUpdatePasswordFromPrompt(password);

  CheckPendingCredentials(saved_match_, form_manager_->GetPendingCredentials());
  EXPECT_FALSE(form_manager_->IsNewLogin());
  EXPECT_FALSE(form_manager_->IsPasswordUpdate());
}

TEST_F(PasswordFormManagerTest, UpdatePasswordValueMultiplePasswordFields) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  FormData form = observed_form_only_password_fields_;

  CreateFormManager(form);
  fetcher_->NotifyFetchCompleted();
  base::string16 password = ASCIIToUTF16("password1");
  base::string16 pin = ASCIIToUTF16("pin");
  form.fields[0].value = password;
  form.fields[1].value = pin;
  form_manager_->ProvisionallySave(form, &driver_, nullptr);

  // Check that a second password field is chosen for saving.
  EXPECT_EQ(pin, form_manager_->GetPendingCredentials().password_value);

  PasswordForm expected = form_manager_->GetPendingCredentials();
  expected.password_value = password;
  expected.password_element = form.fields[0].name;

  // Simulate that the user updates value to save for the first password field.
  form_manager_->OnUpdatePasswordFromPrompt(password);

  // Check that newly created pending credentials are correct.
  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
  EXPECT_TRUE(form_manager_->IsNewLogin());

  // Check that a vote is sent for the field with the value which is chosen by
  // the user.
  std::map<base::string16, ServerFieldType> expected_types;
  expected_types[expected.password_element] = autofill::PASSWORD;

  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(UploadedAutofillTypesAre(expected_types),
                                 false, _, _, true, nullptr));

  // Check that the password which was chosen by the user is saved.
  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());
  PasswordForm saved_form;
  EXPECT_CALL(form_saver, Save(_, _, _)).WillOnce(SaveArg<0>(&saved_form));

  form_manager_->Save();
  CheckPendingCredentials(expected, saved_form);
}

TEST_F(PasswordFormManagerTest, PermanentlyBlacklist) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->NotifyFetchCompleted();

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

  PasswordForm actual_blacklisted_form =
      password_manager_util::MakeNormalizedBlacklistedForm(
          PasswordStore::FormDigest(observed_form_));
  EXPECT_CALL(form_saver,
              PermanentlyBlacklist(PasswordStore::FormDigest(observed_form_)))
      .WillOnce(Return(actual_blacklisted_form));

  form_manager_->PermanentlyBlacklist();
  EXPECT_TRUE(form_manager_->IsBlacklisted());
}

TEST_F(PasswordFormManagerTest, Clone) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->NotifyFetchCompleted();

  // Provisionally save in order to create pending credentials.
  ASSERT_TRUE(
      form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));

  std::unique_ptr<PasswordFormManager> cloned_manager = form_manager_->Clone();

  EXPECT_TRUE(cloned_manager->DoesManage(observed_form_, nullptr));
  EXPECT_TRUE(cloned_manager->GetFormFetcher());
  // Check that |form_fetcher| was cloned.
  EXPECT_NE(form_manager_->GetFormFetcher(), cloned_manager->GetFormFetcher());

  EXPECT_EQ(form_manager_->GetPendingCredentials(),
            cloned_manager->GetPendingCredentials());
  ASSERT_TRUE(cloned_manager->GetSubmittedForm());
  EXPECT_EQ(*form_manager_->GetSubmittedForm(),
            *cloned_manager->GetSubmittedForm());
  EXPECT_TRUE(cloned_manager->is_submitted());
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
TEST_F(PasswordFormManagerTest, RecordReadonlyWhenFilling) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  EXPECT_CALL(driver_, FillPasswordForm(_));
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

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
TEST_F(PasswordFormManagerTest, RecordReadonlyWhenFilling_ParsingFailed) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());

  FormData malformed_form = observed_form_;
  malformed_form.fields.clear();
  CreateFormManager(malformed_form);
  // Only create the recorder after the current form manager is created,
  // otherwise the destruction of the previous one will add unwanted UKM entries
  // in it.
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

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
TEST_F(PasswordFormManagerTest, RecordReadonlyWhenSaving) {
  // The scoped context is needed for the UKM recorder.
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));

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
TEST_F(PasswordFormManagerTest, RecordReadonlyWhenSaving_ParsingFailed) {
  // The scoped context is needed for the UKM recorder.
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  FormData malformed_form = submitted_form_;
  malformed_form.fields.clear();
  EXPECT_FALSE(
      form_manager_->ProvisionallySave(malformed_form, &driver_, nullptr));

  // Destroy the form manager to destroy the UKM recorder it owns. The recorder
  // only records metrics in its destructor.
  form_manager_.reset();

  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::PasswordForm::kEntryName);
  ASSERT_EQ(1u, entries.size());

  EXPECT_FALSE(ParsingSuccessReported(
      entries[0], ukm::builders::PasswordForm::kReadonlyWhenSavingName));
}

TEST_F(PasswordFormManagerTest, PresaveGeneratedPasswordEmptyStore) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  fetcher_->NotifyFetchCompleted();

  EXPECT_FALSE(form_manager_->HasGeneratedPassword());

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

  form_manager_->SetGenerationPopupWasShown(false /* is_manual_generation */);

  // Check that the generated password is presaved.
  PasswordForm saved_form;
  EXPECT_CALL(form_saver, Save(_, IsEmpty(), base::string16()))
      .WillOnce(SaveArg<0>(&saved_form));

  PasswordForm form_with_generated_password = parsed_submitted_form_;
  FormData& form_data = form_with_generated_password.form_data;
  form_manager_->PresaveGeneratedPassword(form_with_generated_password);

  EXPECT_TRUE(form_manager_->HasGeneratedPassword());
  EXPECT_EQ(saved_form.username_value,
            form_data.fields[kUsernameFieldIndex].value);
  EXPECT_EQ(saved_form.password_value,
            form_data.fields[kPasswordFieldIndex].value);

  Mock::VerifyAndClearExpectations(&form_saver);

  // Check that when the generated password is edited, then it's presaved.
  form_with_generated_password.password_value += ASCIIToUTF16("1");
  form_data.fields[kPasswordFieldIndex].value =
      form_with_generated_password.password_value;
  EXPECT_CALL(form_saver,
              UpdateReplace(_, IsEmpty(), ASCIIToUTF16(""),
                            FormHasUniqueKey(form_with_generated_password)))
      .WillOnce(SaveArg<0>(&saved_form));

  form_manager_->PresaveGeneratedPassword(form_with_generated_password);

  EXPECT_TRUE(form_manager_->HasGeneratedPassword());
  EXPECT_EQ(saved_form.username_value,
            form_data.fields[kUsernameFieldIndex].value);
  EXPECT_EQ(saved_form.password_value,
            form_with_generated_password.password_value);

  Mock::VerifyAndClearExpectations(&form_saver);

  // Check UKM metrics.
  form_manager_.reset();
  ExpectedGenerationUKM expected_metrics = {
      base::make_optional(1u) /* shown automatically */,
      1 /* password generated */,
      base::make_optional(1u) /* password modified */};

  CheckPasswordGenerationUKM(test_ukm_recorder, expected_metrics);
}

TEST_F(PasswordFormManagerTest, PresaveGenerated_ModifiedUsername) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  fetcher_->NotifyFetchCompleted();

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

  form_manager_->SetGenerationPopupWasShown(false /* is_manual_generation */);

  // Check that the generated password is presaved.
  PasswordForm saved_form;
  EXPECT_CALL(form_saver, Save(_, _, _)).WillOnce(SaveArg<0>(&saved_form));
  PasswordForm form_with_generated_password = parsed_submitted_form_;
  FormData& form_data = form_with_generated_password.form_data;
  form_manager_->PresaveGeneratedPassword(form_with_generated_password);
  Mock::VerifyAndClearExpectations(&form_saver);

  // Check that when the username is edited, then it's presaved.
  form_with_generated_password.username_value += ASCIIToUTF16("1");
  form_data.fields[kUsernameFieldIndex].value =
      form_with_generated_password.username_value;

  EXPECT_CALL(form_saver, UpdateReplace(_, IsEmpty(), ASCIIToUTF16(""),
                                        FormHasUniqueKey(saved_form)))
      .WillOnce(SaveArg<0>(&saved_form));
  form_manager_->PresaveGeneratedPassword(form_with_generated_password);

  EXPECT_TRUE(form_manager_->HasGeneratedPassword());
  EXPECT_EQ(saved_form.username_value,
            form_with_generated_password.username_value);
  EXPECT_EQ(saved_form.password_value,
            form_with_generated_password.password_value);

  // Check UKM metrics.
  form_manager_.reset();
  ExpectedGenerationUKM expected_metrics = {
      base::make_optional(1u) /* shown automatically */,
      1 /* password generated */,
      base::make_optional(0u) /* password modified */};

  CheckPasswordGenerationUKM(test_ukm_recorder, expected_metrics);
}

TEST_F(PasswordFormManagerTest, GeneratedPasswordWhichIsNotInFormData) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->NotifyFetchCompleted();
  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

  // Create a password form such that |form_data| do not contain the generated
  // password.
  PasswordForm form_with_generated_password;
  form_with_generated_password.form_data = submitted_form_;
  const base::string16 generated_password = ASCIIToUTF16("gen_pw");
  // |password_value| should contain the generated password.
  form_with_generated_password.password_value = generated_password;

  // Check that the generated password is presaved.
  PasswordForm saved_form;
  EXPECT_CALL(form_saver, Save(_, _, _)).WillOnce(SaveArg<0>(&saved_form));

  form_manager_->PresaveGeneratedPassword(form_with_generated_password);
  EXPECT_EQ(submitted_form_.fields[kUsernameFieldIndex].value,
            saved_form.username_value);
  EXPECT_EQ(generated_password, saved_form.password_value);
  EXPECT_TRUE(form_manager_->HasGeneratedPassword());

  // Check that the generated password is saved.
  EXPECT_CALL(form_saver, UpdateReplace(_, IsEmpty(), ASCIIToUTF16(""),
                                        FormHasUniqueKey(saved_form)))
      .WillOnce(SaveArg<0>(&saved_form));
  EXPECT_CALL(client_, UpdateFormManagers());

  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));
  form_manager_->Save();

  EXPECT_EQ(submitted_form_.fields[kUsernameFieldIndex].value,
            saved_form.username_value);
  EXPECT_EQ(generated_password, saved_form.password_value);
}

TEST_F(PasswordFormManagerTest, PresaveGenerationWhenParsingFails) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->NotifyFetchCompleted();
  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

  // Create a password form with empty |form_data|. On this form the form parser
  // should fail.
  PasswordForm form_with_empty_form_data;
  const base::string16 generated_password = ASCIIToUTF16("gen_pw");
  form_with_empty_form_data.password_value = generated_password;

  // Check that nevertheless the generated password is presaved.
  PasswordForm saved_form;
  EXPECT_CALL(form_saver, Save(_, IsEmpty(), base::string16()))
      .WillOnce(SaveArg<0>(&saved_form));
  form_manager_->PresaveGeneratedPassword(form_with_empty_form_data);
  EXPECT_EQ(generated_password, saved_form.password_value);
}

TEST_F(PasswordFormManagerTest, PasswordNoLongerGenerated) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  fetcher_->NotifyFetchCompleted();

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());
  form_manager_->SetGenerationPopupWasShown(true /* is_manual_generation */);

  EXPECT_CALL(form_saver, Save(_, _, _));

  PasswordForm form = parsed_submitted_form_;
  form_manager_->PresaveGeneratedPassword(form);
  Mock::VerifyAndClearExpectations(&form_saver);

  EXPECT_TRUE(form_manager_->HasGeneratedPassword());

  // Check when the user removes the generated password on the page, it is
  // removed from the store.
  EXPECT_CALL(form_saver, Remove(_));
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

TEST_F(PasswordFormManagerTest, PresaveGeneratedPasswordExistingCredential) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

  form_manager_->SetGenerationPopupWasShown(false /* is_manual_generation */);

  // Check that the generated password is presaved.
  PasswordForm saved_form;
  EXPECT_CALL(form_saver, Save(_, _, _)).WillOnce(SaveArg<0>(&saved_form));

  PasswordForm form_with_generated_password = parsed_submitted_form_;
  FormData& form_data = form_with_generated_password.form_data;

  // Check that the generated password is saved with the empty username when
  // there is already a saved credetial with the same username.
  form_data.fields[kUsernameFieldIndex].value = saved_match_.username_value;
  form_manager_->PresaveGeneratedPassword(form_with_generated_password);

  EXPECT_TRUE(form_manager_->HasGeneratedPassword());
  EXPECT_TRUE(saved_form.username_value.empty());
  EXPECT_EQ(form_with_generated_password.password_value,
            saved_form.password_value);
}

TEST_F(PasswordFormManagerTest, UserEventsForGeneration) {
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
    submitted_form.password_value = form_data.fields[kPasswordFieldIndex].value;
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
    submitted_form.password_value = form_data.fields[kPasswordFieldIndex].value;
    form_manager_->PresaveGeneratedPassword(submitted_form);
    form_manager_->PasswordNoLongerGenerated();
    form_manager_.reset();
    histogram_tester.ExpectUniqueSample(
        "PasswordGeneration.UserDecision",
        GeneratedPasswordStatus::kPasswordDeleted, 1);
  }
}

TEST_F(PasswordFormManagerTest, FillForm) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());

  for (bool observed_form_changed : {false, true}) {
    SCOPED_TRACE(testing::Message("observed_form_changed=")
                 << observed_form_changed);
    CreateFormManager(observed_form_);
    EXPECT_CALL(driver_, FillPasswordForm(_));
    SetNonFederatedAndNotifyFetchCompleted({&saved_match_});
    task_runner_->FastForwardUntilNoTasksRemain();
    Mock::VerifyAndClearExpectations(&driver_);

    FormData form = observed_form_;

    if (observed_form_changed) {
      form.fields[kUsernameFieldIndex].unique_renderer_id += 1000;
      form.fields[kUsernameFieldIndex].name += ASCIIToUTF16("1");
      form.fields[kUsernameFieldIndex].id_attribute += ASCIIToUTF16("1");
#if defined(OS_IOS)
      form.fields[kUsernameFieldIndex].unique_id += ASCIIToUTF16("1");
#endif
      form.fields[kPasswordFieldIndex].unique_renderer_id += 1000;
    }

    PasswordFormFillData fill_data;
    EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&fill_data));
    form_manager_->FillForm(form);

    EXPECT_EQ(form.fields[kUsernameFieldIndex].name,
              fill_data.username_field.name);
    EXPECT_EQ(form.fields[kUsernameFieldIndex].unique_renderer_id,
              fill_data.username_field.unique_renderer_id);
    EXPECT_EQ(saved_match_.username_value, fill_data.username_field.value);
    EXPECT_EQ(form.fields[kPasswordFieldIndex].name,
              fill_data.password_field.name);
    EXPECT_EQ(form.fields[kPasswordFieldIndex].unique_renderer_id,
              fill_data.password_field.unique_renderer_id);
    EXPECT_EQ(saved_match_.password_value, fill_data.password_field.value);

    base::HistogramTester histogram_tester;
    form_manager_.reset();
    uint32_t expected_differences_mask = 0;
    if (observed_form_changed)
      expected_differences_mask = 2;  // renderer_id changes.
    histogram_tester.ExpectUniqueSample("PasswordManager.DynamicFormChanges",
                                        expected_differences_mask, 1);
  }
}

TEST_F(PasswordFormManagerTest, FillFormWaitForServerPredictions) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  FormData changed_form = observed_form_;

  changed_form.fields[kUsernameFieldIndex].unique_renderer_id += 1000;
  changed_form.fields[kPasswordFieldIndex].unique_renderer_id += 1000;

  // Check that no filling until server predicions or filling timeout
  // expiration.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
  form_manager_->FillForm(changed_form);
  Mock::VerifyAndClearExpectations(&driver_);

  // Check that the changed form is filled after the filling timeout expires.

  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&fill_data));

  task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(changed_form.fields[kUsernameFieldIndex].unique_renderer_id,
            fill_data.username_field.unique_renderer_id);
  EXPECT_EQ(changed_form.fields[kPasswordFieldIndex].unique_renderer_id,
            fill_data.password_field.unique_renderer_id);

  base::HistogramTester histogram_tester;
  form_manager_.reset();
  uint32_t expected_differences_mask = 2;  // renderer_id changes.
  histogram_tester.ExpectUniqueSample("PasswordManager.DynamicFormChanges",
                                      expected_differences_mask, 1);
}

TEST_F(PasswordFormManagerTest, Update) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());

  PasswordForm not_best_saved_match = saved_match_;
  not_best_saved_match.preferred = false;
  PasswordForm saved_match_another_username = saved_match_;
  saved_match_another_username.username_value += ASCIIToUTF16("1");
  SetNonFederatedAndNotifyFetchCompleted(
      {&saved_match_, &saved_match_another_username});

  FormData submitted_form = observed_form_;
  base::string16 username = saved_match_.username_value;
  base::string16 new_password = saved_match_.password_value + ASCIIToUTF16("1");
  submitted_form.fields[kUsernameFieldIndex].value = username;
  submitted_form.fields[kPasswordFieldIndex].value = new_password;

  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form, &driver_, nullptr));

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());
  PasswordForm updated_form;
  EXPECT_CALL(form_saver, Update(_,
                                 UnorderedElementsAre(
                                     Pointee(saved_match_),
                                     Pointee(saved_match_another_username)),
                                 saved_match_.password_value))
      .WillOnce(SaveArg<0>(&updated_form));
  EXPECT_CALL(client_, UpdateFormManagers());

  const base::Time kNow = base::Time::Now();
  form_manager_->Update(saved_match_);

  EXPECT_TRUE(ArePasswordFormUniqueKeysEqual(saved_match_, updated_form));
  EXPECT_TRUE(updated_form.preferred);
  EXPECT_EQ(new_password, updated_form.password_value);
  EXPECT_GE(updated_form.date_last_used, kNow);
}

// TODO(https://crbug.com/918846): implement FillingAssistance metric on iOS.
#if defined(OS_IOS)
#define MAYBE_FillingAssistanceMetric DISABLED_FillingAssistanceMetric
#else
#define MAYBE_FillingAssistanceMetric FillingAssistanceMetric
#endif
TEST_F(PasswordFormManagerTest, MAYBE_FillingAssistanceMetric) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  // Simulate that the user fills the saved credentials manually.
  submitted_form_.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  submitted_form_.fields[kUsernameFieldIndex].properties_mask =
      FieldPropertiesFlags::AUTOFILLED_ON_USER_TRIGGER;
  submitted_form_.fields[kPasswordFieldIndex].value =
      saved_match_.password_value;
  submitted_form_.fields[kPasswordFieldIndex].properties_mask =
      FieldPropertiesFlags::AUTOFILLED_ON_USER_TRIGGER;

  base::HistogramTester histogram_tester;
  //  Simulate successful submission.
  form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr);
  form_manager_->GetMetricsRecorder()->LogSubmitPassed();

  form_manager_.reset();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.FillingAssistance",
      PasswordFormMetricsRecorder::FillingAssistance::kManual, 1);
}

TEST_F(PasswordFormManagerTest, PasswordRevealedVote) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());

  for (bool password_revealed : {false, true}) {
    SCOPED_TRACE(testing::Message("password_revealed=") << password_revealed);
    CreateFormManager(observed_form_);
    fetcher_->NotifyFetchCompleted();

    EXPECT_TRUE(
        form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));

    if (password_revealed)
      form_manager_->OnPasswordsRevealed();

    EXPECT_CALL(mock_autofill_download_manager_,
                StartUploadRequest(PasswordsWereRevealed(password_revealed),
                                   false, _, _, true, nullptr));
    form_manager_->Save();
    Mock::VerifyAndClearExpectations(&mock_autofill_download_manager_);
  }
}

TEST_F(PasswordFormManagerTest, GenerationUploadOnNoInteraction) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());

  for (bool generation_popup_shown : {false, true}) {
    SCOPED_TRACE(testing::Message("generation_popup_shown=")
                 << generation_popup_shown);
    CreateFormManager(observed_form_);
    fetcher_->NotifyFetchCompleted();

    if (generation_popup_shown) {
      form_manager_->SetGenerationElement(ASCIIToUTF16("password"));
      form_manager_->SetGenerationPopupWasShown(false /*is_manual_generation*/);
    }
    EXPECT_TRUE(
        form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));

    EXPECT_CALL(
        mock_autofill_download_manager_,
        StartUploadRequest(HasGenerationVote(true), false, _, _, true, nullptr))
        .Times(generation_popup_shown ? 1 : 0);
    form_manager_->OnNoInteraction(false /*is_update */);
    Mock::VerifyAndClearExpectations(&mock_autofill_download_manager_);
  }
}

TEST_F(PasswordFormManagerTest, GenerationUploadOnNeverClicked) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());

  for (bool generation_popup_shown : {false, true}) {
    SCOPED_TRACE(testing::Message("generation_popup_shown=")
                 << generation_popup_shown);
    CreateFormManager(observed_form_);
    fetcher_->NotifyFetchCompleted();

    if (generation_popup_shown) {
      form_manager_->SetGenerationElement(ASCIIToUTF16("password"));
      form_manager_->SetGenerationPopupWasShown(false /*is_manual_generation*/);
    }
    EXPECT_TRUE(
        form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));

    EXPECT_CALL(
        mock_autofill_download_manager_,
        StartUploadRequest(HasGenerationVote(true), false, _, _, true, nullptr))
        .Times(generation_popup_shown ? 1 : 0);
    form_manager_->OnNeverClicked();
    Mock::VerifyAndClearExpectations(&mock_autofill_download_manager_);
  }
}

TEST_F(PasswordFormManagerTest, SaveHttpAuthNoHttpAuthStored) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());

  for (bool html_credentials_saved : {false, true}) {
    SCOPED_TRACE(testing::Message("html_credentials_saved=")
                 << html_credentials_saved);
    PasswordForm http_auth_form = parsed_observed_form_;
    http_auth_form.scheme = PasswordForm::Scheme::kBasic;

    // Check that no filling because no http auth credentials are stored.
    EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
    EXPECT_CALL(client_, AutofillHttpAuth(_, _)).Times(0);

    CreateFormManagerForNonWebForm(http_auth_form);
    MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

    std::vector<const PasswordForm*> saved_matches;
    if (html_credentials_saved)
      saved_matches.push_back(&saved_match_);
    SetNonFederatedAndNotifyFetchCompleted(saved_matches);

    base::string16 username = ASCIIToUTF16("user1");
    base::string16 password = ASCIIToUTF16("pass1");
    http_auth_form.username_value = username;
    http_auth_form.password_value = password;

    // Check that submitted credentials are saved.
    ASSERT_TRUE(form_manager_->ProvisionallySaveHttpAuthForm(http_auth_form));
    EXPECT_TRUE(form_manager_->IsNewLogin());

    PasswordForm saved_form;
    EXPECT_CALL(form_saver, Save(_, _, _)).WillOnce(SaveArg<0>(&saved_form));
    form_manager_->Save();

    EXPECT_EQ(http_auth_form.signon_realm, saved_form.signon_realm);
    EXPECT_EQ(username, saved_form.username_value);
    EXPECT_EQ(password, saved_form.password_value);
  }
}

TEST_F(PasswordFormManagerTest, HTTPAuthAlreadySaved) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  PasswordForm http_auth_form = parsed_observed_form_;
  http_auth_form.scheme = PasswordForm::Scheme::kBasic;

  CreateFormManagerForNonWebForm(http_auth_form);

  const base::string16 username = ASCIIToUTF16("user1");
  const base::string16 password = ASCIIToUTF16("pass1");
  http_auth_form.username_value = username;
  http_auth_form.password_value = password;
  EXPECT_CALL(client_, AutofillHttpAuth(http_auth_form, _)).Times(1);
  SetNonFederatedAndNotifyFetchCompleted({&http_auth_form});

  // Check that if known credentials are submitted, then |form_manager_| is not
  // in state new login nor password overridden.
  ASSERT_TRUE(form_manager_->ProvisionallySaveHttpAuthForm(http_auth_form));
  EXPECT_FALSE(form_manager_->IsNewLogin());
  EXPECT_FALSE(form_manager_->IsPasswordUpdate());
}

TEST_F(PasswordFormManagerTest, HTTPAuthPasswordOverridden) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  PasswordForm http_auth_form = parsed_observed_form_;
  http_auth_form.scheme = PasswordForm::Scheme::kBasic;

  CreateFormManagerForNonWebForm(http_auth_form);
  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

  PasswordForm saved_http_auth_form = http_auth_form;
  const base::string16 username = ASCIIToUTF16("user1");
  const base::string16 password = ASCIIToUTF16("pass1");
  saved_http_auth_form.username_value = username;
  saved_http_auth_form.password_value = password;
  EXPECT_CALL(client_, AutofillHttpAuth(saved_http_auth_form, _)).Times(1);
  SetNonFederatedAndNotifyFetchCompleted({&saved_http_auth_form});

  // Check that if new password is submitted, then |form_manager_| is in state
  // password overridden.
  PasswordForm submitted_http_auth_form = saved_http_auth_form;
  base::string16 new_password = password + ASCIIToUTF16("1");
  submitted_http_auth_form.password_value = new_password;
  ASSERT_TRUE(
      form_manager_->ProvisionallySaveHttpAuthForm(submitted_http_auth_form));
  EXPECT_FALSE(form_manager_->IsNewLogin());
  EXPECT_TRUE(form_manager_->IsPasswordUpdate());

  // Check that the password is updated in the stored credential.
  PasswordForm updated_form;
  EXPECT_CALL(form_saver,
              Update(_, ElementsAre(Pointee(saved_http_auth_form)), password))
      .WillOnce(SaveArg<0>(&updated_form));

  form_manager_->Save();

  EXPECT_TRUE(
      ArePasswordFormUniqueKeysEqual(saved_http_auth_form, updated_form));
  EXPECT_EQ(new_password, updated_form.password_value);
}

TEST_F(PasswordFormManagerTest, BlacklistHttpAuthCredentials) {
  PasswordForm http_auth_form = parsed_observed_form_;
  http_auth_form.signon_realm += "my-auth-realm";
  http_auth_form.scheme = PasswordForm::Scheme::kBasic;

  CreateFormManagerForNonWebForm(http_auth_form);
  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

  // Simulate that the user submits http auth credentials.
  http_auth_form.username_value = ASCIIToUTF16("user1");
  http_auth_form.password_value = ASCIIToUTF16("pass1");
  ASSERT_TRUE(form_manager_->ProvisionallySaveHttpAuthForm(http_auth_form));

  // Simulate that the user clicks never.
  PasswordForm blacklisted_form;
  EXPECT_CALL(form_saver,
              PermanentlyBlacklist(PasswordStore::FormDigest(http_auth_form)));
  form_manager_->OnNeverClicked();
}

#if defined(OS_IOS)
TEST_F(PasswordFormManagerTest, iOSPresavedGeneratedPassword) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->NotifyFetchCompleted();
  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

  FormData form_to_presave = observed_form_;
  const base::string16 typed_username = ASCIIToUTF16("user1");
  FormFieldData& username_field = form_to_presave.fields[kUsernameFieldIndex];
  FormFieldData& password_field = form_to_presave.fields[kPasswordFieldIndex];
  username_field.value = typed_username;
  password_field.value = ASCIIToUTF16("not_password");
  // Use |generated_password| different from value in field to test that the
  // generated password is saved.
  const base::string16 generated_password = ASCIIToUTF16("gen_pw");
  // Use different |unique_id| and |name| to test that |unique_id| is taken.
  password_field.unique_id = password_field.name + ASCIIToUTF16("1");
  const base::string16 generation_element = password_field.unique_id;

  PasswordForm saved_form;
  EXPECT_CALL(form_saver, Save(_, IsEmpty(), base::string16()))
      .WillOnce(SaveArg<0>(&saved_form));
  form_manager_->PresaveGeneratedPassword(
      &driver_, form_to_presave, generated_password, generation_element);
  EXPECT_EQ(generated_password, saved_form.password_value);

  Mock::VerifyAndClearExpectations(&form_saver);

  const base::string16 changed_password =
      generated_password + ASCIIToUTF16("1");
  EXPECT_CALL(form_saver, UpdateReplace(_, _, base::string16(), _))
      .WillOnce(SaveArg<0>(&saved_form));

  form_manager_->UpdateGeneratedPasswordOnUserInput(
      form_to_presave.name, generation_element, changed_password);
  EXPECT_EQ(username_field.value, saved_form.username_value);
  EXPECT_EQ(changed_password, saved_form.password_value);
}

TEST_F(PasswordFormManagerTest, UpdateGeneratedPasswordBeforePresaving) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  fetcher_->NotifyFetchCompleted();
  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

  FormData form_to_presave = observed_form_;
  const base::string16 generation_element =
      form_to_presave.fields[kPasswordFieldIndex].unique_id;
  const base::string16 generation_field_value = ASCIIToUTF16("some_password");

  // Check that nothing is saved on changing password, in case when there was no
  // pre-saving.
  EXPECT_CALL(form_saver, Save(_, _, _)).Times(0);
  form_manager_->UpdateGeneratedPasswordOnUserInput(
      form_to_presave.name, generation_element, generation_field_value);
}

#endif  // defined(OS_IOS)

// Tests that username is taken during username first flow.
TEST_F(PasswordFormManagerTest, UsernameFirstFlow) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUsernameFirstFlow);

  CreateFormManager(observed_form_only_password_fields_);
  fetcher_->NotifyFetchCompleted();
  const base::string16 possible_username = ASCIIToUTF16("possible_username");
  PossibleUsernameData possible_username_data(
      saved_match_.signon_realm, 1u /* renderer_id */, possible_username,
      base::Time::Now(), 0 /* driver_id */);

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = ASCIIToUTF16("strongpassword");

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               &possible_username_data));

  // Check that a username is chosen from |possible_username_data|.
  EXPECT_EQ(possible_username,
            form_manager_->GetPendingCredentials().username_value);
}

// Tests that username is not taken when a possible username is not valid.
TEST_F(PasswordFormManagerTest, UsernameFirstFlowDifferentDomains) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUsernameFirstFlow);

  CreateFormManager(observed_form_only_password_fields_);
  fetcher_->NotifyFetchCompleted();
  base::string16 possible_username = ASCIIToUTF16("possible_username");
  PossibleUsernameData possible_username_data(
      "https://another.domain.com", 1u /* renderer_id */, possible_username,
      base::Time::Now(), 0 /* driver_id */);

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = ASCIIToUTF16("strongpassword");

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               &possible_username_data));

  // |possible_username_data| has different domain then |submitted_form|. Check
  // that no username is chosen.
  EXPECT_TRUE(form_manager_->GetPendingCredentials().username_value.empty());
}

// Tests that username is taken during username first flow.
TEST_F(PasswordFormManagerTest, UsernameFirstFlowVotes) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUsernameFirstFlow);

  CreateFormManager(observed_form_only_password_fields_);
  fetcher_->NotifyFetchCompleted();
  const base::string16 possible_username = ASCIIToUTF16("possible_username");
  constexpr uint64_t kUsernameFieldRendererId = 100;
  PossibleUsernameData possible_username_data(
      saved_match_.signon_realm, kUsernameFieldRendererId, possible_username,
      base::Time::Now(), 0 /* driver_id */);

  // Create form predictions and set them to |possible_username_data|.
  FormPredictions predictions;
  constexpr uint64_t kUsernameFormSignature = 1000;
  predictions.form_signature = kUsernameFormSignature;
  PasswordFieldPrediction field_prediction;
  field_prediction.renderer_id = kUsernameFieldRendererId;
  field_prediction.signature = 123;
  field_prediction.type = autofill::SINGLE_USERNAME;
  predictions.fields.push_back(field_prediction);
  possible_username_data.form_predictions = predictions;

  // Simulate submission a form without username. Data from
  // |possible_username_data| will be taken for setting username.
  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = ASCIIToUTF16("strongpassword");

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               &possible_username_data));

  // Check that uploads for both username and password form happen.
  testing::InSequence in_sequence;
  // Upload for the password form.
  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(_, false, _, _, true, nullptr));

  // Upload for the username form.
  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(SignatureIs(kUsernameFormSignature), false, _,
                                 _, true, nullptr));

  form_manager_->Save();
}

}  // namespace

}  // namespace password_manager
