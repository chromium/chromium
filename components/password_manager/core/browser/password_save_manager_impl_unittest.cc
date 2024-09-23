// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_save_manager_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/crowdsourcing/mock_autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/test_utils/vote_uploads_test_matchers.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/vote_uploads_test_matchers.h"
#include "components/password_manager/core/browser/votes_uploader.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using ::autofill::AutofillUploadContents;
using ::autofill::FieldType;
using ::autofill::FormData;
using ::autofill::FormFieldData;
using ::autofill::FormStructure;
using ::autofill::PasswordFormFillData;
using ::autofill::mojom::SubmissionIndicatorEvent;
using ::autofill::upload_contents_matchers::FieldAutofillTypeIs;
using ::autofill::upload_contents_matchers::FieldsContain;
using ::autofill::upload_contents_matchers::FieldSignatureIs;
using ::base::TestMockTimeTaskRunner;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::UnorderedElementsAre;
using upload_contents_matchers::IsPasswordUpload;

// Indices of username and password fields in the observed form.
constexpr int kUsernameFieldIndex = 1;
constexpr int kPasswordFieldIndex = 2;

MATCHER_P(FormHasUniqueKey, key, "") {
  return ArePasswordFormUniqueKeysEqual(arg, key);
}

MATCHER_P2(MatchesUsernameAndPassword, username, password, "") {
  return arg.username_value == username && arg.password_value == password;
}

MATCHER_P(MatchesUpdatedForm, form, "") {
  return ArePasswordFormUniqueKeysEqual(arg, form) &&
         arg.date_last_used >= form.date_last_used;
}

// Creates a matcher for an `autofill::AutofillUploadContents::Field` that
// checks that the field's signature matches that of `field` and its predicted
// type is `type`.
auto UploadFieldIs(const FormFieldData& field, FieldType type) {
  return AllOf(
      FieldSignatureIs(autofill::CalculateFieldSignatureForField(field)),
      FieldAutofillTypeIs({type}));
}

const auto kTrigger = metrics_util::MoveToAccountStoreTrigger::
    kSuccessfulLoginWithProfileStorePassword;

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
  EXPECT_EQ(expected.all_alternative_usernames,
            actual.all_alternative_usernames);
  EXPECT_TRUE(
      autofill::FormData::DeepEqual(expected.form_data, actual.form_data));
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

class MockFormSaver : public StubFormSaver {
 public:
  // FormSaver:
  MOCK_METHOD(PasswordForm, Blocklist, (PasswordFormDigest), (override));
  MOCK_METHOD(void, Unblocklist, (const PasswordFormDigest&), (override));
  MOCK_METHOD(
      void,
      Save,
      (PasswordForm pending,
       (const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>&)
           matches,
       const std::u16string& old_password),
      (override));
  MOCK_METHOD(
      void,
      Update,
      (PasswordForm pending,
       (const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>&)
           matches,
       const std::u16string& old_password),
      (override));
  MOCK_METHOD(
      void,
      UpdateReplace,
      (PasswordForm pending,
       (const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>&)
           matches,
       const std::u16string& old_password,
       const PasswordForm& old_unique_key),
      (override));
  MOCK_METHOD(void, Remove, (const PasswordForm&), (override));

  std::unique_ptr<FormSaver> Clone() override {
    return std::make_unique<MockFormSaver>();
  }
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
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
};

}  // namespace

class PasswordSaveManagerImplTestBase : public testing::Test {
 public:
  explicit PasswordSaveManagerImplTestBase(bool enable_account_store)
      : votes_uploader_(&client_, false /* is_possible_change_password_form */),
        task_runner_(new TestMockTimeTaskRunner) {
    GURL origin = GURL("https://accounts.google.com/a/ServiceLoginAuth");
    GURL action = GURL("https://accounts.google.com/a/ServiceLogin");
    GURL psl_origin = GURL("https://myaccounts.google.com/a/ServiceLoginAuth");
    GURL psl_action = GURL("https://myaccounts.google.com/a/ServiceLogin");

    observed_form_.set_url(origin);
    observed_form_.set_action(action);
    observed_form_.set_name(u"sign-in");
    observed_form_.set_renderer_id(autofill::FormRendererId(1));

    observed_form_only_password_fields_ = observed_form_;

    FormFieldData field;
    field.set_name(u"firstname");
    field.set_id_attribute(field.name());
    field.set_name_attribute(field.name());
    field.set_form_control_type(autofill::FormControlType::kInputText);
    field.set_renderer_id(autofill::FieldRendererId(1));
    test_api(observed_form_).Append(field);

    field.set_name(u"username");
    field.set_id_attribute(field.name());
    field.set_name_attribute(field.name());
    field.set_form_control_type(autofill::FormControlType::kInputText);
    field.set_renderer_id(autofill::FieldRendererId(2));
    test_api(observed_form_).Append(field);

    field.set_name(u"password");
    field.set_id_attribute(field.name());
    field.set_name_attribute(field.name());
    field.set_form_control_type(autofill::FormControlType::kInputPassword);
    field.set_renderer_id(autofill::FieldRendererId(3));
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
    psl_saved_match_.signon_realm = "https://myaccounts.google.com/";
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

    fetcher_ = std::make_unique<FakeFormFetcher>();
    fetcher_->Fetch();

    metrics_recorder_ = base::MakeRefCounted<PasswordFormMetricsRecorder>(
        client_.IsCommittedMainFrameSecure(), client_.GetUkmSourceId(),
        /*pref_service=*/nullptr);

    auto mock_profile_form_saver = std::make_unique<NiceMock<MockFormSaver>>();
    mock_profile_form_saver_ = mock_profile_form_saver.get();

    std::unique_ptr<NiceMock<MockFormSaver>> mock_account_form_saver;
    if (enable_account_store) {
      mock_account_form_saver = std::make_unique<NiceMock<MockFormSaver>>();
      mock_account_form_saver_ = mock_account_form_saver.get();
    }

    password_save_manager_impl_ = std::make_unique<PasswordSaveManagerImpl>(
        /*profile_form_saver=*/std::move(mock_profile_form_saver),
        /*account_form_saver=*/std::move(mock_account_form_saver));

    password_save_manager_impl_->Init(&client_, fetcher_.get(),
                                      metrics_recorder_, &votes_uploader_);

    ON_CALL(client_, GetAutofillCrowdsourcingManager())
        .WillByDefault(Return(&mock_autofill_crowdsourcing_manager_));
    ON_CALL(mock_autofill_crowdsourcing_manager_, StartUploadRequest)
        .WillByDefault(Return(true));
    ON_CALL(*client_.GetPasswordFeatureManager(), GetDefaultPasswordStore)
        .WillByDefault(Return(PasswordForm::Store::kProfileStore));
  }
  PasswordSaveManagerImplTestBase(const PasswordSaveManagerImplTestBase&) =
      delete;
  PasswordSaveManagerImplTestBase& operator=(
      const PasswordSaveManagerImplTestBase&) = delete;

  PasswordForm Parse(const FormData& form_data) {
    return *FormDataParser().Parse(form_data, FormDataParser::Mode::kSaving,
                                   /*stored_usernames=*/{});
  }

  void DestroySaveManagerAndMetricsRecorder() {
    mock_account_form_saver_ = nullptr;
    mock_profile_form_saver_ = nullptr;
    password_save_manager_impl_.reset();
    metrics_recorder_.reset();
  }

  MockPasswordManagerClient* client() { return &client_; }

  PasswordSaveManagerImpl* password_save_manager_impl() {
    return password_save_manager_impl_.get();
  }

  MockFormSaver* mock_account_form_saver() { return mock_account_form_saver_; }
  MockFormSaver* mock_profile_form_saver() { return mock_profile_form_saver_; }

  FakeFormFetcher* fetcher() { return fetcher_.get(); }

  PasswordFormMetricsRecorder* metrics_recorder() {
    return metrics_recorder_.get();
  }

  autofill::MockAutofillCrowdsourcingManager*
  mock_autofill_crowdsourcing_manager() {
    return &mock_autofill_crowdsourcing_manager_;
  }

  TestMockTimeTaskRunner* task_runner() { return task_runner_.get(); }

  void SetNonFederatedAndNotifyFetchCompleted(
      std::vector<PasswordForm> non_federated) {
    fetcher()->SetNonFederated(non_federated);
    fetcher()->SetBestMatches(non_federated);
    fetcher()->NotifyFetchCompleted();
  }

  void SetFederatedAndNotifyFetchCompleted(
      const std::vector<PasswordForm>& federated) {
    fetcher_->set_federated(federated);
    fetcher_->NotifyFetchCompleted();
  }

  void SetAccountStoreEnabled(bool is_enabled) {
    ON_CALL(*client()->GetPasswordFeatureManager(),
            IsOptedInForAccountStorage())
        .WillByDefault(Return(is_enabled));
    ON_CALL(*client()->GetPasswordFeatureManager(),
            ComputePasswordAccountStorageUsageLevel)
        .WillByDefault(Return(
            is_enabled ? features_util::PasswordAccountStorageUsageLevel::
                             kUsingAccountStorage
                       : features_util::PasswordAccountStorageUsageLevel::
                             kNotUsingAccountStorage));
  }

  void SetDefaultPasswordStore(const PasswordForm::Store& store) {
    ON_CALL(*client()->GetPasswordFeatureManager(), GetDefaultPasswordStore())
        .WillByDefault(Return(store));
  }

  PasswordForm CreateSavedFederated() {
    PasswordForm federated;
    federated.url = GURL("https://example.in/login");
    federated.signon_realm = "federation://example.in/google.com";
    federated.type = PasswordForm::Type::kApi;
    federated.federation_origin =
        url::SchemeHostPort(GURL("https://google.com/"));
    federated.username_value = u"federated_username";
    return federated;
  }

  VotesUploader* votes_uploader() { return &votes_uploader_; }

  FormData observed_form_;
  FormData submitted_form_;
  FormData observed_form_only_password_fields_;
  PasswordForm saved_match_;
  PasswordForm psl_saved_match_;
  PasswordForm parsed_observed_form_;
  PasswordForm parsed_submitted_form_;

 private:
  NiceMock<MockPasswordManagerClient> client_;
  VotesUploader votes_uploader_;
  scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder_;
  scoped_refptr<TestMockTimeTaskRunner> task_runner_;

  // Define |fetcher_| before |password_save_manager_impl_|, because the former
  // needs to outlive the latter.
  std::unique_ptr<FakeFormFetcher> fetcher_;
  std::unique_ptr<PasswordSaveManagerImpl> password_save_manager_impl_;
  raw_ptr<NiceMock<MockFormSaver>> mock_account_form_saver_ = nullptr;
  raw_ptr<NiceMock<MockFormSaver>> mock_profile_form_saver_ = nullptr;
  NiceMock<autofill::MockAutofillCrowdsourcingManager>
      mock_autofill_crowdsourcing_manager_{/*client=*/nullptr};
};

// The boolean test parameter maps to the `enable_account_store` constructor
// parameter of the base class.
class PasswordSaveManagerImplTest : public PasswordSaveManagerImplTestBase,
                                    public testing::WithParamInterface<bool> {
 public:
  PasswordSaveManagerImplTest()
      : PasswordSaveManagerImplTestBase(/*enable_account_store=*/GetParam()) {}
};

TEST_P(PasswordSaveManagerImplTest, Blocklist) {
  PasswordFormDigest form_digest(PasswordForm::Scheme::kDigest,
                                 "www.example.com", GURL("www.abc.com"));
  EXPECT_CALL(*mock_profile_form_saver(), Blocklist(form_digest));
  password_save_manager_impl()->Blocklist(form_digest);
}

TEST_P(PasswordSaveManagerImplTest, Unblocklist) {
  PasswordFormDigest form_digest(PasswordForm::Scheme::kDigest,
                                 "www.example.com", GURL("www.abc.com"));
  EXPECT_CALL(*mock_profile_form_saver(), Unblocklist(form_digest));
  password_save_manager_impl()->Unblocklist(form_digest);
}

// Tests creating pending credentials when the password store is empty.
TEST_P(PasswordSaveManagerImplTest, CreatePendingCredentialsEmptyStore) {
  fetcher()->NotifyFetchCompleted();

  const base::Time kNow = base::Time::Now();
  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  const PasswordForm& pending_credentials =
      password_save_manager_impl()->GetPendingCredentials();
  CheckPendingCredentials(parsed_submitted_form_, pending_credentials);
  EXPECT_GE(pending_credentials.date_last_used, kNow);
}

// Tests creating pending credentials when new credentials are submitted and the
// store has another credentials saved.
TEST_P(PasswordSaveManagerImplTest, CreatePendingCredentialsNewCredentials) {
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});
  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  PasswordForm expected_credentials = parsed_submitted_form_;
  expected_credentials.all_alternative_usernames.emplace_back(
      AlternativeElement::Value(saved_match_.username_value));
  CheckPendingCredentials(
      expected_credentials,
      password_save_manager_impl()->GetPendingCredentials());
}

// Tests that when submitted credentials are equal to already saved one then
// pending credentials equal to saved match.
TEST_P(PasswordSaveManagerImplTest, CreatePendingCredentialsAlreadySaved) {
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  test_api(submitted_form_)
      .field(kUsernameFieldIndex)
      .set_value(saved_match_.username_value);
  test_api(submitted_form_)
      .field(kPasswordFieldIndex)
      .set_value(saved_match_.password_value);

  password_save_manager_impl()->CreatePendingCredentials(
      Parse(submitted_form_), &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  CheckPendingCredentials(
      saved_match_, password_save_manager_impl()->GetPendingCredentials());
}

// Tests that when submitted credentials are equal to already saved PSL
// credentials.
TEST_P(PasswordSaveManagerImplTest, CreatePendingCredentialsPSLMatchSaved) {
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

  password_save_manager_impl()->CreatePendingCredentials(
      Parse(submitted_form_), &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  CheckPendingCredentials(
      expected, password_save_manager_impl()->GetPendingCredentials());
}

// Tests creating pending credentials when new credentials are different only in
// password with already saved one.
TEST_P(PasswordSaveManagerImplTest, CreatePendingCredentialsPasswordOverriden) {
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  PasswordForm expected = saved_match_;
  expected.password_value += u"1";

  test_api(submitted_form_)
      .field(kUsernameFieldIndex)
      .set_value(saved_match_.username_value);
  test_api(submitted_form_)
      .field(kPasswordFieldIndex)
      .set_value(expected.password_value);

  password_save_manager_impl()->CreatePendingCredentials(
      Parse(submitted_form_), &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  CheckPendingCredentials(
      expected, password_save_manager_impl()->GetPendingCredentials());
}

// Tests that when submitted credentials are equal to already saved one then
// pending credentials equal to saved match.
TEST_P(PasswordSaveManagerImplTest, CreatePendingCredentialsUpdate) {
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  FormData submitted_form = observed_form_only_password_fields_;
  test_api(submitted_form).field(0).set_value(u"strongpassword");
  test_api(submitted_form).field(1).set_value(u"verystrongpassword");

  PasswordForm expected = saved_match_;
  expected.password_value = u"verystrongpassword";

  password_save_manager_impl()->CreatePendingCredentials(
      Parse(submitted_form), &observed_form_, submitted_form,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  CheckPendingCredentials(
      expected, password_save_manager_impl()->GetPendingCredentials());
}

// Tests creating pending credentials when a change password form is submitted
// and there are multiple saved forms.
TEST_P(PasswordSaveManagerImplTest,
       CreatePendingCredentialsUpdateMultipleSaved) {
  PasswordForm another_saved_match = saved_match_;
  another_saved_match.username_value += u"1";
  SetNonFederatedAndNotifyFetchCompleted({saved_match_, another_saved_match});

  FormData submitted_form = observed_form_only_password_fields_;
  test_api(submitted_form).field(0).set_value(u"strongpassword");
  test_api(submitted_form).field(1).set_value(u"verystrongpassword");

  PasswordForm expected = saved_match_;
  expected.password_value = u"verystrongpassword";
  expected.all_alternative_usernames.emplace_back(
      AlternativeElement::Value(another_saved_match.username_value));

  password_save_manager_impl()->CreatePendingCredentials(
      Parse(submitted_form), &observed_form_, submitted_form,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  CheckPendingCredentials(
      expected, password_save_manager_impl()->GetPendingCredentials());
}

// Tests creating pending credentials when there are multiple saved credentials,
// ensuring that there are no duplicates in the alternative usernames vector.
TEST_P(PasswordSaveManagerImplTest,
       CreatePendingCredentialsMultipleSavedNoDuplicatedAlternatives) {
  PasswordForm another_saved_match = saved_match_;
  another_saved_match.username_value = u"last_name";
  SetNonFederatedAndNotifyFetchCompleted({saved_match_, another_saved_match});

  // Create submitted form, that has one of the previously saved usernames in
  // it.
  FormData submitted_form = observed_form_;
  test_api(submitted_form)
      .field(0)
      .set_value(another_saved_match.username_value);
  test_api(submitted_form).field(1).set_value(u"new_username");
  test_api(submitted_form).field(2).set_value(u"verystrongpassword");

  PasswordForm parsed_form = Parse(submitted_form);
  password_save_manager_impl()->CreatePendingCredentials(
      parsed_form, &observed_form_, submitted_form,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  // Only the match that wasn't present in the submitted form should be added.
  PasswordForm expected = parsed_form;
  expected.all_alternative_usernames.emplace_back(
      AlternativeElement::Value(saved_match_.username_value));

  CheckPendingCredentials(
      expected, password_save_manager_impl()->GetPendingCredentials());
}

// Tests creating pending credentials when the password field has an empty name.
TEST_P(PasswordSaveManagerImplTest, CreatePendingCredentialsEmptyName) {
  fetcher()->NotifyFetchCompleted();

  FormData anonymous_signup = observed_form_;
  // There is an anonymous password field and set it as the new password field.
  test_api(anonymous_signup).field(2).set_name({});
  test_api(anonymous_signup).field(2).set_value(u"a password");
  test_api(anonymous_signup)
      .field(2)
      .set_autocomplete_attribute("new-password");

  password_save_manager_impl()->CreatePendingCredentials(
      Parse(anonymous_signup), &observed_form_, anonymous_signup,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_EQ(
      u"a password",
      password_save_manager_impl()->GetPendingCredentials().password_value);
}

// Tests creating pending credentials when the password store is empty.
TEST_P(PasswordSaveManagerImplTest, ResetPendingCredentials) {
  fetcher()->NotifyFetchCompleted();

  password_save_manager_impl()->CreatePendingCredentials(
      Parse(submitted_form_), &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  password_save_manager_impl()->ResetPendingCredentials();

  // Check that save manager is in None state.
  EXPECT_FALSE(password_save_manager_impl()->IsNewLogin());
  EXPECT_FALSE(password_save_manager_impl()->IsPasswordUpdate());
  EXPECT_FALSE(password_save_manager_impl()->HasGeneratedPassword());
}

// Tests that when credentials with a new username (i.e. not saved yet) is
// successfully submitted, then they are saved correctly.
TEST_P(PasswordSaveManagerImplTest, SaveNewCredentials) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner());
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  FormData submitted_form = observed_form_;
  std::u16string new_username = saved_match_.username_value + u"1";
  std::u16string new_password = saved_match_.password_value + u"1";
  test_api(submitted_form).field(kUsernameFieldIndex).set_value(new_username);
  test_api(submitted_form).field(kPasswordFieldIndex).set_value(new_password);

  PasswordForm parsed_submitted_form = Parse(submitted_form);
  // Set SubmissionIndicatorEvent to test metrics recording.
  parsed_submitted_form.submission_event =
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;

  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form, &observed_form_, submitted_form,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_TRUE(password_save_manager_impl()->IsNewLogin());

  PasswordForm saved_form;
  std::vector<raw_ptr<const PasswordForm, VectorExperimental>> best_matches;
  EXPECT_CALL(*mock_profile_form_saver(), Save)
      .WillOnce(DoAll(SaveArg<0>(&saved_form), SaveArg<1>(&best_matches)));

  password_save_manager_impl()->Save(&observed_form_, parsed_submitted_form);

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

  // Check histograms.
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AcceptedSaveUpdateSubmissionIndicatorEvent",
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION, 1);

  // Check UKM metrics.
  DestroySaveManagerAndMetricsRecorder();
  ExpectedGenerationUKM expected_metrics = {
      {} /* shown manually */,
      0 /* password generated */,
      {} /* generated password is not modified */};

  CheckPasswordGenerationUKM(test_ukm_recorder, expected_metrics);
}

// Check that if there is saved PSL matched credentials with the same
// username/password as in submitted form, then the saved form is the same
// already saved only with origin and signon_realm from the submitted form.
TEST_P(PasswordSaveManagerImplTest, SavePSLToAlreadySaved) {
  SetNonFederatedAndNotifyFetchCompleted({psl_saved_match_});

  FormData submitted_form = observed_form_;
  test_api(submitted_form)
      .field(kUsernameFieldIndex)
      .set_value(psl_saved_match_.username_value);
  test_api(submitted_form)
      .field(kPasswordFieldIndex)
      .set_value(psl_saved_match_.password_value);

  password_save_manager_impl()->CreatePendingCredentials(
      Parse(submitted_form), &observed_form_, submitted_form,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_TRUE(password_save_manager_impl()->IsNewLogin());
  EXPECT_EQ(PasswordForm::MatchType::kPSL,
            password_save_manager_impl()->GetPendingCredentials().match_type);

  PasswordForm saved_form;
  std::vector<raw_ptr<const PasswordForm, VectorExperimental>> best_matches;
  EXPECT_CALL(*mock_profile_form_saver(), Save)
      .WillOnce(DoAll(SaveArg<0>(&saved_form), SaveArg<1>(&best_matches)));

  password_save_manager_impl()->Save(&observed_form_, Parse(submitted_form));

  EXPECT_EQ(submitted_form.url(), saved_form.url);
  EXPECT_EQ(GetSignonRealm(submitted_form.url()), saved_form.signon_realm);
  EXPECT_EQ(psl_saved_match_.username_value, saved_form.username_value);
  EXPECT_EQ(psl_saved_match_.password_value, saved_form.password_value);
  EXPECT_EQ(psl_saved_match_.username_element, saved_form.username_element);
  EXPECT_EQ(psl_saved_match_.password_element, saved_form.password_element);

  ASSERT_EQ(best_matches.size(), 1u);
  EXPECT_EQ(*best_matches[0], psl_saved_match_);
}

// Tests that when credentials with already saved username but with a new
// password are submitted, then the saved password is updated.
TEST_P(PasswordSaveManagerImplTest, OverridePassword) {
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  FormData submitted_form = observed_form_;
  std::u16string username = saved_match_.username_value;
  std::u16string new_password = saved_match_.password_value + u"1";
  test_api(submitted_form).field(kUsernameFieldIndex).set_value(username);
  test_api(submitted_form).field(kPasswordFieldIndex).set_value(new_password);

  password_save_manager_impl()->CreatePendingCredentials(
      Parse(submitted_form), &observed_form_, submitted_form,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_FALSE(password_save_manager_impl()->IsNewLogin());
  EXPECT_TRUE(password_save_manager_impl()->IsPasswordUpdate());

  PasswordForm updated_form;
  EXPECT_CALL(*mock_profile_form_saver(),
              Update(_, ElementsAre(Pointee(saved_match_)),
                     saved_match_.password_value))
      .WillOnce(SaveArg<0>(&updated_form));

  password_save_manager_impl()->Save(&observed_form_, Parse(submitted_form));

  EXPECT_TRUE(ArePasswordFormUniqueKeysEqual(saved_match_, updated_form));
  EXPECT_EQ(new_password, updated_form.password_value);
}

// Tests that when the user changes password on a change password form then the
// saved password is updated.
TEST_P(PasswordSaveManagerImplTest, UpdatePasswordOnChangePasswordForm) {
  PasswordForm not_best_saved_match = saved_match_;
  PasswordForm saved_match_another_username = saved_match_;
  saved_match_another_username.username_value += u"1";

  SetNonFederatedAndNotifyFetchCompleted(
      {saved_match_, not_best_saved_match, saved_match_another_username});

  FormData submitted_form = observed_form_only_password_fields_;
  test_api(submitted_form).field(0).set_value(saved_match_.password_value);
  std::u16string new_password = saved_match_.password_value + u"1";
  test_api(submitted_form).field(1).set_value(new_password);

  password_save_manager_impl()->CreatePendingCredentials(
      Parse(submitted_form), &observed_form_only_password_fields_,
      submitted_form,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_FALSE(password_save_manager_impl()->IsNewLogin());
  EXPECT_TRUE(password_save_manager_impl()->IsPasswordUpdate());

  PasswordForm updated_form;
  EXPECT_CALL(*mock_profile_form_saver(),
              Update(_,
                     UnorderedElementsAre(
                         Pointee(saved_match_), Pointee(not_best_saved_match),
                         Pointee(saved_match_another_username)),
                     saved_match_.password_value))
      .WillOnce(SaveArg<0>(&updated_form));

  password_save_manager_impl()->Save(&observed_form_only_password_fields_,
                                     Parse(submitted_form));

  EXPECT_TRUE(ArePasswordFormUniqueKeysEqual(saved_match_, updated_form));
  EXPECT_EQ(new_password, updated_form.password_value);
}

TEST_P(PasswordSaveManagerImplTest, UpdateUsernameToAnotherFieldValue) {
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner());
  fetcher()->NotifyFetchCompleted();

  std::u16string user_chosen_username = u"user_chosen_username";
  std::u16string automatically_chosen_username =
      u"automatically_chosen_username";
  test_api(submitted_form_).field(0).set_value(user_chosen_username);
  test_api(submitted_form_).field(1).set_value(automatically_chosen_username);
  PasswordForm parsed_submitted_form = Parse(submitted_form_);
  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form, &observed_form_only_password_fields_,
      submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);
  EXPECT_EQ(
      automatically_chosen_username,
      password_save_manager_impl()->GetPendingCredentials().username_value);

  // Simulate username update from the prompt.
  parsed_submitted_form.username_value = user_chosen_username;
  parsed_submitted_form.username_element.clear();
  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form, &observed_form_only_password_fields_,
      submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_EQ(
      user_chosen_username,
      password_save_manager_impl()->GetPendingCredentials().username_value);

  password_save_manager_impl()->Save(&observed_form_only_password_fields_,
                                     parsed_submitted_form);
}

TEST_P(PasswordSaveManagerImplTest, UpdateUsernameToAlreadyExisting) {
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});
  PasswordForm parsed_submitted_form = Parse(submitted_form_);
  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  std::u16string new_username = saved_match_.username_value;
  std::u16string expected_password = parsed_submitted_form_.password_value;
  PasswordForm expected = saved_match_;
  expected.password_value = expected_password;

  // Simulate username update from the prompt.
  parsed_submitted_form.username_value = new_username;
  parsed_submitted_form.username_element.clear();
  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  CheckPendingCredentials(
      expected, password_save_manager_impl()->GetPendingCredentials());
  EXPECT_FALSE(password_save_manager_impl()->IsNewLogin());
  EXPECT_TRUE(password_save_manager_impl()->IsPasswordUpdate());
}

TEST_P(PasswordSaveManagerImplTest, UpdatePasswordValueEmptyStore) {
  fetcher()->NotifyFetchCompleted();

  PasswordForm parsed_submitted_form = Parse(submitted_form_);
  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  std::u16string new_password = parsed_submitted_form_.password_value + u"1";
  PasswordForm expected = parsed_submitted_form;
  expected.password_value = new_password;
  expected.password_element.clear();

  // Simulate password update from the prompt.
  parsed_submitted_form.password_value = new_password;
  parsed_submitted_form.password_element.clear();
  parsed_submitted_form.new_password_value.clear();
  parsed_submitted_form.new_password_element.clear();

  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  CheckPendingCredentials(
      expected, password_save_manager_impl()->GetPendingCredentials());
  EXPECT_TRUE(password_save_manager_impl()->IsNewLogin());

  // TODO(crbug.com/41439338): implement not sending incorrect votes and
  // check that StartUploadRequest is not called.
  EXPECT_CALL(*mock_autofill_crowdsourcing_manager(), StartUploadRequest);
  password_save_manager_impl()->Save(&observed_form_, parsed_submitted_form);
}

TEST_P(PasswordSaveManagerImplTest, UpdatePasswordValueToAlreadyExisting) {
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  // Emulate submitting form with known username and different password.
  test_api(submitted_form_)
      .field(kUsernameFieldIndex)
      .set_value(saved_match_.username_value);

  PasswordForm parsed_submitted_form = Parse(submitted_form_);
  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  // Simulate password update from the prompt to already saved one.
  parsed_submitted_form.password_value = saved_match_.password_value;
  parsed_submitted_form.password_element.clear();
  parsed_submitted_form.new_password_value.clear();
  parsed_submitted_form.new_password_element.clear();

  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  CheckPendingCredentials(
      saved_match_, password_save_manager_impl()->GetPendingCredentials());

  EXPECT_FALSE(password_save_manager_impl()->IsNewLogin());
  EXPECT_FALSE(password_save_manager_impl()->IsPasswordUpdate());
}

TEST_P(PasswordSaveManagerImplTest, UpdatePasswordValueMultiplePasswordFields) {
  FormData submitted_form = observed_form_only_password_fields_;

  fetcher()->NotifyFetchCompleted();
  std::u16string password = u"password1";
  std::u16string pin = u"pin";
  test_api(submitted_form).field(0).set_value(password);
  test_api(submitted_form).field(1).set_value(pin);
  PasswordForm parsed_submitted_form = Parse(submitted_form);

  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form, &observed_form_, submitted_form,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  // Check that a second password field is chosen for saving.
  EXPECT_EQ(
      pin,
      password_save_manager_impl()->GetPendingCredentials().password_value);

  PasswordForm expected = password_save_manager_impl()->GetPendingCredentials();
  expected.password_value = password;
  expected.password_element_renderer_id =
      submitted_form.fields()[0].renderer_id();
  expected.password_element = submitted_form.fields()[0].name();

  // Simulate that the user updates value to save for the first password field
  // using the update prompt.
  parsed_submitted_form.password_value = password;
  parsed_submitted_form.password_element = submitted_form.fields()[0].name();
  parsed_submitted_form.new_password_value.clear();
  parsed_submitted_form.new_password_element.clear();

  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form, &observed_form_, submitted_form,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  // Check that newly created pending credentials are correct.
  CheckPendingCredentials(
      expected, password_save_manager_impl()->GetPendingCredentials());
  EXPECT_TRUE(password_save_manager_impl()->IsNewLogin());

  // Check that a vote is sent for the field with the value which is chosen by
  // the user.
  auto upload_contents_matcher = IsPasswordUpload(FieldsContain(
      UploadFieldIs(submitted_form.fields()[0], FieldType::PASSWORD)));
  EXPECT_CALL(*mock_autofill_crowdsourcing_manager(),
              StartUploadRequest(upload_contents_matcher, _, _));

  // Check that the password which was chosen by the user is saved.
  PasswordForm saved_form;
  EXPECT_CALL(*mock_profile_form_saver(), Save)
      .WillOnce(SaveArg<0>(&saved_form));

  password_save_manager_impl()->Save(&observed_form_, parsed_submitted_form);
  CheckPendingCredentials(expected, saved_form);
}

TEST_P(PasswordSaveManagerImplTest, PresaveGeneratedPasswordEmptyStore) {
  fetcher()->NotifyFetchCompleted();

  EXPECT_FALSE(password_save_manager_impl()->HasGeneratedPassword());

  // Check that the generated password is presaved.
  PasswordForm saved_form;
  EXPECT_CALL(*mock_profile_form_saver(), Save(_, IsEmpty(), std::u16string()))
      .WillOnce(SaveArg<0>(&saved_form));

  PasswordForm form_with_generated_password = parsed_submitted_form_;

  password_save_manager_impl()->PresaveGeneratedPassword(
      form_with_generated_password);

  EXPECT_TRUE(password_save_manager_impl()->HasGeneratedPassword());
  EXPECT_EQ(saved_form.username_value,
            form_with_generated_password.username_value);
  EXPECT_EQ(saved_form.password_value,
            form_with_generated_password.password_value);

  Mock::VerifyAndClearExpectations(mock_profile_form_saver());

  // Check that when the generated password is edited, then it's presaved.
  form_with_generated_password.password_value += u"1";
  EXPECT_CALL(*mock_profile_form_saver(),
              UpdateReplace(_, IsEmpty(), testing::Eq(u""),
                            FormHasUniqueKey(form_with_generated_password)))
      .WillOnce(SaveArg<0>(&saved_form));

  password_save_manager_impl()->PresaveGeneratedPassword(
      form_with_generated_password);

  EXPECT_TRUE(password_save_manager_impl()->HasGeneratedPassword());
  EXPECT_EQ(saved_form.username_value,
            form_with_generated_password.username_value);
  EXPECT_EQ(saved_form.password_value,
            form_with_generated_password.password_value);

  Mock::VerifyAndClearExpectations(mock_profile_form_saver());
}

TEST_P(PasswordSaveManagerImplTest, PresaveGenerated_ModifiedUsername) {
  fetcher()->NotifyFetchCompleted();

  // Check that the generated password is presaved.
  PasswordForm saved_form;
  EXPECT_CALL(*mock_profile_form_saver(), Save)
      .WillOnce(SaveArg<0>(&saved_form));
  PasswordForm form_with_generated_password = parsed_submitted_form_;

  password_save_manager_impl()->PresaveGeneratedPassword(
      form_with_generated_password);

  Mock::VerifyAndClearExpectations(mock_profile_form_saver());

  // Check that when the username is edited, then it's presaved.
  form_with_generated_password.username_value += u"1";

  EXPECT_CALL(*mock_profile_form_saver(),
              UpdateReplace(_, IsEmpty(), testing::Eq(u""),
                            FormHasUniqueKey(saved_form)))
      .WillOnce(SaveArg<0>(&saved_form));

  password_save_manager_impl()->PresaveGeneratedPassword(
      form_with_generated_password);

  EXPECT_TRUE(password_save_manager_impl()->HasGeneratedPassword());
  EXPECT_EQ(saved_form.username_value,
            form_with_generated_password.username_value);
  EXPECT_EQ(saved_form.password_value,
            form_with_generated_password.password_value);
}

TEST_P(PasswordSaveManagerImplTest,
       PresaveGeneratedPasswordExistingCredential) {
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  // Check that the generated password is presaved.
  PasswordForm saved_form;
  EXPECT_CALL(*mock_profile_form_saver(), Save)
      .WillOnce(SaveArg<0>(&saved_form));

  PasswordForm form_with_generated_password = parsed_submitted_form_;

  // Check that the generated password is saved with the empty username when
  // there is already a saved credential with the same username.
  form_with_generated_password.username_value = saved_match_.username_value;

  password_save_manager_impl()->PresaveGeneratedPassword(
      form_with_generated_password);

  EXPECT_TRUE(password_save_manager_impl()->HasGeneratedPassword());
  EXPECT_TRUE(saved_form.username_value.empty());
  EXPECT_EQ(form_with_generated_password.password_value,
            saved_form.password_value);
}

TEST_P(PasswordSaveManagerImplTest, PasswordNoLongerGenerated) {
  fetcher()->NotifyFetchCompleted();
  EXPECT_CALL(*mock_profile_form_saver(), Save);
  PasswordForm submitted_form(parsed_observed_form_);
  submitted_form.password_value = u"password";
  password_save_manager_impl()->PresaveGeneratedPassword(submitted_form);
  EXPECT_CALL(*mock_profile_form_saver(), Remove);
  password_save_manager_impl()->PasswordNoLongerGenerated();
}

TEST_P(PasswordSaveManagerImplTest, UserEventsForGeneration_Accept) {
  using GeneratedPasswordStatus =
      PasswordFormMetricsRecorder::GeneratedPasswordStatus;

  base::HistogramTester histogram_tester;

  password_save_manager_impl()->PresaveGeneratedPassword(parsed_observed_form_);

  DestroySaveManagerAndMetricsRecorder();
  histogram_tester.ExpectUniqueSample(
      "PasswordGeneration.UserDecision",
      GeneratedPasswordStatus::kPasswordAccepted, 1);
}

TEST_P(PasswordSaveManagerImplTest, UserEventsForGeneration_Edit) {
  using GeneratedPasswordStatus =
      PasswordFormMetricsRecorder::GeneratedPasswordStatus;

  PasswordForm submitted_form(parsed_observed_form_);

  base::HistogramTester histogram_tester;

  password_save_manager_impl()->PresaveGeneratedPassword(submitted_form);

  submitted_form.password_value += u"1";

  password_save_manager_impl()->PresaveGeneratedPassword(submitted_form);

  DestroySaveManagerAndMetricsRecorder();
  histogram_tester.ExpectUniqueSample("PasswordGeneration.UserDecision",
                                      GeneratedPasswordStatus::kPasswordEdited,
                                      1);
}

TEST_P(PasswordSaveManagerImplTest, UserEventsForGeneration_Clear) {
  using GeneratedPasswordStatus =
      PasswordFormMetricsRecorder::GeneratedPasswordStatus;

  PasswordForm submitted_form(parsed_observed_form_);

  base::HistogramTester histogram_tester;

  password_save_manager_impl()->PresaveGeneratedPassword(submitted_form);

  submitted_form.password_value += u"2";

  password_save_manager_impl()->PresaveGeneratedPassword(submitted_form);

  password_save_manager_impl()->PasswordNoLongerGenerated();

  DestroySaveManagerAndMetricsRecorder();
  histogram_tester.ExpectUniqueSample("PasswordGeneration.UserDecision",
                                      GeneratedPasswordStatus::kPasswordDeleted,
                                      1);
}

TEST_P(PasswordSaveManagerImplTest, Update) {
  base::HistogramTester histogram_tester;

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

  PasswordForm parsed_submitted_form = Parse(submitted_form);
  // Set SubmissionIndicatorEvent to test metrics recording.
  parsed_submitted_form.submission_event =
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;

  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form, &observed_form_, submitted_form,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  PasswordForm updated_form;
  EXPECT_CALL(
      *mock_profile_form_saver(),
      Update(_,
             UnorderedElementsAre(Pointee(saved_match_),
                                  Pointee(saved_match_another_username)),
             saved_match_.password_value))
      .WillOnce(SaveArg<0>(&updated_form));

  const base::Time kNow = base::Time::Now();

  password_save_manager_impl()->Save(&observed_form_, parsed_submitted_form);

  EXPECT_TRUE(ArePasswordFormUniqueKeysEqual(saved_match_, updated_form));
  EXPECT_EQ(new_password, updated_form.password_value);
  EXPECT_GE(updated_form.date_last_used, kNow);

  // Check histograms.
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AcceptedSaveUpdateSubmissionIndicatorEvent",
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION, 1);
}

TEST_P(PasswordSaveManagerImplTest, HTTPAuthPasswordOverridden) {
  PasswordForm http_auth_form = parsed_observed_form_;
  http_auth_form.scheme = PasswordForm::Scheme::kBasic;
  fetcher()->set_scheme(PasswordForm::Scheme::kBasic);

  PasswordForm saved_http_auth_form = http_auth_form;
  const std::u16string username = u"user1";
  const std::u16string password = u"pass1";
  saved_http_auth_form.username_value = username;
  saved_http_auth_form.password_value = password;

  SetNonFederatedAndNotifyFetchCompleted({saved_http_auth_form});

  // Check that if new password is submitted, then |form_manager_| is in state
  // password overridden.
  PasswordForm submitted_http_auth_form = saved_http_auth_form;
  std::u16string new_password = password + u"1";
  submitted_http_auth_form.password_value = new_password;

  password_save_manager_impl()->CreatePendingCredentials(
      submitted_http_auth_form, &observed_form_, submitted_form_,
      /*is_http_auth=*/true,
      /*is_credential_api_save=*/false);

  EXPECT_FALSE(password_save_manager_impl()->IsNewLogin());
  EXPECT_TRUE(password_save_manager_impl()->IsPasswordUpdate());

  // Check that the password is updated in the stored credential.
  PasswordForm updated_form;
  EXPECT_CALL(*mock_profile_form_saver(),
              Update(_, ElementsAre(Pointee(saved_http_auth_form)), password))
      .WillOnce(SaveArg<0>(&updated_form));

  password_save_manager_impl()->Save(&observed_form_, submitted_http_auth_form);

  EXPECT_TRUE(
      ArePasswordFormUniqueKeysEqual(saved_http_auth_form, updated_form));
  EXPECT_EQ(new_password, updated_form.password_value);
}

TEST_P(PasswordSaveManagerImplTest, IncrementTimesUsedWhenHTMLFormSubmissions) {
  PasswordForm saved_credential = saved_match_;
  saved_credential.times_used_in_html_form = 5;
  saved_credential.scheme = PasswordForm::Scheme::kHtml;
  SetNonFederatedAndNotifyFetchCompleted({saved_credential});

  password_save_manager_impl()->CreatePendingCredentials(
      saved_credential, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_CALL(*mock_profile_form_saver(),
              Update(Field(&PasswordForm::times_used_in_html_form, 6), _, _));
  password_save_manager_impl()->Save(&observed_form_, saved_credential);
}

TEST_P(PasswordSaveManagerImplTest, DontIncrementTimesUsedWhenBasicHTTPAuth) {
  fetcher()->set_scheme(PasswordForm::Scheme::kBasic);
  PasswordForm saved_credential = saved_match_;
  saved_credential.times_used_in_html_form = 0;
  saved_credential.scheme = PasswordForm::Scheme::kBasic;
  SetNonFederatedAndNotifyFetchCompleted({saved_credential});

  password_save_manager_impl()->CreatePendingCredentials(
      saved_credential, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_CALL(*mock_profile_form_saver(),
              Update(Field(&PasswordForm::times_used_in_html_form, 0), _, _));
  password_save_manager_impl()->Save(&observed_form_, saved_credential);
}

TEST_P(PasswordSaveManagerImplTest, UsernameCorrectionVote) {
  // Setup a matched form in the storage for the currently submitted form.
  const std::u16string matched_form_username_field_name = u"new_username_id";
  FormFieldData field1;
  field1.set_name(matched_form_username_field_name);
  field1.set_id_attribute(field1.name());
  field1.set_name_attribute(field1.name());
  field1.set_form_control_type(autofill::FormControlType::kInputText);
  test_api(saved_match_.form_data).Append(field1);

  FormFieldData field2;
  field2.set_name(u"firstname");
  field2.set_id_attribute(field2.name());
  field2.set_name_attribute(field2.name());
  field2.set_form_control_type(autofill::FormControlType::kInputText);
  test_api(saved_match_.form_data).Append(field2);
  saved_match_.username_element = field2.name();

  FormFieldData field3;
  field3.set_name(u"password");
  field3.set_id_attribute(field3.name());
  field3.set_name_attribute(field3.name());
  field3.set_form_control_type(autofill::FormControlType::kInputPassword);
  test_api(saved_match_.form_data).Append(field3);
  saved_match_.password_element = field3.name();

  const std::u16string username = u"user1";
  saved_match_.all_alternative_usernames.emplace_back(
      AlternativeElement::Value(username), autofill::FieldRendererId(),
      AlternativeElement::Name(matched_form_username_field_name));

  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  // Use the same credentials on the submitted form.
  submitted_form_ = observed_form_;
  test_api(submitted_form_).field(kUsernameFieldIndex).set_value(username);
  test_api(submitted_form_)
      .field(kPasswordFieldIndex)
      .set_value(saved_match_.password_value);

  PasswordForm parsed_submitted_form = Parse(submitted_form_);
  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  // Check that a vote is sent for the password field.
  auto upload_contents_matcher = IsPasswordUpload(FieldsContain(UploadFieldIs(
      submitted_form_.fields()[kPasswordFieldIndex], FieldType::PASSWORD)));
  EXPECT_CALL(*mock_autofill_crowdsourcing_manager(),
              StartUploadRequest(upload_contents_matcher, _, _));

  // Check that a correction vote is sent for the earlier saved form.
  upload_contents_matcher = IsPasswordUpload(FieldsContain(
      UploadFieldIs(field1, FieldType::USERNAME),
      UploadFieldIs(field3, FieldType::ACCOUNT_CREATION_PASSWORD)));
  EXPECT_CALL(*mock_autofill_crowdsourcing_manager(),
              StartUploadRequest(upload_contents_matcher, _, _));

  password_save_manager_impl()->Save(&observed_form_, parsed_submitted_form);
}

TEST_P(PasswordSaveManagerImplTest, MarkSharedCredentialAsNotifiedUponSave) {
  PasswordForm saved_shared_credentials = saved_match_;
  saved_shared_credentials.type = PasswordForm::Type::kReceivedViaSharing;
  saved_shared_credentials.sharing_notification_displayed = false;
  SetNonFederatedAndNotifyFetchCompleted({saved_shared_credentials});

  password_save_manager_impl()->CreatePendingCredentials(
      saved_shared_credentials, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_CALL(
      *mock_profile_form_saver(),
      Update(Field(&PasswordForm::sharing_notification_displayed, true), _, _));
  password_save_manager_impl()->Save(&observed_form_, saved_shared_credentials);
}

TEST_P(PasswordSaveManagerImplTest,
       PresavedGeneratedPasswordWithEmptyUsernameUpdate) {
  fetcher()->NotifyFetchCompleted();

  PasswordForm form_with_generated_password = parsed_submitted_form_;
  form_with_generated_password.username_value = u"";

  password_save_manager_impl()->PresaveGeneratedPassword(
      form_with_generated_password);
  password_save_manager_impl()->CreatePendingCredentials(
      form_with_generated_password, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  // Do not consider as update if there is a pre-saved generated password with
  // the same value.
  ASSERT_FALSE(password_save_manager_impl()->IsPasswordUpdate());
}

INSTANTIATE_TEST_SUITE_P(,
                         PasswordSaveManagerImplTest,
                         testing::Values(false, true));

class MultiStorePasswordSaveManagerTest
    : public PasswordSaveManagerImplTestBase {
 public:
  MultiStorePasswordSaveManagerTest()
      : PasswordSaveManagerImplTestBase(/*enable_account_store=*/true) {}
};

TEST_F(MultiStorePasswordSaveManagerTest,
       SaveInAccountStoreWhenAccountStoreEnabled) {
  SetAccountStoreEnabled(/*is_enabled=*/true);

  fetcher()->NotifyFetchCompleted();

  SetDefaultPasswordStore(PasswordForm::Store::kAccountStore);

  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_TRUE(password_save_manager_impl()->IsNewLogin());

  EXPECT_CALL(*mock_profile_form_saver(), Save).Times(0);
  EXPECT_CALL(*mock_account_form_saver(), Save);

  password_save_manager_impl()->Save(&observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       DoNotSaveInAccountStoreWhenAccountStoreDisabled) {
  SetAccountStoreEnabled(/*is_enabled=*/false);

  fetcher()->NotifyFetchCompleted();

  SetDefaultPasswordStore(PasswordForm::Store::kAccountStore);

  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_TRUE(password_save_manager_impl()->IsNewLogin());

  EXPECT_CALL(*mock_profile_form_saver(), Save).Times(0);
  EXPECT_CALL(*mock_account_form_saver(), Save).Times(0);

  password_save_manager_impl()->Save(&observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest, SaveInProfileStore) {
  SetAccountStoreEnabled(/*is_enabled=*/true);

  fetcher()->NotifyFetchCompleted();

  SetDefaultPasswordStore(PasswordForm::Store::kProfileStore);

  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_TRUE(password_save_manager_impl()->IsNewLogin());

  EXPECT_CALL(*mock_profile_form_saver(), Save);
  EXPECT_CALL(*mock_account_form_saver(), Save).Times(0);

  password_save_manager_impl()->Save(&observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest, UpdateInAccountStoreOnly) {
  SetAccountStoreEnabled(/*is_enabled=*/true);

  PasswordForm saved_match_in_account_store(saved_match_);
  saved_match_in_account_store.username_value =
      parsed_submitted_form_.username_value;
  saved_match_in_account_store.in_store = PasswordForm::Store::kAccountStore;
  SetNonFederatedAndNotifyFetchCompleted({saved_match_in_account_store});

  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_FALSE(password_save_manager_impl()->IsNewLogin());
  // An update prompt should be shown.
  EXPECT_TRUE(password_save_manager_impl()->IsPasswordUpdate());

  EXPECT_CALL(*mock_profile_form_saver(), Update).Times(0);
  EXPECT_CALL(*mock_account_form_saver(), Update);

  password_save_manager_impl()->Save(&observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest, UpdateInProfileStoreOnly) {
  SetAccountStoreEnabled(/*is_enabled=*/true);

  PasswordForm saved_match_in_profile_store(saved_match_);
  saved_match_in_profile_store.username_value =
      parsed_submitted_form_.username_value;
  saved_match_in_profile_store.in_store = PasswordForm::Store::kProfileStore;
  SetNonFederatedAndNotifyFetchCompleted({saved_match_in_profile_store});

  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_FALSE(password_save_manager_impl()->IsNewLogin());
  // An update prompt should be shown.
  EXPECT_TRUE(password_save_manager_impl()->IsPasswordUpdate());

  EXPECT_CALL(*mock_profile_form_saver(), Update);
  EXPECT_CALL(*mock_account_form_saver(), Update).Times(0);

  password_save_manager_impl()->Save(&observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest, UpdateInBothStores) {
  // This test assumes that all fields of the PasswordForm in both stores are
  // equal except the |moving_blocked_for_list|. The reason for that is:
  // 1. |moving_blocked_for_list| is the most probable field to have different
  //    values since it's always empty in the account store.
  // 2. Other fields (e.g. |times_used_in_html_form|) are less critical and
  //    should be fine if the value in one store overrides the value in the
  //    other one.

  SetAccountStoreEnabled(/*is_enabled=*/true);

  PasswordForm saved_match_in_account_store(saved_match_);
  saved_match_in_account_store.username_value =
      parsed_submitted_form_.username_value;
  saved_match_in_account_store.in_store = PasswordForm::Store::kAccountStore;
  PasswordForm saved_match_in_profile_store(saved_match_in_account_store);
  saved_match_in_profile_store.in_store = PasswordForm::Store::kProfileStore;
  signin::GaiaIdHash user_id_hash =
      signin::GaiaIdHash::FromGaiaId("user@gmail.com");
  saved_match_in_profile_store.moving_blocked_for_list.push_back(user_id_hash);

  SetNonFederatedAndNotifyFetchCompleted(
      {saved_match_in_profile_store, saved_match_in_account_store});

  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_FALSE(password_save_manager_impl()->IsNewLogin());
  // An update prompt should be shown.
  EXPECT_TRUE(password_save_manager_impl()->IsPasswordUpdate());

  // Both stores should be updated in the following ways:
  // 1. |password_value| is updated.
  // 2. |times_used_in_html_form| is incremented.
  // 3. |date_last_used| is updated.
  // 4. |in_store| field is irrelevant since it's not persisted.
  // 5. The rest of fields are taken arbitrarily from one store.
  PasswordForm expected_profile_updated_form(saved_match_in_profile_store);
  expected_profile_updated_form.password_value =
      parsed_submitted_form_.password_value;
  expected_profile_updated_form.times_used_in_html_form++;
  expected_profile_updated_form.date_last_used =
      password_save_manager_impl()->GetPendingCredentials().date_last_used;
  expected_profile_updated_form.in_store =
      password_save_manager_impl()->GetPendingCredentials().in_store;

  PasswordForm expected_account_updated_form(saved_match_in_account_store);
  expected_account_updated_form.password_value =
      parsed_submitted_form_.password_value;
  expected_account_updated_form.times_used_in_html_form++;
  expected_account_updated_form.date_last_used =
      password_save_manager_impl()->GetPendingCredentials().date_last_used;
  expected_account_updated_form.in_store =
      password_save_manager_impl()->GetPendingCredentials().in_store;

  EXPECT_CALL(*mock_profile_form_saver(),
              Update(MatchesUpdatedForm(expected_profile_updated_form), _, _));
  EXPECT_CALL(*mock_account_form_saver(),
              Update(MatchesUpdatedForm(expected_account_updated_form), _, _));

  password_save_manager_impl()->Save(&observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       UpdateWithAGeneratedPasswordInBothStores) {
  // This test assumes that there is a password with the same username and url
  // saved in both account and profile stores and it's been updated with a
  // generated password. Expected result: the password should be updated in both
  // stores.

  SetAccountStoreEnabled(/*is_enabled=*/true);

  PasswordForm saved_match_in_account_store(saved_match_);
  saved_match_in_account_store.username_value =
      parsed_submitted_form_.username_value;
  saved_match_in_account_store.in_store = PasswordForm::Store::kAccountStore;
  PasswordForm saved_match_in_profile_store(saved_match_in_account_store);
  saved_match_in_profile_store.in_store = PasswordForm::Store::kProfileStore;

  SetNonFederatedAndNotifyFetchCompleted(
      {saved_match_in_profile_store, saved_match_in_account_store});

  password_save_manager_impl()->PresaveGeneratedPassword(
      parsed_submitted_form_);
  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_TRUE(password_save_manager_impl()->IsPasswordUpdate());
  // Password generation manager should call `UpdateRelace` to overwrite the
  // pre-saved password form.
  EXPECT_CALL(*mock_profile_form_saver(), Update);
  EXPECT_CALL(*mock_account_form_saver(), UpdateReplace);

  password_save_manager_impl()->Save(&observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       CreateNewInAccountAndUpdateInProfileWithGeneratedPassword) {
  // This test assumes that there is a credential saved in the profile password
  // store and a credential with the same username and a newly generated
  // password is being saved. Expected result: a new credential is saved in the
  // account password store, the credential in the profile store is updated with
  // the generated password.

  SetAccountStoreEnabled(/*is_enabled=*/true);

  PasswordForm saved_match_in_profile_store(saved_match_);
  saved_match_in_profile_store.username_value =
      parsed_submitted_form_.username_value;
  saved_match_in_profile_store.in_store = PasswordForm::Store::kProfileStore;

  SetNonFederatedAndNotifyFetchCompleted({saved_match_in_profile_store});

  password_save_manager_impl()->PresaveGeneratedPassword(
      parsed_submitted_form_);
  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  // Password generation manager should call `UpdateRelace` to overwrite the
  // pre-saved password form.
  EXPECT_CALL(*mock_profile_form_saver(), Update);
  EXPECT_CALL(*mock_account_form_saver(), UpdateReplace);

  password_save_manager_impl()->Save(&observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest, AutomaticSaveInBothStores) {
  SetAccountStoreEnabled(/*is_enabled=*/true);

  // Set different values for the fields that should be preserved per store
  // (namely: date_created, times_used_in_html_form, moving_blocked_for_list)
  PasswordForm saved_match_in_profile_store(saved_match_);
  saved_match_in_profile_store.username_value =
      parsed_submitted_form_.username_value;
  saved_match_in_profile_store.password_value =
      parsed_submitted_form_.password_value;
  saved_match_in_profile_store.in_store = PasswordForm::Store::kProfileStore;
  saved_match_in_profile_store.date_created =
      base::Time::Now() - base::Days(10);
  saved_match_in_profile_store.times_used_in_html_form = 10;
  saved_match_in_profile_store.moving_blocked_for_list.push_back(
      signin::GaiaIdHash::FromGaiaId("email@gmail.com"));

  PasswordForm saved_match_in_account_store(saved_match_in_profile_store);
  saved_match_in_account_store.in_store = PasswordForm::Store::kAccountStore;
  saved_match_in_account_store.date_created = base::Time::Now();
  saved_match_in_account_store.times_used_in_html_form = 5;
  saved_match_in_account_store.moving_blocked_for_list.clear();

  SetNonFederatedAndNotifyFetchCompleted(
      {saved_match_in_profile_store, saved_match_in_account_store});

  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  // No save or update prompts should be shown.
  EXPECT_FALSE(password_save_manager_impl()->IsNewLogin());
  EXPECT_FALSE(password_save_manager_impl()->IsPasswordUpdate());

  // We still should update both credentials to update the |date_last_used| and
  // |times_used_in_html_form|. Note that |in_store| is irrelevant since it's
  // not persisted. All other fields should be preserved.
  PasswordForm expected_profile_update_form(saved_match_in_profile_store);
  expected_profile_update_form.times_used_in_html_form++;
  expected_profile_update_form.date_last_used =
      password_save_manager_impl()->GetPendingCredentials().date_last_used;
  expected_profile_update_form.in_store =
      password_save_manager_impl()->GetPendingCredentials().in_store;

  PasswordForm expected_account_update_form(saved_match_in_account_store);
  expected_account_update_form.times_used_in_html_form++;
  expected_account_update_form.date_last_used =
      password_save_manager_impl()->GetPendingCredentials().date_last_used;
  expected_account_update_form.in_store =
      password_save_manager_impl()->GetPendingCredentials().in_store;

  EXPECT_CALL(*mock_profile_form_saver(),
              Update(expected_profile_update_form, _, _));
  EXPECT_CALL(*mock_account_form_saver(),
              Update(expected_account_update_form, _, _));

  password_save_manager_impl()->Save(&observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       PresaveGeneratedPasswordInProfileStoreIfAccountStoreDisabled) {
  SetAccountStoreEnabled(/*is_enabled=*/false);
  fetcher()->NotifyFetchCompleted();

  EXPECT_CALL(*mock_profile_form_saver(), Save);
  EXPECT_CALL(*mock_account_form_saver(), Save).Times(0);

  password_save_manager_impl()->PresaveGeneratedPassword(
      parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       SaveInAccountStoreWhenPSLMatchExistsInTheAccountStore) {
  SetAccountStoreEnabled(/*is_enabled=*/true);

  PasswordForm psl_saved_match(psl_saved_match_);
  psl_saved_match.username_value = parsed_submitted_form_.username_value;
  psl_saved_match.password_value = parsed_submitted_form_.password_value;
  psl_saved_match.in_store = PasswordForm::Store::kAccountStore;
  SetNonFederatedAndNotifyFetchCompleted({psl_saved_match});

  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_CALL(*mock_profile_form_saver(), Save).Times(0);
  EXPECT_CALL(*mock_account_form_saver(), Save);

  password_save_manager_impl()->Save(&observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       SaveInProfileStoreWhenPSLMatchExistsInTheProfileStore) {
  SetAccountStoreEnabled(/*is_enabled=*/true);

  PasswordForm psl_saved_match(psl_saved_match_);
  psl_saved_match.username_value = parsed_submitted_form_.username_value;
  psl_saved_match.password_value = parsed_submitted_form_.password_value;
  psl_saved_match.in_store = PasswordForm::Store::kProfileStore;
  SetNonFederatedAndNotifyFetchCompleted({psl_saved_match});

  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_CALL(*mock_profile_form_saver(), Save);
  EXPECT_CALL(*mock_account_form_saver(), Save).Times(0);

  password_save_manager_impl()->Save(&observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       SaveInBothStoresWhenPSLMatchExistsInBothStores) {
  SetAccountStoreEnabled(/*is_enabled=*/true);

  PasswordForm profile_psl_saved_match(psl_saved_match_);
  profile_psl_saved_match.username_value =
      parsed_submitted_form_.username_value;
  profile_psl_saved_match.password_value =
      parsed_submitted_form_.password_value;
  profile_psl_saved_match.in_store = PasswordForm::Store::kProfileStore;

  PasswordForm account_psl_saved_match(psl_saved_match_);
  account_psl_saved_match.username_value =
      parsed_submitted_form_.username_value;
  account_psl_saved_match.password_value =
      parsed_submitted_form_.password_value;
  account_psl_saved_match.in_store = PasswordForm::Store::kAccountStore;

  SetNonFederatedAndNotifyFetchCompleted(
      {profile_psl_saved_match, account_psl_saved_match});

  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_CALL(*mock_profile_form_saver(), Save);
  EXPECT_CALL(*mock_account_form_saver(), Save);

  password_save_manager_impl()->Save(&observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest, UpdateVsPSLMatch) {
  SetAccountStoreEnabled(/*is_enabled=*/true);

  PasswordForm profile_saved_match(saved_match_);
  profile_saved_match.username_value = parsed_submitted_form_.username_value;
  profile_saved_match.password_value = u"old_password";
  profile_saved_match.in_store = PasswordForm::Store::kProfileStore;

  PasswordForm account_psl_saved_match(psl_saved_match_);
  account_psl_saved_match.username_value =
      parsed_submitted_form_.username_value;
  account_psl_saved_match.password_value =
      parsed_submitted_form_.password_value;
  account_psl_saved_match.in_store = PasswordForm::Store::kAccountStore;

  SetNonFederatedAndNotifyFetchCompleted(
      {profile_saved_match, account_psl_saved_match});

  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  // This should *not* result in an update prompt.
  EXPECT_FALSE(password_save_manager_impl()->IsPasswordUpdate());

  EXPECT_CALL(*mock_profile_form_saver(), Update);
  EXPECT_CALL(*mock_account_form_saver(), Save);

  password_save_manager_impl()->Save(&observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest, UnblocklistInBothStores) {
  SetAccountStoreEnabled(/*is_enabled=*/true);
  const PasswordFormDigest form_digest(saved_match_);

  EXPECT_CALL(*mock_profile_form_saver(), Unblocklist(form_digest));
  EXPECT_CALL(*mock_account_form_saver(), Unblocklist(form_digest));

  password_save_manager_impl()->Unblocklist(form_digest);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       BlocklistInAccountStoreWhenAccountStoreEnabled) {
  SetAccountStoreEnabled(/*is_enabled=*/true);
  const PasswordFormDigest form_digest(saved_match_);
  SetDefaultPasswordStore(PasswordForm::Store::kAccountStore);

  EXPECT_CALL(*mock_profile_form_saver(), Blocklist(form_digest)).Times(0);
  EXPECT_CALL(*mock_account_form_saver(), Blocklist(form_digest));
  password_save_manager_impl()->Blocklist(form_digest);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       BlocklistInProfileStoreAlthoughAccountStoreEnabled) {
  SetAccountStoreEnabled(/*is_enabled=*/true);
  const PasswordFormDigest form_digest(saved_match_);
  SetDefaultPasswordStore(PasswordForm::Store::kProfileStore);

  EXPECT_CALL(*mock_profile_form_saver(), Blocklist(form_digest));
  EXPECT_CALL(*mock_account_form_saver(), Blocklist(form_digest)).Times(0);
  password_save_manager_impl()->Blocklist(form_digest);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       BlocklistInProfileStoreWhenAccountStoreDisabled) {
  SetAccountStoreEnabled(/*is_enabled=*/false);
  const PasswordFormDigest form_digest(saved_match_);
  SetDefaultPasswordStore(PasswordForm::Store::kAccountStore);

  EXPECT_CALL(*mock_profile_form_saver(), Blocklist(form_digest));
  EXPECT_CALL(*mock_account_form_saver(), Blocklist(form_digest)).Times(0);
  password_save_manager_impl()->Blocklist(form_digest);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       MoveCredentialsFromProfileToAccountStoreRecordsFlowAccepted) {
  base::HistogramTester histogram_tester;

  PasswordForm saved_match_in_profile_store(saved_match_);
  saved_match_in_profile_store.in_store = PasswordForm::Store::kProfileStore;
  saved_match_in_profile_store.moving_blocked_for_list.push_back(
      signin::GaiaIdHash::FromGaiaId("user@gmail.com"));
  SetNonFederatedAndNotifyFetchCompleted({saved_match_in_profile_store});

  password_save_manager_impl()->CreatePendingCredentials(
      saved_match_in_profile_store, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  PasswordForm saved_match_without_moving_blocked_list(
      saved_match_in_profile_store);
  saved_match_without_moving_blocked_list.moving_blocked_for_list.clear();

  EXPECT_CALL(*mock_profile_form_saver(), Remove(saved_match_in_profile_store));
  EXPECT_CALL(*mock_account_form_saver(),
              Save(saved_match_without_moving_blocked_list, _, _));

  password_save_manager_impl()->MoveCredentialsToAccountStore(kTrigger);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStorage.MoveToAccountStoreFlowAccepted2",
      kTrigger, 1);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       MoveCredentialsFromProfileToAccountStoreWhenExistsOnlyInProfileStore) {
  PasswordForm saved_match_in_profile_store(saved_match_);
  saved_match_in_profile_store.in_store = PasswordForm::Store::kProfileStore;
  saved_match_in_profile_store.moving_blocked_for_list.push_back(
      signin::GaiaIdHash::FromGaiaId("user@gmail.com"));
  SetNonFederatedAndNotifyFetchCompleted({saved_match_in_profile_store});

  password_save_manager_impl()->CreatePendingCredentials(
      saved_match_in_profile_store, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  PasswordForm saved_match_without_moving_blocked_list(
      saved_match_in_profile_store);
  saved_match_without_moving_blocked_list.moving_blocked_for_list.clear();

  EXPECT_CALL(*mock_profile_form_saver(), Remove(saved_match_in_profile_store));
  EXPECT_CALL(*mock_account_form_saver(),
              Save(saved_match_without_moving_blocked_list, _, _));

  password_save_manager_impl()->MoveCredentialsToAccountStore(kTrigger);
}

TEST_F(
    MultiStorePasswordSaveManagerTest,
    DoNotMoveCredentialsFromProfileToAccountStoreWhenExistsOnlyInProfileStoreWithDifferentUserName) {
  PasswordForm saved_match_in_profile_store(saved_match_);
  saved_match_in_profile_store.in_store = PasswordForm::Store::kProfileStore;
  SetNonFederatedAndNotifyFetchCompleted({saved_match_in_profile_store});
  PasswordForm credentials_with_diffrent_username(saved_match_in_profile_store);
  credentials_with_diffrent_username.username_value = u"different_username";
  password_save_manager_impl()->CreatePendingCredentials(
      credentials_with_diffrent_username, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_CALL(*mock_profile_form_saver(), Remove(saved_match_in_profile_store))
      .Times(0);
  EXPECT_CALL(*mock_account_form_saver(),
              Save(saved_match_in_profile_store, _, _))
      .Times(0);

  password_save_manager_impl()->MoveCredentialsToAccountStore(kTrigger);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       MovePSLMatchedCredentialsFromProfileToAccountStore) {
  PasswordForm saved_match_in_profile_store(saved_match_);
  saved_match_in_profile_store.in_store = PasswordForm::Store::kProfileStore;
  PasswordForm psl_saved_match_in_profile_store(psl_saved_match_);
  psl_saved_match_in_profile_store.in_store =
      PasswordForm::Store::kProfileStore;
  SetNonFederatedAndNotifyFetchCompleted(
      {saved_match_in_profile_store, psl_saved_match_in_profile_store});

  password_save_manager_impl()->CreatePendingCredentials(
      saved_match_in_profile_store, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_CALL(*mock_profile_form_saver(), Remove(saved_match_in_profile_store));
  EXPECT_CALL(*mock_profile_form_saver(),
              Remove(psl_saved_match_in_profile_store));
  EXPECT_CALL(*mock_account_form_saver(),
              Save(saved_match_in_profile_store, _, _));
  EXPECT_CALL(*mock_account_form_saver(),
              Save(psl_saved_match_in_profile_store, _, _));

  password_save_manager_impl()->MoveCredentialsToAccountStore(kTrigger);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       MoveFederatedCredentialsFromProfileToAccountStore) {
  PasswordForm federated_match_in_profile_store = CreateSavedFederated();
  federated_match_in_profile_store.in_store =
      PasswordForm::Store::kProfileStore;

  SetFederatedAndNotifyFetchCompleted({federated_match_in_profile_store});

  password_save_manager_impl()->CreatePendingCredentials(
      federated_match_in_profile_store, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_CALL(*mock_profile_form_saver(),
              Remove(federated_match_in_profile_store));

  EXPECT_CALL(*mock_account_form_saver(),
              Save(federated_match_in_profile_store, _, _));

  password_save_manager_impl()->MoveCredentialsToAccountStore(kTrigger);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       MoveCredentialsFromProfileToAccountStoreWhenExistsInBothStores) {
  PasswordForm saved_match_in_profile_store(saved_match_);
  saved_match_in_profile_store.in_store = PasswordForm::Store::kProfileStore;
  PasswordForm saved_match_in_account_store(saved_match_);
  saved_match_in_account_store.in_store = PasswordForm::Store::kAccountStore;
  SetNonFederatedAndNotifyFetchCompleted(
      {saved_match_in_profile_store, saved_match_in_account_store});

  password_save_manager_impl()->CreatePendingCredentials(
      saved_match_in_profile_store, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_CALL(*mock_profile_form_saver(), Remove(saved_match_in_profile_store));
  EXPECT_CALL(*mock_account_form_saver(), Save).Times(0);

  password_save_manager_impl()->MoveCredentialsToAccountStore(kTrigger);
}

TEST_F(
    MultiStorePasswordSaveManagerTest,
    MoveCredentialsFromProfileToAccountStoreWhenExistsInBothStoresWithDifferentPassword) {
  PasswordForm saved_match_in_profile_store(saved_match_);
  saved_match_in_profile_store.in_store = PasswordForm::Store::kProfileStore;
  saved_match_in_profile_store.password_value = u"password1";
  PasswordForm saved_match_in_account_store(saved_match_);
  saved_match_in_account_store.in_store = PasswordForm::Store::kAccountStore;
  saved_match_in_account_store.password_value = u"password2";
  SetNonFederatedAndNotifyFetchCompleted(
      {saved_match_in_profile_store, saved_match_in_account_store});

  password_save_manager_impl()->CreatePendingCredentials(
      saved_match_in_profile_store, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_CALL(*mock_profile_form_saver(), Remove(saved_match_in_profile_store));
  EXPECT_CALL(*mock_account_form_saver(),
              Save(saved_match_in_profile_store, _, _));

  password_save_manager_impl()->MoveCredentialsToAccountStore(kTrigger);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       MoveCredentialsFromProfileToAccountStoreWhenPSLMatchExistsInBothStores) {
  PasswordForm saved_match_in_profile_store(saved_match_);
  saved_match_in_profile_store.in_store = PasswordForm::Store::kProfileStore;

  PasswordForm psl_saved_match_in_profile_store(psl_saved_match_);
  psl_saved_match_in_profile_store.in_store =
      PasswordForm::Store::kProfileStore;

  PasswordForm psl_saved_match_in_account_store(psl_saved_match_);
  psl_saved_match_in_account_store.in_store =
      PasswordForm::Store::kAccountStore;

  SetNonFederatedAndNotifyFetchCompleted({saved_match_in_profile_store,
                                          psl_saved_match_in_profile_store,
                                          psl_saved_match_in_account_store});

  password_save_manager_impl()->CreatePendingCredentials(
      saved_match_in_profile_store, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_CALL(*mock_profile_form_saver(), Remove(saved_match_in_profile_store));
  EXPECT_CALL(*mock_profile_form_saver(),
              Remove(psl_saved_match_in_profile_store));
  EXPECT_CALL(*mock_account_form_saver(),
              Save(saved_match_in_profile_store, _, _));

  password_save_manager_impl()->MoveCredentialsToAccountStore(kTrigger);
}

TEST_F(MultiStorePasswordSaveManagerTest, BlockMovingWhenExistsInProfileStore) {
  signin::GaiaIdHash user1_id_hash =
      signin::GaiaIdHash::FromGaiaId("user1@gmail.com");
  signin::GaiaIdHash user2_id_hash =
      signin::GaiaIdHash::FromGaiaId("user2@gmail.com");

  PasswordForm profile_saved_match(saved_match_);
  profile_saved_match.username_value = parsed_submitted_form_.username_value;
  profile_saved_match.password_value = parsed_submitted_form_.password_value;
  profile_saved_match.in_store = PasswordForm::Store::kProfileStore;
  profile_saved_match.moving_blocked_for_list = {user1_id_hash};

  SetNonFederatedAndNotifyFetchCompleted({profile_saved_match});

  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  PasswordForm profile_updated_match(profile_saved_match);
  profile_updated_match.date_last_used =
      password_save_manager_impl()->GetPendingCredentials().date_last_used;
  profile_updated_match.moving_blocked_for_list.push_back(user2_id_hash);

  EXPECT_CALL(*mock_account_form_saver(), Update).Times(0);
  EXPECT_CALL(*mock_profile_form_saver(), Update(profile_updated_match, _, _));

  password_save_manager_impl()->BlockMovingToAccountStoreFor(user2_id_hash);
}

TEST_F(MultiStorePasswordSaveManagerTest, BlockMovingWhenExistsInBothStores) {
  signin::GaiaIdHash user1_id_hash =
      signin::GaiaIdHash::FromGaiaId("user1@gmail.com");
  signin::GaiaIdHash user2_id_hash =
      signin::GaiaIdHash::FromGaiaId("user2@gmail.com");

  PasswordForm account_saved_match(saved_match_);
  account_saved_match.username_value = parsed_submitted_form_.username_value;
  account_saved_match.password_value = parsed_submitted_form_.password_value;
  account_saved_match.in_store = PasswordForm::Store::kAccountStore;

  PasswordForm profile_saved_match(account_saved_match);
  profile_saved_match.in_store = PasswordForm::Store::kProfileStore;
  profile_saved_match.moving_blocked_for_list = {user1_id_hash};

  SetNonFederatedAndNotifyFetchCompleted({profile_saved_match});

  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  PasswordForm profile_updated_match(profile_saved_match);
  profile_updated_match.date_last_used =
      password_save_manager_impl()->GetPendingCredentials().date_last_used;
  profile_updated_match.moving_blocked_for_list.push_back(user2_id_hash);

  EXPECT_CALL(*mock_account_form_saver(), Update).Times(0);
  EXPECT_CALL(*mock_profile_form_saver(), Update(profile_updated_match, _, _));

  password_save_manager_impl()->BlockMovingToAccountStoreFor(user2_id_hash);
}

TEST_F(
    MultiStorePasswordSaveManagerTest,
    PresaveGeneratedPasswordInAccountStoreIfOptedInAndDefaultStoreIsAccount) {
  ON_CALL(*client()->GetPasswordFeatureManager(), IsOptedInForAccountStorage)
      .WillByDefault(Return(true));
  ON_CALL(*client()->GetPasswordFeatureManager(), GetDefaultPasswordStore)
      .WillByDefault(Return(PasswordForm::Store::kAccountStore));

  EXPECT_CALL(*mock_profile_form_saver(), Save).Times(0);
  EXPECT_CALL(*mock_account_form_saver(), Save);

  password_save_manager_impl()->PresaveGeneratedPassword(
      parsed_submitted_form_);
}

TEST_F(
    MultiStorePasswordSaveManagerTest,
    PresaveGeneratedPasswordInAccountStoreIfOptedInAndDefaultStoreIsProfile) {
  ON_CALL(*client()->GetPasswordFeatureManager(), IsOptedInForAccountStorage)
      .WillByDefault(Return(true));
  ON_CALL(*client()->GetPasswordFeatureManager(), GetDefaultPasswordStore)
      .WillByDefault(Return(PasswordForm::Store::kProfileStore));

  EXPECT_CALL(*mock_profile_form_saver(), Save).Times(0);
  EXPECT_CALL(*mock_account_form_saver(), Save);

  password_save_manager_impl()->PresaveGeneratedPassword(
      parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       PresaveGeneratedPasswordInProfileStoreIfOptedOutOfAccountStorage) {
  // Generation is offered only to users who are either syncing or have opted-in
  // for account store. Therefore, if the user isn't opted in, it is guaranteed
  // they are syncing and the password should be stored in the profile store.
  ON_CALL(*client()->GetPasswordFeatureManager(), IsOptedInForAccountStorage)
      .WillByDefault(Return(false));
  ON_CALL(*client()->GetPasswordFeatureManager(), GetDefaultPasswordStore)
      .WillByDefault(Return(PasswordForm::Store::kProfileStore));

  EXPECT_CALL(*mock_profile_form_saver(), Save);
  EXPECT_CALL(*mock_account_form_saver(), Save).Times(0);

  password_save_manager_impl()->PresaveGeneratedPassword(
      parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       GetPasswordStoreForSavingReturnsAccountForNewPasswordWhenEnabled) {
  SetAccountStoreEnabled(/*is_enabled=*/true);
  SetDefaultPasswordStore(PasswordForm::Store::kAccountStore);
  PasswordForm::Store store_to_save =
      password_save_manager_impl()->GetPasswordStoreForSaving(
          parsed_observed_form_);
  EXPECT_EQ(PasswordForm::Store::kAccountStore, store_to_save);
}

TEST_F(
    MultiStorePasswordSaveManagerTest,
    GetPasswordStoreForSavingReturnsProfileForNewPasswordWhenAccountDisabled) {
  SetAccountStoreEnabled(/*is_enabled=*/false);
  PasswordForm::Store store_to_save =
      password_save_manager_impl()->GetPasswordStoreForSaving(
          parsed_observed_form_);
  EXPECT_EQ(PasswordForm::Store::kProfileStore, store_to_save);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       GetPasswordStoreForSavingReturnsProfileWhenUpdatingInProfile) {
  SetAccountStoreEnabled(/*is_enabled=*/true);
  SetDefaultPasswordStore(PasswordForm::Store::kAccountStore);

  saved_match_.in_store = PasswordForm::Store::kProfileStore;
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  PasswordForm::Store store_to_save =
      password_save_manager_impl()->GetPasswordStoreForSaving(
          parsed_observed_form_);
  EXPECT_EQ(PasswordForm::Store::kProfileStore, store_to_save);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       GetPasswordStoreForSavingReturnsBothWhenCredentialDuplicated) {
  SetAccountStoreEnabled(/*is_enabled=*/true);
  SetDefaultPasswordStore(PasswordForm::Store::kAccountStore);

  saved_match_.in_store =
      PasswordForm::Store::kProfileStore | PasswordForm::Store::kAccountStore;
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  PasswordForm::Store store_to_save =
      password_save_manager_impl()->GetPasswordStoreForSaving(
          parsed_observed_form_);
  EXPECT_EQ(
      PasswordForm::Store::kProfileStore | PasswordForm::Store::kAccountStore,
      store_to_save);
}

TEST_F(
    MultiStorePasswordSaveManagerTest,
    GetPasswordStoreForSavingReturnsBothForUpdateInProfileWithGeneratedPassword) {
  SetAccountStoreEnabled(/*is_enabled=*/true);
  SetDefaultPasswordStore(PasswordForm::Store::kAccountStore);

  saved_match_.in_store = PasswordForm::Store::kProfileStore;
  SetNonFederatedAndNotifyFetchCompleted({saved_match_});

  password_save_manager_impl()->PresaveGeneratedPassword(parsed_observed_form_);
  PasswordForm::Store store_to_save =
      password_save_manager_impl()->GetPasswordStoreForSaving(
          parsed_observed_form_);
  EXPECT_EQ(
      PasswordForm::Store::kProfileStore | PasswordForm::Store::kAccountStore,
      store_to_save);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       GetPasswordStoreForSavingReturnsProfileWhenAccountDisabled) {
  SetAccountStoreEnabled(/*is_enabled=*/false);

  password_save_manager_impl()->PresaveGeneratedPassword(parsed_observed_form_);
  PasswordForm::Store store_to_save =
      password_save_manager_impl()->GetPasswordStoreForSaving(
          parsed_observed_form_);
  EXPECT_EQ(PasswordForm::Store::kProfileStore, store_to_save);
}

// Since conflicts in the profile store should not be taken into account during
// generation, below is a parameterized fixture to run the same tests for all 4
// combinations that can exist there (no matches, same username match, empty
// username match, and both).
class MultiStorePasswordSaveManagerGenerationConflictTest
    : public MultiStorePasswordSaveManagerTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  MultiStorePasswordSaveManagerGenerationConflictTest() {
    SetAccountStoreEnabled(/*is_enabled=*/true);
  }

  // Returns a password form using |saved_match_| with |username|, |password|
  // and |in_store|.
  PasswordForm CreateSavedMatch(const std::u16string& username,
                                const std::u16string& password,
                                const PasswordForm::Store in_store) const {
    PasswordForm form = saved_match_;
    form.username_value = username;
    form.password_value = password;
    form.in_store = in_store;
    return form;
  }

  // Returns at most two entries in the profile store, either with the same
  // username value as |username|, or an empty one.
  // The test parameters determine which of the conflicts should be included.
  std::vector<PasswordForm> CreateProfileStoreMatchesForTestParameters(
      const std::u16string& username) const {
    auto [add_same_username_match, add_empty_username_match] = GetParam();

    std::vector<PasswordForm> profile_store_matches;
    if (add_same_username_match) {
      profile_store_matches.push_back(CreateSavedMatch(
          username, u"password_for_same_username_match_in_profile",
          PasswordForm::Store::kProfileStore));
    }
    if (add_empty_username_match) {
      profile_store_matches.push_back(
          CreateSavedMatch(u"", u"password_for_empty_username_match_in_profile",
                           PasswordForm::Store::kProfileStore));
    }
    return profile_store_matches;
  }
};

TEST_P(MultiStorePasswordSaveManagerGenerationConflictTest,
       PresaveGeneratedPasswordWithNoMatchesInAccountStore) {
  std::vector<PasswordForm> matches =
      CreateProfileStoreMatchesForTestParameters(
          parsed_submitted_form_.username_value);
  SetNonFederatedAndNotifyFetchCompleted(matches);

  EXPECT_CALL(*mock_profile_form_saver(), Save).Times(0);
  // Presaving found no entry in the account store with the same username, so
  // stores the form as is.
  EXPECT_CALL(
      *mock_account_form_saver(),
      Save(MatchesUsernameAndPassword(parsed_submitted_form_.username_value,
                                      parsed_submitted_form_.password_value),
           _, _));

  password_save_manager_impl()->PresaveGeneratedPassword(
      parsed_submitted_form_);
}

TEST_P(MultiStorePasswordSaveManagerGenerationConflictTest,
       PresaveGeneratedPasswordWithSameUsernameMatchInAccountStore) {
  std::vector<PasswordForm> matches =
      CreateProfileStoreMatchesForTestParameters(
          parsed_submitted_form_.username_value);
  matches.push_back(
      CreateSavedMatch(parsed_submitted_form_.username_value,
                       u"password_for_same_username_conflict_in_account",
                       PasswordForm::Store::kAccountStore));
  SetNonFederatedAndNotifyFetchCompleted(matches);

  EXPECT_CALL(*mock_profile_form_saver(), Save).Times(0);
  // Presaving found an entry in the account store with the same username, so
  // stores the form with an empty username instead.
  EXPECT_CALL(*mock_account_form_saver(),
              Save(MatchesUsernameAndPassword(
                       u"", parsed_submitted_form_.password_value),
                   _, _));

  password_save_manager_impl()->PresaveGeneratedPassword(
      parsed_submitted_form_);
}

TEST_P(MultiStorePasswordSaveManagerGenerationConflictTest,
       PresaveGeneratedPasswordWithEmptyUsernameMatchInAccountStore) {
  std::vector<PasswordForm> matches =
      CreateProfileStoreMatchesForTestParameters(
          parsed_submitted_form_.username_value);
  matches.push_back(
      CreateSavedMatch(u"", u"password_for_empty_username_conflict_in_account",
                       PasswordForm::Store::kAccountStore));
  SetNonFederatedAndNotifyFetchCompleted(matches);

  EXPECT_CALL(*mock_profile_form_saver(), Save).Times(0);
  // Presaving found only an entry with an empty username in the account store,
  // so stores the form as is.
  EXPECT_CALL(
      *mock_account_form_saver(),
      Save(MatchesUsernameAndPassword(parsed_submitted_form_.username_value,
                                      parsed_submitted_form_.password_value),
           _, _));

  password_save_manager_impl()->PresaveGeneratedPassword(
      parsed_submitted_form_);
}

INSTANTIATE_TEST_SUITE_P(,
                         MultiStorePasswordSaveManagerGenerationConflictTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace password_manager
