// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_save_manager_impl.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/common/renderer_id.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "components/password_manager/core/browser/multi_store_password_save_manager.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/vote_uploads_test_matchers.h"
#include "components/password_manager/core/browser/votes_uploader.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::AutofillUploadContents;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormStructure;
using autofill::PasswordFormFillData;
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
using testing::UnorderedElementsAre;

namespace password_manager {

namespace {

// Indices of username and password fields in the observed form.
const int kUsernameFieldIndex = 1;
const int kPasswordFieldIndex = 2;

MATCHER_P(FormHasUniqueKey, key, "") {
  return ArePasswordFormUniqueKeysEqual(arg, key);
}

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
  EXPECT_TRUE(
      autofill::FormDataEqualForTesting(expected.form_data, actual.form_data));
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
  // FormSaver:
  MOCK_METHOD(PasswordForm,
              PermanentlyBlacklist,
              (PasswordStore::FormDigest),
              (override));
  MOCK_METHOD(void,
              Unblacklist,
              (const PasswordStore::FormDigest&),
              (override));
  MOCK_METHOD(void,
              Save,
              (PasswordForm pending,
               const std::vector<const PasswordForm*>& matches,
               const base::string16& old_password),
              (override));
  MOCK_METHOD(void,
              Update,
              (PasswordForm pending,
               const std::vector<const PasswordForm*>& matches,
               const base::string16& old_password),
              (override));
  MOCK_METHOD(void,
              UpdateReplace,
              (PasswordForm pending,
               const std::vector<const PasswordForm*>& matches,
               const base::string16& old_password,
               const PasswordForm& old_unique_key),
              (override));
  MOCK_METHOD(void, Remove, (const PasswordForm&), (override));

  std::unique_ptr<FormSaver> Clone() override {
    return std::make_unique<MockFormSaver>();
  }
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
  MOCK_METHOD(bool, IsCommittedMainFrameSecure, (), (const, override));
};

class MockAutofillDownloadManager : public autofill::AutofillDownloadManager {
 public:
  MockAutofillDownloadManager()
      : AutofillDownloadManager(nullptr, &fake_observer) {}

  MOCK_METHOD(bool,
              StartUploadRequest,
              (const FormStructure&,
               bool,
               const autofill::ServerFieldTypeSet&,
               const std::string&,
               bool,
               PrefService*),
              (override));

 private:
  class StubObserver : public AutofillDownloadManager::Observer {
    void OnLoadedServerPredictions(
        std::string response,
        const std::vector<autofill::FormSignature>& form_signatures) override {}
  };

  StubObserver fake_observer;
  DISALLOW_COPY_AND_ASSIGN(MockAutofillDownloadManager);
};

}  // namespace

class PasswordSaveManagerImplTest : public testing::Test,
                                    public testing::WithParamInterface<bool> {
 public:
  PasswordSaveManagerImplTest()
      : votes_uploader_(&client_, false /* is_possible_change_password_form */),
        task_runner_(new TestMockTimeTaskRunner) {
    GURL origin = GURL("https://accounts.google.com/a/ServiceLoginAuth");
    GURL action = GURL("https://accounts.google.com/a/ServiceLogin");
    GURL psl_origin = GURL("https://myaccounts.google.com/a/ServiceLoginAuth");
    GURL psl_action = GURL("https://myaccounts.google.com/a/ServiceLogin");

    observed_form_.url = origin;
    observed_form_.action = action;
    observed_form_.name = ASCIIToUTF16("sign-in");
    observed_form_.unique_renderer_id = autofill::FormRendererId(1);
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
    field.unique_renderer_id = autofill::FieldRendererId(5);
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

    fetcher_ = std::make_unique<FakeFormFetcher>();
    fetcher_->Fetch();

    metrics_recorder_ = base::MakeRefCounted<PasswordFormMetricsRecorder>(
        client_.IsCommittedMainFrameSecure(), client_.GetUkmSourceId(),
        /*pref_service=*/nullptr);
    auto mock_form_saver = std::make_unique<NiceMock<MockFormSaver>>();
    mock_form_saver_ = mock_form_saver.get();

    if (!GetParam()) {
      password_save_manager_impl_ =
          std::make_unique<PasswordSaveManagerImpl>(std::move(mock_form_saver));
    } else {
      password_save_manager_impl_ = std::make_unique<
          MultiStorePasswordSaveManager>(
          /*account_form_saver=*/std::move(mock_form_saver),
          /*account_form_saver=*/std::make_unique<NiceMock<MockFormSaver>>());
    }

    password_save_manager_impl_->Init(&client_, fetcher_.get(),
                                      metrics_recorder_, &votes_uploader_);

    ON_CALL(client_, GetAutofillDownloadManager())
        .WillByDefault(Return(&mock_autofill_download_manager_));
    ON_CALL(mock_autofill_download_manager_,
            StartUploadRequest(_, _, _, _, _, _))
        .WillByDefault(Return(true));
    ON_CALL(*client_.GetPasswordFeatureManager(), GetDefaultPasswordStore)
        .WillByDefault(Return(PasswordForm::Store::kProfileStore));
  }

  PasswordForm Parse(const FormData& form_data) {
    return *FormDataParser().Parse(form_data, FormDataParser::Mode::kSaving);
  }

  void DestroySaveManagerAndMetricsRecorder() {
    password_save_manager_impl_.reset();
    metrics_recorder_.reset();
  }

  MockPasswordManagerClient* client() { return &client_; }

  PasswordSaveManagerImpl* password_save_manager_impl() {
    return password_save_manager_impl_.get();
  }

  MockFormSaver* mock_form_saver() { return mock_form_saver_; }

  FakeFormFetcher* fetcher() { return fetcher_.get(); }

  PasswordFormMetricsRecorder* metrics_recorder() {
    return metrics_recorder_.get();
  }

  MockAutofillDownloadManager* mock_autofill_download_manager() {
    return &mock_autofill_download_manager_;
  }

  TestMockTimeTaskRunner* task_runner() { return task_runner_.get(); }

  void SetNonFederatedAndNotifyFetchCompleted(
      const std::vector<const PasswordForm*>& non_federated) {
    fetcher()->SetNonFederated(non_federated);
    fetcher()->NotifyFetchCompleted();
  }

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
  NiceMock<MockFormSaver>* mock_form_saver_;
  NiceMock<MockAutofillDownloadManager> mock_autofill_download_manager_;

  DISALLOW_COPY_AND_ASSIGN(PasswordSaveManagerImplTest);
};

TEST_P(PasswordSaveManagerImplTest, PermanentlyBlacklist) {
  PasswordStore::FormDigest form_digest(PasswordForm::Scheme::kDigest,
                                        "www.example.com", GURL("www.abc.com"));
  EXPECT_CALL(*mock_form_saver(), PermanentlyBlacklist(form_digest));
  password_save_manager_impl()->PermanentlyBlacklist(form_digest);
}

TEST_P(PasswordSaveManagerImplTest, Unblacklist) {
  PasswordStore::FormDigest form_digest(PasswordForm::Scheme::kDigest,
                                        "www.example.com", GURL("www.abc.com"));
  EXPECT_CALL(*mock_form_saver(), Unblacklist(form_digest));
  password_save_manager_impl()->Unblacklist(form_digest);
}

// Tests creating pending credentials when the password store is empty.
TEST_P(PasswordSaveManagerImplTest, CreatePendingCredentialsEmptyStore) {
  fetcher()->NotifyFetchCompleted();

  const base::Time kNow = base::Time::Now();
  password_save_manager_impl()->CreatePendingCredentials(
      Parse(submitted_form_), &observed_form_, submitted_form_,
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
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});
  password_save_manager_impl()->CreatePendingCredentials(
      Parse(submitted_form_), &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  CheckPendingCredentials(
      parsed_submitted_form_,
      password_save_manager_impl()->GetPendingCredentials());
}

// Tests that when submitted credentials are equal to already saved one then
// pending credentials equal to saved match.
TEST_P(PasswordSaveManagerImplTest, CreatePendingCredentialsAlreadySaved) {
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  submitted_form_.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  submitted_form_.fields[kPasswordFieldIndex].value =
      saved_match_.password_value;

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
  saved_match_.is_public_suffix_match = true;

  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  submitted_form_.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  submitted_form_.fields[kPasswordFieldIndex].value =
      saved_match_.password_value;

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
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  PasswordForm expected = saved_match_;
  expected.password_value += ASCIIToUTF16("1");

  submitted_form_.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;
  submitted_form_.fields[kPasswordFieldIndex].value = expected.password_value;

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
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = ASCIIToUTF16("strongpassword");
  submitted_form.fields[1].value = ASCIIToUTF16("verystrongpassword");

  PasswordForm expected = saved_match_;
  expected.password_value = ASCIIToUTF16("verystrongpassword");

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
  another_saved_match.username_value += ASCIIToUTF16("1");
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_, &another_saved_match});

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = ASCIIToUTF16("strongpassword");
  submitted_form.fields[1].value = ASCIIToUTF16("verystrongpassword");

  PasswordForm expected = saved_match_;
  expected.password_value = ASCIIToUTF16("verystrongpassword");
  password_save_manager_impl()->CreatePendingCredentials(
      Parse(submitted_form), &observed_form_, submitted_form,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  CheckPendingCredentials(
      expected, password_save_manager_impl()->GetPendingCredentials());
}

// Tests creating pending credentials when the password field has an empty name.
TEST_P(PasswordSaveManagerImplTest, CreatePendingCredentialsEmptyName) {
  fetcher()->NotifyFetchCompleted();

  FormData anonymous_signup = observed_form_;
  // There is an anonymous password field and set it as the new password field.
  anonymous_signup.fields[2].name.clear();
  anonymous_signup.fields[2].value = ASCIIToUTF16("a password");
  anonymous_signup.fields[2].autocomplete_attribute = "new-password";

  password_save_manager_impl()->CreatePendingCredentials(
      Parse(anonymous_signup), &observed_form_, anonymous_signup,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_EQ(
      ASCIIToUTF16("a password"),
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
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  FormData submitted_form = observed_form_;
  base::string16 new_username = saved_match_.username_value + ASCIIToUTF16("1");
  base::string16 new_password = saved_match_.password_value + ASCIIToUTF16("1");
  submitted_form.fields[kUsernameFieldIndex].value = new_username;
  submitted_form.fields[kPasswordFieldIndex].value = new_password;

  password_save_manager_impl()->CreatePendingCredentials(
      Parse(submitted_form), &observed_form_, submitted_form,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_TRUE(password_save_manager_impl()->IsNewLogin());

  PasswordForm saved_form;
  std::vector<const PasswordForm*> best_matches;
  EXPECT_CALL(*mock_form_saver(), Save(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&saved_form), SaveArg<1>(&best_matches)));

  password_save_manager_impl()->Save(&observed_form_, Parse(submitted_form));

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
  SetNonFederatedAndNotifyFetchCompleted({&psl_saved_match_});

  FormData submitted_form = observed_form_;
  submitted_form.fields[kUsernameFieldIndex].value =
      psl_saved_match_.username_value;
  submitted_form.fields[kPasswordFieldIndex].value =
      psl_saved_match_.password_value;

  password_save_manager_impl()->CreatePendingCredentials(
      Parse(submitted_form), &observed_form_, submitted_form,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_TRUE(password_save_manager_impl()->IsNewLogin());
  EXPECT_TRUE(password_save_manager_impl()
                  ->GetPendingCredentials()
                  .is_public_suffix_match);

  PasswordForm saved_form;
  std::vector<const PasswordForm*> best_matches;
  EXPECT_CALL(*mock_form_saver(), Save(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&saved_form), SaveArg<1>(&best_matches)));

  password_save_manager_impl()->Save(&observed_form_, Parse(submitted_form));

  EXPECT_EQ(submitted_form.url, saved_form.url);
  EXPECT_EQ(GetSignonRealm(submitted_form.url), saved_form.signon_realm);
  EXPECT_EQ(psl_saved_match_.username_value, saved_form.username_value);
  EXPECT_EQ(psl_saved_match_.password_value, saved_form.password_value);
  EXPECT_EQ(psl_saved_match_.username_element, saved_form.username_element);
  EXPECT_EQ(psl_saved_match_.password_element, saved_form.password_element);

  EXPECT_EQ(std::vector<const PasswordForm*>{&psl_saved_match_}, best_matches);
}

// Tests that when credentials with already saved username but with a new
// password are submitted, then the saved password is updated.
TEST_P(PasswordSaveManagerImplTest, OverridePassword) {
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  FormData submitted_form = observed_form_;
  base::string16 username = saved_match_.username_value;
  base::string16 new_password = saved_match_.password_value + ASCIIToUTF16("1");
  submitted_form.fields[kUsernameFieldIndex].value = username;
  submitted_form.fields[kPasswordFieldIndex].value = new_password;

  password_save_manager_impl()->CreatePendingCredentials(
      Parse(submitted_form), &observed_form_, submitted_form,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_FALSE(password_save_manager_impl()->IsNewLogin());
  EXPECT_TRUE(password_save_manager_impl()->IsPasswordUpdate());

  PasswordForm updated_form;
  EXPECT_CALL(*mock_form_saver(), Update(_, ElementsAre(Pointee(saved_match_)),
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
  saved_match_another_username.username_value += ASCIIToUTF16("1");

  SetNonFederatedAndNotifyFetchCompleted(
      {&saved_match_, &not_best_saved_match, &saved_match_another_username});

  FormData submitted_form = observed_form_only_password_fields_;
  submitted_form.fields[0].value = saved_match_.password_value;
  base::string16 new_password = saved_match_.password_value + ASCIIToUTF16("1");
  submitted_form.fields[1].value = new_password;

  password_save_manager_impl()->CreatePendingCredentials(
      Parse(submitted_form), &observed_form_only_password_fields_,
      submitted_form,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_FALSE(password_save_manager_impl()->IsNewLogin());
  EXPECT_TRUE(password_save_manager_impl()->IsPasswordUpdate());

  PasswordForm updated_form;
  EXPECT_CALL(*mock_form_saver(),
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

  base::string16 user_chosen_username = ASCIIToUTF16("user_chosen_username");
  base::string16 automatically_chosen_username =
      ASCIIToUTF16("automatically_chosen_username");
  submitted_form_.fields[0].value = user_chosen_username;
  submitted_form_.fields[1].value = automatically_chosen_username;
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
}

TEST_P(PasswordSaveManagerImplTest, UpdateUsernameToAlreadyExisting) {
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});
  PasswordForm parsed_submitted_form = Parse(submitted_form_);
  password_save_manager_impl()->CreatePendingCredentials(
      parsed_submitted_form, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  base::string16 new_username = saved_match_.username_value;
  base::string16 expected_password = parsed_submitted_form_.password_value;
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

  base::string16 new_password =
      parsed_submitted_form_.password_value + ASCIIToUTF16("1");
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

  // TODO(https://crbug.com/928690): implement not sending incorrect votes and
  // check that StartUploadRequest is not called.
  EXPECT_CALL(*mock_autofill_download_manager(),
              StartUploadRequest(_, _, _, _, _, _))
      .Times(1);
  password_save_manager_impl()->Save(&observed_form_, parsed_submitted_form);
}

TEST_P(PasswordSaveManagerImplTest, UpdatePasswordValueToAlreadyExisting) {
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  // Emulate submitting form with known username and different password.
  submitted_form_.fields[kUsernameFieldIndex].value =
      saved_match_.username_value;

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
  base::string16 password = ASCIIToUTF16("password1");
  base::string16 pin = ASCIIToUTF16("pin");
  submitted_form.fields[0].value = password;
  submitted_form.fields[1].value = pin;
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
  expected.password_element = submitted_form.fields[0].name;

  // Simulate that the user updates value to save for the first password field
  // using the update prompt.
  parsed_submitted_form.password_value = password;
  parsed_submitted_form.password_element = submitted_form.fields[0].name;
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
  std::map<base::string16, autofill::ServerFieldType> expected_types;
  expected_types[expected.password_element] = autofill::PASSWORD;

  EXPECT_CALL(*mock_autofill_download_manager(),
              StartUploadRequest(UploadedAutofillTypesAre(expected_types),
                                 false, _, _, true, nullptr));

  // Check that the password which was chosen by the user is saved.
  PasswordForm saved_form;
  EXPECT_CALL(*mock_form_saver(), Save(_, _, _))
      .WillOnce(SaveArg<0>(&saved_form));

  password_save_manager_impl()->Save(&observed_form_, parsed_submitted_form);
  CheckPendingCredentials(expected, saved_form);
}

TEST_P(PasswordSaveManagerImplTest, PresaveGeneratedPasswordEmptyStore) {
  fetcher()->NotifyFetchCompleted();

  EXPECT_FALSE(password_save_manager_impl()->HasGeneratedPassword());

  // Check that the generated password is presaved.
  PasswordForm saved_form;
  EXPECT_CALL(*mock_form_saver(), Save(_, IsEmpty(), base::string16()))
      .WillOnce(SaveArg<0>(&saved_form));

  PasswordForm form_with_generated_password = parsed_submitted_form_;

  password_save_manager_impl()->PresaveGeneratedPassword(
      form_with_generated_password);

  EXPECT_TRUE(password_save_manager_impl()->HasGeneratedPassword());
  EXPECT_EQ(saved_form.username_value,
            form_with_generated_password.username_value);
  EXPECT_EQ(saved_form.password_value,
            form_with_generated_password.password_value);

  Mock::VerifyAndClearExpectations(mock_form_saver());

  // Check that when the generated password is edited, then it's presaved.
  form_with_generated_password.password_value += ASCIIToUTF16("1");
  EXPECT_CALL(*mock_form_saver(),
              UpdateReplace(_, IsEmpty(), ASCIIToUTF16(""),
                            FormHasUniqueKey(form_with_generated_password)))
      .WillOnce(SaveArg<0>(&saved_form));

  password_save_manager_impl()->PresaveGeneratedPassword(
      form_with_generated_password);

  EXPECT_TRUE(password_save_manager_impl()->HasGeneratedPassword());
  EXPECT_EQ(saved_form.username_value,
            form_with_generated_password.username_value);
  EXPECT_EQ(saved_form.password_value,
            form_with_generated_password.password_value);

  Mock::VerifyAndClearExpectations(mock_form_saver());
}

TEST_P(PasswordSaveManagerImplTest, PresaveGenerated_ModifiedUsername) {
  fetcher()->NotifyFetchCompleted();

  // Check that the generated password is presaved.
  PasswordForm saved_form;
  EXPECT_CALL(*mock_form_saver(), Save(_, _, _))
      .WillOnce(SaveArg<0>(&saved_form));
  PasswordForm form_with_generated_password = parsed_submitted_form_;

  password_save_manager_impl()->PresaveGeneratedPassword(
      form_with_generated_password);

  Mock::VerifyAndClearExpectations(mock_form_saver());

  // Check that when the username is edited, then it's presaved.
  form_with_generated_password.username_value += ASCIIToUTF16("1");

  EXPECT_CALL(*mock_form_saver(), UpdateReplace(_, IsEmpty(), ASCIIToUTF16(""),
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
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_});

  // Check that the generated password is presaved.
  PasswordForm saved_form;
  EXPECT_CALL(*mock_form_saver(), Save(_, _, _))
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
  EXPECT_CALL(*mock_form_saver(), Save(_, _, _));
  PasswordForm submitted_form(parsed_observed_form_);
  submitted_form.password_value = ASCIIToUTF16("password");
  password_save_manager_impl()->PresaveGeneratedPassword(submitted_form);
  EXPECT_CALL(*mock_form_saver(), Remove(_));
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

  submitted_form.password_value += ASCIIToUTF16("1");

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

  submitted_form.password_value += ASCIIToUTF16("2");

  password_save_manager_impl()->PresaveGeneratedPassword(submitted_form);

  password_save_manager_impl()->PasswordNoLongerGenerated();

  DestroySaveManagerAndMetricsRecorder();
  histogram_tester.ExpectUniqueSample("PasswordGeneration.UserDecision",
                                      GeneratedPasswordStatus::kPasswordDeleted,
                                      1);
}

TEST_P(PasswordSaveManagerImplTest, Update) {
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

  password_save_manager_impl()->CreatePendingCredentials(
      Parse(submitted_form), &observed_form_, submitted_form,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  PasswordForm updated_form;
  EXPECT_CALL(
      *mock_form_saver(),
      Update(_,
             UnorderedElementsAre(Pointee(saved_match_),
                                  Pointee(saved_match_another_username)),
             saved_match_.password_value))
      .WillOnce(SaveArg<0>(&updated_form));

  const base::Time kNow = base::Time::Now();

  password_save_manager_impl()->Update(saved_match_, &observed_form_,
                                       Parse(submitted_form));

  EXPECT_TRUE(ArePasswordFormUniqueKeysEqual(saved_match_, updated_form));
  EXPECT_EQ(new_password, updated_form.password_value);
  EXPECT_GE(updated_form.date_last_used, kNow);
}

TEST_P(PasswordSaveManagerImplTest, HTTPAuthPasswordOverridden) {
  PasswordForm http_auth_form = parsed_observed_form_;
  http_auth_form.scheme = PasswordForm::Scheme::kBasic;
  fetcher()->set_scheme(PasswordForm::Scheme::kBasic);

  PasswordForm saved_http_auth_form = http_auth_form;
  const base::string16 username = ASCIIToUTF16("user1");
  const base::string16 password = ASCIIToUTF16("pass1");
  saved_http_auth_form.username_value = username;
  saved_http_auth_form.password_value = password;

  SetNonFederatedAndNotifyFetchCompleted({&saved_http_auth_form});

  // Check that if new password is submitted, then |form_manager_| is in state
  // password overridden.
  PasswordForm submitted_http_auth_form = saved_http_auth_form;
  base::string16 new_password = password + ASCIIToUTF16("1");
  submitted_http_auth_form.password_value = new_password;

  password_save_manager_impl()->CreatePendingCredentials(
      submitted_http_auth_form, &observed_form_, submitted_form_,
      /*is_http_auth=*/true,
      /*is_credential_api_save=*/false);

  EXPECT_FALSE(password_save_manager_impl()->IsNewLogin());
  EXPECT_TRUE(password_save_manager_impl()->IsPasswordUpdate());

  // Check that the password is updated in the stored credential.
  PasswordForm updated_form;
  EXPECT_CALL(*mock_form_saver(),
              Update(_, ElementsAre(Pointee(saved_http_auth_form)), password))
      .WillOnce(SaveArg<0>(&updated_form));

  password_save_manager_impl()->Save(&observed_form_, submitted_http_auth_form);

  EXPECT_TRUE(
      ArePasswordFormUniqueKeysEqual(saved_http_auth_form, updated_form));
  EXPECT_EQ(new_password, updated_form.password_value);
}

INSTANTIATE_TEST_SUITE_P(All,
                         PasswordSaveManagerImplTest,
                         testing::Values(false, true));

}  // namespace password_manager
