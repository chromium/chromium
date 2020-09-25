// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/multi_store_password_save_manager.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/common/renderer_id.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/votes_uploader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::FormData;
using autofill::FormFieldData;
using base::ASCIIToUTF16;
using testing::_;
using testing::NiceMock;
using testing::Return;

namespace password_manager {

namespace {

MATCHER_P2(MatchesUsernameAndPassword, username, password, "") {
  return arg.username_value == username && arg.password_value == password;
}

// Indices of username and password fields in the observed form.
const int kUsernameFieldIndex = 1;
const int kPasswordFieldIndex = 2;

const auto kTrigger = metrics_util::MoveToAccountStoreTrigger::
    kSuccessfulLoginWithProfileStorePassword;

}  // namespace

class MockFormSaver : public StubFormSaver {
 public:
  MockFormSaver() = default;

  ~MockFormSaver() override = default;

  // FormSaver:
  MOCK_METHOD1(PermanentlyBlacklist, PasswordForm(PasswordStore::FormDigest));
  MOCK_METHOD1(Unblacklist, void(const PasswordStore::FormDigest&));
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

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFormSaver);
};

class MultiStorePasswordSaveManagerTest : public testing::Test {
 public:
  MultiStorePasswordSaveManagerTest()
      : votes_uploader_(&client_,
                        false /* is_possible_change_password_form */) {
    GURL origin = GURL("https://accounts.google.com/a/ServiceLoginAuth");
    GURL action = GURL("https://accounts.google.com/a/ServiceLogin");
    GURL psl_origin = GURL("https://myaccounts.google.com/a/ServiceLoginAuth");
    GURL psl_action = GURL("https://myaccounts.google.com/a/ServiceLogin");

    observed_form_.url = origin;
    observed_form_.action = action;
    observed_form_.name = ASCIIToUTF16("sign-in");
    observed_form_.unique_renderer_id = autofill::FormRendererId(1);
    observed_form_.is_form_tag = true;

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

    auto mock_profile_form_saver = std::make_unique<NiceMock<MockFormSaver>>();
    mock_profile_form_saver_ = mock_profile_form_saver.get();

    auto mock_account_form_saver = std::make_unique<NiceMock<MockFormSaver>>();
    mock_account_form_saver_ = mock_account_form_saver.get();

    password_save_manager_ = std::make_unique<MultiStorePasswordSaveManager>(
        std::move(mock_profile_form_saver), std::move(mock_account_form_saver));
    password_save_manager_->Init(&client_, fetcher_.get(), metrics_recorder_,
                                 &votes_uploader_);
  }

  void SetNonFederatedAndNotifyFetchCompleted(
      const std::vector<const PasswordForm*>& non_federated) {
    fetcher_->SetNonFederated(non_federated);
    fetcher_->NotifyFetchCompleted();
  }

  void SetFederatedAndNotifyFetchCompleted(
      const std::vector<const PasswordForm*>& federated) {
    fetcher_->set_federated(federated);
    fetcher_->NotifyFetchCompleted();
  }

  void SetAccountStoreEnabled(bool is_enabled) {
    ON_CALL(*client()->GetPasswordFeatureManager(),
            IsOptedInForAccountStorage())
        .WillByDefault(Return(is_enabled));
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
        url::Origin::Create(GURL("https://google.com/"));
    federated.username_value = ASCIIToUTF16("federated_username");
    return federated;
  }

  StubPasswordManagerClient* client() { return &client_; }
  MockFormSaver* mock_account_form_saver() { return mock_account_form_saver_; }
  MockFormSaver* mock_profile_form_saver() { return mock_profile_form_saver_; }
  FakeFormFetcher* fetcher() { return fetcher_.get(); }
  MultiStorePasswordSaveManager* password_save_manager() {
    return password_save_manager_.get();
  }

  FormData observed_form_;
  FormData submitted_form_;
  PasswordForm saved_match_;
  PasswordForm psl_saved_match_;
  PasswordForm parsed_observed_form_;
  PasswordForm parsed_submitted_form_;

 private:
  StubPasswordManagerClient client_;
  VotesUploader votes_uploader_;
  scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder_;

  // Define |fetcher_| before |password_save_manager_|, because the former
  // needs to outlive the latter.
  std::unique_ptr<FakeFormFetcher> fetcher_;
  std::unique_ptr<MultiStorePasswordSaveManager> password_save_manager_;
  NiceMock<MockFormSaver>* mock_account_form_saver_;
  NiceMock<MockFormSaver>* mock_profile_form_saver_;

  DISALLOW_COPY_AND_ASSIGN(MultiStorePasswordSaveManagerTest);
};

TEST_F(MultiStorePasswordSaveManagerTest,
       SaveInAccountStoreWhenAccountStoreEnabled) {
  SetAccountStoreEnabled(/*is_enabled=*/true);

  fetcher()->NotifyFetchCompleted();

  SetDefaultPasswordStore(PasswordForm::Store::kAccountStore);

  password_save_manager()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_TRUE(password_save_manager()->IsNewLogin());

  EXPECT_CALL(*mock_profile_form_saver(), Save(_, _, _)).Times(0);
  EXPECT_CALL(*mock_account_form_saver(), Save(_, _, _));

  password_save_manager()->Save(&observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       DoNotSaveInAccountStoreWhenAccountStoreDisabled) {
  SetAccountStoreEnabled(/*is_enabled=*/false);

  fetcher()->NotifyFetchCompleted();

  SetDefaultPasswordStore(PasswordForm::Store::kAccountStore);

  password_save_manager()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_TRUE(password_save_manager()->IsNewLogin());

  EXPECT_CALL(*mock_profile_form_saver(), Save(_, _, _)).Times(0);
  EXPECT_CALL(*mock_account_form_saver(), Save(_, _, _)).Times(0);

  password_save_manager()->Save(&observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest, SaveInProfileStore) {
  SetAccountStoreEnabled(/*is_enabled=*/true);

  fetcher()->NotifyFetchCompleted();

  SetDefaultPasswordStore(PasswordForm::Store::kProfileStore);

  password_save_manager()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_TRUE(password_save_manager()->IsNewLogin());

  EXPECT_CALL(*mock_profile_form_saver(), Save(_, _, _));
  EXPECT_CALL(*mock_account_form_saver(), Save(_, _, _)).Times(0);

  password_save_manager()->Save(&observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest, UpdateInAccountStoreOnly) {
  SetAccountStoreEnabled(/*is_enabled=*/true);

  PasswordForm saved_match_in_account_store(saved_match_);
  saved_match_in_account_store.username_value =
      parsed_submitted_form_.username_value;
  saved_match_in_account_store.in_store = PasswordForm::Store::kAccountStore;
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_in_account_store});

  password_save_manager()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_FALSE(password_save_manager()->IsNewLogin());
  // An update prompt should be shown.
  EXPECT_TRUE(password_save_manager()->IsPasswordUpdate());

  EXPECT_CALL(*mock_profile_form_saver(), Update(_, _, _)).Times(0);
  EXPECT_CALL(*mock_account_form_saver(), Update(_, _, _));

  password_save_manager()->Save(&observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest, UpdateInProfileStoreOnly) {
  SetAccountStoreEnabled(/*is_enabled=*/true);

  PasswordForm saved_match_in_profile_store(saved_match_);
  saved_match_in_profile_store.username_value =
      parsed_submitted_form_.username_value;
  saved_match_in_profile_store.in_store = PasswordForm::Store::kProfileStore;
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_in_profile_store});

  password_save_manager()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_FALSE(password_save_manager()->IsNewLogin());
  // An update prompt should be shown.
  EXPECT_TRUE(password_save_manager()->IsPasswordUpdate());

  EXPECT_CALL(*mock_profile_form_saver(), Update(_, _, _));
  EXPECT_CALL(*mock_account_form_saver(), Update(_, _, _)).Times(0);

  password_save_manager()->Save(&observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest, UpdateInBothStores) {
  // This test assumes that all fields of the PasswordForm in both stores are
  // equal except the |moving_blocked_for_list|. The reason for that is:
  // 1. |moving_blocked_for_list| is the most probable field to have different
  //    values since it's always empty in the account store.
  // 2. Other fields (e.g. |times_used|) are less critical and should be fine if
  //    the value in one store overrides the value in the other one.

  SetAccountStoreEnabled(/*is_enabled=*/true);

  PasswordForm saved_match_in_account_store(saved_match_);
  saved_match_in_account_store.username_value =
      parsed_submitted_form_.username_value;
  saved_match_in_account_store.in_store = PasswordForm::Store::kAccountStore;
  PasswordForm saved_match_in_profile_store(saved_match_in_account_store);
  saved_match_in_profile_store.in_store = PasswordForm::Store::kProfileStore;
  autofill::GaiaIdHash user_id_hash =
      autofill::GaiaIdHash::FromGaiaId("user@gmail.com");
  saved_match_in_profile_store.moving_blocked_for_list.push_back(user_id_hash);

  SetNonFederatedAndNotifyFetchCompleted(
      {&saved_match_in_profile_store, &saved_match_in_account_store});

  password_save_manager()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_FALSE(password_save_manager()->IsNewLogin());
  // An update prompt should be shown.
  EXPECT_TRUE(password_save_manager()->IsPasswordUpdate());

  // Both stores should be updated in the following ways:
  // 1. |password_value| is updated.
  // 2. |times_used| is incremented.
  // 3. |date_last_used| is updated.
  // 4. |in_store| field is irrelevant since it's not persisted.
  // 5. The rest of fields are taken aribtrary from one store.
  PasswordForm expected_profile_updated_form(saved_match_in_profile_store);
  expected_profile_updated_form.password_value =
      parsed_submitted_form_.password_value;
  expected_profile_updated_form.times_used++;
  expected_profile_updated_form.date_last_used =
      password_save_manager()->GetPendingCredentials().date_last_used;
  expected_profile_updated_form.in_store =
      password_save_manager()->GetPendingCredentials().in_store;

  PasswordForm expected_account_updated_form(saved_match_in_account_store);
  expected_account_updated_form.password_value =
      parsed_submitted_form_.password_value;
  expected_account_updated_form.times_used++;
  expected_account_updated_form.date_last_used =
      password_save_manager()->GetPendingCredentials().date_last_used;
  expected_account_updated_form.in_store =
      password_save_manager()->GetPendingCredentials().in_store;

  EXPECT_CALL(*mock_profile_form_saver(),
              Update(expected_profile_updated_form, _, _));
  EXPECT_CALL(*mock_account_form_saver(),
              Update(expected_account_updated_form, _, _));

  password_save_manager()->Save(&observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest, AutomaticSaveInBothStores) {
  SetAccountStoreEnabled(/*is_enabled=*/true);

  // Set different values for the fields that should be preserved per store
  // (namely: date_created, date_synced, times_used, moving_blocked_for_list)
  PasswordForm saved_match_in_profile_store(saved_match_);
  saved_match_in_profile_store.username_value =
      parsed_submitted_form_.username_value;
  saved_match_in_profile_store.password_value =
      parsed_submitted_form_.password_value;
  saved_match_in_profile_store.in_store = PasswordForm::Store::kProfileStore;
  saved_match_in_profile_store.date_created =
      base::Time::Now() - base::TimeDelta::FromDays(10);
  saved_match_in_profile_store.times_used = 10;
  saved_match_in_profile_store.moving_blocked_for_list.push_back(
      autofill::GaiaIdHash::FromGaiaId("email@gmail.com"));

  PasswordForm saved_match_in_account_store(saved_match_in_profile_store);
  saved_match_in_account_store.in_store = PasswordForm::Store::kAccountStore;
  saved_match_in_account_store.date_created = base::Time::Now();
  saved_match_in_account_store.date_synced = base::Time::Now();
  saved_match_in_account_store.times_used = 5;
  saved_match_in_account_store.moving_blocked_for_list.clear();

  SetNonFederatedAndNotifyFetchCompleted(
      {&saved_match_in_profile_store, &saved_match_in_account_store});

  password_save_manager()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  // No save or update prompts should be shown.
  EXPECT_FALSE(password_save_manager()->IsNewLogin());
  EXPECT_FALSE(password_save_manager()->IsPasswordUpdate());

  // We still should update both credentials to update the |date_last_used| and
  // |times_used|. Note that |in_store| is irrelevant since it's not persisted.
  // All other fields should be preserved.
  PasswordForm expected_profile_update_form(saved_match_in_profile_store);
  expected_profile_update_form.times_used++;
  expected_profile_update_form.date_last_used =
      password_save_manager()->GetPendingCredentials().date_last_used;
  expected_profile_update_form.in_store =
      password_save_manager()->GetPendingCredentials().in_store;

  PasswordForm expected_account_update_form(saved_match_in_account_store);
  expected_account_update_form.times_used++;
  expected_account_update_form.date_last_used =
      password_save_manager()->GetPendingCredentials().date_last_used;
  expected_account_update_form.in_store =
      password_save_manager()->GetPendingCredentials().in_store;

  EXPECT_CALL(*mock_profile_form_saver(),
              Update(expected_profile_update_form, _, _));
  EXPECT_CALL(*mock_account_form_saver(),
              Update(expected_account_update_form, _, _));

  password_save_manager()->Save(&observed_form_, parsed_submitted_form_);
}

// Since conflicts in the profile store should not be taken into account during
// generation, below is a parameterized fixture to run the same tests for all 4
// combinations that can exist there (no matches, same username match, empty
// username match, and both).
class MultiStorePasswordSaveManagerTestGenerationConflictWithAccountStoreEnabled
    : public MultiStorePasswordSaveManagerTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  MultiStorePasswordSaveManagerTestGenerationConflictWithAccountStoreEnabled() {
    SetAccountStoreEnabled(/*is_enabled=*/true);
  }

  // Returns a password form using |saved_match_| with |username|, |password|
  // and |in_store|.
  PasswordForm CreateSavedMatch(const base::string16& username,
                                const base::string16& password,
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
      const base::string16& username) const {
    bool add_same_username_match, add_empty_username_match;
    std::tie(add_same_username_match, add_empty_username_match) = GetParam();

    std::vector<PasswordForm> profile_store_matches;
    if (add_same_username_match) {
      profile_store_matches.push_back(CreateSavedMatch(
          username,
          base::ASCIIToUTF16("password_for_same_username_match_in_profile"),
          PasswordForm::Store::kProfileStore));
    }
    if (add_empty_username_match) {
      profile_store_matches.push_back(CreateSavedMatch(
          ASCIIToUTF16(""),
          base::ASCIIToUTF16("password_for_empty_username_match_in_profile"),
          PasswordForm::Store::kProfileStore));
    }
    return profile_store_matches;
  }

  // Helper function used because SetNonFederatedAndNotifyFetchCompleted() needs
  // a vector of pointers.
  std::vector<const PasswordForm*> GetFormPointers(
      const std::vector<PasswordForm>& forms) const {
    std::vector<const PasswordForm*> pointers_to_forms;
    for (const auto& form : forms) {
      pointers_to_forms.push_back(&form);
    }
    return pointers_to_forms;
  }
};

TEST_P(
    MultiStorePasswordSaveManagerTestGenerationConflictWithAccountStoreEnabled,
    PresaveGeneratedPasswordWithNoMatchesInAccountStore) {
  std::vector<PasswordForm> matches =
      CreateProfileStoreMatchesForTestParameters(
          parsed_submitted_form_.username_value);
  SetNonFederatedAndNotifyFetchCompleted(GetFormPointers(matches));

  EXPECT_CALL(*mock_profile_form_saver(), Save(_, _, _)).Times(0);
  // Presaving found no entry in the account store with the same username, so
  // stores the form as is.
  EXPECT_CALL(
      *mock_account_form_saver(),
      Save(MatchesUsernameAndPassword(parsed_submitted_form_.username_value,
                                      parsed_submitted_form_.password_value),
           _, _));

  password_save_manager()->PresaveGeneratedPassword(parsed_submitted_form_);
}

TEST_P(
    MultiStorePasswordSaveManagerTestGenerationConflictWithAccountStoreEnabled,
    PresaveGeneratedPasswordWithSameUsernameMatchInAccountStore) {
  std::vector<PasswordForm> matches =
      CreateProfileStoreMatchesForTestParameters(
          parsed_submitted_form_.username_value);
  matches.push_back(CreateSavedMatch(
      parsed_submitted_form_.username_value,
      base::ASCIIToUTF16("password_for_same_username_conflict_in_account"),
      PasswordForm::Store::kAccountStore));
  SetNonFederatedAndNotifyFetchCompleted(GetFormPointers(matches));

  EXPECT_CALL(*mock_profile_form_saver(), Save(_, _, _)).Times(0);
  // Presaving found an entry in the account store with the same username, so
  // stores the form with an empty username instead.
  EXPECT_CALL(
      *mock_account_form_saver(),
      Save(MatchesUsernameAndPassword(base::ASCIIToUTF16(""),
                                      parsed_submitted_form_.password_value),
           _, _));

  password_save_manager()->PresaveGeneratedPassword(parsed_submitted_form_);
}

TEST_P(
    MultiStorePasswordSaveManagerTestGenerationConflictWithAccountStoreEnabled,
    PresaveGeneratedPasswordWithEmptyUsernameMatchInAccountStore) {
  std::vector<PasswordForm> matches =
      CreateProfileStoreMatchesForTestParameters(
          parsed_submitted_form_.username_value);
  matches.push_back(CreateSavedMatch(
      base::ASCIIToUTF16(""),
      base::ASCIIToUTF16("password_for_empty_username_conflict_in_account"),
      PasswordForm::Store::kAccountStore));
  SetNonFederatedAndNotifyFetchCompleted(GetFormPointers(matches));

  EXPECT_CALL(*mock_profile_form_saver(), Save(_, _, _)).Times(0);
  // Presaving found only an entry with an empty username in the account store,
  // so stores the form as is.
  EXPECT_CALL(
      *mock_account_form_saver(),
      Save(MatchesUsernameAndPassword(parsed_submitted_form_.username_value,
                                      parsed_submitted_form_.password_value),
           _, _));

  password_save_manager()->PresaveGeneratedPassword(parsed_submitted_form_);
}

INSTANTIATE_TEST_SUITE_P(
    MultiStorePasswordSaveManagerTestGenerationConflictWithAccountStoreEnabled,
    MultiStorePasswordSaveManagerTestGenerationConflictWithAccountStoreEnabled,
    testing::Combine(testing::Bool(), testing::Bool()));

TEST_F(MultiStorePasswordSaveManagerTest,
       PresaveGeneratedPasswordInProfileStoreIfAccountStoreDisabled) {
  SetAccountStoreEnabled(/*is_enabled=*/false);
  fetcher()->NotifyFetchCompleted();

  EXPECT_CALL(*mock_profile_form_saver(), Save(_, _, _));
  EXPECT_CALL(*mock_account_form_saver(), Save(_, _, _)).Times(0);

  password_save_manager()->PresaveGeneratedPassword(parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       SaveInAccountStoreWhenPSLMatchExistsInTheAccountStore) {
  SetAccountStoreEnabled(/*is_enabled=*/true);

  PasswordForm psl_saved_match(psl_saved_match_);
  psl_saved_match.username_value = parsed_submitted_form_.username_value;
  psl_saved_match.password_value = parsed_submitted_form_.password_value;
  psl_saved_match.in_store = PasswordForm::Store::kAccountStore;
  SetNonFederatedAndNotifyFetchCompleted({&psl_saved_match});

  password_save_manager()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_CALL(*mock_profile_form_saver(), Save(_, _, _)).Times(0);
  EXPECT_CALL(*mock_account_form_saver(), Save(_, _, _));

  password_save_manager()->Save(&observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       SaveInProfileStoreWhenPSLMatchExistsInTheProfileStore) {
  SetAccountStoreEnabled(/*is_enabled=*/true);

  PasswordForm psl_saved_match(psl_saved_match_);
  psl_saved_match.username_value = parsed_submitted_form_.username_value;
  psl_saved_match.password_value = parsed_submitted_form_.password_value;
  psl_saved_match.in_store = PasswordForm::Store::kProfileStore;
  SetNonFederatedAndNotifyFetchCompleted({&psl_saved_match});

  password_save_manager()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_CALL(*mock_profile_form_saver(), Save(_, _, _));
  EXPECT_CALL(*mock_account_form_saver(), Save(_, _, _)).Times(0);

  password_save_manager()->Save(&observed_form_, parsed_submitted_form_);
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
      {&profile_psl_saved_match, &account_psl_saved_match});

  password_save_manager()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_CALL(*mock_profile_form_saver(), Save(_, _, _));
  EXPECT_CALL(*mock_account_form_saver(), Save(_, _, _));

  password_save_manager()->Save(&observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest, UpdateVsPSLMatch) {
  SetAccountStoreEnabled(/*is_enabled=*/true);

  PasswordForm profile_saved_match(saved_match_);
  profile_saved_match.username_value = parsed_submitted_form_.username_value;
  profile_saved_match.password_value = base::ASCIIToUTF16("old_password");
  profile_saved_match.in_store = PasswordForm::Store::kProfileStore;

  PasswordForm account_psl_saved_match(psl_saved_match_);
  account_psl_saved_match.username_value =
      parsed_submitted_form_.username_value;
  account_psl_saved_match.password_value =
      parsed_submitted_form_.password_value;
  account_psl_saved_match.in_store = PasswordForm::Store::kAccountStore;

  SetNonFederatedAndNotifyFetchCompleted(
      {&profile_saved_match, &account_psl_saved_match});

  password_save_manager()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  // This should *not* result in an update prompt.
  EXPECT_FALSE(password_save_manager()->IsPasswordUpdate());

  EXPECT_CALL(*mock_profile_form_saver(), Update(_, _, _));
  EXPECT_CALL(*mock_account_form_saver(), Save(_, _, _));

  password_save_manager()->Save(&observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest, UnblacklistInBothStores) {
  SetAccountStoreEnabled(/*is_enabled=*/true);
  const PasswordStore::FormDigest form_digest(saved_match_);

  EXPECT_CALL(*mock_profile_form_saver(), Unblacklist(form_digest));
  EXPECT_CALL(*mock_account_form_saver(), Unblacklist(form_digest));

  password_save_manager()->Unblacklist(form_digest);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       BlacklistInAccountStoreWhenAccountStoreEnabled) {
  SetAccountStoreEnabled(/*is_enabled=*/true);
  const PasswordStore::FormDigest form_digest(saved_match_);
  SetDefaultPasswordStore(PasswordForm::Store::kAccountStore);

  EXPECT_CALL(*mock_profile_form_saver(), PermanentlyBlacklist(form_digest))
      .Times(0);
  EXPECT_CALL(*mock_account_form_saver(), PermanentlyBlacklist(form_digest));
  password_save_manager()->PermanentlyBlacklist(form_digest);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       BlacklistInProfileStoreAlthoughAccountStoreEnabled) {
  SetAccountStoreEnabled(/*is_enabled=*/true);
  const PasswordStore::FormDigest form_digest(saved_match_);
  SetDefaultPasswordStore(PasswordForm::Store::kProfileStore);

  EXPECT_CALL(*mock_profile_form_saver(), PermanentlyBlacklist(form_digest));
  EXPECT_CALL(*mock_account_form_saver(), PermanentlyBlacklist(form_digest))
      .Times(0);
  password_save_manager()->PermanentlyBlacklist(form_digest);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       BlacklistInProfileStoreWhenAccountStoreDisabled) {
  SetAccountStoreEnabled(/*is_enabled=*/false);
  const PasswordStore::FormDigest form_digest(saved_match_);
  SetDefaultPasswordStore(PasswordForm::Store::kAccountStore);

  EXPECT_CALL(*mock_profile_form_saver(), PermanentlyBlacklist(form_digest));
  EXPECT_CALL(*mock_account_form_saver(), PermanentlyBlacklist(form_digest))
      .Times(0);
  password_save_manager()->PermanentlyBlacklist(form_digest);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       MoveCredentialsFromProfileToAccountStoreRecordsFlowAccepted) {
  base::HistogramTester histogram_tester;

  PasswordForm saved_match_in_profile_store(saved_match_);
  saved_match_in_profile_store.in_store = PasswordForm::Store::kProfileStore;
  saved_match_in_profile_store.moving_blocked_for_list.push_back(
      autofill::GaiaIdHash::FromGaiaId("user@gmail.com"));
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_in_profile_store});

  password_save_manager()->CreatePendingCredentials(
      saved_match_in_profile_store, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  PasswordForm saved_match_without_moving_blocked_list(
      saved_match_in_profile_store);
  saved_match_without_moving_blocked_list.moving_blocked_for_list.clear();

  EXPECT_CALL(*mock_profile_form_saver(), Remove(saved_match_in_profile_store));
  EXPECT_CALL(*mock_account_form_saver(),
              Save(saved_match_without_moving_blocked_list, _, _));

  password_save_manager()->MoveCredentialsToAccountStore(kTrigger);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStorage.MoveToAccountStoreFlowAccepted", kTrigger,
      1);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       MoveCredentialsFromProfileToAccountStoreWhenExistsOnlyInProfileStore) {
  PasswordForm saved_match_in_profile_store(saved_match_);
  saved_match_in_profile_store.in_store = PasswordForm::Store::kProfileStore;
  saved_match_in_profile_store.moving_blocked_for_list.push_back(
      autofill::GaiaIdHash::FromGaiaId("user@gmail.com"));
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_in_profile_store});

  password_save_manager()->CreatePendingCredentials(
      saved_match_in_profile_store, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  PasswordForm saved_match_without_moving_blocked_list(
      saved_match_in_profile_store);
  saved_match_without_moving_blocked_list.moving_blocked_for_list.clear();

  EXPECT_CALL(*mock_profile_form_saver(), Remove(saved_match_in_profile_store));
  EXPECT_CALL(*mock_account_form_saver(),
              Save(saved_match_without_moving_blocked_list, _, _));

  password_save_manager()->MoveCredentialsToAccountStore(kTrigger);
}

TEST_F(
    MultiStorePasswordSaveManagerTest,
    DoNotMoveCredentialsFromProfileToAccountStoreWhenExistsOnlyInProfileStoreWithDifferentUserName) {
  PasswordForm saved_match_in_profile_store(saved_match_);
  saved_match_in_profile_store.in_store = PasswordForm::Store::kProfileStore;
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_in_profile_store});
  PasswordForm credentials_with_diffrent_username(saved_match_in_profile_store);
  credentials_with_diffrent_username.username_value =
      ASCIIToUTF16("different_username");
  password_save_manager()->CreatePendingCredentials(
      credentials_with_diffrent_username, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_CALL(*mock_profile_form_saver(), Remove(saved_match_in_profile_store))
      .Times(0);
  EXPECT_CALL(*mock_account_form_saver(),
              Save(saved_match_in_profile_store, _, _))
      .Times(0);

  password_save_manager()->MoveCredentialsToAccountStore(kTrigger);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       MovePSLMatchedCredentialsFromProfileToAccountStore) {
  PasswordForm saved_match_in_profile_store(saved_match_);
  saved_match_in_profile_store.in_store = PasswordForm::Store::kProfileStore;
  PasswordForm psl_saved_match_in_profile_store(psl_saved_match_);
  psl_saved_match_in_profile_store.in_store =
      PasswordForm::Store::kProfileStore;
  SetNonFederatedAndNotifyFetchCompleted(
      {&saved_match_in_profile_store, &psl_saved_match_in_profile_store});

  password_save_manager()->CreatePendingCredentials(
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

  password_save_manager()->MoveCredentialsToAccountStore(kTrigger);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       MoveFederatedCredentialsFromProfileToAccountStore) {
  PasswordForm federated_match_in_profile_store = CreateSavedFederated();
  federated_match_in_profile_store.in_store =
      PasswordForm::Store::kProfileStore;

  SetFederatedAndNotifyFetchCompleted({&federated_match_in_profile_store});

  password_save_manager()->CreatePendingCredentials(
      federated_match_in_profile_store, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_CALL(*mock_profile_form_saver(),
              Remove(federated_match_in_profile_store));

  EXPECT_CALL(*mock_account_form_saver(),
              Save(federated_match_in_profile_store, _, _));

  password_save_manager()->MoveCredentialsToAccountStore(kTrigger);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       MoveCredentialsFromProfileToAccountStoreWhenExistsInBothStores) {
  PasswordForm saved_match_in_profile_store(saved_match_);
  saved_match_in_profile_store.in_store = PasswordForm::Store::kProfileStore;
  PasswordForm saved_match_in_account_store(saved_match_);
  saved_match_in_account_store.in_store = PasswordForm::Store::kAccountStore;
  SetNonFederatedAndNotifyFetchCompleted(
      {&saved_match_in_profile_store, &saved_match_in_account_store});

  password_save_manager()->CreatePendingCredentials(
      saved_match_in_profile_store, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_CALL(*mock_profile_form_saver(), Remove(saved_match_in_profile_store));
  EXPECT_CALL(*mock_account_form_saver(), Save).Times(0);

  password_save_manager()->MoveCredentialsToAccountStore(kTrigger);
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

  SetNonFederatedAndNotifyFetchCompleted({&saved_match_in_profile_store,
                                          &psl_saved_match_in_profile_store,
                                          &psl_saved_match_in_account_store});

  password_save_manager()->CreatePendingCredentials(
      saved_match_in_profile_store, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_CALL(*mock_profile_form_saver(), Remove(saved_match_in_profile_store));
  EXPECT_CALL(*mock_profile_form_saver(),
              Remove(psl_saved_match_in_profile_store));
  EXPECT_CALL(*mock_account_form_saver(),
              Save(saved_match_in_profile_store, _, _));

  password_save_manager()->MoveCredentialsToAccountStore(kTrigger);
}

TEST_F(MultiStorePasswordSaveManagerTest, BlockMovingWhenExistsInProfileStore) {
  autofill::GaiaIdHash user1_id_hash =
      autofill::GaiaIdHash::FromGaiaId("user1@gmail.com");
  autofill::GaiaIdHash user2_id_hash =
      autofill::GaiaIdHash::FromGaiaId("user2@gmail.com");

  PasswordForm profile_saved_match(saved_match_);
  profile_saved_match.username_value = parsed_submitted_form_.username_value;
  profile_saved_match.password_value = parsed_submitted_form_.password_value;
  profile_saved_match.in_store = PasswordForm::Store::kProfileStore;
  profile_saved_match.moving_blocked_for_list = {user1_id_hash};

  SetNonFederatedAndNotifyFetchCompleted({&profile_saved_match});

  password_save_manager()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  PasswordForm profile_updated_match(profile_saved_match);
  profile_updated_match.date_last_used =
      password_save_manager()->GetPendingCredentials().date_last_used;
  profile_updated_match.moving_blocked_for_list.push_back(user2_id_hash);

  EXPECT_CALL(*mock_account_form_saver(), Update).Times(0);
  EXPECT_CALL(*mock_profile_form_saver(), Update(profile_updated_match, _, _));

  password_save_manager()->BlockMovingToAccountStoreFor(user2_id_hash);
}

TEST_F(MultiStorePasswordSaveManagerTest, BlockMovingWhenExistsInBothStores) {
  autofill::GaiaIdHash user1_id_hash =
      autofill::GaiaIdHash::FromGaiaId("user1@gmail.com");
  autofill::GaiaIdHash user2_id_hash =
      autofill::GaiaIdHash::FromGaiaId("user2@gmail.com");

  PasswordForm account_saved_match(saved_match_);
  account_saved_match.username_value = parsed_submitted_form_.username_value;
  account_saved_match.password_value = parsed_submitted_form_.password_value;
  account_saved_match.in_store = PasswordForm::Store::kAccountStore;

  PasswordForm profile_saved_match(account_saved_match);
  profile_saved_match.in_store = PasswordForm::Store::kProfileStore;
  profile_saved_match.moving_blocked_for_list = {user1_id_hash};

  SetNonFederatedAndNotifyFetchCompleted({&profile_saved_match});

  password_save_manager()->CreatePendingCredentials(
      parsed_submitted_form_, &observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  PasswordForm profile_updated_match(profile_saved_match);
  profile_updated_match.date_last_used =
      password_save_manager()->GetPendingCredentials().date_last_used;
  profile_updated_match.moving_blocked_for_list.push_back(user2_id_hash);

  EXPECT_CALL(*mock_account_form_saver(), Update).Times(0);
  EXPECT_CALL(*mock_profile_form_saver(), Update(profile_updated_match, _, _));

  password_save_manager()->BlockMovingToAccountStoreFor(user2_id_hash);
}

}  // namespace password_manager
