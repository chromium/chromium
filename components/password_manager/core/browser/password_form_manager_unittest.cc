// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_manager.h"

#include <memory>

#include <string>
#include <type_traits>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/gaia_id_hash.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/renderer_id.h"
#include "components/autofill/core/common/signatures.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/field_info_manager.h"
#include "components/password_manager/core/browser/multi_store_password_save_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/possible_username_data.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/browser/vote_uploads_test_matchers.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::AutofillUploadContents;
using autofill::FieldPropertiesFlags;
using autofill::FieldRendererId;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormRendererId;
using autofill::FormSignature;
using autofill::FormStructure;
using autofill::GaiaIdHash;
using autofill::NOT_USERNAME;
using autofill::PasswordFormFillData;
using autofill::PasswordFormGenerationData;
using autofill::ServerFieldType;
using autofill::SINGLE_USERNAME;
using autofill::UNKNOWN_TYPE;
using base::ASCIIToUTF16;
using testing::_;
using testing::AllOf;
using testing::Contains;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Mock;
using testing::NiceMock;
using testing::Pointee;
using testing::Return;
using testing::ReturnRef;
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

MATCHER_P(FormHasUsernameValue, username_value, "") {
  return arg.username_value == username_value;
}

MATCHER_P(FormHasPassword, password_value, "") {
  if (arg.new_password_value.empty())
    return arg.password_value == password_value;
  return arg.new_password_value == password_value;
}

MATCHER_P(FormDataPointeeEqualTo, form_data, "") {
  return autofill::FormDataEqualForTesting(*arg, form_data);
}

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MOCK_METHOD1(FillPasswordForm, void(const PasswordFormFillData&));
  MOCK_METHOD1(AllowPasswordGenerationForForm, void(const PasswordForm&));
  MOCK_METHOD1(FormEligibleForGenerationFound,
               void(const autofill::PasswordFormGenerationData&));
};

class MockAutofillDownloadManager : public autofill::AutofillDownloadManager {
 public:
  MockAutofillDownloadManager()
      : AutofillDownloadManager(nullptr, &fake_observer) {}
  MockAutofillDownloadManager(const MockAutofillDownloadManager&) = delete;
  MockAutofillDownloadManager& operator=(const MockAutofillDownloadManager&) =
      delete;

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
        const std::vector<autofill::FormSignature>& form_signatures) override {}
  };

  StubObserver fake_observer;
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_METHOD(bool, IsIncognito, (), (const, override));
  MOCK_METHOD(autofill::AutofillDownloadManager*,
              GetAutofillDownloadManager,
              (),
              (override));
  MOCK_METHOD(void, UpdateFormManagers, (), (override));
  MOCK_METHOD(void,
              AutofillHttpAuth,
              (const PasswordForm&, const PasswordFormManagerForUI*),
              (override));
  MOCK_METHOD(SyncState, GetPasswordSyncState, (), (const, override));
  MOCK_METHOD(bool, IsCommittedMainFrameSecure, (), (const, override));
  MOCK_METHOD(FieldInfoManager*, GetFieldInfoManager, (), (const, override));
  MOCK_METHOD(signin::IdentityManager*, GetIdentityManager, (), (override));
  MOCK_METHOD(PrefService*, GetPrefs, (), (const, override));
};

void CheckPendingCredentials(const PasswordForm& expected,
                             const PasswordForm& actual) {
  EXPECT_EQ(expected.signon_realm, actual.signon_realm);
  EXPECT_EQ(expected.url, actual.url);
  EXPECT_EQ(expected.action, actual.action);
  EXPECT_EQ(expected.username_value, actual.username_value);
  EXPECT_EQ(expected.password_value, actual.password_value);
  EXPECT_EQ(expected.username_element, actual.username_element);
  EXPECT_EQ(expected.password_element, actual.password_element);
  EXPECT_EQ(expected.blocked_by_user, actual.blocked_by_user);
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
    autofill::FieldRendererId renderer_id =
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
  MockFormSaver(const MockFormSaver&) = delete;
  MockFormSaver& operator=(const MockFormSaver&) = delete;
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
};

class MockFieldInfoManager : public FieldInfoManager {
 public:
  MOCK_METHOD(void,
              AddFieldType,
              (autofill::FormSignature,
               autofill::FieldSignature,
               ServerFieldType),
              (override));
  MOCK_METHOD(ServerFieldType,
              GetFieldType,
              (autofill::FormSignature, autofill::FieldSignature),
              (const override));
};

class PasswordFormManagerTest : public testing::Test,
                                public testing::WithParamInterface<bool> {
 public:
  PasswordFormManagerTest() {
    pref_service_.registry()->RegisterTimePref(
        prefs::kProfileStoreDateLastUsedForFilling, base::Time());
    pref_service_.registry()->RegisterTimePref(
        prefs::kAccountStoreDateLastUsedForFilling, base::Time());

    form_manager_->set_wait_for_server_predictions_for_filling(true);

    GURL origin = GURL("https://accounts.google.com/a/ServiceLoginAuth");
    GURL action = GURL("https://accounts.google.com/a/ServiceLogin");
    GURL psl_origin = GURL("https://myaccounts.google.com/a/ServiceLoginAuth");
    GURL psl_action = GURL("https://myaccounts.google.com/a/ServiceLogin");

    observed_form_.url = origin;
    observed_form_.action = action;
    observed_form_.name = ASCIIToUTF16("sign-in");
    observed_form_.unique_renderer_id = FormRendererId(0);
    observed_form_.is_form_tag = true;

    observed_form_only_password_fields_ = observed_form_;

    FormFieldData field;
    field.name = ASCIIToUTF16("firstname");
    field.id_attribute = field.name;
    field.name_attribute = field.name;
    field.form_control_type = "text";
    field.unique_renderer_id = autofill::FieldRendererId(1);
    observed_form_.fields.push_back(field);

    field.name = ASCIIToUTF16("username");
    field.id_attribute = field.name;
    field.name_attribute = field.name;
    field.form_control_type = "text";
    field.unique_renderer_id = autofill::FieldRendererId(2);
    observed_form_.fields.push_back(field);

    non_password_form_ = observed_form_;

    field.name = ASCIIToUTF16("password");
    field.id_attribute = field.name;
    field.name_attribute = field.name;
    field.form_control_type = "password";
    field.unique_renderer_id = autofill::FieldRendererId(3);
    observed_form_.fields.push_back(field);
    observed_form_only_password_fields_.fields.push_back(field);

    field.name = ASCIIToUTF16("password2");
    field.id_attribute = field.name;
    field.name_attribute = field.name;
    field.form_control_type = "password";
    field.unique_renderer_id = autofill::FieldRendererId(4);
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

    submitted_non_password_form_ = non_password_form_;
    submitted_non_password_form_.fields[kUsernameFieldIndex].value =
        ASCIIToUTF16("user1");

    saved_match_.url = origin;
    saved_match_.action = action;
    saved_match_.signon_realm = "https://accounts.google.com/";
    saved_match_.username_value = ASCIIToUTF16("test@gmail.com");
    saved_match_.username_element = ASCIIToUTF16("field1");
    saved_match_.password_value = ASCIIToUTF16("test1");
    saved_match_.password_element = ASCIIToUTF16("field2");
    saved_match_.is_public_suffix_match = false;
    saved_match_.scheme = PasswordForm::Scheme::kHtml;
    saved_match_.in_store = PasswordForm::Store::kProfileStore;

    psl_saved_match_ = saved_match_;
    psl_saved_match_.url = psl_origin;
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
    ON_CALL(client_, GetPrefs()).WillByDefault(Return(&pref_service_));
    ON_CALL(client_, IsCommittedMainFrameSecure()).WillByDefault(Return(true));
    ON_CALL(*client_.GetPasswordFeatureManager(),
            ShouldShowAccountStorageBubbleUi)
        .WillByDefault(Return(true));
    ON_CALL(mock_autofill_download_manager_,
            StartUploadRequest(_, _, _, _, _, _))
        .WillByDefault(Return(true));
    ON_CALL(*client_.GetPasswordFeatureManager(), GetDefaultPasswordStore)
        .WillByDefault(Return(PasswordForm::Store::kProfileStore));

    fetcher_ = std::make_unique<FakeFormFetcher>();
    fetcher_->Fetch();
  }

  void SetUp() override { CreateFormManager(observed_form_); }

 protected:
  MockAutofillDownloadManager mock_autofill_download_manager_;
  FormData observed_form_;
  FormData submitted_form_;
  FormData observed_form_only_password_fields_;
  FormData non_password_form_;
  FormData submitted_non_password_form_;
  PasswordForm saved_match_;
  PasswordForm psl_saved_match_;
  PasswordForm parsed_observed_form_;
  PasswordForm parsed_submitted_form_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;
  TestingPrefServiceSimple pref_service_;
  MockPasswordManagerClient client_;
  MockPasswordManagerDriver driver_;

  // Define |fetcher_| before |form_manager_|, because the former needs to
  // outlive the latter.
  std::unique_ptr<FakeFormFetcher> fetcher_;
  std::unique_ptr<PasswordFormManager> form_manager_;

  // Creates PasswordFormManager and sets it to |form_manager_|. Along the
  // way a new |fetcher_| is created.
  virtual void CreateFormManager(const FormData& observed_form) {
    auto password_save_manager =
        GetParam() ? std::make_unique<MultiStorePasswordSaveManager>(
                         /*account_form_saver=*/std::make_unique<
                             NiceMock<MockFormSaver>>(),
                         /*account_form_saver=*/std::make_unique<
                             NiceMock<MockFormSaver>>())
                   : std::make_unique<PasswordSaveManagerImpl>(
                         std::make_unique<NiceMock<MockFormSaver>>());

    form_manager_ = std::make_unique<PasswordFormManager>(
        &client_, driver_.AsWeakPtr(), observed_form, fetcher_.get(),
        std::move(password_save_manager), nullptr);
  }

  // Creates PasswordFormManager and sets it to |form_manager_| for
  // |base_auth_observed_form|. Along the way a new |fetcher_| is created.
  virtual void CreateFormManagerForNonWebForm(
      const PasswordForm& base_auth_observed_form) {
    auto password_save_manager =
        GetParam() ? std::make_unique<MultiStorePasswordSaveManager>(
                         /*account_form_saver=*/std::make_unique<
                             NiceMock<MockFormSaver>>(),
                         /*account_form_saver=*/std::make_unique<
                             NiceMock<MockFormSaver>>())
                   : std::make_unique<PasswordSaveManagerImpl>(
                         std::make_unique<NiceMock<MockFormSaver>>());
    fetcher_->set_scheme(
        PasswordStore::FormDigest(base_auth_observed_form).scheme);
    form_manager_ = std::make_unique<PasswordFormManager>(
        &client_, PasswordStore::FormDigest(base_auth_observed_form),
        fetcher_.get(), std::move(password_save_manager));
  }

  void SetNonFederatedAndNotifyFetchCompleted(
      const std::vector<const PasswordForm*>& non_federated) {
    fetcher_->SetNonFederated(non_federated);
    fetcher_->NotifyFetchCompleted();
  }
};

TEST_P(PasswordFormManagerTest, DoesManage) {
  EXPECT_TRUE(form_manager_->DoesManage(observed_form_, &driver_));
  // Forms on other drivers are not considered managed.
  MockPasswordManagerDriver another_driver;
  EXPECT_FALSE(form_manager_->DoesManage(observed_form_, &another_driver));
  FormData another_form = observed_form_;
  another_form.is_form_tag = false;
  EXPECT_FALSE(form_manager_->DoesManage(another_form, &driver_));

  // Unique_renderer_id is the form identifier.
  another_form = observed_form_;
  another_form.unique_renderer_id.value() += 1;
  EXPECT_FALSE(form_manager_->DoesManage(another_form, &driver_));
}

TEST_P(PasswordFormManagerTest, DoesManageNoFormTag) {
  observed_form_.is_form_tag = false;
  CreateFormManager(observed_form_);

  FormData another_form = observed_form_;
  // Simulate that new input was added by JavaScript.
  another_form.fields.emplace_back();
  EXPECT_TRUE(form_manager_->DoesManage(another_form, &driver_));
  // Forms on other drivers are not considered managed.
  EXPECT_FALSE(form_manager_->DoesManage(another_form, nullptr));
}

TEST_P(PasswordFormManagerTest, Autofill) {
  CreateFormManager(observed_form_);
  EXPECT_CALL(driver_, FormEligibleForGenerationFound(_)).Times(0);
  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&fill_data));
  CreateFormManager(observed_form_);
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_EQ(observed_form_.url, fill_data.url);

  // On Android Touch To Fill will prevent autofilling credentials on page load.
#if defined(OS_ANDROID)
  EXPECT_TRUE(fill_data.wait_for_username);
#else
  EXPECT_FALSE(fill_data.wait_for_username);
#endif

  EXPECT_EQ(observed_form_.fields[1].name, fill_data.username_field.name);
  EXPECT_EQ(saved_match_.username_value, fill_data.username_field.value);
  EXPECT_EQ(observed_form_.fields[2].name, fill_data.password_field.name);
  EXPECT_EQ(saved_match_.password_value, fill_data.password_field.value);
}

TEST_P(PasswordFormManagerTest, AutofillNotMoreThan5Times) {
  EXPECT_CALL(driver_, FillPasswordForm(_));
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  task_environment_.FastForwardUntilNoTasksRemain();
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
TEST_P(PasswordFormManagerTest, AutofillSignUpForm) {
  // Make |observed_form_| to be sign-up form.
  observed_form_.fields.back().autocomplete_attribute = "new-password";

  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&fill_data));

  PasswordFormGenerationData generation_data;
  EXPECT_CALL(driver_, FormEligibleForGenerationFound(_))
      .WillOnce(SaveArg<0>(&generation_data));

  CreateFormManager(observed_form_);
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(fill_data.password_field.unique_renderer_id.is_null());
  EXPECT_EQ(saved_match_.password_value, fill_data.password_field.value);
#if defined(OS_IOS)
  EXPECT_EQ(observed_form_.unique_renderer_id,
            generation_data.form_renderer_id);
#else
  EXPECT_EQ(observed_form_.fields.back().unique_renderer_id,
            generation_data.new_password_renderer_id);
  EXPECT_TRUE(generation_data.confirmation_password_renderer_id.is_null());
#endif
}

// Check that generation signal is sent the the renderer when new password
// fields are marked with autocomplete attribute.
TEST_P(PasswordFormManagerTest, GenerationOnNewAndConfirmPasswordFields) {
  // Make |observed_form_| to be sign-up form.
  observed_form_.fields.back().autocomplete_attribute = "new-password";
  const autofill::FieldRendererId new_password_render_id =
      observed_form_.fields.back().unique_renderer_id;
  // Add a confirmation field.
  FormFieldData field;
  const autofill::FieldRendererId confirm_password_render_id(
      new_password_render_id.value() + 1);
  field.unique_renderer_id = confirm_password_render_id;
  field.form_control_type = "password";
  field.autocomplete_attribute = "new-password";
  observed_form_.fields.push_back(field);

  PasswordFormGenerationData generation_data;
  EXPECT_CALL(driver_, FormEligibleForGenerationFound(_))
      .WillOnce(SaveArg<0>(&generation_data));

  CreateFormManager(observed_form_);
  fetcher_->NotifyFetchCompleted();

  task_environment_.FastForwardUntilNoTasksRemain();
#if defined(OS_IOS)
  EXPECT_EQ(observed_form_.unique_renderer_id,
            generation_data.form_renderer_id);
#else
  EXPECT_EQ(new_password_render_id, generation_data.new_password_renderer_id);
  EXPECT_EQ(confirm_password_render_id,
            generation_data.confirmation_password_renderer_id);
#endif
}

TEST_P(PasswordFormManagerTest, AutofillWithBlacklistedMatch) {
  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&fill_data));
  fetcher_->SetNonFederated({&saved_match_});
  fetcher_->SetBlacklisted(true);
  fetcher_->NotifyFetchCompleted();

  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_EQ(observed_form_.url, fill_data.url);
  EXPECT_EQ(saved_match_.username_value, fill_data.username_field.value);
  EXPECT_EQ(saved_match_.password_value, fill_data.password_field.value);
}

TEST_P(PasswordFormManagerTest, SetSubmitted) {
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

TEST_P(PasswordFormManagerTest, SetSubmittedMultipleTimes) {
  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));
  EXPECT_TRUE(form_manager_->is_submitted());

  // Make the submitted form to be invalid password form.
  submitted_form_.fields.clear();

  EXPECT_FALSE(
      form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));
  EXPECT_FALSE(form_manager_->is_submitted());
  EXPECT_FALSE(form_manager_->GetSubmittedForm());
  EXPECT_EQ(PasswordForm(), form_manager_->GetPendingCredentials());
}

TEST_P(PasswordFormManagerTest, ResetState) {
  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));
  EXPECT_TRUE(form_manager_->is_submitted());
  EXPECT_TRUE(form_manager_->GetSubmittedForm());
  EXPECT_NE(PasswordForm(), form_manager_->GetPendingCredentials());

  form_manager_->ResetState();

  EXPECT_FALSE(form_manager_->is_submitted());
  EXPECT_FALSE(form_manager_->GetSubmittedForm());
  EXPECT_EQ(PasswordForm(), form_manager_->GetPendingCredentials());
}

// Tests that when PasswordFormManager receives saved matches it waits for
// server predictions and fills on receiving them.
TEST_P(PasswordFormManagerTest, ServerPredictionsWithinDelay) {
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

  // Expect no filling on receiving predictions again.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
  form_manager_->ProcessServerPredictions(predictions);
}

// Tests that PasswordFormManager fills after some delay even without
// server predictions.
TEST_P(PasswordFormManagerTest, ServerPredictionsAfterDelay) {
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(1);
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});
  // Expect filling after passing filling delay.

  // Simulate passing filling delay.
  task_environment_.FastForwardUntilNoTasksRemain();
  Mock::VerifyAndClearExpectations(&driver_);

  std::map<FormSignature, FormPredictions> predictions = CreatePredictions(
      observed_form_, {std::make_pair(2, autofill::PASSWORD)});

  // Expect filling on receiving server predictions because it was less than
  // kMaxTimesAutofill attempts to fill.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(1);
  form_manager_->ProcessServerPredictions(predictions);
  task_environment_.FastForwardUntilNoTasksRemain();
}

// Tests that filling happens immediately if server predictions are received
// before saved matches.
TEST_P(PasswordFormManagerTest, ServerPredictionsBeforeFetcher) {
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
TEST_P(PasswordFormManagerTest, CreatePendingCredentialsEmptyStore) {
  fetcher_->NotifyFetchCompleted();

  const base::Time kNow = base::Time::Now();

  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));

  const PasswordForm& pending_credentials =
      form_manager_->GetPendingCredentials();
  CheckPendingCredentials(parsed_submitted_form_, pending_credentials);
  EXPECT_GE(pending_credentials.date_last_used, kNow);
}

// Tests creating pending credentials when fetch completed
TEST_P(PasswordFormManagerTest, CreatePendingCredentialsWhenFetchCompleted) {
  form_manager_->set_wait_for_server_predictions_for_filling(false);
  form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr);
  SetNonFederatedAndNotifyFetchCompleted({&parsed_submitted_form_});
  EXPECT_FALSE(form_manager_->IsNewLogin());
}

// Tests creating pending credentials when new credentials are submitted and the
// store has another credentials saved.
TEST_P(PasswordFormManagerTest, CreatePendingCredentialsNewCredentials) {
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));
  CheckPendingCredentials(parsed_submitted_form_,
                          form_manager_->GetPendingCredentials());
}

// Tests that when submitted credentials are equal to already saved one then
// pending credentials equal to saved match.
TEST_P(PasswordFormManagerTest, CreatePendingCredentialsAlreadySaved) {
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
  }
}

// Tests that when submitted credentials are equal to already saved PSL
// credentials.
TEST_P(PasswordFormManagerTest, CreatePendingCredentialsPSLMatchSaved) {
  PasswordForm expected = saved_match_;

  saved_match_.url = GURL("https://m.accounts.google.com/auth");
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
}

// Tests creating pending credentials when new credentials are different only in
// password with already saved one.
TEST_P(PasswordFormManagerTest, CreatePendingCredentialsPasswordOverridden) {
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  PasswordForm expected = saved_match_;
  expected.password_value += ASCIIToUTF16("1");

  submitted_form_.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  submitted_form_.fields[kPasswordFieldIndex].value = expected.password_value;
  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));
  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
}

// Tests that when submitted credentials are equal to already saved one then
// pending credentials equal to saved match.
TEST_P(PasswordFormManagerTest, CreatePendingCredentialsUpdate) {
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = ASCIIToUTF16("strongpassword");
  submitted_form.fields[1].value = ASCIIToUTF16("verystrongpassword");

  PasswordForm expected = saved_match_;
  expected.password_value = ASCIIToUTF16("verystrongpassword");

  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form, &driver_, nullptr));
  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
}

// Tests creating pending credentials when a change password form is submitted
// and there are multiple saved forms.
TEST_P(PasswordFormManagerTest, CreatePendingCredentialsUpdateMultipleSaved) {
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
TEST_P(PasswordFormManagerTest, CreatePendingCredentialsEmptyName) {
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
TEST_P(PasswordFormManagerTest, NoCrashOnNonPasswordForm) {
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

TEST_P(PasswordFormManagerTest, IsEqualToSubmittedForm) {
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

  observed_form_.unique_renderer_id.value() += 10;
  observed_form_.fields.clear();

  EXPECT_TRUE(form_manager_->IsEqualToSubmittedForm(observed_form_));

  observed_form_.action = GURL("https://example.com");
  EXPECT_FALSE(form_manager_->IsEqualToSubmittedForm(observed_form_));
}

// Tests that when credentials with a new username (i.e. not saved yet) is
// successfully submitted, then they are saved correctly.
TEST_P(PasswordFormManagerTest, SaveNewCredentials) {
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
  EXPECT_EQ(submitted_form.url, saved_form.url);
  EXPECT_EQ(expected_signon_realm, saved_form.signon_realm);
  EXPECT_EQ(new_username, saved_form.username_value);
  EXPECT_EQ(new_password, saved_form.password_value);

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
TEST_P(PasswordFormManagerTest, SavePSLToAlreadySaved) {
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

  EXPECT_EQ(submitted_form.url, saved_form.url);
  EXPECT_EQ(GetSignonRealm(submitted_form.url), saved_form.signon_realm);
  EXPECT_EQ(saved_form.username_value, psl_saved_match_.username_value);
  EXPECT_EQ(saved_form.password_value, psl_saved_match_.password_value);
  EXPECT_EQ(saved_form.username_element, psl_saved_match_.username_element);
  EXPECT_EQ(saved_form.password_element, psl_saved_match_.password_element);

  EXPECT_EQ(std::vector<const PasswordForm*>{&psl_saved_match_}, best_matches);
}

// Tests that when credentials with already saved username but with a new
// password are submitted, then the saved password is updated.
TEST_P(PasswordFormManagerTest, OverridePassword) {
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
  EXPECT_EQ(new_password, updated_form.password_value);
}

// Tests that when the user changes password on a change password form then the
// saved password is updated.
TEST_P(PasswordFormManagerTest, UpdatePasswordOnChangePasswordForm) {
  CreateFormManager(observed_form_only_password_fields_);
  PasswordForm not_best_saved_match = saved_match_;
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
  EXPECT_EQ(new_password, updated_form.password_value);
}

TEST_P(PasswordFormManagerTest, VotesUploadingOnPasswordUpdate) {
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

TEST_P(PasswordFormManagerTest, UpdateUsernameEmptyStore) {
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

TEST_P(PasswordFormManagerTest, UpdateUsernameToAnotherFieldValue) {
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

TEST_P(PasswordFormManagerTest, UpdateUsernameToAlreadyExisting) {
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

TEST_P(PasswordFormManagerTest, UpdatePasswordValueEmptyStore) {
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

TEST_P(PasswordFormManagerTest, UpdatePasswordValueToAlreadyExisting) {
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

TEST_P(PasswordFormManagerTest, UpdatePasswordValueMultiplePasswordFields) {
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

TEST_P(PasswordFormManagerTest, PermanentlyBlacklist) {
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

TEST_P(PasswordFormManagerTest, Clone) {
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
TEST_P(PasswordFormManagerTest, RecordReadonlyWhenFilling) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  EXPECT_CALL(driver_, FillPasswordForm(_));
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  task_environment_.FastForwardUntilNoTasksRemain();

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
TEST_P(PasswordFormManagerTest, RecordReadonlyWhenFilling_ParsingFailed) {
  FormData malformed_form = observed_form_;
  malformed_form.fields.clear();
  CreateFormManager(malformed_form);
  // Only create the recorder after the current form manager is created,
  // otherwise the destruction of the previous one will add unwanted UKM entries
  // in it.
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  task_environment_.FastForwardUntilNoTasksRemain();

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
TEST_P(PasswordFormManagerTest, RecordReadonlyWhenSaving) {
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
TEST_P(PasswordFormManagerTest, RecordReadonlyWhenSaving_ParsingFailed) {
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

TEST_P(PasswordFormManagerTest, PresaveGeneratedPasswordEmptyStore) {
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
  form_manager_->PresaveGeneratedPassword(
      form_data, form_with_generated_password.password_value);

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

  form_manager_->PresaveGeneratedPassword(
      form_with_generated_password.form_data,
      form_with_generated_password.password_value);

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

TEST_P(PasswordFormManagerTest, PresaveGenerated_ModifiedUsername) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  fetcher_->NotifyFetchCompleted();

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

  form_manager_->SetGenerationPopupWasShown(false /* is_manual_generation */);

  // Check that the generated password is presaved.
  PasswordForm saved_form;
  EXPECT_CALL(form_saver, Save(_, _, _)).WillOnce(SaveArg<0>(&saved_form));
  PasswordForm form_with_generated_password = parsed_submitted_form_;
  FormData& form_data = form_with_generated_password.form_data;
  form_manager_->PresaveGeneratedPassword(
      form_with_generated_password.form_data,
      form_with_generated_password.password_value);
  Mock::VerifyAndClearExpectations(&form_saver);

  // Check that when the username is edited, then it's presaved.
  form_with_generated_password.username_value += ASCIIToUTF16("1");
  form_data.fields[kUsernameFieldIndex].value =
      form_with_generated_password.username_value;

  EXPECT_CALL(form_saver, UpdateReplace(_, IsEmpty(), ASCIIToUTF16(""),
                                        FormHasUniqueKey(saved_form)))
      .WillOnce(SaveArg<0>(&saved_form));
  form_manager_->PresaveGeneratedPassword(
      form_with_generated_password.form_data,
      form_with_generated_password.password_value);

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

TEST_P(PasswordFormManagerTest, GeneratedPasswordWhichIsNotInFormData) {
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

  form_manager_->PresaveGeneratedPassword(
      form_with_generated_password.form_data,
      form_with_generated_password.password_value);
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

TEST_P(PasswordFormManagerTest, PresaveGenerationWhenParsingFails) {
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
  form_manager_->PresaveGeneratedPassword(
      form_with_empty_form_data.form_data,
      form_with_empty_form_data.password_value);
  EXPECT_EQ(generated_password, saved_form.password_value);
}

TEST_P(PasswordFormManagerTest, PasswordNoLongerGenerated) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  fetcher_->NotifyFetchCompleted();

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());
  form_manager_->SetGenerationPopupWasShown(true /* is_manual_generation */);

  EXPECT_CALL(form_saver, Save(_, _, _));

  PasswordForm form = parsed_submitted_form_;
  form_manager_->PresaveGeneratedPassword(form.form_data, form.password_value);
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

TEST_P(PasswordFormManagerTest, PresaveGeneratedPasswordExistingCredential) {
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
  form_manager_->PresaveGeneratedPassword(
      form_with_generated_password.form_data,
      form_with_generated_password.password_value);

  EXPECT_TRUE(form_manager_->HasGeneratedPassword());
  EXPECT_TRUE(saved_form.username_value.empty());
  EXPECT_EQ(form_with_generated_password.password_value,
            saved_form.password_value);
}

TEST_P(PasswordFormManagerTest, UserEventsForGeneration) {
  using GeneratedPasswordStatus =
      PasswordFormMetricsRecorder::GeneratedPasswordStatus;

  PasswordForm submitted_form(parsed_observed_form_);
  submitted_form.form_data = submitted_form_;
  FormData& form_data = submitted_form.form_data;

  {  // User accepts a generated password.
    base::HistogramTester histogram_tester;
    CreateFormManager(observed_form_);
    form_manager_->PresaveGeneratedPassword(submitted_form.form_data,
                                            submitted_form.password_value);
    form_manager_.reset();
    histogram_tester.ExpectUniqueSample(
        "PasswordGeneration.UserDecision",
        GeneratedPasswordStatus::kPasswordAccepted, 1);
  }

  {  // User edits the generated password.
    base::HistogramTester histogram_tester;
    CreateFormManager(observed_form_);
    form_manager_->PresaveGeneratedPassword(submitted_form.form_data,
                                            submitted_form.password_value);
    form_data.fields[kPasswordFieldIndex].value += ASCIIToUTF16("1");
    submitted_form.password_value = form_data.fields[kPasswordFieldIndex].value;
    form_manager_->PresaveGeneratedPassword(submitted_form.form_data,
                                            submitted_form.password_value);
    form_manager_.reset();
    histogram_tester.ExpectUniqueSample(
        "PasswordGeneration.UserDecision",
        GeneratedPasswordStatus::kPasswordEdited, 1);
  }

  {  // User clears the generated password.
    base::HistogramTester histogram_tester;
    CreateFormManager(observed_form_);
    form_manager_->PresaveGeneratedPassword(submitted_form.form_data,
                                            submitted_form.password_value);
    form_data.fields[kPasswordFieldIndex].value += ASCIIToUTF16("2");
    submitted_form.password_value = form_data.fields[kPasswordFieldIndex].value;
    form_manager_->PresaveGeneratedPassword(submitted_form.form_data,
                                            submitted_form.password_value);
    form_manager_->PasswordNoLongerGenerated();
    form_manager_.reset();
    histogram_tester.ExpectUniqueSample(
        "PasswordGeneration.UserDecision",
        GeneratedPasswordStatus::kPasswordDeleted, 1);
  }
}

TEST_P(PasswordFormManagerTest, FillForm) {
  for (bool observed_form_changed : {false, true}) {
    SCOPED_TRACE(testing::Message("observed_form_changed=")
                 << observed_form_changed);
    CreateFormManager(observed_form_);
    EXPECT_CALL(driver_, FillPasswordForm(_));
    SetNonFederatedAndNotifyFetchCompleted({&saved_match_});
    task_environment_.FastForwardUntilNoTasksRemain();
    Mock::VerifyAndClearExpectations(&driver_);

    FormData form = observed_form_;

    if (observed_form_changed) {
      form.fields[kUsernameFieldIndex].unique_renderer_id.value() += 1000;
      form.fields[kUsernameFieldIndex].name += ASCIIToUTF16("1");
      form.fields[kUsernameFieldIndex].id_attribute += ASCIIToUTF16("1");
#if defined(OS_IOS)
      form.fields[kUsernameFieldIndex].unique_id += ASCIIToUTF16("1");
#endif
      form.fields[kPasswordFieldIndex].unique_renderer_id.value() += 1000;
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

TEST_P(PasswordFormManagerTest, FillFormWaitForServerPredictions) {
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  FormData changed_form = observed_form_;

  changed_form.fields[kUsernameFieldIndex].unique_renderer_id.value() += 1000;
  changed_form.fields[kPasswordFieldIndex].unique_renderer_id.value() += 1000;

  // Check that no filling until server predicions or filling timeout
  // expiration.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
  form_manager_->FillForm(changed_form);
  Mock::VerifyAndClearExpectations(&driver_);

  // Check that the changed form is filled after the filling timeout expires.

  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&fill_data));

  task_environment_.FastForwardUntilNoTasksRemain();
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

TEST_P(PasswordFormManagerTest, Update) {
  PasswordForm not_best_saved_match = saved_match_;
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
  EXPECT_EQ(new_password, updated_form.password_value);
  EXPECT_GE(updated_form.date_last_used, kNow);
}

TEST_P(PasswordFormManagerTest, FillingAssistanceMetric) {
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  // Simulate that the user fills the saved credentials manually.
  submitted_form_.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  submitted_form_.fields[kUsernameFieldIndex].properties_mask =
      FieldPropertiesFlags::kAutofilledOnUserTrigger;
  submitted_form_.fields[kPasswordFieldIndex].value =
      saved_match_.password_value;
  submitted_form_.fields[kPasswordFieldIndex].properties_mask =
      FieldPropertiesFlags::kAutofilledOnUserTrigger;

  base::HistogramTester histogram_tester;
  //  Simulate successful submission.
  form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr);
  form_manager_->GetMetricsRecorder()->LogSubmitPassed();

  form_manager_.reset();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.FillingAssistance",
      PasswordFormMetricsRecorder::FillingAssistance::kManual, 1);
}

TEST_P(PasswordFormManagerTest, PasswordRevealedVote) {
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

TEST_P(PasswordFormManagerTest, GenerationUploadOnNoInteraction) {
  for (bool generation_popup_shown : {false, true}) {
    SCOPED_TRACE(testing::Message("generation_popup_shown=")
                 << generation_popup_shown);
    CreateFormManager(observed_form_);
    fetcher_->NotifyFetchCompleted();

    if (generation_popup_shown) {
      form_manager_->SetGenerationElement(FieldRendererId(3));
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

TEST_P(PasswordFormManagerTest, GenerationUploadOnNeverClicked) {
  for (bool generation_popup_shown : {false, true}) {
    SCOPED_TRACE(testing::Message("generation_popup_shown=")
                 << generation_popup_shown);
    CreateFormManager(observed_form_);
    fetcher_->NotifyFetchCompleted();

    if (generation_popup_shown) {
      form_manager_->SetGenerationElement(FieldRendererId(3));
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

TEST_P(PasswordFormManagerTest, SaveHttpAuthNoHttpAuthStored) {
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

TEST_P(PasswordFormManagerTest, HTTPAuthAlreadySaved) {
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

TEST_P(PasswordFormManagerTest, HTTPAuthPasswordOverridden) {
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

TEST_P(PasswordFormManagerTest, BlacklistHttpAuthCredentials) {
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
TEST_P(PasswordFormManagerTest, iOSPresavedGeneratedPassword) {
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
  FieldRendererId generation_element = password_field.unique_renderer_id;

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

  form_manager_->UpdateStateOnUserInput(form_to_presave.unique_renderer_id,
                                        generation_element, changed_password);
  EXPECT_EQ(username_field.value, saved_form.username_value);
  EXPECT_EQ(changed_password, saved_form.password_value);
}

TEST_P(PasswordFormManagerTest, iOSUpdateStateWithoutPresaving) {
  fetcher_->NotifyFetchCompleted();
  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

  FieldRendererId password_field =
      observed_form_.fields[kPasswordFieldIndex].unique_renderer_id;
  const base::string16 new_field_value = ASCIIToUTF16("some_password");

  // Check that nothing is saved on changing password, in case when there was no
  // pre-saving.
  EXPECT_CALL(form_saver, Save(_, _, _)).Times(0);
  EXPECT_TRUE(form_manager_->UpdateStateOnUserInput(
      observed_form_.unique_renderer_id, password_field, new_field_value));

  EXPECT_EQ(new_field_value,
            form_manager_->observed_form()->fields[kPasswordFieldIndex].value);
}

TEST_P(PasswordFormManagerTest, iOSUsingFieldDataManagerData) {
  CreateFormManager(observed_form_);

  auto field_data_manager = base::MakeRefCounted<autofill::FieldDataManager>();
  field_data_manager->UpdateFieldDataMap(
      observed_form_.fields[1].unique_renderer_id,
      base::UTF8ToUTF16("typed_username"), FieldPropertiesFlags::kUserTyped);
  field_data_manager->UpdateFieldDataWithAutofilledValue(
      observed_form_.fields[2].unique_renderer_id,
      base::UTF8ToUTF16("autofilled_pw"),
      FieldPropertiesFlags::kAutofilledOnPageLoad);

  form_manager_->UpdateObservedFormDataWithFieldDataManagerInfo(
      field_data_manager.get());

  EXPECT_EQ(form_manager_->observed_form()->fields[1].typed_value,
            base::UTF8ToUTF16("typed_username"));
  EXPECT_EQ(form_manager_->observed_form()->fields[1].properties_mask,
            FieldPropertiesFlags::kUserTyped);

  EXPECT_EQ(form_manager_->observed_form()->fields[2].value,
            base::UTF8ToUTF16("autofilled_pw"));
  EXPECT_EQ(form_manager_->observed_form()->fields[2].properties_mask,
            FieldPropertiesFlags::kAutofilledOnPageLoad);
}

#endif  // defined(OS_IOS)

// Tests that username is taken during username first flow.
TEST_P(PasswordFormManagerTest, UsernameFirstFlow) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUsernameFirstFlow);

  CreateFormManager(observed_form_only_password_fields_);
  fetcher_->NotifyFetchCompleted();
  const base::string16 possible_username = ASCIIToUTF16("possible_username");
  PossibleUsernameData possible_username_data(
      saved_match_.signon_realm, autofill::FieldRendererId(1),
      possible_username, base::Time::Now(), 0 /* driver_id */);

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = ASCIIToUTF16("strongpassword");

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               &possible_username_data));

#if !defined(OS_ANDROID)
  // Check that a username is chosen from |possible_username_data|.
  EXPECT_EQ(possible_username,
            form_manager_->GetPendingCredentials().username_value);
#else
  // Local heuristics on Android for username first flow are not supported, so
  // the username should not be taken from the username form.
  EXPECT_TRUE(form_manager_->GetPendingCredentials().username_value.empty());
#endif  // !defined(OS_ANDROID)
}

// Tests that username is not taken when a possible username is not valid.
TEST_P(PasswordFormManagerTest, UsernameFirstFlowDifferentDomains) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUsernameFirstFlow);

  CreateFormManager(observed_form_only_password_fields_);
  fetcher_->NotifyFetchCompleted();
  base::string16 possible_username = ASCIIToUTF16("possible_username");
  PossibleUsernameData possible_username_data(
      "https://another.domain.com", autofill::FieldRendererId(1u),
      possible_username, base::Time::Now(), 0 /* driver_id */);

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = ASCIIToUTF16("strongpassword");

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               &possible_username_data));

  // |possible_username_data| has different domain then |submitted_form|. Check
  // that no username is chosen.
  EXPECT_TRUE(form_manager_->GetPendingCredentials().username_value.empty());
}

// Tests that username is taken during username first flow.
TEST_P(PasswordFormManagerTest, UsernameFirstFlowVotes) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUsernameFirstFlow);

  CreateFormManager(observed_form_only_password_fields_);
  fetcher_->NotifyFetchCompleted();
  const base::string16 possible_username = ASCIIToUTF16("possible_username");
  constexpr autofill::FieldRendererId kUsernameFieldRendererId(100);
  PossibleUsernameData possible_username_data(
      saved_match_.signon_realm, kUsernameFieldRendererId, possible_username,
      base::Time::Now(), 0 /* driver_id */);

  // Create form predictions and set them to |possible_username_data|.
  FormPredictions predictions;
  constexpr autofill::FormSignature kUsernameFormSignature(1000);
  predictions.form_signature = kUsernameFormSignature;
  PasswordFieldPrediction field_prediction;
  field_prediction.renderer_id = kUsernameFieldRendererId;
  field_prediction.signature.value() = 123;
  field_prediction.type = autofill::SINGLE_USERNAME;
  predictions.fields.push_back(field_prediction);
  possible_username_data.form_predictions = predictions;

  MockFieldInfoManager mock_field_manager;
  ON_CALL(mock_field_manager, GetFieldType(_, _))
      .WillByDefault(Return(UNKNOWN_TYPE));
  ON_CALL(client_, GetFieldInfoManager())
      .WillByDefault(Return(&mock_field_manager));

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

// Tests that server prediction are taken into consideration for offering
// username on username first flow.
TEST_P(PasswordFormManagerTest, PossibleUsernameServerPredictions) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUsernameFirstFlow);

  const base::string16 possible_username = ASCIIToUTF16("possible_username");
  PossibleUsernameData possible_username_data(
      saved_match_.signon_realm, autofill::FieldRendererId(101u),
      possible_username, base::Time::Now(), 0 /* driver_id */);

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = ASCIIToUTF16("strongpassword");

  for (ServerFieldType prediction : {SINGLE_USERNAME, NOT_USERNAME}) {
    SCOPED_TRACE(testing::Message("prediction=") << prediction);

    FormPredictions form_predictions;
    form_predictions.fields.push_back(
        {.renderer_id = possible_username_data.renderer_id,
         .type = prediction});

    possible_username_data.form_predictions = form_predictions;

    CreateFormManager(observed_form_only_password_fields_);
    fetcher_->NotifyFetchCompleted();

    ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                                 &possible_username_data));

    if (prediction == SINGLE_USERNAME) {
      // Check that a username is chosen from |possible_username_data|.
      EXPECT_EQ(possible_username,
                form_manager_->GetPendingCredentials().username_value);
    } else {
      // Check that a username is not chosen from |possible_username_data|.
      EXPECT_TRUE(
          form_manager_->GetPendingCredentials().username_value.empty());
    }
  }
}

#if !defined(OS_ANDROID)
// Tests that data from FieldInfoManager is taken into consideration for
// offering username on username first flow.
// Local heuristics on Android for username first flow is not supported.
TEST_P(PasswordFormManagerTest, PossibleUsernameFieldManager) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUsernameFirstFlow);

  const base::string16 possible_username = ASCIIToUTF16("possible_username");
  PossibleUsernameData possible_username_data(
      saved_match_.signon_realm, autofill::FieldRendererId(101u),
      possible_username, base::Time::Now(), 0 /* driver_id */);

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = ASCIIToUTF16("strongpassword");

  for (ServerFieldType prediction : {SINGLE_USERNAME, NOT_USERNAME}) {
    SCOPED_TRACE(testing::Message("prediction=") << prediction);

    constexpr autofill::FormSignature kFormSignature(1234);
    constexpr autofill::FieldSignature kFieldSignature(12);
    FormPredictions form_predictions;
    form_predictions.form_signature = kFormSignature;
    // Simulate that the server knows nothing about username field.
    form_predictions.fields.push_back(
        {.renderer_id = possible_username_data.renderer_id,
         .signature = kFieldSignature,
         .type = autofill::UNKNOWN_TYPE});
    possible_username_data.form_predictions = form_predictions;

    MockFieldInfoManager mock_field_manager;
    EXPECT_CALL(client_, GetFieldInfoManager())
        .WillOnce(Return(&mock_field_manager));
    EXPECT_CALL(mock_field_manager,
                GetFieldType(kFormSignature, kFieldSignature))
        .WillOnce(Return(prediction));

    CreateFormManager(observed_form_only_password_fields_);
    fetcher_->NotifyFetchCompleted();

    ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                                 &possible_username_data));

    if (prediction == SINGLE_USERNAME) {
      // Check that a username is chosen from |possible_username_data|.
      EXPECT_EQ(possible_username,
                form_manager_->GetPendingCredentials().username_value);
    } else {
      // Check that a username is not chosen from |possible_username_data|.
      EXPECT_TRUE(
          form_manager_->GetPendingCredentials().username_value.empty());
    }
    Mock::VerifyAndClearExpectations(&client_);
  }
}
#endif  //  !defined(OS_ANDROID)

// Tests that the a form with the username field but without a password field is
// not provisionally saved.
TEST_P(PasswordFormManagerTest, ProvisinallySavedOnSingleUsernameForm) {
  CreateFormManager(non_password_form_);
  std::map<FormSignature, FormPredictions> predictions =
      CreatePredictions(non_password_form_,
                        {std::make_pair(kUsernameFieldIndex, SINGLE_USERNAME)});
  form_manager_->ProcessServerPredictions(predictions);

  EXPECT_FALSE(form_manager_->ProvisionallySave(submitted_non_password_form_,
                                                &driver_, nullptr));
}

TEST_P(PasswordFormManagerTest, NotMovableToAccountStoreWhenBlocked) {
  const std::string kEmail = "email@gmail.com";
  const std::string kGaiaId = signin::GetTestGaiaIdForEmail(kEmail);

  PasswordForm saved_match(saved_match_);
  saved_match.in_store = PasswordForm::Store::kProfileStore;
  // Moving stored credentials is blocked for |kEmail|.
  saved_match.moving_blocked_for_list.push_back(
      GaiaIdHash::FromGaiaId(kGaiaId));
  SetNonFederatedAndNotifyFetchCompleted({&saved_match});

  submitted_form_.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  submitted_form_.fields[kPasswordFieldIndex].value =
      saved_match_.password_value;

  form_manager_->Fill();
  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));

  // Even with |kEmail| is signed in, credentials should NOT be movable.
  ON_CALL(client_, GetIdentityManager())
      .WillByDefault(Return(identity_test_env_.identity_manager()));
  identity_test_env_.SetPrimaryAccount(kEmail);
  EXPECT_FALSE(form_manager_->IsMovableToAccountStore());
}

TEST_P(PasswordFormManagerTest, MovableToAccountStore) {
  const std::string kEmail = "email@gmail.com";
  const std::string kGaiaId = signin::GetTestGaiaIdForEmail(kEmail);

  PasswordForm saved_match(saved_match_);
  saved_match.in_store = PasswordForm::Store::kProfileStore;
  // Moving stored credentials is blocked for |kEmail|.
  saved_match.moving_blocked_for_list.push_back(
      GaiaIdHash::FromGaiaId(kGaiaId));
  SetNonFederatedAndNotifyFetchCompleted({&saved_match});

  submitted_form_.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  submitted_form_.fields[kPasswordFieldIndex].value =
      saved_match_.password_value;

  form_manager_->Fill();
  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));

  // If another user is signed in, credentials should be movable.
  identity_test_env_.SetPrimaryAccount("another-user@gmail.com");
  ON_CALL(client_, GetIdentityManager())
      .WillByDefault(Return(identity_test_env_.identity_manager()));
  EXPECT_TRUE(form_manager_->IsMovableToAccountStore());
}

INSTANTIATE_TEST_SUITE_P(All,
                         PasswordFormManagerTest,
                         testing::Values(false, true));

class MockPasswordSaveManager : public PasswordSaveManager {
 public:
  MockPasswordSaveManager() = default;
  MockPasswordSaveManager(const MockPasswordSaveManager&) = delete;
  MockPasswordSaveManager& operator=(const MockPasswordSaveManager&) = delete;
  ~MockPasswordSaveManager() override = default;
  MOCK_METHOD4(Init,
               void(PasswordManagerClient*,
                    const FormFetcher*,
                    scoped_refptr<PasswordFormMetricsRecorder>,
                    VotesUploader*));
  MOCK_CONST_METHOD0(GetPendingCredentials, const PasswordForm&());
  MOCK_CONST_METHOD0(GetGeneratedPassword, const base::string16&());
  MOCK_CONST_METHOD0(GetFormSaver, FormSaver*());
  MOCK_METHOD5(CreatePendingCredentials,
               void(const PasswordForm&,
                    const autofill::FormData*,
                    const autofill::FormData&,
                    bool,
                    bool));
  MOCK_METHOD0(ResetPendingCredentials, void());
  MOCK_METHOD2(Save, void(const autofill::FormData*, const PasswordForm&));
  MOCK_METHOD3(Update,
               void(const PasswordForm&,
                    const autofill::FormData*,
                    const PasswordForm&));
  MOCK_METHOD1(PermanentlyBlacklist, void(const PasswordStore::FormDigest&));
  MOCK_METHOD1(Unblacklist, void(const PasswordStore::FormDigest&));
  MOCK_METHOD1(PresaveGeneratedPassword, void(PasswordForm));
  MOCK_METHOD2(GeneratedPasswordAccepted,
               void(PasswordForm, base::WeakPtr<PasswordManagerDriver>));
  MOCK_METHOD0(PasswordNoLongerGenerated, void());
  MOCK_CONST_METHOD0(IsNewLogin, bool());
  MOCK_CONST_METHOD0(IsPasswordUpdate, bool());
  MOCK_CONST_METHOD0(HasGeneratedPassword, bool());
  std::unique_ptr<PasswordSaveManager> Clone() override {
    return std::make_unique<MockPasswordSaveManager>();
  }
  MOCK_METHOD1(MoveCredentialsToAccountStore,
               void(metrics_util::MoveToAccountStoreTrigger));
  MOCK_METHOD1(BlockMovingToAccountStoreFor, void(const autofill::GaiaIdHash&));
};

class PasswordFormManagerTestWithMockedSaver : public PasswordFormManagerTest {
 public:
  PasswordFormManagerTestWithMockedSaver() = default;
  PasswordFormManagerTestWithMockedSaver(
      const PasswordFormManagerTestWithMockedSaver&) = delete;
  PasswordFormManagerTestWithMockedSaver& operator=(
      const PasswordFormManagerTestWithMockedSaver&) = delete;

  MockPasswordSaveManager* mock_password_save_manager() {
    return mock_password_save_manager_;
  }

  // Creates PasswordFormManager and sets it to |form_manager_|. Along the
  // way a new |fetcher_| is created.
  void CreateFormManager(const FormData& observed_form) override {
    auto mock_password_save_manager =
        std::make_unique<NiceMock<MockPasswordSaveManager>>();
    mock_password_save_manager_ = mock_password_save_manager.get();
    EXPECT_CALL(*mock_password_save_manager_, Init(_, _, _, _));
    form_manager_ = std::make_unique<PasswordFormManager>(
        &client_, driver_.AsWeakPtr(), observed_form, fetcher_.get(),
        std::move(mock_password_save_manager), nullptr);
  }

  // Creates PasswordFormManager and sets it to |form_manager_| for
  // |base_auth_observed_form|. Along the way a new |fetcher_| is created.
  void CreateFormManagerForNonWebForm(
      const PasswordForm& base_auth_observed_form) override {
    fetcher_->set_scheme(
        PasswordStore::FormDigest(base_auth_observed_form).scheme);
    auto mock_password_save_manager =
        std::make_unique<NiceMock<MockPasswordSaveManager>>();
    mock_password_save_manager_ = mock_password_save_manager.get();
    EXPECT_CALL(*mock_password_save_manager_, Init(_, _, _, _));
    form_manager_ = std::make_unique<PasswordFormManager>(
        &client_, PasswordStore::FormDigest(base_auth_observed_form),
        fetcher_.get(), std::move(mock_password_save_manager));
  }

 private:
  NiceMock<MockPasswordSaveManager>* mock_password_save_manager_;
};

TEST_F(
    PasswordFormManagerTestWithMockedSaver,
    ProviosnallySaveShouldCreatePendingPasswordFormManagerTestWithMockedSaverCredentials) {
  EXPECT_CALL(*mock_password_save_manager(),
              CreatePendingCredentials(_, _, _, _, _));
  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));
}

// Tests that when credentials are saved, the PasswordSaveManager is called
// accordingly.
TEST_F(PasswordFormManagerTestWithMockedSaver, SaveCredentials) {
  fetcher_->NotifyFetchCompleted();
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  FormData submitted_form = observed_form_;
  base::string16 new_username = saved_match_.username_value + ASCIIToUTF16("1");
  base::string16 new_password = saved_match_.password_value + ASCIIToUTF16("1");
  submitted_form.fields[kUsernameFieldIndex].value = new_username;
  submitted_form.fields[kPasswordFieldIndex].value = new_password;
  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form, &driver_, nullptr));
  PasswordForm updated_form;
  EXPECT_CALL(*mock_password_save_manager(),
              Save(FormDataPointeeEqualTo(observed_form_), _))
      .WillOnce(SaveArg<1>(&updated_form));
  EXPECT_CALL(client_, UpdateFormManagers());
  form_manager_->Save();
  std::string expected_signon_realm = submitted_form.url.GetOrigin().spec();
  EXPECT_EQ(submitted_form.url, updated_form.url);
  EXPECT_EQ(expected_signon_realm, updated_form.signon_realm);
  EXPECT_EQ(new_username, updated_form.username_value);
  EXPECT_EQ(new_password, updated_form.password_value);
  EXPECT_EQ(submitted_form.fields[kUsernameFieldIndex].name,
            updated_form.username_element);
  EXPECT_EQ(submitted_form.fields[kPasswordFieldIndex].name,
            updated_form.password_element);
  // Check UKM metrics.
  form_manager_.reset();
  ExpectedGenerationUKM expected_metrics = {
      {} /* shown manually */,
      0 /* password generated */,
      {} /* generated password is not modified */};
  CheckPasswordGenerationUKM(test_ukm_recorder, expected_metrics);
}

TEST_F(PasswordFormManagerTestWithMockedSaver, UpdateUsernameEmptyStore) {
  fetcher_->NotifyFetchCompleted();
  form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr);
  base::string16 new_username =
      parsed_submitted_form_.username_value + ASCIIToUTF16("1");
  PasswordForm expected = parsed_submitted_form_;
  expected.username_value = new_username;
  expected.username_element.clear();
  EXPECT_CALL(
      *mock_password_save_manager(),
      CreatePendingCredentials(FormHasUsernameValue(new_username), _, _, _, _));
  form_manager_->OnUpdateUsernameFromPrompt(new_username);
}

TEST_F(PasswordFormManagerTestWithMockedSaver,
       UpdateUsernameToAnotherFieldValue) {
  fetcher_->NotifyFetchCompleted();
  base::string16 user_chosen_username = ASCIIToUTF16("user_chosen_username");
  base::string16 automatically_chosen_username =
      ASCIIToUTF16("automatically_chosen_username");
  submitted_form_.fields[0].value = user_chosen_username;
  submitted_form_.fields[1].value = automatically_chosen_username;
  EXPECT_CALL(
      *mock_password_save_manager(),
      CreatePendingCredentials(
          FormHasUsernameValue(automatically_chosen_username), _, _, _, _));
  form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr);
  EXPECT_CALL(*mock_password_save_manager(),
              CreatePendingCredentials(
                  FormHasUsernameValue(user_chosen_username), _, _, _, _));
  form_manager_->OnUpdateUsernameFromPrompt(user_chosen_username);
}

TEST_F(PasswordFormManagerTestWithMockedSaver, UpdateUsernameFromPrompt) {
  fetcher_->NotifyFetchCompleted();
  EXPECT_CALL(*mock_password_save_manager(),
              CreatePendingCredentials(
                  FormHasUsernameValue(parsed_submitted_form_.username_value),
                  _, _, _, _));
  form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr);
  base::string16 new_username = saved_match_.username_value;
  base::string16 expected_password = parsed_submitted_form_.password_value;
  PasswordForm expected = saved_match_;
  expected.password_value = expected_password;
  EXPECT_CALL(
      *mock_password_save_manager(),
      CreatePendingCredentials(FormHasUsernameValue(new_username), _, _, _, _));
  form_manager_->OnUpdateUsernameFromPrompt(new_username);
}

TEST_F(PasswordFormManagerTestWithMockedSaver, UpdatePasswordValueFromPrompt) {
  fetcher_->NotifyFetchCompleted();
  EXPECT_CALL(
      *mock_password_save_manager(),
      CreatePendingCredentials(
          FormHasPassword(parsed_submitted_form_.password_value), _, _, _, _));
  form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr);
  base::string16 new_password = ASCIIToUTF16("new_password");
  EXPECT_CALL(
      *mock_password_save_manager(),
      CreatePendingCredentials(FormHasPassword(new_password), _, _, _, _));
  form_manager_->OnUpdatePasswordFromPrompt(new_password);
}

TEST_F(PasswordFormManagerTestWithMockedSaver,
       UpdatePasswordValueMultiplePasswordFields) {
  FormData form = observed_form_only_password_fields_;
  CreateFormManager(form);
  fetcher_->NotifyFetchCompleted();
  base::string16 password = ASCIIToUTF16("password1");
  base::string16 pin = ASCIIToUTF16("pin");
  form.fields[0].value = password;
  form.fields[1].value = pin;
  // Check that a second password field is chosen for saving.
  EXPECT_CALL(*mock_password_save_manager(),
              CreatePendingCredentials(FormHasPassword(pin), _, _, _, _));
  form_manager_->ProvisionallySave(form, &driver_, nullptr);
  // Simulate that the user updates value to save for the first password
  EXPECT_CALL(*mock_password_save_manager(),
              CreatePendingCredentials(FormHasPassword(password), _, _, _, _));
  form_manager_->OnUpdatePasswordFromPrompt(password);
}

TEST_F(PasswordFormManagerTestWithMockedSaver, PermanentlyBlacklist) {
  fetcher_->NotifyFetchCompleted();
  PasswordForm actual_blacklisted_form =
      password_manager_util::MakeNormalizedBlacklistedForm(
          PasswordStore::FormDigest(observed_form_));
  EXPECT_CALL(*mock_password_save_manager(),
              PermanentlyBlacklist(PasswordStore::FormDigest(observed_form_)));
  form_manager_->PermanentlyBlacklist();
  EXPECT_TRUE(form_manager_->IsBlacklisted());
}

TEST_F(PasswordFormManagerTestWithMockedSaver, MoveCredentialsToAccountStore) {
  ON_CALL(*client_.GetPasswordFeatureManager(), IsOptedInForAccountStorage)
      .WillByDefault(Return(true));
  EXPECT_CALL(*mock_password_save_manager(),
              MoveCredentialsToAccountStore(
                  metrics_util::MoveToAccountStoreTrigger::
                      kSuccessfulLoginWithProfileStorePassword));
  form_manager_->MoveCredentialsToAccountStore();
}

TEST_F(PasswordFormManagerTestWithMockedSaver,
       BlockMovingCredentialsToAccountStore) {
  const std::string kEmail = "email@gmail.com";
  const std::string kGaiaId = signin::GetTestGaiaIdForEmail(kEmail);

  PasswordForm saved_match(saved_match_);
  saved_match.in_store = PasswordForm::Store::kProfileStore;
  SetNonFederatedAndNotifyFetchCompleted({&saved_match});

  ON_CALL(*mock_password_save_manager(), GetPendingCredentials)
      .WillByDefault(ReturnRef(saved_match));

  ON_CALL(client_, GetIdentityManager())
      .WillByDefault(Return(identity_test_env_.identity_manager()));

  identity_test_env_.SetPrimaryAccount(kEmail);

  EXPECT_CALL(*mock_password_save_manager(),
              BlockMovingToAccountStoreFor(GaiaIdHash::FromGaiaId(kGaiaId)));
  form_manager_->BlockMovingCredentialsToAccountStore();
}

TEST_F(PasswordFormManagerTestWithMockedSaver, IsNewLogin) {
  EXPECT_CALL(*mock_password_save_manager(), IsNewLogin());
  form_manager_->IsNewLogin();
}

TEST_F(PasswordFormManagerTestWithMockedSaver, IsPasswordUpdate) {
  EXPECT_CALL(*mock_password_save_manager(), IsPasswordUpdate());
  form_manager_->IsPasswordUpdate();
}

TEST_F(PasswordFormManagerTestWithMockedSaver, HasGeneratedPassword) {
  EXPECT_CALL(*mock_password_save_manager(), HasGeneratedPassword());
  form_manager_->HasGeneratedPassword();
}

TEST_F(PasswordFormManagerTestWithMockedSaver, GetPendingCredentials) {
  PasswordForm password_form;
  EXPECT_CALL(*mock_password_save_manager(), GetPendingCredentials())
      .WillOnce(ReturnRef(password_form));
  form_manager_->GetPendingCredentials();
}

TEST_F(PasswordFormManagerTestWithMockedSaver, PresaveGeneratedPassword) {
  fetcher_->NotifyFetchCompleted();
  EXPECT_FALSE(form_manager_->HasGeneratedPassword());
  form_manager_->SetGenerationPopupWasShown(/*is_manual_generation=*/false);
  PasswordForm form_with_generated_password = parsed_submitted_form_;
  FormData& form_data = form_with_generated_password.form_data;
  // Check that the generated password is forwarded to the save manager.
  EXPECT_CALL(*mock_password_save_manager(),
              PresaveGeneratedPassword(FormHasPassword(
                  form_data.fields[kPasswordFieldIndex].value)));
  form_manager_->PresaveGeneratedPassword(
      form_with_generated_password.form_data,
      form_with_generated_password.password_value);
}

TEST_F(PasswordFormManagerTestWithMockedSaver,
       GeneratedPasswordWhichIsNotInFormData) {
  fetcher_->NotifyFetchCompleted();
  // Create a password form such that |form_data| do not contain the generated
  // password.
  PasswordForm form_with_generated_password;
  form_with_generated_password.form_data = submitted_form_;
  const base::string16 generated_password = ASCIIToUTF16("gen_pw");
  // |password_value| should contain the generated password.
  form_with_generated_password.password_value = generated_password;
  // Check that the generated password is forwarded to the
  PasswordForm updated_form;
  EXPECT_CALL(*mock_password_save_manager(),
              PresaveGeneratedPassword(FormHasPassword(generated_password)))
      .WillOnce(SaveArg<0>(&updated_form));
  form_manager_->PresaveGeneratedPassword(
      form_with_generated_password.form_data,
      form_with_generated_password.password_value);
  // Now, the password save manager should have a generated password.
  ON_CALL(*mock_password_save_manager(), HasGeneratedPassword())
      .WillByDefault(Return(true));
  EXPECT_EQ(submitted_form_.fields[kUsernameFieldIndex].value,
            updated_form.username_value);
  EXPECT_TRUE(form_manager_->HasGeneratedPassword());
  // Check that the generated password is saved.
  EXPECT_CALL(client_, UpdateFormManagers());
  EXPECT_CALL(*mock_password_save_manager(),
              CreatePendingCredentials(_, _, _, _, _));
  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));
  EXPECT_CALL(*mock_password_save_manager(),
              Save(FormDataPointeeEqualTo(submitted_form_), _))
      .WillOnce(SaveArg<1>(&updated_form));
  form_manager_->Save();
  EXPECT_EQ(submitted_form_.fields[kUsernameFieldIndex].value,
            updated_form.username_value);
}

TEST_F(PasswordFormManagerTestWithMockedSaver,
       PresaveGenerationWhenParsingFails) {
  fetcher_->NotifyFetchCompleted();
  // Create a password form with empty |form_data|. On this form the form parser
  // should fail.
  PasswordForm form_with_empty_form_data;
  const base::string16 generated_password = ASCIIToUTF16("gen_pw");
  form_with_empty_form_data.password_value = generated_password;
  // Check that nevertheless the generated password is forwarded to the
  // PasswordSaveManager.
  EXPECT_CALL(*mock_password_save_manager(),
              PresaveGeneratedPassword(FormHasPassword(generated_password)));
  form_manager_->PresaveGeneratedPassword(
      form_with_empty_form_data.form_data,
      form_with_empty_form_data.password_value);
}

TEST_F(PasswordFormManagerTestWithMockedSaver, PasswordNoLongerGenerated) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  fetcher_->NotifyFetchCompleted();
  form_manager_->SetGenerationPopupWasShown(true /* is_manual_generation */);
  EXPECT_CALL(*mock_password_save_manager(), PresaveGeneratedPassword(_));
  PasswordForm form = parsed_submitted_form_;
  form_manager_->PresaveGeneratedPassword(form.form_data, form.password_value);
  Mock::VerifyAndClearExpectations(mock_password_save_manager());
  // Now, the password save manager should have a generated password.
  ON_CALL(*mock_password_save_manager(), HasGeneratedPassword())
      .WillByDefault(Return(true));
  EXPECT_TRUE(form_manager_->HasGeneratedPassword());
  // Check when the user removes the generated password on the page, it is
  // removed from the store.
  EXPECT_CALL(*mock_password_save_manager(), PasswordNoLongerGenerated());
  form_manager_->PasswordNoLongerGenerated();
  // Now, the password save manager should not have a generated password.
  ON_CALL(*mock_password_save_manager(), HasGeneratedPassword())
      .WillByDefault(Return(false));
  EXPECT_FALSE(form_manager_->HasGeneratedPassword());
  // Check UKM metrics.
  form_manager_.reset();
  ExpectedGenerationUKM expected_metrics = {
      base::make_optional(2u) /* shown manually */,
      0 /* password generated */,
      {} /* generated password is not modified */};
  CheckPasswordGenerationUKM(test_ukm_recorder, expected_metrics);
}

TEST_F(PasswordFormManagerTestWithMockedSaver,
       OnGeneratedPasswordAccepted_InvalidField) {
  FormData form_data = observed_form_only_password_fields_;
  // A call with a non-existent password field should not crash.
  form_manager_->OnGeneratedPasswordAccepted(std::move(form_data),
                                             autofill::FieldRendererId(12345),
                                             ASCIIToUTF16("new_password"));
}

TEST_F(PasswordFormManagerTestWithMockedSaver, SaveHttpAuthNoHttpAuthStored) {
  for (bool html_credentials_saved : {false, true}) {
    SCOPED_TRACE(testing::Message("html_credentials_saved=")
                 << html_credentials_saved);
    PasswordForm http_auth_form = parsed_observed_form_;
    http_auth_form.scheme = PasswordForm::Scheme::kBasic;
    // Check that no filling because no http auth credentials are stored.
    EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
    EXPECT_CALL(client_, AutofillHttpAuth(_, _)).Times(0);
    CreateFormManagerForNonWebForm(http_auth_form);
    std::vector<const PasswordForm*> saved_matches;
    if (html_credentials_saved)
      saved_matches.push_back(&saved_match_);
    SetNonFederatedAndNotifyFetchCompleted(saved_matches);
    base::string16 username = ASCIIToUTF16("user1");
    base::string16 password = ASCIIToUTF16("pass1");
    http_auth_form.username_value = username;
    http_auth_form.password_value = password;
    // Check that submitted credentials are forwarded to the password save
    // manager.
    EXPECT_CALL(*mock_password_save_manager(),
                CreatePendingCredentials(http_auth_form, _, _, true, _));
    ASSERT_TRUE(form_manager_->ProvisionallySaveHttpAuthForm(http_auth_form));
    PasswordForm updated_form;
    // Check that the password save manager is invoked.
    EXPECT_CALL(*mock_password_save_manager(), Save(_, http_auth_form));
    form_manager_->Save();
  }
}

TEST_F(PasswordFormManagerTestWithMockedSaver, HTTPAuthAlreadySaved) {
  PasswordForm http_auth_form = parsed_observed_form_;
  http_auth_form.scheme = PasswordForm::Scheme::kBasic;
  CreateFormManagerForNonWebForm(http_auth_form);
  const base::string16 username = ASCIIToUTF16("user1");
  const base::string16 password = ASCIIToUTF16("pass1");
  http_auth_form.username_value = username;
  http_auth_form.password_value = password;
  EXPECT_CALL(client_, AutofillHttpAuth(http_auth_form, _)).Times(1);
  SetNonFederatedAndNotifyFetchCompleted({&http_auth_form});
  EXPECT_CALL(*mock_password_save_manager(),
              CreatePendingCredentials(http_auth_form, _, _, true, _));
  ASSERT_TRUE(form_manager_->ProvisionallySaveHttpAuthForm(http_auth_form));
  // Check that the password save manager is invoked.
  EXPECT_CALL(*mock_password_save_manager(), Save(_, http_auth_form));
  form_manager_->Save();
}

#if !defined(OS_ANDROID)
// Tests that username is taken during username first flow.
// Local heuristics on Android for username first flow is not supported.
TEST_F(PasswordFormManagerTestWithMockedSaver, UsernameFirstFlow) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUsernameFirstFlow);
  CreateFormManager(observed_form_only_password_fields_);
  fetcher_->NotifyFetchCompleted();
  const base::string16 possible_username = ASCIIToUTF16("possible_username");
  PossibleUsernameData possible_username_data(
      saved_match_.signon_realm, autofill::FieldRendererId(1u),
      possible_username, base::Time::Now(), 0 /* driver_id */);
  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = ASCIIToUTF16("strongpassword");
  // Check that a username is chosen from |possible_username_data|.
  EXPECT_CALL(*mock_password_save_manager(),
              CreatePendingCredentials(FormHasUsernameValue(possible_username),
                                       _, _, _, _));
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               &possible_username_data));
}
#endif  // !defined(OS_ANDROID)

// Tests that username is not taken when a possible username is not valid.
TEST_F(PasswordFormManagerTestWithMockedSaver,
       UsernameFirstFlowDifferentDomains) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUsernameFirstFlow);
  CreateFormManager(observed_form_only_password_fields_);
  fetcher_->NotifyFetchCompleted();
  base::string16 possible_username = ASCIIToUTF16("possible_username");
  PossibleUsernameData possible_username_data(
      "https://another.domain.com", autofill::FieldRendererId(1u),
      possible_username, base::Time::Now(), 0 /* driver_id */);
  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = ASCIIToUTF16("strongpassword");
  PasswordForm parsed_submitted_form;
  EXPECT_CALL(*mock_password_save_manager(),
              CreatePendingCredentials(_, _, _, _, _))
      .WillOnce(SaveArg<0>(&parsed_submitted_form));
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               &possible_username_data));
  EXPECT_TRUE(parsed_submitted_form.username_value.empty());
}

}  // namespace

}  // namespace password_manager
