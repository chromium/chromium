// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_pending_request_task.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

class TestPasswordManagerClient : public StubPasswordManagerClient {
 public:
  TestPasswordManagerClient(PasswordStoreInterface* profile_store,
                            PasswordStoreInterface* account_store)
      : profile_store_(profile_store), account_store_(account_store) {}
  PasswordStoreInterface* GetProfilePasswordStore() const override {
    return profile_store_;
  }
  PasswordStoreInterface* GetAccountPasswordStore() const override {
    return account_store_;
  }

  // Store |forms| in |forms_passed_to_ui_|.
  bool PromptUserToChooseCredentials(
      std::vector<std::unique_ptr<PasswordForm>> forms,
      const url::Origin& origin,
      CredentialsCallback callback) override {
    forms_passed_to_ui_ = std::move(forms);
    return true;
  }

  const std::vector<std::unique_ptr<PasswordForm>>& forms_passed_to_ui() {
    return forms_passed_to_ui_;
  }

 private:
  std::vector<std::unique_ptr<PasswordForm>> forms_passed_to_ui_;
  raw_ptr<PasswordStoreInterface> profile_store_;
  raw_ptr<PasswordStoreInterface> account_store_;
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

    client_ = std::make_unique<TestPasswordManagerClient>(profile_store_.get(),
                                                          account_store_.get());

    GURL url("http://www.example.com");
    ON_CALL(delegate_mock_, client)
        .WillByDefault(testing::Return(client_.get()));
    ON_CALL(delegate_mock_, GetOrigin)
        .WillByDefault(testing::Return(url::Origin::Create(url)));

    form_.username_value = u"Username";
    form_.password_value = u"Password";
    form_.url = url;
    form_.signon_realm = form_.url.DeprecatedGetOriginAsURL().spec();
    form_.scheme = PasswordForm::Scheme::kHtml;
    form_.skip_zero_click = false;
  }
  ~CredentialManagerPendingRequestTaskTest() override = default;

  void TearDown() override {
    account_store_->ShutdownOnUIThread();
    profile_store_->ShutdownOnUIThread();
    // It's needed to cleanup the password store asynchronously.
    task_environment_.RunUntilIdle();
  }

  TestPasswordManagerClient* client() { return client_.get(); }

 protected:
  testing::NiceMock<CredentialManagerPendingRequestTaskDelegateMock>
      delegate_mock_;
  scoped_refptr<TestPasswordStore> profile_store_;
  scoped_refptr<TestPasswordStore> account_store_;
  PasswordForm form_;

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestPasswordManagerClient> client_;
};

TEST_F(CredentialManagerPendingRequestTaskTest, QueryProfileStore) {
  CredentialManagerPendingRequestTask task(
      &delegate_mock_, /*callback=*/base::DoNothing(),
      CredentialMediationRequirement::kSilent, /*include_passwords=*/false,
      /*request_federations=*/{}, StoresToQuery::kProfileStore);

  // We are expecting results from only one store, delegate should be called
  // upon getting a response from the store.
  EXPECT_CALL(delegate_mock_, SendCredential);
  task.OnGetPasswordStoreResultsFrom(profile_store_.get(), {});
}

TEST_F(CredentialManagerPendingRequestTaskTest, QueryProfileAndAccountStores) {
  CredentialManagerPendingRequestTask task(
      &delegate_mock_, /*callback=*/base::DoNothing(),
      CredentialMediationRequirement::kSilent, /*include_passwords=*/false,
      /*request_federations=*/{}, StoresToQuery::kProfileAndAccountStores);

  // We are expecting results from 2 stores, the delegate shouldn't be called
  // until both stores respond.
  EXPECT_CALL(delegate_mock_, SendCredential).Times(0);
  task.OnGetPasswordStoreResultsFrom(profile_store_.get(), {});

  testing::Mock::VerifyAndClearExpectations(&delegate_mock_);

  EXPECT_CALL(delegate_mock_, SendCredential);
  task.OnGetPasswordStoreResultsFrom(account_store_.get(), {});
}

TEST_F(CredentialManagerPendingRequestTaskTest,
       SameUsernameDifferentPasswordsInBothStores) {
  // This is testing that when two credentials have the same username and
  // different passwords from two store, both are passed to the UI.
  CredentialManagerPendingRequestTask task(
      &delegate_mock_, /*callback=*/base::DoNothing(),
      CredentialMediationRequirement::kOptional, /*include_passwords=*/true,
      /*request_federations=*/{}, StoresToQuery::kProfileAndAccountStores);

  PasswordForm profile_form = form_;
  profile_form.in_store = PasswordForm::Store::kProfileStore;
  profile_form.password_value = u"ProfilePassword";
  std::vector<std::unique_ptr<PasswordForm>> profile_forms;
  profile_forms.push_back(std::make_unique<PasswordForm>(profile_form));

  PasswordForm account_form = form_;
  account_form.in_store = PasswordForm::Store::kAccountStore;
  account_form.password_value = u"AccountPassword";
  std::vector<std::unique_ptr<PasswordForm>> account_forms;
  account_forms.push_back(std::make_unique<PasswordForm>(account_form));

  task.OnGetPasswordStoreResultsFrom(profile_store_.get(),
                                     std::move(profile_forms));
  task.OnGetPasswordStoreResultsFrom(account_store_.get(),
                                     std::move(account_forms));
  EXPECT_EQ(2U, client()->forms_passed_to_ui().size());
}

TEST_F(CredentialManagerPendingRequestTaskTest,
       SameUsernameSamePasswordsInBothStores) {
  // This is testing that when two credentials have the same username and
  // passwords from two store, the account store version is passed to the UI.
  CredentialManagerPendingRequestTask task(
      &delegate_mock_, /*callback=*/base::DoNothing(),
      CredentialMediationRequirement::kOptional, /*include_passwords=*/true,
      /*request_federations=*/{}, StoresToQuery::kProfileAndAccountStores);

  PasswordForm profile_form = form_;
  profile_form.in_store = PasswordForm::Store::kProfileStore;
  std::vector<std::unique_ptr<PasswordForm>> profile_forms;
  profile_forms.push_back(std::make_unique<PasswordForm>(profile_form));

  PasswordForm account_form = form_;
  account_form.in_store = PasswordForm::Store::kAccountStore;
  std::vector<std::unique_ptr<PasswordForm>> account_forms;
  account_forms.push_back(std::make_unique<PasswordForm>(account_form));

  task.OnGetPasswordStoreResultsFrom(profile_store_.get(),
                                     std::move(profile_forms));
  task.OnGetPasswordStoreResultsFrom(account_store_.get(),
                                     std::move(account_forms));
  ASSERT_EQ(1U, client()->forms_passed_to_ui().size());
  EXPECT_TRUE(client()->forms_passed_to_ui()[0]->IsUsingAccountStore());
}

TEST_F(CredentialManagerPendingRequestTaskTest,
       SameFederatedCredentialsInBothStores) {
  // This is testing that when two federated credentials have the same username
  // for the same origin, the account store version is passed to the UI.
  GURL federation_url("https://google.com/");
  CredentialManagerPendingRequestTask task(
      &delegate_mock_, /*callback=*/base::DoNothing(),
      CredentialMediationRequirement::kOptional, /*include_passwords=*/false,
      {federation_url}, StoresToQuery::kProfileAndAccountStores);

  form_.federation_origin = url::Origin::Create(federation_url);
  form_.password_value = std::u16string();
  form_.signon_realm = "federation://example.com/google.com";

  PasswordForm profile_form = form_;
  profile_form.in_store = PasswordForm::Store::kProfileStore;
  std::vector<std::unique_ptr<PasswordForm>> profile_forms;
  profile_forms.push_back(std::make_unique<PasswordForm>(profile_form));

  PasswordForm account_form = form_;
  account_form.in_store = PasswordForm::Store::kAccountStore;
  std::vector<std::unique_ptr<PasswordForm>> account_forms;
  account_forms.push_back(std::make_unique<PasswordForm>(account_form));

  task.OnGetPasswordStoreResultsFrom(profile_store_.get(),
                                     std::move(profile_forms));
  task.OnGetPasswordStoreResultsFrom(account_store_.get(),
                                     std::move(account_forms));
  ASSERT_EQ(1U, client()->forms_passed_to_ui().size());
  EXPECT_TRUE(client()->forms_passed_to_ui()[0]->IsUsingAccountStore());
}

}  // namespace password_manager
