// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_impl.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/password_manager/core/browser/affiliation/mock_affiliated_match_helper.h"
#include "components/password_manager/core/browser/credential_type_flags.h"
#include "components/password_manager/core/browser/credential_manager_pending_request_task.h"
#include "components/password_manager/core/browser/credential_manager_utils.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"
#include "components/password_manager/core/browser/leak_detection/mock_leak_detection_check_factory.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "components/os_crypt/sync/os_crypt_mocker.h"
#endif

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

namespace password_manager {

namespace {

const char kTestWebOrigin[] = "https://example.com/";
const char kTestAndroidRealm1[] = "android://hash@com.example.one.android/";
const char kTestAndroidRealm2[] = "android://hash@com.example.two.android/";

constexpr int kIncludePasswordsFlag =
    static_cast<int>(CredentialTypeFlags::kPassword);

class MockLeakDetectionCheck : public LeakDetectionCheck {
 public:
  MOCK_METHOD(
      void,
      Start,
      (LeakDetectionInitiator, const GURL&, std::u16string, std::u16string),
      (override));
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_METHOD(bool,
              IsSavingAndFillingEnabled,
              (const GURL&),
              (const, override));
  MOCK_METHOD(bool, IsFillingEnabled, (const GURL&), (const, override));
  MOCK_METHOD(bool, IsOffTheRecord, (), (const, override));
  MOCK_METHOD(bool, NotifyUserAutoSigninPtr, (), ());
  MOCK_METHOD(bool,
              NotifyUserCouldBeAutoSignedInPtr,
              (PasswordForm * form),
              ());
  MOCK_METHOD(void, NotifyStorePasswordCalled, (), (override));
  MOCK_METHOD(bool,
              PromptUserToSaveOrUpdatePassword,
              (std::unique_ptr<PasswordFormManagerForUI>, bool),
              (override));
  MOCK_METHOD(bool,
              PromptUserToChooseCredentialsPtr,
              (const std::vector<PasswordForm*>& local_forms,
               const url::Origin& origin,
               CredentialsCallback callback),
              ());
  MOCK_METHOD(void,
              PasswordWasAutofilled,
              (base::span<const PasswordForm>,
               const url::Origin&,
               (base::span<const PasswordForm>),
               bool was_autofilled_on_pageload),
              (override));

  explicit MockPasswordManagerClient(PasswordStoreInterface* profile_store,
                                     PasswordStoreInterface* account_store)
      : profile_store_(profile_store),
        account_store_(account_store),
        password_manager_(this) {
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterBooleanPref(
        prefs::kWasAutoSignInFirstRunExperienceShown, true);
    prefs_->registry()->RegisterBooleanPref(
        prefs::kPasswordLeakDetectionEnabled, true);
    prefs_->registry()->RegisterBooleanPref(::prefs::kSafeBrowsingEnabled,
                                            true);
    prefs_->registry()->RegisterBooleanPref(::prefs::kSafeBrowsingEnhanced,
                                            false);
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    OSCryptMocker::SetUp();
    prefs_->registry()->RegisterIntegerPref(
        password_manager::prefs::kRelaunchChromeBubbleDismissedCounter, 0);
#endif
  }
  MockPasswordManagerClient(const MockPasswordManagerClient&) = delete;
  MockPasswordManagerClient& operator=(const MockPasswordManagerClient&) =
      delete;
  ~MockPasswordManagerClient() override = default;

  bool IsAutoSignInEnabled() const override { return auto_sign_in_enabled_; }

  void NotifyUserCouldBeAutoSignedIn(
      std::unique_ptr<PasswordForm> form) override {
    NotifyUserCouldBeAutoSignedInPtr(form.get());
  }

  PasswordStoreInterface* GetProfilePasswordStore() const override {
    return profile_store_;
  }
  PasswordStoreInterface* GetAccountPasswordStore() const override {
    return account_store_;
  }

  PrefService* GetPrefs() const override { return prefs_.get(); }

  PrefService* GetLocalStatePrefs() const override { return prefs_.get(); }

  const PasswordManager* GetPasswordManager() const override {
    return &password_manager_;
  }

  url::Origin GetLastCommittedOrigin() const override {
    return url::Origin::Create(last_committed_url_);
  }

  bool PromptUserToChooseCredentials(
      std::vector<std::unique_ptr<PasswordForm>> local_forms,
      const url::Origin& origin,
      CredentialsCallback callback) override {
    EXPECT_FALSE(local_forms.empty());
    const PasswordForm* form = local_forms[0].get();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  base::Owned(new PasswordForm(*form))));
    PromptUserToChooseCredentialsPtr(
        base::ToVector(local_forms, &std::unique_ptr<PasswordForm>::get),
        origin, base::DoNothing());
    return true;
  }

  void NotifyUserAutoSignin(
      std::vector<std::unique_ptr<PasswordForm>> local_forms,
      const url::Origin& origin) override {
    EXPECT_FALSE(local_forms.empty());
    NotifyUserAutoSigninPtr();
  }

  void set_zero_click_enabled(bool zero_click_enabled) {
    auto_sign_in_enabled_ = zero_click_enabled;
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
  raw_ptr<PasswordStoreInterface> profile_store_;
  raw_ptr<PasswordStoreInterface> account_store_;
  PasswordManager password_manager_;
  GURL last_committed_url_{kTestWebOrigin};
  bool auto_sign_in_enabled_ = true;
};

// Callbacks from CredentialManagerImpl methods
void RespondCallback(bool* called) {
  *called = true;
}

void GetCredentialCallback(bool* called,
                           CredentialManagerError* out_error,
                           std::optional<CredentialInfo>* out_info,
                           CredentialManagerError error,
                           const std::optional<CredentialInfo>& info) {
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

// Test param controls whether to enable the account storage feature.
class CredentialManagerImplTest : public testing::Test,
                                  public testing::WithParamInterface<bool> {
 public:
  CredentialManagerImplTest() = default;

  void SetUp() override {
    store_ = new TestPasswordStore;

    fake_affiliation_service_ =
        std::make_unique<affiliations::FakeAffiliationService>();
    auto owning_mock_match_helper =
        std::make_unique<NiceMock<MockAffiliatedMatchHelper>>(
            fake_affiliation_service_.get());
    mock_match_helper_ = owning_mock_match_helper.get();
    store_->Init(/*prefs=*/nullptr, std::move(owning_mock_match_helper));

    if (GetParam()) {
      account_store_ = new TestPasswordStore(IsAccountStore(true));
      account_store_->Init(/*prefs=*/nullptr,
                           /*affiliated_match_helper=*/nullptr);
    }
    client_ = std::make_unique<testing::NiceMock<MockPasswordManagerClient>>(
        store_.get(), account_store_.get());
    cm_service_impl_ = std::make_unique<CredentialManagerImpl>(client_.get());
    cm_service_impl_->set_leak_factory(
        std::make_unique<NiceMock<MockLeakDetectionCheckFactory>>());

    ON_CALL(*client_, IsSavingAndFillingEnabled).WillByDefault(Return(true));
    ON_CALL(*client_, IsFillingEnabled).WillByDefault(Return(true));
    ON_CALL(*client_, IsOffTheRecord()).WillByDefault(Return(false));

    form_.username_value = u"Username";
    form_.display_name = u"Display Name";
    form_.icon_url = GURL("https://example.com/icon.png");
    form_.password_value = u"Password";
    form_.url = client_->GetLastCommittedOrigin().GetURL();
    form_.signon_realm = form_.url.DeprecatedGetOriginAsURL().spec();
    form_.scheme = PasswordForm::Scheme::kHtml;
    form_.skip_zero_click = false;

    affiliated_form1_.username_value = u"Affiliated 1";
    affiliated_form1_.display_name = u"Display Name";
    affiliated_form1_.password_value = u"Password";
    affiliated_form1_.url = GURL(kTestAndroidRealm1);
    affiliated_form1_.signon_realm = kTestAndroidRealm1;
    affiliated_form1_.scheme = PasswordForm::Scheme::kHtml;
    affiliated_form1_.skip_zero_click = false;

    affiliated_form2_.username_value = u"Affiliated 2";
    affiliated_form2_.display_name = u"Display Name";
    affiliated_form2_.password_value = u"Password";
    affiliated_form2_.url = GURL(kTestAndroidRealm2);
    affiliated_form2_.signon_realm = kTestAndroidRealm2;
    affiliated_form2_.scheme = PasswordForm::Scheme::kHtml;
    affiliated_form2_.skip_zero_click = false;

    origin_path_form_.username_value = u"Username 2";
    origin_path_form_.display_name = u"Display Name 2";
    origin_path_form_.password_value = u"Password 2";
    origin_path_form_.url = GURL("https://example.com/path");
    origin_path_form_.signon_realm =
        origin_path_form_.url.DeprecatedGetOriginAsURL().spec();
    origin_path_form_.scheme = PasswordForm::Scheme::kHtml;
    origin_path_form_.skip_zero_click = false;

    subdomain_form_.username_value = u"Username 2";
    subdomain_form_.display_name = u"Display Name 2";
    subdomain_form_.password_value = u"Password 2";
    subdomain_form_.url = GURL("https://subdomain.example.com/path");
    subdomain_form_.signon_realm =
        subdomain_form_.url.DeprecatedGetOriginAsURL().spec();
    subdomain_form_.scheme = PasswordForm::Scheme::kHtml;
    subdomain_form_.skip_zero_click = false;

    cross_origin_form_.username_value = u"Username";
    cross_origin_form_.display_name = u"Display Name";
    cross_origin_form_.password_value = u"Password";
    cross_origin_form_.url = GURL("https://example.net/");
    cross_origin_form_.signon_realm =
        cross_origin_form_.url.DeprecatedGetOriginAsURL().spec();
    cross_origin_form_.scheme = PasswordForm::Scheme::kHtml;
    cross_origin_form_.skip_zero_click = false;

    store_->Clear();
    EXPECT_TRUE(store_->IsEmpty());
  }

  void TearDown() override {
    cm_service_impl_.reset();
    // Reset the match helper, since it references an object owned by the store.
    mock_match_helper_ = nullptr;
    if (account_store_) {
      account_store_->ShutdownOnUIThread();
    }
    store_->ShutdownOnUIThread();

    // It's needed to cleanup the password store asynchronously.
    RunAllPendingTasks();
  }

  void ExpectZeroClickSignInFailure(CredentialMediationRequirement mediation,
                                    bool include_passwords,
                                    const std::vector<GURL>& federations) {
    bool called = false;
    CredentialManagerError error;
    std::optional<CredentialInfo> credential;
    EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr).Times(0);
    EXPECT_CALL(*client_, NotifyUserAutoSigninPtr).Times(0);
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
    std::optional<CredentialInfo> credential;
    EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr).Times(0);
    EXPECT_CALL(*client_, NotifyUserAutoSigninPtr);
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
    std::optional<CredentialInfo> credential;
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
    cm_service_impl_->Get(mediation,
                          /*requested_credential_type_flags=*/
                              include_passwords ? kIncludePasswordsFlag : 0,
                          federations, std::move(callback));
  }

  void RunAllPendingTasks() { task_environment_.RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  PasswordForm form_;
  PasswordForm affiliated_form1_;
  PasswordForm affiliated_form2_;
  PasswordForm origin_path_form_;
  PasswordForm subdomain_form_;
  PasswordForm cross_origin_form_;
  scoped_refptr<TestPasswordStore> store_;
  scoped_refptr<TestPasswordStore> account_store_;
  std::unique_ptr<testing::NiceMock<MockPasswordManagerClient>> client_;
  std::unique_ptr<affiliations::FakeAffiliationService>
      fake_affiliation_service_;
  raw_ptr<MockAffiliatedMatchHelper> mock_match_helper_ = nullptr;
  std::unique_ptr<CredentialManagerImpl> cm_service_impl_;
};

TEST_P(CredentialManagerImplTest, IsZeroClickAllowed) {
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

TEST_P(CredentialManagerImplTest, CredentialManagerOnStoreEmptyCredential) {
  EXPECT_CALL(*client_, PromptUserToSaveOrUpdatePassword).Times(0);

  bool called = false;
  auto info = CredentialInfo();
  CallStore(info, base::BindOnce(&RespondCallback, &called));

  RunAllPendingTasks();

  EXPECT_TRUE(called);
}

TEST_P(CredentialManagerImplTest, CredentialManagerOnStore) {
  auto info = PasswordFormToCredentialInfo(form_);
  std::unique_ptr<PasswordFormManagerForUI> pending_manager;
  EXPECT_CALL(*client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&pending_manager, true));
  EXPECT_CALL(*client_, NotifyStorePasswordCalled);

  bool called = false;
  CallStore(info, base::BindOnce(&RespondCallback, &called));

  // Allow the PasswordFormManager to talk to the password store, determine
  // that the form is new, and set it as pending.
  RunAllPendingTasks();

  EXPECT_TRUE(called);

  PasswordForm new_form = pending_manager->GetPendingCredentials();
  EXPECT_EQ(form_.username_value, new_form.username_value);
  EXPECT_EQ(form_.display_name, new_form.display_name);
  EXPECT_EQ(form_.password_value, new_form.password_value);
  EXPECT_EQ(form_.url, new_form.url);
  EXPECT_EQ(form_.signon_realm, new_form.signon_realm);
  EXPECT_EQ(new_form.federation_origin, url::SchemeHostPort());
  EXPECT_EQ(form_.icon_url, new_form.icon_url);
  EXPECT_FALSE(form_.skip_zero_click);
  EXPECT_EQ(PasswordForm::Scheme::kHtml, new_form.scheme);
}

TEST_P(CredentialManagerImplTest, CredentialManagerOnStoreFederated) {
  std::unique_ptr<PasswordFormManagerForUI> pending_manager;
  EXPECT_CALL(*client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&pending_manager, true));
  EXPECT_CALL(*client_, NotifyStorePasswordCalled());

  bool called = false;
  form_.federation_origin = url::SchemeHostPort(GURL("https://google.com/"));
  form_.password_value = std::u16string();
  form_.signon_realm = "federation://example.com/google.com";
  auto info = PasswordFormToCredentialInfo(form_);
  CallStore(info, base::BindOnce(&RespondCallback, &called));

  // Allow the PasswordFormManager to talk to the password store, determine
  // that the form is new, and set it as pending.
  RunAllPendingTasks();

  EXPECT_TRUE(called);

  PasswordForm new_form = pending_manager->GetPendingCredentials();
  EXPECT_EQ(form_.username_value, new_form.username_value);
  EXPECT_EQ(form_.display_name, new_form.display_name);
  EXPECT_EQ(form_.password_value, new_form.password_value);
  EXPECT_EQ(form_.url, new_form.url);
  EXPECT_EQ(form_.signon_realm, new_form.signon_realm);
  EXPECT_EQ(form_.federation_origin, new_form.federation_origin);
  EXPECT_EQ(form_.icon_url, new_form.icon_url);
  EXPECT_FALSE(form_.skip_zero_click);
  EXPECT_EQ(PasswordForm::Scheme::kHtml, new_form.scheme);
}

TEST_P(CredentialManagerImplTest, StoreFederatedAfterPassword) {
  // Populate the PasswordStore with a form.
  store_->AddLogin(form_);

  PasswordForm federated = form_;
  federated.password_value.clear();
  federated.type = PasswordForm::Type::kApi;
  federated.federation_origin =
      url::SchemeHostPort(GURL("https://google.com/"));
  federated.signon_realm = "federation://example.com/google.com";
  auto info = PasswordFormToCredentialInfo(federated);
  std::unique_ptr<PasswordFormManagerForUI> pending_manager;
  EXPECT_CALL(*client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&pending_manager, true));
  EXPECT_CALL(*client_, NotifyStorePasswordCalled);

  bool called = false;
  CallStore(info, base::BindOnce(&RespondCallback, &called));

  // Allow the PasswordFormManager to talk to the password store, determine
  // that the form is new, and set it as pending.
  RunAllPendingTasks();

  EXPECT_TRUE(called);
  pending_manager->Save();

  RunAllPendingTasks();
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_THAT(passwords["https://example.com/"],
              ElementsAre(MatchesFormExceptStore(form_)));
  federated.date_created =
      passwords["federation://example.com/google.com"][0].date_created;
  federated.date_last_used =
      passwords["federation://example.com/google.com"][0].date_last_used;
  federated.date_password_modified =
      passwords["federation://example.com/google.com"][0]
          .date_password_modified;
  EXPECT_THAT(passwords["federation://example.com/google.com"],
              ElementsAre(MatchesFormExceptStore(federated)));
}

TEST_P(CredentialManagerImplTest, CredentialManagerStoreOverwrite) {
  // Add an unrelated form to complicate the task.
  store_->AddLogin(origin_path_form_);
  // Populate the PasswordStore with a form.
  form_.display_name = u"Old Name";
  form_.icon_url = GURL();
  store_->AddLogin(form_);
  RunAllPendingTasks();

  // Calling 'Store' with a credential that matches |form_| should update
  // the password without prompting the user.
  auto info = PasswordFormToCredentialInfo(form_);
  info.password = u"Totally new password.";
  info.name = u"New Name";
  info.icon = GURL("https://example.com/icon.png");
  EXPECT_CALL(*client_, PromptUserToSaveOrUpdatePassword).Times(0);
  EXPECT_CALL(*client_, NotifyStorePasswordCalled);
  bool called = false;
  CallStore(info, base::BindOnce(&RespondCallback, &called));

  // Allow the PasswordFormManager to talk to the password store, determine
  // the form is a match for an existing form, and update the PasswordStore.
  RunAllPendingTasks();

  EXPECT_TRUE(called);

  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_EQ(1U, passwords.size());
  EXPECT_EQ(2U, passwords[form_.signon_realm].size());
  EXPECT_THAT(origin_path_form_,
              MatchesFormExceptStore(passwords[form_.signon_realm][0]));
  EXPECT_EQ(u"Totally new password.",
            passwords[form_.signon_realm][1].password_value);
  EXPECT_EQ(u"New Name", passwords[form_.signon_realm][1].display_name);
  EXPECT_EQ(GURL("https://example.com/icon.png"),
            passwords[form_.signon_realm][1].icon_url);
}

TEST_P(CredentialManagerImplTest,
       CredentialManagerStorePSLMatchDoesNotTriggerBubble) {
  PasswordForm psl_form = subdomain_form_;
  psl_form.username_value = form_.username_value;
  psl_form.password_value = form_.password_value;
  store_->AddLogin(psl_form);

  // Calling 'Store' with a new credential that is a PSL match for an existing
  // credential with identical username and password should result in a silent
  // save without prompting the user.
  auto info = PasswordFormToCredentialInfo(form_);
  EXPECT_CALL(*client_, PromptUserToSaveOrUpdatePassword).Times(0);
  EXPECT_CALL(*client_, NotifyStorePasswordCalled);
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

TEST_P(CredentialManagerImplTest,
       CredentialManagerStorePSLMatchWithDifferentUsernameTriggersBubble) {
  std::u16string delta = u"_totally_different";
  PasswordForm psl_form = subdomain_form_;
  psl_form.username_value = form_.username_value + delta;
  psl_form.password_value = form_.password_value;
  store_->AddLogin(psl_form);

  // Calling 'Store' with a new credential that is a PSL match for an existing
  // credential but has a different username should prompt the user and not
  // result in a silent save.
  auto info = PasswordFormToCredentialInfo(form_);
  std::unique_ptr<PasswordFormManagerForUI> pending_manager;
  EXPECT_CALL(*client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&pending_manager, true));
  EXPECT_CALL(*client_, NotifyStorePasswordCalled);
  bool called = false;
  CallStore(info, base::BindOnce(&RespondCallback, &called));
  RunAllPendingTasks();
  EXPECT_TRUE(called);

  // Check that only the initial credential is present in the password store
  // and the new one is still pending.
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_EQ(1U, passwords.size());
  EXPECT_EQ(1U, passwords[psl_form.signon_realm].size());

  const auto& pending_cred = pending_manager->GetPendingCredentials();
  EXPECT_EQ(info.id, pending_cred.username_value);
  EXPECT_EQ(info.password, pending_cred.password_value);
}

TEST_P(CredentialManagerImplTest,
       CredentialManagerStorePSLMatchWithDifferentPasswordTriggersBubble) {
  std::u16string delta = u"_totally_different";
  PasswordForm psl_form = subdomain_form_;
  psl_form.username_value = form_.username_value;
  psl_form.password_value = form_.password_value + delta;
  store_->AddLogin(psl_form);

  // Calling 'Store' with a new credential that is a PSL match for an existing
  // credential but has a different password should prompt the user and not
  // result in a silent save.
  auto info = PasswordFormToCredentialInfo(form_);
  std::unique_ptr<PasswordFormManagerForUI> pending_manager;
  EXPECT_CALL(*client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&pending_manager, true));
  EXPECT_CALL(*client_, NotifyStorePasswordCalled);
  bool called = false;
  CallStore(info, base::BindOnce(&RespondCallback, &called));
  RunAllPendingTasks();
  EXPECT_TRUE(called);

  // Check that only the initial credential is present in the password store
  // and the new one is still pending.
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_EQ(1U, passwords.size());
  EXPECT_EQ(1U, passwords[psl_form.signon_realm].size());

  const auto& pending_cred = pending_manager->GetPendingCredentials();
  EXPECT_EQ(info.id, pending_cred.username_value);
  EXPECT_EQ(info.password, pending_cred.password_value);
}

TEST_P(CredentialManagerImplTest, CredentialManagerStoreOverwriteZeroClick) {
  form_.skip_zero_click = true;
  store_->AddLogin(form_);
  RunAllPendingTasks();

  // Calling 'Store' with a credential that matches |form_| should update
  // the credential without prompting the user.
  auto info = PasswordFormToCredentialInfo(form_);
  bool called = false;
  EXPECT_CALL(*client_, NotifyStorePasswordCalled);
  CallStore(info, base::BindOnce(&RespondCallback, &called));

  // Allow the PasswordFormManager to talk to the password store, determine
  // the form is a match for an existing form, and update the PasswordStore.
  RunAllPendingTasks();

  // Verify that the update toggled the skip_zero_click flag off.
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);
}

TEST_P(CredentialManagerImplTest,
       CredentialManagerFederatedStoreOverwriteZeroClick) {
  form_.federation_origin = url::SchemeHostPort(GURL("https://example.com/"));
  form_.password_value = std::u16string();
  form_.skip_zero_click = true;
  form_.signon_realm = "federation://example.com/example.com";
  form_.match_type = PasswordForm::MatchType::kExact;
  store_->AddLogin(form_);
  RunAllPendingTasks();

  // Calling 'Store' with a credential that matches |form_| should update
  // the credential without prompting the user.
  auto info = PasswordFormToCredentialInfo(form_);
  bool called = false;
  EXPECT_CALL(*client_, NotifyStorePasswordCalled);
  CallStore(info, base::BindOnce(&RespondCallback, &called));

  // Allow the PasswordFormManager to talk to the password store, determine
  // the form is a match for an existing form, and update the PasswordStore.
  RunAllPendingTasks();

  // Verify that the update toggled the skip_zero_click flag off.
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);
}

TEST_P(CredentialManagerImplTest, CredentialManagerGetOverwriteZeroClick) {
  // Set the global zero click flag on, and populate the PasswordStore with a
  // form that's set to skip zero click and has a primary key that won't match
  // credentials initially created via `store()`.
  client_->set_zero_click_enabled(true);
  form_.skip_zero_click = true;
  form_.username_element = u"username-element";
  form_.password_element = u"password-element";
  form_.url = GURL("https://example.com/old_form.html");
  store_->AddLogin(form_);
  RunAllPendingTasks();

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr);
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr).Times(0);

  bool called = false;
  CredentialManagerError error;
  std::optional<CredentialInfo> credential;
  CallGet(CredentialMediationRequirement::kOptional, true, federations,
          base::BindOnce(&GetCredentialCallback, &called, &error, &credential));

  RunAllPendingTasks();

  EXPECT_TRUE(called);
  EXPECT_EQ(CredentialManagerError::SUCCESS, error);

  // Verify that the update toggled the skip_zero_click flag.
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);
}

TEST_P(CredentialManagerImplTest,
       CredentialManagerSignInWithSavingDisabledForCurrentPage) {
  auto info = PasswordFormToCredentialInfo(form_);
  EXPECT_CALL(*client_, IsSavingAndFillingEnabled(form_.url))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*client_, PromptUserToSaveOrUpdatePassword).Times(0);
  EXPECT_CALL(*client_, NotifyStorePasswordCalled).Times(0);

  bool called = false;
  CallStore(info, base::BindOnce(&RespondCallback, &called));

  RunAllPendingTasks();

  EXPECT_TRUE(called);
}

TEST_P(CredentialManagerImplTest, CredentialManagerOnPreventSilentAccess) {
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

TEST_P(CredentialManagerImplTest,
       CredentialManagerOnPreventSilentAccessIncognito) {
  EXPECT_CALL(*client_, IsSavingAndFillingEnabled)
      .WillRepeatedly(Return(false));
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

TEST_P(CredentialManagerImplTest,
       CredentialManagerOnPreventMediatedAccessIncognito) {
  EXPECT_CALL(*client_, IsOffTheRecord()).WillRepeatedly(Return(true));
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr).Times(0);
  store_->AddLogin(form_);
  RunAllPendingTasks();

  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  ASSERT_EQ(1U, passwords.size());
  ASSERT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);

  bool called = false;
  CredentialManagerError error;
  std::vector<GURL> federations;
  std::optional<CredentialInfo> credential;
  CallGet(CredentialMediationRequirement::kOptional, true, federations,
          base::BindOnce(&GetCredentialCallback, &called, &error, &credential));
  RunAllPendingTasks();

  EXPECT_TRUE(called);
  ASSERT_TRUE(credential);
  EXPECT_FALSE(credential->password);
  EXPECT_EQ(CredentialManagerError::SUCCESS, error);
}

TEST_P(CredentialManagerImplTest,
       CredentialManagerOnPreventSilentAccessWithAffiliation) {
  store_->AddLogin(form_);
  store_->AddLogin(cross_origin_form_);
  store_->AddLogin(affiliated_form1_);
  store_->AddLogin(affiliated_form2_);

  std::vector<GURL> federations;
  std::vector<std::string> affiliated_realms;
  affiliated_realms.push_back(kTestAndroidRealm1);
  mock_match_helper_->ExpectCallToGetAffiliatedAndGrouped(
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

TEST_P(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithEmptyPasswordStore) {
  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToSaveOrUpdatePassword).Times(0);
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr).Times(0);
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr).Times(0);

  ExpectCredentialType(CredentialMediationRequirement::kOptional, true,
                       federations, CredentialType::CREDENTIAL_TYPE_EMPTY);
}

TEST_P(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithEmptyUsernames) {
  form_.username_value.clear();
  store_->AddLogin(form_);
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr).Times(0);
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr).Times(0);

  std::vector<GURL> federations;
  ExpectCredentialType(CredentialMediationRequirement::kOptional, true,
                       federations, CredentialType::CREDENTIAL_TYPE_EMPTY);
}

TEST_P(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithPSLCredential) {
  store_->AddLogin(subdomain_form_);
  subdomain_form_.match_type = PasswordForm::MatchType::kPSL;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(
                            UnorderedElementsAre(Pointee(
                                MatchesFormExceptStore(subdomain_form_))),
                            _, _));
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr).Times(0);

  ExpectCredentialType(CredentialMediationRequirement::kOptional, true,
                       std::vector<GURL>(),
                       CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_P(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithPSLAndNormalCredentials) {
  store_->AddLogin(form_);
  store_->AddLogin(origin_path_form_);
  store_->AddLogin(subdomain_form_);

  form_.match_type = PasswordForm::MatchType::kExact;
  origin_path_form_.match_type = PasswordForm::MatchType::kExact;

  EXPECT_CALL(*client_,
              PromptUserToChooseCredentialsPtr(
                  UnorderedElementsAre(
                      Pointee(MatchesFormExceptStore(origin_path_form_)),
                      Pointee(MatchesFormExceptStore(form_))),
                  _, _));

  ExpectCredentialType(CredentialMediationRequirement::kOptional, true,
                       std::vector<GURL>(),
                       CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_P(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithEmptyAndNonemptyUsernames) {
  store_->AddLogin(form_);
  PasswordForm empty = form_;
  empty.username_value.clear();
  store_->AddLogin(empty);
  PasswordForm duplicate = form_;
  duplicate.username_element = u"different_username_element";
  store_->AddLogin(duplicate);

  std::vector<GURL> federations;
  ExpectZeroClickSignInSuccess(CredentialMediationRequirement::kOptional, true,
                               federations,
                               CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_P(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithDuplicates) {
  // Add 6 credentials. Two buckets of duplicates, one empty username and one
  // federated one. There should be just 3 in the account chooser.
  form_.username_element = u"username_element";
  store_->AddLogin(form_);
  PasswordForm empty = form_;
  empty.username_value.clear();
  store_->AddLogin(empty);
  PasswordForm duplicate = form_;
  duplicate.username_element = u"username_element2";
  store_->AddLogin(duplicate);

  store_->AddLogin(origin_path_form_);
  duplicate = origin_path_form_;
  duplicate.username_element = u"username_element4";
  store_->AddLogin(duplicate);
  PasswordForm federated = origin_path_form_;
  federated.password_value.clear();
  federated.federation_origin =
      url::SchemeHostPort(GURL("https://google.com/"));
  federated.signon_realm =
      "federation://" + federated.url.host() + "/google.com";
  store_->AddLogin(federated);

  form_.match_type = PasswordForm::MatchType::kExact;
  origin_path_form_.match_type = PasswordForm::MatchType::kExact;
  federated.match_type = PasswordForm::MatchType::kExact;

  EXPECT_CALL(*client_,
              PromptUserToChooseCredentialsPtr(
                  UnorderedElementsAre(
                      Pointee(MatchesFormExceptStore(form_)),
                      Pointee(MatchesFormExceptStore(origin_path_form_)),
                      Pointee(MatchesFormExceptStore(federated))),
                  _, _));

  bool called = false;
  CredentialManagerError error;
  std::optional<CredentialInfo> credential;
  std::vector<GURL> federations;
  federations.emplace_back("https://google.com/");
  CallGet(CredentialMediationRequirement::kOptional, true, federations,
          base::BindOnce(&GetCredentialCallback, &called, &error, &credential));

  RunAllPendingTasks();
}

TEST_P(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithCrossOriginPasswordStore) {
  store_->AddLogin(cross_origin_form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToSaveOrUpdatePassword).Times(0);
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr).Times(0);
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr).Times(0);

  ExpectCredentialType(CredentialMediationRequirement::kOptional, true,
                       federations, CredentialType::CREDENTIAL_TYPE_EMPTY);
}

TEST_P(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithFullPasswordStore) {
  client_->set_zero_click_enabled(false);
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr);
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr).Times(0);

  bool called = false;
  CredentialManagerError error;
  std::optional<CredentialInfo> credential;
  CallGet(CredentialMediationRequirement::kOptional, true, federations,
          base::BindOnce(&GetCredentialCallback, &called, &error, &credential));

  RunAllPendingTasks();

  EXPECT_TRUE(called);
  EXPECT_EQ(CredentialManagerError::SUCCESS, error);
}

TEST_P(
    CredentialManagerImplTest,
    CredentialManagerOnRequestCredentialWithZeroClickOnlyEmptyPasswordStore) {
  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr).Times(0);
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr).Times(0);

  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, true,
                               federations);
}

TEST_P(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithZeroClickOnlyFullPasswordStore) {
  store_->AddLogin(form_);
  client_->set_first_run_seen(true);

  std::vector<GURL> federations;

  EXPECT_CALL(*client_, NotifyUserCouldBeAutoSignedInPtr).Times(0);

  ExpectZeroClickSignInSuccess(CredentialMediationRequirement::kSilent, true,
                               federations,
                               CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_P(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithoutPasswords) {
  store_->AddLogin(form_);
  client_->set_first_run_seen(true);

  std::vector<GURL> federations;

  EXPECT_CALL(*client_, NotifyUserCouldBeAutoSignedInPtr).Times(0);
  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, false,
                               federations);
}

TEST_P(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialFederatedMatch) {
  form_.federation_origin = url::SchemeHostPort(GURL("https://example.com/"));
  form_.password_value = std::u16string();
  store_->AddLogin(form_);
  client_->set_first_run_seen(true);

  std::vector<GURL> federations;
  federations.emplace_back("https://example.com/");

  EXPECT_CALL(*client_, NotifyUserCouldBeAutoSignedInPtr).Times(0);

  ExpectZeroClickSignInSuccess(CredentialMediationRequirement::kSilent, true,
                               federations,
                               CredentialType::CREDENTIAL_TYPE_FEDERATED);
}

TEST_P(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialFederatedNoMatch) {
  form_.federation_origin = url::SchemeHostPort(GURL("https://example.com/"));
  form_.password_value = std::u16string();
  store_->AddLogin(form_);
  client_->set_first_run_seen(true);

  std::vector<GURL> federations;
  federations.emplace_back("https://not-example.com/");

  EXPECT_CALL(*client_, NotifyUserCouldBeAutoSignedInPtr).Times(0);

  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, true,
                               federations);
}

TEST_P(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialAffiliatedPasswordMatch) {
  store_->AddLogin(affiliated_form1_);
  client_->set_first_run_seen(true);

  std::vector<GURL> federations;
  std::vector<std::string> affiliated_realms;
  affiliated_realms.push_back(kTestAndroidRealm1);
  mock_match_helper_->ExpectCallToGetAffiliatedAndGrouped(
      cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);

  // We pass in 'true' for the 'include_passwords' argument to ensure that
  // password-type credentials are included as potential matches.
  ExpectZeroClickSignInSuccess(CredentialMediationRequirement::kSilent, true,
                               federations,
                               CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_P(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialAffiliatedPasswordNoMatch) {
  store_->AddLogin(affiliated_form1_);
  client_->set_first_run_seen(true);

  std::vector<GURL> federations;
  std::vector<std::string> affiliated_realms;
  affiliated_realms.push_back(kTestAndroidRealm1);
  mock_match_helper_->ExpectCallToGetAffiliatedAndGrouped(
      cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);

  // We pass in 'false' for the 'include_passwords' argument to ensure that
  // password-type credentials are excluded as potential matches.
  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, false,
                               federations);
}

TEST_P(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialAffiliatedFederatedMatch) {
  affiliated_form1_.federation_origin =
      url::SchemeHostPort(GURL("https://example.com/"));
  affiliated_form1_.password_value = std::u16string();
  store_->AddLogin(affiliated_form1_);
  client_->set_first_run_seen(true);

  std::vector<GURL> federations;
  federations.emplace_back("https://example.com/");

  std::vector<std::string> affiliated_realms;
  affiliated_realms.push_back(kTestAndroidRealm1);
  mock_match_helper_->ExpectCallToGetAffiliatedAndGrouped(
      cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);

  ExpectZeroClickSignInSuccess(CredentialMediationRequirement::kSilent, true,
                               federations,
                               CredentialType::CREDENTIAL_TYPE_FEDERATED);
}

TEST_P(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialAffiliatedFederatedNoMatch) {
  affiliated_form1_.federation_origin =
      url::SchemeHostPort(GURL("https://example.com/"));
  affiliated_form1_.password_value = std::u16string();
  store_->AddLogin(affiliated_form1_);
  client_->set_first_run_seen(true);

  std::vector<GURL> federations;
  federations.emplace_back("https://not-example.com/");

  std::vector<std::string> affiliated_realms;
  affiliated_realms.push_back(kTestAndroidRealm1);
  mock_match_helper_->ExpectCallToGetAffiliatedAndGrouped(
      cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);

  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, true,
                               federations);
}

TEST_P(CredentialManagerImplTest, RequestCredentialWithoutFirstRun) {
  client_->set_first_run_seen(false);
  store_->AddLogin(form_);
  form_.match_type = PasswordForm::MatchType::kExact;

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, NotifyUserCouldBeAutoSignedInPtr(
                            Pointee(MatchesFormExceptStore(form_))));

  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, true,
                               federations);
}

TEST_P(CredentialManagerImplTest, RequestCredentialWithFirstRunAndSkip) {
  client_->set_first_run_seen(true);
  form_.skip_zero_click = true;
  store_->AddLogin(form_);
  form_.match_type = PasswordForm::MatchType::kExact;

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, NotifyUserCouldBeAutoSignedInPtr(
                            Pointee(MatchesFormExceptStore(form_))));

  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, true,
                               federations);
}

TEST_P(CredentialManagerImplTest, RequestCredentialWithTLSErrors) {
  // If we encounter TLS errors, we won't return credentials.
  EXPECT_CALL(*client_, IsFillingEnabled).WillRepeatedly(Return(false));

  store_->AddLogin(form_);
  std::vector<GURL> federations;

  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, true,
                               federations);
}

TEST_P(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWithZeroClickOnlyTwoPasswordStore) {
  store_->AddLogin(form_);
  store_->AddLogin(origin_path_form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr).Times(0);
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr).Times(0);

  // With two items in the password store, we shouldn't get credentials back.
  ExpectCredentialType(CredentialMediationRequirement::kSilent, true,
                       federations, CredentialType::CREDENTIAL_TYPE_EMPTY);
}

TEST_P(CredentialManagerImplTest,
       OnRequestCredentialWithZeroClickOnlyAndSkipZeroClickPasswordStore) {
  form_.skip_zero_click = true;
  store_->AddLogin(form_);
  store_->AddLogin(origin_path_form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr).Times(0);
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr).Times(0);

  // With two items in the password store, we shouldn't get credentials back,
  // even though only one item has |skip_zero_click| set |false|.
  ExpectCredentialType(CredentialMediationRequirement::kSilent, true,
                       federations, CredentialType::CREDENTIAL_TYPE_EMPTY);
}

TEST_P(CredentialManagerImplTest,
       OnRequestCredentialWithZeroClickOnlyCrossOriginPasswordStore) {
  store_->AddLogin(cross_origin_form_);

  form_.skip_zero_click = true;
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr).Times(0);
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr).Times(0);

  // We only have cross-origin zero-click credentials; they should not be
  // returned.
  ExpectCredentialType(CredentialMediationRequirement::kSilent, true,
                       federations, CredentialType::CREDENTIAL_TYPE_EMPTY);
}

TEST_P(CredentialManagerImplTest,
       CredentialManagerOnRequestCredentialWhileRequestPending) {
  client_->set_zero_click_enabled(false);
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr).Times(0);
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr).Times(0);

  // 1st request.
  bool called_1 = false;
  CredentialManagerError error_1;
  std::optional<CredentialInfo> credential_1;
  CallGet(CredentialMediationRequirement::kOptional, true, federations,
          base::BindOnce(&GetCredentialCallback, &called_1, &error_1,
                         &credential_1));
  // 2nd request.
  bool called_2 = false;
  CredentialManagerError error_2;
  std::optional<CredentialInfo> credential_2;
  CallGet(CredentialMediationRequirement::kOptional, true, federations,
          base::BindOnce(&GetCredentialCallback, &called_2, &error_2,
                         &credential_2));

  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr);
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr).Times(0);

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

TEST_P(CredentialManagerImplTest, ResetSkipZeroClickInProfileStoreAfterPrompt) {
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
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr);
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr).Times(0);

  bool called = false;
  CredentialManagerError error;
  std::optional<CredentialInfo> credential;
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

TEST_P(CredentialManagerImplTest, ResetSkipZeroClickInAccountStoreAfterPrompt) {
  // This test is relevant only for account store users.
  if (!GetParam()) {
    return;
  }
  DCHECK(account_store_);
  // This is simplified version of the test above that tests against the account
  // store.
  // Turn on the global zero-click flag, and add the credential for
  // |kTestWebOrigin| and set to skip zero-click.
  client_->set_zero_click_enabled(true);
  form_.skip_zero_click = true;
  account_store_->AddLogin(form_);

  // Trigger a request which should return the credential found in |form_|, and
  // wait for it to process.
  // Check that the form in the database has been updated. `OnRequestCredential`
  // generates a call to prompt the user to choose a credential.
  // MockPasswordManagerClient mocks a user choice, and when users choose a
  // credential (and have the global zero-click flag enabled), we make sure that
  // they'll be logged in again next time.
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr);
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr).Times(0);

  bool called = false;
  CredentialManagerError error;
  std::optional<CredentialInfo> credential;
  CallGet(CredentialMediationRequirement::kOptional, true, /*federations=*/{},
          base::BindOnce(&GetCredentialCallback, &called, &error, &credential));

  RunAllPendingTasks();

  TestPasswordStore::PasswordMap passwords = account_store_->stored_passwords();
  ASSERT_EQ(1U, passwords.size());
  ASSERT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);
}

TEST_P(CredentialManagerImplTest,
       ResetSkipZeroClickInAccountStoreAfterPromptIfExistsInBothStores) {
  // This test is relevant only for account store users.
  if (!GetParam()) {
    return;
  }
  DCHECK(account_store_);
  // This is simplified version of the test above that tests against both the
  // profile the account stores. When the same credential is stored in both
  // stores, we favor the one in the account.

  // Turn on the global zero-click flag, and add the credential for
  // |kTestWebOrigin| , both set to skip zero-click on both stores.
  client_->set_zero_click_enabled(true);
  form_.skip_zero_click = true;
  account_store_->AddLogin(form_);
  store_->AddLogin(form_);

  // Trigger a request which should return the credential found in |form_|, and
  // wait for it to process.
  // Check that the form in the database has been updated. `OnRequestCredential`
  // generates a call to prompt the user to choose a credential.
  // MockPasswordManagerClient mocks a user choice, and when users choose a
  // credential (and have the global zero-click flag enabled), we make sure that
  // they'll be logged in again next time.
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr);
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr).Times(0);

  bool called = false;
  CredentialManagerError error;
  std::optional<CredentialInfo> credential;
  CallGet(CredentialMediationRequirement::kOptional, true, /*federations=*/{},
          base::BindOnce(&GetCredentialCallback, &called, &error, &credential));

  RunAllPendingTasks();

  // Only the one in the account store is affected.
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  ASSERT_EQ(1U, passwords.size());
  ASSERT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_TRUE(passwords[form_.signon_realm][0].skip_zero_click);

  passwords = account_store_->stored_passwords();
  ASSERT_EQ(1U, passwords.size());
  ASSERT_EQ(1U, passwords[form_.signon_realm].size());
  EXPECT_FALSE(passwords[form_.signon_realm][0].skip_zero_click);
}

TEST_P(CredentialManagerImplTest, IncognitoZeroClickRequestCredential) {
  EXPECT_CALL(*client_, IsOffTheRecord()).WillRepeatedly(Return(true));
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr).Times(0);
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr).Times(0);

  ExpectCredentialType(CredentialMediationRequirement::kSilent, true,
                       federations, CredentialType::CREDENTIAL_TYPE_EMPTY);
}

TEST_P(CredentialManagerImplTest, ZeroClickWithAffiliatedFormInPasswordStore) {
  // Insert the affiliated form into the store, and mock out the association
  // with the current origin. As it's the only form matching the origin, it
  // ought to be returned automagically.
  store_->AddLogin(affiliated_form1_);

  std::vector<GURL> federations;
  std::vector<std::string> affiliated_realms = {kTestAndroidRealm1};
  mock_match_helper_->ExpectCallToGetAffiliatedAndGrouped(
      cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);

  ExpectZeroClickSignInSuccess(CredentialMediationRequirement::kSilent, true,
                               federations,
                               CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_P(CredentialManagerImplTest,
       ZeroClickWithTwoAffiliatedFormsInPasswordStore) {
  // Insert two affiliated forms into the store, and mock out the association
  // with the current origin. Multiple forms === no zero-click sign in.
  store_->AddLogin(affiliated_form1_);
  store_->AddLogin(affiliated_form2_);

  std::vector<GURL> federations;
  std::vector<std::string> affiliated_realms;
  affiliated_realms.push_back(kTestAndroidRealm1);
  affiliated_realms.push_back(kTestAndroidRealm2);
  mock_match_helper_->ExpectCallToGetAffiliatedAndGrouped(
      cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);

  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, true,
                               federations);
}

TEST_P(CredentialManagerImplTest,
       ZeroClickWithUnaffiliatedFormsInPasswordStore) {
  // Insert the affiliated form into the store, but don't mock out the
  // association with the current origin. No association === no zero-click sign
  // in.
  store_->AddLogin(affiliated_form1_);

  std::vector<std::string> affiliated_realms;
  PasswordFormDigest digest = cm_service_impl_->GetSynthesizedFormForOrigin();
  // First expect affiliations for the HTTPS domain.
  mock_match_helper_->ExpectCallToGetAffiliatedAndGrouped(digest,
                                                          affiliated_realms);

  digest.url = HttpURLFromHttps(digest.url);
  digest.signon_realm = digest.url.spec();
  // The second call happens for HTTP as the migration is triggered.
  mock_match_helper_->ExpectCallToGetAffiliatedAndGrouped(digest,
                                                          affiliated_realms);

  std::vector<GURL> federations;
  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, true,
                               federations);
}

TEST_P(CredentialManagerImplTest,
       ZeroClickWithFormAndUnaffiliatedFormsInPasswordStore) {
  // Insert the affiliated form into the store, along with a real form for the
  // origin, and don't mock out the association with the current origin. No
  // association + existing form === zero-click sign in.
  store_->AddLogin(form_);
  store_->AddLogin(affiliated_form1_);

  std::vector<GURL> federations;
  std::vector<std::string> affiliated_realms;
  mock_match_helper_->ExpectCallToGetAffiliatedAndGrouped(
      cm_service_impl_->GetSynthesizedFormForOrigin(), affiliated_realms);

  ExpectZeroClickSignInSuccess(CredentialMediationRequirement::kSilent, true,
                               federations,
                               CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_P(CredentialManagerImplTest, ZeroClickWithPSLCredential) {
  subdomain_form_.skip_zero_click = false;
  store_->AddLogin(subdomain_form_);

  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, true,
                               std::vector<GURL>());
}

TEST_P(CredentialManagerImplTest, ZeroClickWithPSLAndNormalCredentials) {
  form_.password_value.clear();
  form_.federation_origin = url::SchemeHostPort(GURL("https://google.com/"));
  form_.signon_realm = "federation://" + form_.url.host() + "/google.com";
  form_.skip_zero_click = false;
  store_->AddLogin(form_);
  store_->AddLogin(subdomain_form_);

  std::vector<GURL> federations = {GURL("https://google.com/")};
  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kSilent, true,
                               federations);
}

TEST_P(CredentialManagerImplTest, ZeroClickAfterMigratingHttpCredential) {
  // There is an http credential saved. It should be migrated and used for auto
  // sign-in.
  form_.url = HttpURLFromHttps(form_.url);
  form_.signon_realm = form_.url.DeprecatedGetOriginAsURL().spec();
  // That is the default value for old credentials.
  form_.skip_zero_click = true;
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  ExpectZeroClickSignInSuccess(CredentialMediationRequirement::kSilent, true,
                               federations,
                               CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_P(CredentialManagerImplTest, ZeroClickOnLocalhost) {
  // HTTP scheme is valid for localhost. Nothing should crash.
  client_->set_last_committed_url(GURL("http://127.0.0.1:8000/"));

  std::vector<GURL> federations;
  ExpectZeroClickSignInFailure(CredentialMediationRequirement::kOptional, true,
                               federations);
}

TEST_P(CredentialManagerImplTest, MediationRequiredPreventsAutoSignIn) {
  form_.skip_zero_click = false;
  store_->AddLogin(form_);

  std::vector<GURL> federations;
  bool called = false;
  CredentialManagerError error;
  std::optional<CredentialInfo> credential;

  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr);
  EXPECT_CALL(*client_, NotifyUserAutoSigninPtr).Times(0);
  CallGet(CredentialMediationRequirement::kRequired, true, federations,
          base::BindOnce(&GetCredentialCallback, &called, &error, &credential));

  RunAllPendingTasks();

  EXPECT_TRUE(called);
  EXPECT_EQ(CredentialManagerError::SUCCESS, error);
  EXPECT_EQ(CredentialType::CREDENTIAL_TYPE_PASSWORD, credential->type);
}

TEST_P(CredentialManagerImplTest, GetSynthesizedFormForOrigin) {
  PasswordFormDigest synthesized =
      cm_service_impl_->GetSynthesizedFormForOrigin();
  EXPECT_EQ(kTestWebOrigin, synthesized.url.spec());
  EXPECT_EQ(kTestWebOrigin, synthesized.signon_realm);
  EXPECT_EQ(PasswordForm::Scheme::kHtml, synthesized.scheme);
}

TEST_P(CredentialManagerImplTest, BlockedPasswordCredential) {
  std::unique_ptr<PasswordFormManagerForUI> pending_manager;
  EXPECT_CALL(*client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&pending_manager, true));

  auto info = PasswordFormToCredentialInfo(form_);
  bool called = false;
  CallStore(info, base::BindOnce(&RespondCallback, &called));
  // Allow the PasswordFormManager to talk to the password store
  RunAllPendingTasks();

  ASSERT_TRUE(pending_manager);
  pending_manager->Blocklist();
  // Allow the PasswordFormManager to talk to the password store.
  RunAllPendingTasks();

  // Verify that the site is blocked.
  PasswordForm blocked_form;
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  blocked_form.blocked_by_user = true;
  blocked_form.url = form_.url;
  blocked_form.signon_realm = form_.signon_realm;
  blocked_form.date_created = passwords[form_.signon_realm][0].date_created;
  EXPECT_THAT(passwords[form_.signon_realm],
              ElementsAre(MatchesFormExceptStore(blocked_form)));
}

TEST_P(CredentialManagerImplTest, BlockedFederatedCredential) {
  form_.federation_origin = url::SchemeHostPort(GURL("https://example.com/"));
  form_.password_value = std::u16string();
  form_.signon_realm = "federation://example.com/example.com";

  std::unique_ptr<PasswordFormManagerForUI> pending_manager;
  EXPECT_CALL(*client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&pending_manager, true));
  auto info = PasswordFormToCredentialInfo(form_);
  bool called = false;
  CallStore(info, base::BindOnce(&RespondCallback, &called));
  // Allow the PasswordFormManager to talk to the password store
  RunAllPendingTasks();

  ASSERT_TRUE(pending_manager);
  pending_manager->Blocklist();
  // Allow the PasswordFormManager to talk to the password store.
  RunAllPendingTasks();

  // Verify that the site is blocked.
  TestPasswordStore::PasswordMap passwords = store_->stored_passwords();
  ASSERT_TRUE(passwords.count(form_.url.spec()));
  PasswordForm blocked_form;
  blocked_form.blocked_by_user = true;
  blocked_form.url = form_.url;
  blocked_form.signon_realm = blocked_form.url.spec();
  blocked_form.date_created =
      passwords[blocked_form.signon_realm][0].date_created;
  EXPECT_THAT(passwords[blocked_form.signon_realm],
              ElementsAre(MatchesFormExceptStore(blocked_form)));
}

TEST_P(CredentialManagerImplTest, RespecBlockedPasswordCredential) {
  PasswordForm blocked_form;
  blocked_form.blocked_by_user = true;
  blocked_form.url = form_.url;
  blocked_form.signon_realm = blocked_form.url.spec();
  store_->AddLogin(blocked_form);

  auto info = PasswordFormToCredentialInfo(form_);
  bool called = false;
  std::unique_ptr<PasswordFormManagerForUI> pending_manager;
  EXPECT_CALL(*client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&pending_manager, true));
  CallStore(info, base::BindOnce(&RespondCallback, &called));
  // Allow the PasswordFormManager to talk to the password store
  RunAllPendingTasks();

  ASSERT_TRUE(pending_manager);
  EXPECT_TRUE(pending_manager->IsBlocklisted());
}

TEST_P(CredentialManagerImplTest, RespectBlockedFederatedCredential) {
  PasswordForm blocked_form;
  blocked_form.blocked_by_user = true;
  blocked_form.url = form_.url;
  blocked_form.signon_realm = blocked_form.url.spec();
  store_->AddLogin(blocked_form);

  form_.federation_origin = url::SchemeHostPort(GURL("https://example.com/"));
  form_.password_value = std::u16string();
  form_.signon_realm = "federation://example.com/example.com";
  auto info = PasswordFormToCredentialInfo(form_);
  bool called = false;
  std::unique_ptr<PasswordFormManagerForUI> pending_manager;
  EXPECT_CALL(*client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&pending_manager, true));
  CallStore(info, base::BindOnce(&RespondCallback, &called));
  // Allow the PasswordFormManager to talk to the password store
  RunAllPendingTasks();

  ASSERT_TRUE(pending_manager);
  EXPECT_TRUE(pending_manager->IsBlocklisted());
}

TEST_P(CredentialManagerImplTest,
       ManagePasswordsUICredentialsUpdatedUnconditionallyInSilentMediation) {
  PasswordForm federated = origin_path_form_;
  federated.federation_origin =
      url::SchemeHostPort(GURL("https://google.com/"));
  federated.signon_realm =
      "federation://" + federated.url.host() + "/google.com";
  store_->AddLogin(federated);

  form_.username_value = u"username_value";
  store_->AddLogin(form_);

  form_.match_type = PasswordForm::MatchType::kExact;
  federated.match_type = PasswordForm::MatchType::kExact;

  EXPECT_CALL(*client_, PasswordWasAutofilled(
                            ElementsAre(MatchesFormExceptStore(form_)), _,
                            ElementsAre(MatchesFormExceptStore(federated)), _));

  bool called = false;
  CredentialManagerError error;
  std::optional<CredentialInfo> credential;
  std::vector<GURL> federations;
  federations.emplace_back("https://google.com/");

  CallGet(CredentialMediationRequirement::kSilent, true, federations,
          base::BindOnce(&GetCredentialCallback, &called, &error, &credential));

  RunAllPendingTasks();
}

// Check that following a call to store() a federated credential is not checked
// for leaks.
TEST_P(CredentialManagerImplTest,
       StoreFederatedCredentialDoesNotStartLeakDetection) {
  auto mock_factory =
      std::make_unique<testing::StrictMock<MockLeakDetectionCheckFactory>>();
  EXPECT_CALL(*mock_factory, TryCreateLeakCheck).Times(0);
  cm_service_impl()->set_leak_factory(std::move(mock_factory));

  form_.federation_origin = url::SchemeHostPort(GURL("https://example.com/"));
  form_.password_value = std::u16string();
  form_.signon_realm = "federation://example.com/example.com";
  CallStore(PasswordFormToCredentialInfo(form_), base::DoNothing());

  RunAllPendingTasks();
}

// Check that following a call to store() a password credential is checked for
// leaks.
TEST_P(CredentialManagerImplTest, StorePasswordCredentialStartsLeakDetection) {
  auto mock_factory =
      std::make_unique<testing::StrictMock<MockLeakDetectionCheckFactory>>();
  auto* weak_factory = mock_factory.get();
  cm_service_impl()->set_leak_factory(std::move(mock_factory));

  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  EXPECT_CALL(*check_instance,
              Start(LeakDetectionInitiator::kSignInCheck, form_.url,
                    form_.username_value, form_.password_value));
  EXPECT_CALL(*weak_factory, TryCreateLeakCheck)
      .WillOnce(Return(testing::ByMove(std::move(check_instance))));
  CallStore(PasswordFormToCredentialInfo(form_), base::DoNothing());

  RunAllPendingTasks();
}

INSTANTIATE_TEST_SUITE_P(All, CredentialManagerImplTest, testing::Bool());

}  // namespace password_manager
