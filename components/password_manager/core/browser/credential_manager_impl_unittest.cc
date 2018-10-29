// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_impl.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "components/password_manager/core/browser/android_affiliation/mock_affiliated_match_helper.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using testing::_;
using testing::ElementsAre;
using testing::Pointee;
using testing::UnorderedElementsAre;

namespace password_manager {

namespace {

const char kTestWebOrigin[] = "https://example.com/";
const char kTestAndroidRealm1[] = "android://hash@com.example.one.android/";
const char kTestAndroidRealm2[] = "android://hash@com.example.two.android/";

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_CONST_METHOD0(IsSavingAndFillingEnabledForCurrentPage, bool());
  MOCK_CONST_METHOD0(IsFillingEnabledForCurrentPage, bool());
  MOCK_METHOD0(OnCredentialManagerUsed, bool());
  MOCK_CONST_METHOD0(IsIncognito, bool());
  MOCK_METHOD0(NotifyUserAutoSigninPtr, bool());
  MOCK_METHOD1(NotifyUserCouldBeAutoSignedInPtr,
               bool(autofill::PasswordForm* form));
  MOCK_METHOD0(NotifyStorePasswordCalled, void());
  MOCK_METHOD1(PromptUserToSavePasswordPtr, void(PasswordFormManagerForUI*));
  MOCK_METHOD3(PromptUserToChooseCredentialsPtr,
               bool(const std::vector<autofill::PasswordForm*>& local_forms,
                    const GURL& origin,
                    const CredentialsCallback& callback));

  explicit MockPasswordManagerClient(PasswordStore* store)
      : store_(store), password_manager_(this) {
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterBooleanPref(prefs::kCredentialsEnableAutosignin,
                                            true);
    prefs_->registry()->RegisterBooleanPref(
        prefs::kWasAutoSignInFirstRunExperienceShown, true);
  }
  ~MockPasswordManagerClient() override {}

  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<PasswordFormManagerForUI> manager,
      bool update_password) override {
    manager_.swap(manager);
    PromptUserToSavePasswordPtr(manager_.get());
    return true;
  }

  void NotifyUserCouldBeAutoSignedIn(
      std::unique_ptr<autofill::PasswordForm> form) override {
    NotifyUserCouldBeAutoSignedInPtr(form.get());
  }

  PasswordStore* GetPasswordStore() const override { return store_; }

  PrefService* GetPrefs() const override { return prefs_.get(); }

  const PasswordManager* GetPasswordManager() const override {
    return &password_manager_;
  }

  const GURL& GetLastCommittedEntryURL() const override {
    return last_committed_url_;
  }

  bool PromptUserToChooseCredentials(
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
      const GURL& origin,
      const CredentialsCallback& callback) override {
    EXPECT_FALSE(local_forms.empty());
    const autofill::PasswordForm* form = local_forms[0].get();
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(callback,
                       base::Owned(new autofill::PasswordForm(*form))));
    std::vector<autofill::PasswordForm*> raw_forms(local_forms.size());
    std::transform(local_forms.begin(), local_forms.end(), raw_forms.begin(),
                   [](const std::unique_ptr<autofill::PasswordForm>& form) {
                     return form.get();
                   });
    PromptUserToChooseCredentialsPtr(raw_forms, origin, callback);
    return true;
  }

  void NotifyUserAutoSignin(
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
      const GURL& origin) override {
    EXPECT_FALSE(local_forms.empty());
    NotifyUserAutoSigninPtr();
  }

  PasswordFormManagerForUI* pending_manager() const { return manager_.get(); }

  void set_zero_click_enabled(bool zero_click_enabled) {
    prefs_->SetBoolean(prefs::kCredentialsEnableAutosignin, zero_click_enabled);
  }

  void set_first_run_seen(bool first_run_seen) {
    prefs_->SetBoolean(prefs::kWasAutoSignInFirstRunExperienceShown,
                       first_run_seen);
  }

  void set_last_committed_url(GURL last_committed_url) {
    last_committed_url_ = std::move(last_committed_url);
  }

 private:
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  PasswordStore* store_;
  std::unique_ptr<PasswordFormManagerForUI> manager_;
  PasswordManager password_manager_;
  GURL last_committed_url_{kTestWebOrigin};

  DISALLOW_COPY_AND_ASSIGN(MockPasswordManagerClient);
};

// Callbacks from CredentialManagerImpl methods
void RespondCallback(bool* called) {
  *called = true;
}

void GetCredentialCallback(bool* called,
                           CredentialManagerError* out_error,
                           base::Optional<CredentialInfo>* out_info,
                           CredentialManagerError error,
                           const base::Optional<CredentialInfo>& info) {
  *called = true;
  *out_error = error;
  *out_info = info;
}

GURL HttpURLFromHttps(const GURL& https_url) {
  GURL::Replacements rep;
  rep.SetSchemeStr(url::kHttpScheme);
  return https_url.ReplaceComponents(rep);
}

}  // namespace

class CredentialManagerImplTest : public testing::Test {
 public:
  CredentialManagerImplTest() {}

  void SetUp() override {
    store_ = new TestPasswordStore;
    store_->Init(syncer::SyncableService::StartSyncFlare(), nullptr);
    client_.reset(
        new testing::NiceMock<MockPasswordManagerClient>(store_.get()));
    cm_service_impl_.reset(new CredentialManagerImpl(client_.get()));
    ON_CALL(*client_, IsSavingAndFillingEnabledForCurrentPage())
        .WillByDefault(testing::Return(true));
    ON_CALL(*client_, IsFillingEnabledForCurrentPage())
        .WillByDefault(testing::Return(true));
    ON_CALL(*client_, OnCredentialManagerUsed())
        .WillByDefault(testing::Return(true));
    ON_CALL(*client_, IsIncognito()).WillByDefault(testing::Return(false));

    form_.username_value = base::ASCIIToUTF16("Username");
    form_.display_name = base::ASCIIToUTF16("Display Name");
    form_.icon_url = GURL("https://example.com/icon.png");
    form_.password_value = base::ASCIIToUTF16("Password");
    form_.origin = client_->GetLastCommittedEntryURL();
    form_.signon_realm = form_.origin.GetOrigin().spec();
    form_.scheme = autofill::PasswordForm::SCHEME_HTML;
    form_.skip_zero_click = false;

    affiliated_form1_.username_value = base::ASCIIToUTF16("Affiliated 1");
    affiliated_form1_.display_name = base::ASCIIToUTF16("Display Name");
    affiliated_form1_.password_value = base::ASCIIToUTF16("Password");
    affiliated_form1_.origin = GURL();
    affiliated_form1_.signon_realm = kTestAndroidRealm1;
    affiliated_form1_.scheme = autofill::PasswordForm::SCHEME_HTML;
    affiliated_form1_.skip_zero_click = false;

    affiliated_form2_.username_value = base::ASCIIToUTF16("Affiliated 2");
    affiliated_form2_.display_name = base::ASCIIToUTF16("Display Name");
    affiliated_form2_.password_value = base::ASCIIToUTF16("Password");
    affiliated_form2_.origin = GURL();
    affiliated_form2_.signon_realm = kTestAndroidRealm2;
    affiliated_form2_.scheme = autofill::PasswordForm::SCHEME_HTML;
    affiliated_form2_.skip_zero_click = false;

    origin_path_form_.username_value = base::ASCIIToUTF16("Username 2");
    origin_path_form_.display_name = base::ASCIIToUTF16("Display Name 2");
    origin_path_form_.password_value = base::ASCIIToUTF16("Password 2");
    origin_path_form_.origin = GURL("https://example.com/path");
    origin_path_form_.signon_realm =
        origin_path_form_.origin.GetOrigin().spec();
    origin_path_form_.scheme = autofill::PasswordForm::SCHEME_HTML;
    origin_path_form_.skip_zero_click = false;

    subdomain_form_.username_value = base::ASCIIToUTF16("Username 2");
    subdomain_form_.display_name = base::ASCIIToUTF16("Display Name 2");
    subdomain_form_.password_value = base::ASCIIToUTF16("Password 2");
    subdomain_form_.origin = GURL("https://subdomain.example.com/path");
    subdomain_form_.signon_realm = subdomain_form_.origin.GetOrigin().spec();
    subdomain_form_.scheme = autofill::PasswordForm::SCHEME_HTML;
    subdomain_form_.skip_zero_click = false;

    cross_origin_form_.username_value = base::ASCIIToUTF16("Username");
    cross_origin_form_.display_name = base::ASCIIToUTF16("Display Name");
    cross_origin_form_.password_value = base::ASCIIToUTF16("Password");
    cross_origin_form_.origin = GURL("https://example.net/");
    cross_origin_form_.signon_realm =
        cross_origin_form_.origin.GetOrigin().spec();
    cross_origin_form_.scheme = autofill::PasswordForm::SCHEME_HTML;
    cross_origin_form_.skip_zero_click = false;

    store_->Clear();
    EXPECT_TRUE(store_->IsEmpty());
  }

  void TearDown() override {
    cm_service_impl_.reset();

    store_->ShutdownOnUIThread();

    // It's needed to cleanup the password store asynchronously.
    RunAllPendingTasks();
  }

  void ExpectZeroClickSignInFailure(CredentialMediationRequirement mediation,
                                    bool include_passwords,
                                    const std::vector<GURL>& federations) {
    bool called = false;
    CredentialManagerError error;
    base::Optional<CredentialInfo> credential;
    EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _))
        .Times(testing::Exactly(0));
    EXPECT_CALL(*client_, NotifyUserAutoSigninPtr()).Times(testing::Exactly(0));
    CallGet(
        mediation, include_passwords, federations,
        base::BindOnce(&GetCredentialCallback, &called, &error, &credential));

    RunAllPendingTasks();

    EXPECT_TRUE(called);
    EXPECT_EQ(CredentialManagerError::SUCCESS, error);
    EXPECT_EQ(CredentialType::CREDENTIAL_TYPE_EMPTY, credential->type);
  }

  void ExpectZeroClickSignInSuccess(CredentialMediationRequirement mediation,
                                    bool include_passwords,
                                    const std::vector<GURL>& federations,
                                    CredentialType type) {
    bool called = false;
    CredentialManagerError error;
    base::Optional<CredentialInfo> credential;
    EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _))
        .Times(testing::Exactly(0));
    EXPECT_CALL(*client_, NotifyUserAutoSigninPtr()).Times(testing::Exactly(1));
    CallGet(
        mediation, include_passwords, federations,
        base::BindOnce(&GetCredentialCallback, &called, &error, &credential));

    RunAllPendingTasks();

    EXPECT_TRUE(called);
    EXPECT_EQ(CredentialManagerError::SUCCESS, error);
    EXPECT_EQ(type, credential->type);
  }

  void ExpectCredentialType(CredentialMediationRequirement mediation,
                            bool include_passwords,
                            const std::vector<GURL>& federations,
                            CredentialType type) {
    bool called = false;
    CredentialManagerError error;
    base::Optional<CredentialInfo> credential;
    CallGet(
        mediation, include_passwords, federations,
        base::BindOnce(&GetCredentialCallback, &called, &error, &credential));

    RunAllPendingTasks();

    EXPECT_TRUE(called);
    EXPECT_EQ(CredentialManagerError::SUCCESS, error);
    EXPECT_EQ(type, credential->type);
  }

  CredentialManagerImpl* cm_service_impl() { return cm_service_impl_.get(); }

  // Helpers for testing CredentialManagerImpl methods.
  void CallStore(const CredentialInfo& info, StoreCallback callback) {
    cm_service_impl_->Store(info, std::move(callback));
  }

  void CallPreventSilentAccess(PreventSilentAccessCallback callback) {
    cm_service_impl_->PreventSilentAccess(std::move(callback));
  }

  void CallGet(CredentialMediationRequirement mediation,
               bool include_passwords,
               const std::vector<GURL>& federations,
               GetCallback callback) {
    cm_service_impl_->Get(mediation, include_passwords, federations,
                          std::move(callback));
  }

  void RunAllPendingTasks() { scoped_task_environment_.RunUntilIdle(); }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  autofill::PasswordForm form_;
  autofill::PasswordForm affiliated_form1_;
  autofill::PasswordForm affiliated_form2_;
  autofill::PasswordForm origin_path_form_;
  autofill::PasswordForm subdomain_form_;
  autofill::PasswordForm cross_origin_form_;
  scoped_refptr<TestPasswordStore> store_;
  std::unique_ptr<testing::NiceMock<MockPasswordManagerClient>> client_;
  std::unique_ptr<CredentialManagerImpl> cm_service_impl_;
};

TEST_F(CredentialManagerImplTest, IsZeroClickAllowed) {
  // IsZeroClickAllowed is uneffected by the first-run status.
  client_->set_zero_click_enabled(true);
  client_->set_first_run_seen(true);
  EXPECT_TRUE(cm_service_impl()->IsZeroClickAllowed());

  client_->set_zero_click_enabled(true);
  client_->set_first_run_seen(false);
  EXPECT_TRUE(cm_service_impl()->IsZeroClickAllowed());

  client_->set_zero_click_enabled(false);
  client_->set_first_run_seen(true);
  EXPECT_FALSE(cm_service_impl()->IsZeroClickAllowed());

  client_->set_zero_click_enabled(false);
  client_->set_first_run_seen(false);
  EXPECT_FALSE(cm_service_impl()->IsZeroClickAllowed());
}

TEST_F(CredentialManagerImplTest, CredentialManagerOnStore) {
  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_PASSWORD);
  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(_))
      .Times(testing::Exactly(1));
  EXPECT_CALL(*client_, NotifyStorePasswordCalled());

  bool called = false;
  CallStore(info, base::BindOnce(&RespondCallback, &called));

  // Allow the PasswordFormManager to talk to the password store, determine
  // that the form is new, and set it as pending.
  RunAllPendingTasks();

  EXPECT_TRUE(called);
  EXPECT_EQ(FormFetcher::State::NOT_WAITING,
            client_->pending_manager()->GetFormFetcher()->GetState());

  autofill::PasswordForm new_form =
      client_->pending_manager()->GetPendingCredentials();
  EXPECT_EQ(form_.username_value, new_form.username_value);
  EXPECT_EQ(form_.display_name, new_form.display_name);
  EXPECT_EQ(form_.password_value, new_form.password_value);
  EXPECT_EQ(form_.origin, new_form.origin);
  EXPECT_EQ(form_.signon_realm, new_form.signon_realm);
  EXPECT_TRUE(new_form.federation_origin.opaque());
  EXPECT_EQ(form_.icon_url, new_form.icon_url);
  EXPECT_FALSE(form_.skip_zero_click);
  EXPECT_EQ(autofill::PasswordForm::SCHEME_HTML, new_form.scheme);
}

TEST_F(CredentialManagerImplTest, CredentialManagerOnStoreFederated) {
  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(_))
      .Times(testing::Exactly(1));
  EXPECT_CALL(*client_, NotifyStorePasswordCalled());

  bool called = false;
  form_.federation_origin = url::Origin::Create(GURL("https://google.com/"));
  form_.password_value = base::string16();
  form_.signon_realm = "federation://example.com/google.com";
  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_FEDERATED);
  CallStore(info, base::BindOnce(&RespondCallback, &called));

  // Allow the PasswordFormManager to talk to the password store, determine
  // that the form is new, and set it as pending.
  RunAllPendingTasks();

  EXPECT_TRUE(called);
  EXPECT_EQ(FormFetcher::State::NOT_WAITING,
            client_->pending_manager()->GetFormFetcher()->GetState());

  autofill::PasswordForm new_form =
      client_->pending_manager()->GetPendingCredentials();
  EXPECT_EQ(form_.username_value, new_form.username_value);
  EXPECT_EQ(form_.display_name, new_form.display_name);
  EXPECT_EQ(form_.password_value, new_form.password_value);
  EXPECT_EQ(form_.origin, new_form.origin);
  EXPECT_EQ(form_.signon_realm, new_form.signon_realm);
  EXPECT_EQ(form_.federation_origin, new_form.federation_origin);
  EXPECT_EQ(form_.icon_url, new_form.icon_url);
  EXPECT_FALSE(form_.skip_zero_click);
  EXPECT_EQ(autofill::PasswordForm::SCHEME_HTML, new_form.scheme);
}

TEST_F(CredentialManagerImplTest, StoreFederatedAfterPassword) {
  // Populate the PasswordStore with a form.
  store_->AddLogin(form_);

  autofill::PasswordForm federated = form_;
  federated.password_value.clear();
  federated.type = autofill::PasswordForm::TYPE_API;
  federated.preferred = true;
  federated.federation_origin =
      url::Origin::Create(GURL("https://google.com/"));
  federated.signon_realm = "federation://example.com/google.com";
  CredentialInfo info(federated, CredentialType::CREDENTIAL_TYPE_FEDERATED);
  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(_));
  EXPECT_CALL(*client_, NotifyStorePasswordCalled());

  bool called = false;
  CallStore(info, base::BindOnce(&RespondCallback, &called));

  // Allow the PasswordFormManager to talk to the password store, determine
  // that the form is new, and set it as pending.
  RunAllPendingTasks();

  EXPECT_TRUE(called);
  EXPECT_EQ(FormFetcher::State::NOT_WAITING,
            client_->pending_manager()->GetFormFetcher()->GetState());
  client_->pending_manager()->Save();

  RunAllPendingTasks();
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_THAT(passwords["https://example.com/"], ElementsAre(form_));
  federated.date_created =
      passwords["federation://example.com/google.com"][0].date_created;
  EXPECT_THAT(passwords["federation://example.com/google.com"],
              ElementsAre(federated));
}

TEST_F(CredentialManagerImplTest, CredentialManagerStoreOverwrite) {
  // Add an unrelated form to complicate the task.
  origin_path_form_.preferred = true;
  store_->AddLogin(origin_path_form_);
  // Populate the PasswordStore with a form.
  form_.preferred = false;
  form_.display_name = base::ASCIIToUTF16("Old Name");
  form_.icon_url = GURL();
  store_->AddLogin(form_);
  RunAllPendingTasks();

  // Calling 'Store' with a credential that matches |form_| should update
  // the password without prompting the user.
  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_PASSWORD);
  info.password = base::ASCIIToUTF16("Totally new password.");
  info.name = base::ASCIIToUTF16("New Name");
  info.icon = GURL("https://example.com/icon.png");
  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(_)).Times(0);
  EXPECT_CALL(*client_, NotifyStorePasswordCalled());
  bool called = false;
  CallStore(info, base::BindOnce(&RespondCallback, &called));

  // Allow the PasswordFormManager to talk to the password store, determine
  // the form is a match for an existing form, and update the PasswordStore.
  RunAllPendingTasks();

  EXPECT_TRUE(called);

  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_EQ(1U, passwords.size());
  EXPECT_EQ(2U, passwords[form_.signon_realm].size());
  origin_path_form_.preferred = false;
  EXPECT_EQ(origin_path_form_, passwords[form_.signon_realm][0]);
  EXPECT_EQ(base::ASCIIToUTF16("Totally new password."),
            passwords[form_.signon_realm][1].password_value);
  EXPECT_EQ(base::ASCIIToUTF16("New Name"),
            passwords[form_.signon_realm][1].display_name);
  EXPECT_EQ(GURL("https://example.com/icon.png"),
            passwords[form_.signon_realm][1].icon_url);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerStorePSLMatchDoesNotTriggerBubble) {
  autofill::PasswordForm psl_form = subdomain_form_;
  psl_form.username_value = form_.username_value;
  psl_form.password_value = form_.password_value;
  store_->AddLogin(psl_form);

  // Calling 'Store' with a new credential that is a PSL match for an existing
  // credential with identical username and password should result in a silent
  // save without prompting the user.
  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_PASSWORD);
  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(_))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyStorePasswordCalled());
  bool called = false;
  CallStore(info, base::BindOnce(&RespondCallback, &called));
  RunAllPendingTasks();
  EXPECT_TRUE(called);

  // Check that both credentials are present in the password store.
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_EQ(2U, passwords.size());
  EXPECT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_EQ(1U, passwords[psl_form.signon_realm].size());
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerStorePSLMatchWithDifferentUsernameTriggersBubble) {
  base::string16 delta = base::ASCIIToUTF16("_totally_different");
  autofill::PasswordForm psl_form = subdomain_form_;
  psl_form.username_value = form_.username_value + delta;
  psl_form.password_value = form_.password_value;
  store_->AddLogin(psl_form);

  // Calling 'Store' with a new credential that is a PSL match for an existing
  // credential but has a different username should prompt the user and not
  // result in a silent save.
  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_PASSWORD);
  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(_))
      .Times(testing::Exactly(1));
  EXPECT_CALL(*client_, NotifyStorePasswordCalled());
  bool called = false;
  CallStore(info, base::BindOnce(&RespondCallback, &called));
  RunAllPendingTasks();
  EXPECT_TRUE(called);

  // Check that only the initial credential is present in the password store
  // and the new one is still pending.
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_EQ(1U, passwords.size());
  EXPECT_EQ(1U, passwords[psl_form.signon_realm].size());

  const auto& pending_cred =
      client_->pending_manager()->GetPendingCredentials();
  EXPECT_EQ(info.id, pending_cred.username_value);
  EXPECT_EQ(info.password, pending_cred.password_value);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerStorePSLMatchWithDifferentPasswordTriggersBubble) {
  base::string16 delta = base::ASCIIToUTF16("_totally_different");
  autofill::PasswordForm psl_form = subdomain_form_;
  psl_form.username_value = form_.username_value;
  psl_form.password_value = form_.password_value + delta;
  store_->AddLogin(psl_form);

  // Calling 'Store' with a new credential that is a PSL match for an existing
  // credential but has a different password should prompt the user and not
  // result in a silent save.
  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_PASSWORD);
  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(_))
      .Times(testing::Exactly(1));
  EXPECT_CALL(*client_, NotifyStorePasswordCalled());
  bool called = false;
  CallStore(info, base::BindOnce(&RespondCallback, &called));
  RunAllPendingTasks();
  EXPECT_TRUE(called);

  // Check that only the initial credential is present in the password store
  // and the new one is still pending.
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_EQ(1U, passwords.size());
  EXPECT_EQ(1U, passwords[psl_form.signon_realm].size());

  const auto& pending_cred =
      client_->pending_manager()->GetPendingCredentials();
  EXPECT_EQ(info.id, pending_cred.username_value);
  EXPECT_EQ(info.password, pending_cred.password_value);
}

TEST_F(CredentialManagerImplTest, CredentialManagerStoreOverwriteZeroClick) {
  form_.skip_zero_click = true;
  store_->AddLogin(form_);
  RunAllPendingTasks();

  // Calling 'Store' with a credential that matches |form_| should update
  // the credential without prompting the user.
  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_PASSWORD);
  bool called = false;
  EXPECT_CALL(*client_, NotifyStorePasswordCalled());
  CallStore(info, base::BindOnce(&RespondCallback, &called));

  // Allow the PasswordFormManager to talk to the password store, determine
  // the form is a match for an existing form, and update the PasswordStore.
  RunAllPendingTasks();

  // Verify that the update toggled the skip_zero_click flag off.
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerFederatedStoreOverwriteZeroClick) {
  form_.federation_origin = url::Origin::Create(GURL("https://example.com/"));
  form_.password_value = base::string16();
  form_.skip_zero_click = true;
  form_.signon_realm = "federation://example.com/example.com";
  store_->AddLogin(form_);
  RunAllPendingTasks();

  // Calling 'Store' with a credential that matches |form_| should update
  // the credential without prompting the user.
  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_FEDERATED);
  bool called = false;
  EXPECT_CALL(*client_, NotifyStorePasswordCalled());
  CallStore(info, base::BindOnce(&RespondCallback, &called));

  // Allow the PasswordFormManager to talk to the password store, determine
  // the form is a match for an existing form, and update the PasswordStore.
  RunAllPendingTasks();

  // Verify that the update toggled the skip_zero_click flag off.
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);
}

TEST_F(CredentialManagerImplTest, CredentialManagerGetOverwriteZeroClick) {
  // Set the global zero click flag on, and populate the PasswordStore with a
  // form that's set to skip zero click and has a primary key that won't match
  // credentials initially created via `store()`.
  client_->set_zero_click_enabled(true);
  form_.skip_zero_click = true;
  form_.username_element = base::ASCIIToUTF16("username-element");
  form_.password_element = base::ASCIIToUTF16("password-element");
  form_.origin = GURL("https://example.com/old_form.html");
  store_->AddLogin(form_);
  RunAllPendingTasks();

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _))
      .Times(testing::Exactly(1));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr()).Times(testing::Exactly(0));

  bool called = false;
  CredentialManagerError error;
  base::Optional<CredentialInfo> credential;
  CallGet(CredentialMediationRequirement::kOptional, true, federations,
          base::BindOnce(&GetCredentialCallback, &called, &error, &credential));

  RunAllPendingTasks();

  EXPECT_TRUE(called);
  EXPECT_EQ(CredentialManagerError::SUCCESS, error);

  // Verify that the update toggled the skip_zero_click flag.
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerSignInWithSavingDisabledForCurrentPage) {
  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_PASSWORD);
  EXPECT_CALL(*client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(_))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyStorePasswordCalled()).Times(0);

  bool called = false;
  CallStore(info, base::BindOnce(&RespondCallback, &called));

  RunAllPendingTasks();

  EXPECT_TRUE(called);
  EXPECT_FALSE(client_->pending_manager());
}

TEST_F(CredentialManagerImplTest, CredentialManagerOnPreventSilentAccess) {
  store_->AddLogin(form_);
  store_->AddLogin(subdomain_form_);
  store_->AddLogin(cross_origin_form_);
  RunAllPendingTasks();

  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_EQ(3U, passwords.size());
  EXPECT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_EQ(1U, passwords[subdomain_form_.signon_realm].size());
  EXPECT_EQ(1U, passwords[cross_origin_form_.signon_realm].size());
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);
  EXPECT_FALSE(passwords[subdomain_form_.signon_realm][0].skip_zero_click);
  EXPECT_FALSE(passwords[cross_origin_form_.signon_realm][0].skip_zero_click);

  bool called = false;
  CallPreventSilentAccess(base::BindOnce(&RespondCallback, &called));

  RunAllPendingTasks();

  EXPECT_TRUE(called);

  passwords = store_->stored_passwords();
  EXPECT_EQ(3U, passwords.size());
  EXPECT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_EQ(1U, passwords[subdomain_form_.signon_realm].size());
  EXPECT_EQ(1U, passwords[cross_origin_form_.signon_realm].size());
  EXPECT_TRUE(passwords[form_.signon_realm][0].skip_zero_click);
  EXPECT_TRUE(passwords[subdomain_form_.signon_realm][0].skip_zero_click);
  EXPECT_FALSE(passwords[cross_origin_form_.signon_realm][0].skip_zero_click);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnPreventSilentAccessIncognito) {
  EXPECT_CALL(*client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(testing::Return(false));
  store_->AddLogin(form_);
  RunAllPendingTasks();

  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  ASSERT_EQ(1U, passwords.size());
  ASSERT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);

  bool called = false;
  CallPreventSilentAccess(base::BindOnce(&RespondCallback, &called));
  RunAllPendingTasks();

  EXPECT_TRUE(called);

  passwords = store_->stored_passwords();
  ASSERT_EQ(1U, passwords.size());
  ASSERT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnPreventSilentAccessWithAffiliation) {
  store_->AddLogin(form_);
  store_->AddLogin(cross_origin_form_);
  store_->AddLogin(affiliated_form1_);
  store_->AddLogin(affiliated_form2_);

  store_->SetAffiliatedMatchHelper(
      std::make_unique<MockAffiliatedMatchHelper>());

  std::vector<GURL> federations;
  std::vector<std::string> affiliated_realms;
  affiliated_realms.push_back(kTestAndroidRealm1);
  static_cast<MockAffiliatedMatchHelper*>(store_->affiliated_match_helper())
      ->ExpectCallToGetAffiliatedAndroidRealms(
          cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);
  RunAllPendingTasks();

  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_EQ(4U, passwords.size());
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);
  EXPECT_FALSE(passwords[cross_origin_form_.signon_realm][0].skip_zero_click);
  EXPECT_FALSE(passwords[affiliated_form1_.signon_realm][0].skip_zero_click);
  EXPECT_FALSE(passwords[affiliated_form2_.signon_realm][0].skip_zero_click);

  bool called = false;
  CallPreventSilentAccess(base::BindOnce(&RespondCallback, &called));
  RunAllPendingTasks();

  passwords = store_->stored_passwords();
  EXPECT_EQ(4U, passwords.size());
  EXPECT_TRUE(passwords[form_.signon_realm][0].skip_zero_click);
  EXPECT_FALSE(passwords[cross_origin_form_.signon_realm][0].skip_zero_click);
  EXPECT_TRUE(passwords[affiliated_form1_.signon_realm][0].skip_zero_click);
  EXPECT_FALSE(passwords[affiliated_form2_.signon_realm][0].skip_zero_click);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithEmptyPasswordStore) {
  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(_))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr()).Times(testing::Exactly(0));

  ExpectCredentialType(CredentialMediationRequirement::kOptional, true,
                       federations, CredentialType::CREDENTIAL_TYPE_EMPTY);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithEmptyUsernames) {
  form_.username_value.clear();
  store_->AddLogin(form_);
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr()).Times(testing::Exactly(0));

  std::vector<GURL> federations;
  ExpectCredentialType(CredentialMediationRequirement::kOptional, true,
                       federations, CredentialType::CREDENTIAL_TYPE_EMPTY);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithPSLCredential) {
  store_->AddLogin(subdomain_form_);
  subdomain_form_.is_public_suffix_match = true;
  EXPECT_CALL(*client_,
              PromptUserToChooseCredentialsPtr(
                  UnorderedElementsAre(Pointee(subdomain_form_)), _, _));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr()).Times(0);

  ExpectCredentialType(CredentialMediationRequirement::kOptional, true,
                       std::vector<GURL>(),
                       CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithPSLAndNormalCredentials) {
  store_->AddLogin(form_);
  store_->AddLogin(origin_path_form_);
  store_->AddLogin(subdomain_form_);

  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(
                            UnorderedElementsAre(Pointee(origin_path_form_),
                                                 Pointee(form_)),
                            _, _));

  ExpectCredentialType(CredentialMediationRequirement::kOptional, true,
                       std::vector<GURL>(),
                       CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithEmptyAndNonemptyUsernames) {
  store_->AddLogin(form_);
  autofill::PasswordForm empty = form_;
  empty.username_value.clear();
  store_->AddLogin(empty);
  autofill::PasswordForm duplicate = form_;
  duplicate.username_element = base::ASCIIToUTF16("different_username_element");
  store_->AddLogin(duplicate);

  std::vector<GURL> federations;
  ExpectZeroClickSignInSuccess(CredentialMediationRequirement::kOptional, true,
                               federations,
                               CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithDuplicates) {
  // Add 6 credentials. Two buckets of duplicates, one empty username and one
  // federated one. There should be just 3 in the account chooser.
  form_.preferred = true;
  form_.username_element = base::ASCIIToUTF16("username_element");
  store_->AddLogin(form_);
  autofill::PasswordForm empty = form_;
  empty.username_value.clear();
  store_->AddLogin(empty);
  autofill::PasswordForm duplicate = form_;
  duplicate.username_element = base::ASCIIToUTF16("username_element2");
  duplicate.preferred = false;
  store_->AddLogin(duplicate);

  origin_path_form_.preferred = true;
  store_->AddLogin(origin_path_form_);
  duplicate = origin_path_form_;
  duplicate.username_element = base::ASCIIToUTF16("username_element4");
  duplicate.preferred = false;
  store_->AddLogin(duplicate);
  autofill::PasswordForm federated = origin_path_form_;
  federated.password_value.clear();
  federated.federation_origin =
      url::Origin::Create(GURL("https://google.com/"));
  federated.signon_realm =
      "federation://" + federated.origin.host() + "/google.com";
  store_->AddLogin(federated);

  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(
                            UnorderedElementsAre(Pointee(form_),
                                                 Pointee(origin_path_form_),
                                                 Pointee(federated)),
                            _, _));

  bool called = false;
  CredentialManagerError error;
  base::Optional<CredentialInfo> credential;
  std::vector<GURL> federations;
  federations.push_back(GURL("https://google.com/"));
  CallGet(CredentialMediationRequirement::kOptional, true, federations,
          base::BindOnce(&GetCredentialCallback, &called, &error, &credential));

  RunAllPendingTasks();
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithCrossOriginPasswordStore) {
  store_->AddLogin(cross_origin_form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(_))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr()).Times(testing::Exactly(0));

  ExpectCredentialType(CredentialMediationRequirement::kOptional, true,
                       federations, CredentialType::CREDENTIAL_TYPE_EMPTY);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithFullPasswordStore) {
  client_->set_zero_click_enabled(false);
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _))
      .Times(testing::Exactly(1));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr()).Times(testing::Exactly(0));

  bool called = false;
  CredentialManagerError error;
  base::Optional<CredentialInfo> credential;
  CallGet(CredentialMediationRequirement::kOptional, true, federations,
          base::BindOnce(&GetCredentialCallback, &called, &error, &credential));

  RunAllPendingTasks();

  EXPECT_TRUE(called);
  EXPECT_EQ(CredentialManagerError::SUCCESS, error);
}

TEST_F(
    CredentialManagerImplTest,
    CredentialManagerOnRequestCredentialWithZeroClickOnlyEmptyPasswordStore) {
  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr()).Times(testing::Exactly(0));

  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, true,
                               federations);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithZeroClickOnlyFullPasswordStore) {
  store_->AddLogin(form_);
  client_->set_first_run_seen(true);

  std::vector<GURL> federations;

  EXPECT_CALL(*client_, NotifyUserCouldBeAutoSignedInPtr(_)).Times(0);

  ExpectZeroClickSignInSuccess(CredentialMediationRequirement::kSilent, true,
                               federations,
                               CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithoutPasswords) {
  store_->AddLogin(form_);
  client_->set_first_run_seen(true);

  std::vector<GURL> federations;

  EXPECT_CALL(*client_, NotifyUserCouldBeAutoSignedInPtr(_)).Times(0);

  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, false,
                               federations);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialFederatedMatch) {
  form_.federation_origin = url::Origin::Create(GURL("https://example.com/"));
  form_.password_value = base::string16();
  store_->AddLogin(form_);
  client_->set_first_run_seen(true);

  std::vector<GURL> federations;
  federations.push_back(GURL("https://example.com/"));

  EXPECT_CALL(*client_, NotifyUserCouldBeAutoSignedInPtr(_)).Times(0);

  ExpectZeroClickSignInSuccess(CredentialMediationRequirement::kSilent, true,
                               federations,
                               CredentialType::CREDENTIAL_TYPE_FEDERATED);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialFederatedNoMatch) {
  form_.federation_origin = url::Origin::Create(GURL("https://example.com/"));
  form_.password_value = base::string16();
  store_->AddLogin(form_);
  client_->set_first_run_seen(true);

  std::vector<GURL> federations;
  federations.push_back(GURL("https://not-example.com/"));

  EXPECT_CALL(*client_, NotifyUserCouldBeAutoSignedInPtr(_)).Times(0);

  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, true,
                               federations);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialAffiliatedPasswordMatch) {
  store_->AddLogin(affiliated_form1_);
  client_->set_first_run_seen(true);
  store_->SetAffiliatedMatchHelper(
      std::make_unique<MockAffiliatedMatchHelper>());

  std::vector<GURL> federations;
  std::vector<std::string> affiliated_realms;
  affiliated_realms.push_back(kTestAndroidRealm1);
  static_cast<MockAffiliatedMatchHelper*>(store_->affiliated_match_helper())
      ->ExpectCallToGetAffiliatedAndroidRealms(
          cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);

  // We pass in 'true' for the 'include_passwords' argument to ensure that
  // password-type credentials are included as potential matches.
  ExpectZeroClickSignInSuccess(CredentialMediationRequirement::kSilent, true,
                               federations,
                               CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialAffiliatedPasswordNoMatch) {
  store_->AddLogin(affiliated_form1_);
  client_->set_first_run_seen(true);
  store_->SetAffiliatedMatchHelper(
      std::make_unique<MockAffiliatedMatchHelper>());

  std::vector<GURL> federations;
  std::vector<std::string> affiliated_realms;
  affiliated_realms.push_back(kTestAndroidRealm1);
  static_cast<MockAffiliatedMatchHelper*>(store_->affiliated_match_helper())
      ->ExpectCallToGetAffiliatedAndroidRealms(
          cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);

  // We pass in 'false' for the 'include_passwords' argument to ensure that
  // password-type credentials are excluded as potential matches.
  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, false,
                               federations);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialAffiliatedFederatedMatch) {
  affiliated_form1_.federation_origin =
      url::Origin::Create(GURL("https://example.com/"));
  affiliated_form1_.password_value = base::string16();
  store_->AddLogin(affiliated_form1_);
  client_->set_first_run_seen(true);
  store_->SetAffiliatedMatchHelper(
      std::make_unique<MockAffiliatedMatchHelper>());

  std::vector<GURL> federations;
  federations.push_back(GURL("https://example.com/"));

  std::vector<std::string> affiliated_realms;
  affiliated_realms.push_back(kTestAndroidRealm1);
  static_cast<MockAffiliatedMatchHelper*>(store_->affiliated_match_helper())
      ->ExpectCallToGetAffiliatedAndroidRealms(
          cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);

  ExpectZeroClickSignInSuccess(CredentialMediationRequirement::kSilent, true,
                               federations,
                               CredentialType::CREDENTIAL_TYPE_FEDERATED);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialAffiliatedFederatedNoMatch) {
  affiliated_form1_.federation_origin =
      url::Origin::Create(GURL("https://example.com/"));
  affiliated_form1_.password_value = base::string16();
  store_->AddLogin(affiliated_form1_);
  client_->set_first_run_seen(true);
  store_->SetAffiliatedMatchHelper(
      std::make_unique<MockAffiliatedMatchHelper>());

  std::vector<GURL> federations;
  federations.push_back(GURL("https://not-example.com/"));

  std::vector<std::string> affiliated_realms;
  affiliated_realms.push_back(kTestAndroidRealm1);
  static_cast<MockAffiliatedMatchHelper*>(store_->affiliated_match_helper())
      ->ExpectCallToGetAffiliatedAndroidRealms(
          cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);

  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, true,
                               federations);
}

TEST_F(CredentialManagerImplTest, RequestCredentialWithoutFirstRun) {
  client_->set_first_run_seen(false);

  store_->AddLogin(form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_,
              NotifyUserCouldBeAutoSignedInPtr(testing::Pointee(form_)))
      .Times(1);

  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, true,
                               federations);
}

TEST_F(CredentialManagerImplTest, RequestCredentialWithFirstRunAndSkip) {
  client_->set_first_run_seen(true);

  form_.skip_zero_click = true;
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_,
              NotifyUserCouldBeAutoSignedInPtr(testing::Pointee(form_)))
      .Times(1);

  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, true,
                               federations);
}

TEST_F(CredentialManagerImplTest, RequestCredentialWithTLSErrors) {
  // If we encounter TLS errors, we won't return credentials.
  EXPECT_CALL(*client_, IsFillingEnabledForCurrentPage())
      .WillRepeatedly(testing::Return(false));

  store_->AddLogin(form_);

  std::vector<GURL> federations;

  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, true,
                               federations);
}

TEST_F(CredentialManagerImplTest, RequestCredentialWhilePrerendering) {
  // The client disallows the credential manager for the current page.
  EXPECT_CALL(*client_, OnCredentialManagerUsed())
      .WillRepeatedly(testing::Return(false));

  store_->AddLogin(form_);

  std::vector<GURL> federations;

  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, true,
                               federations);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithZeroClickOnlyTwoPasswordStore) {
  store_->AddLogin(form_);
  store_->AddLogin(origin_path_form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr()).Times(testing::Exactly(0));

  // With two items in the password store, we shouldn't get credentials back.
  ExpectCredentialType(CredentialMediationRequirement::kSilent, true,
                       federations, CredentialType::CREDENTIAL_TYPE_EMPTY);
}

TEST_F(CredentialManagerImplTest,
       OnRequestCredentialWithZeroClickOnlyAndSkipZeroClickPasswordStore) {
  form_.skip_zero_click = true;
  store_->AddLogin(form_);
  store_->AddLogin(origin_path_form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr()).Times(testing::Exactly(0));

  // With two items in the password store, we shouldn't get credentials back,
  // even though only one item has |skip_zero_click| set |false|.
  ExpectCredentialType(CredentialMediationRequirement::kSilent, true,
                       federations, CredentialType::CREDENTIAL_TYPE_EMPTY);
}

TEST_F(CredentialManagerImplTest,
       OnRequestCredentialWithZeroClickOnlyCrossOriginPasswordStore) {
  store_->AddLogin(cross_origin_form_);

  form_.skip_zero_click = true;
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr()).Times(testing::Exactly(0));

  // We only have cross-origin zero-click credentials; they should not be
  // returned.
  ExpectCredentialType(CredentialMediationRequirement::kSilent, true,
                       federations, CredentialType::CREDENTIAL_TYPE_EMPTY);
}

TEST_F(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWhileRequestPending) {
  client_->set_zero_click_enabled(false);
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr()).Times(testing::Exactly(0));

  // 1st request.
  bool called_1 = false;
  CredentialManagerError error_1;
  base::Optional<CredentialInfo> credential_1;
  CallGet(CredentialMediationRequirement::kOptional, true, federations,
          base::BindOnce(&GetCredentialCallback, &called_1, &error_1,
                         &credential_1));
  // 2nd request.
  bool called_2 = false;
  CredentialManagerError error_2;
  base::Optional<CredentialInfo> credential_2;
  CallGet(CredentialMediationRequirement::kOptional, true, federations,
          base::BindOnce(&GetCredentialCallback, &called_2, &error_2,
                         &credential_2));

  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _))
      .Times(testing::Exactly(1));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr()).Times(testing::Exactly(0));

  // Execute the PasswordStore asynchronousness.
  RunAllPendingTasks();

  // Check that the second request triggered a rejection.
  EXPECT_TRUE(called_2);
  EXPECT_EQ(CredentialManagerError::PENDING_REQUEST, error_2);
  EXPECT_FALSE(credential_2);

  // Check that the first request resolves.
  EXPECT_TRUE(called_1);
  EXPECT_EQ(CredentialManagerError::SUCCESS, error_1);
  EXPECT_NE(CredentialType::CREDENTIAL_TYPE_EMPTY, credential_1->type);
}

TEST_F(CredentialManagerImplTest, ResetSkipZeroClickAfterPrompt) {
  // Turn on the global zero-click flag, and add two credentials in separate
  // origins, both set to skip zero-click.
  client_->set_zero_click_enabled(true);
  form_.skip_zero_click = true;
  store_->AddLogin(form_);
  cross_origin_form_.skip_zero_click = true;
  store_->AddLogin(cross_origin_form_);

  // Execute the PasswordStore asynchronousness to ensure everything is
  // written before proceeding.
  RunAllPendingTasks();

  // Sanity check.
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_EQ(2U, passwords.size());
  EXPECT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_EQ(1U, passwords[cross_origin_form_.signon_realm].size());
  EXPECT_TRUE(passwords[form_.signon_realm][0].skip_zero_click);
  EXPECT_TRUE(passwords[cross_origin_form_.signon_realm][0].skip_zero_click);

  // Trigger a request which should return the credential found in |form_|, and
  // wait for it to process.
  std::vector<GURL> federations;
  // Check that the form in the database has been updated. `OnRequestCredential`
  // generates a call to prompt the user to choose a credential.
  // MockPasswordManagerClient mocks a user choice, and when users choose a
  // credential (and have the global zero-click flag enabled), we make sure that
  // they'll be logged in again next time.
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _))
      .Times(testing::Exactly(1));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr()).Times(testing::Exactly(0));

  bool called = false;
  CredentialManagerError error;
  base::Optional<CredentialInfo> credential;
  CallGet(CredentialMediationRequirement::kOptional, true, federations,
          base::BindOnce(&GetCredentialCallback, &called, &error, &credential));

  RunAllPendingTasks();

  passwords = store_->stored_passwords();
  EXPECT_EQ(2U, passwords.size());
  EXPECT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_EQ(1U, passwords[cross_origin_form_.signon_realm].size());
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);
  EXPECT_TRUE(passwords[cross_origin_form_.signon_realm][0].skip_zero_click);
}

TEST_F(CredentialManagerImplTest, NoResetSkipZeroClickAfterPromptInIncognito) {
  EXPECT_CALL(*client_, IsIncognito()).WillRepeatedly(testing::Return(true));
  // Turn on the global zero-click flag which should be overriden by Incognito.
  client_->set_zero_click_enabled(true);
  form_.skip_zero_click = true;
  store_->AddLogin(form_);
  RunAllPendingTasks();

  // Sanity check.
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  ASSERT_EQ(1U, passwords.size());
  ASSERT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_TRUE(passwords[form_.signon_realm][0].skip_zero_click);

  // Trigger a request which should return the credential found in |form_|, and
  // wait for it to process.
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _))
      .Times(testing::Exactly(1));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr()).Times(testing::Exactly(0));

  bool called = false;
  CredentialManagerError error;
  base::Optional<CredentialInfo> credential;
  CallGet(CredentialMediationRequirement::kOptional, true, std::vector<GURL>(),
          base::BindOnce(&GetCredentialCallback, &called, &error, &credential));

  RunAllPendingTasks();

  // The form shouldn't become a zero-click one.
  passwords = store_->stored_passwords();
  ASSERT_EQ(1U, passwords.size());
  ASSERT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_TRUE(passwords[form_.signon_realm][0].skip_zero_click);
}

TEST_F(CredentialManagerImplTest, IncognitoZeroClickRequestCredential) {
  EXPECT_CALL(*client_, IsIncognito()).WillRepeatedly(testing::Return(true));
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _))
      .Times(testing::Exactly(0));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr()).Times(testing::Exactly(0));

  ExpectCredentialType(CredentialMediationRequirement::kSilent, true,
                       federations, CredentialType::CREDENTIAL_TYPE_EMPTY);
}

TEST_F(CredentialManagerImplTest, ZeroClickWithAffiliatedFormInPasswordStore) {
  // Insert the affiliated form into the store, and mock out the association
  // with the current origin. As it's the only form matching the origin, it
  // ought to be returned automagically.
  store_->AddLogin(affiliated_form1_);

  store_->SetAffiliatedMatchHelper(
      std::make_unique<MockAffiliatedMatchHelper>());

  std::vector<GURL> federations;
  std::vector<std::string> affiliated_realms;
  affiliated_realms.push_back(kTestAndroidRealm1);
  static_cast<MockAffiliatedMatchHelper*>(store_->affiliated_match_helper())
      ->ExpectCallToGetAffiliatedAndroidRealms(
          cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);

  ExpectZeroClickSignInSuccess(CredentialMediationRequirement::kSilent, true,
                               federations,
                               CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_F(CredentialManagerImplTest,
       ZeroClickWithTwoAffiliatedFormsInPasswordStore) {
  // Insert two affiliated forms into the store, and mock out the association
  // with the current origin. Multiple forms === no zero-click sign in.
  store_->AddLogin(affiliated_form1_);
  store_->AddLogin(affiliated_form2_);

  store_->SetAffiliatedMatchHelper(
      std::make_unique<MockAffiliatedMatchHelper>());

  std::vector<GURL> federations;
  std::vector<std::string> affiliated_realms;
  affiliated_realms.push_back(kTestAndroidRealm1);
  affiliated_realms.push_back(kTestAndroidRealm2);
  static_cast<MockAffiliatedMatchHelper*>(store_->affiliated_match_helper())
      ->ExpectCallToGetAffiliatedAndroidRealms(
          cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);

  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, true,
                               federations);
}

TEST_F(CredentialManagerImplTest,
       ZeroClickWithUnaffiliatedFormsInPasswordStore) {
  // Insert the affiliated form into the store, but don't mock out the
  // association with the current origin. No association === no zero-click sign
  // in.
  store_->AddLogin(affiliated_form1_);

  store_->SetAffiliatedMatchHelper(
      std::make_unique<MockAffiliatedMatchHelper>());

  std::vector<std::string> affiliated_realms;
  PasswordStore::FormDigest digest =
      cm_service_impl_->GetSynthesizedFormForOrigin();
  // First expect affiliations for the HTTPS domain.
  static_cast<MockAffiliatedMatchHelper*>(store_->affiliated_match_helper())
      ->ExpectCallToGetAffiliatedAndroidRealms(digest, affiliated_realms);

  digest.origin = HttpURLFromHttps(digest.origin);
  digest.signon_realm = digest.origin.spec();
  // The second call happens for HTTP as the migration is triggered.
  static_cast<MockAffiliatedMatchHelper*>(store_->affiliated_match_helper())
      ->ExpectCallToGetAffiliatedAndroidRealms(digest, affiliated_realms);

  std::vector<GURL> federations;
  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, true,
                               federations);
}

TEST_F(CredentialManagerImplTest,
       ZeroClickWithFormAndUnaffiliatedFormsInPasswordStore) {
  // Insert the affiliated form into the store, along with a real form for the
  // origin, and don't mock out the association with the current origin. No
  // association + existing form === zero-click sign in.
  store_->AddLogin(form_);
  store_->AddLogin(affiliated_form1_);

  store_->SetAffiliatedMatchHelper(
      std::make_unique<MockAffiliatedMatchHelper>());

  std::vector<GURL> federations;
  std::vector<std::string> affiliated_realms;
  static_cast<MockAffiliatedMatchHelper*>(store_->affiliated_match_helper())
      ->ExpectCallToGetAffiliatedAndroidRealms(
          cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);

  ExpectZeroClickSignInSuccess(CredentialMediationRequirement::kSilent, true,
                               federations,
                               CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_F(CredentialManagerImplTest, ZeroClickWithPSLCredential) {
  subdomain_form_.skip_zero_click = false;
  store_->AddLogin(subdomain_form_);

  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, true,
                               std::vector<GURL>());
}

TEST_F(CredentialManagerImplTest, ZeroClickWithPSLAndNormalCredentials) {
  form_.password_value.clear();
  form_.federation_origin = url::Origin::Create(GURL("https://google.com/"));
  form_.signon_realm = "federation://" + form_.origin.host() + "/google.com";
  form_.skip_zero_click = false;
  store_->AddLogin(form_);
  store_->AddLogin(subdomain_form_);

  std::vector<GURL> federations = {GURL("https://google.com/")};
  ExpectZeroClickSignInSuccess(CredentialMediationRequirement::kSilent, true,
                               federations,
                               CredentialType::CREDENTIAL_TYPE_FEDERATED);
}

TEST_F(CredentialManagerImplTest, ZeroClickAfterMigratingHttpCredential) {
  // There is an http credential saved. It should be migrated and used for auto
  // sign-in.
  form_.origin = HttpURLFromHttps(form_.origin);
  form_.signon_realm = form_.origin.GetOrigin().spec();
  // That is the default value for old credentials.
  form_.skip_zero_click = true;
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  ExpectZeroClickSignInSuccess(CredentialMediationRequirement::kSilent, true,
                               federations,
                               CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_F(CredentialManagerImplTest, ZeroClickOnLocalhost) {
  // HTTP scheme is valid for localhost. Nothing should crash.
  client_->set_last_committed_url(GURL("http://127.0.0.1:8000/"));

  std::vector<GURL> federations;
  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kOptional, true,
                               federations);
}

TEST_F(CredentialManagerImplTest, MediationRequiredPreventsAutoSignIn) {
  form_.skip_zero_click = false;
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  bool called = false;
  CredentialManagerError error;
  base::Optional<CredentialInfo> credential;

  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _))
      .Times(testing::Exactly(1));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr()).Times(testing::Exactly(0));
  CallGet(CredentialMediationRequirement::kRequired, true, federations,
          base::BindOnce(&GetCredentialCallback, &called, &error, &credential));

  RunAllPendingTasks();

  EXPECT_TRUE(called);
  EXPECT_EQ(CredentialManagerError::SUCCESS, error);
  EXPECT_EQ(CredentialType::CREDENTIAL_TYPE_PASSWORD, credential->type);
}

TEST_F(CredentialManagerImplTest, GetSynthesizedFormForOrigin) {
  PasswordStore::FormDigest synthesized =
      cm_service_impl_->GetSynthesizedFormForOrigin();
  EXPECT_EQ(kTestWebOrigin, synthesized.origin.spec());
  EXPECT_EQ(kTestWebOrigin, synthesized.signon_realm);
  EXPECT_EQ(autofill::PasswordForm::SCHEME_HTML, synthesized.scheme);
}

TEST_F(CredentialManagerImplTest, GetBlacklistedPasswordCredential) {
  autofill::PasswordForm blacklisted;
  blacklisted.blacklisted_by_user = true;
  blacklisted.origin = form_.origin;
  blacklisted.signon_realm = blacklisted.origin.spec();
  // Deliberately use a wrong format with a non-empty username to simulate a
  // leak. See https://crbug.com/817754.
  blacklisted.username_value = base::ASCIIToUTF16("Username");
  store_->AddLogin(blacklisted);

  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _)).Times(0);
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr()).Times(0);

  std::vector<GURL> federations;
  ExpectCredentialType(CredentialMediationRequirement::kOptional, true,
                       federations, CredentialType::CREDENTIAL_TYPE_EMPTY);
}

TEST_F(CredentialManagerImplTest, BlacklistPasswordCredential) {
  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(_));

  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_PASSWORD);
  bool called = false;
  CallStore(info, base::BindOnce(&RespondCallback, &called));
  // Allow the PasswordFormManager to talk to the password store
  RunAllPendingTasks();

  ASSERT_TRUE(client_->pending_manager());
  client_->pending_manager()->PermanentlyBlacklist();
  // Allow the PasswordFormManager to talk to the password store.
  RunAllPendingTasks();

  // Verify that the site is blacklisted.
  autofill::PasswordForm blacklisted;
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  blacklisted.blacklisted_by_user = true;
  blacklisted.origin = form_.origin;
  blacklisted.signon_realm = form_.signon_realm;
  blacklisted.type = autofill::PasswordForm::TYPE_API;
  blacklisted.date_created = passwords[form_.signon_realm][0].date_created;
  EXPECT_THAT(passwords[form_.signon_realm], testing::ElementsAre(blacklisted));
}

TEST_F(CredentialManagerImplTest, BlacklistFederatedCredential) {
  form_.federation_origin = url::Origin::Create(GURL("https://example.com/"));
  form_.password_value = base::string16();
  form_.signon_realm = "federation://example.com/example.com";

  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(_));
  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_FEDERATED);
  bool called = false;
  CallStore(info, base::BindOnce(&RespondCallback, &called));
  // Allow the PasswordFormManager to talk to the password store
  RunAllPendingTasks();

  ASSERT_TRUE(client_->pending_manager());
  client_->pending_manager()->PermanentlyBlacklist();
  // Allow the PasswordFormManager to talk to the password store.
  RunAllPendingTasks();

  // Verify that the site is blacklisted.
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  ASSERT_TRUE(passwords.count(form_.origin.spec()));
  autofill::PasswordForm blacklisted;
  blacklisted.blacklisted_by_user = true;
  blacklisted.origin = form_.origin;
  blacklisted.signon_realm = blacklisted.origin.spec();
  blacklisted.type = autofill::PasswordForm::TYPE_API;
  blacklisted.date_created =
      passwords[blacklisted.signon_realm][0].date_created;
  EXPECT_THAT(passwords[blacklisted.signon_realm],
              testing::ElementsAre(blacklisted));
}

TEST_F(CredentialManagerImplTest, RespectBlacklistingPasswordCredential) {
  autofill::PasswordForm blacklisted;
  blacklisted.blacklisted_by_user = true;
  blacklisted.origin = form_.origin;
  blacklisted.signon_realm = blacklisted.origin.spec();
  store_->AddLogin(blacklisted);

  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_PASSWORD);
  bool called = false;
  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(_));
  CallStore(info, base::BindOnce(&RespondCallback, &called));
  // Allow the PasswordFormManager to talk to the password store
  RunAllPendingTasks();

  ASSERT_TRUE(client_->pending_manager());
  EXPECT_TRUE(client_->pending_manager()->IsBlacklisted());
}

TEST_F(CredentialManagerImplTest, RespectBlacklistingFederatedCredential) {
  autofill::PasswordForm blacklisted;
  blacklisted.blacklisted_by_user = true;
  blacklisted.origin = form_.origin;
  blacklisted.signon_realm = blacklisted.origin.spec();
  store_->AddLogin(blacklisted);

  form_.federation_origin = url::Origin::Create(GURL("https://example.com/"));
  form_.password_value = base::string16();
  form_.signon_realm = "federation://example.com/example.com";
  CredentialInfo info(form_, CredentialType::CREDENTIAL_TYPE_FEDERATED);
  bool called = false;
  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(_));
  CallStore(info, base::BindOnce(&RespondCallback, &called));
  // Allow the PasswordFormManager to talk to the password store
  RunAllPendingTasks();

  ASSERT_TRUE(client_->pending_manager());
  EXPECT_TRUE(client_->pending_manager()->IsBlacklisted());
}

}  // namespace password_manager
