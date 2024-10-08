// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
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
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/crowdsourcing/mock_autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/browser/test_utils/vote_uploads_test_matchers.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
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
#include "components/password_manager/core/browser/password_store/psl_matching_helper.h"
#include "components/password_manager/core/browser/possible_username_data.h"
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

#if BUILDFLAG(IS_ANDROID)
#include "components/webauthn/android/cred_man_support.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "components/os_crypt/sync/os_crypt_mocker.h"
#endif

namespace password_manager {

namespace {

using ::autofill::AutofillUploadContents;
using ::autofill::CalculateFieldSignatureForField;
using ::autofill::FieldPropertiesFlags;
using ::autofill::FieldRendererId;
using ::autofill::FieldSignature;
using ::autofill::FieldType;
using ::autofill::FieldTypeSet;
using ::autofill::FormData;
using ::autofill::FormFieldData;
using ::autofill::FormRendererId;
using ::autofill::FormSignature;
using ::autofill::FormStructure;
using ::autofill::IsMostRecentSingleUsernameCandidate;
using ::autofill::NOT_USERNAME;
using ::autofill::PasswordFormFillData;
using ::autofill::PasswordFormGenerationData;
using ::autofill::SINGLE_USERNAME;
using ::autofill::SINGLE_USERNAME_FORGOT_PASSWORD;
using ::autofill::UNKNOWN_TYPE;
using ::autofill::password_generation::PasswordGenerationType;
using ::autofill::upload_contents_matchers::FieldAutofillTypeIs;
using ::autofill::upload_contents_matchers::FieldsContain;
using ::autofill::upload_contents_matchers::FieldSignatureIs;
using ::autofill::upload_contents_matchers::FormSignatureIs;
using ::base::TestMockTimeTaskRunner;
using ::signin::GaiaIdHash;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::SaveArgPointee;
using ::testing::UnorderedElementsAre;
using upload_contents_matchers::FieldGenerationTypeIs;
using upload_contents_matchers::FieldIsMostRecentSingleUsernameCandidateIs;
using upload_contents_matchers::FieldSingleUsernameVoteTypeIs;
using upload_contents_matchers::FieldVoteTypeIs;
using upload_contents_matchers::IsPasswordUpload;
using upload_contents_matchers::PasswordsRevealedIs;
using upload_contents_matchers::SingleUsernameDataIs;
using Field = ::autofill::AutofillUploadContents::Field;

// Indices of username and password fields in the observed form.
constexpr int kUsernameFieldIndex = 1;
constexpr int kPasswordFieldIndex = 2;

// Signatures of the single username form and field.
constexpr autofill::FormSignature kSingleUsernameFormSignature(1000);
constexpr autofill::FieldSignature kSingleUsernameFieldSignature(123);

// Unique renderer id of the single username field.
constexpr autofill::FieldRendererId kSingleUsernameFieldRendererId(101);

const std::optional<std::vector<PasskeyCredential>> kNullopt = std::nullopt;
const std::optional<std::vector<PasskeyCredential>> kNoPasskeys =
    std::vector<PasskeyCredential>();
const PasskeyCredential kPasskey(
    PasskeyCredential::Source::kGooglePasswordManager,
    PasskeyCredential::RpId("bestRpInTheWorld.com"),
    PasskeyCredential::CredentialId({1, 2, 3, 4}),
    PasskeyCredential::UserId(),
    PasskeyCredential::Username(""));

// Creates a matcher for an `autofill::AutofillUploadContents::Field` that
// checks that the field's signature matches that of `field` and its predicted
// type is `type`.
auto UploadFieldIs(const FormFieldData& field,
                   FieldType type,
                   auto... matchers) {
  return AllOf(FieldSignatureIs(CalculateFieldSignatureForField(field)),
               FieldAutofillTypeIs({type}), matchers...);
}

// Returns a matcher that checks that a vote upload happened for a form with
// `kSingleUsernameFormSignature` and a field with
// `kSingleUsernameFieldSignature` with type `SINGLE_USERNAME`.
auto IsSingleUsernameUpload() {
  return IsPasswordUpload(
      FormSignatureIs(kSingleUsernameFormSignature),
      FieldsContain(AllOf(FieldSignatureIs(kSingleUsernameFieldSignature),
                          FieldAutofillTypeIs({FieldType::SINGLE_USERNAME}))));
}

MATCHER_P(FormHasUniqueKey, key, "") {
  return ArePasswordFormUniqueKeysEqual(arg, key);
}

MATCHER_P(FormHasUsernameValue, username_value, "") {
  return arg.username_value == username_value;
}

MATCHER_P(FormHasPassword, password_value, "") {
  if (arg.new_password_value.empty()) {
    return arg.password_value == password_value;
  }
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

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_METHOD(bool,
              IsSavingAndFillingEnabled,
              (const GURL&),
              (const, override));
  MOCK_METHOD(bool, IsOffTheRecord, (), (const, override));
  MOCK_METHOD(autofill::AutofillCrowdsourcingManager*,
              GetAutofillCrowdsourcingManager,
              (),
              (override));
  MOCK_METHOD(void, UpdateFormManagers, (), (override));
  MOCK_METHOD(void,
              AutofillHttpAuth,
              (const PasswordForm&, const PasswordFormManagerForUI*),
              (override));
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
  MOCK_METHOD(void, NotifyKeychainError, (), (override));
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
  std::optional<int64_t> generation_popup_shown;
  int64_t has_generated_password;
  std::optional<int64_t> generated_password_modified;
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
  if (expected) {
    EXPECT_EQ(*expected, *actual);
  }
}

// Check that |recorder| records metrics |expected_metrics|.
void CheckPasswordGenerationUKM(const ukm::TestAutoSetUkmRecorder& recorder,
                                const ExpectedGenerationUKM& expected_metrics) {
  auto entries =
      recorder.GetEntriesByName(ukm::builders::PasswordForm::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const int64_t* expected_popup_shown = nullptr;
  if (expected_metrics.generation_popup_shown) {
    expected_popup_shown = &expected_metrics.generation_popup_shown.value();
  }
  CheckMetric(expected_popup_shown, entries[0],
              ukm::builders::PasswordForm::kGeneration_PopupShownName);

  CheckMetric(&expected_metrics.has_generated_password, entries[0],
              ukm::builders::PasswordForm::kGeneration_GeneratedPasswordName);

  const int64_t* expected_password_modified = nullptr;
  if (expected_metrics.generated_password_modified) {
    expected_password_modified =
        &expected_metrics.generated_password_modified.value();
  }
  CheckMetric(
      expected_password_modified, entries[0],
      ukm::builders::PasswordForm::kGeneration_GeneratedPasswordModifiedName);
}

// Create predictions for |form| using field predictions |field_predictions|.
std::map<FormSignature, FormPredictions> CreatePredictions(
    const FormData& form,
    std::vector<std::pair<int, FieldType>> field_predictions,
    bool is_override = false) {
  FormPredictions predictions;
  for (const auto& index_prediction : field_predictions) {
    autofill::FieldRendererId renderer_id =
        form.fields()[index_prediction.first].renderer_id();
    autofill::FieldSignature field_signature =
        CalculateFieldSignatureForField(form.fields()[index_prediction.first]);
    FieldType server_type = index_prediction.second;
    predictions.fields.emplace_back(renderer_id, field_signature, server_type,
                                    /*may_use_prefilled_placeholder=*/false,
                                    is_override);
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

  autofill::FieldType type = has_single_username_prediction
                                 ? autofill::SINGLE_USERNAME
                                 : autofill::NO_SERVER_DATA;
  predictions.fields.emplace_back(renderer_id, field_signature, type,
                                  /*may_use_prefilled_placeholder=*/false,
                                  /*is_override=*/false);

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
  MOCK_METHOD3(
      Save,
      void(PasswordForm pending,
           const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>&
               matches,
           const std::u16string& old_password));
  MOCK_METHOD3(
      Update,
      void(PasswordForm pending,
           const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>&
               matches,
           const std::u16string& old_password));
  MOCK_METHOD4(
      UpdateReplace,
      void(PasswordForm pending,
           const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>&
               matches,
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
  PasswordFormManagerTest()
      : task_runner_(new TestMockTimeTaskRunner),
        possible_usernames_(/*max_size=*/2) {
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
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kBiometricAuthenticationBeforeFilling, true);
#endif
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    OSCryptMocker::SetUp();
    pref_service_.registry()->RegisterIntegerPref(
        password_manager::prefs::kRelaunchChromeBubbleDismissedCounter, 0);
#endif
    PasswordFormManager::set_wait_for_server_predictions_for_filling(true);

    GURL origin = GURL("https://accounts.google.com/a/ServiceLoginAuth");
    GURL action = GURL("https://accounts.google.com/a/ServiceLogin");
    GURL psl_origin = GURL("https://myaccounts.google.com/a/ServiceLoginAuth");
    GURL psl_action = GURL("https://myaccounts.google.com/a/ServiceLogin");

    observed_form_.set_url(origin);
    observed_form_.set_action(action);
    observed_form_.set_name(u"sign-in");
    observed_form_.set_renderer_id(FormRendererId(1));

    observed_form_only_password_fields_ = observed_form_;

    FormFieldData field;
    field.set_name(u"firstname");
    field.set_id_attribute(field.name());
    field.set_name_attribute(field.name());
    field.set_form_control_type(autofill::FormControlType::kInputText);
    field.set_renderer_id(autofill::FieldRendererId(2));
    test_api(observed_form_).Append(field);

    field.set_name(u"username");
    field.set_id_attribute(field.name());
    field.set_name_attribute(field.name());
    field.set_form_control_type(autofill::FormControlType::kInputText);
    field.set_renderer_id(autofill::FieldRendererId(3));
    test_api(observed_form_).Append(field);

    non_password_form_ = observed_form_;

    field.set_name(u"password");
    field.set_id_attribute(field.name());
    field.set_name_attribute(field.name());
    field.set_form_control_type(autofill::FormControlType::kInputPassword);
    field.set_renderer_id(autofill::FieldRendererId(4));
    test_api(observed_form_).Append(field);
    test_api(observed_form_only_password_fields_).Append(field);

    field.set_name(u"password2");
    field.set_id_attribute(field.name());
    field.set_name_attribute(field.name());
    field.set_form_control_type(autofill::FormControlType::kInputPassword);
    field.set_renderer_id(autofill::FieldRendererId(5));
    test_api(observed_form_only_password_fields_).Append(field);

    submitted_form_ = observed_form_;
    test_api(submitted_form_).field(kUsernameFieldIndex).set_value(u"user1");
    test_api(submitted_form_).field(kPasswordFieldIndex).set_value(u"secret1");

    submitted_non_password_form_ = non_password_form_;
    test_api(submitted_non_password_form_)
        .field(kUsernameFieldIndex)
        .set_value(u"user1");

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
        observed_form_.fields()[kUsernameFieldIndex].name();
    parsed_observed_form_.password_element =
        observed_form_.fields()[kPasswordFieldIndex].name();

    parsed_submitted_form_ = parsed_observed_form_;
    parsed_submitted_form_.form_data = submitted_form_;
    parsed_submitted_form_.username_value =
        submitted_form_.fields()[kUsernameFieldIndex].value();
    parsed_submitted_form_.password_value =
        submitted_form_.fields()[kPasswordFieldIndex].value();

    EXPECT_CALL(client_, GetAutofillCrowdsourcingManager())
        .WillRepeatedly(Return(&crowdsourcing_manager()));
    ON_CALL(client_, GetPrefs()).WillByDefault(Return(&pref_service_));
    ON_CALL(client_, IsCommittedMainFrameSecure()).WillByDefault(Return(true));
    ON_CALL(*client_.GetPasswordFeatureManager(),
            ShouldShowAccountStorageBubbleUi)
        .WillByDefault(Return(true));
    ON_CALL(crowdsourcing_manager(), StartUploadRequest)
        .WillByDefault(Return(true));
    ON_CALL(*client_.GetPasswordFeatureManager(), GetDefaultPasswordStore)
        .WillByDefault(Return(PasswordForm::Store::kProfileStore));

    ON_CALL(client_, GetLastCommittedURL())
        .WillByDefault(ReturnRef(observed_form_.url()));
    ON_CALL(client_, GetLastCommittedOrigin)
        .WillByDefault(Return(url::Origin::Create(observed_form_.url())));
    ON_CALL(client_, GetWebAuthnCredentialsDelegateForDriver)
        .WillByDefault(Return(&webauthn_credentials_delegate_));
    ON_CALL(webauthn_credentials_delegate_, GetPasskeys)
        .WillByDefault(ReturnRef(passkeys_));
    ON_CALL(webauthn_credentials_delegate_, IsSecurityKeyOrHybridFlowAvailable)
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
      const std::vector<PasswordForm>& non_federated) {
    fetcher_->SetNonFederated(non_federated);
    fetcher_->SetBestMatches(non_federated);
    fetcher_->NotifyFetchCompleted();
  }

  void AddFieldInfo(int driver_id,
                    FieldRendererId field_id,
                    const GURL& url,
                    const std::u16string field_value,
                    FormSignature form_signature,
                    FieldSignature field_signature,
                    bool is_likely_otp,
                    FieldType predicted_type) {
    FieldInfo info(driver_id, field_id, GetSignonRealm(url), field_value,
                   is_likely_otp);
    FormPredictions predictions;
    predictions.form_signature = form_signature;
    predictions.fields.emplace_back(field_id, field_signature, predicted_type,
                                    /*may_use_prefilled_placeholder=*/false,
                                    /*is_override=*/false);
    field_info_manager_->AddFieldInfo(info, predictions);
  }

  void ExpectIsSingleUsernameUpload(
      FormSignature form_signature,
      Field::SingleUsernameVoteType vote_type,
      FieldType field_type,
      IsMostRecentSingleUsernameCandidate
          is_most_recent_single_username_candidate =
              IsMostRecentSingleUsernameCandidate::
                  kNotPartOfUsernameFirstFlow) {
    std::optional<bool> most_recent;
    if (is_most_recent_single_username_candidate !=
        IsMostRecentSingleUsernameCandidate::kNotPartOfUsernameFirstFlow) {
      most_recent = is_most_recent_single_username_candidate ==
                    IsMostRecentSingleUsernameCandidate::kMostRecentCandidate;
    }
    auto upload_contents_matcher = IsPasswordUpload(
        FormSignatureIs(form_signature),
        FieldsContain(
            AllOf(FieldAutofillTypeIs({field_type}),
                  FieldSingleUsernameVoteTypeIs(vote_type),
                  FieldIsMostRecentSingleUsernameCandidateIs(most_recent))));
    EXPECT_CALL(crowdsourcing_manager(),
                StartUploadRequest(upload_contents_matcher, _, _));
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

  autofill::MockAutofillCrowdsourcingManager& crowdsourcing_manager() {
    return mock_autofill_crowdsourcing_manager_;
  }

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
  std::optional<std::vector<PasskeyCredential>> passkeys_;
  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames_;

  // Define |fetcher_| before |form_manager_|, because the former needs to
  // outlive the latter.
  std::unique_ptr<FakeFormFetcher> fetcher_;
  std::unique_ptr<PasswordFormManager> form_manager_;

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  NiceMock<autofill::MockAutofillCrowdsourcingManager>
      mock_autofill_crowdsourcing_manager_{/*client=*/nullptr};
};

TEST_P(PasswordFormManagerTest, DoesManage) {
  EXPECT_TRUE(
      form_manager_->DoesManage(observed_form_.renderer_id(), &driver_));
  // Forms on other drivers are not considered managed.
  MockPasswordManagerDriver another_driver;
  EXPECT_FALSE(
      form_manager_->DoesManage(observed_form_.renderer_id(), &another_driver));
  FormData another_form = observed_form_;
  another_form.set_renderer_id(FormRendererId());
  EXPECT_FALSE(form_manager_->DoesManage(another_form.renderer_id(), &driver_));

  // Unique_renderer_id is the form identifier.
  another_form = observed_form_;
  another_form.set_renderer_id(
      FormRendererId(another_form.renderer_id().value() + 1));
  EXPECT_FALSE(form_manager_->DoesManage(another_form.renderer_id(), &driver_));
}

TEST_P(PasswordFormManagerTest, DoesManageNoFormTag) {
  observed_form_.set_renderer_id(FormRendererId());
  CreateFormManager(observed_form_);

  FormData another_form = observed_form_;
  // Simulate that new input was added by JavaScript.
  test_api(another_form).fields().emplace_back();
  EXPECT_TRUE(form_manager_->DoesManage(another_form.renderer_id(), &driver_));
  // Forms on other drivers are not considered managed.
  EXPECT_FALSE(form_manager_->DoesManage(another_form.renderer_id(), nullptr));
}

TEST_P(PasswordFormManagerTest, Autofill) {
  CreateFormManager(observed_form_);
  EXPECT_CALL(driver_, FormEligibleForGenerationFound(_)).Times(0);
  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, SetPasswordFillData).WillOnce(SaveArg<0>(&fill_data));
  CreateFormManager(observed_form_);
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_EQ(observed_form_.url(), fill_data.url);

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
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

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
  test_api(observed_form_).field(-1).set_autocomplete_attribute("new-password");

  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, SetPasswordFillData).WillOnce(SaveArg<0>(&fill_data));

  PasswordFormGenerationData generation_data;
  EXPECT_CALL(driver_, FormEligibleForGenerationFound(_))
      .WillOnce(SaveArg<0>(&generation_data));

  CreateFormManager(observed_form_);
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(fill_data.password_element_renderer_id.is_null());
  EXPECT_EQ(saved_match_.password_value,
            fill_data.preferred_login.password_value);
#if BUILDFLAG(IS_IOS)
  EXPECT_EQ(observed_form_.renderer_id(), generation_data.form_renderer_id);
#else
  EXPECT_EQ(observed_form_.fields().back().renderer_id(),
            generation_data.new_password_renderer_id);
  EXPECT_TRUE(generation_data.confirmation_password_renderer_id.is_null());
#endif
}

// Checks that generation signal is sent to the renderer when new password
// fields are marked with autocomplete attribute.
TEST_P(PasswordFormManagerTest, GenerationOnNewAndConfirmPasswordFields) {
  // Make |observed_form_| to be sign-up form.
  test_api(observed_form_).field(-1).set_autocomplete_attribute("new-password");
  const autofill::FieldRendererId new_password_renderer_id =
      observed_form_.fields().back().renderer_id();
  // Add a confirmation field.
  FormFieldData field;
  const autofill::FieldRendererId confirm_password_renderer_id(
      new_password_renderer_id.value() + 1);
  field.set_renderer_id(confirm_password_renderer_id);
  field.set_form_control_type(autofill::FormControlType::kInputPassword);
  field.set_autocomplete_attribute("new-password");
  test_api(observed_form_).Append(field);

  PasswordFormGenerationData generation_data;
  EXPECT_CALL(driver_, FormEligibleForGenerationFound(_))
      .WillOnce(SaveArg<0>(&generation_data));

  CreateFormManager(observed_form_);
  fetcher_->NotifyFetchCompleted();

  task_environment_.FastForwardUntilNoTasksRemain();
#if BUILDFLAG(IS_IOS)
  EXPECT_EQ(observed_form_.renderer_id(), generation_data.form_renderer_id);
#else
  EXPECT_EQ(new_password_renderer_id, generation_data.new_password_renderer_id);
  EXPECT_EQ(confirm_password_renderer_id,
            generation_data.confirmation_password_renderer_id);
#endif
}

// Checks that `FormDataParser` classifies text field with `NEW_PASSWORD`
// override as a new password field.
TEST_P(PasswordFormManagerTest, GenerationOnTextFieldsDueToOverride) {
  test_api(observed_form_)
      .field(kPasswordFieldIndex)
      .set_form_control_type(autofill::FormControlType::kInputText);
  auto predictions = CreatePredictions(
      observed_form_, {{kPasswordFieldIndex, FieldType::NEW_PASSWORD}},
      /*is_override=*/true);

  PasswordFormGenerationData generation_data;
  EXPECT_CALL(driver_, FormEligibleForGenerationFound(_))
      .WillOnce(SaveArg<0>(&generation_data));

  CreateFormManager(observed_form_);
  form_manager_->ProcessServerPredictions(predictions);
  fetcher_->NotifyFetchCompleted();

  task_environment_.FastForwardUntilNoTasksRemain();
#if BUILDFLAG(IS_IOS)
  EXPECT_EQ(observed_form_.renderer_id(), generation_data.form_renderer_id);
#else
  EXPECT_EQ(generation_data.new_password_renderer_id,
            observed_form_.fields()[kPasswordFieldIndex].renderer_id());
#endif
}

TEST_P(PasswordFormManagerTest, AutofillWithBlocklistedMatch) {
  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, SetPasswordFillData).WillOnce(SaveArg<0>(&fill_data));
  fetcher_->SetNonFederated({saved_match_});
  fetcher_->SetBestMatches({saved_match_});
  fetcher_->SetBlocklisted(true);
  fetcher_->NotifyFetchCompleted();

  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_EQ(observed_form_.url(), fill_data.url);
  EXPECT_EQ(saved_match_.username_value,
            fill_data.preferred_login.username_value);
  EXPECT_EQ(saved_match_.password_value,
            fill_data.preferred_login.password_value);
}

TEST_P(PasswordFormManagerTest, SetSubmitted) {
  EXPECT_FALSE(form_manager_->is_submitted());
  EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               possible_usernames_));
  EXPECT_TRUE(form_manager_->is_submitted());

  FormData another_form = submitted_form_;
  another_form.set_name(another_form.name() + u"1");
#if !BUILDFLAG(IS_IOS)
  // |another_form| is managed because the same |renderer_id| as
  // |observed_form_|.
  EXPECT_TRUE(form_manager_->ProvisionallySave(another_form, &driver_,
                                               possible_usernames_));
  EXPECT_TRUE(form_manager_->is_submitted());
#endif
}

TEST_P(PasswordFormManagerTest, TestSaveFormAllowedNegative) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(submitted_form_.url()))
      .WillRepeatedly(Return(false));
  form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                   possible_usernames_);
  EXPECT_FALSE(form_manager_->IsSavingAllowed());
}

TEST_P(PasswordFormManagerTest, TestSaveFormAllowed) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(submitted_form_.url()))
      .WillRepeatedly(Return(true));
  form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                   possible_usernames_);
  EXPECT_TRUE(form_manager_->IsSavingAllowed());
}

TEST_P(PasswordFormManagerTest, SetSubmittedMultipleTimes) {
  EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               possible_usernames_));
  EXPECT_TRUE(form_manager_->is_submitted());

  // Make the submitted form to be invalid password form.
  submitted_form_.set_fields({});

  EXPECT_FALSE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                                possible_usernames_));
  EXPECT_FALSE(form_manager_->is_submitted());
  EXPECT_FALSE(form_manager_->GetSubmittedForm());
  EXPECT_EQ(PasswordForm(), form_manager_->GetPendingCredentials());
}

// Tests that when PasswordFormManager receives saved matches it waits for
// server predictions and fills on receiving them.
TEST_P(PasswordFormManagerTest, ServerPredictionsWithinDelay) {
  // Expects no filling on save matches receiving.
  EXPECT_CALL(driver_, SetPasswordFillData).Times(0);
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});
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
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});
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
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});
}

// Tests creating pending credentials when the password store is empty.
TEST_P(PasswordFormManagerTest, CreatePendingCredentialsEmptyStore) {
  fetcher_->NotifyFetchCompleted();

  const base::Time kNow = base::Time::Now();

  EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               possible_usernames_));

  const PasswordForm& pending_credentials =
      form_manager_->GetPendingCredentials();
  CheckPendingCredentials(parsed_submitted_form_, pending_credentials);
  EXPECT_GE(pending_credentials.date_last_used, kNow);
}

// Tests creating pending credentials when fetch completed
TEST_P(PasswordFormManagerTest, CreatePendingCredentialsWhenFetchCompleted) {
  PasswordFormManager::set_wait_for_server_predictions_for_filling(false);
  form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                   possible_usernames_);
  SetNonFederatedAndNotifyFetchCompleted({parsed_submitted_form_});
  EXPECT_FALSE(form_manager_->IsNewLogin());
}

// Tests creating pending credentials when new credentials are submitted and the
// store has another credentials saved.
TEST_P(PasswordFormManagerTest, CreatePendingCredentialsNewCredentials) {
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               possible_usernames_));
  CheckPendingCredentials(parsed_submitted_form_,
                          form_manager_->GetPendingCredentials());
}

// Tests that when submitted credentials are equal to already saved one then
// pending credentials equal to saved match.
TEST_P(PasswordFormManagerTest, CreatePendingCredentialsAlreadySaved) {
  PasswordFormManager::set_wait_for_server_predictions_for_filling(false);
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  test_api(submitted_form_)
      .field(kUsernameFieldIndex)
      .set_value(saved_match_.username_value);
  test_api(submitted_form_)
      .field(kPasswordFieldIndex)
      .set_value(saved_match_.password_value);

  // Tests that depending on whether we fill on page load or account select that
  // correct user action is recorded. Fill on account select is simulated by
  // pretending we are in incognito mode.
#if !BUILDFLAG(IS_IOS) && !defined(ANDROID)
  for (bool is_incognito : {false, true}) {
    EXPECT_CALL(client_, IsOffTheRecord).WillOnce(Return(is_incognito));
#endif
    form_manager_->Fill();
    EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                                 possible_usernames_));
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

  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  test_api(submitted_form_)
      .field(kUsernameFieldIndex)
      .set_value(saved_match_.username_value);
  test_api(submitted_form_)
      .field(kPasswordFieldIndex)
      .set_value(saved_match_.password_value);

  EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               possible_usernames_));
  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
}

// Tests creating pending credentials when new credentials are different only in
// password with already saved one.
TEST_P(PasswordFormManagerTest, CreatePendingCredentialsPasswordOverridden) {
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  PasswordForm expected = saved_match_;
  expected.password_value += u"1";

  test_api(submitted_form_)
      .field(kUsernameFieldIndex)
      .set_value(saved_match_.username_value);
  test_api(submitted_form_)
      .field(kPasswordFieldIndex)
      .set_value(expected.password_value);
  EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               possible_usernames_));
  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
}

// Tests that when submitted credentials are equal to already saved one then
// pending credentials equal to saved match.
TEST_P(PasswordFormManagerTest, CreatePendingCredentialsUpdate) {
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  FormData submitted_form = observed_form_only_password_fields_;
  test_api(submitted_form).field(0).set_value(u"strongpassword");
  test_api(submitted_form).field(1).set_value(u"verystrongpassword");

  PasswordForm expected = saved_match_;
  expected.password_value = u"verystrongpassword";

  EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames_));
  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
}

// Tests creating pending credentials when a change password form is submitted
// and there are multiple saved forms.
TEST_P(PasswordFormManagerTest, CreatePendingCredentialsUpdateMultipleSaved) {
  PasswordForm another_saved_match = saved_match_;
  another_saved_match.username_value += u"1";
  SetNonFederatedAndNotifyFetchCompleted({saved_match_, another_saved_match});

  FormData submitted_form = observed_form_only_password_fields_;
  test_api(submitted_form).field(0).set_value(u"strongpassword");
  test_api(submitted_form).field(1).set_value(u"verystrongpassword");

  PasswordForm expected = saved_match_;
  expected.password_value = u"verystrongpassword";

  EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames_));
  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
}

// Tests creating pending credentials when the password field has an empty name.
TEST_P(PasswordFormManagerTest, CreatePendingCredentialsEmptyName) {
  fetcher_->NotifyFetchCompleted();

  FormData anonymous_signup = observed_form_;
  // There is an anonymous password field.
  test_api(anonymous_signup).field(2).set_name({});
  test_api(anonymous_signup).field(2).set_value(u"a password");
  // Mark the password field as new-password.
  std::map<FormSignature, FormPredictions> predictions = CreatePredictions(
      observed_form_, {std::make_pair(2, autofill::ACCOUNT_CREATION_PASSWORD)});

  form_manager_->ProcessServerPredictions(predictions);

  EXPECT_TRUE(form_manager_->ProvisionallySave(anonymous_signup, &driver_,
                                               possible_usernames_));
  EXPECT_EQ(u"a password",
            form_manager_->GetPendingCredentials().password_value);
}

// Tests that there is no crash even when the observed form is a not password
// form and the submitted form is password form.
TEST_P(PasswordFormManagerTest, NoCrashOnNonPasswordForm) {
  FormData form_without_password_fields = observed_form_;
  // Remove the password field.
  test_api(form_without_password_fields).Resize(kPasswordFieldIndex);
  CreateFormManager(form_without_password_fields);
  fetcher_->NotifyFetchCompleted();

  FormData submitted_form = observed_form_;
  test_api(submitted_form).field(kUsernameFieldIndex).set_value(u"username");
  test_api(submitted_form).field(kPasswordFieldIndex).set_value(u"password");

  // Expect no crash.
  form_manager_->ProvisionallySave(submitted_form, &driver_,
                                   possible_usernames_);
}

TEST_P(PasswordFormManagerTest, IsEqualToSubmittedForm) {
  fetcher_->NotifyFetchCompleted();

  FormData submitted_form = observed_form_;
  test_api(submitted_form)
      .field(kUsernameFieldIndex)
      .set_value(saved_match_.username_value);
  test_api(submitted_form)
      .field(kPasswordFieldIndex)
      .set_value(saved_match_.password_value);

  // No submitted form yet.
  EXPECT_FALSE(form_manager_->IsEqualToSubmittedForm(submitted_form));

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames_));

  observed_form_.set_renderer_id(
      FormRendererId(observed_form_.renderer_id().value() + 10));
  observed_form_.set_fields({});

  EXPECT_TRUE(form_manager_->IsEqualToSubmittedForm(observed_form_));

  observed_form_.set_action(GURL("https://example.com"));
  EXPECT_FALSE(form_manager_->IsEqualToSubmittedForm(observed_form_));
}

// Tests that when credentials with a new username (i.e. not saved yet) is
// successfully submitted, then they are saved correctly.
TEST_P(PasswordFormManagerTest, SaveNewCredentials) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  FormData submitted_form = observed_form_;
  std::u16string new_username = saved_match_.username_value + u"1";
  std::u16string new_password = saved_match_.password_value + u"1";
  test_api(submitted_form).field(kUsernameFieldIndex).set_value(new_username);
  test_api(submitted_form).field(kPasswordFieldIndex).set_value(new_password);

  EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames_));
  EXPECT_TRUE(form_manager_->IsNewLogin());

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());
  PasswordForm saved_form;
  std::vector<raw_ptr<const PasswordForm, VectorExperimental>> best_matches;
  EXPECT_CALL(form_saver, Save(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&saved_form), SaveArg<1>(&best_matches)));
  EXPECT_CALL(client_, UpdateFormManagers());

  form_manager_->Save();

  std::string expected_signon_realm =
      submitted_form.url().DeprecatedGetOriginAsURL().spec();
  EXPECT_EQ(submitted_form.url(), saved_form.url);
  EXPECT_EQ(expected_signon_realm, saved_form.signon_realm);
  EXPECT_EQ(new_username, saved_form.username_value);
  EXPECT_EQ(new_password, saved_form.password_value);

  EXPECT_EQ(submitted_form.fields()[kUsernameFieldIndex].name(),
            saved_form.username_element);
  EXPECT_EQ(submitted_form.fields()[kPasswordFieldIndex].name(),
            saved_form.password_element);
  ASSERT_EQ(best_matches.size(), 1u);
  EXPECT_EQ(*best_matches[0], saved_match_);

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
  SetNonFederatedAndNotifyFetchCompleted({psl_saved_match_});

  FormData submitted_form = observed_form_;
  // Change
  test_api(submitted_form)
      .field(kUsernameFieldIndex)
      .set_value(psl_saved_match_.username_value);
  test_api(submitted_form)
      .field(kPasswordFieldIndex)
      .set_value(psl_saved_match_.password_value);

  EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames_));
  EXPECT_TRUE(form_manager_->IsNewLogin());

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());
  PasswordForm saved_form;
  std::vector<raw_ptr<const PasswordForm, VectorExperimental>> best_matches;
  EXPECT_CALL(form_saver, Save(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&saved_form), SaveArg<1>(&best_matches)));

  form_manager_->Save();

  EXPECT_EQ(submitted_form.url(), saved_form.url);
  EXPECT_EQ(GetSignonRealm(submitted_form.url()), saved_form.signon_realm);
  EXPECT_EQ(saved_form.username_value, psl_saved_match_.username_value);
  EXPECT_EQ(saved_form.password_value, psl_saved_match_.password_value);
  EXPECT_EQ(saved_form.username_element, psl_saved_match_.username_element);
  EXPECT_EQ(saved_form.password_element, psl_saved_match_.password_element);

  ASSERT_EQ(best_matches.size(), 1u);
  EXPECT_EQ(*best_matches[0], psl_saved_match_);
}

// Tests that when credentials with already saved username but with a new
// password are submitted, then the saved password is updated.
TEST_P(PasswordFormManagerTest, OverridePassword) {
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  FormData submitted_form = observed_form_;
  std::u16string username = saved_match_.username_value;
  std::u16string new_password = saved_match_.password_value + u"1";
  test_api(submitted_form).field(kUsernameFieldIndex).set_value(username);
  test_api(submitted_form).field(kPasswordFieldIndex).set_value(new_password);

  EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames_));
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
      {saved_match_, not_best_saved_match, saved_match_another_username});

  FormData submitted_form = observed_form_only_password_fields_;
  test_api(submitted_form).field(0).set_value(saved_match_.password_value);
  std::u16string new_password = saved_match_.password_value + u"1";
  test_api(submitted_form).field(1).set_value(new_password);

  EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames_));
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
    SetNonFederatedAndNotifyFetchCompleted({saved_match_});

    FormData submitted_form = observed_form_only_password_fields_;
    test_api(submitted_form).field(0).set_value(saved_match_.password_value);
    auto new_password = saved_match_.password_value + u"1";
    test_api(submitted_form).field(1).set_value(new_password);

    EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                                 possible_usernames_));

    testing::InSequence in_sequence;
    auto upload_contents_matcher = IsPasswordUpload(FieldsContain(
        UploadFieldIs(submitted_form.fields()[0], FieldType::PASSWORD),
        UploadFieldIs(submitted_form.fields()[1], expected_vote)));
    EXPECT_CALL(crowdsourcing_manager(),
                StartUploadRequest(upload_contents_matcher, _, _));
    if (expected_vote == autofill::NEW_PASSWORD) {
      // An unrelated |FIRST_USE| vote.
      EXPECT_CALL(crowdsourcing_manager(), StartUploadRequest);
    }

    if (expected_vote == autofill::NEW_PASSWORD) {
      form_manager_->Save();
    } else if (expected_vote == autofill::PROBABLY_NEW_PASSWORD) {
      form_manager_->OnNoInteraction(true /* is_update */);
    } else {
      form_manager_->OnNopeUpdateClicked();
    }
    Mock::VerifyAndClearExpectations(&crowdsourcing_manager());
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
  size_t alternative_username_field_index =
      saved_match_.form_data.fields().size();
  for (const AlternativeElement& alternative :
       saved_match_.all_alternative_usernames) {
    FormFieldData text_field;
    text_field.set_name(alternative.name);
    text_field.set_form_control_type(autofill::FormControlType::kInputText);
    test_api(saved_match_.form_data).Append(text_field);
  }
  FormFieldData password_field;
  password_field.set_name(saved_match_.password_element);
  password_field.set_form_control_type(
      autofill::FormControlType::kInputPassword);
  test_api(saved_match_.form_data).Append(password_field);
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  // On a login form, the user uses the password value.
  test_api(submitted_form_)
      .field(kPasswordFieldIndex)
      .set_value(saved_match_.password_value);
  // The username should be corrected. To intensify testing, simulate that the
  // user changes the username value many times.
  for (const std::u16string& new_username_value :
       {saved_match_.all_alternative_usernames[2].value,
        saved_match_.all_alternative_usernames[1].value,
        saved_match_.all_alternative_usernames[0].value,
        std::u16string(u"random"), std::u16string(),
        saved_match_.all_alternative_usernames[0].value}) {
    test_api(submitted_form_)
        .field(kUsernameFieldIndex)
        .set_value(new_username_value);
    EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                                 possible_usernames_));
  }

  testing::InSequence in_sequence;
  // Unrelated regular PASSWORD vote.
  EXPECT_CALL(crowdsourcing_manager(), StartUploadRequest);

  // The first key in the map should be
  // `saved_match_.form_data.fields()[0].renderer_id`, but the new
  // `renderer_id` of value 1 should be assigned in
  // `GenerateSyntheticRenderIdsAndAssignThem()` in the flow
  // of `Save()`.
  auto upload_contents_matcher = IsPasswordUpload(
      FormSignatureIs(CalculateFormSignature(saved_match_.form_data)),
      FieldsContain(
          UploadFieldIs(
              saved_match_.form_data.fields()[alternative_username_field_index],
              FieldType::USERNAME,
              FieldVoteTypeIs(Field::USERNAME_OVERWRITTEN)),
          UploadFieldIs(password_field, FieldType::ACCOUNT_CREATION_PASSWORD)));
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(upload_contents_matcher, _, _));

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
    field.set_name(alternative.name);
    field.set_form_control_type(autofill::FormControlType::kInputText);
    test_api(saved_match_.form_data).Append(field);
  }
  FormFieldData password_field;
  password_field.set_name(saved_match_.password_element);
  password_field.set_form_control_type(
      autofill::FormControlType::kInputPassword);
  test_api(saved_match_.form_data).Append(password_field);

  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  // On a login form, the user reuses the username value and password value.
  test_api(submitted_form_)
      .field(kUsernameFieldIndex)
      .set_value(saved_match_.username_value);
  test_api(submitted_form_)
      .field(kPasswordFieldIndex)
      .set_value(saved_match_.password_value);

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               possible_usernames_));

  // Credentials saved on the signup form were reused on a login form. The vote
  // applies to the first (signup) form.
  auto upload_contents_matcher = IsPasswordUpload(
      FormSignatureIs(CalculateFormSignature(saved_match_.form_data)),
      FieldsContain(
          AllOf(FieldSignatureIs(CalculateFieldSignatureForField(
                    saved_match_.form_data.fields()[kUsernameFieldIndex])),
                FieldVoteTypeIs(Field::CREDENTIALS_REUSED)),
          AllOf(FieldSignatureIs(CalculateFieldSignatureForField(
                    saved_match_.form_data.fields()[kPasswordFieldIndex])),
                FieldVoteTypeIs(Field::CREDENTIALS_REUSED))));
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(upload_contents_matcher, _, _));

  // Saved credentials from the signup form were used for the first time on a
  // submitted form. The vote applies to the new form being submitted.
  upload_contents_matcher = IsPasswordUpload(
      FormSignatureIs(CalculateFormSignature(submitted_form_)),
      FieldsContain(AllOf(FieldSignatureIs(CalculateFieldSignatureForField(
                              submitted_form_.fields()[kUsernameFieldIndex])),
                          FieldVoteTypeIs(Field::FIRST_USE)),
                    AllOf(FieldSignatureIs(CalculateFieldSignatureForField(
                              submitted_form_.fields()[kPasswordFieldIndex])),
                          FieldVoteTypeIs(Field::FIRST_USE))));
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(upload_contents_matcher, _, _));

  form_manager_->Save();
}

TEST_P(PasswordFormManagerTest, UpdateUsernameEmptyStore) {
  fetcher_->NotifyFetchCompleted();

  form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                   possible_usernames_);

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
  test_api(submitted_form_).field(0).set_value(user_chosen_username);
  test_api(submitted_form_).field(1).set_value(automatically_chosen_username);
  form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                   possible_usernames_);
  EXPECT_EQ(automatically_chosen_username,
            form_manager_->GetPendingCredentials().username_value);

  form_manager_->OnUpdateUsernameFromPrompt(user_chosen_username);
  EXPECT_EQ(user_chosen_username,
            form_manager_->GetPendingCredentials().username_value);

  auto upload_contents_matcher = IsPasswordUpload(FieldsContain(
      UploadFieldIs(submitted_form_.fields()[0], FieldType::USERNAME,
                    FieldVoteTypeIs(Field::USERNAME_EDITED)),
      UploadFieldIs(submitted_form_.fields()[kPasswordFieldIndex],
                    FieldType::PASSWORD,
                    FieldGenerationTypeIs(Field::NO_GENERATION))));
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(upload_contents_matcher, _, _));
  form_manager_->Save();
}

TEST_P(PasswordFormManagerTest, UpdateUsernameToAlreadyExisting) {
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                   possible_usernames_);

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

  form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                   possible_usernames_);

  std::u16string new_password = parsed_submitted_form_.password_value + u"1";
  PasswordForm expected = parsed_submitted_form_;
  expected.password_value = new_password;
  expected.password_element.clear();

  form_manager_->OnUpdatePasswordFromPrompt(new_password);

  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
  EXPECT_TRUE(form_manager_->IsNewLogin());

  // TODO(crbug.com/41439338): implement not sending incorrect votes and
  // check that StartUploadRequest is not called.
  EXPECT_CALL(crowdsourcing_manager(), StartUploadRequest);
  form_manager_->Save();
}

TEST_P(PasswordFormManagerTest, UpdatePasswordValueToAlreadyExisting) {
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  // Emulate submitting form with known username and different password.
  test_api(submitted_form_)
      .field(kUsernameFieldIndex)
      .set_value(saved_match_.username_value);
  form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                   possible_usernames_);

  // The user changes password to already saved one.
  std::u16string password = saved_match_.password_value;
  form_manager_->OnUpdatePasswordFromPrompt(password);

  CheckPendingCredentials(saved_match_, form_manager_->GetPendingCredentials());
  EXPECT_FALSE(form_manager_->IsNewLogin());
  EXPECT_FALSE(form_manager_->IsPasswordUpdate());
}

TEST_P(PasswordFormManagerTest, UpdatePasswordValueToUnknownValueFromPrompt) {
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  // Emulate submitting form that updates the password for a known username.
  test_api(submitted_form_)
      .field(kUsernameFieldIndex)
      .set_value(saved_match_.username_value);
  test_api(submitted_form_)
      .field(kPasswordFieldIndex)
      .set_autocomplete_attribute("new-password");
  test_api(submitted_form_)
      .field(kPasswordFieldIndex)
      .set_value(u"new_password_field_value");
  form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                   possible_usernames_);

  // The user changes password to a prevuiously unseen one.
  form_manager_->OnUpdatePasswordFromPrompt(u"totally_unexpected_value");
  EXPECT_TRUE(form_manager_->IsPasswordUpdate());

  // Since the user has modified the password value, the password field was
  // likely picked wrong. Make sure votes for password field and password
  // generation attributes are not uploaded.
  auto upload_contents_matcher = IsPasswordUpload(
      FieldsContain(UploadFieldIs(submitted_form_.fields()[kUsernameFieldIndex],
                                  FieldType::USERNAME),
                    AllOf(FieldSignatureIs(CalculateFieldSignatureForField(
                              submitted_form_.fields()[kPasswordFieldIndex])),
                          FieldGenerationTypeIs(Field::NO_GENERATION))));
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(upload_contents_matcher, _, _));
  form_manager_->Save();
}

TEST_P(PasswordFormManagerTest, UpdatePasswordValueMultiplePasswordFields) {
  FormData form = observed_form_only_password_fields_;

  CreateFormManager(form);
  fetcher_->NotifyFetchCompleted();
  std::u16string password = u"password1";
  std::u16string pin = u"pin";
  test_api(form).field(0).set_value(password);
  test_api(form).field(1).set_value(pin);
  form_manager_->ProvisionallySave(form, &driver_, possible_usernames_);

  // Check that a second password field is chosen for saving.
  EXPECT_EQ(pin, form_manager_->GetPendingCredentials().password_value);

  PasswordForm expected = form_manager_->GetPendingCredentials();
  expected.password_value = password;
  expected.password_element = form.fields()[0].name();

  // Simulate that the user updates value to save for the first password field.
  form_manager_->OnUpdatePasswordFromPrompt(password);

  // Check that newly created pending credentials are correct.
  CheckPendingCredentials(expected, form_manager_->GetPendingCredentials());
  EXPECT_TRUE(form_manager_->IsNewLogin());

  // Check that a vote is sent for the field with the value which is chosen by
  // the user.
  auto upload_contents_matcher = IsPasswordUpload(FieldsContain(UploadFieldIs(
      submitted_form_.fields()[kPasswordFieldIndex], FieldType::PASSWORD)));
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(upload_contents_matcher, _, _));

  // Check that the password which was chosen by the user is saved.
  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());
  PasswordForm saved_form;
  EXPECT_CALL(form_saver, Save).WillOnce(SaveArg<0>(&saved_form));

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
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               possible_usernames_));

  std::unique_ptr<PasswordFormManager> cloned_manager = form_manager_->Clone();

  EXPECT_TRUE(
      cloned_manager->DoesManage(observed_form_.renderer_id(), nullptr));
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
                            std::string_view metric_name) {
  const int64_t* value =
      ukm::TestUkmRecorder::GetEntryMetric(entry, metric_name);
  EXPECT_TRUE(value);
  // Ideally, an ASSERT_TRUE above would prevent the test suite from crashing on
  // dereferencing |value| below. But ASSERT_* is not available in non-void
  // returning functions, so the null value is handled explicitly.
  if (!value) {
    return false;  // Value does not matter, the test already failed.
  }
  return 1 == (1 & *value);
}

// Test that an attempt to log to ReadonlyWhenFilling UKM is made when filling.
TEST_P(PasswordFormManagerTest, RecordReadonlyWhenFilling) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  EXPECT_CALL(driver_, SetPasswordFillData);
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

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
  malformed_form.set_fields({});
  CreateFormManager(malformed_form);
  // Only create the recorder after the current form manager is created,
  // otherwise the destruction of the previous one will add unwanted UKM entries
  // in it.
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

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
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               possible_usernames_));

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
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  FormData malformed_form = submitted_form_;
  malformed_form.set_fields({});
  EXPECT_FALSE(form_manager_->ProvisionallySave(malformed_form, &driver_,
                                                possible_usernames_));

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
            form_data.fields()[kUsernameFieldIndex].value());
  EXPECT_EQ(saved_form.password_value,
            form_data.fields()[kPasswordFieldIndex].value());

  Mock::VerifyAndClearExpectations(&form_saver);

  // Check that when the generated password is edited, then it's presaved.
  form_with_generated_password.password_value += u"1";
  test_api(form_data)
      .field(kPasswordFieldIndex)
      .set_value(form_with_generated_password.password_value);
  EXPECT_CALL(form_saver,
              UpdateReplace(_, IsEmpty(), testing::Eq(u""),
                            FormHasUniqueKey(form_with_generated_password)))
      .WillOnce(SaveArg<0>(&saved_form));

  form_manager_->PresaveGeneratedPassword(
      form_with_generated_password.form_data,
      form_with_generated_password.password_value);

  EXPECT_TRUE(form_manager_->HasGeneratedPassword());
  EXPECT_EQ(saved_form.username_value,
            form_data.fields()[kUsernameFieldIndex].value());
  EXPECT_EQ(saved_form.password_value,
            form_with_generated_password.password_value);

  Mock::VerifyAndClearExpectations(&form_saver);

  // Check UKM metrics.
  form_manager_.reset();
  ExpectedGenerationUKM expected_metrics = {
      std::make_optional(1u) /* shown automatically */,
      1 /* password generated */,
      std::make_optional(1u) /* password modified */};

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
  test_api(form_data)
      .field(kUsernameFieldIndex)
      .set_value(form_with_generated_password.username_value);

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
      std::make_optional(1u) /* shown automatically */,
      1 /* password generated */,
      std::make_optional(0u) /* password modified */};

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
  EXPECT_EQ(submitted_form_.fields()[kUsernameFieldIndex].value(),
            saved_form.username_value);
  EXPECT_EQ(generated_password, saved_form.password_value);
  EXPECT_TRUE(form_manager_->HasGeneratedPassword());

  // Check that the generated password is saved.
  EXPECT_CALL(form_saver, UpdateReplace(_, IsEmpty(), testing::Eq(u""),
                                        FormHasUniqueKey(saved_form)))
      .WillOnce(SaveArg<0>(&saved_form));
  EXPECT_CALL(client_, UpdateFormManagers());

  EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               possible_usernames_));
  form_manager_->Save();

  EXPECT_EQ(submitted_form_.fields()[kUsernameFieldIndex].value(),
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
  form.form_data.set_url(
      GURL("https://accounts.google.com/a/ServiceLoginAuth"));

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
      std::make_optional(2u) /* shown manually */,
      0 /* password generated */,
      {} /* generated password is not modified */};

  CheckPasswordGenerationUKM(test_ukm_recorder, expected_metrics);
}

TEST_P(PasswordFormManagerTest, PresaveGeneratedPasswordExistingCredential) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

  form_manager_->SetGenerationPopupWasShown(PasswordGenerationType::kAutomatic);

  // Check that the generated password is presaved.
  PasswordForm saved_form;
  EXPECT_CALL(form_saver, Save(_, _, _)).WillOnce(SaveArg<0>(&saved_form));

  PasswordForm form_with_generated_password = parsed_submitted_form_;
  FormData& form_data = form_with_generated_password.form_data;

  // Check that the generated password is saved with the empty username when
  // there is already a saved credential with the same username.
  test_api(form_data)
      .field(kUsernameFieldIndex)
      .set_value(saved_match_.username_value);
  form_manager_->PresaveGeneratedPassword(
      form_with_generated_password.form_data,
      form_with_generated_password.password_value);

  EXPECT_TRUE(form_manager_->HasGeneratedPassword());
  EXPECT_TRUE(saved_form.username_value.empty());
  EXPECT_EQ(form_with_generated_password.password_value,
            saved_form.password_value);
}

TEST_P(PasswordFormManagerTest, RecordsExactMatch) {
  PasswordFormManager::set_wait_for_server_predictions_for_filling(false);
  base::HistogramTester histogram_tester;
  CreateFormManager(observed_form_);
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  fetcher_->set_preferred_or_potential_matched_form_type(
      PasswordFormMetricsRecorder::MatchedFormType::kExactMatch);

  form_manager_->Fill();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.MatchedFormType",
      PasswordFormMetricsRecorder::MatchedFormType::kExactMatch, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PotentialBestMatchFormType",
      PasswordFormMetricsRecorder::MatchedFormType::kExactMatch, 1);
}

TEST_P(PasswordFormManagerTest, RecordsPSLMatch) {
  PasswordFormManager::set_wait_for_server_predictions_for_filling(false);
  base::HistogramTester histogram_tester;
  CreateFormManager(observed_form_);
  SetNonFederatedAndNotifyFetchCompleted({psl_saved_match_});

  fetcher_->set_preferred_or_potential_matched_form_type(
      PasswordFormMetricsRecorder::MatchedFormType::kPublicSuffixMatch);

  form_manager_->Fill();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.MatchedFormType",
      PasswordFormMetricsRecorder::MatchedFormType::kPublicSuffixMatch, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PotentialBestMatchFormType",
      PasswordFormMetricsRecorder::MatchedFormType::kPublicSuffixMatch, 1);
}

TEST_P(PasswordFormManagerTest, RecordsAffiliatedWebsiteMatch) {
  PasswordFormManager::set_wait_for_server_predictions_for_filling(false);
  base::HistogramTester histogram_tester;
  CreateFormManager(observed_form_);

  PasswordForm affiliated_website_form = saved_match_;
  affiliated_website_form.url =
      GURL("https://affiliated.domain.com/a/ServiceLoginAuth");
  affiliated_website_form.action =
      GURL("https://affiliated.domain.com/a/ServiceLogin");
  affiliated_website_form.signon_realm = "https://affiliated.domain.com/";
  affiliated_website_form.match_type = PasswordForm::MatchType::kAffiliated;
  fetcher_->set_preferred_or_potential_matched_form_type(
      PasswordFormMetricsRecorder::MatchedFormType::kAffiliatedWebsites);
  SetNonFederatedAndNotifyFetchCompleted({affiliated_website_form});

  form_manager_->Fill();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.MatchedFormType",
      PasswordFormMetricsRecorder::MatchedFormType::kAffiliatedWebsites, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PotentialBestMatchFormType",
      PasswordFormMetricsRecorder::MatchedFormType::kAffiliatedWebsites, 1);
}

TEST_P(PasswordFormManagerTest, RecordsAffiliatedAndroidAppMatch) {
  PasswordFormManager::set_wait_for_server_predictions_for_filling(false);
  base::HistogramTester histogram_tester;
  CreateFormManager(observed_form_);

  PasswordForm affiliated_app_form = saved_match_;
  affiliated_app_form.url = GURL("android://hash@com.example.android/");
  affiliated_app_form.action = GURL("android://hash@com.example.android/");
  affiliated_app_form.signon_realm = "android://hash@com.example.android/";
  affiliated_app_form.match_type = PasswordForm::MatchType::kAffiliated;
  fetcher_->set_preferred_or_potential_matched_form_type(
      PasswordFormMetricsRecorder::MatchedFormType::kAffiliatedApp);
  SetNonFederatedAndNotifyFetchCompleted({affiliated_app_form});

  form_manager_->Fill();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.MatchedFormType",
      PasswordFormMetricsRecorder::MatchedFormType::kAffiliatedApp, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PotentialBestMatchFormType",
      PasswordFormMetricsRecorder::MatchedFormType::kAffiliatedApp, 1);
}

TEST_P(PasswordFormManagerTest, RecordsGroupedWebsiteMatch) {
  PasswordFormManager::set_wait_for_server_predictions_for_filling(false);
  base::HistogramTester histogram_tester;
  CreateFormManager(observed_form_);

  // Grouped credentials are ignored by the form fetched and are not returned to
  // the consumers. The only way to detect them is via the
  // `FormFetched::GetPreferredOrPotentialMatchedFormType()` API.
  fetcher_->set_preferred_or_potential_matched_form_type(
      PasswordFormMetricsRecorder::MatchedFormType::kGroupedWebsites);
  SetNonFederatedAndNotifyFetchCompleted({});

  form_manager_->Fill();

  // `PasswordManager.MatchedFormType` metric is not recorded for the grouped
  // credentials. It is only recorded when the best match is available.
  histogram_tester.ExpectTotalCount("PasswordManager.MatchedFormType", 0);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PotentialBestMatchFormType",
      PasswordFormMetricsRecorder::MatchedFormType::kGroupedWebsites, 1);
}

TEST_P(PasswordFormManagerTest, RecordsGroupedAppMatch) {
  PasswordFormManager::set_wait_for_server_predictions_for_filling(false);
  base::HistogramTester histogram_tester;
  CreateFormManager(observed_form_);

  // Grouped credentials are ignored by the form fetched and are not returned to
  // the consumers. The only way to detect them is via the
  // `FormFetched::GetPreferredOrPotentialMatchedFormType()` API.
  fetcher_->set_preferred_or_potential_matched_form_type(
      PasswordFormMetricsRecorder::MatchedFormType::kGroupedApp);
  SetNonFederatedAndNotifyFetchCompleted({});

  form_manager_->Fill();

  // `PasswordManager.MatchedFormType` metric is not recorded for the grouped
  // credentials. It is only recorded when the best match is available.
  histogram_tester.ExpectTotalCount("PasswordManager.MatchedFormType", 0);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PotentialBestMatchFormType",
      PasswordFormMetricsRecorder::MatchedFormType::kGroupedApp, 1);
}

TEST_P(PasswordFormManagerTest, RecordsNoMatchesWhenNoCredentialsFetched) {
  PasswordFormManager::set_wait_for_server_predictions_for_filling(false);
  base::HistogramTester histogram_tester;
  CreateFormManager(observed_form_);

  SetNonFederatedAndNotifyFetchCompleted({});

  form_manager_->Fill();

  histogram_tester.ExpectTotalCount("PasswordManager.MatchedFormType", 0);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.PotentialBestMatchFormType", 0);
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
    test_api(form_data)
        .field(kPasswordFieldIndex)
        .set_value(form_data.fields()[kPasswordFieldIndex].value() + u"1");
    submitted_form.password_value =
        form_data.fields()[kPasswordFieldIndex].value();
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
    test_api(form_data)
        .field(kPasswordFieldIndex)
        .set_value(form_data.fields()[kPasswordFieldIndex].value() + u"2");
    submitted_form.password_value =
        form_data.fields()[kPasswordFieldIndex].value();
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
  test_api(form)
      .field(kUsernameFieldIndex)
      .set_renderer_id(FieldRendererId(
          form.fields()[kUsernameFieldIndex].renderer_id().value() + 100));
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
  test_api(form)
      .field(kUsernameFieldIndex)
      .set_name(form.fields()[kUsernameFieldIndex].name() + u"123");
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
  test_api(form)
      .field(kUsernameFieldIndex)
      .set_autocomplete_attribute(
          form.fields()[kUsernameFieldIndex].autocomplete_attribute() + "...");
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
  test_api(form)
      .field(kUsernameFieldIndex)
      .set_form_control_type(autofill::FormControlType::kInputPassword);
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
  test_api(form).Append(autofill::test::CreateTestFormField(
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
  test_api(form).field(kUsernameFieldIndex).set_css_classes(u"class1");
  EXPECT_FALSE(HasObservedFormChanged(form, *form_manager_));
  form_manager_.reset();

  histogram_tester.ExpectUniqueSample("PasswordManager.DynamicFormChanges", 0,
                                      1);
}

TEST_P(PasswordFormManagerTest, UpdateFormAndFill) {
  CreateFormManager(observed_form_);
  EXPECT_CALL(driver_, SetPasswordFillData);
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});
  task_environment_.FastForwardUntilNoTasksRemain();
  Mock::VerifyAndClearExpectations(&driver_);

  FormData form = observed_form_;
  test_api(form)
      .field(kUsernameFieldIndex)
      .set_renderer_id(FieldRendererId(
          form.fields()[kUsernameFieldIndex].renderer_id().value() + 1000));
  test_api(form)
      .field(kUsernameFieldIndex)
      .set_name(form.fields()[kUsernameFieldIndex].name() + u"1");
  test_api(form)
      .field(kUsernameFieldIndex)
      .set_id_attribute(form.fields()[kUsernameFieldIndex].id_attribute() +
                        u"1");
  test_api(form)
      .field(kPasswordFieldIndex)
      .set_renderer_id(FieldRendererId(
          form.fields()[kPasswordFieldIndex].renderer_id().value() + 1000));

  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, SetPasswordFillData).WillOnce(SaveArg<0>(&fill_data));
  form_manager_->UpdateFormManagerWithFormChanges(form, {});
  form_manager_->Fill();
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_EQ(form.fields()[kUsernameFieldIndex].renderer_id(),
            fill_data.username_element_renderer_id);
  EXPECT_EQ(saved_match_.username_value,
            fill_data.preferred_login.username_value);
  EXPECT_EQ(form.fields()[kPasswordFieldIndex].renderer_id(),
            fill_data.password_element_renderer_id);
  EXPECT_EQ(saved_match_.password_value,
            fill_data.preferred_login.password_value);
}

TEST_P(PasswordFormManagerTest, FillWaitsForServerPredictions) {
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  EXPECT_CALL(driver_, SetPasswordFillData).Times(0);
  form_manager_->Fill();
  Mock::VerifyAndClearExpectations(&driver_);

  // Check that the changed form is filled after the filling timeout expires.
  EXPECT_CALL(driver_, SetPasswordFillData);
  task_environment_.FastForwardBy(kMaxFillingDelayForAsyncPredictions);
}

TEST_P(PasswordFormManagerTest, RepeatedFillDoesNotResetTimer) {
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  EXPECT_CALL(driver_, SetPasswordFillData).Times(0);
  form_manager_->Fill();
  task_environment_.FastForwardBy(kMaxFillingDelayForAsyncPredictions / 2);
  form_manager_->Fill();

  // Check that the fill call is not delayed further.
  EXPECT_CALL(driver_, SetPasswordFillData);
  task_environment_.FastForwardBy(kMaxFillingDelayForAsyncPredictions / 2);
}

TEST_P(PasswordFormManagerTest, UpdateFormManagerWithFormChangesResetsTimer) {
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  FormData changed_form = observed_form_;
  test_api(changed_form)
      .field(kUsernameFieldIndex)
      .set_renderer_id(FieldRendererId(
          changed_form.fields()[kUsernameFieldIndex].renderer_id().value() +
          1000));
  test_api(changed_form)
      .field(kPasswordFieldIndex)
      .set_renderer_id(FieldRendererId(
          changed_form.fields()[kPasswordFieldIndex].renderer_id().value() +
          1000));

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
  EXPECT_EQ(changed_form.fields()[kUsernameFieldIndex].renderer_id(),
            fill_data.username_element_renderer_id);
  EXPECT_EQ(changed_form.fields()[kPasswordFieldIndex].renderer_id(),
            fill_data.password_element_renderer_id);
}

TEST_P(PasswordFormManagerTest, Update) {
  PasswordForm not_best_saved_match = saved_match_;
  PasswordForm saved_match_another_username = saved_match_;
  saved_match_another_username.username_value += u"1";
  SetNonFederatedAndNotifyFetchCompleted(
      {saved_match_, saved_match_another_username});

  FormData submitted_form = observed_form_;
  std::u16string username = saved_match_.username_value;
  std::u16string new_password = saved_match_.password_value + u"1";
  test_api(submitted_form).field(kUsernameFieldIndex).set_value(username);
  test_api(submitted_form).field(kPasswordFieldIndex).set_value(new_password);

  EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames_));

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
  form_manager_->Save();

  EXPECT_TRUE(ArePasswordFormUniqueKeysEqual(saved_match_, updated_form));
  EXPECT_EQ(new_password, updated_form.password_value);
  EXPECT_GE(updated_form.date_last_used, kNow);
}

TEST_P(PasswordFormManagerTest, FillingAssistanceMetric) {
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  // Simulate that the user fills the saved credentials manually.
  test_api(submitted_form_)
      .field(kUsernameFieldIndex)
      .set_value(saved_match_.username_value);
  test_api(submitted_form_)
      .field(kUsernameFieldIndex)
      .set_properties_mask(FieldPropertiesFlags::kAutofilledOnUserTrigger);
  test_api(submitted_form_)
      .field(kPasswordFieldIndex)
      .set_value(saved_match_.password_value);
  test_api(submitted_form_)
      .field(kPasswordFieldIndex)
      .set_properties_mask(FieldPropertiesFlags::kAutofilledOnUserTrigger);

  base::HistogramTester histogram_tester;
  //  Simulate successful submission.
  form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                   possible_usernames_);
  form_manager_->GetMetricsRecorder()->LogSubmitPassed();

  form_manager_.reset();

  // Verify that the filling assistance metric for single username forms isn't
  // recorded for password forms.
  histogram_tester.ExpectTotalCount(
      "PasswordManager.SingleUsernameFillingAssistance", 0);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.FillingAssistance",
      PasswordFormMetricsRecorder::FillingAssistance::kManual, 1);
}

// Test calculating the filling assistance metric on a single username form on
// provisional save.
TEST_P(PasswordFormManagerTest, FillingAssistanceMetric_SingleUsernameForm) {
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  // Simulate that the user fills the saved username manually.
  test_api(non_password_form_)
      .field(kUsernameFieldIndex)
      .set_value(saved_match_.username_value);
  test_api(non_password_form_)
      .field(kUsernameFieldIndex)
      .set_autocomplete_attribute("username");
  test_api(non_password_form_)
      .field(kUsernameFieldIndex)
      .set_properties_mask(FieldPropertiesFlags::kAutofilledOnUserTrigger);

  base::HistogramTester histogram_tester;

  form_manager_->ProvisionallySave(non_password_form_, &driver_,
                                   possible_usernames_);
  form_manager_.reset();

  // Verify that the filling assistance metric for forms with a password isn't
  // recorded for single username forms.
  histogram_tester.ExpectTotalCount("PasswordManager.FillingAssistance", 0);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.FillingAssistanceForSingleUsername",
      PasswordFormMetricsRecorder::SingleUsernameFillingAssistance::kManual, 1);
}

TEST_P(PasswordFormManagerTest, PasswordRevealedVote) {
  for (bool password_revealed : {false, true}) {
    SCOPED_TRACE(testing::Message("password_revealed=") << password_revealed);
    CreateFormManager(observed_form_);
    fetcher_->NotifyFetchCompleted();

    EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                                 possible_usernames_));

    if (password_revealed) {
      form_manager_->OnPasswordsRevealed();
    }

    EXPECT_CALL(
        crowdsourcing_manager(),
        StartUploadRequest(
            IsPasswordUpload(PasswordsRevealedIs(password_revealed)), _, _));
    form_manager_->Save();
    Mock::VerifyAndClearExpectations(&crowdsourcing_manager());
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
    EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                                 possible_usernames_));

    auto upload_contents_matcher = IsPasswordUpload(FieldsContain(
        AllOf(FieldSignatureIs(CalculateFieldSignatureForField(
                  submitted_form_.fields()[kPasswordFieldIndex])),
              FieldGenerationTypeIs(Field::IGNORED_GENERATION_POPUP))));
    EXPECT_CALL(crowdsourcing_manager(),
                StartUploadRequest(upload_contents_matcher, _, _))
        .Times(generation_popup_shown ? 1 : 0);
    form_manager_->OnNoInteraction(false /*is_update */);
    Mock::VerifyAndClearExpectations(&crowdsourcing_manager());
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
    EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                                 possible_usernames_));

    auto upload_contents_matcher = IsPasswordUpload(FieldsContain(
        AllOf(FieldSignatureIs(CalculateFieldSignatureForField(
                  submitted_form_.fields()[kPasswordFieldIndex])),
              FieldGenerationTypeIs(Field::IGNORED_GENERATION_POPUP))));
    EXPECT_CALL(crowdsourcing_manager(),
                StartUploadRequest(upload_contents_matcher, _, _))
        .Times(generation_popup_shown ? 1 : 0);

    form_manager_->OnNeverClicked();
    Mock::VerifyAndClearExpectations(&crowdsourcing_manager());
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

    std::vector<PasswordForm> saved_matches;
    if (html_credentials_saved) {
      saved_matches.push_back(saved_match_);
    }

    fetcher_->SetNonFederated(saved_matches, {});
    fetcher_->NotifyFetchCompleted();

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
  SetNonFederatedAndNotifyFetchCompleted({http_auth_form});

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
  SetNonFederatedAndNotifyFetchCompleted({saved_http_auth_form});

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
  FormFieldData& username_field =
      test_api(form_to_presave).field(kUsernameFieldIndex);
  FormFieldData& password_field =
      test_api(form_to_presave).field(kPasswordFieldIndex);
  username_field.set_value(typed_username);
  password_field.set_value(u"not_password");
  // Use |generated_password| different from value in field to test that the
  // generated password is saved.
  const std::u16string generated_password = u"gen_pw";
  FieldRendererId generation_element = password_field.renderer_id();
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

  form_manager_->UpdateStateOnUserInput(form_to_presave.renderer_id(),
                                        generation_element, changed_password);
  EXPECT_EQ(username_field.value(), saved_form.username_value);
  EXPECT_EQ(changed_password, saved_form.password_value);
}

TEST_P(PasswordFormManagerTest, iOSUpdateStateWithoutPresaving) {
  fetcher_->NotifyFetchCompleted();
  MockFormSaver& form_saver = MockFormSaver::Get(form_manager_.get());

  FieldRendererId password_field =
      observed_form_.fields()[kPasswordFieldIndex].renderer_id();
  const std::u16string new_field_value = u"some_password";

  // Check that nothing is saved on changing password, in case when there was no
  // pre-saving.
  EXPECT_CALL(form_saver, Save).Times(0);
  form_manager_->UpdateStateOnUserInput(observed_form_.renderer_id(),
                                        password_field, new_field_value);

  EXPECT_EQ(
      new_field_value,
      form_manager_->observed_form()->fields()[kPasswordFieldIndex].value());
}

TEST_P(PasswordFormManagerTest, iOSUsingFieldDataManagerData) {
  CreateFormManager(observed_form_);

  auto field_data_manager = base::MakeRefCounted<autofill::FieldDataManager>();
  field_data_manager->UpdateFieldDataMap(
      observed_form_.fields()[1].renderer_id(), u"typed_username",
      FieldPropertiesFlags::kUserTyped);
  field_data_manager->UpdateFieldDataMap(
      observed_form_.fields()[2].renderer_id(), u"autofilled_pw",
      FieldPropertiesFlags::kAutofilledOnUserTrigger);

  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames(2);
  form_manager_->ProvisionallySaveFieldDataManagerInfo(
      *field_data_manager, &driver_, possible_usernames);

  EXPECT_EQ(form_manager_->observed_form()->fields()[1].user_input(),
            u"typed_username");
  EXPECT_EQ(form_manager_->observed_form()->fields()[1].properties_mask(),
            FieldPropertiesFlags::kUserTyped);

  EXPECT_EQ(form_manager_->observed_form()->fields()[2].user_input(),
            u"autofilled_pw");
  EXPECT_EQ(form_manager_->observed_form()->fields()[2].properties_mask(),
            FieldPropertiesFlags::kAutofilledOnUserTrigger);
}

// Tests provisional saving of credentials from field data manager info update
// during username first flow.
TEST_P(PasswordFormManagerTest,
       iOSUsingFieldDataManagerDataInUsernameFirstFlow) {
  FormData observed_form = observed_form_only_password_fields_;

  CreateFormManager(observed_form);
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  auto field_data_manager = base::MakeRefCounted<autofill::FieldDataManager>();
  field_data_manager->UpdateFieldDataMap(
      observed_form.fields()[0].renderer_id(), u"typed_password",
      FieldPropertiesFlags::kUserTyped);

  // Create possible username data with predictions.
  constexpr autofill::FieldRendererId kUsernameFieldRendererId(101);
  const std::u16string possible_username = u"test@example.com";
  PossibleUsernameData possible_username_data(
      saved_match_.signon_realm, kUsernameFieldRendererId, possible_username,
      base::Time::Now(), /*driver_id=*/0,
      /*autocomplete_attribute_has_username=*/false, /*is_likely_otp=*/false);
  possible_username_data.form_predictions = MakeSingleUsernamePredictions(
      kSingleUsernameFormSignature, kUsernameFieldRendererId,
      kSingleUsernameFieldSignature);
  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames = MakePossibleUsernamesCache({possible_username_data});

  form_manager_->ProvisionallySaveFieldDataManagerInfo(
      *field_data_manager, &driver_, possible_usernames);

  // Check that a username is chosen from |possible_username_data|.
  EXPECT_EQ(possible_username,
            form_manager_->GetPendingCredentials().username_value);
}

#endif  // BUILDFLAG(IS_IOS)

// Tests provisional saving of credentials during username first flow.
TEST_P(PasswordFormManagerTest, UsernameFirstFlowProvisionalSave) {
  CreateFormManager(observed_form_only_password_fields_);
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  // Create possible username data.
  constexpr autofill::FieldRendererId kUsernameFieldRendererId(101);
  const std::u16string possible_username = u"test@example.com";
  PossibleUsernameData possible_username_data(
      saved_match_.signon_realm, kUsernameFieldRendererId, possible_username,
      base::Time::Now(), /*driver_id=*/0,
      /*autocomplete_attribute_has_username=*/false, /*is_likely_otp=*/false);

  FormData submitted_form = observed_form_only_password_fields_;
  test_api(submitted_form).field(0).set_value(u"strongpassword");

  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames = MakePossibleUsernamesCache({possible_username_data});

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames));

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
                                               possible_usernames));

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
  test_api(submitted_form).field(0).set_value(u"strongpassword");

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames));

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
  test_api(submitted_form).field(0).set_value(u"strongpassword");

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames));

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
  test_api(submitted_form).field(0).set_autocomplete_attribute("new-password");
  test_api(submitted_form).field(0).set_value(u"strongpassword");

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames));

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
      observed_form_.fields()[1].renderer_id();
  std::u16string possible_username = u"possible_username";
  PossibleUsernameData possible_username_data(
      GetSignonRealm(observed_form_.url()), kUsernameFieldRendererId,
      possible_username, base::Time::Now(),
      /*driver_id=*/0, /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);
  FormPredictions predictions;
  predictions.form_signature = CalculateFormSignature(observed_form_);
  predictions.fields.emplace_back(
      kUsernameFieldRendererId,
      CalculateFieldSignatureForField(observed_form_.fields()[1]),
      FieldType::UNKNOWN_TYPE,
      /*may_use_prefilled_placeholder=*/false,
      /*is_override=*/false);

  possible_username_data.form_predictions = predictions;
  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames = MakePossibleUsernamesCache({possible_username_data});

  // Simulate submitting the form.
  FormData submitted_form = observed_form_;
  test_api(submitted_form).field(1).set_value(possible_username);
  test_api(submitted_form).field(2).set_value(u"strongpassword");
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames));

  // Check that no SINGLE_USERNAME vote is sent.
  auto upload_contents_matcher = IsPasswordUpload(
      FormSignatureIs(CalculateFormSignature(submitted_form)),
      FieldsContain(
          UploadFieldIs(submitted_form.fields()[1], FieldType::UNKNOWN_TYPE),
          UploadFieldIs(submitted_form.fields()[2], FieldType::PASSWORD)));
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(upload_contents_matcher, _, _));
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
      SetNonFederatedAndNotifyFetchCompleted({saved_match_});
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
    test_api(submitted_form).field(0).set_value(u"oldpassword");
    // Simulate password reset flow for password update case.
    if (is_password_update) {
      test_api(submitted_form).field(1).set_value(u"newpassword");
    }

    ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                                 possible_usernames));
    EXPECT_EQ(form_manager_->IsPasswordUpdate(), is_password_update);

    // Check that uploads for both username and password form happen.
    testing::InSequence in_sequence;

    // Upload username first flow votes on the username form.
    if constexpr (!BUILDFLAG(IS_ANDROID)) {
      EXPECT_CALL(crowdsourcing_manager(),
                  StartUploadRequest(IsSingleUsernameUpload(), _, _));
    }

    // Upload username first flow votes on the password form.
    AutofillUploadContents::SingleUsernameData expected_single_username_data;
    expected_single_username_data.set_username_form_signature(
        kSingleUsernameFormSignature.value());
    expected_single_username_data.set_username_field_signature(
        kSingleUsernameFieldSignature.value());
    expected_single_username_data.set_value_type(
        is_password_update ? AutofillUploadContents::STORED_FOR_CURRENT_DOMAIN
                           : AutofillUploadContents::USERNAME_LIKE);
    expected_single_username_data.set_prompt_edit(
        AutofillUploadContents::EDITED_POSITIVE);
    auto upload_contents_matcher = IsPasswordUpload(
        FormSignatureIs(CalculateFormSignature(submitted_form)),
        SingleUsernameDataIs(
            EqualsSingleUsernameDataVector({expected_single_username_data})));
    EXPECT_CALL(crowdsourcing_manager(),
                StartUploadRequest(upload_contents_matcher, _, _));

    if (is_password_update) {
      EXPECT_CALL(crowdsourcing_manager(),
                  StartUploadRequest(
                      IsPasswordUpload(SingleUsernameDataIs(IsEmpty())), _, _));
    }

    base::HistogramTester histogram_tester;

    form_manager_->Save();

#if !BUILDFLAG(IS_ANDROID)
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.SingleUsername.PasswordFormHadUsernameField", 0, 1);
#else
    histogram_tester.ExpectTotalCount(
        "PasswordManager.SingleUsername.PasswordFormHadUsernameField", 0);
#endif
    Mock::VerifyAndClearExpectations(&crowdsourcing_manager());
  }
}

// Tests that if the username matches the single username from the previous
// form, a vote is set.
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
      submitted_form_.fields()[kUsernameFieldIndex].value(), base::Time::Now(),
      /*driver_id=*/0,
      /*autocomplete_attribute_has_username=*/false, /*is_likely_otp=*/false);
  possible_username_data.form_predictions = MakeSingleUsernamePredictions(
      kSingleUsernameFormSignature, kSingleUsernameFieldRendererId,
      kSingleUsernameFieldSignature);
  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames = MakePossibleUsernamesCache({possible_username_data});

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               possible_usernames));

  // Check that uploads for both single username and sign-up form happen.
  testing::InSequence in_sequence;

  // Upload username first flow vote on the single username form.
  if constexpr (!BUILDFLAG(IS_ANDROID)) {
    EXPECT_CALL(crowdsourcing_manager(),
                StartUploadRequest(IsSingleUsernameUpload(), _, _));
  }

  // Upload username first flow vote on the sign-up form.
  AutofillUploadContents::SingleUsernameData expected_single_username_data;
  expected_single_username_data.set_username_form_signature(
      kSingleUsernameFormSignature.value());
  expected_single_username_data.set_username_field_signature(
      kSingleUsernameFieldSignature.value());
  expected_single_username_data.set_value_type(
      AutofillUploadContents::VALUE_WITH_NO_WHITESPACE);
  expected_single_username_data.set_prompt_edit(
      AutofillUploadContents::NOT_EDITED_POSITIVE);
  auto upload_contents_matcher = IsPasswordUpload(
      FormSignatureIs(CalculateFormSignature(submitted_form_)),
      SingleUsernameDataIs(
          EqualsSingleUsernameDataVector({expected_single_username_data})));
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(upload_contents_matcher, _, _));

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
  Mock::VerifyAndClearExpectations(&crowdsourcing_manager());
}

// Tests that when the save/update prompt suggests single username value and the
// user edits username prompt to the value of one of the text fields found in
// the password form negative in form overrule vote is sent.
TEST_P(PasswordFormManagerTest, UsernameFirstFlowInFormOverruleVotes) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kUsernameFirstFlowFallbackCrowdsourcing,
                            features::
                                kUsernameFirstFlowWithIntermediateValuesVoting},
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
  test_api(submitted_form_)
      .field(kPasswordFieldIndex)
      .set_value(u"strongpassword");
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               possible_usernames));
  form_manager_->SaveSuggestedUsernameValueToVotesUploader();

  // Simulate the user modifying the username in the prompt.
  form_manager_->OnUpdateUsernameFromPrompt(
      submitted_form_.fields()[kUsernameFieldIndex].value());

#if !BUILDFLAG(IS_ANDROID)
  // Expect a negative `IN_FORM_OVERRULE` vote on the username form.
  ExpectIsSingleUsernameUpload(
      kSingleUsernameFormSignature, Field::IN_FORM_OVERRULE,
      FieldType::NOT_USERNAME,
      IsMostRecentSingleUsernameCandidate::kMostRecentCandidate);
#endif

  // Expect upload for the password form. This upload is unrelated to UFF: it
  // is a result of saving a new password on the password form.
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(IsPasswordUpload(FormSignatureIs(
                                     CalculateFormSignature(submitted_form_))),
                                 _, _));
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
                            features::
                                kUsernameFirstFlowWithIntermediateValuesVoting},
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
  test_api(submitted_form_)
      .field(kPasswordFieldIndex)
      .set_value(u"strongpassword");
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               possible_usernames));
  form_manager_->SaveSuggestedUsernameValueToVotesUploader();

  // Simulate the user modifying the username in the prompt.
  form_manager_->OnUpdateUsernameFromPrompt(kPossibleUsername);

  // Check that uploads for both single username and sign-up form happen.
  testing::InSequence in_sequence;

#if !BUILDFLAG(IS_ANDROID)
  // Expect a positive `IN_FORM_OVERRULE` vote on the username form.
  ExpectIsSingleUsernameUpload(
      kSingleUsernameFormSignature, Field::IN_FORM_OVERRULE,
      FieldType::SINGLE_USERNAME,
      IsMostRecentSingleUsernameCandidate::kMostRecentCandidate);
#endif

  // Expect upload for the password form. This upload is unrelated to UFF: it
  // is a result of saving a new password on the password form.
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(IsPasswordUpload(FormSignatureIs(
                                     CalculateFormSignature(submitted_form_))),
                                 _, _));
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
                            features::
                                kUsernameFirstFlowWithIntermediateValuesVoting},
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
  test_api(submitted_form_)
      .field(kPasswordFieldIndex)
      .set_value(u"strongpassword");
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               possible_usernames));
  form_manager_->SaveSuggestedUsernameValueToVotesUploader();

  // Expect no upload on single text field.
  EXPECT_CALL(
      crowdsourcing_manager(),
      StartUploadRequest(
          IsPasswordUpload(FormSignatureIs(kSingleUsernameFormSignature)), _,
          _))
      .Times(0);

  // Expect upload for the password form. This upload is unrelated to UFF: it
  // is a result of saving a new password on the password form.
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(IsPasswordUpload(FormSignatureIs(
                                     CalculateFormSignature(submitted_form_))),
                                 _, _));
  form_manager_->Save();
}

// Tests that if prefilled username inside the password form has a matching
// value with a field found outside of the password form the vote will be sent
// correctly. In this test case, there is an intermediary field between them.
TEST_P(PasswordFormManagerTest,
       UsernameFirstFlowWithIntermediaryFieldsAndPrefilledUsername) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kUsernameFirstFlowFallbackCrowdsourcing},
      /*disabled_features=*/{});

  CreateFormManager(submitted_form_);
  fetcher_->NotifyFetchCompleted();

  // Create possible username data.
  PossibleUsernameData possible_username_data(
      saved_match_.signon_realm, kSingleUsernameFieldRendererId,
      submitted_form_.fields()[kUsernameFieldIndex].value(), base::Time::Now(),
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
                                               possible_usernames));

  // Check that uploads for both single username and sign-up form happen.
  testing::InSequence in_sequence;

  // Upload username first flow vote on the single username form.
  if constexpr (!BUILDFLAG(IS_ANDROID)) {
    EXPECT_CALL(crowdsourcing_manager(),
                StartUploadRequest(IsSingleUsernameUpload(), _, _));
  }

  // Upload username first flow vote on the sign-up form.
  AutofillUploadContents::SingleUsernameData expected_single_username_data;
  expected_single_username_data.set_username_form_signature(
      kSingleUsernameFormSignature.value());
  expected_single_username_data.set_username_field_signature(
      kSingleUsernameFieldSignature.value());
  expected_single_username_data.set_value_type(
      AutofillUploadContents::VALUE_WITH_NO_WHITESPACE);
  expected_single_username_data.set_prompt_edit(
      AutofillUploadContents::NOT_EDITED_POSITIVE);
  auto upload_contents_matcher = IsPasswordUpload(
      FormSignatureIs(CalculateFormSignature(submitted_form_)),
      SingleUsernameDataIs(
          EqualsSingleUsernameDataVector({expected_single_username_data})));
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(upload_contents_matcher, _, _));

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
  Mock::VerifyAndClearExpectations(&crowdsourcing_manager());
}

// Tests that when the user edits the username value in the save prompt
// manually, a strong positive vote is sent for the form that user has typed the
// saved value into, and a strong negative vote is sent for the form, into which
// the user has typed a different value.
TEST_P(PasswordFormManagerTest, UsernameFirstFlowSendVotesOnRecentFields) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kUsernameFirstFlowFallbackCrowdsourcing,
                            features::
                                kUsernameFirstFlowWithIntermediateValuesVoting},
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
  test_api(submitted_form).field(0).set_value(u"strongpassword");
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames));

  form_manager_->SaveSuggestedUsernameValueToVotesUploader();

  // Simulate the user modifying the username in the prompt.
  form_manager_->OnUpdateUsernameFromPrompt(kPossibleUsername);

#if !BUILDFLAG(IS_ANDROID)
  // Expect a strong positive vote on the single username form.
  ExpectIsSingleUsernameUpload(
      kSingleUsernameFormSignature, Field::STRONG, FieldType::SINGLE_USERNAME,
      IsMostRecentSingleUsernameCandidate::kMostRecentCandidate);

  // Expect a strong negative vote on another single text field form.
  ExpectIsSingleUsernameUpload(
      kOtherFormSignature, Field::STRONG, FieldType::NOT_USERNAME,
      IsMostRecentSingleUsernameCandidate::kHasIntermediateValuesInBetween);
#endif

  // Expect upload for the password form. This upload is unrelated to UFF: it
  // is a result of saving a new password on the password form.
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(IsPasswordUpload(FormSignatureIs(
                                     CalculateFormSignature(submitted_form))),
                                 _, _));

  form_manager_->Save();
}

// Test that the username field in a single username form can be filled.
TEST_P(PasswordFormManagerTest, UsernameFirstFlowFillSingleUsernameForm) {
  // Create the form manager for a form with only a username in it.
  CreateFormManager(non_password_form_);

  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, SetPasswordFillData).WillOnce(SaveArg<0>(&fill_data));

  // Provide server predictions for the single username form, which will trigger
  // FillNow().
  std::map<FormSignature, FormPredictions> predictions =
      CreatePredictions(non_password_form_,
                        {std::make_pair(kUsernameFieldIndex, SINGLE_USERNAME)});
  form_manager_->ProcessServerPredictions(predictions);

  // Verify the fill data used for autofilling the single username form.
  EXPECT_EQ(non_password_form_.fields()[kUsernameFieldIndex].renderer_id(),
            fill_data.username_element_renderer_id);
  EXPECT_EQ(saved_match_.username_value,
            fill_data.preferred_login.username_value);
  EXPECT_EQ(autofill::FieldRendererId(),
            fill_data.password_element_renderer_id);
  EXPECT_EQ(saved_match_.password_value,
            fill_data.preferred_login.password_value);
}

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
  predictions.fields.emplace_back(kUsernameFieldRendererId,
                                  kUsernameFieldSignature, SINGLE_USERNAME,
                                  /*may_use_prefilled_placeholder=*/false,
                                  /*is_override=*/false);
  possible_username_data.form_predictions = std::move(predictions);
  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames = MakePossibleUsernamesCache({possible_username_data});

  // Simulate submission a form without username. Data from
  // |possible_username_data| will be taken for setting username.
  FormData submitted_form = observed_form_only_password_fields_;
  test_api(submitted_form).field(0).set_value(u"strongpassword");
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames));

  // Simulate showing the prompt and saving the suggested value.
  // TODO(crbug.com/40626063) Add a unittest for the case when this method is
  // not called.
  form_manager_->SaveSuggestedUsernameValueToVotesUploader();

  // Simulate the user modifying the username in the prompt.
  form_manager_->OnUpdateUsernameFromPrompt(u"different_username");

  // Check that uploads for both username and password form happen.
  testing::InSequence in_sequence;

  // Upload for the username form. Ensure that we send `NOT_USERNAME` for the
  // username field.
  if constexpr (!BUILDFLAG(IS_ANDROID)) {
    auto upload_contents_matcher = IsPasswordUpload(
        FormSignatureIs(kUsernameFormSignature),
        FieldsContain(AllOf(FieldSignatureIs(kUsernameFieldSignature),
                            FieldAutofillTypeIs({FieldType::NOT_USERNAME}))));
    EXPECT_CALL(crowdsourcing_manager(),
                StartUploadRequest(upload_contents_matcher, _, _));
  } else {
    EXPECT_CALL(crowdsourcing_manager(), StartUploadRequest).Times(0);
  }

  // Upload for the password form.
  AutofillUploadContents::SingleUsernameData expected_single_username_data;
  expected_single_username_data.set_username_form_signature(
      kUsernameFormSignature.value());
  expected_single_username_data.set_username_field_signature(
      kUsernameFieldSignature.value());
  expected_single_username_data.set_value_type(
      AutofillUploadContents::USERNAME_LIKE);
  expected_single_username_data.set_prompt_edit(
      AutofillUploadContents::EDITED_NEGATIVE);
  auto upload_contents_matcher = IsPasswordUpload(
      FormSignatureIs(CalculateFormSignature(submitted_form)),
      SingleUsernameDataIs(
          EqualsSingleUsernameDataVector({expected_single_username_data})));
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(upload_contents_matcher, _, _));
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
  test_api(submitted_form).field(0).set_value(u"strongpassword");

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
                                               possible_usernames));
  // Check that a username is not chosen from |possible_username_data|.
  EXPECT_TRUE(form_manager_->GetPendingCredentials().username_value.empty());

  // Check vote uploads.
  testing::InSequence in_sequence;
  // No single username upload for the OTP field.
  EXPECT_CALL(
      crowdsourcing_manager(),
      StartUploadRequest(
          IsPasswordUpload(FormSignatureIs(kSingleUsernameFormSignature)), _,
          _))
      .Times(0);

  // Upload single username data for the password form.
  AutofillUploadContents::SingleUsernameData expected_single_username_data;
  expected_single_username_data.set_value_type(
      AutofillUploadContents::NO_VALUE_TYPE);
  auto upload_contents_matcher = IsPasswordUpload(
      FormSignatureIs(CalculateFormSignature(submitted_form)),
      SingleUsernameDataIs(
          EqualsSingleUsernameDataVector({expected_single_username_data})));
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(upload_contents_matcher, _, _));
  form_manager_->Save();
}

// Tests that no single username votes are sent on an unrelated website.
TEST_P(PasswordFormManagerTest, NoSingleUsernameVotingOnUnrelatedWebsite) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kUsernameFirstFlowFallbackCrowdsourcing,
                            features::
                                kUsernameFirstFlowWithIntermediateValuesVoting},
      /*disabled_features=*/{});
  // Simulate user input in a single username form.
  constexpr char16_t kPossibleUsername[] = u"possible_username";
  PossibleUsernameData single_username_data(
      saved_match_.signon_realm, kSingleUsernameFieldRendererId,
      kPossibleUsername, base::Time::Now(),
      /*driver_id=*/0, /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);

  // Server has a `SINGLE_USERNAME` prediction for the single username form.
  single_username_data.form_predictions = MakeSingleUsernamePredictions(
      kSingleUsernameFormSignature, kSingleUsernameFieldRendererId,
      kSingleUsernameFieldSignature, /*has_single_username_prediction=*/true);

  // Simulate user input in another single text field form on a different
  // website.
  constexpr FormSignature kOtherFormSignature(2000);
  constexpr FieldRendererId kOtherFieldRendererId(200);
  constexpr FieldSignature kOtherFieldSignature(4000);
  PossibleUsernameData unrelated_website_text_field_data(
      /*signon_realm=*/"https://unrelated-website.com", kOtherFieldRendererId,
      u"Message", base::Time::Now(),
      /*driver_id=*/0, /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);

  // Server has no predictions for the unrelated field.
  // `unrelated_website_text_field_data.form_predictions` is required to be set
  // correctly with field signature, form signature, and field renderer id to be
  // able to send single username votes.
  unrelated_website_text_field_data.form_predictions =
      MakeSingleUsernamePredictions(kOtherFormSignature, kOtherFieldRendererId,
                                    kOtherFieldSignature,
                                    /*has_single_username_prediction=*/false);

  base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
      possible_usernames = MakePossibleUsernamesCache(
          {single_username_data, unrelated_website_text_field_data});

  // Simulate submitting a password form.
  FormData submitted_form = observed_form_only_password_fields_;
  CreateFormManager(observed_form_only_password_fields_);
  fetcher_->NotifyFetchCompleted();
  test_api(submitted_form).field(0).set_value(u"strongpassword");
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames));
  EXPECT_EQ(form_manager_->GetPendingCredentials().username_value,
            single_username_data.value);

  form_manager_->SaveSuggestedUsernameValueToVotesUploader();

#if !BUILDFLAG(IS_ANDROID)
  // Expect a strong positive vote on the single username form.
  ExpectIsSingleUsernameUpload(
      kSingleUsernameFormSignature, Field::WEAK, FieldType::SINGLE_USERNAME,
      IsMostRecentSingleUsernameCandidate::kMostRecentCandidate);
  // Expect no vote being sent on an unrelated text field.
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(
                  IsPasswordUpload(FormSignatureIs(kOtherFormSignature)), _, _))
      .Times(0);
#endif

  // Expect upload for the password form. This upload is unrelated to UFF: it
  // is a result of saving a new password on the password form.
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(IsPasswordUpload(FormSignatureIs(
                                     CalculateFormSignature(submitted_form))),
                                 _, _));
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
  test_api(submitted_form).field(0).set_value(u"strongpassword");

  for (FieldType prediction : {SINGLE_USERNAME, NOT_USERNAME}) {
    SCOPED_TRACE(testing::Message("prediction=") << prediction);

    FormPredictions form_predictions;
    form_predictions.fields.emplace_back(
        possible_username_data.renderer_id, autofill::FieldSignature(123),
        prediction,
        /*may_use_prefilled_placeholder=*/false,
        /*is_override=*/false);

    possible_username_data.form_predictions = form_predictions;
    base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>
        possible_usernames =
            MakePossibleUsernamesCache({possible_username_data});

    CreateFormManager(observed_form_only_password_fields_);
    fetcher_->NotifyFetchCompleted();

    ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                                 possible_usernames));

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
  test_api(submitted_form).field(0).set_value(u"strongpassword");

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
                                               possible_usernames));
  EXPECT_EQ(possible_username_with_prediction,
            form_manager_->GetPendingCredentials().username_value);
}

// Tests that boolean representing autocomplete = "username" is taken into
// consideration when offering username in prompt in username first flow.
TEST_P(PasswordFormManagerTest, PossibleUsernameFromAutocomplete) {
  // A single password form is loaded on the page.
  FormData submitted_form = observed_form_only_password_fields_;
  CreateFormManager(submitted_form);
  fetcher_->NotifyFetchCompleted();

  // User types a password value.
  test_api(submitted_form).field(0).set_value(u"strongpassword");

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
                                               possible_usernames));

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
  test_api(submitted_form).field(0).set_value(u"strongpassword");

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
                                               possible_usernames));

  // Check that a username is chosen from |possible_username_data|.
  EXPECT_EQ(possible_username,
            form_manager_->GetPendingCredentials().username_value);
}

// Tests that probable change password submission is detected for a form that
// does not contain a username fields, but contains old and new password fields.
TEST_P(PasswordFormManagerTest, ChangePasswordFormWithoutUsernameSubmitted) {
  // A form with old and new password fields without username.
  FormData submitted_form = observed_form_only_password_fields_;
  test_api(submitted_form).field(0).set_value(u"oldpassword");
  test_api(submitted_form).field(1).set_value(u"newpassword");

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames_));
  EXPECT_TRUE(form_manager_->HasLikelyChangeOrResetFormSubmitted());
}

// Tests that probable change password submission is detected properly for forms
// containing username fields.
TEST_P(PasswordFormManagerTest, ChangePasswordFormWithUsernameSubmitted) {
  // A form with username, current and new password fields.
  FormData submitted_form = observed_form_only_password_fields_;
  FormFieldData username_field;
  username_field.set_name(u"username");
  username_field.set_form_control_type(autofill::FormControlType::kInputText);
  username_field.set_value(u"oldusername");
  username_field.set_renderer_id(autofill::FieldRendererId(2));
  test_api(submitted_form)
      .fields()
      .insert(std::begin(submitted_form.fields()), username_field);

  test_api(submitted_form).field(1).set_value(u"oldpassword");
  test_api(submitted_form).field(2).set_value(u"newpassword");

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames_));
  EXPECT_TRUE(form_manager_->HasLikelyChangeOrResetFormSubmitted());

  // A form with username and new password fields (most likely sign-up).
  test_api(submitted_form).field(1).set_value(u"newpassword");

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames_));
  EXPECT_FALSE(form_manager_->HasLikelyChangeOrResetFormSubmitted());
}

// Tests that probable reset password submission is detected for a form that
// does not contain a username and current password fields.
TEST_P(PasswordFormManagerTest, ResetPasswordFormSubmitted) {
  // A form with new and confirmation password fields without username.
  FormData submitted_form = observed_form_only_password_fields_;
  test_api(submitted_form).field(0).set_value(u"newpassword");
  test_api(submitted_form).field(1).set_value(u"newpassword");

  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames_));
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
                                                &driver_, possible_usernames_));
}

TEST_P(PasswordFormManagerTest, NotMovableToAccountStoreWhenBlocked) {
  const std::string kEmail = "email@gmail.com";
  const std::string kGaiaId = signin::GetTestGaiaIdForEmail(kEmail);

  PasswordForm saved_match(saved_match_);
  saved_match.in_store = PasswordForm::Store::kProfileStore;
  // Moving stored credentials is blocked for |kEmail|.
  saved_match.moving_blocked_for_list.push_back(
      GaiaIdHash::FromGaiaId(kGaiaId));
  SetNonFederatedAndNotifyFetchCompleted({saved_match});

  test_api(submitted_form_)
      .field(kUsernameFieldIndex)
      .set_value(saved_match_.username_value);
  test_api(submitted_form_)
      .field(kPasswordFieldIndex)
      .set_value(saved_match_.password_value);

  form_manager_->Fill();
  EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               possible_usernames_));

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
  SetNonFederatedAndNotifyFetchCompleted({saved_match});

  test_api(submitted_form_)
      .field(kUsernameFieldIndex)
      .set_value(saved_match_.username_value);
  test_api(submitted_form_)
      .field(kPasswordFieldIndex)
      .set_value(saved_match_.password_value);

  form_manager_->Fill();
  EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               possible_usernames_));

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

  form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                   possible_usernames_);

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
      .WillOnce(ReturnRef(submitted_form_.url()));
  form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                   possible_usernames_);

  // Check metrics recorded on the form manager destruction.
  form_manager_.reset();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SubmittedFormFrame2",
      metrics_util::SubmittedFormFrame::IFRAME_WITH_SAME_URL_AS_MAIN_FRAME, 1);
}

TEST_P(PasswordFormManagerTest, ReportSubmittedFormFrameSameSignOnRealmIframe) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(driver_, IsInPrimaryMainFrame).WillRepeatedly(Return(false));

  GURL main_frame_url = GURL(GetSignonRealm(submitted_form_.url()));
  ASSERT_NE(submitted_form_.url(), main_frame_url);
  EXPECT_CALL(client_, GetLastCommittedURL)
      .WillRepeatedly(ReturnRef(main_frame_url));
  form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                   possible_usernames_);

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

  submitted_form_.set_url(GURL("http://facebook.com"));
  GURL main_frame_url = GURL("http://m.facebook.com");
  ASSERT_TRUE(IsPublicSuffixDomainMatch(submitted_form_.url().spec(),
                                        main_frame_url.spec()));
  EXPECT_CALL(client_, GetLastCommittedURL)
      .WillRepeatedly(ReturnRef(main_frame_url));
  form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                   possible_usernames_);

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
  ASSERT_NE(GetSignonRealm(submitted_form_.url()),
            GetSignonRealm(main_frame_url));
  ASSERT_FALSE(IsPublicSuffixDomainMatch(submitted_form_.url().spec(),
                                         main_frame_url.spec()));
  EXPECT_CALL(client_, GetLastCommittedURL)
      .WillRepeatedly(ReturnRef(main_frame_url));
  form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                   possible_usernames_);

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
               observed_form_only_password_fields_.url(), kPossibleUsername,
               kSingleUsernameFormSignature, kSingleUsernameFieldSignature,
               /*is_likely_otp=*/false, UNKNOWN_TYPE);

  // Simulate user input in another single text field form, unrelated to FPF.
  constexpr FormSignature kOtherFormSignature(2000);
  constexpr FieldRendererId kOtherFieldRendererId(200);
  constexpr FieldSignature kOtherFieldSignature(4000);
  AddFieldInfo(/*driver_id=*/0, kOtherFieldRendererId,
               observed_form_only_password_fields_.url(), u"OTP",
               kOtherFormSignature, kOtherFieldSignature,
               /*is_likely_otp=*/false, UNKNOWN_TYPE);

  // Simulate submitting a password form.
  FormData submitted_form = observed_form_only_password_fields_;
  test_api(submitted_form).field(0).set_value(u"strongpassword");
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames_));
  form_manager_->SaveSuggestedUsernameValueToVotesUploader();

  // Simulate the user modifying the username in the prompt.
  form_manager_->OnUpdateUsernameFromPrompt(kPossibleUsername);

  // Expect a strong positive vote on the username form.
  ExpectIsSingleUsernameUpload(kSingleUsernameFormSignature,
                               Field::STRONG_FORGOT_PASSWORD,
                               FieldType::SINGLE_USERNAME_FORGOT_PASSWORD);

  // Expect a strong negative vote on another single text field form, unrelated
  // to FPF.
  ExpectIsSingleUsernameUpload(kOtherFormSignature,
                               Field::STRONG_FORGOT_PASSWORD,
                               FieldType::NOT_USERNAME);

  // Expect upload for the password form. This upload is unrelated to FPF: it
  // is a result of saving a new password on the password form.
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(IsPasswordUpload(FormSignatureIs(
                                     CalculateFormSignature(submitted_form))),
                                 _, _));

  form_manager_->Save();
}

// Tests that when the user accepts the username value in the save prompt
// without editing, a weak positive vote is sent for the form that user has
// typed the saved value into, and a weak negative vote is sent for the form,
// into which the user has typed a different value.
TEST_P(PasswordFormManagerTest, WeakForgotPasswordFormVotes) {
  CreateFormManager(observed_form_only_password_fields_);
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  // Simulate user input in a single text field in a forgot password form.
  AddFieldInfo(/*driver_id=*/0, kSingleUsernameFieldRendererId,
               observed_form_only_password_fields_.url(),
               saved_match_.username_value, kSingleUsernameFormSignature,
               kSingleUsernameFieldSignature,
               /*is_likely_otp=*/false, UNKNOWN_TYPE);

  // Simulate user input in another single text field form, unrelated to FPF.
  constexpr FormSignature kOtherFormSignature(2000);
  constexpr FieldRendererId kOtherFieldRendererId(200);
  constexpr FieldSignature kOtherFieldSignature(4000);
  AddFieldInfo(/*driver_id=*/0, kOtherFieldRendererId,
               observed_form_only_password_fields_.url(), u"OTP",
               kOtherFormSignature, kOtherFieldSignature,
               /*is_likely_otp=*/false, UNKNOWN_TYPE);

  // Simulate submitting a password form. A previously saved username value
  // (`saved_match_.username_value`) is offered as username in the prompt
  FormData submitted_form = observed_form_only_password_fields_;
  test_api(submitted_form).field(0).set_value(u"strongpassword");
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames_));
  form_manager_->SaveSuggestedUsernameValueToVotesUploader();

  // Expect a weak positive vote on the username form.
  ExpectIsSingleUsernameUpload(kSingleUsernameFormSignature,
                               Field::WEAK_FORGOT_PASSWORD,
                               FieldType::SINGLE_USERNAME_FORGOT_PASSWORD);

  // Expect a weak negative vote on another single text field form, unrelated to
  // FPF.
  ExpectIsSingleUsernameUpload(kOtherFormSignature, Field::WEAK_FORGOT_PASSWORD,
                               FieldType::NOT_USERNAME);

  // Expect upload for the password form. This upload is unrelated to FPF: it
  // is a result of saving a new password on the password form.
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(IsPasswordUpload(FormSignatureIs(
                                     CalculateFormSignature(submitted_form))),
                                 _, _));

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
               observed_form_only_password_fields_.url(), kPossibleUsername,
               kSingleUsernameFormSignature, kSingleUsernameFieldSignature,
               /*is_likely_otp=*/false, UNKNOWN_TYPE);

  // Simulate user input in another single text field form, unrelated to FPF.
  constexpr FormSignature kOtherFormSignature(2000);
  constexpr FieldRendererId kOtherFieldRendererId(200);
  constexpr FieldSignature kOtherFieldSignature(4000);
  AddFieldInfo(/*driver_id=*/0, kOtherFieldRendererId,
               observed_form_only_password_fields_.url(), u"OTP",
               kOtherFormSignature, kOtherFieldSignature,
               /*is_likely_otp=*/false, UNKNOWN_TYPE);

  // Simulate submitting a password form.
  FormData submitted_form = observed_form_;
  test_api(submitted_form).field(1).set_value(kPossibleUsername);
  test_api(submitted_form).field(2).set_value(u"strongpassword");
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames_));
  form_manager_->SaveSuggestedUsernameValueToVotesUploader();

  // Expect a weak positive vote on the username form.
  ExpectIsSingleUsernameUpload(kSingleUsernameFormSignature,
                               Field::WEAK_FORGOT_PASSWORD,
                               autofill::SINGLE_USERNAME_FORGOT_PASSWORD);

  // Expect no vote on another single text field form, unrelated to FPF.
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(
                  IsPasswordUpload(FormSignatureIs(kOtherFormSignature)), _, _))
      .Times(0);

  // Expect upload for the password form. This upload is unrelated to FPF: it
  // is a result of saving a new password on the password form.
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(IsPasswordUpload(FormSignatureIs(
                                     CalculateFormSignature(submitted_form))),
                                 _, _));

  form_manager_->Save();
}

// Tests that no vote is sent for the OTP field, unless there is a server
// prediction confirming it's a single username field.
TEST_P(PasswordFormManagerTest, ForgotPasswordFormVotesOnLikelyOTPField) {
  CreateFormManager(observed_form_only_password_fields_);
  fetcher_->NotifyFetchCompleted();

  // Simulate user input in a single text field in a forgot password form,
  // for which PasswordManager's heuristics say it's an OTP field, but server
  // predictions contradict that.
  constexpr char16_t kPossibleUsername[] = u"possible_username";
  AddFieldInfo(/*driver_id=*/0, kSingleUsernameFieldRendererId,
               observed_form_only_password_fields_.url(), kPossibleUsername,
               kSingleUsernameFormSignature, kSingleUsernameFieldSignature,
               /*is_likely_otp=*/true, SINGLE_USERNAME_FORGOT_PASSWORD);

  // Simulate user input in another single text field form, unrelated to FPF,
  // for which PasswordManager's heuristics say it's an OTP field, and server
  // does not contradict that.
  constexpr FormSignature kOtherFormSignature(2000);
  constexpr FieldRendererId kOtherFieldRendererId(200);
  constexpr FieldSignature kOtherFieldSignature(4000);
  AddFieldInfo(/*driver_id=*/0, kOtherFieldRendererId,
               observed_form_only_password_fields_.url(), u"OTP",
               kOtherFormSignature, kOtherFieldSignature,
               /*is_likely_otp=*/true, NOT_USERNAME);

  // Simulate submitting a password form.
  FormData submitted_form = observed_form_only_password_fields_;
  test_api(submitted_form).field(0).set_value(u"strongpassword");
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames_));
  // kPossibleUsername is suggested in the prompt, because the field has a
  // single username prediction.
  form_manager_->SaveSuggestedUsernameValueToVotesUploader();

  ExpectIsSingleUsernameUpload(kSingleUsernameFormSignature,
                               Field::WEAK_FORGOT_PASSWORD,
                               FieldType::SINGLE_USERNAME_FORGOT_PASSWORD);

  // Expect no vote on another single text field form, unrelated to FPF.
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(
                  IsPasswordUpload(FormSignatureIs(kOtherFormSignature)), _, _))
      .Times(0);

  // Expect upload for the password form. This upload is unrelated to FPF: it
  // is a result of saving a new password on the password form.
  EXPECT_CALL(crowdsourcing_manager(),
              StartUploadRequest(IsPasswordUpload(FormSignatureIs(
                                     CalculateFormSignature(submitted_form))),
                                 _, _));

  form_manager_->Save();
}

// Tests that single username info stored in FieldInfoManager is used to
// build pending credentials if it is predicted to be a single username field
// by the server.
TEST_P(PasswordFormManagerTest, ForgotPasswordFormUsernamePopulatedInPrompt) {
  CreateFormManager(observed_form_only_password_fields_);
  fetcher_->NotifyFetchCompleted();

  // Simulate user input in a single text field in a forgot password form.
  constexpr char16_t kPossibleUsername[] = u"possible_username";
  AddFieldInfo(/*driver_id=*/0, kSingleUsernameFieldRendererId,
               observed_form_only_password_fields_.url(), kPossibleUsername,
               kSingleUsernameFormSignature, kSingleUsernameFieldSignature,
               /*is_likely_otp=*/false, SINGLE_USERNAME_FORGOT_PASSWORD);

  // Provisionally save the form on password input.
  FormData submitted_form = observed_form_only_password_fields_;
  test_api(submitted_form).field(0).set_value(u"strong_password");
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames_));

  // Check that single username is used to build pending credentials.
  EXPECT_EQ(kPossibleUsername,
            form_manager_->GetPendingCredentials().username_value);
}

// Tests that single username info stored in FieldInfoManager is not used to
// build pending credentials if it is not predicted to be a single username
// field by the server.
TEST_P(PasswordFormManagerTest,
       ForgotPasswordFormUsernameNotPopulatedInPrompt) {
  CreateFormManager(observed_form_only_password_fields_);
  fetcher_->NotifyFetchCompleted();

  // Simulate user input in a single text field in a forgot password form.
  constexpr char16_t kPossibleUsername[] = u"possible_username";
  AddFieldInfo(/*driver_id=*/0, kSingleUsernameFieldRendererId,
               observed_form_only_password_fields_.url(), kPossibleUsername,
               kSingleUsernameFormSignature, kSingleUsernameFieldSignature,
               /*is_likely_otp=*/false, UNKNOWN_TYPE);

  // Provisionally save the form on password input.
  FormData submitted_form = observed_form_only_password_fields_;
  test_api(submitted_form).field(0).set_value(u"strong_password");
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames_));

  // Check that single username is used to build pending credentials.
  EXPECT_EQ(u"", form_manager_->GetPendingCredentials().username_value);
}

// Tests that PasswordManager does not use server precictions to parse
// localhost forms.
TEST_P(PasswordFormManagerTest, ServerPredictionsIgnoredOnLocalhost) {
  // Observe a single text field form on localhost.
  FormData observed_form;
  const std::string kLocalHostUrl = "http://localhost";
  observed_form.set_url(GURL(kLocalHostUrl));
  observed_form.set_renderer_id(FormRendererId(1));
  FormFieldData field;
  field.set_form_control_type(autofill::FormControlType::kInputText);
  field.set_renderer_id(kSingleUsernameFieldRendererId);
  test_api(observed_form).Append(field);
  CreateFormManager(observed_form);

  // Expect no filling on receiving saved matches, because single username
  // field cannot be filled without server predictions.
  PasswordForm localhost_match = saved_match_;
  localhost_match.url = GURL(kLocalHostUrl);
  EXPECT_CALL(driver_, SetPasswordFillData).Times(0);
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});
  Mock::VerifyAndClearExpectations(&driver_);

  // Receive single username predictions.
  const FormSignature kFormSignature = CalculateFormSignature(observed_form);
  FormPredictions predictions = MakeSingleUsernamePredictions(
      kFormSignature, kSingleUsernameFieldRendererId,
      kSingleUsernameFieldSignature);

  // Expect no filling on receiving predictions.
  EXPECT_CALL(driver_, SetPasswordFillData).Times(0);
  form_manager_->ProcessServerPredictions({{kFormSignature, predictions}});
}

// Tests that crowdsourcing votes are not uploaded for forms on localhost.
TEST_P(PasswordFormManagerTest, NoVotesUploadedForLocalHost) {
  observed_form_.set_url(GURL("http://localhost"));
  CreateFormManager(observed_form_);
  fetcher_->NotifyFetchCompleted();

  submitted_form_.set_url(observed_form_.url());
  form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                   possible_usernames_);
  EXPECT_TRUE(form_manager_->IsNewLogin());

  // Saving a new credential normally triggers uploading a password vote,
  // but it shouldn't in case of the form on localhost.
  EXPECT_CALL(crowdsourcing_manager(), StartUploadRequest).Times(0);
  form_manager_->Save();
}

// Tests that VotesUploader is not created for HTTP auth forms.
TEST_P(PasswordFormManagerTest, NoVotesUploaderForHTTPAuth) {
  PasswordFormDigest form_digest(PasswordForm::Scheme::kBasic,
                                 saved_match_.signon_realm, saved_match_.url);
  auto form_manager = std::make_unique<PasswordFormManager>(
      &client_, form_digest, fetcher_.get(),
      std::make_unique<PasswordSaveManagerImpl>(&client_));
  EXPECT_FALSE(form_manager->votes_uploader());
}

#if BUILDFLAG(IS_ANDROID)
TEST_P(PasswordFormManagerTest,
       ClientShouldShowErrorMessageForAuthErrorResolvable) {
  fetcher_->SetProfileStoreBackendError(PasswordStoreBackendError(
      PasswordStoreBackendErrorType::kAuthErrorResolvable));

  EXPECT_CALL(client_,
              ShowPasswordManagerErrorMessage(
                  password_manager::ErrorMessageFlowType::kFillFlow,
                  PasswordStoreBackendErrorType::kAuthErrorResolvable));
  fetcher_->NotifyFetchCompleted();
}

TEST_P(PasswordFormManagerTest,
       ClientShouldShowErrorMessageForAuthErrorForAccountStore) {
  fetcher_->SetAccountStoreBackendError(PasswordStoreBackendError(
      PasswordStoreBackendErrorType::kAuthErrorResolvable));

  EXPECT_CALL(client_,
              ShowPasswordManagerErrorMessage(
                  password_manager::ErrorMessageFlowType::kFillFlow,
                  PasswordStoreBackendErrorType::kAuthErrorResolvable));
  fetcher_->NotifyFetchCompleted();
}

TEST_P(PasswordFormManagerTest,
       ClientShouldShowErrorMessageForKeyRetrivalError) {
  fetcher_->SetProfileStoreBackendError(PasswordStoreBackendError(
      PasswordStoreBackendErrorType::kKeyRetrievalRequired));

  EXPECT_CALL(client_,
              ShowPasswordManagerErrorMessage(
                  password_manager::ErrorMessageFlowType::kFillFlow,
                  PasswordStoreBackendErrorType::kKeyRetrievalRequired));
  fetcher_->NotifyFetchCompleted();
}

// Tests that the error message is displayed in the case when both account and
// profile store are requested and the result is the following:
// - account store replies with an authentication error,
// - profile store replies with another backend error.
TEST_P(PasswordFormManagerTest,
       ClientShouldShowErrorMessageWhenBothStoresHaveDifferentErrors) {
  fetcher_->SetAccountStoreBackendError(PasswordStoreBackendError(
      PasswordStoreBackendErrorType::kAuthErrorResolvable));
  fetcher_->SetProfileStoreBackendError(
      PasswordStoreBackendError(PasswordStoreBackendErrorType::kUncategorized));

  EXPECT_CALL(client_,
              ShowPasswordManagerErrorMessage(
                  password_manager::ErrorMessageFlowType::kFillFlow,
                  PasswordStoreBackendErrorType::kAuthErrorResolvable));
  fetcher_->NotifyFetchCompleted();
}

TEST_P(PasswordFormManagerTest,
       ClientShouldShowErrorMessageForAuthErrorUnresolvable) {
  fetcher_->SetProfileStoreBackendError(PasswordStoreBackendError(
      PasswordStoreBackendErrorType::kAuthErrorUnresolvable));

  EXPECT_CALL(client_,
              ShowPasswordManagerErrorMessage(
                  password_manager::ErrorMessageFlowType::kFillFlow,
                  PasswordStoreBackendErrorType::kAuthErrorUnresolvable));
  fetcher_->NotifyFetchCompleted();
}

TEST_P(PasswordFormManagerTest,
       ClientShouldNotShowErrorMessageWhenThereIsNoError) {
  fetcher_->SetProfileStoreBackendError(std::nullopt);

  EXPECT_CALL(client_, ShowPasswordManagerErrorMessage).Times(0);
  fetcher_->NotifyFetchCompleted();
}

TEST_P(PasswordFormManagerTest,
       ClientShouldNotShowErrorMessageWhenErrorIsNotAuthError) {
  fetcher_->SetProfileStoreBackendError(
      PasswordStoreBackendError(PasswordStoreBackendErrorType::kUncategorized));

  EXPECT_CALL(client_, ShowPasswordManagerErrorMessage).Times(0);
  fetcher_->NotifyFetchCompleted();
}

TEST_P(PasswordFormManagerTest, ClientShouldNotShowErrorMessageWhenUnenrolled) {
  client_.GetPrefs()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);
  fetcher_->SetProfileStoreBackendError(PasswordStoreBackendError(
      PasswordStoreBackendErrorType::kAuthErrorResolvable));

  EXPECT_CALL(client_, ShowPasswordManagerErrorMessage).Times(0);
  fetcher_->NotifyFetchCompleted();
}
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST_P(PasswordFormManagerTest, ClientShouldShowKeychainErrorMessage) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kRestartToGainAccessToKeychain);

  fetcher_->SetProfileStoreBackendError(
      PasswordStoreBackendError(PasswordStoreBackendErrorType::kKeychainError));

  EXPECT_CALL(client_, NotifyKeychainError);
  fetcher_->NotifyFetchCompleted();
}

TEST_P(PasswordFormManagerTest, ClientShouldNotShowKeychainErrorMessage) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kRestartToGainAccessToKeychain);

  fetcher_->SetProfileStoreBackendError(std::nullopt);

  EXPECT_CALL(client_, NotifyKeychainError).Times(0);
  fetcher_->NotifyFetchCompleted();

  // If the bubble shouldn't be shown, the counter should be reset.
  EXPECT_EQ(client_.GetPrefs()->GetInteger(
                password_manager::prefs::kRelaunchChromeBubbleDismissedCounter),
            0);
}

// Expects banned field to be sent to the `PasswordManagerDriver` if server
// finds credit card field.
TEST_P(PasswordFormManagerTest, SetCreditCardFieldsAsBanned) {
  FormFieldData field;
  field.set_name(u"credit_card_field");
  field.set_id_attribute(field.name());
  field.set_name_attribute(field.name());
  field.set_form_control_type(autofill::FormControlType::kInputPassword);
  field.set_renderer_id(autofill::FieldRendererId(6));
  test_api(observed_form_).fields().push_back(field);

  CreateFormManager(observed_form_);
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  // Server prediction marks element on index three as a credit card field.
  std::map<FormSignature, FormPredictions> predictions = CreatePredictions(
      observed_form_, {std::make_pair(3, autofill::CREDIT_CARD_NAME_FULL)});

  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, SetPasswordFillData).WillOnce(SaveArg<0>(&fill_data));
  form_manager_->ProcessServerPredictions(predictions);

  // Expect credit card field to be sent to the `PasswordManagerDriver` as a
  // filling suggestion banned field.
  EXPECT_THAT(fill_data.suggestion_banned_fields,
              ElementsAre(field.renderer_id()));
}

#endif

INSTANTIATE_TEST_SUITE_P(All, PasswordFormManagerTest, testing::Bool());

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
  MOCK_METHOD(PasswordForm::Store,
              GetPasswordStoreForSaving,
              (const PasswordForm& password_form),
              (const override));
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
  EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               possible_usernames_));
}

// Tests that when credentials are saved, the PasswordSaveManager is called
// accordingly.
TEST_F(PasswordFormManagerTestWithMockedSaver, SaveCredentials) {
  fetcher_->NotifyFetchCompleted();
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  FormData submitted_form = observed_form_;
  std::u16string new_username = saved_match_.username_value + u"1";
  std::u16string new_password = saved_match_.password_value + u"1";
  test_api(submitted_form).field(kUsernameFieldIndex).set_value(new_username);
  test_api(submitted_form).field(kPasswordFieldIndex).set_value(new_password);
  EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames_));
  PasswordForm updated_form;
  EXPECT_CALL(*mock_password_save_manager(),
              Save(FormDataPointeeEqualTo(observed_form_), _))
      .WillOnce(SaveArg<1>(&updated_form));
  EXPECT_CALL(client_, UpdateFormManagers());
  form_manager_->Save();
  std::string expected_signon_realm =
      submitted_form.url().DeprecatedGetOriginAsURL().spec();
  EXPECT_EQ(submitted_form.url(), updated_form.url);
  EXPECT_EQ(expected_signon_realm, updated_form.signon_realm);
  EXPECT_EQ(new_username, updated_form.username_value);
  EXPECT_EQ(new_password, updated_form.password_value);
  EXPECT_EQ(submitted_form.fields()[kUsernameFieldIndex].name(),
            updated_form.username_element);
  EXPECT_EQ(submitted_form.fields()[kPasswordFieldIndex].name(),
            updated_form.password_element);
  // Check UKM metrics.
  ResetFormManager();
  ExpectedGenerationUKM expected_metrics = {
      {} /* shown manually */,
      0 /* password generated */,
      {} /* generated password is not modified */};
  CheckPasswordGenerationUKM(test_ukm_recorder, expected_metrics);
}

TEST_F(PasswordFormManagerTestWithMockedSaver,
       SaveStartedAfterFormFetcherIsReady) {
  FormData submitted_form = observed_form_;
  std::u16string new_username = saved_match_.username_value + u"1";
  std::u16string new_password = saved_match_.password_value + u"1";
  test_api(submitted_form).field(kUsernameFieldIndex).set_value(new_username);
  test_api(submitted_form).field(kPasswordFieldIndex).set_value(new_password);
  EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames_));

  fetcher_->Fetch();
  form_manager_->Save();

  PasswordForm saved_form;
  EXPECT_CALL(*mock_password_save_manager(),
              Save(FormDataPointeeEqualTo(observed_form_),
                   FormHasPassword(new_password)))
      .WillOnce(SaveArg<1>(&saved_form));
  EXPECT_CALL(client_, UpdateFormManagers());
  fetcher_->NotifyFetchCompleted();
  EXPECT_THAT(*form_manager_->GetSubmittedForm(), FormHasUniqueKey(saved_form));
}

TEST_F(PasswordFormManagerTestWithMockedSaver, UpdateUsernameEmptyStore) {
  fetcher_->NotifyFetchCompleted();
  form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                   possible_usernames_);
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
  test_api(submitted_form_).field(0).set_value(user_chosen_username);
  test_api(submitted_form_).field(1).set_value(automatically_chosen_username);
  EXPECT_CALL(
      *mock_password_save_manager(),
      CreatePendingCredentials(
          FormHasUsernameValue(automatically_chosen_username), _, _, _, _));
  form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                   possible_usernames_);
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
  form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                   possible_usernames_);
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
  form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                   possible_usernames_);
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
  test_api(form).field(0).set_value(password);
  test_api(form).field(1).set_value(pin);
  // Check that a second password field is chosen for saving.
  EXPECT_CALL(*mock_password_save_manager(),
              CreatePendingCredentials(FormHasPassword(pin), _, _, _, _));
  form_manager_->ProvisionallySave(form, &driver_, possible_usernames_);
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
  SetNonFederatedAndNotifyFetchCompleted({saved_match});

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
                  form_data.fields()[kPasswordFieldIndex].value())));
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
  EXPECT_EQ(submitted_form_.fields()[kUsernameFieldIndex].value(),
            updated_form.username_value);
  EXPECT_TRUE(form_manager_->HasGeneratedPassword());
  // Check that the generated password is saved.
  EXPECT_CALL(client_, UpdateFormManagers());
  EXPECT_CALL(*mock_password_save_manager(),
              CreatePendingCredentials(_, _, _, _, _));
  EXPECT_TRUE(form_manager_->ProvisionallySave(submitted_form_, &driver_,
                                               possible_usernames_));
  EXPECT_CALL(*mock_password_save_manager(),
              Save(FormDataPointeeEqualTo(submitted_form_), _))
      .WillOnce(SaveArg<1>(&updated_form));
  form_manager_->Save();
  EXPECT_EQ(submitted_form_.fields()[kUsernameFieldIndex].value(),
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
  form.form_data.set_url(
      GURL("https://accounts.google.com/a/ServiceLoginAuth"));

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
      std::make_optional(2u) /* shown manually */,
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
    std::vector<PasswordForm> saved_matches;
    if (html_credentials_saved) {
      saved_matches.push_back(saved_match_);
    }
    fetcher_->SetNonFederated(saved_matches, {});
    fetcher_->NotifyFetchCompleted();
    std::u16string username = u"user1";
    std::u16string password = u"pass1";
    http_auth_form.username_value = username;
    http_auth_form.password_value = password;
    // Check that submitted credentials are forwarded to the password save
    // manager.
    EXPECT_CALL(*mock_password_save_manager(),
                CreatePendingCredentials(http_auth_form, _, _, true, _));
    ASSERT_TRUE(form_manager_->ProvisionallySaveHttpAuthForm(http_auth_form));
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
  SetNonFederatedAndNotifyFetchCompleted({http_auth_form});
  EXPECT_CALL(*mock_password_save_manager(),
              CreatePendingCredentials(http_auth_form, _, _, true, _));
  ASSERT_TRUE(form_manager_->ProvisionallySaveHttpAuthForm(http_auth_form));
  // Check that the password save manager is invoked.
  EXPECT_CALL(*mock_password_save_manager(), Save(_, http_auth_form));
  form_manager_->Save();
}

// Tests that username is taken during username first flow.
TEST_F(PasswordFormManagerTestWithMockedSaver, UsernameFirstFlow) {
  CreateFormManager(observed_form_only_password_fields_);
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

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
  test_api(submitted_form).field(0).set_value(u"strongpassword");
  // Check that a username is chosen from |possible_username_data|.
  EXPECT_CALL(*mock_password_save_manager(),
              CreatePendingCredentials(FormHasUsernameValue(possible_username),
                                       _, _, _, _));
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames));
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
  test_api(submitted_form).field(0).set_value(u"strongpassword");
  PasswordForm parsed_submitted_form;
  EXPECT_CALL(*mock_password_save_manager(), CreatePendingCredentials)
      .WillOnce(SaveArg<0>(&parsed_submitted_form));
  ASSERT_TRUE(form_manager_->ProvisionallySave(submitted_form, &driver_,
                                               possible_usernames));
  EXPECT_TRUE(parsed_submitted_form.username_value.empty());
}
}  // namespace

class PasswordFormManagerWebAuthnCredentialsTest : public testing::Test {
 protected:
  void SetUp() override {
    PasswordFormManager::set_wait_for_server_predictions_for_filling(false);
#if BUILDFLAG(IS_ANDROID)
    webauthn::WebAuthnCredManDelegate::override_cred_man_support_for_testing(
        webauthn::CredManSupport::DISABLED);
#endif
    auto password_save_manager = std::make_unique<PasswordSaveManagerImpl>(
        /*profile_form_saver=*/std::make_unique<NiceMock<MockFormSaver>>(),
        /*account_form_saver=*/nullptr);
    fetcher_.Fetch();
    form_manager_ = std::make_unique<PasswordFormManager>(
        &client_, driver_.AsWeakPtr(), FormData(), &fetcher_,
        std::move(password_save_manager), nullptr);

    ON_CALL(client_, GetWebAuthnCredentialsDelegateForDriver)
        .WillByDefault(Return(&webauthn_credentials_delegate_));
    ON_CALL(webauthn_credentials_delegate_, GetPasskeys)
        .WillByDefault(ReturnRef(kNullopt));
  }

  MockPasswordManagerClient& client() { return client_; }

  MockWebAuthnCredentialsDelegate& webauthn_credentials_delegate() {
    return webauthn_credentials_delegate_;
  }

  PasswordFormManager& form_manager() { return *form_manager_.get(); }

 private:
  MockPasswordManagerClient client_;
  MockPasswordManagerDriver driver_;
  MockWebAuthnCredentialsDelegate webauthn_credentials_delegate_;
  FakeFormFetcher fetcher_;

  std::unique_ptr<PasswordFormManager> form_manager_;
};

TEST_F(PasswordFormManagerWebAuthnCredentialsTest, NoDelegate) {
  ON_CALL(client(), GetWebAuthnCredentialsDelegateForDriver)
      .WillByDefault(Return(nullptr));

  EXPECT_FALSE(form_manager().WebAuthnCredentialsAvailable());
}

TEST_F(PasswordFormManagerWebAuthnCredentialsTest, NoConditionalRequest) {
  ON_CALL(client(), GetWebAuthnCredentialsDelegateForDriver)
      .WillByDefault(Return(nullptr));

  EXPECT_FALSE(form_manager().WebAuthnCredentialsAvailable());
}

TEST_F(PasswordFormManagerWebAuthnCredentialsTest,
       OnePasskeysFromConditionalRequest) {
  const std::optional<std::vector<PasskeyCredential>> kOnePasskey =
      std::vector<PasskeyCredential>{kPasskey};
  ON_CALL(webauthn_credentials_delegate(), GetPasskeys)
      .WillByDefault(ReturnRef(kOnePasskey));

  EXPECT_TRUE(form_manager().WebAuthnCredentialsAvailable());
}

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
TEST_F(
    PasswordFormManagerWebAuthnCredentialsTest,
    NoPasskeysFromConditionalRequest_WhenUseAnotherDeviceInContextMenu_ThenNoWebauthnCredentials) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {features::kWebAuthnUsePasskeyFromAnotherDeviceInContextMenu,
       features::kPasswordManualFallbackAvailable},
      {});
  ON_CALL(webauthn_credentials_delegate(), GetPasskeys)
      .WillByDefault(ReturnRef(kNoPasskeys));

  EXPECT_FALSE(form_manager().WebAuthnCredentialsAvailable());
}

TEST_F(
    PasswordFormManagerWebAuthnCredentialsTest,
    NoPasskeysFromConditionalRequest_WhenUseAnotherDeviceInAutofillPopup_ThenWebauthnCredentials) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {features::kPasswordManualFallbackAvailable},
      {features::kWebAuthnUsePasskeyFromAnotherDeviceInContextMenu});
  ON_CALL(webauthn_credentials_delegate(), GetPasskeys)
      .WillByDefault(ReturnRef(kNoPasskeys));

  EXPECT_TRUE(form_manager().WebAuthnCredentialsAvailable());
}
#else

TEST_F(PasswordFormManagerWebAuthnCredentialsTest,
       NoPasskeysFromConditionalRequest_ThenWebauthnCredentials) {
  ON_CALL(webauthn_credentials_delegate(), GetPasskeys)
      .WillByDefault(ReturnRef(kNoPasskeys));

  EXPECT_TRUE(form_manager().WebAuthnCredentialsAvailable());
}
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

}  // namespace password_manager
