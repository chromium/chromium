// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync_credentials_filter.h"

#include <stddef.h>

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/mock_webauthn_credentials_delegate.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/browser/sync_username_test_base.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

const char kEnterpriseURL[] = "https://enterprise.test/";

PasswordForm SimpleGAIAChangePasswordForm() {
  PasswordForm form;
  form.url = GURL("https://myaccount.google.com/");
  form.signon_realm = "https://myaccount.google.com/";
  return form;
}

PasswordForm SimpleForm(const char* signon_realm, const char* username) {
  PasswordForm form;
  form.signon_realm = signon_realm;
  form.url = GURL(signon_realm);
  form.username_value = base::ASCIIToUTF16(username);
  return form;
}

class FakePasswordManagerClient : public StubPasswordManagerClient {
 public:
  FakePasswordManagerClient(signin::IdentityManager* identity_manager,
                            const syncer::SyncService* sync_service)
      : identity_manager_(identity_manager), sync_service_(sync_service) {
    if (!base::FeatureList::IsEnabled(
            features::kPasswordReuseDetectionEnabled)) {
      return;
    }
    ON_CALL(webauthn_credentials_delegate_, GetPasskeys)
        .WillByDefault(testing::ReturnRef(passkeys_));
    ON_CALL(webauthn_credentials_delegate_, IsSecurityKeyOrHybridFlowAvailable)
        .WillByDefault(testing::Return(true));

    // Initializes and configures prefs.
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterStringPref(
        ::prefs::kPasswordProtectionChangePasswordURL, "");
    prefs_->registry()->RegisterListPref(::prefs::kPasswordProtectionLoginURLs);
    prefs_->SetString(::prefs::kPasswordProtectionChangePasswordURL,
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
  const syncer::SyncService* GetSyncService() const override {
    return sync_service_;
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
  TestingPrefServiceSimple* GetPrefs() const override { return prefs_.get(); }
  bool IsOffTheRecord() const override { return is_incognito_; }

  void set_last_committed_entry_url(std::string_view url_spec) {
    last_committed_origin_ = url::Origin::Create(GURL(url_spec));
  }

  void SetIsOffTheRecord(bool is_incognito) { is_incognito_ = is_incognito; }

 private:
  url::Origin last_committed_origin_;
  scoped_refptr<testing::NiceMock<MockPasswordStoreInterface>> password_store_ =
      new testing::NiceMock<MockPasswordStoreInterface>;
  MockWebAuthnCredentialsDelegate webauthn_credentials_delegate_;
  std::optional<std::vector<PasskeyCredential>> passkeys_;
  bool is_incognito_ = false;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<const syncer::SyncService> sync_service_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
};

}  // namespace

class CredentialsFilterTest : public SyncUsernameTestBase {
 public:
  // Flag for creating a PasswordFormManager, deciding its IsNewLogin() value.
  enum class LoginState { NEW, EXISTING };

  CredentialsFilterTest() {
    client_ = std::make_unique<FakePasswordManagerClient>(identity_manager(),
                                                          sync_service());
    signin::IdentityManager::RegisterProfilePrefs(
        client_->GetPrefs()->registry());
    form_manager_ = std::make_unique<PasswordFormManager>(
        client_.get(), driver_.AsWeakPtr(), pending_.form_data, &fetcher_,
        std::make_unique<PasswordSaveManagerImpl>(
            /*profile_form_saver=*/std::make_unique<StubFormSaver>(),
            /*account_form_saver=*/nullptr),
        nullptr /* metrics_recorder */);
    filter_ = std::make_unique<SyncCredentialsFilter>(client_.get());

    fetcher_.Fetch();
  }

  // Makes |form_manager_| provisionally save |pending_|. Depending on
  // |login_state| being NEW or EXISTING, prepares |form_manager_| in a state in
  // which |pending_| looks like a new or existing credential, respectively.
  void SavePending(LoginState login_state) {
    std::vector<PasswordForm> matches;
    if (login_state == LoginState::EXISTING) {
      matches.push_back(pending_);
    }
    fetcher_.SetNonFederated(matches);
    fetcher_.SetBestMatches(matches);
    fetcher_.NotifyFetchCompleted();

    form_manager_->ProvisionallySave(
        pending_.form_data, &driver_,
        base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>(
            /*max_size=*/2));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<FakePasswordManagerClient> client_;
  StubPasswordManagerDriver driver_;
  PasswordForm pending_ = SimpleGaiaForm("user@gmail.com");
  FakeFormFetcher fetcher_;
  std::unique_ptr<PasswordFormManager> form_manager_;

  std::unique_ptr<SyncCredentialsFilter> filter_;
};

TEST_F(CredentialsFilterTest, ShouldSave_NotSignedIn) {
  const char kTestEmail[] = "user@example.org";

  ASSERT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(sync_service()->GetAccountInfo().IsEmpty());

  SetSyncingPasswords(false);

  // Non-Gaia forms should always offer saving.
  EXPECT_TRUE(filter_->ShouldSave(SimpleNonGaiaForm(kTestEmail)));
  EXPECT_TRUE(filter_->ShouldSave(SimpleNonGaiaForm("")));

  // See comments inside ShouldSave() for the justification.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_TRUE(filter_->ShouldSave(SimpleGaiaForm("")));
  EXPECT_TRUE(filter_->ShouldSave(SimpleGAIAChangePasswordForm()));
  EXPECT_TRUE(filter_->ShouldSave(SimpleGaiaForm(kTestEmail)));
#else   // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_FALSE(filter_->ShouldSave(SimpleGaiaForm("")));
  // A web password change probably wouldn't cause a browser sign-in in this
  // case since the original web sign-in didn't, but oh well.
  EXPECT_FALSE(filter_->ShouldSave(SimpleGAIAChangePasswordForm()));
  EXPECT_FALSE(filter_->ShouldSave(SimpleGaiaForm(kTestEmail)));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

// Effectively the same as ShouldSave_NotSignedIn.
TEST_F(CredentialsFilterTest, ShouldSave_SignedInWithSyncServiceNull) {
  const char kPrimaryAccountEmail[] = "sync_user@example.org";

  FakeSigninAs(kPrimaryAccountEmail, signin::ConsentLevel::kSync);
  SetSyncingPasswords(false);

  // Create a new filter that uses a null SyncService.
  filter_.reset();
  form_manager_.reset();
  client_ =
      std::make_unique<FakePasswordManagerClient>(identity_manager(),
                                                  /*sync_service=*/nullptr);
  signin::IdentityManager::RegisterProfilePrefs(
      client_->GetPrefs()->registry());
  filter_ = std::make_unique<SyncCredentialsFilter>(client_.get());

  // Non-Gaia forms should always offer saving.
  EXPECT_TRUE(filter_->ShouldSave(SimpleNonGaiaForm(kPrimaryAccountEmail)));
  EXPECT_TRUE(filter_->ShouldSave(SimpleNonGaiaForm("")));
  EXPECT_TRUE(filter_->ShouldSave(
      SimpleForm("https://subdomain.google.com/", kPrimaryAccountEmail)));
  EXPECT_TRUE(
      filter_->ShouldSave(SimpleForm("https://subdomain.google.com/", "")));

  // See comments inside ShouldSave() for the justification.
  const PasswordForm simple_gaia_form =
      SimpleGaiaForm("non_sync_user@example.org");
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_TRUE(filter_->ShouldSave(simple_gaia_form));
  EXPECT_TRUE(filter_->ShouldSave(SimpleGaiaForm("")));
  EXPECT_TRUE(filter_->ShouldSave(SimpleGAIAChangePasswordForm()));
  EXPECT_TRUE(filter_->ShouldSave(SimpleGaiaForm(kPrimaryAccountEmail)));
#else   // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_FALSE(filter_->ShouldSave(simple_gaia_form));
  EXPECT_FALSE(filter_->ShouldSave(SimpleGaiaForm("")));
  EXPECT_FALSE(filter_->ShouldSave(SimpleGAIAChangePasswordForm()));
  EXPECT_FALSE(filter_->ShouldSave(SimpleGaiaForm(kPrimaryAccountEmail)));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

TEST_F(CredentialsFilterTest, ShouldSave_SyncFeatureOn) {
  const char kPrimaryAccountEmail[] = "sync_user@example.org";

  FakeSigninAs(kPrimaryAccountEmail, signin::ConsentLevel::kSync);
  SetSyncingPasswords(true);

  // Non-Gaia forms should always offer saving.
  EXPECT_TRUE(filter_->ShouldSave(SimpleNonGaiaForm(kPrimaryAccountEmail)));
  EXPECT_TRUE(filter_->ShouldSave(SimpleNonGaiaForm("")));
  EXPECT_TRUE(filter_->ShouldSave(
      SimpleForm("https://subdomain.google.com/", kPrimaryAccountEmail)));
  EXPECT_TRUE(
      filter_->ShouldSave(SimpleForm("https://subdomain.google.com/", "")));

  // Gaia forms can offer saving only if the username doesn't match the primary
  // account.
  EXPECT_FALSE(filter_->ShouldSave(SimpleGaiaForm(kPrimaryAccountEmail)));
  EXPECT_TRUE(filter_->ShouldSave(SimpleGaiaForm("non_sync_user@example.org")));
  EXPECT_FALSE(filter_->ShouldSave(SimpleGaiaForm("")));
  EXPECT_FALSE(filter_->ShouldSave(SimpleGAIAChangePasswordForm()));
}

TEST_F(CredentialsFilterTest, ShouldSave_SignedInWithSyncFeatureOff) {
  const char kPrimaryAccountEmail[] = "primary_user@example.org";

  FakeSigninAs(kPrimaryAccountEmail, signin::ConsentLevel::kSignin);

  // Non-Gaia forms should always offer saving.
  EXPECT_TRUE(filter_->ShouldSave(SimpleNonGaiaForm(kPrimaryAccountEmail)));
  EXPECT_TRUE(filter_->ShouldSave(SimpleNonGaiaForm("")));
  EXPECT_TRUE(filter_->ShouldSave(
      SimpleForm("https://subdomain.google.com/", kPrimaryAccountEmail)));
  EXPECT_TRUE(
      filter_->ShouldSave(SimpleForm("https://subdomain.google.com/", "")));

  // Gaia forms can offer saving if the username doesn't match the primary
  // account.
  EXPECT_FALSE(filter_->ShouldSave(SimpleGaiaForm(kPrimaryAccountEmail)));
  EXPECT_TRUE(
      filter_->ShouldSave(SimpleGaiaForm("arbitrary_user@example.org")));
  EXPECT_FALSE(filter_->ShouldSave(SimpleGaiaForm("")));
  EXPECT_FALSE(filter_->ShouldSave(SimpleGAIAChangePasswordForm()));
}

TEST_F(CredentialsFilterTest, ShouldSave_SignIn_Form) {
  PasswordForm form = SimpleGaiaForm("user@example.org");
  form.form_data.set_is_gaia_with_skip_save_password_form(true);

  SetSyncingPasswords(false);
  EXPECT_FALSE(filter_->ShouldSave(form));
}

TEST_F(CredentialsFilterTest, ShouldSaveIfBrowserSigninDisabled) {
  client_->GetPrefs()->SetBoolean(::prefs::kSigninAllowed, false);
  EXPECT_TRUE(filter_->ShouldSave(SimpleGaiaForm("user@gmail.com")));
}

TEST_F(CredentialsFilterTest, ShouldSaveGaiaPasswordHash) {
  PasswordForm gaia_form = SimpleGaiaForm("user@gmail.org");
  EXPECT_TRUE(filter_->ShouldSaveGaiaPasswordHash(gaia_form));

  PasswordForm other_form = SimpleNonGaiaForm("user@example.org");
  EXPECT_FALSE(filter_->ShouldSaveGaiaPasswordHash(other_form));
}

TEST_F(CredentialsFilterTest, ShouldNotSaveGaiaPasswordHashIncognito) {
  client_->SetIsOffTheRecord(true);
  PasswordForm gaia_form = SimpleGaiaForm("user@gmail.org");
  EXPECT_FALSE(filter_->ShouldSaveGaiaPasswordHash(gaia_form));

  PasswordForm other_form = SimpleNonGaiaForm("user@example.org");
  EXPECT_FALSE(filter_->ShouldSaveGaiaPasswordHash(other_form));
}

TEST_F(CredentialsFilterTest, ShouldSaveEnterprisePasswordHash) {
  PasswordForm gaia_form = SimpleGaiaForm("user@gmail.org");
  EXPECT_FALSE(filter_->ShouldSaveEnterprisePasswordHash(gaia_form));

  PasswordForm other_form = SimpleNonGaiaForm("user@example.org");
  EXPECT_FALSE(filter_->ShouldSaveEnterprisePasswordHash(other_form));

  PasswordForm enterprise_form =
      SimpleNonGaiaForm("user@enterprise.test", kEnterpriseURL);
  EXPECT_TRUE(filter_->ShouldSaveEnterprisePasswordHash(enterprise_form));
}

TEST_F(CredentialsFilterTest, ShouldNotSaveEnterprisePasswordHashIncognito) {
  client_->SetIsOffTheRecord(true);
  PasswordForm gaia_form = SimpleGaiaForm("user@gmail.org");
  EXPECT_FALSE(filter_->ShouldSaveEnterprisePasswordHash(gaia_form));

  PasswordForm other_form = SimpleNonGaiaForm("user@example.org");
  EXPECT_FALSE(filter_->ShouldSaveEnterprisePasswordHash(other_form));

  PasswordForm enterprise_form =
      SimpleNonGaiaForm("user@enterprise.test", kEnterpriseURL);
  EXPECT_FALSE(filter_->ShouldSaveEnterprisePasswordHash(enterprise_form));
}

TEST_F(CredentialsFilterTest, IsSyncAccountEmailWithSyncFeatureEnabled) {
  FakeSigninAs("user@gmail.com", signin::ConsentLevel::kSync);
  EXPECT_FALSE(filter_->IsSyncAccountEmail("user"));
  EXPECT_FALSE(filter_->IsSyncAccountEmail("user2@gmail.com"));
  EXPECT_FALSE(filter_->IsSyncAccountEmail("user2@example.com"));
  EXPECT_TRUE(filter_->IsSyncAccountEmail("user@gmail.com"));
  EXPECT_TRUE(filter_->IsSyncAccountEmail("us.er@gmail.com"));
  EXPECT_TRUE(filter_->IsSyncAccountEmail("user@googlemail.com"));
}

TEST_F(CredentialsFilterTest,
       IsSyncAccountEmailWithSyncFeatureEnabledAndIncognito) {
  client_->SetIsOffTheRecord(true);
  FakeSigninAs("user@gmail.com", signin::ConsentLevel::kSync);
  EXPECT_FALSE(filter_->IsSyncAccountEmail("user"));
  EXPECT_FALSE(filter_->IsSyncAccountEmail("user2@gmail.com"));
  EXPECT_FALSE(filter_->IsSyncAccountEmail("user2@example.com"));
  EXPECT_TRUE(filter_->IsSyncAccountEmail("user@gmail.com"));
  EXPECT_TRUE(filter_->IsSyncAccountEmail("us.er@gmail.com"));
  EXPECT_TRUE(filter_->IsSyncAccountEmail("user@googlemail.com"));
}

}  // namespace password_manager
