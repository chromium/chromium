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
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/browser/sync_username_test_base.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
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

void DisallowSyncOnReauth(base::test::ScopedFeatureList* feature_list) {
  feature_list->InitFromCommandLine(
      features::kProtectSyncCredentialOnReauth.name,
      features::kProtectSyncCredential.name);
}

void DisallowSync(base::test::ScopedFeatureList* feature_list) {
  feature_list->InitFromCommandLine(
      features::kProtectSyncCredential.name + std::string(",") +
          features::kProtectSyncCredentialOnReauth.name,
      std::string());
}

class FakePasswordManagerClient : public StubPasswordManagerClient {
 public:
  FakePasswordManagerClient()
      : password_store_(new testing::NiceMock<MockPasswordStore>),
        is_incognito_(false) {
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
  MockPasswordStore* GetPasswordStore() const override {
    return password_store_.get();
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
  base::MessageLoop message_loop_;  // For |password_store_|.
  GURL last_committed_entry_url_;
  scoped_refptr<testing::NiceMock<MockPasswordStore>> password_store_;
  bool is_incognito_;
#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
#endif  // SYNC_PASSWORD_REUSE_DETECTION_ENABLED

  DISALLOW_COPY_AND_ASSIGN(FakePasswordManagerClient);
};

bool IsFormFiltered(const CredentialsFilter* filter, const PasswordForm& form) {
  std::vector<std::unique_ptr<PasswordForm>> vector;
  vector.push_back(std::make_unique<PasswordForm>(form));
  vector = filter->FilterResults(std::move(vector));
  return vector.empty();
}

}  // namespace

class CredentialsFilterTest : public SyncUsernameTestBase {
 public:
  struct TestCase {
    enum { SYNCING_PASSWORDS, NOT_SYNCING_PASSWORDS } password_sync;
    PasswordForm form;
    const char* const last_committed_entry_url;
    enum { FORM_FILTERED, FORM_NOT_FILTERED } is_form_filtered;
    enum { NO_HISTOGRAM, HISTOGRAM_REPORTED } histogram_reported;
  };

  // Flag for creating a PasswordFormManager, deciding its IsNewLogin() value.
  enum class LoginState { NEW, EXISTING };

  CredentialsFilterTest()
      : password_manager_(&client_),
        pending_(SimpleGaiaForm("user@gmail.com")),
        form_manager_(&password_manager_,
                      &client_,
                      driver_.AsWeakPtr(),
                      pending_,
                      std::make_unique<StubFormSaver>(),
                      &fetcher_),
        filter_(&client_,
                base::BindRepeating(&SyncUsernameTestBase::sync_service,
                                    base::Unretained(this)),
                base::BindRepeating(&SyncUsernameTestBase::signin_manager,
                                    base::Unretained(this))) {
    form_manager_.Init(nullptr);
    fetcher_.Fetch();
  }

  void CheckFilterResultsTestCase(const TestCase& test_case) {
    DCHECK(signin_manager()->IsAuthenticated());

    SetSyncingPasswords(test_case.password_sync == TestCase::SYNCING_PASSWORDS);
    client_.set_last_committed_entry_url(test_case.last_committed_entry_url);
    base::HistogramTester tester;
    const bool expected_is_form_filtered =
        test_case.is_form_filtered == TestCase::FORM_FILTERED;
    EXPECT_EQ(expected_is_form_filtered,
              IsFormFiltered(&filter_, test_case.form));
    if (test_case.histogram_reported == TestCase::HISTOGRAM_REPORTED) {
      tester.ExpectUniqueSample("PasswordManager.SyncCredentialFiltered",
                                expected_is_form_filtered, 1);
    } else {
      tester.ExpectTotalCount("PasswordManager.SyncCredentialFiltered", 0);
    }
  }

  // Makes |form_manager_| provisionally save |pending_|. Depending on
  // |login_state| being NEW or EXISTING, prepares |form_manager_| in a state in
  // which |pending_| looks like a new or existing credential, respectively.
  void SavePending(LoginState login_state) {
    std::vector<const PasswordForm*> matches;
    if (login_state == LoginState::EXISTING) {
      matches.push_back(&pending_);
    }
    fetcher_.SetNonFederated(matches, 0u);

    form_manager_.ProvisionallySave(pending_);
  }

 protected:
  FakePasswordManagerClient client_;
  PasswordManager password_manager_;
  StubPasswordManagerDriver driver_;
  PasswordForm pending_;
  FakeFormFetcher fetcher_;
  PasswordFormManager form_manager_;

  SyncCredentialsFilter filter_;
};

TEST_F(CredentialsFilterTest, FilterResults_AllowAll_NonSyncingAccount) {
  FakeSigninAs("another_user@example.org");

  CheckFilterResultsTestCase(
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "https://accounts.google.com/login?rart=123&continue=blah",
       TestCase::FORM_NOT_FILTERED, TestCase::NO_HISTOGRAM});
}

TEST_F(CredentialsFilterTest, FilterResults_AllowAll_SyncingAccount) {
  FakeSigninAs("user@example.org");

  // By default, sync username is not filtered at all.
  const TestCase kTestCases[] = {
      // Reauth URL.
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "https://accounts.google.com/login?rart=123&continue=blah",
       TestCase::FORM_NOT_FILTERED, TestCase::NO_HISTOGRAM},

      // Slightly invalid reauth URL.
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "https://accounts.google.com/addlogin?rart",  // Missing rart value.
       TestCase::FORM_NOT_FILTERED, TestCase::NO_HISTOGRAM},

      // Non-reauth URL.
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "https://accounts.google.com/login?param=123",
       TestCase::FORM_NOT_FILTERED, TestCase::NO_HISTOGRAM},

      // Non-GAIA "reauth" URL.
      {TestCase::SYNCING_PASSWORDS, SimpleNonGaiaForm("user@example.org"),
       "https://site.com/login?rart=678", TestCase::FORM_NOT_FILTERED,
       TestCase::NO_HISTOGRAM},
  };

  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "i=" << i);
    CheckFilterResultsTestCase(kTestCases[i]);
  }
}

TEST_F(CredentialsFilterTest,
       FilterResults_DisallowSyncOnReauth_NonSyncingAccount) {
  FakeSigninAs("another_user@example.org");

  // Only 'ProtectSyncCredentialOnReauth' feature is kept enabled, fill the
  // sync credential everywhere but on reauth.
  base::test::ScopedFeatureList scoped_feature_list;
  DisallowSyncOnReauth(&scoped_feature_list);

  CheckFilterResultsTestCase(
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "https://accounts.google.com/login?rart=123&continue=blah",
       TestCase::FORM_NOT_FILTERED, TestCase::HISTOGRAM_REPORTED});
}

TEST_F(CredentialsFilterTest,
       FilterResults_DisallowSyncOnReauth_SyncingAccount) {
  FakeSigninAs("user@example.org");

  // Only 'ProtectSyncCredentialOnReauth' feature is kept enabled, fill the
  // sync credential everywhere but on reauth.
  base::test::ScopedFeatureList scoped_feature_list;
  DisallowSyncOnReauth(&scoped_feature_list);

  const TestCase kTestCases[] = {
      // Reauth URL.
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "https://accounts.google.com/login?rart=123&continue=blah",
       TestCase::FORM_FILTERED, TestCase::HISTOGRAM_REPORTED},

      // Slightly invalid reauth URL.
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "https://accounts.google.com/addlogin?rart",  // Missing rart value.
       TestCase::FORM_FILTERED, TestCase::HISTOGRAM_REPORTED},

      // Non-reauth URL.
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "https://accounts.google.com/login?param=123",
       TestCase::FORM_NOT_FILTERED, TestCase::NO_HISTOGRAM},

      // Non-GAIA "reauth" URL.
      {TestCase::SYNCING_PASSWORDS, SimpleNonGaiaForm("user@example.org"),
       "https://site.com/login?rart=678", TestCase::FORM_NOT_FILTERED,
       TestCase::NO_HISTOGRAM},
  };

  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "i=" << i);
    CheckFilterResultsTestCase(kTestCases[i]);
  }
}

TEST_F(CredentialsFilterTest, FilterResults_DisallowSync_NonSyncingAccount) {
  FakeSigninAs("another_user@example.org");

  // Both features are kept enabled, should cause sync credential to be
  // filtered.
  base::test::ScopedFeatureList scoped_feature_list;
  DisallowSync(&scoped_feature_list);

  CheckFilterResultsTestCase(
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "https://accounts.google.com/login?rart=123&continue=blah",
       TestCase::FORM_NOT_FILTERED, TestCase::HISTOGRAM_REPORTED});
}

TEST_F(CredentialsFilterTest, FilterResults_DisallowSync_SyncingAccount) {
  FakeSigninAs("user@example.org");

  // Both features are kept enabled, should cause sync credential to be
  // filtered.
  base::test::ScopedFeatureList scoped_feature_list;
  DisallowSync(&scoped_feature_list);

  const TestCase kTestCases[] = {
      // Reauth URL.
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "https://accounts.google.com/login?rart=123&continue=blah",
       TestCase::FORM_FILTERED, TestCase::HISTOGRAM_REPORTED},

      // Slightly invalid reauth URL.
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "https://accounts.google.com/addlogin?rart",  // Missing rart value.
       TestCase::FORM_FILTERED, TestCase::HISTOGRAM_REPORTED},

      // Non-reauth URL.
      {TestCase::SYNCING_PASSWORDS, SimpleGaiaForm("user@example.org"),
       "https://accounts.google.com/login?param=123", TestCase::FORM_FILTERED,
       TestCase::HISTOGRAM_REPORTED},

      // Non-GAIA "reauth" URL.
      {TestCase::SYNCING_PASSWORDS, SimpleNonGaiaForm("user@example.org"),
       "https://site.com/login?rart=678", TestCase::FORM_NOT_FILTERED,
       TestCase::HISTOGRAM_REPORTED},
  };

  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "i=" << i);
    CheckFilterResultsTestCase(kTestCases[i]);
  }
}

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
            signin_manager()->GetAuthenticatedAccountInfo().email);
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
  form.is_gaia_with_skip_save_password_form = true;

  SetSyncingPasswords(false);
  EXPECT_FALSE(filter_.ShouldSave(form));
}

TEST_F(CredentialsFilterTest, ShouldSave_SyncCredential_NotSyncingPasswords) {
  PasswordForm form = SimpleGaiaForm("user@example.org");

  FakeSigninAs("user@example.org");
  SetSyncingPasswords(false);
  EXPECT_TRUE(filter_.ShouldSave(form));
}

TEST_F(CredentialsFilterTest, ShouldFilterOneForm) {
  // Both features are kept enabled, should cause sync credential to be
  // filtered.
  base::test::ScopedFeatureList scoped_feature_list;
  DisallowSync(&scoped_feature_list);

  std::vector<std::unique_ptr<PasswordForm>> results;
  results.push_back(
      std::make_unique<PasswordForm>(SimpleGaiaForm("test1@gmail.com")));
  results.push_back(
      std::make_unique<PasswordForm>(SimpleGaiaForm("test2@gmail.com")));

  FakeSigninAs("test1@gmail.com");

  results = filter_.FilterResults(std::move(results));

  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(SimpleGaiaForm("test2@gmail.com"), *results[0]);
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
