// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync_credentials_filter.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/mock_password_store_interface.h"
#include "components/password_manager/core/browser/mock_webauthn_credentials_delegate.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/browser/sync_username_test_base.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

const char kFilledAndLoginActionName[] =
    "PasswordManager_SyncCredentialFilledAndLoginSuccessfull";
const char kEnterpriseURL[] = "https://enterprise.test/";

class FakePasswordManagerClient : public StubPasswordManagerClient {
 public:
  explicit FakePasswordManagerClient(signin::IdentityManager* identity_manager)
      : identity_manager_(identity_manager) {
    if (!base::FeatureList::IsEnabled(
            features::kPasswordReuseDetectionEnabled)) {
      return;
    }
    ON_CALL(webauthn_credentials_delegate_, GetWebAuthnSuggestions)
        .WillByDefault(testing::ReturnRef(webauthn_suggestions_));

    // Initializes and configures prefs.
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterStringPref(
        prefs::kPasswordProtectionChangePasswordURL, "");
    prefs_->registry()->RegisterListPref(prefs::kPasswordProtectionLoginURLs);
    prefs_->SetString(prefs::kPasswordProtectionChangePasswordURL,
                      kEnterpriseURL);
  }

  FakePasswordManagerClient(const FakePasswordManagerClient&) = delete;
  FakePasswordManagerClient& operator=(const FakePasswordManagerClient&) =
      delete;

  ~FakePasswordManagerClient() override {
    password_store_->ShutdownOnUIThread();
  }

  // PasswordManagerClient:
  url::Origin GetLastCommittedOrigin() const override {
    return last_committed_origin_;
  }
  MockPasswordStoreInterface* GetProfilePasswordStore() const override {
    return password_store_.get();
  }
  signin::IdentityManager* GetIdentityManager() override {
    return identity_manager_;
  }
  MockWebAuthnCredentialsDelegate* GetWebAuthnCredentialsDelegateForDriver(
      password_manager::PasswordManagerDriver*) override {
    return &webauthn_credentials_delegate_;
  }

  void set_last_committed_entry_url(base::StringPiece url_spec) {
    last_committed_origin_ = url::Origin::Create(GURL(url_spec));
  }

  PrefService* GetPrefs() const override { return prefs_.get(); }

  bool IsIncognito() const override { return is_incognito_; }

  void SetIsIncognito(bool is_incognito) { is_incognito_ = is_incognito; }

 private:
  url::Origin last_committed_origin_;
  scoped_refptr<testing::NiceMock<MockPasswordStoreInterface>> password_store_ =
      new testing::NiceMock<MockPasswordStoreInterface>;
  MockWebAuthnCredentialsDelegate webauthn_credentials_delegate_;
  absl::optional<std::vector<autofill::Suggestion>> webauthn_suggestions_;
  bool is_incognito_ = false;
  raw_ptr<signin::IdentityManager> identity_manager_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
};

}  // namespace

// The bool param specifies whether features::kEnablePasswordsAccountStorage is
// enabled.
class CredentialsFilterTest : public SyncUsernameTestBase,
                              public testing::WithParamInterface<bool> {
 public:
  // Flag for creating a PasswordFormManager, deciding its IsNewLogin() value.
  enum class LoginState { NEW, EXISTING };

  CredentialsFilterTest() : pending_(SimpleGaiaForm("user@gmail.com")) {
    if (GetParam()) {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kPasswordReuseDetectionEnabled,
                                features::kEnablePasswordsAccountStorage},
          /*disabled_features=*/{});
    } else {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kPasswordReuseDetectionEnabled},
          /*disabled_features=*/{features::kEnablePasswordsAccountStorage});
    }

    client_ = std::make_unique<FakePasswordManagerClient>(identity_manager());
    form_manager_ = std::make_unique<PasswordFormManager>(
        client_.get(), driver_.AsWeakPtr(), pending_.form_data, &fetcher_,
        std::make_unique<PasswordSaveManagerImpl>(
            /*profile_form_saver=*/std::make_unique<StubFormSaver>(),
            /*account_form_saver=*/nullptr),
        nullptr /* metrics_recorder */);
    filter_ = std::make_unique<SyncCredentialsFilter>(
        client_.get(), base::BindRepeating(&SyncUsernameTestBase::sync_service,
                                           base::Unretained(this)));

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

    form_manager_->ProvisionallySave(pending_.form_data, &driver_, nullptr);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<FakePasswordManagerClient> client_;
  StubPasswordManagerDriver driver_;
  PasswordForm pending_;
  FakeFormFetcher fetcher_;
  std::unique_ptr<PasswordFormManager> form_manager_;

  std::unique_ptr<SyncCredentialsFilter> filter_;
};

TEST_P(CredentialsFilterTest, ReportFormLoginSuccess_ExistingSyncCredentials) {
  FakeSigninAs("user@gmail.com");
  SetSyncingPasswords(true);

  base::UserActionTester tester;
  SavePending(LoginState::EXISTING);
  filter_->ReportFormLoginSuccess(*form_manager_);
  EXPECT_EQ(1, tester.GetActionCount(kFilledAndLoginActionName));
}

TEST_P(CredentialsFilterTest, ReportFormLoginSuccess_NewSyncCredentials) {
  FakeSigninAs("user@gmail.com");
  SetSyncingPasswords(true);

  base::UserActionTester tester;
  SavePending(LoginState::NEW);
  filter_->ReportFormLoginSuccess(*form_manager_);
  EXPECT_EQ(0, tester.GetActionCount(kFilledAndLoginActionName));
}

TEST_P(CredentialsFilterTest, ReportFormLoginSuccess_GAIANotSyncCredentials) {
  const char kOtherUsername[] = "other_user@gmail.com";
  const char16_t kOtherUsername16[] = u"other_user@gmail.com";
  FakeSigninAs(kOtherUsername);
  ASSERT_NE(pending_.username_value, kOtherUsername16);
  SetSyncingPasswords(true);

  base::UserActionTester tester;
  SavePending(LoginState::EXISTING);
  filter_->ReportFormLoginSuccess(*form_manager_);
  EXPECT_EQ(0, tester.GetActionCount(kFilledAndLoginActionName));
}

TEST_P(CredentialsFilterTest, ReportFormLoginSuccess_NotGAIACredentials) {
  pending_ = SimpleNonGaiaForm("user@gmail.com");
  FakeSigninAs("user@gmail.com");
  SetSyncingPasswords(true);

  base::UserActionTester tester;
  SavePending(LoginState::EXISTING);
  filter_->ReportFormLoginSuccess(*form_manager_);
  EXPECT_EQ(0, tester.GetActionCount(kFilledAndLoginActionName));
}

TEST_P(CredentialsFilterTest, ReportFormLoginSuccess_NotSyncing) {
  FakeSigninAs("user@gmail.com");
  SetSyncingPasswords(false);

  base::UserActionTester tester;
  SavePending(LoginState::EXISTING);
  filter_->ReportFormLoginSuccess(*form_manager_);
  EXPECT_EQ(0, tester.GetActionCount(kFilledAndLoginActionName));
}

TEST_P(CredentialsFilterTest, ShouldSave_NotSignedIn) {
  PasswordForm form = SimpleGaiaForm("user@example.org");

  ASSERT_TRUE(identity_manager()
                  ->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
                  .IsEmpty());
  SetSyncingPasswords(false);
  // If kEnablePasswordsAccountStorage is enabled, then Chrome shouldn't offer
  // to save the password for the primary account. If there is no primary
  // account yet, then the just-signed-in account will *become* the primary
  // account immediately, so it shouldn't be saved either.
  if (base::FeatureList::IsEnabled(features::kEnablePasswordsAccountStorage))
    EXPECT_FALSE(filter_->ShouldSave(form));
  else
    EXPECT_TRUE(filter_->ShouldSave(form));
}

TEST_P(CredentialsFilterTest, ShouldSave_NotSyncCredential) {
  PasswordForm form = SimpleGaiaForm("user@example.org");

  FakeSigninAs("different_user@example.org");
  SetSyncingPasswords(true);
  EXPECT_TRUE(filter_->ShouldSave(form));
}

TEST_P(CredentialsFilterTest, ShouldSave_SyncCredential) {
  PasswordForm form = SimpleGaiaForm("user@example.org");

  FakeSigninAs("user@example.org");
  SetSyncingPasswords(true);
  EXPECT_FALSE(filter_->ShouldSave(form));
}

TEST_P(CredentialsFilterTest, ShouldSave_SignIn_Form) {
  PasswordForm form = SimpleGaiaForm("user@example.org");
  form.form_data.is_gaia_with_skip_save_password_form = true;

  SetSyncingPasswords(false);
  EXPECT_FALSE(filter_->ShouldSave(form));
}

TEST_P(CredentialsFilterTest, ShouldSave_SyncCredential_NotSyncingPasswords) {
  PasswordForm form = SimpleGaiaForm("user@example.org");

  FakeSigninAs("user@example.org");
  SetSyncingPasswords(false);
  // If kEnablePasswordsAccountStorage is enabled, then Chrome shouldn't offer
  // to save the password for the primary account - doesn't matter if passwords
  // are being synced or not.
  if (base::FeatureList::IsEnabled(features::kEnablePasswordsAccountStorage))
    EXPECT_FALSE(filter_->ShouldSave(form));
  else
    EXPECT_TRUE(filter_->ShouldSave(form));
}

TEST_P(CredentialsFilterTest, ShouldSaveGaiaPasswordHash) {
  PasswordForm gaia_form = SimpleGaiaForm("user@gmail.org");
  EXPECT_TRUE(filter_->ShouldSaveGaiaPasswordHash(gaia_form));

  PasswordForm other_form = SimpleNonGaiaForm("user@example.org");
  EXPECT_FALSE(filter_->ShouldSaveGaiaPasswordHash(other_form));
}

TEST_P(CredentialsFilterTest, ShouldNotSaveGaiaPasswordHashIncognito) {
  client_->SetIsIncognito(true);
  PasswordForm gaia_form = SimpleGaiaForm("user@gmail.org");
  EXPECT_FALSE(filter_->ShouldSaveGaiaPasswordHash(gaia_form));

  PasswordForm other_form = SimpleNonGaiaForm("user@example.org");
  EXPECT_FALSE(filter_->ShouldSaveGaiaPasswordHash(other_form));
}

TEST_P(CredentialsFilterTest, ShouldSaveEnterprisePasswordHash) {
  PasswordForm gaia_form = SimpleGaiaForm("user@gmail.org");
  EXPECT_FALSE(filter_->ShouldSaveEnterprisePasswordHash(gaia_form));

  PasswordForm other_form = SimpleNonGaiaForm("user@example.org");
  EXPECT_FALSE(filter_->ShouldSaveEnterprisePasswordHash(other_form));

  PasswordForm enterprise_form =
      SimpleNonGaiaForm("user@enterprise.test", kEnterpriseURL);
  EXPECT_TRUE(filter_->ShouldSaveEnterprisePasswordHash(enterprise_form));
}

TEST_P(CredentialsFilterTest, ShouldNotSaveEnterprisePasswordHashIncognito) {
  client_->SetIsIncognito(true);
  PasswordForm gaia_form = SimpleGaiaForm("user@gmail.org");
  EXPECT_FALSE(filter_->ShouldSaveEnterprisePasswordHash(gaia_form));

  PasswordForm other_form = SimpleNonGaiaForm("user@example.org");
  EXPECT_FALSE(filter_->ShouldSaveEnterprisePasswordHash(other_form));

  PasswordForm enterprise_form =
      SimpleNonGaiaForm("user@enterprise.test", kEnterpriseURL);
  EXPECT_FALSE(filter_->ShouldSaveEnterprisePasswordHash(enterprise_form));
}

TEST_P(CredentialsFilterTest, IsSyncAccountEmail) {
  FakeSigninAs("user@gmail.com");
  EXPECT_FALSE(filter_->IsSyncAccountEmail("user"));
  EXPECT_FALSE(filter_->IsSyncAccountEmail("user2@gmail.com"));
  EXPECT_FALSE(filter_->IsSyncAccountEmail("user2@example.com"));
  EXPECT_TRUE(filter_->IsSyncAccountEmail("user@gmail.com"));
  EXPECT_TRUE(filter_->IsSyncAccountEmail("us.er@gmail.com"));
  EXPECT_TRUE(filter_->IsSyncAccountEmail("user@googlemail.com"));
}

TEST_P(CredentialsFilterTest, IsSyncAccountEmailIncognito) {
  client_->SetIsIncognito(true);
  FakeSigninAs("user@gmail.com");
  EXPECT_FALSE(filter_->IsSyncAccountEmail("user"));
  EXPECT_FALSE(filter_->IsSyncAccountEmail("user2@gmail.com"));
  EXPECT_FALSE(filter_->IsSyncAccountEmail("user2@example.com"));
  EXPECT_TRUE(filter_->IsSyncAccountEmail("user@gmail.com"));
  EXPECT_TRUE(filter_->IsSyncAccountEmail("us.er@gmail.com"));
  EXPECT_TRUE(filter_->IsSyncAccountEmail("user@googlemail.com"));
}

INSTANTIATE_TEST_SUITE_P(, CredentialsFilterTest, ::testing::Bool());

}  // namespace password_manager
