// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_manager.h"

#include <memory>

#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include "base/containers/lru_cache.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/browser/test_utils/vote_uploads_test_matchers.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/field_info_manager.h"
#include "components/password_manager/core/browser/mock_webauthn_credentials_delegate.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/possible_username_data.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/browser/vote_uploads_test_matchers.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/webauthn/android/cred_man_support.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#endif  // BUILDFLAG(IS_ANDROID)

using autofill::AutofillUploadContents;
using autofill::FieldPropertiesFlags;
using autofill::FieldRendererId;
using autofill::FieldSignature;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormRendererId;
using autofill::FormSignature;
using autofill::FormStructure;
using autofill::IsMostRecentSingleUsernameCandidate;
using autofill::NOT_USERNAME;
using autofill::PasswordFormFillData;
using autofill::PasswordFormGenerationData;
using autofill::ServerFieldType;
using autofill::ServerFieldTypeSet;
using autofill::SINGLE_USERNAME;
using autofill::SINGLE_USERNAME_FORGOT_PASSWORD;
using autofill::UNKNOWN_TYPE;
using autofill::password_generation::PasswordGenerationType;
using base::TestMockTimeTaskRunner;
using signin::GaiaIdHash;
using testing::_;
using testing::AllOf;
using testing::Contains;
using testing::DoAll;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::IsNull;
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
constexpr int kUsernameFieldIndex = 1;
constexpr int kPasswordFieldIndex = 2;

// Signatures of the single username form and field.
constexpr autofill::FormSignature kSingleUsernameFormSignature(1000);
constexpr autofill::FieldSignature kSingleUsernameFieldSignature(123);

// Unique renderer id of the single username field.
constexpr autofill::FieldRendererId kSingleUsernameFieldRendererId(101);

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
  return autofill::FormData::DeepEqual(*arg, form_data);
}

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MOCK_METHOD(void,
              SetPasswordFillData,
              (const PasswordFormFillData&),
              (override));
  MOCK_METHOD(void,
              FormEligibleForGenerationFound,
              (const autofill::PasswordFormGenerationData&),
              (override));
  MOCK_METHOD(bool, IsInPrimaryMainFrame, (), (const, override));
};

class MockAutofillDownloadManager : public autofill::AutofillDownloadManager {
 public:
  MockAutofillDownloadManager()
      : AutofillDownloadManager(nullptr,
                                version_info::Channel::UNKNOWN,
                                nullptr) {}
  MockAutofillDownloadManager(const MockAutofillDownloadManager&) = delete;
  MockAutofillDownloadManager& operator=(const MockAutofillDownloadManager&) =
      delete;

  MOCK_METHOD(bool,
              StartUploadRequest,
              (const FormStructure&,
               bool,
               const ServerFieldTypeSet&,
               const std::string&,
               bool,
               PrefService*,
               base::WeakPtr<Observer>),
              (override));
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_METHOD(bool, IsOffTheRecord, (), (const, override));
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
  MOCK_METHOD(signin::IdentityManager*, GetIdentityManager, (), (override));
  MOCK_METHOD(PrefService*, GetPrefs, (), (const, override));
  MOCK_METHOD(const GURL&, GetLastCommittedURL, (), (const, override));
  MOCK_METHOD(url::Origin, GetLastCommittedOrigin, (), (const, override));
  MOCK_METHOD(FieldInfoManager*, GetFieldInfoManager, (), (const, override));
  MOCK_METHOD(WebAuthnCredentialsDelegate*,
              GetWebAuthnCredentialsDelegateForDriver,
              (PasswordManagerDriver*),
              (override));
#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(void,
              ShowPasswordManagerErrorMessage,
              (password_manager::ErrorMessageFlowType,
               password_manager::PasswordStoreBackendErrorType),
              (override));
#endif
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
  EXPECT_TRUE(FormData::DeepEqual(expected.form_data, actual.form_data));
}

struct ExpectedGenerationUKM {
  absl::optional<int64_t> generation_popup_shown;
  int64_t has_generated_password;
  absl::optional<int64_t> generated_password_modified;
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

// Create simple predictions on single username field.
FormPredictions MakeSingleUsernamePredictions(
    const autofill::FormSignature form_signature,
    const autofill::FieldRendererId renderer_id,
    const autofill::FieldSignature field_signature,
    bool has_single_username_prediction = true) {
  FormPredictions predictions;
  predictions.form_signature = form_signature;

  PasswordFieldPrediction field_prediction;
  field_prediction.renderer_id = renderer_id;
  field_prediction.signature = field_signature;
  field_prediction.type = has_single_username_prediction
                              ? autofill::SINGLE_USERNAME
                              : autofill::NO_SERVER_DATA;
  predictions.fields.push_back(field_prediction);

  return predictions;
}

class MockFormSaver : public StubFormSaver {
 public:
  MockFormSaver() = default;
  MockFormSaver(const MockFormSaver&) = delete;
  MockFormSaver& operator=(const MockFormSaver&) = delete;
  ~MockFormSaver() override = default;

  // FormSaver:
  MOCK_METHOD1(Blocklist, PasswordForm(PasswordFormDigest));
  MOCK_METHOD3(Save,
               void(PasswordForm pending,
                    const std::vector<const PasswordForm*>& matches,
                    const std::u16string& old_password));
  MOCK_METHOD3(Update,
               void(PasswordForm pending,
                    const std::vector<const PasswordForm*>& matches,
                    const std::u16string& old_password));
  MOCK_METHOD4(UpdateReplace,
               void(PasswordForm pending,
                    const std::vector<const PasswordForm*>& matches,
                    const std::u16string& old_password,
                    const PasswordForm& old_unique_key));
  MOCK_METHOD1(Remove, void(const PasswordForm&));

  std::unique_ptr<FormSaver> Clone() override {
    return std::make_unique<MockFormSaver>();
  }

  // Convenience downcasting method.
  static MockFormSaver& Get(PasswordFormManager* form_manager) {
    return *static_cast<MockFormSaver*>(
        form_manager->profile_store_form_saver());
  }
};

class PasswordFormManagerTest : public testing::Test,
                                public testing::WithParamInterface<bool> {
 public:
  PasswordFormManagerTest() : task_runner_(new TestMockTimeTaskRunner) {
    pref_service_.registry()->RegisterTimePref(
        prefs::kProfileStoreDateLastUsedForFilling, base::Time());
    pref_service_.registry()->RegisterTimePref(
        prefs::kAccountStoreDateLastUsedForFilling, base::Time());
    pref_service_.registry()->RegisterBooleanPref(
        autofill::RandomizedEncoder::kUrlKeyedAnonymizedDataCollectionEnabled,
        true);
    pref_service_.registry()->RegisterStringPref(
        autofill::prefs::kAutofillUploadEncodingSeed, "seed");
#if BUILDFLAG(IS_ANDROID)
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
        false);
#endif
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kBiometricAuthenticationBeforeFilling, true);
#endif

    form_manager_->set_wait_for_server_predictions_for_filling(true);

    GURL origin = GURL("https://accounts.google.com/a/ServiceLoginAuth");
    GURL action = GURL("https://accounts.google.com/a/ServiceLogin");
    GURL psl_origin = GURL("https://myaccounts.google.com/a/ServiceLoginAuth");
    GURL psl_action = GURL("https://myaccounts.google.com/a/ServiceLogin");

    observed_form_.url = origin;
    observed_form_.action = action;
    observed_form_.name = u"sign-in";
    observed_form_.unique_renderer_id = FormRendererId(1);
    observed_form_.is_form_tag = true;

    observed_form_only_password_fields_ = observed_form_;

    FormFieldData field;
    field.name = u"firstname";
    field.id_attribute = field.name;
    field.name_attribute = field.name;
    field.form_control_type = autofill::FormControlType::kInputText;
    field.unique_renderer_id = autofill::FieldRendererId(2);
    observed_form_.fields.push_back(field);

    field.name = u"username";
    field.id_attribute = field.name;
    field.name_attribute = field.name;
    field.form_control_type = autofill::FormControlType::kInputText;
    field.unique_renderer_id = autofill::FieldRendererId(3);
    observed_form_.fields.push_back(field);

    non_password_form_ = observed_form_;

    field.name = u"password";
    field.id_attribute = field.name;
    field.name_attribute = field.name;
    field.form_control_type = autofill::FormControlType::kInputPassword;
    field.unique_renderer_id = autofill::FieldRendererId(4);
    observed_form_.fields.push_back(field);
    observed_form_only_password_fields_.fields.push_back(field);

    field.name = u"password2";
    field.id_attribute = field.name;
    field.name_attribute = field.name;
    field.form_control_type = autofill::FormControlType::kInputPassword;
    field.unique_renderer_id = autofill::FieldRendererId(5);
    observed_form_only_password_fields_.fields.push_back(field);

    submitted_form_ = observed_form_;
    submitted_form_.fields[kUsernameFieldIndex].value = u"user1";
    submitted_form_.fields[kPasswordFieldIndex].value = u"secret1";

    submitted_non_password_form_ = non_password_form_;
    submitted_non_password_form_.fields[kUsernameFieldIndex].value = u"user1";

    saved_match_.url = origin;
    saved_match_.action = action;
    saved_match_.signon_realm = "https://accounts.google.com/";
    saved_match_.username_value = u"test@gmail.com";
    saved_match_.username_element = u"field1";
    saved_match_.password_value = u"test1";
    saved_match_.password_element = u"field2";
    saved_match_.match_type = PasswordForm::MatchType::kExact;
    saved_match_.scheme = PasswordForm::Scheme::kHtml;
    saved_match_.in_store = PasswordForm::Store::kProfileStore;

    psl_saved_match_ = saved_match_;
    psl_saved_match_.url = psl_origin;
    psl_saved_match_.action = psl_action;
    psl_saved_match_.signon_realm = "https://myaccount.google.com/";
    psl_saved_match_.match_type = PasswordForm::MatchType::kPSL;

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
    ON_CALL(mock_autofill_download_manager_, StartUploadRequest)
        .WillByDefault(Return(true));
    ON_CALL(*client_.GetPasswordFeatureManager(), GetDefaultPasswordStore)
        .WillByDefault(Return(PasswordForm::Store::kProfileStore));

    ON_CALL(client_, GetLastCommittedURL())
        .WillByDefault(ReturnRef(observed_form_.url));
    ON_CALL(client_, GetLastCommittedOrigin)
        .WillByDefault(Return(url::Origin::Create(observed_form_.url)));
    ON_CALL(client_, GetWebAuthnCredentialsDelegateForDriver)
        .WillByDefault(Return(&webauthn_credentials_delegate_));
    ON_CALL(webauthn_credentials_delegate_, GetPasskeys)
        .WillByDefault(ReturnRef(passkeys_));
    ON_CALL(webauthn_credentials_delegate_,
            OfferPasskeysFromAnotherDeviceOption)
        .WillByDefault(Return(true));
#if BUILDFLAG(IS_ANDROID)
    webauthn::WebAuthnCredManDelegate::override_cred_man_support_for_testing(
        webauthn::CredManSupport::DISABLED);
#endif  // BUILDFLAG(IS_ANDROID)

    field_info_manager_ = std::make_unique<FieldInfoManager>(task_runner_);
    ON_CALL(client_, GetFieldInfoManager())
        .WillByDefault(Return(field_info_manager_.get()));

    fetcher_ = std::make_unique<FakeFormFetcher>();
    fetcher_->Fetch();
  }

  void SetUp() override { CreateFormManager(observed_form_); }

 protected:
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
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
  NiceMock<MockPasswordManagerClient> client_;
  NiceMock<MockPasswordManagerDriver> driver_;
  NiceMock<MockWebAuthnCredentialsDelegate> webauthn_credentials_delegate_;
  std::unique_ptr<FieldInfoManager> field_info_manager_;
  scoped_refptr<TestMockTimeTaskRunner> task_runner_;
  absl::optional<std::vector<PasskeyCredential>> passkeys_;

  // Define |fetcher_| before |form_manager_|, because the former needs to
  // outlive the latter.
  std::unique_ptr<FakeFormFetcher> fetcher_;
  std::unique_ptr<PasswordFormManager> form_manager_;

  // Creates PasswordFormManager and sets it to |form_manager_|. Along the
  // way a new |fetcher_| is created.
  virtual void CreateFormManager(const FormData& observed_form) {
    auto password_save_manager = std::make_unique<PasswordSaveManagerImpl>(
        /*profile_form_saver=*/std::make_unique<NiceMock<MockFormSaver>>(),
        /*account_form_saver=*/GetParam()
            ? std::make_unique<NiceMock<MockFormSaver>>()
            : nullptr);

    form_manager_ = std::make_unique<PasswordFormManager>(
        &client_, driver_.AsWeakPtr(), observed_form, fetcher_.get(),
        std::move(password_save_manager), nullptr);
  }

  // Creates PasswordFormManager and sets it to |form_manager_| for
  // |base_auth_observed_form|. Along the way a new |fetcher_| is created.
  virtual void CreateFormManagerForNonWebForm(
      const PasswordForm& base_auth_observed_form) {
    auto password_save_manager = std::make_unique<PasswordSaveManagerImpl>(
        /*profile_form_saver=*/std::make_unique<NiceMock<MockFormSaver>>(),
        /*account_form_saver=*/GetParam()
            ? std::make_unique<NiceMock<MockFormSaver>>()
            : nullptr);
    fetcher_->set_scheme(PasswordFormDigest(base_auth_observed_form).scheme);
    form_manager_ = std::make_unique<PasswordFormManager>(
        &client_, PasswordFormDigest(base_auth_observed_form), fetcher_.get(),
        std::move(password_save_manager));
  }

  void SetNonFederatedAndNotifyFetchCompleted(
      const std::vector<const PasswordForm*>& non_federated) {
    fetcher_->SetNonFederated(non_federated);
    fetcher_->NotifyFetchCompleted();
  }

  void AddFieldInfo(int driver_id,
                    FieldRendererId field_id,
                    const GURL& url,
                    const std::u16string field_value,
                    FormSignature form_signature,
                    FieldSignature field_signature,
                    bool is_likely_otp,
                    ServerFieldType predicted_type) {
    FieldInfo info(driver_id, field_id, GetSignonRealm(url), field_value,
                   is_likely_otp);
    FormPredictions predictions;
    predictions.form_signature = form_signature;
    predictions.fields.push_back({
        .renderer_id = field_id,
        .signature = field_signature,
        .type = predicted_type,
    });
    field_info_manager_->AddFieldInfo(info, predictions);
  }

  void ExpectSingleUsernameUpload(
      FormSignature form_signature,
      AutofillUploadContents::Field::SingleUsernameVoteType vote_type,
      autofill::ServerFieldType field_type,
      autofill::IsMostRecentSingleUsernameCandidate
          is_most_recent_single_username_candidate =
              autofill::IsMostRecentSingleUsernameCandidate::
                  kNotPartOfUsernameFirstFlow) {
    EXPECT_CALL(mock_autofill_download_manager_,
                StartUploadRequest(
                    AllOf(SignatureIs(form_signature),
                          UploadedSingleUsernameVoteTypeIs(vote_type),
                          UploadedSingleUsernameVoteIsMostRecentCandidate(
                              is_most_recent_single_username_candidate)),
                    _, ServerFieldTypeSet{field_type}, _, _, _, _));
  }

  // Creates LRU cache simulating user modifying non-password field outside of
  // the form.
  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
  MakePossibleUsernamesCache(
      std::vector<PossibleUsernameData> possible_usernames_data) {
    base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
        cache = base::LRUCache<PossibleUsernameFieldIdentifier,
                               PossibleUsernameData>(
            possible_usernames_data.size());
    for (auto possible_username_data : possible_usernames_data) {
      cache.Put({{possible_username_data.driver_id,
                  possible_username_data.renderer_id},
                 possible_username_data});
    }
    return cache;
  }
};

TEST_P(PasswordFormManagerTest, DoesManage) {
  EXPECT_TRUE(
      form_manager_->DoesManage(observed_form_.unique_renderer_id, &driver_));
  // Forms on other drivers are not considered managed.
  MockPasswordManagerDriver another_driver;
  EXPECT_FALSE(form_manager_->DoesManage(observed_form_.unique_renderer_id,
                                         &another_driver));
  FormData another_form = observed_form_;
  another_form.is_form_tag = false;
  another_form.unique_renderer_id = FormRendererId();
  EXPECT_FALSE(
      form_manager_->DoesManage(another_form.unique_renderer_id, &driver_));

  // Unique_renderer_id is the form identifier.
  another_form = observed_form_;
  another_form.unique_renderer_id.value() += 1;
  EXPECT_FALSE(
      form_manager_->DoesManage(another_form.unique_renderer_id, &driver_));
}

TEST_P(PasswordFormManagerTest, DoesManageNoFormTag) {
  observed_form_.is_form_tag = false;
  CreateFormManager(observed_form_);

  FormData another_form = observed_form_;
  // Simulate that new input was added by JavaScript.
  another_form.fields.emplace_back();
  EXPECT_TRUE(
      form_manager_->DoesManage(another_form.unique_renderer_id, &driver_));
  // Forms on other drivers are not considered managed.
  EXPECT_FALSE(
      form_manager_->DoesManage(another_form.unique_renderer_id, nullptr));
}

TEST_P(PasswordFormManagerTest, Autofill) {
  CreateFormManager(observed_form_);
  EXPECT_CALL(driver_, FormEligibleForGenerationFound(_)).Times(0);
  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, SetPasswordFillData).WillOnce(SaveArg<0>(&fill_data));
  CreateFormManager(observed_form_);
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_EQ(observed_form_.url, fill_data.url);

  // On Android Touch To Fill will prevent autofilling credentials on page load.
  // On iOS bio-metric reauth will prevent autofilling as well.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_TRUE(fill_data.wait_for_username);
#else
  EXPECT_FALSE(fill_data.wait_for_username);
#endif

  EXPECT_EQ(saved_match_.username_value,
            fill_data.preferred_login.username_value);
  EXPECT_EQ(saved_match_.password_value,
            fill_data.preferred_login.password_value);
}

TEST_P(PasswordFormManagerTest, AutofillNotMoreThan5Times) {
  PasswordFormManager::set_wait_for_server_predictions_for_filling(false);
  EXPECT_CALL(driver_, SetPasswordFillData);
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  task_environment_.FastForwardUntilNoTasksRemain();
  Mock::VerifyAndClearExpectations(&driver_);

  for (size_t i = 0; i < PasswordFormManager::kMaxTimesAutofill - 1; ++i) {
    EXPECT_CALL(driver_, SetPasswordFillData);
    form_manager_->Fill();
    Mock::VerifyAndClearExpectations(&driver_);
  }

  EXPECT_CALL(driver_, SetPasswordFillData).Times(0);
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
  EXPECT_CALL(driver_, SetPasswordFillData).WillOnce(SaveArg<0>(&fill_data));

  PasswordFormGenerationData generation_data;
  EXPECT_CALL(driver_, FormEligibleForGenerationFound(_))
      .WillOnce(SaveArg<0>(&generation_data));

  CreateFormManager(observed_form_);
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(fill_data.password_element_renderer_id.is_null());
  EXPECT_EQ(saved_match_.password_value,
            fill_data.preferred_login.password_value);
#if BUILDFLAG(IS_IOS)
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
  field.form_control_type = autofill::FormControlType::kInputPassword;
  field.autocomplete_attribute = "new-password";
  observed_form_.fields.push_back(field);

  PasswordFormGenerationData generation_data;
  EXPECT_CALL(driver_, FormEligibleForGenerationFound(_))
      .WillOnce(SaveArg<0>(&generation_data));

  CreateFormManager(observed_form_);
  fetcher_->NotifyFetchCompleted();

  task_environment_.FastForwardUntilNoTasksRemain();
#if BUILDFLAG(IS_IOS)
  EXPECT_EQ(observed_form_.unique_renderer_id,
            generation_data.form_renderer_id);
#else
  EXPECT_EQ(new_password_render_id, generation_data.new_password_renderer_id);
  EXPECT_EQ(confirm_password_render_id,
            generation_data.confirmation_password_renderer_id);
#endif
}

TEST_P(PasswordFormManagerTest, AutofillWithBlocklistedMatch) {
  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, SetPasswordFillData).WillOnce(SaveArg<0>(&fill_data));
  fetcher_->SetNonFederated({&saved_match_});
  fetcher_->SetBlocklisted(true);
  fetcher_->NotifyFetchCompleted();

  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_EQ(observed_form_.url, fill_data.url);
  EXPECT_EQ(saved_match_.username_value,
            fill_data.preferred_login.username_value);
  EXPECT_EQ(saved_match_.password_value,
            fill_data.preferred_login.password_value);
}

TEST_P(PasswordFormManagerTest, SetSubmitted) {
  EXPECT_FALSE(form_manager_->is_submitted());
  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));
  EXPECT_TRUE(form_manager_->is_submitted());

  FormData another_form = submitted_form_;
  another_form.name += u"1";
#if !BUILDFLAG(IS_IOS)
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

// Tests that when PasswordFormManager receives saved matches it waits for
// server predictions and fills on receiving them.
TEST_P(PasswordFormManagerTest, ServerPredictionsWithinDelay) {
  // Expects no filling on save matches receiving.
  EXPECT_CALL(driver_, SetPasswordFillData).Times(0);
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});
  Mock::VerifyAndClearExpectations(&driver_);

  std::map<FormSignature, FormPredictions> predictions = CreatePredictions(
      observed_form_, {std::make_pair(2, autofill::PASSWORD)});

  // Expect filling without delay on receiving server predictions.
  EXPECT_CALL(driver_, SetPasswordFillData).Times(1);
  form_manager_->ProcessServerPredictions(predictions);
  Mock::VerifyAndClearExpectations(&driver_);

  // Expect no filling on receiving predictions again.
  EXPECT_CALL(driver_, SetPasswordFillData).Times(0);
  form_manager_->ProcessServerPredictions(predictions);
}

// Tests that PasswordFormManager fills after some delay even without
// server predictions.
TEST_P(PasswordFormManagerTest, ServerPredictionsAfterDelay) {
  EXPECT_CALL(driver_, SetPasswordFillData).Times(1);
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});
  // Expect filling after passing filling delay.

  // Simulate passing filling delay.
  task_environment_.FastForwardUntilNoTasksRemain();
  Mock::VerifyAndClearExpectations(&driver_);

  std::map<FormSignature, FormPredictions> predictions = CreatePredictions(
      observed_form_, {std::make_pair(2, autofill::PASSWORD)});

  // Expect filling on receiving server predictions because it was less than
  // kMaxTimesAutofill attempts to fill.
  EXPECT_CALL(driver_, SetPasswordFillData).Times(1);
  form_manager_->ProcessServerPredictions(predictions);
  task_environment_.FastForwardUntilNoTasksRemain();
}

// Tests that filling happens immediately if server predictions are received
// before saved matches.
TEST_P(PasswordFormManagerTest, ServerPredictionsBeforeFetcher) {
  // Expect no filling after receiving saved matches from |fetcher_|, since
  // |form_manager| is waiting for server-side predictions.
  EXPECT_CALL(driver_, SetPasswordFillData).Times(0);
  CreateFormManager(observed_form_);

  std::map<FormSignature, FormPredictions> predictions = CreatePredictions(
      observed_form_, {std::make_pair(2, autofill::PASSWORD)});
  form_manager_->ProcessServerPredictions(predictions);
  Mock::VerifyAndClearExpectations(&driver_);

  // Expect filling without delay on receiving server predictions.
  EXPECT_CALL(driver_, SetPasswordFillData).Times(1);
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
  PasswordFormManager::set_wait_for_server_predictions_for_filling(false);
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  submitted_form_.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  submitted_form_.fields[kPasswordFieldIndex].value =
      saved_match_.password_value;

  // Tests that depending on whether we fill on page load or account select that
  // correct user action is recorded. Fill on account select is simulated by
  // pretending we are in incognito mode.
#if !BUILDFLAG(IS_IOS) && !defined(ANDROID)
  for (bool is_incognito : {false, true}) {
    EXPECT_CALL(client_, IsOffTheRecord).WillOnce(Return(is_incognito));
#endif
    form_manager_->Fill();
    EXPECT_TRUE(
        form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));
    CheckPendingCredentials(/* expected */ saved_match_,
                            form_manager_->GetPendingCredentials());
#if !BUILDFLAG(IS_IOS) && !defined(ANDROID)
  }
#endif
}

// Tests that when submitted credentials are equal to already saved PSL
// credentials.
TEST_P(PasswordFormManagerTest, CreatePendingCredentialsPSLMatchSaved) {
  PasswordForm expected = saved_match_;

  saved_match_.url = GURL("https://m.accounts.google.com/auth");
  saved_match_.signon_realm = "https://m.accounts.google.com/";
  saved_match_.match_type = PasswordForm::MatchType::kPSL;

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
  expected.password_value += u"1";

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
  submitted_form.fields[0].value = u"strongpassword";
  submitted_form.fields[1].value = u"verystrongpassword";

  PasswordForm expected = saved_match_;
  expected.password_value = u"verystrongpassword";

  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form, &driver_, nullptr));
  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
}

// Tests creating pending credentials when a change password form is submitted
// and there are multiple saved forms.
TEST_P(PasswordFormManagerTest, CreatePendingCredentialsUpdateMultipleSaved) {
  PasswordForm another_saved_match = saved_match_;
  another_saved_match.username_value += u"1";
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_, &another_saved_match});

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = u"strongpassword";
  submitted_form.fields[1].value = u"verystrongpassword";

  PasswordForm expected = saved_match_;
  expected.password_value = u"verystrongpassword";

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
  anonymous_signup.fields[2].value = u"a password";
  // Mark the password field as new-password.
  std::map<FormSignature, FormPredictions> predictions = CreatePredictions(
      observed_form_, {std::make_pair(2, autofill::ACCOUNT_CREATION_PASSWORD)});

  form_manager_->ProcessServerPredictions(predictions);

  EXPECT_TRUE(
      form_manager_->ProvisionallySave(anonymous_signup, &driver_, nullptr));
  EXPECT_EQ(u"a password",
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
  submitted_form.fields[kUsernameFieldIndex].value = u"username";
  submitted_form.fields[kPasswordFieldIndex].value = u"password";

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
  std::u16string new_username = saved_match_.username_value + u"1";
  std::u16string new_password = saved_match_.password_value + u"1";
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

  std::string expected_signon_realm =
      submitted_form.url.DeprecatedGetOriginAsURL().spec();
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
  std::u16string username = saved_match_.username_value;
  std::u16string new_password = saved_match_.password_value + u"1";
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
  saved_match_another_username.username_value += u"1";

  SetNonFederatedAndNotifyFetchCompleted(
      {&saved_match_, &not_best_saved_match, &saved_match_another_username});

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = saved_match_.password_value;
  std::u16string new_password = saved_match_.password_value + u"1";
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
  EXPECT_CALL(
      *client_.GetPasswordChangeSuccessTracker(),
      OnChangePasswordFlowCompleted(
          submitted_form.url, base::UTF16ToUTF8(saved_match_.username_value),
          PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen,
          /* phished= */ false));

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
    auto new_password = saved_match_.password_value + u"1";
    submitted_form.fields[1].value = new_password;

    EXPECT_TRUE(
        form_manager_->ProvisionallySave(submitted_form, &driver_, nullptr));

    std::map<std::u16string, autofill::ServerFieldType> expected_types;
    expected_types[u"password"] = autofill::PASSWORD;
    expected_types[u"password2"] = expected_vote;

    testing::InSequence in_sequence;
    EXPECT_CALL(
        mock_autofill_download_manager_,
        StartUploadRequest(UploadedAutofillTypesAre(expected_types), false, _,
                           _, true, nullptr, /*observer=*/IsNull()));
    if (expected_vote == autofill::NEW_PASSWORD) {
      // An unrelated |FIRST_USE| vote.
      EXPECT_CALL(mock_autofill_download_manager_,
                  StartUploadRequest(_, _, _, _, _, _, /*observer=*/IsNull()));
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

// When Chrome picks a wrong field as the username (e.g. full name) on a signup
// form, the user will probably type the stored password and overwrite the
// username with the correct value when the user will log in. At that moment,
// Chrome will check whether the new username value was typed on that sign-up
// form (highly likely, it was) and upload so called username correction vote
// for the field of the signup form where the correct username value was.
TEST_P(PasswordFormManagerTest, UsernameCorrectionVote) {
  // The credential was saved on a sign-up form, i.e. a form with many text
  // fields where is not trivial to pick the right field as username. Chrome
  // picked the wrong field as username - the full name field.
  saved_match_.username_value = u"John Smith";
  // The actual username (the email address) is among these values. They are all
  // values entered by the user on the signup form. Password form deserialized
  // from a form storage and doesn't contain field_renderer_id.
  saved_match_.all_alternative_usernames = {
      {AlternativeElement::Value(u"user@gmail.com"),
       autofill::FieldRendererId(), AlternativeElement::Name(u"email_field")},
      {AlternativeElement::Value(u"John Smith"), autofill::FieldRendererId(),
       AlternativeElement::Name(u"fname_field")},
      {AlternativeElement::Value(u"+1(650)000-0000"),
       autofill::FieldRendererId(), AlternativeElement::Name(u"phone_field")}};
  // Add fields because it is necessary for vote uploading.
  for (const AlternativeElement& alternative :
       saved_match_.all_alternative_usernames) {
    FormFieldData text_field;
    text_field.name = alternative.name;
    text_field.form_control_type = autofill::FormControlType::kInputText;
    saved_match_.form_data.fields.push_back(text_field);
  }
  FormFieldData password_field;
  password_field.name = saved_match_.password_element;
  password_field.form_control_type = autofill::FormControlType::kInputPassword;
  saved_match_.form_data.fields.push_back(password_field);
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  // On a login form, the user uses the password value.
  submitted_form_.fields[kPasswordFieldIndex].value =
      saved_match_.password_value;
  // The username should be corrected. To intensify testing, simulate that the
  // user changes the username value many times.
  for (const std::u16string& new_username_value :
       {saved_match_.all_alternative_usernames[2].value,
        saved_match_.all_alternative_usernames[1].value,
        saved_match_.all_alternative_usernames[0].value,
        std::u16string(u"random"), std::u16string(),
        saved_match_.all_alternative_usernames[0].value}) {
    submitted_form_.fields[kUsernameFieldIndex].value = new_username_value;
    EXPECT_TRUE(
        form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                         /*possible_username=*/nullptr));
  }

  testing::InSequence in_sequence;
  // Unrelated regular PASSWORD vote.
  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(_, _, _, _, _, _, _));
  std::map<std::u16string, autofill::ServerFieldType> expected_types;
  expected_types[saved_match_.all_alternative_usernames[0].name] =
      autofill::USERNAME;
  expected_types[saved_match_.password_element] =
      autofill::ACCOUNT_CREATION_PASSWORD;

  // The first key in the map should be
  // `saved_match_.form_data.fields[0].unique_renderer_id`, but the new
  // `unique_renderer_id` of value 1 should be assigned in
  // `GenerateSyntheticRenderIdsAndAssignThem()` in the flow
  // of `Save()`.
  VoteTypeMap expected_vote_types = {
      {autofill::FieldRendererId(1),
       AutofillUploadContents::Field::USERNAME_OVERWRITTEN}};

  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(
          AllOf(SignatureIs(CalculateFormSignature(saved_match_.form_data)),
                UploadedAutofillTypesAre(expected_types),
                VoteTypesAre(expected_vote_types)),
          _, _, _, _, _, _));

  form_manager_->Save();
}

// Checks votes uploading when user reuses credentials on login form. Simulates
// the whole flow by saving the same username and password from the sign-up form
// (`saved_match_`) on a different form (`submitted_form`).
TEST_P(PasswordFormManagerTest, CredentialsReusedVote) {
  // The credentials are saved on a sign-up form.
  saved_match_.username_value = u"username_value";
  saved_match_.all_alternative_usernames = {
      {AlternativeElement::Value(u"user@gmail.com"),
       autofill::FieldRendererId(), AlternativeElement::Name(u"email_field")},
      {AlternativeElement::Value(u"username_value"),
       autofill::FieldRendererId(),
       AlternativeElement::Name(u"username_field")}};

  // Add fields because it is necessary for vote uploading.
  for (const AlternativeElement& alternative :
       saved_match_.all_alternative_usernames) {
    FormFieldData field;
    field.name = alternative.name;
    field.form_control_type = autofill::FormControlType::kInputText;
    saved_match_.form_data.fields.push_back(field);
  }
  FormFieldData password_field;
  password_field.name = saved_match_.password_element;
  password_field.form_control_type = autofill::FormControlType::kInputPassword;
  saved_match_.form_data.fields.push_back(password_field);

  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  // On a login form, the user reuses the username value and password value.
  submitted_form_.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  submitted_form_.fields[kPasswordFieldIndex].value =
      saved_match_.password_value;

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               /*possible_username=*/nullptr));

  // Credentials saved on the signup form were reused on a login form. The vote
  // applies to the first (signup) form.
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(
          AllOf(SignatureIs(CalculateFormSignature(saved_match_.form_data)),
                VoteTypesAre(
                    {{saved_match_.form_data.fields[kUsernameFieldIndex]
                          .unique_renderer_id,
                      AutofillUploadContents::Field::CREDENTIALS_REUSED},
                     {saved_match_.form_data.fields[kPasswordFieldIndex]
                          .unique_renderer_id,
                      AutofillUploadContents::Field::CREDENTIALS_REUSED}})),
          _, _, _, _, _, _));

  // Saved credentials from the signup form were used for the first time on a
  // submitted form. The vote applies to the new form being submitted.
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(
          AllOf(SignatureIs(CalculateFormSignature(submitted_form_)),
                VoteTypesAre({{submitted_form_.fields[kPasswordFieldIndex]
                                   .unique_renderer_id,
                               AutofillUploadContents::Field::FIRST_USE},
                              {submitted_form_.fields[kUsernameFieldIndex]
                                   .unique_renderer_id,
                               AutofillUploadContents::Field::FIRST_USE}})),
          _, _, _, _, _, _));

  form_manager_->Save();
}

TEST_P(PasswordFormManagerTest, UpdateUsernameEmptyStore) {
  fetcher_->NotifyFetchCompleted();

  form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr);

  std::u16string new_username = parsed_submitted_form_.username_value + u"1";
  PasswordForm expected = parsed_submitted_form_;
  expected.username_value = new_username;
  expected.username_element.clear();

  form_manager_->OnUpdateUsernameFromPrompt(new_username);

  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
  EXPECT_TRUE(form_manager_->IsNewLogin());
}

TEST_P(PasswordFormManagerTest, UpdateUsernameToAnotherFieldValue) {
  fetcher_->NotifyFetchCompleted();

  std::u16string user_chosen_username = u"user_chosen_username";
  std::u16string automatically_chosen_username =
      u"automatically_chosen_username";
  submitted_form_.fields[0].value = user_chosen_username;
  submitted_form_.fields[1].value = automatically_chosen_username;
  form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr);

  EXPECT_EQ(automatically_chosen_username,
            form_manager_->GetPendingCredentials().username_value);

  form_manager_->OnUpdateUsernameFromPrompt(user_chosen_username);

  EXPECT_EQ(user_chosen_username,
            form_manager_->GetPendingCredentials().username_value);

  std::map<std::u16string, ServerFieldType> expected_types = {
      {u"firstname", autofill::USERNAME}, {u"password", autofill::PASSWORD}};
  VoteTypeMap expected_vote_types = {
      {submitted_form_.fields[0].unique_renderer_id,
       AutofillUploadContents::Field::USERNAME_EDITED}};
  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(AllOf(UploadedAutofillTypesAre(expected_types),
                                       HasGenerationVote(false),
                                       VoteTypesAre(expected_vote_types)),
                                 _, Contains(autofill::USERNAME), _, _, nullptr,
                                 /*observer=*/IsNull()));
  form_manager_->Save();
}

TEST_P(PasswordFormManagerTest, UpdateUsernameToAlreadyExisting) {
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr);

  std::u16string new_username = saved_match_.username_value;
  std::u16string expected_password = parsed_submitted_form_.password_value;
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

  std::u16string new_password = parsed_submitted_form_.password_value + u"1";
  PasswordForm expected = parsed_submitted_form_;
  expected.password_value = new_password;
  expected.password_element.clear();

  form_manager_->OnUpdatePasswordFromPrompt(new_password);

  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
  EXPECT_TRUE(form_manager_->IsNewLogin());

  // TODO(https://crbug.com/928690): implement not sending incorrect votes and
  // check that StartUploadRequest is not called.
  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(_, _, _, _, _, _, /*observer=*/IsNull()))
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
  std::u16string password = saved_match_.password_value;
  form_manager_->OnUpdatePasswordFromPrompt(password);

  CheckPendingCredentials(saved_match_, form_manager_->GetPendingCredentials());
  EXPECT_FALSE(form_manager_->IsNewLogin());
  EXPECT_FALSE(form_manager_->IsPasswordUpdate());
}

TEST_P(PasswordFormManagerTest, UpdatePasswordValueToUnknownValueFromPrompt) {
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  // Emulate submitting form that updates the password for a known username.
  submitted_form_.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  submitted_form_.fields[kPasswordFieldIndex].autocomplete_attribute =
      "new-password";
  submitted_form_.fields[kPasswordFieldIndex].value =
      u"new_password_field_value";
  form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr);

  // The user changes password to a prevuiously unseen one.
  form_manager_->OnUpdatePasswordFromPrompt(u"totally_unexpected_value");
  EXPECT_TRUE(form_manager_->IsPasswordUpdate());

  // Since the user has modified the password value, the password field was
  // likely picked wrong. Make sure votes for password field and password
  // generation attributes are not uploaded.
  ServerFieldTypeSet expected_types = {autofill::USERNAME};
  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(HasPasswordAttributesVote(false), _,
                                 expected_types, _, _, _, _));
  form_manager_->Save();
}

TEST_P(PasswordFormManagerTest, UpdatePasswordValueMultiplePasswordFields) {
  FormData form = observed_form_only_password_fields_;

  CreateFormManager(form);
  fetcher_->NotifyFetchCompleted();
  std::u16string password = u"password1";
  std::u16string pin = u"pin";
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
  std::map<std::u16string, ServerFieldType> expected_types;
  expected_types[expected.password_element] = autofill::PASSWORD;

  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(UploadedAutofillTypesAre(expected_types), false, _, _,
                         true, nullptr, /*observer=*/IsNull()));

  // Check that the password which was chosen by the user is saved.
  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());
  PasswordForm saved_form;
  EXPECT_CALL(form_saver, Save(_, _, _)).WillOnce(SaveArg<0>(&saved_form));

  form_manager_->Save();
  CheckPendingCredentials(expected, saved_form);
}

TEST_P(PasswordFormManagerTest, Blocklist) {
  fetcher_->NotifyFetchCompleted();

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

  PasswordForm actual_blocklisted_form =
      password_manager_util::MakeNormalizedBlocklistedForm(
          PasswordFormDigest(observed_form_));
  EXPECT_CALL(form_saver, Blocklist(PasswordFormDigest(observed_form_)))
      .WillOnce(Return(actual_blocklisted_form));

  form_manager_->Blocklist();
  EXPECT_TRUE(form_manager_->IsBlocklisted());
}

TEST_P(PasswordFormManagerTest, Clone) {
  fetcher_->NotifyFetchCompleted();

  // Provisionally save in order to create pending credentials.
  ASSERT_TRUE(
      form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));

  std::unique_ptr<PasswordFormManager> cloned_manager = form_manager_->Clone();

  EXPECT_TRUE(
      cloned_manager->DoesManage(observed_form_.unique_renderer_id, nullptr));
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
  EXPECT_CALL(driver_, SetPasswordFillData);
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

  form_manager_->SetGenerationPopupWasShown(PasswordGenerationType::kAutomatic);

  // Check that the generated password is presaved.
  PasswordForm saved_form;
  EXPECT_CALL(form_saver, Save(_, IsEmpty(), std::u16string()))
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
  form_with_generated_password.password_value += u"1";
  form_data.fields[kPasswordFieldIndex].value =
      form_with_generated_password.password_value;
  EXPECT_CALL(form_saver,
              UpdateReplace(_, IsEmpty(), testing::Eq(u""),
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
      absl::make_optional(1u) /* shown automatically */,
      1 /* password generated */,
      absl::make_optional(1u) /* password modified */};

  CheckPasswordGenerationUKM(test_ukm_recorder, expected_metrics);
}

TEST_P(PasswordFormManagerTest, PresaveGenerated_ModifiedUsername) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  fetcher_->NotifyFetchCompleted();

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

  form_manager_->SetGenerationPopupWasShown(PasswordGenerationType::kAutomatic);

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
  form_with_generated_password.username_value += u"1";
  form_data.fields[kUsernameFieldIndex].value =
      form_with_generated_password.username_value;

  EXPECT_CALL(form_saver, UpdateReplace(_, IsEmpty(), testing::Eq(u""),
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
      absl::make_optional(1u) /* shown automatically */,
      1 /* password generated */,
      absl::make_optional(0u) /* password modified */};

  CheckPasswordGenerationUKM(test_ukm_recorder, expected_metrics);
}

TEST_P(PasswordFormManagerTest, GeneratedPasswordWhichIsNotInFormData) {
  fetcher_->NotifyFetchCompleted();
  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

  // Create a password form such that |form_data| do not contain the generated
  // password.
  PasswordForm form_with_generated_password;
  form_with_generated_password.form_data = submitted_form_;
  const std::u16string generated_password = u"gen_pw";
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
  EXPECT_CALL(form_saver, UpdateReplace(_, IsEmpty(), testing::Eq(u""),
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

  // Create a password form with url. Other members will be empty. On this form
  // the form parser should fail. The url is required for LogFormData to work.
  PasswordForm form;
  const std::u16string generated_password = u"gen_pw";
  form.password_value = generated_password;
  form.form_data.url = GURL("https://accounts.google.com/a/ServiceLoginAuth");

  // Check that nevertheless the generated password is presaved.
  PasswordForm saved_form;
  EXPECT_CALL(form_saver, Save(_, IsEmpty(), std::u16string()))
      .WillOnce(SaveArg<0>(&saved_form));
  form_manager_->PresaveGeneratedPassword(form.form_data, form.password_value);
  EXPECT_EQ(generated_password, saved_form.password_value);
}

TEST_P(PasswordFormManagerTest, PasswordNoLongerGenerated) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  fetcher_->NotifyFetchCompleted();

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());
  form_manager_->SetGenerationPopupWasShown(PasswordGenerationType::kManual);

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
      absl::make_optional(2u) /* shown manually */,
      0 /* password generated */,
      {} /* generated password is not modified */};

  CheckPasswordGenerationUKM(test_ukm_recorder, expected_metrics);
}

TEST_P(PasswordFormManagerTest, PresaveGeneratedPasswordExistingCredential) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

  form_manager_->SetGenerationPopupWasShown(PasswordGenerationType::kAutomatic);

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
    form_data.fields[kPasswordFieldIndex].value += u"1";
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
    form_data.fields[kPasswordFieldIndex].value += u"2";
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

TEST_P(PasswordFormManagerTest, HasObservedFormChangedRendererIds) {
  CreateFormManager(observed_form_);
  base::HistogramTester histogram_tester;

  FormData form = observed_form_;
  form.fields[kUsernameFieldIndex].unique_renderer_id.value() += 100;
  EXPECT_TRUE(HasObservedFormChanged(form, *form_manager_));
  form_manager_.reset();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.DynamicFormChanges",
      PasswordFormMetricsRecorder::kRendererFieldIDs, 1);
}

TEST_P(PasswordFormManagerTest, HasObservedFormChangedNames) {
  CreateFormManager(observed_form_);
  base::HistogramTester histogram_tester;

  FormData form = observed_form_;
  form.fields[kUsernameFieldIndex].name += u"123";
  EXPECT_TRUE(HasObservedFormChanged(form, *form_manager_));
  form_manager_.reset();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.DynamicFormChanges",
      PasswordFormMetricsRecorder::kFormFieldNames, 1);
}

TEST_P(PasswordFormManagerTest, HasObservedFormChangedAutocompleteAttribute) {
  CreateFormManager(observed_form_);
  base::HistogramTester histogram_tester;

  FormData form = observed_form_;
  form.fields[kUsernameFieldIndex].autocomplete_attribute += "...";
  EXPECT_TRUE(HasObservedFormChanged(form, *form_manager_));
  form_manager_.reset();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.DynamicFormChanges",
      PasswordFormMetricsRecorder::kAutocompleteAttributes, 1);
}

TEST_P(PasswordFormManagerTest, HasObservedFormChangedFormControlsType) {
  CreateFormManager(observed_form_);
  base::HistogramTester histogram_tester;

  FormData form = observed_form_;
  form.fields[kUsernameFieldIndex].form_control_type =
      autofill::FormControlType::kInputPassword;
  EXPECT_TRUE(HasObservedFormChanged(form, *form_manager_));
  form_manager_.reset();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.DynamicFormChanges",
      PasswordFormMetricsRecorder::kFormControlTypes, 1);
}

TEST_P(PasswordFormManagerTest, HasObservedFormChangedFieldsNumber) {
  CreateFormManager(observed_form_);
  base::HistogramTester histogram_tester;

  FormData form = observed_form_;
  form.fields.push_back(autofill::test::CreateTestFormField(
      "label", "new field", "", autofill::FormControlType::kInputText));
  EXPECT_TRUE(HasObservedFormChanged(form, *form_manager_));
  form_manager_.reset();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.DynamicFormChanges",
      PasswordFormMetricsRecorder::kFieldsNumber, 1);
}

TEST_P(PasswordFormManagerTest, HasObservedFormChangedCssClasses) {
  CreateFormManager(observed_form_);
  base::HistogramTester histogram_tester;

  FormData form = observed_form_;
  form.fields[kUsernameFieldIndex].css_classes = u"class1";
  EXPECT_FALSE(HasObservedFormChanged(form, *form_manager_));
  form_manager_.reset();

  histogram_tester.ExpectUniqueSample("PasswordManager.DynamicFormChanges", 0,
                                      1);
}

TEST_P(PasswordFormManagerTest, UpdateFormAndFill) {
  CreateFormManager(observed_form_);
  EXPECT_CALL(driver_, SetPasswordFillData);
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});
  task_environment_.FastForwardUntilNoTasksRemain();
  Mock::VerifyAndClearExpectations(&driver_);

  FormData form = observed_form_;
  form.fields[kUsernameFieldIndex].unique_renderer_id.value() += 1000;
  form.fields[kUsernameFieldIndex].name += u"1";
  form.fields[kUsernameFieldIndex].id_attribute += u"1";
  form.fields[kPasswordFieldIndex].unique_renderer_id.value() += 1000;

  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, SetPasswordFillData).WillOnce(SaveArg<0>(&fill_data));
  form_manager_->UpdateFormManagerWithFormChanges(form, {});
  form_manager_->Fill();
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_EQ(form.fields[kUsernameFieldIndex].unique_renderer_id,
            fill_data.username_element_renderer_id);
  EXPECT_EQ(saved_match_.username_value,
            fill_data.preferred_login.username_value);
  EXPECT_EQ(form.fields[kPasswordFieldIndex].unique_renderer_id,
            fill_data.password_element_renderer_id);
  EXPECT_EQ(saved_match_.password_value,
            fill_data.preferred_login.password_value);
}

TEST_P(PasswordFormManagerTest, FillWaitsForServerPredictions) {
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  EXPECT_CALL(driver_, SetPasswordFillData).Times(0);
  form_manager_->Fill();
  Mock::VerifyAndClearExpectations(&driver_);

  // Check that the changed form is filled after the filling timeout expires.
  EXPECT_CALL(driver_, SetPasswordFillData);
  task_environment_.FastForwardBy(kMaxFillingDelayForAsyncPredictions);
}

TEST_P(PasswordFormManagerTest, RepeatedFillDoesNotResetTimer) {
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  EXPECT_CALL(driver_, SetPasswordFillData).Times(0);
  form_manager_->Fill();
  task_environment_.FastForwardBy(kMaxFillingDelayForAsyncPredictions / 2);
  form_manager_->Fill();

  // Check that the fill call is not delayed further.
  EXPECT_CALL(driver_, SetPasswordFillData);
  task_environment_.FastForwardBy(kMaxFillingDelayForAsyncPredictions / 2);
}

TEST_P(PasswordFormManagerTest, UpdateFormManagerWithFormChangesResetsTimer) {
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  FormData changed_form = observed_form_;
  changed_form.fields[kUsernameFieldIndex].unique_renderer_id.value() += 1000;
  changed_form.fields[kPasswordFieldIndex].unique_renderer_id.value() += 1000;

  // Check that no filling happens until server predictions arrive or the
  // filling timeout expires.
  EXPECT_CALL(driver_, SetPasswordFillData).Times(0);

  // Wait half-delay time before updating form.
  task_environment_.FastForwardBy(kMaxFillingDelayForAsyncPredictions / 2);

  // Updating form should cancel the previous task for fill and start a new
  // delayed fill task for waiting server-side predictions.
  form_manager_->UpdateFormManagerWithFormChanges(changed_form, {});
  form_manager_->Fill();

  // Fire the cancelled fill task should do nothing.
  task_environment_.FastForwardBy(kMaxFillingDelayForAsyncPredictions / 2);

  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, SetPasswordFillData).WillOnce(SaveArg<0>(&fill_data));

  // Check that the new fill task triggers form filling.
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(changed_form.fields[kUsernameFieldIndex].unique_renderer_id,
            fill_data.username_element_renderer_id);
  EXPECT_EQ(changed_form.fields[kPasswordFieldIndex].unique_renderer_id,
            fill_data.password_element_renderer_id);
}

TEST_P(PasswordFormManagerTest, Update) {
  PasswordForm not_best_saved_match = saved_match_;
  PasswordForm saved_match_another_username = saved_match_;
  saved_match_another_username.username_value += u"1";
  SetNonFederatedAndNotifyFetchCompleted(
      {&saved_match_, &saved_match_another_username});

  FormData submitted_form = observed_form_;
  std::u16string username = saved_match_.username_value;
  std::u16string new_password = saved_match_.password_value + u"1";
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
  EXPECT_CALL(
      *client_.GetPasswordChangeSuccessTracker(),
      OnChangePasswordFlowCompleted(
          submitted_form.url, base::UTF16ToUTF8(username),
          PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen,
          /* phished= */ false));
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

    EXPECT_CALL(
        mock_autofill_download_manager_,
        StartUploadRequest(PasswordsWereRevealed(password_revealed), false, _,
                           _, true, nullptr, /*observer=*/IsNull()));
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
      form_manager_->SetGenerationElement(FieldRendererId(4));
      form_manager_->SetGenerationPopupWasShown(
          PasswordGenerationType::kAutomatic);
    }
    EXPECT_TRUE(
        form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));

    EXPECT_CALL(mock_autofill_download_manager_,
                StartUploadRequest(HasGenerationVote(true), false, _, _, true,
                                   nullptr, /*observer=*/IsNull()))
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
      form_manager_->SetGenerationElement(FieldRendererId(4));
      form_manager_->SetGenerationPopupWasShown(
          PasswordGenerationType::kAutomatic);
    }
    EXPECT_TRUE(
        form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr));

    EXPECT_CALL(mock_autofill_download_manager_,
                StartUploadRequest(HasGenerationVote(true), false, _, _, true,
                                   nullptr, /*observer=*/IsNull()))
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
    EXPECT_CALL(driver_, SetPasswordFillData).Times(0);
    EXPECT_CALL(client_, AutofillHttpAuth(_, _)).Times(0);

    CreateFormManagerForNonWebForm(http_auth_form);
    MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

    std::vector<const PasswordForm*> saved_matches;
    if (html_credentials_saved)
      saved_matches.push_back(&saved_match_);
    SetNonFederatedAndNotifyFetchCompleted(saved_matches);

    std::u16string username = u"user1";
    std::u16string password = u"pass1";
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

  const std::u16string username = u"user1";
  const std::u16string password = u"pass1";
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
  const std::u16string username = u"user1";
  const std::u16string password = u"pass1";
  saved_http_auth_form.username_value = username;
  saved_http_auth_form.password_value = password;
  EXPECT_CALL(client_, AutofillHttpAuth(saved_http_auth_form, _)).Times(1);
  SetNonFederatedAndNotifyFetchCompleted({&saved_http_auth_form});

  // Check that if new password is submitted, then |form_manager_| is in state
  // password overridden.
  PasswordForm submitted_http_auth_form = saved_http_auth_form;
  std::u16string new_password = password + u"1";
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

TEST_P(PasswordFormManagerTest, BlocklistHttpAuthCredentials) {
  PasswordForm http_auth_form = parsed_observed_form_;
  http_auth_form.signon_realm += "my-auth-realm";
  http_auth_form.scheme = PasswordForm::Scheme::kBasic;

  CreateFormManagerForNonWebForm(http_auth_form);
  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

  // Simulate that the user submits http auth credentials.
  http_auth_form.username_value = u"user1";
  http_auth_form.password_value = u"pass1";
  ASSERT_TRUE(form_manager_->ProvisionallySaveHttpAuthForm(http_auth_form));

  // Simulate that the user clicks never.
  PasswordForm blocklisted_form;
  EXPECT_CALL(form_saver, Blocklist(PasswordFormDigest(http_auth_form)));
  form_manager_->OnNeverClicked();
}

#if BUILDFLAG(IS_IOS)
TEST_P(PasswordFormManagerTest, iOSPresavedGeneratedPassword) {
  fetcher_->NotifyFetchCompleted();
  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

  FormData form_to_presave = observed_form_;
  const std::u16string typed_username = u"user1";
  FormFieldData& username_field = form_to_presave.fields[kUsernameFieldIndex];
  FormFieldData& password_field = form_to_presave.fields[kPasswordFieldIndex];
  username_field.value = typed_username;
  password_field.value = u"not_password";
  // Use |generated_password| different from value in field to test that the
  // generated password is saved.
  const std::u16string generated_password = u"gen_pw";
  FieldRendererId generation_element = password_field.unique_renderer_id;
  form_manager_->SetGenerationElement(generation_element);

  PasswordForm saved_form;
  EXPECT_CALL(form_saver, Save(_, IsEmpty(), std::u16string()))
      .WillOnce(SaveArg<0>(&saved_form));
  form_manager_->PresaveGeneratedPassword(form_to_presave, generated_password);
  EXPECT_EQ(generated_password, saved_form.password_value);

  Mock::VerifyAndClearExpectations(&form_saver);

  const std::u16string changed_password = generated_password + u"1";
  EXPECT_CALL(form_saver, UpdateReplace(_, _, std::u16string(), _))
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
  const std::u16string new_field_value = u"some_password";

  // Check that nothing is saved on changing password, in case when there was no
  // pre-saving.
  EXPECT_CALL(form_saver, Save).Times(0);
  form_manager_->UpdateStateOnUserInput(observed_form_.unique_renderer_id,
                                        password_field, new_field_value);

  EXPECT_EQ(new_field_value,
            form_manager_->observed_form()->fields[kPasswordFieldIndex].value);
}

TEST_P(PasswordFormManagerTest, iOSUsingFieldDataManagerData) {
  CreateFormManager(observed_form_);

  auto field_data_manager = base::MakeRefCounted<autofill::FieldDataManager>();
  field_data_manager->UpdateFieldDataMap(
      observed_form_.fields[1].unique_renderer_id, u"typed_username",
      FieldPropertiesFlags::kUserTyped);
  field_data_manager->UpdateFieldDataMap(
      observed_form_.fields[2].unique_renderer_id, u"autofilled_pw",
      FieldPropertiesFlags::kAutofilledOnUserTrigger);

  form_manager_->ProvisionallySaveFieldDataManagerInfo(*field_data_manager,
                                                       &driver_);

  EXPECT_EQ(form_manager_->observed_form()->fields[1].user_input,
            u"typed_username");
  EXPECT_EQ(form_manager_->observed_form()->fields[1].properties_mask,
            FieldPropertiesFlags::kUserTyped);

  EXPECT_EQ(form_manager_->observed_form()->fields[2].user_input,
            u"autofilled_pw");
  EXPECT_EQ(form_manager_->observed_form()->fields[2].properties_mask,
            FieldPropertiesFlags::kAutofilledOnUserTrigger);
}

#endif  // BUILDFLAG(IS_IOS)

// Tests provisional saving of credentials during username first flow.
TEST_P(PasswordFormManagerTest, UsernameFirstFlowProvisionalSave) {
  CreateFormManager(observed_form_only_password_fields_);
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  // Create possible username data.
  constexpr autofill::FieldRendererId kUsernameFieldRendererId(101);
  const std::u16string possible_username = u"test@example.com";
  PossibleUsernameData possible_username_data(
      saved_match_.signon_realm, kUsernameFieldRendererId, possible_username,
      base::Time::Now(), /*driver_id=*/0,
      /*autocomplete_attribute_has_username=*/false, /*is_likely_otp=*/false);

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = u"strongpassword";

  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames = MakePossibleUsernamesCache({possible_username_data});

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               &possible_usernames));

  // Without server predictions the username should not be taken from the
  // single username form.
  EXPECT_EQ(saved_match_.username_value,
            form_manager_->GetPendingCredentials().username_value);

  // Create form predictions and set them to |possible_username_data|.
  possible_username_data.form_predictions = MakeSingleUsernamePredictions(
      kSingleUsernameFormSignature, kSingleUsernameFieldRendererId,
      kSingleUsernameFieldSignature);
  possible_usernames.Put(
      {{possible_username_data.driver_id, possible_username_data.renderer_id},
       possible_username_data});

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               &possible_usernames));

  // Check that a username is chosen from |possible_username_data| now.
  EXPECT_EQ(possible_username,
            form_manager_->GetPendingCredentials().username_value);
}

// Tests that username is not taken if domains of possible username field
// and submitted password form are different.
TEST_P(PasswordFormManagerTest, UsernameFirstFlowDifferentDomains) {
  CreateFormManager(observed_form_only_password_fields_);
  fetcher_->NotifyFetchCompleted();

  // Create possible username data.
  constexpr autofill::FieldRendererId kUsernameFieldRendererId(101);
  std::u16string possible_username = u"possible_username";
  PossibleUsernameData possible_username_data(
      "https://another.domain.com", kUsernameFieldRendererId, possible_username,
      base::Time::Now(),
      /*driver_id=*/0, /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);
  possible_username_data.form_predictions = MakeSingleUsernamePredictions(
      kSingleUsernameFormSignature, kSingleUsernameFieldRendererId,
      kSingleUsernameFieldSignature);
  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames = MakePossibleUsernamesCache({possible_username_data});

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = u"strongpassword";

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               &possible_usernames));

  // |possible_username_data| has different domain than |submitted_form|. Check
  // that no username is chosen.
  EXPECT_TRUE(form_manager_->GetPendingCredentials().username_value.empty());
}

// Tests that possible username with server prediction will be suggested in the
// save prompt, if the possible username was found on a domain that is an eTLD+1
// match.
TEST_P(PasswordFormManagerTest, PossibleUsernameFromPSLMatch) {
  CreateFormManager(observed_form_only_password_fields_);
  fetcher_->NotifyFetchCompleted();

  // Create possible username data.
  std::u16string possible_username = u"possible_username";
  PossibleUsernameData possible_username_data(
      psl_saved_match_.signon_realm, kSingleUsernameFieldRendererId,
      possible_username, base::Time::Now(),
      /*driver_id=*/0, /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);
  possible_username_data.form_predictions = MakeSingleUsernamePredictions(
      kSingleUsernameFormSignature, kSingleUsernameFieldRendererId,
      kSingleUsernameFieldSignature);
  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames = MakePossibleUsernamesCache({possible_username_data});

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = u"strongpassword";

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               &possible_usernames));

  // |possible_username_data| has different domain than |submitted_form|. Check
  // that username outside of the password form is still chosen.
  EXPECT_EQ(possible_username,
            form_manager_->GetPendingCredentials().username_value);
}

// Tests that username is not taken during the sign up flow (when there is no
// current password field in the password form).
TEST_P(PasswordFormManagerTest, UsernameFirstFlowSignupForm) {
  CreateFormManager(observed_form_only_password_fields_);
  fetcher_->NotifyFetchCompleted();

  // Create possible username data.
  std::u16string possible_username = u"possible_username";
  PossibleUsernameData possible_username_data(
      "https://another.domain.com", kSingleUsernameFieldRendererId,
      possible_username, base::Time::Now(),
      /*driver_id=*/0, /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);
  possible_username_data.form_predictions = MakeSingleUsernamePredictions(
      kSingleUsernameFormSignature, kSingleUsernameFieldRendererId,
      kSingleUsernameFieldSignature);
  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames = MakePossibleUsernamesCache({possible_username_data});

  FormData submitted_form = observed_form_only_password_fields_;
  // Imitate sign-up flow: the only filled password field is a new password.
  submitted_form.fields[0].autocomplete_attribute = "new-password";
  submitted_form.fields[0].value = u"strongpassword";

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               &possible_usernames));

  // Check that single username is not chosen.
  EXPECT_TRUE(form_manager_->GetPendingCredentials().username_value.empty());
}

// Tests that no UFF vote can be sent on the username field when it is in the
// submitted password form.
TEST_P(PasswordFormManagerTest, UsernameFirstFlowUsernameInThePasswordForm) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kUsernameFirstFlowFallbackCrowdsourcing},
      /*disabled_features=*/{});

  CreateFormManager(observed_form_);
  fetcher_->NotifyFetchCompleted();

  // Create possible username data for a username field from `observed_form_`.
  autofill::FieldRendererId kUsernameFieldRendererId =
      observed_form_.fields[1].unique_renderer_id;
  std::u16string possible_username = u"possible_username";
  PossibleUsernameData possible_username_data(
      GetSignonRealm(observed_form_.url), kUsernameFieldRendererId,
      possible_username, base::Time::Now(),
      /*driver_id=*/0, /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);
  FormPredictions predictions;
  predictions.form_signature = CalculateFormSignature(observed_form_);
  PasswordFieldPrediction field_prediction;
  field_prediction.renderer_id = kUsernameFieldRendererId;
  field_prediction.signature =
      CalculateFieldSignatureForField(observed_form_.fields[0]);
  field_prediction.type = autofill::UNKNOWN_TYPE;
  predictions.fields.push_back(field_prediction);
  possible_username_data.form_predictions = predictions;
  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames = MakePossibleUsernamesCache({possible_username_data});

  // Simulate submitting the form.
  FormData submitted_form = observed_form_;
  submitted_form.fields[1].value = possible_username;
  submitted_form.fields[2].value = u"strongpassword";
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               &possible_usernames));

  // Check that no SINGLE_USERNAME vote is sent (PASSWORD != SINGLE_USERNAME).
  auto expected_votes = ServerFieldTypeSet{autofill::PASSWORD};
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(SignatureIs(CalculateFormSignature(submitted_form)), _,
                         expected_votes, _, _, _, _));
  form_manager_->Save();
}

// Tests that username is taken and votes are uploaded during username first
// flow both on password saving and updating.
TEST_P(PasswordFormManagerTest, UsernameFirstFlow) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kUsernameFirstFlowFallbackCrowdsourcing},
      /*disabled_features=*/{});

  for (bool is_password_update : {false, true}) {
    CreateFormManager(observed_form_only_password_fields_);
    if (!is_password_update) {
      fetcher_->NotifyFetchCompleted();
    } else {
      SetNonFederatedAndNotifyFetchCompleted({&saved_match_});
    }

    // Create possible username data.
    const std::u16string possible_username =
        is_password_update ? saved_match_.username_value : u"possible_username";
    PossibleUsernameData possible_username_data(
        saved_match_.signon_realm, kSingleUsernameFieldRendererId,
        possible_username, base::Time::Now(),
        /*driver_id=*/0,
        /*autocomplete_attribute_has_username=*/false, /*is_likely_otp=*/false);
    possible_username_data.form_predictions = MakeSingleUsernamePredictions(
        kSingleUsernameFormSignature, kSingleUsernameFieldRendererId,
        kSingleUsernameFieldSignature);
    base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
        possible_usernames =
            MakePossibleUsernamesCache({possible_username_data});

    // Simulate submitting a form without a username. Data from
    // |possible_username_data| will be taken for setting username.
    FormData submitted_form = observed_form_only_password_fields_;
    submitted_form.fields[0].value = u"oldpassword";
    // Simulate password reset flow for password update case.
    if (is_password_update) {
      submitted_form.fields[1].value = u"newpassword";
    }

    ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                                 &possible_usernames));
    EXPECT_EQ(form_manager_->IsPasswordUpdate(), is_password_update);

    // Check that uploads for both username and password form happen.
    testing::InSequence in_sequence;

    // Upload username first flow votes on the username form.
#if !BUILDFLAG(IS_ANDROID)
    EXPECT_CALL(mock_autofill_download_manager_,
                StartUploadRequest(SignatureIs(kSingleUsernameFormSignature),
                                   false, ServerFieldTypeSet{SINGLE_USERNAME},
                                   _, true, nullptr, /*observer=*/IsNull()));
#endif  // !BUILDFLAG(IS_ANDROID)

    // Upload username first flow votes on the password form.
    autofill::AutofillUploadContents::SingleUsernameData
        expected_single_username_data;
    expected_single_username_data.set_username_form_signature(
        kSingleUsernameFormSignature.value());
    expected_single_username_data.set_username_field_signature(
        kSingleUsernameFieldSignature.value());
    expected_single_username_data.set_value_type(
        is_password_update
            ? autofill::AutofillUploadContents::STORED_FOR_CURRENT_DOMAIN
            : autofill::AutofillUploadContents::USERNAME_LIKE);
    expected_single_username_data.set_prompt_edit(
        autofill::AutofillUploadContents::EDITED_POSITIVE);
    EXPECT_CALL(mock_autofill_download_manager_,
                StartUploadRequest(
                    AllOf(SignatureIs(CalculateFormSignature(submitted_form)),
                          UploadedSingleUsernameDataIs(
                              {expected_single_username_data})),
                    _, _, _, _, _, /*observer=*/IsNull()));

    if (is_password_update) {
      // Expect another upload for first login votes. This upload is not related
      // to UFF, so it should not contain single username data.
      EXPECT_CALL(mock_autofill_download_manager_,
                  StartUploadRequest(SingleUsernameDataNotUploaded(), _, _, _,
                                     _, _, /*observer=*/IsNull()));
    }

    base::HistogramTester histogram_tester;

    if (!is_password_update)
      form_manager_->Save();
    else
      form_manager_->Update(saved_match_);

#if !BUILDFLAG(IS_ANDROID)
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.SingleUsername.PasswordFormHadUsernameField", 0, 1);
#else
    histogram_tester.ExpectTotalCount(
        "PasswordManager.SingleUsername.PasswordFormHadUsernameField", 0);
#endif
    Mock::VerifyAndClearExpectations(&mock_autofill_download_manager_);
  }
}

// Tests that if username matches with single username from previous form, vote
// is set.
TEST_P(PasswordFormManagerTest, UsernameFirstFlowWithPrefilledUsername) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kUsernameFirstFlowFallbackCrowdsourcing},
      /*disabled_features=*/{});

  CreateFormManager(submitted_form_);
  fetcher_->NotifyFetchCompleted();

  // Create possible username data.
  PossibleUsernameData possible_username_data(
      saved_match_.signon_realm, kSingleUsernameFieldRendererId,
      submitted_form_.fields[kUsernameFieldIndex].value, base::Time::Now(),
      /*driver_id=*/0,
      /*autocomplete_attribute_has_username=*/false, /*is_likely_otp=*/false);
  possible_username_data.form_predictions = MakeSingleUsernamePredictions(
      kSingleUsernameFormSignature, kSingleUsernameFieldRendererId,
      kSingleUsernameFieldSignature);
  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames = MakePossibleUsernamesCache({possible_username_data});

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               &possible_usernames));

  // Check that uploads for both single username and sign-up form happen.
  testing::InSequence in_sequence;

  // Upload username first flow vote on the single username form.
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(SignatureIs(kSingleUsernameFormSignature),
                                 false, ServerFieldTypeSet{SINGLE_USERNAME}, _,
                                 true, nullptr, /*observer=*/IsNull()));
#endif  // !BUILDFLAG(IS_ANDROID)

  // Upload username first flow vote on the sign-up form.
  autofill::AutofillUploadContents::SingleUsernameData
      expected_single_username_data;
  expected_single_username_data.set_username_form_signature(
      kSingleUsernameFormSignature.value());
  expected_single_username_data.set_username_field_signature(
      kSingleUsernameFieldSignature.value());
  expected_single_username_data.set_value_type(
      autofill::AutofillUploadContents::VALUE_WITH_NO_WHITESPACE);
  expected_single_username_data.set_prompt_edit(
      autofill::AutofillUploadContents::NOT_EDITED_POSITIVE);
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(
          AllOf(SignatureIs(CalculateFormSignature(submitted_form_)),
                UploadedSingleUsernameDataIs({expected_single_username_data})),
          _, _, _, _, _, /*observer=*/IsNull()));

  // Simulate showing the prompt and saving the suggested value.
  form_manager_->SaveSuggestedUsernameValueToVotesUploader();

  base::HistogramTester histogram_tester;

  form_manager_->Save();

#if !BUILDFLAG(IS_ANDROID)
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SingleUsername.PasswordFormHadUsernameField", 1, 1);
#else
  histogram_tester.ExpectTotalCount(
      "PasswordManager.SingleUsername.PasswordFormHadUsernameField", 0);
#endif
  Mock::VerifyAndClearExpectations(&mock_autofill_download_manager_);
}

// Tests that when the save/update prompt suggests single username value and the
// user edits username prompt to the value of one of the text fields found in
// the password form negative in form overrule vote is sent.
TEST_P(PasswordFormManagerTest, UsernameFirstFlowInFormOverruleVotes) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kUsernameFirstFlowFallbackCrowdsourcing,
                            features::kUsernameFirstFlowWithIntermediateValues,
                            features::
                                kUsernameFirstFlowWithIntermediateValuesVoting,
                            features::kUsernameFirstFlowStoreSeveralValues},
      /*disabled_features=*/{});
  CreateFormManager(submitted_form_);
  fetcher_->NotifyFetchCompleted();

  // Simulate user input in a single text field in a single username form.
  constexpr char16_t kPossibleUsername[] = u"possible_username";
  PossibleUsernameData possible_username_data(
      saved_match_.signon_realm, kSingleUsernameFieldRendererId,
      kPossibleUsername, base::Time::Now(),
      /*driver_id=*/0, /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);
  possible_username_data.form_predictions = MakeSingleUsernamePredictions(
      kSingleUsernameFormSignature, kSingleUsernameFieldRendererId,
      kSingleUsernameFieldSignature);

  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames = MakePossibleUsernamesCache({possible_username_data});

  // Simulate submitting a password form.
  submitted_form_.fields[kPasswordFieldIndex].value = u"strongpassword";
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               &possible_usernames));
  form_manager_->SaveSuggestedUsernameValueToVotesUploader();

  // Simulate the user modifying the username in the prompt.
  form_manager_->OnUpdateUsernameFromPrompt(
      submitted_form_.fields[kUsernameFieldIndex].value);

#if !BUILDFLAG(IS_ANDROID)
  // Expect a negative `IN_FORM_OVERRULE` vote on the username form.
  ExpectSingleUsernameUpload(kSingleUsernameFormSignature,
                             AutofillUploadContents::Field::IN_FORM_OVERRULE,
                             autofill::NOT_USERNAME);
#endif

  // Expect upload for the password form. This upload is unrelated to UFF: it
  // is a result of saving a new password on the password form.
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(SignatureIs(CalculateFormSignature(submitted_form_)),
                         _, _, _, _, _, _));

  form_manager_->Save();
}

// Tests scenario when the save/update prompt suggests some value typed in the
// password form and the user edits username prompt to the value of one of the
// text fields found outside of the password form. Positive in form overrule
// single username vote must be sent.
TEST_P(PasswordFormManagerTest, UsernameFirstFlowPositiveInFormOverruleVote) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kUsernameFirstFlowFallbackCrowdsourcing,
                            features::kUsernameFirstFlowWithIntermediateValues,
                            features::
                                kUsernameFirstFlowWithIntermediateValuesVoting,
                            features::kUsernameFirstFlowStoreSeveralValues},
      /*disabled_features=*/{});

  CreateFormManager(submitted_form_);
  fetcher_->NotifyFetchCompleted();

  // Simulate user input in a single text field in a single username form.
  constexpr char16_t kPossibleUsername[] = u"possible_username";
  PossibleUsernameData possible_username_data(
      saved_match_.signon_realm, kSingleUsernameFieldRendererId,
      kPossibleUsername, base::Time::Now(),
      /*driver_id=*/0, /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);
  possible_username_data.form_predictions = MakeSingleUsernamePredictions(
      kSingleUsernameFormSignature, kSingleUsernameFieldRendererId,
      kSingleUsernameFieldSignature, /*has_single_username_prediction=*/false);

  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames = MakePossibleUsernamesCache({possible_username_data});

  // Simulate submitting a password form.
  submitted_form_.fields[kPasswordFieldIndex].value = u"strongpassword";
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               &possible_usernames));
  form_manager_->SaveSuggestedUsernameValueToVotesUploader();

  // Simulate the user modifying the username in the prompt.
  form_manager_->OnUpdateUsernameFromPrompt(kPossibleUsername);

  // Check that uploads for both single username and sign-up form happen.
  testing::InSequence in_sequence;

#if !BUILDFLAG(IS_ANDROID)
  // Expect a positive `IN_FORM_OVERRULE` vote on the username form.
  ExpectSingleUsernameUpload(kSingleUsernameFormSignature,
                             AutofillUploadContents::Field::IN_FORM_OVERRULE,
                             autofill::SINGLE_USERNAME);
#endif

  // Expect upload for the password form. This upload is unrelated to UFF: it
  // is a result of saving a new password on the password form.
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(SignatureIs(CalculateFormSignature(submitted_form_)),
                         _, _, _, _, _, _));

  form_manager_->Save();
}

// Tests scenario when the save/update prompt suggests some value typed in the
// password form and the user saves credentials without modifying them. This
// test checks that Username First Flow votes are not sent, since there is no
// signal that this is a Username First Flow.
TEST_P(PasswordFormManagerTest,
       UsernameFirstFlowDoNotSendVotesOnNotUsernameFirstFlow) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kUsernameFirstFlowFallbackCrowdsourcing,
                            features::kUsernameFirstFlowWithIntermediateValues,
                            features::
                                kUsernameFirstFlowWithIntermediateValuesVoting,
                            features::kUsernameFirstFlowStoreSeveralValues},
      /*disabled_features=*/{});

  CreateFormManager(submitted_form_);
  fetcher_->NotifyFetchCompleted();

  // Simulate user input in a single text field outside of the password form.
  constexpr char16_t kNotUsername[] = u"not_a_username";
  PossibleUsernameData possible_username_data(
      saved_match_.signon_realm, kSingleUsernameFieldRendererId, kNotUsername,
      base::Time::Now(),
      /*driver_id=*/0, /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);
  possible_username_data.form_predictions = MakeSingleUsernamePredictions(
      kSingleUsernameFormSignature, kSingleUsernameFieldRendererId,
      kSingleUsernameFieldSignature, /*has_single_username_prediction=*/false);

  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames = MakePossibleUsernamesCache({possible_username_data});

  // Simulate submitting a password form.
  submitted_form_.fields[kPasswordFieldIndex].value = u"strongpassword";
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               &possible_usernames));
  form_manager_->SaveSuggestedUsernameValueToVotesUploader();

  // Expect no upload on single text field.
  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(SignatureIs(kSingleUsernameFormSignature), _,
                                 _, _, _, _, _))
      .Times(0);

  // Expect upload for the password form. This upload is unrelated to UFF: it
  // is a result of saving a new password on the password form.
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(SignatureIs(CalculateFormSignature(submitted_form_)),
                         _, _, _, _, _, _));

  form_manager_->Save();
}

// Tests that if prefilled username inside the password form has a matching
// value with a field found outside of the password form the vote will be sent
// correctly. In this test case, there is an intermediary field between them.
TEST_P(PasswordFormManagerTest,
       UsernameFirstFlowWithIntermediaryFieldsAndPrefilledUsername) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kUsernameFirstFlowFallbackCrowdsourcing,
                            features::kUsernameFirstFlowWithIntermediateValues,
                            features::kUsernameFirstFlowStoreSeveralValues},
      /*disabled_features=*/{});

  CreateFormManager(submitted_form_);
  fetcher_->NotifyFetchCompleted();

  // Create possible username data.
  PossibleUsernameData possible_username_data(
      saved_match_.signon_realm, kSingleUsernameFieldRendererId,
      submitted_form_.fields[kUsernameFieldIndex].value, base::Time::Now(),
      /*driver_id=*/0,
      /*autocomplete_attribute_has_username=*/false, /*is_likely_otp=*/false);

  // Server has no predictions for the single username form.
  // `possible_username_data.form_predictions` is required to be set correctly
  // with field signature, form signature, and field renderer id to be able to
  // send single username votes.
  auto server_prediction = MakeSingleUsernamePredictions(
      kSingleUsernameFormSignature, kSingleUsernameFieldRendererId,
      kSingleUsernameFieldSignature, /*has_single_username_prediction=*/false);
  possible_username_data.form_predictions = server_prediction;

  // Create more recent data.
  const std::u16string possible_username_otp = u"not_username_value";
  PossibleUsernameData possible_username_data_otp(
      saved_match_.signon_realm, autofill::FieldRendererId(103u),
      possible_username_otp, base::Time::Now(),
      /*driver_id=*/0, /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);

  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames = MakePossibleUsernamesCache(
          {possible_username_data_otp, possible_username_data});

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               &possible_usernames));

  // Check that uploads for both single username and sign-up form happen.
  testing::InSequence in_sequence;

  // Upload username first flow vote on the single username form.
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(SignatureIs(kSingleUsernameFormSignature),
                                 false, ServerFieldTypeSet{SINGLE_USERNAME}, _,
                                 true, nullptr, /*observer=*/IsNull()));
#endif  // !BUILDFLAG(IS_ANDROID)

  // Upload username first flow vote on the sign-up form.
  autofill::AutofillUploadContents::SingleUsernameData
      expected_single_username_data;
  expected_single_username_data.set_username_form_signature(
      kSingleUsernameFormSignature.value());
  expected_single_username_data.set_username_field_signature(
      kSingleUsernameFieldSignature.value());
  expected_single_username_data.set_value_type(
      autofill::AutofillUploadContents::VALUE_WITH_NO_WHITESPACE);
  expected_single_username_data.set_prompt_edit(
      autofill::AutofillUploadContents::NOT_EDITED_POSITIVE);
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(
          AllOf(SignatureIs(CalculateFormSignature(submitted_form_)),
                UploadedSingleUsernameDataIs({expected_single_username_data})),
          _, _, _, _, _, /*observer=*/IsNull()));

  // Simulate showing the prompt and saving the suggested value.
  form_manager_->SaveSuggestedUsernameValueToVotesUploader();

  base::HistogramTester histogram_tester;

  form_manager_->Save();

#if !BUILDFLAG(IS_ANDROID)
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SingleUsername.PasswordFormHadUsernameField", 1, 1);
#else
  histogram_tester.ExpectTotalCount(
      "PasswordManager.SingleUsername.PasswordFormHadUsernameField", 0);
#endif
  Mock::VerifyAndClearExpectations(&mock_autofill_download_manager_);
}

// Tests that when the user edits the username value in the save prompt
// manually, a strong positive vote is sent for the form that user has typed the
// saved value into, and a strong negative vote is sent for the form, into which
// the user has typed a different value.
TEST_P(PasswordFormManagerTest, UsernameFirstFlowSendVotesOnRecentFields) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kUsernameFirstFlowFallbackCrowdsourcing,
                            features::kUsernameFirstFlowWithIntermediateValues,
                            features::
                                kUsernameFirstFlowWithIntermediateValuesVoting,
                            features::kUsernameFirstFlowStoreSeveralValues},
      /*disabled_features=*/{});
  CreateFormManager(observed_form_only_password_fields_);
  fetcher_->NotifyFetchCompleted();

  // Simulate user input in a single text field in a forgot password form.
  constexpr char16_t kPossibleUsername[] = u"possible_username";
  PossibleUsernameData single_username_data(
      saved_match_.signon_realm, kSingleUsernameFieldRendererId,
      kPossibleUsername, base::Time::Now(),
      /*driver_id=*/0, /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);

  // Server has no predictions for the single username form.
  // `possible_username_data.form_predictions` is required to be set correctly
  // with field signature, form signature, and field renderer id to be able to
  // send single username votes.
  single_username_data.form_predictions = MakeSingleUsernamePredictions(
      kSingleUsernameFormSignature, kSingleUsernameFieldRendererId,
      kSingleUsernameFieldSignature, /*has_single_username_prediction=*/false);

  // Simulate user input in another single text field form, unrelated to UFF.
  constexpr FormSignature kOtherFormSignature(2000);
  constexpr FieldRendererId kOtherFieldRendererId(200);
  constexpr FieldSignature kOtherFieldSignature(4000);
  PossibleUsernameData first_name_data(
      saved_match_.signon_realm, kOtherFieldRendererId, u"Firstname",
      base::Time::Now(),
      /*driver_id=*/0, /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);

  // Server has no predictions for the single username form.
  // `possible_username_data.form_predictions` is required to be set correctly
  // with field signature, form signature, and field renderer id to be able to
  // send single username votes.
  first_name_data.form_predictions = MakeSingleUsernamePredictions(
      kOtherFormSignature, kOtherFieldRendererId, kOtherFieldSignature,
      /*has_single_username_prediction=*/false);

  // Put elements in reverse order so that the most recent element is first.
  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames =
          MakePossibleUsernamesCache({first_name_data, single_username_data});

  // Simulate submitting a password form.
  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = u"strongpassword";
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               &possible_usernames));

  form_manager_->SaveSuggestedUsernameValueToVotesUploader();

  // Simulate the user modifying the username in the prompt.
  form_manager_->OnUpdateUsernameFromPrompt(kPossibleUsername);

#if !BUILDFLAG(IS_ANDROID)
  // Expect a strong positive vote on the single username form.
  ExpectSingleUsernameUpload(
      kSingleUsernameFormSignature, AutofillUploadContents::Field::STRONG,
      autofill::SINGLE_USERNAME,
      IsMostRecentSingleUsernameCandidate::kMostRecentCandidate);

  // Expect a strong negative vote on another single text field form.
  ExpectSingleUsernameUpload(
      kOtherFormSignature, autofill::AutofillUploadContents::Field::STRONG,
      autofill::NOT_USERNAME,
      IsMostRecentSingleUsernameCandidate::kHasIntermediateValuesInBetween);
#endif

  // Expect upload for the password form. This upload is unrelated to UFF: it
  // is a result of saving a new password on the password form.
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(SignatureIs(CalculateFormSignature(submitted_form)), _,
                         _, _, _, _, _));

  form_manager_->Save();
}

// Test that the username field in a single username form can be filled.
TEST_P(PasswordFormManagerTest, UsernameFirstFlowFillSingleUsernameForm) {
#if BUILDFLAG(IS_IOS)
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kIOSPasswordSignInUff);
#endif  // BUILDFLAG(IS_IOS)

  // Create the form manager for a form with only a username in it.
  CreateFormManager(non_password_form_);

  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, SetPasswordFillData).WillOnce(SaveArg<0>(&fill_data));

  // Provide server predictions for the single username form, which will trigger
  // FillNow().
  std::map<FormSignature, FormPredictions> predictions =
      CreatePredictions(non_password_form_,
                        {std::make_pair(kUsernameFieldIndex, SINGLE_USERNAME)});
  form_manager_->ProcessServerPredictions(predictions);

  // Verify the fill data used for autofilling the single username form.
  EXPECT_EQ(non_password_form_.fields[kUsernameFieldIndex].unique_renderer_id,
            fill_data.username_element_renderer_id);
  EXPECT_EQ(saved_match_.username_value,
            fill_data.preferred_login.username_value);
  EXPECT_EQ(autofill::FieldRendererId(),
            fill_data.password_element_renderer_id);
  EXPECT_EQ(saved_match_.password_value,
            fill_data.preferred_login.password_value);
}

#if BUILDFLAG(IS_IOS)
// TODO(b/305833151): Remove this test once sign-in uff on iOS is launched.
// Test that the username field in a single username form isn't filled if the
// feature isn't enabled.
TEST_P(PasswordFormManagerTest,
       UsernameFirstFlowFillSingleUsernameForm_SingleUsernameNotEnabledOnIos) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kIOSPasswordSignInUff);

  // Create the form manager for a form with only a username in it.
  CreateFormManager(non_password_form_);

  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, SetPasswordFillData).Times(0);

  // Provide server predictions for the single username form, which will trigger
  // FillNow().
  std::map<FormSignature, FormPredictions> predictions =
      CreatePredictions(non_password_form_,
                        {std::make_pair(kUsernameFieldIndex, SINGLE_USERNAME)});
  form_manager_->ProcessServerPredictions(predictions);
}
#endif

// Tests that a negative vote is sent when a single username candidate is
// populated in a prompt, but then is removed by the user in the prompt.
TEST_P(PasswordFormManagerTest, NegativeUsernameFirstFlowVotes) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kUsernameFirstFlowFallbackCrowdsourcing},
      /*disabled_features=*/{});

  constexpr char16_t kPossibleUsername[] = u"possible_username";

  constexpr autofill::FieldRendererId kUsernameFieldRendererId(100);
  constexpr autofill::FormSignature kUsernameFormSignature(1000);
  constexpr autofill::FieldSignature kUsernameFieldSignature(123);

  CreateFormManager(observed_form_only_password_fields_);
  fetcher_->NotifyFetchCompleted();

  // Create possible username data.
  PossibleUsernameData possible_username_data(
      saved_match_.signon_realm, kUsernameFieldRendererId, kPossibleUsername,
      base::Time::Now(), /*driver_id=*/0,
      /*autocomplete_attribute_has_username=*/false, /*is_likely_otp=*/false);
  FormPredictions predictions;
  predictions.form_signature = kUsernameFormSignature;
  predictions.fields.push_back({
      .renderer_id = kUsernameFieldRendererId,
      .signature = kUsernameFieldSignature,
      .type = SINGLE_USERNAME,
  });
  possible_username_data.form_predictions = std::move(predictions);
  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames = MakePossibleUsernamesCache({possible_username_data});

  // Simulate submission a form without username. Data from
  // |possible_username_data| will be taken for setting username.
  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = u"strongpassword";
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               &possible_usernames));

  // Simulate showing the prompt and saving the suggested value.
  // TODO(crbug/959776) Add a unittest for the case when this method is not
  // called.
  form_manager_->SaveSuggestedUsernameValueToVotesUploader();

  // Simulate the user modifying the username in the prompt.
  form_manager_->OnUpdateUsernameFromPrompt(u"different_username");

  // Check that uploads for both username and password form happen.
  testing::InSequence in_sequence;

  // Upload for the username form. Ensure that we send `NOT_USERNAME` for the
  // username field.
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(SignatureIs(kUsernameFormSignature), false,
                                 ServerFieldTypeSet{NOT_USERNAME}, _, true,
                                 nullptr, /*observer=*/IsNull()));
#else
  EXPECT_CALL(mock_autofill_download_manager_, StartUploadRequest).Times(0);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Upload for the password form.
  autofill::AutofillUploadContents::SingleUsernameData
      expected_single_username_data;
  expected_single_username_data.set_username_form_signature(
      kUsernameFormSignature.value());
  expected_single_username_data.set_username_field_signature(
      kUsernameFieldSignature.value());
  expected_single_username_data.set_value_type(
      autofill::AutofillUploadContents::USERNAME_LIKE);
  expected_single_username_data.set_prompt_edit(
      autofill::AutofillUploadContents::EDITED_NEGATIVE);
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(
          AllOf(SignatureIs(CalculateFormSignature(submitted_form)),
                UploadedSingleUsernameDataIs({expected_single_username_data})),
          _, _, _, _, _, /*observer=*/IsNull()));

  base::HistogramTester histogram_tester;

  form_manager_->Save();

#if !BUILDFLAG(IS_ANDROID)
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SingleUsername.PasswordFormHadUsernameField", 0, 1);
#else
  histogram_tester.ExpectTotalCount(
      "PasswordManager.SingleUsername.PasswordFormHadUsernameField", 0);
#endif
}

// Tests that no votes are sent for an OTP field.
TEST_P(PasswordFormManagerTest, PossibleUsernameLikelyOTP) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kUsernameFirstFlowFallbackCrowdsourcing},
      /*disabled_features=*/{});

  // A single password form is loaded on the page.
  FormData submitted_form = observed_form_only_password_fields_;
  CreateFormManager(submitted_form);
  fetcher_->NotifyFetchCompleted();
  // User types a password value.
  submitted_form.fields[0].value = u"strongpassword";

  // User entered single username data on the previous form.
  const std::u16string single_username_value = u"single_username_value";
  PossibleUsernameData possible_username_data(
      saved_match_.signon_realm, kSingleUsernameFieldRendererId,
      single_username_value, base::Time::Now(),
      /*driver_id=*/0, /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/true);
  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames = MakePossibleUsernamesCache({possible_username_data});

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               &possible_usernames));
  // Check that a username is not chosen from |possible_username_data|.
  EXPECT_TRUE(form_manager_->GetPendingCredentials().username_value.empty());

  // Check vote uploads.
  testing::InSequence in_sequence;
  // No single username upload for the OTP field.
  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(SignatureIs(kSingleUsernameFormSignature), _,
                                 _, _, _, _, /*observer=*/IsNull()))
      .Times(0);

  // Upload single username data for the password form.
  autofill::AutofillUploadContents::SingleUsernameData
      expected_single_username_data;
  expected_single_username_data.set_value_type(
      autofill::AutofillUploadContents::NO_VALUE_TYPE);
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(
          AllOf(SignatureIs(CalculateFormSignature(submitted_form)),
                UploadedSingleUsernameDataIs({expected_single_username_data})),
          _, _, _, _, _, /*observer=*/IsNull()));

  form_manager_->Save();
}

// Tests that server prediction are taken into consideration for offering
// username on username first flow.
TEST_P(PasswordFormManagerTest, PossibleUsernameServerPredictions) {
  const std::u16string possible_username = u"possible_username";
  PossibleUsernameData possible_username_data(
      saved_match_.signon_realm, autofill::FieldRendererId(102u),
      possible_username, base::Time::Now(),
      /*driver_id=*/0, /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = u"strongpassword";

  for (ServerFieldType prediction : {SINGLE_USERNAME, NOT_USERNAME}) {
    SCOPED_TRACE(testing::Message("prediction=") << prediction);

    FormPredictions form_predictions;
    form_predictions.fields.push_back(
        {.renderer_id = possible_username_data.renderer_id,
         .type = prediction});

    possible_username_data.form_predictions = form_predictions;
    base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
        possible_usernames =
            MakePossibleUsernamesCache({possible_username_data});

    CreateFormManager(observed_form_only_password_fields_);
    fetcher_->NotifyFetchCompleted();

    ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                                 &possible_usernames));

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

// Tests that the possible username with server prediction is picked as a
// username in password save prompt, even if there is a field that was more
// recently typed by the user as a possible username in Username First Flow.
TEST_P(PasswordFormManagerTest, PossibleUsernamesPrioritizeServerPrediction) {
  const std::u16string possible_username_with_prediction =
      u"possible_username_with_prediction";
  PossibleUsernameData possible_username_data_with_prediction(
      saved_match_.signon_realm, kSingleUsernameFieldRendererId,
      possible_username_with_prediction, base::Time::Now(),
      /*driver_id=*/0, /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);
  const std::u16string possible_username_without_prediction =
      u"possible_username_without_prediction";
  PossibleUsernameData possible_username_data_without_prediction(
      saved_match_.signon_realm, autofill::FieldRendererId(103u),
      possible_username_without_prediction, base::Time::Now(),
      /*driver_id=*/0, /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = u"strongpassword";

  possible_username_data_with_prediction.form_predictions =
      MakeSingleUsernamePredictions(kSingleUsernameFormSignature,
                                    kSingleUsernameFieldRendererId,
                                    kSingleUsernameFieldSignature);
  // The most recent username doesn't have a prediction, while the least recent
  // has.
  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames =
          MakePossibleUsernamesCache({possible_username_data_without_prediction,
                                      possible_username_data_with_prediction});

  CreateFormManager(observed_form_only_password_fields_);
  fetcher_->NotifyFetchCompleted();

  EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               &possible_usernames));
  EXPECT_EQ(possible_username_with_prediction,
            form_manager_->GetPendingCredentials().username_value);
}

// Tests that boolean representing autocomplete = "username" is taken into
// consideration when offering username in prompt in username first flow.
TEST_P(PasswordFormManagerTest, PossibleUsernameFromAutocomplete) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kUsernameFirstFlowHonorAutocomplete);

  // A single password form is loaded on the page.
  FormData submitted_form = observed_form_only_password_fields_;
  CreateFormManager(submitted_form);
  fetcher_->NotifyFetchCompleted();

  // User types a password value.
  submitted_form.fields[0].value = u"strongpassword";

  // User entered single username data on the previous form.
  const std::u16string single_username_value = u"single_username_value";
  PossibleUsernameData possible_username_data(
      saved_match_.signon_realm, autofill::FieldRendererId(102u),
      single_username_value, base::Time::Now(),
      /*driver_id=*/0, /*autocomplete_attribute_has_username=*/true,
      /*is_likely_otp=*/false);
  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames = MakePossibleUsernamesCache({possible_username_data});

  // Provisionally save the form on user input.
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               &possible_usernames));

  // Check that a username is chosen from |possible_username_data|.
  EXPECT_EQ(single_username_value,
            form_manager_->GetPendingCredentials().username_value);
}

// Tests that a field possible username field type that was parsed as on OTP
// can be overridden by a server prediction and offered as username.
TEST_P(PasswordFormManagerTest, PossibleUsernameLikelyOTPWithServerOverride) {
  // A single password form is loaded on the page.
  FormData submitted_form = observed_form_only_password_fields_;
  CreateFormManager(submitted_form);
  fetcher_->NotifyFetchCompleted();

  // User types a password value.
  submitted_form.fields[0].value = u"strongpassword";

  // User entered single username data on the previous form.
  const std::u16string possible_username = u"test@example.org";
  PossibleUsernameData possible_username_data(
      saved_match_.signon_realm, kSingleUsernameFieldRendererId,
      possible_username, base::Time::Now(),
      /*driver_id=*/0, /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);
  // Create form predictions and set them to |possible_username_data|.
  possible_username_data.form_predictions = MakeSingleUsernamePredictions(
      kSingleUsernameFormSignature, kSingleUsernameFieldRendererId,
      kSingleUsernameFieldSignature);
  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames = MakePossibleUsernamesCache({possible_username_data});

  // Provisionally save the form on user input.
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               &possible_usernames));

  // Check that a username is chosen from |possible_username_data|.
  EXPECT_EQ(possible_username,
            form_manager_->GetPendingCredentials().username_value);
}

// Tests that probable change password submission is detected for a form that
// does not contain a username fields, but contains old and new password fields.
TEST_P(PasswordFormManagerTest, ChangePasswordFormWithoutUsernameSubmitted) {
  // A form with old and new password fields without username.
  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = u"oldpassword";
  submitted_form.fields[1].value = u"newpassword";

  ASSERT_TRUE(
      form_manager_->ProvisionallySave(submitted_form, &driver_, nullptr));
  EXPECT_TRUE(form_manager_->HasLikelyChangeOrResetFormSubmitted());
}

// Tests that probable change password submission is detected properly for forms
// containing username fields.
TEST_P(PasswordFormManagerTest, ChangePasswordFormWithUsernameSubmitted) {
  // A form with username, current and new password fields.
  FormData submitted_form = observed_form_only_password_fields_;
  FormFieldData username_field;
  username_field.name = u"username";
  username_field.form_control_type = autofill::FormControlType::kInputText;
  username_field.value = u"oldusername";
  username_field.unique_renderer_id = autofill::FieldRendererId(2);
  submitted_form.fields.insert(std::begin(submitted_form.fields),
                               username_field);

  submitted_form.fields[1].value = u"oldpassword";
  submitted_form.fields[2].value = u"newpassword";

  ASSERT_TRUE(
      form_manager_->ProvisionallySave(submitted_form, &driver_, nullptr));
  EXPECT_TRUE(form_manager_->HasLikelyChangeOrResetFormSubmitted());

  // A form with username and new password fields (most likely sign-up).
  submitted_form.fields[1].value = u"newpassword";

  ASSERT_TRUE(
      form_manager_->ProvisionallySave(submitted_form, &driver_, nullptr));
  EXPECT_FALSE(form_manager_->HasLikelyChangeOrResetFormSubmitted());
}

// Tests that probable reset password submission is detected for a form that
// does not contain a username and current password fields.
TEST_P(PasswordFormManagerTest, ResetPasswordFormSubmitted) {
  // A form with new and confirmation password fields without username.
  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = u"newpassword";
  submitted_form.fields[1].value = u"newpassword";

  ASSERT_TRUE(
      form_manager_->ProvisionallySave(submitted_form, &driver_, nullptr));
  EXPECT_TRUE(form_manager_->HasLikelyChangeOrResetFormSubmitted());
}

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
  identity_test_env_.SetPrimaryAccount(kEmail, signin::ConsentLevel::kSync);
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
  identity_test_env_.SetPrimaryAccount("another-user@gmail.com",
                                       signin::ConsentLevel::kSync);
  ON_CALL(client_, GetIdentityManager())
      .WillByDefault(Return(identity_test_env_.identity_manager()));
  EXPECT_TRUE(form_manager_->IsMovableToAccountStore());
}

TEST_P(PasswordFormManagerTest, ReportSubmittedFormFrameMainFrame) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(driver_, IsInPrimaryMainFrame).WillRepeatedly(Return(true));

  form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr);

  // Check metrics recorded on the form manager destruction.
  form_manager_.reset();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SubmittedFormFrame2",
      metrics_util::SubmittedFormFrame::MAIN_FRAME, 1);
}

TEST_P(PasswordFormManagerTest, ReportSubmittedFormFrameSameOriginIframe) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(driver_, IsInPrimaryMainFrame).WillRepeatedly(Return(false));

  EXPECT_CALL(client_, GetLastCommittedURL)
      .WillOnce(ReturnRef(submitted_form_.url));
  form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr);

  // Check metrics recorded on the form manager destruction.
  form_manager_.reset();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SubmittedFormFrame2",
      metrics_util::SubmittedFormFrame::IFRAME_WITH_SAME_URL_AS_MAIN_FRAME, 1);
}

TEST_P(PasswordFormManagerTest, ReportSubmittedFormFrameSameSignOnRealmIframe) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(driver_, IsInPrimaryMainFrame).WillRepeatedly(Return(false));

  GURL main_frame_url = GURL(GetSignonRealm(submitted_form_.url));
  ASSERT_NE(submitted_form_.url, main_frame_url);
  EXPECT_CALL(client_, GetLastCommittedURL)
      .WillRepeatedly(ReturnRef(main_frame_url));
  form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr);

  // Check metrics recorded on the form manager destruction.
  form_manager_.reset();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SubmittedFormFrame2",
      metrics_util::SubmittedFormFrame::
          IFRAME_WITH_DIFFERENT_URL_SAME_SIGNON_REALM_AS_MAIN_FRAME,
      1);
}

TEST_P(PasswordFormManagerTest, ReportSubmittedFormFramePSLMatchedIframe) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(driver_, IsInPrimaryMainFrame).WillRepeatedly(Return(false));

  submitted_form_.url = GURL("http://facebook.com");
  GURL main_frame_url = GURL("http://m.facebook.com");
  ASSERT_TRUE(IsPublicSuffixDomainMatch(submitted_form_.url.spec(),
                                        main_frame_url.spec()));
  EXPECT_CALL(client_, GetLastCommittedURL)
      .WillRepeatedly(ReturnRef(main_frame_url));
  form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr);

  // Check metrics recorded on the form manager destruction.
  form_manager_.reset();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SubmittedFormFrame2",
      metrics_util::SubmittedFormFrame::IFRAME_WITH_PSL_MATCHED_SIGNON_REALM,
      1);
}

TEST_P(PasswordFormManagerTest, ReportSubmittedFormFrameCrossOriginIframe) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(driver_, IsInPrimaryMainFrame).WillRepeatedly(Return(false));

  GURL main_frame_url = GURL("http://www.crossorigin.com/login");
  ASSERT_NE(GetSignonRealm(submitted_form_.url),
            GetSignonRealm(main_frame_url));
  ASSERT_FALSE(IsPublicSuffixDomainMatch(submitted_form_.url.spec(),
                                         main_frame_url.spec()));
  EXPECT_CALL(client_, GetLastCommittedURL)
      .WillRepeatedly(ReturnRef(main_frame_url));
  form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr);

  // Check metrics recorded on the form manager destruction.
  form_manager_.reset();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SubmittedFormFrame2",
      metrics_util::SubmittedFormFrame::
          IFRAME_WITH_DIFFERENT_AND_NOT_PSL_MATCHED_SIGNON_REALM,
      1);
}

// Tests that when the user edits the username value in the save prompt
// manually, a strong positive vote is sent for the form that user has typed the
// saved value into, and a strong negative vote is sent for the form, into which
// the user has typed a different value.
TEST_P(PasswordFormManagerTest, StrongForgotPasswordFormVotes) {
  CreateFormManager(observed_form_only_password_fields_);
  fetcher_->NotifyFetchCompleted();

  // Simulate user input in a single text field in a forgot password form.
  constexpr char16_t kPossibleUsername[] = u"possible_username";
  AddFieldInfo(/*driver_id=*/0, kSingleUsernameFieldRendererId,
               observed_form_only_password_fields_.url, kPossibleUsername,
               kSingleUsernameFormSignature, kSingleUsernameFieldSignature,
               /*is_likely_otp=*/false, UNKNOWN_TYPE);

  // Simulate user input in another single text field form, unrelated to FPF.
  constexpr FormSignature kOtherFormSignature(2000);
  constexpr FieldRendererId kOtherFieldRendererId(200);
  constexpr FieldSignature kOtherFieldSignature(4000);
  AddFieldInfo(/*driver_id=*/0, kOtherFieldRendererId,
               observed_form_only_password_fields_.url, u"OTP",
               kOtherFormSignature, kOtherFieldSignature,
               /*is_likely_otp=*/false, UNKNOWN_TYPE);

  // Simulate submitting a password form.
  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = u"strongpassword";
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               /*possible_username=*/nullptr));
  form_manager_->SaveSuggestedUsernameValueToVotesUploader();

  // Simulate the user modifying the username in the prompt.
  form_manager_->OnUpdateUsernameFromPrompt(kPossibleUsername);

  // Expect a strong positive vote on the username form.
  ExpectSingleUsernameUpload(
      kSingleUsernameFormSignature,
      AutofillUploadContents::Field::STRONG_FORGOT_PASSWORD,
      autofill::SINGLE_USERNAME_FORGOT_PASSWORD);

  // Expect a strong negative vote on another single text field form, unrelated
  // to FPF.
  ExpectSingleUsernameUpload(
      kOtherFormSignature,
      autofill::AutofillUploadContents::Field::STRONG_FORGOT_PASSWORD,
      autofill::NOT_USERNAME);

  // Expect upload for the password form. This upload is unrelated to FPF: it
  // is a result of saving a new password on the password form.
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(SignatureIs(CalculateFormSignature(submitted_form)), _,
                         _, _, _, _, _));

  form_manager_->Save();
}

// Tests that when the user accepts the username value in the save prompt
// without editing, a weak positive vote is sent for the form that user has
// typed the saved value into, and a weak negative vote is sent for the form,
// into which the user has typed a different value.
TEST_P(PasswordFormManagerTest, WeakForgotPasswordFormVotes) {
  CreateFormManager(observed_form_only_password_fields_);
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  // Simulate user input in a single text field in a forgot password form.
  AddFieldInfo(/*driver_id=*/0, kSingleUsernameFieldRendererId,
               observed_form_only_password_fields_.url,
               saved_match_.username_value, kSingleUsernameFormSignature,
               kSingleUsernameFieldSignature,
               /*is_likely_otp=*/false, UNKNOWN_TYPE);

  // Simulate user input in another single text field form, unrelated to FPF.
  constexpr FormSignature kOtherFormSignature(2000);
  constexpr FieldRendererId kOtherFieldRendererId(200);
  constexpr FieldSignature kOtherFieldSignature(4000);
  AddFieldInfo(/*driver_id=*/0, kOtherFieldRendererId,
               observed_form_only_password_fields_.url, u"OTP",
               kOtherFormSignature, kOtherFieldSignature,
               /*is_likely_otp=*/false, UNKNOWN_TYPE);

  // Simulate submitting a password form. A previously saved username value
  // (`saved_match_.username_value`) is offered as username in the prompt
  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = u"strongpassword";
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               /*possible_username=*/nullptr));
  form_manager_->SaveSuggestedUsernameValueToVotesUploader();

  // Expect a weak positive vote on the username form.
  ExpectSingleUsernameUpload(
      kSingleUsernameFormSignature,
      AutofillUploadContents::Field::WEAK_FORGOT_PASSWORD,
      autofill::SINGLE_USERNAME_FORGOT_PASSWORD);

  // Expect a weak negative vote on another single text field form, unrelated to
  // FPF.
  ExpectSingleUsernameUpload(
      kOtherFormSignature,
      autofill::AutofillUploadContents::Field::WEAK_FORGOT_PASSWORD,
      autofill::NOT_USERNAME);

  // Expect upload for the password form. This upload is unrelated to FPF: it
  // is a result of saving a new password on the password form.
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(SignatureIs(CalculateFormSignature(submitted_form)), _,
                         _, _, _, _, _));

  form_manager_->Save();
}

// Tests a forgot password flow in which the password form has a non-empty
// username value. A vote will only be sent for a text field, that matches the
// username in the password form.
TEST_P(PasswordFormManagerTest,
       ForgotPasswordFormVotesPasswordFormHasUsername) {
  CreateFormManager(observed_form_);
  fetcher_->NotifyFetchCompleted();

  // Simulate user input in a single text field in a forgot password form.
  constexpr char16_t kPossibleUsername[] = u"possible_username";
  AddFieldInfo(/*driver_id=*/0, kSingleUsernameFieldRendererId,
               observed_form_only_password_fields_.url, kPossibleUsername,
               kSingleUsernameFormSignature, kSingleUsernameFieldSignature,
               /*is_likely_otp=*/false, UNKNOWN_TYPE);

  // Simulate user input in another single text field form, unrelated to FPF.
  constexpr FormSignature kOtherFormSignature(2000);
  constexpr FieldRendererId kOtherFieldRendererId(200);
  constexpr FieldSignature kOtherFieldSignature(4000);
  AddFieldInfo(/*driver_id=*/0, kOtherFieldRendererId,
               observed_form_only_password_fields_.url, u"OTP",
               kOtherFormSignature, kOtherFieldSignature,
               /*is_likely_otp=*/false, UNKNOWN_TYPE);

  // Simulate submitting a password form.
  FormData submitted_form = observed_form_;
  submitted_form.fields[1].value = kPossibleUsername;
  submitted_form.fields[2].value = u"strongpassword";
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               /*possible_username=*/nullptr));
  form_manager_->SaveSuggestedUsernameValueToVotesUploader();

  // Expect a weak positive vote on the username form.
  ExpectSingleUsernameUpload(
      kSingleUsernameFormSignature,
      AutofillUploadContents::Field::WEAK_FORGOT_PASSWORD,
      autofill::SINGLE_USERNAME_FORGOT_PASSWORD);

  // Expect no vote on another single text field form, unrelated to FPF.
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(SignatureIs(kOtherFormSignature), _, _, _, _, _, _))
      .Times(0);

  // Expect upload for the password form. This upload is unrelated to FPF: it
  // is a result of saving a new password on the password form.
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(SignatureIs(CalculateFormSignature(submitted_form)), _,
                         _, _, _, _, _));

  form_manager_->Save();
}

// Tests that no vote is sent for the OTP field, unless there is a server
// prediction confirming it's a single username field.
TEST_P(PasswordFormManagerTest, ForgotPasswordFormVotesOnLiklelyOTPField) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kForgotPasswordFormSupport},
      /*disabled_features=*/{});

  CreateFormManager(observed_form_only_password_fields_);
  fetcher_->NotifyFetchCompleted();

  // Simulate user input in a single text field in a forgot password form,
  // for which PasswordManager's heuristics say it's an OTP field, but server
  // predictions contradict that.
  constexpr char16_t kPossibleUsername[] = u"possible_username";
  AddFieldInfo(/*driver_id=*/0, kSingleUsernameFieldRendererId,
               observed_form_only_password_fields_.url, kPossibleUsername,
               kSingleUsernameFormSignature, kSingleUsernameFieldSignature,
               /*is_likely_otp=*/true, SINGLE_USERNAME_FORGOT_PASSWORD);

  // Simulate user input in another single text field form, unrelated to FPF,
  // for which PasswordManager's heuristics say it's an OTP field, and server
  // does not contradict that.
  constexpr FormSignature kOtherFormSignature(2000);
  constexpr FieldRendererId kOtherFieldRendererId(200);
  constexpr FieldSignature kOtherFieldSignature(4000);
  AddFieldInfo(/*driver_id=*/0, kOtherFieldRendererId,
               observed_form_only_password_fields_.url, u"OTP",
               kOtherFormSignature, kOtherFieldSignature,
               /*is_likely_otp=*/true, NOT_USERNAME);

  // Simulate submitting a password form.
  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = u"strongpassword";
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               /*possible_username=*/nullptr));
  form_manager_->SaveSuggestedUsernameValueToVotesUploader();

  // Simulate the user modifying the username in the prompt.
  form_manager_->OnUpdateUsernameFromPrompt(kPossibleUsername);

  // Expect a strong positive vote on the single text field in a forgot password
  // form.
  ExpectSingleUsernameUpload(
      kSingleUsernameFormSignature,
      AutofillUploadContents::Field::STRONG_FORGOT_PASSWORD,
      autofill::SINGLE_USERNAME_FORGOT_PASSWORD);

  // Expect no vote on another single text field form, unrelated to FPF.
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(SignatureIs(kOtherFormSignature), _, _, _, _, _, _))
      .Times(0);

  // Expect upload for the password form. This upload is unrelated to FPF: it
  // is a result of saving a new password on the password form.
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(SignatureIs(CalculateFormSignature(submitted_form)), _,
                         _, _, _, _, _));

  form_manager_->Save();
}

// Tests that single username info stored in FieldInfoManager is used to
// build pending credentials if it is predicted to be a single username field
// by the server.
TEST_P(PasswordFormManagerTest, ForgotPasswordFormUsernamePopulatedInPrompt) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kForgotPasswordFormSupport},
      /*disabled_features=*/{});

  CreateFormManager(observed_form_only_password_fields_);
  fetcher_->NotifyFetchCompleted();

  // Simulate user input in a single text field in a forgot password form.
  constexpr char16_t kPossibleUsername[] = u"possible_username";
  AddFieldInfo(/*driver_id=*/0, kSingleUsernameFieldRendererId,
               observed_form_only_password_fields_.url, kPossibleUsername,
               kSingleUsernameFormSignature, kSingleUsernameFieldSignature,
               /*is_likely_otp=*/false, SINGLE_USERNAME_FORGOT_PASSWORD);

  // Provisionally save the form on password input.
  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = u"strong_password";
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               /*possible_usernames=*/nullptr));

  // Check that single username is used to build pending credentials.
  EXPECT_EQ(kPossibleUsername,
            form_manager_->GetPendingCredentials().username_value);
}

// Tests that single username info stored in FieldInfoManager is not used to
// build pending credentials if it is not predicted to be a single username
// field by the server.
TEST_P(PasswordFormManagerTest,
       ForgotPasswordFormUsernameNotPopulatedInPrompt) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kForgotPasswordFormSupport},
      /*disabled_features=*/{});

  CreateFormManager(observed_form_only_password_fields_);
  fetcher_->NotifyFetchCompleted();

  // Simulate user input in a single text field in a forgot password form.
  constexpr char16_t kPossibleUsername[] = u"possible_username";
  AddFieldInfo(/*driver_id=*/0, kSingleUsernameFieldRendererId,
               observed_form_only_password_fields_.url, kPossibleUsername,
               kSingleUsernameFormSignature, kSingleUsernameFieldSignature,
               /*is_likely_otp=*/false, UNKNOWN_TYPE);

  // Provisionally save the form on password input.
  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = u"strong_password";
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               /*possible_usernames=*/nullptr));

  // Check that single username is used to build pending credentials.
  EXPECT_EQ(u"", form_manager_->GetPendingCredentials().username_value);
}

#if BUILDFLAG(IS_ANDROID)
TEST_P(PasswordFormManagerTest,
       ClientShouldShowErrorMessageForAuthErrorResolvable) {
  base::test::ScopedFeatureList feature_list;
  fetcher_->SetProfileStoreBackendError(PasswordStoreBackendError(
      PasswordStoreBackendErrorType::kAuthErrorResolvable,
      PasswordStoreBackendErrorRecoveryType::kRecoverable));

  EXPECT_CALL(client_,
              ShowPasswordManagerErrorMessage(
                  password_manager::ErrorMessageFlowType::kFillFlow,
                  PasswordStoreBackendErrorType::kAuthErrorResolvable));
  fetcher_->NotifyFetchCompleted();
}

TEST_P(PasswordFormManagerTest,
       ClientShouldShowErrorMessageForAuthErrorUnresolvable) {
  fetcher_->SetProfileStoreBackendError(PasswordStoreBackendError(
      PasswordStoreBackendErrorType::kAuthErrorUnresolvable,
      PasswordStoreBackendErrorRecoveryType::kRecoverable));

  EXPECT_CALL(client_,
              ShowPasswordManagerErrorMessage(
                  password_manager::ErrorMessageFlowType::kFillFlow,
                  PasswordStoreBackendErrorType::kAuthErrorUnresolvable));
  fetcher_->NotifyFetchCompleted();
}

TEST_P(PasswordFormManagerTest,
       ClientShouldNotShowErrorMessageWhenThereIsNoError) {
  fetcher_->SetProfileStoreBackendError(absl::nullopt);

  EXPECT_CALL(client_, ShowPasswordManagerErrorMessage).Times(0);
  fetcher_->NotifyFetchCompleted();
}

TEST_P(PasswordFormManagerTest,
       ClientShouldNotShowErrorMessageWhenErrorIsNotAuthError) {
  fetcher_->SetProfileStoreBackendError(PasswordStoreBackendError(
      PasswordStoreBackendErrorType::kUncategorized,
      PasswordStoreBackendErrorRecoveryType::kUnspecified));

  EXPECT_CALL(client_, ShowPasswordManagerErrorMessage).Times(0);
  fetcher_->NotifyFetchCompleted();
}

TEST_P(PasswordFormManagerTest, ClientShouldNotShowErrorMessageWhenUnenrolled) {
  client_.GetPrefs()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);
  fetcher_->SetProfileStoreBackendError(PasswordStoreBackendError(
      PasswordStoreBackendErrorType::kAuthErrorResolvable,
      PasswordStoreBackendErrorRecoveryType::kRecoverable));

  EXPECT_CALL(client_, ShowPasswordManagerErrorMessage).Times(0);
  fetcher_->NotifyFetchCompleted();
}
#endif

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
  MOCK_CONST_METHOD0(GetGeneratedPassword, const std::u16string&());
  MOCK_CONST_METHOD0(GetProfileStoreFormSaverForTesting, FormSaver*());
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
  MOCK_METHOD1(Blocklist, void(const PasswordFormDigest&));
  MOCK_METHOD1(Unblocklist, void(const PasswordFormDigest&));
  MOCK_METHOD1(PresaveGeneratedPassword, void(PasswordForm));
  MOCK_METHOD2(GeneratedPasswordAccepted,
               void(PasswordForm, base::WeakPtr<PasswordManagerDriver>));
  MOCK_METHOD0(PasswordNoLongerGenerated, void());
  MOCK_METHOD1(UpdateSubmissionIndicatorEvent,
               void(autofill::mojom::SubmissionIndicatorEvent));
  MOCK_CONST_METHOD0(IsNewLogin, bool());
  MOCK_CONST_METHOD0(IsPasswordUpdate, bool());
  MOCK_CONST_METHOD0(HasGeneratedPassword, bool());
  MOCK_METHOD0(UsernameUpdatedInBubble, void());
  std::unique_ptr<PasswordSaveManager> Clone() override {
    return std::make_unique<MockPasswordSaveManager>();
  }
  MOCK_METHOD1(MoveCredentialsToAccountStore,
               void(metrics_util::MoveToAccountStoreTrigger));
  MOCK_METHOD1(BlockMovingToAccountStoreFor, void(const signin::GaiaIdHash&));
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
    // The PasswordFormManagerTestWithMockedSaver tests don't rely on the
    // return value of GetPendingCredentials. So, it is fine to return an
    // arbitrary form.
    ON_CALL(*mock_password_save_manager_, GetPendingCredentials)
        .WillByDefault(ReturnRef(saved_match_));
    form_manager_ = std::make_unique<PasswordFormManager>(
        &client_, driver_.AsWeakPtr(), observed_form, fetcher_.get(),
        std::move(mock_password_save_manager), nullptr);
  }

  // Creates PasswordFormManager and sets it to |form_manager_| for
  // |base_auth_observed_form|. Along the way a new |fetcher_| is created.
  void CreateFormManagerForNonWebForm(
      const PasswordForm& base_auth_observed_form) override {
    fetcher_->set_scheme(PasswordFormDigest(base_auth_observed_form).scheme);
    auto mock_password_save_manager =
        std::make_unique<NiceMock<MockPasswordSaveManager>>();
    mock_password_save_manager_ = mock_password_save_manager.get();
    EXPECT_CALL(*mock_password_save_manager_, Init(_, _, _, _));
    // The PasswordFormManagerTestWithMockedSaver tests don't rely on the
    // return value of GetPendingCredentials. So, it is fine to return an
    // arbitrary form.
    ON_CALL(*mock_password_save_manager_, GetPendingCredentials)
        .WillByDefault(ReturnRef(saved_match_));
    form_manager_ = std::make_unique<PasswordFormManager>(
        &client_, PasswordFormDigest(base_auth_observed_form), fetcher_.get(),
        std::move(mock_password_save_manager));
  }

  void ResetFormManager() {
    mock_password_save_manager_ = nullptr;
    form_manager_.reset();
  }

 private:
  raw_ptr<NiceMock<MockPasswordSaveManager>> mock_password_save_manager_ =
      nullptr;
};

TEST_F(
    PasswordFormManagerTestWithMockedSaver,
    ProvisionallySaveShouldCreatePendingPasswordFormManagerTestWithMockedSaverCredentials) {
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
  std::u16string new_username = saved_match_.username_value + u"1";
  std::u16string new_password = saved_match_.password_value + u"1";
  submitted_form.fields[kUsernameFieldIndex].value = new_username;
  submitted_form.fields[kPasswordFieldIndex].value = new_password;
  EXPECT_TRUE(
      form_manager_->ProvisionallySave(submitted_form, &driver_, nullptr));
  PasswordForm updated_form;
  EXPECT_CALL(*mock_password_save_manager(),
              Save(FormDataPointeeEqualTo(observed_form_), _))
      .WillOnce(SaveArg<1>(&updated_form));
  EXPECT_CALL(client_, UpdateFormManagers());
  EXPECT_CALL(*mock_password_save_manager(), HasGeneratedPassword)
      .WillOnce(Return(false));
  EXPECT_CALL(
      *client_.GetPasswordChangeSuccessTracker(),
      OnChangePasswordFlowCompleted(
          submitted_form.url, base::UTF16ToUTF8(saved_match_.username_value),
          PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen,
          /* phished= */ false));
  form_manager_->Save();
  std::string expected_signon_realm =
      submitted_form.url.DeprecatedGetOriginAsURL().spec();
  EXPECT_EQ(submitted_form.url, updated_form.url);
  EXPECT_EQ(expected_signon_realm, updated_form.signon_realm);
  EXPECT_EQ(new_username, updated_form.username_value);
  EXPECT_EQ(new_password, updated_form.password_value);
  EXPECT_EQ(submitted_form.fields[kUsernameFieldIndex].name,
            updated_form.username_element);
  EXPECT_EQ(submitted_form.fields[kPasswordFieldIndex].name,
            updated_form.password_element);
  // Check UKM metrics.
  ResetFormManager();
  ExpectedGenerationUKM expected_metrics = {
      {} /* shown manually */,
      0 /* password generated */,
      {} /* generated password is not modified */};
  CheckPasswordGenerationUKM(test_ukm_recorder, expected_metrics);
}

TEST_F(PasswordFormManagerTestWithMockedSaver, UpdateUsernameEmptyStore) {
  fetcher_->NotifyFetchCompleted();
  form_manager_->ProvisionallySave(submitted_form_, &driver_, nullptr);
  std::u16string new_username = parsed_submitted_form_.username_value + u"1";
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
  std::u16string user_chosen_username = u"user_chosen_username";
  std::u16string automatically_chosen_username =
      u"automatically_chosen_username";
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
  std::u16string new_username = saved_match_.username_value;
  std::u16string expected_password = parsed_submitted_form_.password_value;
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
  std::u16string new_password = u"new_password";
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
  std::u16string password = u"password1";
  std::u16string pin = u"pin";
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

TEST_F(PasswordFormManagerTestWithMockedSaver, Blocklist) {
  fetcher_->NotifyFetchCompleted();
  PasswordForm actual_blocklisted_form =
      password_manager_util::MakeNormalizedBlocklistedForm(
          PasswordFormDigest(observed_form_));
  EXPECT_CALL(*mock_password_save_manager(),
              Blocklist(PasswordFormDigest(observed_form_)));
  form_manager_->Blocklist();
  EXPECT_TRUE(form_manager_->IsBlocklisted());
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

  identity_test_env_.SetPrimaryAccount(kEmail, signin::ConsentLevel::kSync);

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
  form_manager_->SetGenerationPopupWasShown(PasswordGenerationType::kAutomatic);
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
  const std::u16string generated_password = u"gen_pw";
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
  EXPECT_CALL(
      *client_.GetPasswordChangeSuccessTracker(),
      OnChangePasswordFlowCompleted(
          submitted_form_.url, base::UTF16ToUTF8(saved_match_.username_value),
          PasswordChangeSuccessTracker::EndEvent::
              kManualFlowGeneratedPasswordChosen,
          /* phished= */ false));
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
  // Create a password form with url. Other members will be empty. On
  // this form the form parser should fail. The url is required for LogFormData
  // to work.
  PasswordForm form;
  const std::u16string generated_password = u"gen_pw";
  form.password_value = generated_password;
  form.form_data.url = GURL("https://accounts.google.com/a/ServiceLoginAuth");

  // Check that nevertheless the generated password is forwarded to the
  // PasswordSaveManager.
  EXPECT_CALL(*mock_password_save_manager(),
              PresaveGeneratedPassword(FormHasPassword(generated_password)));
  form_manager_->PresaveGeneratedPassword(form.form_data, form.password_value);
}

TEST_F(PasswordFormManagerTestWithMockedSaver, PasswordNoLongerGenerated) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  fetcher_->NotifyFetchCompleted();
  form_manager_->SetGenerationPopupWasShown(PasswordGenerationType::kManual);
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
  ResetFormManager();
  ExpectedGenerationUKM expected_metrics = {
      absl::make_optional(2u) /* shown manually */,
      0 /* password generated */,
      {} /* generated password is not modified */};
  CheckPasswordGenerationUKM(test_ukm_recorder, expected_metrics);
}

TEST_F(PasswordFormManagerTestWithMockedSaver,
       OnGeneratedPasswordAccepted_InvalidField) {
  FormData form_data = observed_form_only_password_fields_;
  // A call with a non-existent password field should not crash.
  form_manager_->OnGeneratedPasswordAccepted(
      std::move(form_data), autofill::FieldRendererId(12345), u"new_password");
}

TEST_F(PasswordFormManagerTestWithMockedSaver, SaveHttpAuthNoHttpAuthStored) {
  for (bool html_credentials_saved : {false, true}) {
    SCOPED_TRACE(testing::Message("html_credentials_saved=")
                 << html_credentials_saved);
    PasswordForm http_auth_form = parsed_observed_form_;
    http_auth_form.scheme = PasswordForm::Scheme::kBasic;
    // Check that no filling because no http auth credentials are stored.
    EXPECT_CALL(driver_, SetPasswordFillData).Times(0);
    EXPECT_CALL(client_, AutofillHttpAuth(_, _)).Times(0);
    CreateFormManagerForNonWebForm(http_auth_form);
    std::vector<const PasswordForm*> saved_matches;
    if (html_credentials_saved)
      saved_matches.push_back(&saved_match_);
    SetNonFederatedAndNotifyFetchCompleted(saved_matches);
    std::u16string username = u"user1";
    std::u16string password = u"pass1";
    http_auth_form.username_value = username;
    http_auth_form.password_value = password;
    // Check that submitted credentials are forwarded to the password save
    // manager.
    EXPECT_CALL(*mock_password_save_manager(),
                CreatePendingCredentials(http_auth_form, _, _, true, _));
    ASSERT_TRUE(form_manager_->ProvisionallySaveHttpAuthForm(http_auth_form));
    EXPECT_CALL(*mock_password_save_manager(), HasGeneratedPassword)
        .WillOnce(Return(false));
    EXPECT_CALL(
        *client_.GetPasswordChangeSuccessTracker(),
        OnChangePasswordFlowCompleted(
            http_auth_form.url, base::UTF16ToUTF8(saved_match_.username_value),
            PasswordChangeSuccessTracker::EndEvent::
                kManualFlowOwnPasswordChosen,
            /* phished= */ false));
    // Check that the password save manager is invoked.
    EXPECT_CALL(*mock_password_save_manager(), Save(_, http_auth_form));
    form_manager_->Save();
  }
}

TEST_F(PasswordFormManagerTestWithMockedSaver, HTTPAuthAlreadySaved) {
  PasswordForm http_auth_form = parsed_observed_form_;
  http_auth_form.scheme = PasswordForm::Scheme::kBasic;
  CreateFormManagerForNonWebForm(http_auth_form);
  const std::u16string username = u"user1";
  const std::u16string password = u"pass1";
  http_auth_form.username_value = username;
  http_auth_form.password_value = password;
  EXPECT_CALL(client_, AutofillHttpAuth(http_auth_form, _)).Times(1);
  SetNonFederatedAndNotifyFetchCompleted({&http_auth_form});
  EXPECT_CALL(*mock_password_save_manager(),
              CreatePendingCredentials(http_auth_form, _, _, true, _));
  ASSERT_TRUE(form_manager_->ProvisionallySaveHttpAuthForm(http_auth_form));
  EXPECT_CALL(*mock_password_save_manager(), HasGeneratedPassword)
      .WillOnce(Return(false));
  EXPECT_CALL(
      *client_.GetPasswordChangeSuccessTracker(),
      OnChangePasswordFlowCompleted(
          http_auth_form.url, base::UTF16ToUTF8(saved_match_.username_value),
          PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen,
          /* phished= */ false));
  // Check that the password save manager is invoked.
  EXPECT_CALL(*mock_password_save_manager(), Save(_, http_auth_form));
  form_manager_->Save();
}

// Tests that username is taken during username first flow.
TEST_F(PasswordFormManagerTestWithMockedSaver, UsernameFirstFlow) {
  CreateFormManager(observed_form_only_password_fields_);
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  // Create possible username data.
  const std::u16string possible_username = u"test@example.org";
  PossibleUsernameData possible_username_data(
      saved_match_.signon_realm, kSingleUsernameFieldRendererId,
      possible_username, base::Time::Now(),
      /*driver_id=*/0, /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);
  possible_username_data.form_predictions = MakeSingleUsernamePredictions(
      kSingleUsernameFormSignature, kSingleUsernameFieldRendererId,
      kSingleUsernameFieldSignature);
  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames = MakePossibleUsernamesCache({possible_username_data});

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = u"strongpassword";
  // Check that a username is chosen from |possible_username_data|.
  EXPECT_CALL(*mock_password_save_manager(),
              CreatePendingCredentials(FormHasUsernameValue(possible_username),
                                       _, _, _, _));
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               &possible_usernames));
}

// Tests that username is not taken when a possible username is not valid.
TEST_F(PasswordFormManagerTestWithMockedSaver,
       UsernameFirstFlowDifferentDomains) {
  CreateFormManager(observed_form_only_password_fields_);
  fetcher_->NotifyFetchCompleted();

  // Create possible username data.
  std::u16string possible_username = u"possible_username";
  PossibleUsernameData possible_username_data(
      "https://another.domain.com", kSingleUsernameFieldRendererId,
      possible_username, base::Time::Now(),
      /*driver_id=*/0, /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);
  possible_username_data.form_predictions = MakeSingleUsernamePredictions(
      kSingleUsernameFormSignature, kSingleUsernameFieldRendererId,
      kSingleUsernameFieldSignature);
  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames = MakePossibleUsernamesCache({possible_username_data});

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = u"strongpassword";
  PasswordForm parsed_submitted_form;
  EXPECT_CALL(*mock_password_save_manager(), CreatePendingCredentials)
      .WillOnce(SaveArg<0>(&parsed_submitted_form));
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               &possible_usernames));
  EXPECT_TRUE(parsed_submitted_form.username_value.empty());
}

}  // namespace

}  // namespace password_manager
