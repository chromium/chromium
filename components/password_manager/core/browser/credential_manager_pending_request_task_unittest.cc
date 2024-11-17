// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_pending_request_task.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/credential_type_flags.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using testing::_;
using testing::Return;

constexpr int kIncludePasswordsFlag =
    static_cast<int>(CredentialTypeFlags::kPassword);

PasswordFormDigest GetFormDigest() {
  PasswordFormDigest digest(PasswordForm::Scheme::kHtml,
                            "http://www.example.com/",
                            GURL("http://www.example.com/"));
  return digest;
}

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;

  MOCK_METHOD(bool,
              PromptUserToChooseCredentials,
              (std::vector<std::unique_ptr<PasswordForm>>,
               const url::Origin&,
               CredentialsCallback),
              (override));
  MOCK_METHOD(void,
              NotifyUserAutoSignin,
              (std::vector<std::unique_ptr<PasswordForm>>, const url::Origin&),
              (override));
  MOCK_METHOD(PasswordStoreInterface*,
              GetProfilePasswordStore,
              (),
              (const, override));
  MOCK_METHOD(PasswordStoreInterface*,
              GetAccountPasswordStore,
              (),
              (const, override));
  MOCK_METHOD(PrefService*, GetPrefs, (), (const, override));
};

class CredentialManagerPendingRequestTaskDelegateMock
    : public CredentialManagerPendingRequestTaskDelegate {
 public:
  CredentialManagerPendingRequestTaskDelegateMock() = default;
  ~CredentialManagerPendingRequestTaskDelegateMock() = default;

  MOCK_METHOD(bool, IsZeroClickAllowed, (), (const, override));
  MOCK_METHOD(url::Origin, GetOrigin, (), (const, override));
  MOCK_METHOD(PasswordManagerClient*, client, (), (const, override));
  MOCK_METHOD(void,
              SendCredential,
              (SendCredentialCallback send_callback,
               const CredentialInfo& credential),
              (override));
  MOCK_METHOD(void,
              SendPasswordForm,
              (SendCredentialCallback send_callback,
               CredentialMediationRequirement mediation,
               const PasswordForm* form),
              (override));
};
}  // namespace
class CredentialManagerPendingRequestTaskTest : public ::testing::Test {
 public:
  CredentialManagerPendingRequestTaskTest() {
    profile_store_ = new TestPasswordStore(IsAccountStore(false));
    profile_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);

    account_store_ = new TestPasswordStore(IsAccountStore(true));
    account_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);

    prefs_.registry()->RegisterBooleanPref(
        prefs::kWasAutoSignInFirstRunExperienceShown, true);

    client_ = std::make_unique<MockPasswordManagerClient>();
    ON_CALL(*client_, GetProfilePasswordStore)
        .WillByDefault(Return(profile_store_.get()));
    ON_CALL(*client_, GetAccountPasswordStore)
        .WillByDefault(Return(account_store_.get()));
    ON_CALL(*client_, GetPrefs()).WillByDefault(Return(&prefs_));

    GURL url("http://www.example.com/");
    ON_CALL(delegate_mock_, client).WillByDefault(Return(client_.get()));
    ON_CALL(delegate_mock_, GetOrigin)
        .WillByDefault(Return(url::Origin::Create(url)));

    form_.username_value = u"Username";
    form_.password_value = u"Password";
    form_.url = url;
    form_.signon_realm = form_.url.spec();
    form_.scheme = PasswordForm::Scheme::kHtml;
    form_.skip_zero_click = false;
    form_.match_type = PasswordForm::MatchType::kExact;
  }
  ~CredentialManagerPendingRequestTaskTest() override = default;

  void TearDown() override {
    account_store_->ShutdownOnUIThread();
    profile_store_->ShutdownOnUIThread();
    // It's needed to cleanup the password store asynchronously.
    task_environment_.RunUntilIdle();
  }

  void RunAllPendingTasks() { task_environment_.RunUntilIdle(); }

  MockPasswordManagerClient* client() { return client_.get(); }

 protected:
  testing::NiceMock<CredentialManagerPendingRequestTaskDelegateMock>
      delegate_mock_;
  scoped_refptr<TestPasswordStore> profile_store_;
  scoped_refptr<TestPasswordStore> account_store_;
  TestingPrefServiceSimple prefs_;
  PasswordForm form_;

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockPasswordManagerClient> client_;
};

TEST_F(CredentialManagerPendingRequestTaskTest, OnlyProfileStore) {
  ON_CALL(*client(), GetAccountPasswordStore).WillByDefault(Return(nullptr));

  form_.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(form_);
  RunAllPendingTasks();

  std::vector<std::unique_ptr<PasswordForm>> expected_forms;
  expected_forms.push_back(std::make_unique<PasswordForm>(form_));
  EXPECT_CALL(*client(),
              PromptUserToChooseCredentials(
                  UnorderedPasswordFormElementsAre(&expected_forms), _, _));

  CredentialManagerPendingRequestTask task(
      &delegate_mock_, /*callback=*/base::DoNothing(),
      CredentialMediationRequirement::kOptional,
      /*requested_credential_type_flags=*/kIncludePasswordsFlag,
      /*request_federations=*/{}, GetFormDigest());
  RunAllPendingTasks();
}

TEST_F(CredentialManagerPendingRequestTaskTest,
       SameUsernameDifferentPasswordsInBothStores) {
  // This is testing that when two credentials have the same username and
  // different passwords from two store, both are passed to the UI.
  PasswordForm profile_form = form_;
  profile_form.in_store = PasswordForm::Store::kProfileStore;
  profile_form.password_value = u"ProfilePassword";
  profile_store_->AddLogin(profile_form);

  PasswordForm account_form = form_;
  account_form.in_store = PasswordForm::Store::kAccountStore;
  account_form.password_value = u"AccountPassword";
  account_store_->AddLogin(account_form);
  RunAllPendingTasks();

  std::vector<std::unique_ptr<PasswordForm>> expected_forms;
  expected_forms.push_back(std::make_unique<PasswordForm>(profile_form));
  expected_forms.push_back(std::make_unique<PasswordForm>(account_form));
  EXPECT_CALL(*client(),
              PromptUserToChooseCredentials(
                  UnorderedPasswordFormElementsAre(&expected_forms), _, _));

  CredentialManagerPendingRequestTask task(
      &delegate_mock_, /*callback=*/base::DoNothing(),
      CredentialMediationRequirement::kOptional,
      /*requested_credential_type_flags=*/kIncludePasswordsFlag,
      /*request_federations=*/{}, GetFormDigest());
  RunAllPendingTasks();
}

TEST_F(CredentialManagerPendingRequestTaskTest,
       SameUsernameSamePasswordsInBothStores) {
  // This is testing that when two credentials have the same username and
  // passwords from two store, the account store version is passed to the UI.
  PasswordForm profile_form = form_;
  profile_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(profile_form);

  PasswordForm account_form = form_;
  account_form.in_store = PasswordForm::Store::kAccountStore;
  account_store_->AddLogin(account_form);
  RunAllPendingTasks();

  std::vector<std::unique_ptr<PasswordForm>> expected_forms;
  PasswordForm expected_form = form_;
  expected_form.in_store =
      PasswordForm::Store::kProfileStore | PasswordForm::Store::kAccountStore;
  expected_forms.push_back(std::make_unique<PasswordForm>(expected_form));
  EXPECT_CALL(*client(),
              PromptUserToChooseCredentials(
                  UnorderedPasswordFormElementsAre(&expected_forms), _, _));

  CredentialManagerPendingRequestTask task(
      &delegate_mock_, /*callback=*/base::DoNothing(),
      CredentialMediationRequirement::kOptional,
      /*requested_credential_type_flags=*/kIncludePasswordsFlag,
      /*request_federations=*/{}, GetFormDigest());
  RunAllPendingTasks();
}

TEST_F(CredentialManagerPendingRequestTaskTest,
       SameFederatedCredentialsInBothStores) {
  // This is testing that when two federated credentials have the same username
  // for the same origin, the account store version is passed to the UI.
  GURL federation_url("https://google.com/");
  form_.federation_origin = url::SchemeHostPort(federation_url);
  form_.password_value = std::u16string();
  form_.signon_realm = "federation://www.example.com/google.com";

  PasswordForm profile_form = form_;
  profile_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(profile_form);

  PasswordForm account_form = form_;
  account_form.in_store = PasswordForm::Store::kAccountStore;
  account_store_->AddLogin(account_form);
  RunAllPendingTasks();

  std::vector<std::unique_ptr<PasswordForm>> expected_forms;
  expected_forms.push_back(std::make_unique<PasswordForm>(account_form));
  EXPECT_CALL(*client(),
              PromptUserToChooseCredentials(
                  UnorderedPasswordFormElementsAre(&expected_forms), _, _));

  CredentialManagerPendingRequestTask task(
      &delegate_mock_, /*callback=*/base::DoNothing(),
      CredentialMediationRequirement::kOptional,
      /*requested_credential_type_flags=*/0, {federation_url}, GetFormDigest());
  RunAllPendingTasks();
}

TEST_F(CredentialManagerPendingRequestTaskTest,
       AutosigninForExactlyMatchingForm) {
  profile_store_->AddLogin(form_);
  RunAllPendingTasks();

  std::vector<std::unique_ptr<PasswordForm>> expected_forms;
  form_.in_store = PasswordForm::Store::kProfileStore;
  expected_forms.push_back(std::make_unique<PasswordForm>(form_));

  EXPECT_CALL(*client(),
              NotifyUserAutoSignin(
                  UnorderedPasswordFormElementsAre(&expected_forms), _));
  EXPECT_CALL(*client(), PromptUserToChooseCredentials).Times(0);
  EXPECT_CALL(delegate_mock_, IsZeroClickAllowed).WillOnce(Return(true));

  CredentialManagerPendingRequestTask task(
      &delegate_mock_, /*callback=*/base::DoNothing(),
      CredentialMediationRequirement::kOptional,
      /*requested_credential_type_flags=*/kIncludePasswordsFlag,
      /*request_federations=*/{}, GetFormDigest());
  RunAllPendingTasks();
}

TEST_F(CredentialManagerPendingRequestTaskTest, NoAutosigninForPSLMatches) {
  EXPECT_CALL(delegate_mock_, IsZeroClickAllowed).WillOnce(Return(true));

  PasswordForm psl_form = form_;
  psl_form.signon_realm = "http://m.example.com/";

  profile_store_->AddLogin(psl_form);
  RunAllPendingTasks();

  std::vector<std::unique_ptr<PasswordForm>> expected_forms;
  psl_form.match_type = PasswordForm::MatchType::kPSL;
  psl_form.in_store = PasswordForm::Store::kProfileStore;
  expected_forms.push_back(std::make_unique<PasswordForm>(psl_form));

  EXPECT_CALL(*client(), NotifyUserAutoSignin).Times(0);
  EXPECT_CALL(*client(),
              PromptUserToChooseCredentials(
                  UnorderedPasswordFormElementsAre(&expected_forms), _, _));

  CredentialManagerPendingRequestTask task(
      &delegate_mock_, /*callback=*/base::DoNothing(),
      CredentialMediationRequirement::kOptional,
      /*requested_credential_type_flags=*/kIncludePasswordsFlag,
      /*request_federations=*/{}, GetFormDigest());
  RunAllPendingTasks();
}

TEST_F(CredentialManagerPendingRequestTaskTest,
       NoAutosigninIfExactAndPSLMatch) {
  PasswordForm psl_form = form_;
  psl_form.username_value = u"admin";
  psl_form.signon_realm = "http://m.example.com/";

  profile_store_->AddLogin(form_);
  profile_store_->AddLogin(psl_form);
  RunAllPendingTasks();

  std::vector<std::unique_ptr<PasswordForm>> expected_forms;
  form_.in_store = PasswordForm::Store::kProfileStore;
  expected_forms.push_back(std::make_unique<PasswordForm>(form_));
  psl_form.match_type = PasswordForm::MatchType::kPSL;
  psl_form.in_store = PasswordForm::Store::kProfileStore;
  expected_forms.push_back(std::make_unique<PasswordForm>(psl_form));

  EXPECT_CALL(*client(), NotifyUserAutoSignin).Times(0);
  EXPECT_CALL(*client(),
              PromptUserToChooseCredentials(
                  UnorderedPasswordFormElementsAre(&expected_forms), _, _));

  CredentialManagerPendingRequestTask task(
      &delegate_mock_, /*callback=*/base::DoNothing(),
      CredentialMediationRequirement::kOptional,
      /*requested_credential_type_flags=*/kIncludePasswordsFlag,
      /*request_federations=*/{}, GetFormDigest());
  RunAllPendingTasks();
}

}  // namespace password_manager
