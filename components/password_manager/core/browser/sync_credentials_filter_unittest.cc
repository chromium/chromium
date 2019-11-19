// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync_credentials_filter.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/user_action_tester.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/browser/sync_username_test_base.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
#include "components/safe_browsing/common/safe_browsing_prefs.h"  // nogncheck
#endif  // SYNC_PASSWORD_REUSE_DETECTION_ENABLED

using autofill::PasswordForm;

namespace password_manager {

namespace {

const char kFilledAndLoginActionName[] =
    "PasswordManager_SyncCredentialFilledAndLoginSuccessfull";

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
const char kEnterpriseURL[] = "https://enterprise.test/";
#endif  // SYNC_PASSWORD_REUSE_DETECTION_ENABLED

class FakePasswordManagerClient : public StubPasswordManagerClient {
 public:
  explicit FakePasswordManagerClient(signin::IdentityManager* identity_manager)
      : identity_manager_(identity_manager) {
#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
    // Initializes and configures prefs.
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterStringPref(
        prefs::kPasswordProtectionChangePasswordURL, "");
    prefs_->registry()->RegisterListPref(prefs::kPasswordProtectionLoginURLs);
    prefs_->SetString(prefs::kPasswordProtectionChangePasswordURL,
                      kEnterpriseURL);
#endif  // SYNC_PASSWORD_REUSE_DETECTION_ENABLED
  }

  ~FakePasswordManagerClient() override {
    password_store_->ShutdownOnUIThread();
  }

  // PasswordManagerClient:
  const GURL& GetLastCommittedEntryURL() const override {
    return last_committed_entry_url_;
  }
  MockPasswordStore* GetProfilePasswordStore() const override {
    return password_store_.get();
  }
  signin::IdentityManager* GetIdentityManager() override {
    return identity_manager_;
  }

  void set_last_committed_entry_url(const char* url_spec) {
    last_committed_entry_url_ = GURL(url_spec);
  }

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  PrefService* GetPrefs() const override { return prefs_.get(); }
#endif

  bool IsIncognito() const override { return is_incognito_; }

  void SetIsIncognito(bool is_incognito) { is_incognito_ = is_incognito; }

 private:
  GURL last_committed_entry_url_;
  scoped_refptr<testing::NiceMock<MockPasswordStore>> password_store_ =
      new testing::NiceMock<MockPasswordStore>;
  bool is_incognito_ = false;
  signin::IdentityManager* identity_manager_;
#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
#endif  // SYNC_PASSWORD_REUSE_DETECTION_ENABLED

  DISALLOW_COPY_AND_ASSIGN(FakePasswordManagerClient);
};

}  // namespace

class CredentialsFilterTest : public SyncUsernameTestBase {
 public:
  // Flag for creating a PasswordFormManager, deciding its IsNewLogin() value.
  enum class LoginState { NEW, EXISTING };

  CredentialsFilterTest()
      : client_(identity_manager()),
        pending_(SimpleGaiaForm("user@gmail.com")),
        form_manager_(&client_,
                      driver_.AsWeakPtr(),
                      pending_.form_data,
                      &fetcher_,
                      std::make_unique<StubFormSaver>(),
                      nullptr /* metrics_recorder */),
        filter_(&client_,
                base::BindRepeating(&SyncUsernameTestBase::sync_service,
                                    base::Unretained(this))) {
    fetcher_.Fetch();
  }

  // Makes |form_manager_| provisionally save |pending_|. Depending on
  // |login_state| being NEW or EXISTING, prepares |form_manager_| in a state in
  // which |pending_| looks like a new or existing credential, respectively.
  void SavePending(LoginState login_state) {
    std::vector<const PasswordForm*> matches;
    if (login_state == LoginState::EXISTING) {
      matches.push_back(&pending_);
    }
    fetcher_.SetNonFederated(matches);
    fetcher_.NotifyFetchCompleted();

    form_manager_.ProvisionallySave(pending_.form_data, &driver_, nullptr);
  }

 protected:
  FakePasswordManagerClient client_;
  StubPasswordManagerDriver driver_;
  PasswordForm pending_;
  FakeFormFetcher fetcher_;
  PasswordFormManager form_manager_;

  SyncCredentialsFilter filter_;
};

TEST_F(CredentialsFilterTest, ReportFormLoginSuccess_ExistingSyncCredentials) {
  FakeSigninAs("user@gmail.com");
  SetSyncingPasswords(true);

  base::UserActionTester tester;
  SavePending(LoginState::EXISTING);
  filter_.ReportFormLoginSuccess(form_manager_);
  EXPECT_EQ(1, tester.GetActionCount(kFilledAndLoginActionName));
}

TEST_F(CredentialsFilterTest, ReportFormLoginSuccess_NewSyncCredentials) {
  FakeSigninAs("user@gmail.com");
  SetSyncingPasswords(true);

  base::UserActionTester tester;
  SavePending(LoginState::NEW);
  filter_.ReportFormLoginSuccess(form_manager_);
  EXPECT_EQ(0, tester.GetActionCount(kFilledAndLoginActionName));
}

TEST_F(CredentialsFilterTest, ReportFormLoginSuccess_GAIANotSyncCredentials) {
  const char kOtherUsername[] = "other_user@gmail.com";
  FakeSigninAs(kOtherUsername);
  ASSERT_NE(pending_.username_value, base::ASCIIToUTF16(kOtherUsername));
  SetSyncingPasswords(true);

  base::UserActionTester tester;
  SavePending(LoginState::EXISTING);
  filter_.ReportFormLoginSuccess(form_manager_);
  EXPECT_EQ(0, tester.GetActionCount(kFilledAndLoginActionName));
}

TEST_F(CredentialsFilterTest, ReportFormLoginSuccess_NotGAIACredentials) {
  pending_ = SimpleNonGaiaForm("user@gmail.com");
  FakeSigninAs("user@gmail.com");
  SetSyncingPasswords(true);

  base::UserActionTester tester;
  SavePending(LoginState::EXISTING);
  filter_.ReportFormLoginSuccess(form_manager_);
  EXPECT_EQ(0, tester.GetActionCount(kFilledAndLoginActionName));
}

TEST_F(CredentialsFilterTest, ReportFormLoginSuccess_NotSyncing) {
  FakeSigninAs("user@gmail.com");
  SetSyncingPasswords(false);

  base::UserActionTester tester;
  SavePending(LoginState::EXISTING);
  filter_.ReportFormLoginSuccess(form_manager_);
  EXPECT_EQ(0, tester.GetActionCount(kFilledAndLoginActionName));
}

TEST_F(CredentialsFilterTest, ShouldSave_NotSyncCredential) {
  PasswordForm form = SimpleGaiaForm("user@example.org");

  ASSERT_NE("user@example.org",
            identity_manager()->GetPrimaryAccountInfo().email);
  SetSyncingPasswords(true);
  EXPECT_TRUE(filter_.ShouldSave(form));
}

TEST_F(CredentialsFilterTest, ShouldSave_SyncCredential) {
  PasswordForm form = SimpleGaiaForm("user@example.org");

  FakeSigninAs("user@example.org");
  SetSyncingPasswords(true);
  EXPECT_FALSE(filter_.ShouldSave(form));
}

TEST_F(CredentialsFilterTest, ShouldSave_SignIn_Form) {
  PasswordForm form = SimpleGaiaForm("user@example.org");
  form.form_data.is_gaia_with_skip_save_password_form = true;

  SetSyncingPasswords(false);
  EXPECT_FALSE(filter_.ShouldSave(form));
}

TEST_F(CredentialsFilterTest, ShouldSave_SyncCredential_NotSyncingPasswords) {
  PasswordForm form = SimpleGaiaForm("user@example.org");

  FakeSigninAs("user@example.org");
  SetSyncingPasswords(false);
  EXPECT_TRUE(filter_.ShouldSave(form));
}

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
TEST_F(CredentialsFilterTest, ShouldSaveGaiaPasswordHash) {
  PasswordForm gaia_form = SimpleGaiaForm("user@gmail.org");
  EXPECT_TRUE(filter_.ShouldSaveGaiaPasswordHash(gaia_form));

  PasswordForm other_form = SimpleNonGaiaForm("user@example.org");
  EXPECT_FALSE(filter_.ShouldSaveGaiaPasswordHash(other_form));
}

TEST_F(CredentialsFilterTest, ShouldNotSaveGaiaPasswordHashIncognito) {
  client_.SetIsIncognito(true);
  PasswordForm gaia_form = SimpleGaiaForm("user@gmail.org");
  EXPECT_FALSE(filter_.ShouldSaveGaiaPasswordHash(gaia_form));

  PasswordForm other_form = SimpleNonGaiaForm("user@example.org");
  EXPECT_FALSE(filter_.ShouldSaveGaiaPasswordHash(other_form));
}

TEST_F(CredentialsFilterTest, ShouldSaveEnterprisePasswordHash) {
  PasswordForm gaia_form = SimpleGaiaForm("user@gmail.org");
  EXPECT_FALSE(filter_.ShouldSaveEnterprisePasswordHash(gaia_form));

  PasswordForm other_form = SimpleNonGaiaForm("user@example.org");
  EXPECT_FALSE(filter_.ShouldSaveEnterprisePasswordHash(other_form));

  PasswordForm enterprise_form =
      SimpleNonGaiaForm("user@enterprise.test", kEnterpriseURL);
  EXPECT_TRUE(filter_.ShouldSaveEnterprisePasswordHash(enterprise_form));
}

TEST_F(CredentialsFilterTest, ShouldNotSaveEnterprisePasswordHashIncognito) {
  client_.SetIsIncognito(true);
  PasswordForm gaia_form = SimpleGaiaForm("user@gmail.org");
  EXPECT_FALSE(filter_.ShouldSaveEnterprisePasswordHash(gaia_form));

  PasswordForm other_form = SimpleNonGaiaForm("user@example.org");
  EXPECT_FALSE(filter_.ShouldSaveEnterprisePasswordHash(other_form));

  PasswordForm enterprise_form =
      SimpleNonGaiaForm("user@enterprise.test", kEnterpriseURL);
  EXPECT_FALSE(filter_.ShouldSaveEnterprisePasswordHash(enterprise_form));
}

TEST_F(CredentialsFilterTest, IsSyncAccountEmail) {
  FakeSigninAs("user@gmail.com");
  EXPECT_FALSE(filter_.IsSyncAccountEmail("user"));
  EXPECT_FALSE(filter_.IsSyncAccountEmail("user2@gmail.com"));
  EXPECT_FALSE(filter_.IsSyncAccountEmail("user2@example.com"));
  EXPECT_TRUE(filter_.IsSyncAccountEmail("user@gmail.com"));
  EXPECT_TRUE(filter_.IsSyncAccountEmail("us.er@gmail.com"));
  EXPECT_TRUE(filter_.IsSyncAccountEmail("user@googlemail.com"));
}

TEST_F(CredentialsFilterTest, IsSyncAccountEmailIncognito) {
  client_.SetIsIncognito(true);
  FakeSigninAs("user@gmail.com");
  EXPECT_FALSE(filter_.IsSyncAccountEmail("user"));
  EXPECT_FALSE(filter_.IsSyncAccountEmail("user2@gmail.com"));
  EXPECT_FALSE(filter_.IsSyncAccountEmail("user2@example.com"));
  EXPECT_TRUE(filter_.IsSyncAccountEmail("user@gmail.com"));
  EXPECT_TRUE(filter_.IsSyncAccountEmail("us.er@gmail.com"));
  EXPECT_TRUE(filter_.IsSyncAccountEmail("user@googlemail.com"));
}
#endif  // SYNC_PASSWORD_REUSE_DETECTION_ENABLED

}  // namespace password_manager
