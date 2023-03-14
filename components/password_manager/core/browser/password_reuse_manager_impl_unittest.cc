// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_reuse_manager_impl.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_signin_notifier.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::UnorderedElementsAreArray;

PasswordForm CreateForm(
    base::StringPiece signon_realm,
    base::StringPiece16 username,
    base::StringPiece16 password,
    PasswordForm::Store store = PasswordForm::Store::kProfileStore) {
  PasswordForm form;
  form.scheme = PasswordForm::Scheme::kHtml;
  form.signon_realm = std::string(signon_realm);
  form.username_value = std::u16string(username);
  form.password_value = std::u16string(password);
  form.url = GURL(signon_realm);
  form.date_last_used =
      base::Time::FromDoubleT(1546300800);  // 00:00 Jan 1 2019 UTC
  form.date_created =
      base::Time::FromDoubleT(1546300800);  // 00:00 Jan 1 2019 UTC
  form.in_store = store;
  return form;
}

absl::optional<PasswordHashData> GetPasswordFromPref(
    const std::string& username,
    bool is_gaia_password,
    TestingPrefServiceSimple& prefs) {
  HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(&prefs);

  return hash_password_manager.RetrievePasswordHash(username, is_gaia_password);
}

class MockPasswordStoreSigninNotifier : public PasswordStoreSigninNotifier {
 public:
  MOCK_METHOD(void,
              SubscribeToSigninEvents,
              (PasswordReuseManager * manager),
              (override));
  MOCK_METHOD(void, UnsubscribeFromSigninEvents, (), (override));
};

class PasswordReuseManagerImplTest : public testing::Test {
 public:
  PasswordReuseManagerImplTest() = default;
  ~PasswordReuseManagerImplTest() override = default;

  void SetUp() override {
    // Mock OSCrypt. There is a call to OSCrypt on initializling
    // PasswordReuseDetector, so it should be mocked.
    OSCryptMocker::SetUp();

    feature_list_.InitWithFeatures({features::kPasswordReuseDetectionEnabled},
                                   {});

    prefs_.registry()->RegisterBooleanPref(prefs::kWereOldGoogleLoginsRemoved,
                                           false);
    prefs_.registry()->RegisterListPref(prefs::kPasswordHashDataList,
                                        PrefRegistry::NO_REGISTRATION_FLAGS);

    profile_store_ = base::MakeRefCounted<TestPasswordStore>();
    profile_store_->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
    account_store_ = base::MakeRefCounted<TestPasswordStore>();
    account_store_->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
    reuse_manager_.Init(&prefs(), profile_store(), account_store());
    FastForwardUntilNoTasksRemain();
  }

  void TearDown() override {
    OSCryptMocker::TearDown();
    reuse_manager_.Shutdown();
    profile_store_->ShutdownOnUIThread();
    account_store_->ShutdownOnUIThread();
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }
  void FastForwardUntilNoTasksRemain() {
    task_environment_.FastForwardUntilNoTasksRemain();
  }
  TestPasswordStore* profile_store() { return profile_store_.get(); }
  TestPasswordStore* account_store() { return account_store_.get(); }
  PasswordReuseManager* reuse_manager() { return &reuse_manager_; }
  TestingPrefServiceSimple& prefs() { return prefs_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple prefs_;
  scoped_refptr<TestPasswordStore> profile_store_;
  scoped_refptr<TestPasswordStore> account_store_;
  PasswordReuseManagerImpl reuse_manager_;
};

TEST_F(PasswordReuseManagerImplTest, CheckPasswordReuse) {
  std::vector<PasswordForm> forms = {
      CreateForm("https://www.google.com", u"username1", u"password"),
      CreateForm("https://facebook.com", u"username2", u"topsecret")};

  for (const auto& form : forms) {
    profile_store()->AddLogin(form);
  }

  struct {
    const char16_t* input;
    const char* domain;
    const size_t reused_password_len;  // Set to 0 if no reuse is expected.
  } kReuseTestData[] = {
      {u"12345password", "https://evil.com", strlen("password")},
      {u"1234567890", "https://evil.com", 0},
      {u"topsecret", "https://m.facebook.com", 0},
  };
  RunUntilIdle();

  for (const auto& test_data : kReuseTestData) {
    MockPasswordReuseDetectorConsumer mock_consumer;
    if (test_data.reused_password_len != 0) {
      const std::vector<MatchingReusedCredential> credentials = {
          {"https://www.google.com", u"username1",
           PasswordForm::Store::kProfileStore}};
      EXPECT_CALL(mock_consumer,
                  OnReuseCheckDone(true, test_data.reused_password_len,
                                   Matches(absl::nullopt),
                                   ElementsAreArray(credentials), 2, _, _));
    } else {
      EXPECT_CALL(mock_consumer, OnReuseCheckDone(false, _, _, _, _, _, _));
    }

    reuse_manager()->CheckReuse(test_data.input, test_data.domain,
                                &mock_consumer);
    RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(&mock_consumer);
  }
}

TEST_F(PasswordReuseManagerImplTest, BasicSynced) {
  ASSERT_FALSE(prefs().HasPrefPath(prefs::kSyncPasswordHash));

  const std::u16string sync_password = u"password";
  const std::u16string input = u"123password";
  reuse_manager()->SaveGaiaPasswordHash(
      "sync_username", sync_password,
      /*is_primary_account=*/true,
      metrics_util::GaiaPasswordHashChange::SAVED_ON_CHROME_SIGNIN);
  RunUntilIdle();

  EXPECT_TRUE(prefs().HasPrefPath(prefs::kPasswordHashDataList));
  absl::optional<PasswordHashData> sync_password_hash =
      GetPasswordFromPref("sync_username", /*is_gaia_password=*/true, prefs());
  EXPECT_TRUE(sync_password_hash.has_value());

  // Check that sync password reuse is found.
  MockPasswordReuseDetectorConsumer mock_consumer;
  EXPECT_CALL(mock_consumer, OnReuseCheckDone(true, sync_password.size(),
                                              Matches(sync_password_hash),
                                              IsEmpty(), 0, _, _));
  reuse_manager()->CheckReuse(input, "https://facebook.com", &mock_consumer);
  RunUntilIdle();
}

TEST_F(PasswordReuseManagerImplTest, BasicUnsynced) {
  ASSERT_FALSE(prefs().HasPrefPath(prefs::kSyncPasswordHash));

  const std::u16string gaia_password = u"3password";
  const std::u16string input = u"123password";
  // Save a non-sync Gaia password.
  reuse_manager()->SaveGaiaPasswordHash(
      "other_gaia_username", gaia_password,
      /*is_primary_account=*/false,
      GaiaPasswordHashChange::NOT_SYNC_PASSWORD_CHANGE);
  absl::optional<PasswordHashData> gaia_password_hash = GetPasswordFromPref(
      "other_gaia_username", /*is_gaia_password=*/true, prefs());
  ASSERT_TRUE(gaia_password_hash.has_value());

  // Check that Gaia password reuse is found.
  MockPasswordReuseDetectorConsumer mock_consumer;
  EXPECT_CALL(mock_consumer, OnReuseCheckDone(true, gaia_password.size(),
                                              Matches(gaia_password_hash),
                                              IsEmpty(), 0, _, _));
  reuse_manager()->CheckReuse(input, "https://example.com", &mock_consumer);
  RunUntilIdle();
}

TEST_F(PasswordReuseManagerImplTest, ClearGaiaPasswordHash) {
  ASSERT_FALSE(prefs().HasPrefPath(prefs::kSyncPasswordHash));

  const std::u16string gaia_password = u"3password";
  const std::u16string input = u"123password";
  // Save a non-sync Gaia password.
  reuse_manager()->SaveGaiaPasswordHash(
      "sync_username", gaia_password,
      /*is_primary_account=*/true,
      metrics_util::GaiaPasswordHashChange::SAVED_ON_CHROME_SIGNIN);
  absl::optional<PasswordHashData> gaia_password_hash =
      GetPasswordFromPref("sync_username", /*is_gaia_password=*/true, prefs());
  ASSERT_TRUE(gaia_password_hash.has_value());

  // Check that no sync password reuse is found after clearing the password
  // hash.
  reuse_manager()->ClearGaiaPasswordHash("sync_username");
  EXPECT_EQ(0u, prefs().GetList(prefs::kPasswordHashDataList).size());
  MockPasswordReuseDetectorConsumer mock_consumer;
  EXPECT_CALL(mock_consumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  reuse_manager()->CheckReuse(input, "https://facebook.com", &mock_consumer);
  RunUntilIdle();
}

TEST_F(PasswordReuseManagerImplTest, ClearAllGaiaPasswordHash) {
  ASSERT_FALSE(prefs().HasPrefPath(prefs::kSyncPasswordHash));

  const std::u16string gaia_password = u"3password";
  const std::u16string input = u"123password";
  // Save a non-sync Gaia password.
  reuse_manager()->SaveGaiaPasswordHash(
      "other_gaia_username", gaia_password,
      /*is_primary_account=*/false,
      GaiaPasswordHashChange::NOT_SYNC_PASSWORD_CHANGE);
  absl::optional<PasswordHashData> gaia_password_hash = GetPasswordFromPref(
      "other_gaia_username", /*is_gaia_password=*/true, prefs());
  ASSERT_TRUE(gaia_password_hash.has_value());

  reuse_manager()->ClearAllGaiaPasswordHash();

  // Check that no Gaia password reuse is found after clearing all Gaia
  // password hash.
  MockPasswordReuseDetectorConsumer mock_consumer;
  EXPECT_EQ(0u, prefs().GetList(prefs::kPasswordHashDataList).size());
  EXPECT_CALL(mock_consumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  reuse_manager()->CheckReuse(input, "https://example.com", &mock_consumer);
  RunUntilIdle();
}

TEST_F(PasswordReuseManagerImplTest, SaveEnterprisePasswordHash) {
  ASSERT_FALSE(prefs().HasPrefPath(prefs::kSyncPasswordHash));

  const std::u16string input = u"123password";
  const std::u16string enterprise_password = u"23password";
  reuse_manager()->SaveEnterprisePasswordHash("enterprise_username",
                                              enterprise_password);
  absl::optional<PasswordHashData> enterprise_password_hash =
      GetPasswordFromPref("enterprise_username", /*is_gaia_password=*/false,
                          prefs());
  ASSERT_TRUE(enterprise_password_hash.has_value());

  // Check that enterprise password reuse is found.
  MockPasswordReuseDetectorConsumer mock_consumer;
  EXPECT_CALL(mock_consumer, OnReuseCheckDone(true, enterprise_password.size(),
                                              Matches(enterprise_password_hash),
                                              IsEmpty(), 0, _, _));
  reuse_manager()->CheckReuse(input, "https://example.com", &mock_consumer);
  RunUntilIdle();
}

TEST_F(PasswordReuseManagerImplTest, ClearAllEnterprisePasswordHash) {
  ASSERT_FALSE(prefs().HasPrefPath(prefs::kSyncPasswordHash));

  const std::u16string input = u"123password";
  const std::u16string enterprise_password = u"23password";
  reuse_manager()->SaveEnterprisePasswordHash("enterprise_username",
                                              enterprise_password);
  absl::optional<PasswordHashData> enterprise_password_hash =
      GetPasswordFromPref("enterprise_username", /*is_gaia_password=*/false,
                          prefs());
  ASSERT_TRUE(enterprise_password_hash.has_value());

  // Check that no enterprise password reuse is found after clearing the
  // password hash.
  reuse_manager()->ClearAllEnterprisePasswordHash();
  EXPECT_EQ(0u, prefs().GetList(prefs::kPasswordHashDataList).size());
  MockPasswordReuseDetectorConsumer mock_consumer;
  EXPECT_CALL(mock_consumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  reuse_manager()->CheckReuse(input, "https://example.com", &mock_consumer);
  RunUntilIdle();
}

TEST_F(PasswordReuseManagerImplTest, ClearAllNonGmailPasswordHash) {
  const std::u16string non_sync_gaia_password = u"3password";
  const std::u16string gmail_password = u"gmailpass";

  // Save a non Gmail password.
  reuse_manager()->SaveGaiaPasswordHash(
      "non_sync_gaia_password@gsuite.com", non_sync_gaia_password,
      /*is_primary_account=*/false,
      GaiaPasswordHashChange::NOT_SYNC_PASSWORD_CHANGE);
  absl::optional<PasswordHashData> non_sync_gaia_password_hash =
      GetPasswordFromPref("non_sync_gaia_password@gsuite.com",
                          /*is_gaia_password=*/true, prefs());
  ASSERT_TRUE(non_sync_gaia_password_hash.has_value());

  // Save a Gmail password.
  reuse_manager()->SaveGaiaPasswordHash(
      "username@gmail.com", gmail_password,
      /*is_primary_account=*/false,
      GaiaPasswordHashChange::NOT_SYNC_PASSWORD_CHANGE);
  RunUntilIdle();
  EXPECT_TRUE(prefs().HasPrefPath(prefs::kPasswordHashDataList));
  absl::optional<PasswordHashData> gmail_password_hash = GetPasswordFromPref(
      "username@gmail.com", /*is_gaia_password=*/true, prefs());
  ASSERT_TRUE(gmail_password_hash.has_value());

  EXPECT_EQ(2u, prefs().GetList(prefs::kPasswordHashDataList).size());

  // Check that no non-gmail password reuse is found after clearing the
  // password hash.
  reuse_manager()->ClearAllNonGmailPasswordHash();
  MockPasswordReuseDetectorConsumer mock_consumer;
  EXPECT_EQ(1u, prefs().GetList(prefs::kPasswordHashDataList).size());
  EXPECT_CALL(mock_consumer, OnReuseCheckDone(false, _, _, _, _, _, _));
  reuse_manager()->CheckReuse(non_sync_gaia_password, "https://example.com",
                              &mock_consumer);
  RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_consumer);
  EXPECT_CALL(mock_consumer, OnReuseCheckDone(true, gmail_password.size(),
                                              Matches(gmail_password_hash),
                                              IsEmpty(), 0, _, _));
  reuse_manager()->CheckReuse(gmail_password, "https://example.com",
                              &mock_consumer);
  RunUntilIdle();
}

TEST_F(PasswordReuseManagerImplTest, ReportMetrics) {
  // Hash does not exist yet.
  base::HistogramTester histogram_tester;
  reuse_manager()->ReportMetrics("not_sync_username",
                                 /*is_under_advanced_protection=*/true);
  std::string name =
      "PasswordManager.IsSyncPasswordHashSavedForAdvancedProtectionUser";
  histogram_tester.ExpectBucketCount(
      name, metrics_util::IsSyncPasswordHashSaved::NOT_SAVED, 1);

  // Save password.
  const std::u16string not_sync_password = u"password";
  const std::u16string input = u"123password";
  reuse_manager()->SaveGaiaPasswordHash(
      "not_sync_username", not_sync_password,
      /*is_primary_account=*/false,
      GaiaPasswordHashChange::NOT_SYNC_PASSWORD_CHANGE);
  RunUntilIdle();

  reuse_manager()->ReportMetrics("not_sync_username",
                                 /*is_under_advanced_protection=*/true);
  // Check that the non sync hash password was saved.
  histogram_tester.ExpectBucketCount(
      name, metrics_util::IsSyncPasswordHashSaved::SAVED_VIA_LIST_PREF, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.NonSyncPasswordHashChange",
      GaiaPasswordHashChange::NOT_SYNC_PASSWORD_CHANGE, 1);
}

TEST_F(PasswordReuseManagerImplTest,
       SubscriptionAndUnsubscriptionFromSignInEvents) {
  std::unique_ptr<MockPasswordStoreSigninNotifier> notifier =
      std::make_unique<MockPasswordStoreSigninNotifier>();
  MockPasswordStoreSigninNotifier* notifier_weak = notifier.get();

  // Check that |reuse_manager| is subscribed to sign-in events.
  EXPECT_CALL(*notifier_weak, SubscribeToSigninEvents(reuse_manager()));
  reuse_manager()->SetPasswordStoreSigninNotifier(std::move(notifier));
  testing::Mock::VerifyAndClearExpectations(reuse_manager());

  // Check that |reuse_manager| is unsubscribed from sign-in events on shutdown.
  EXPECT_CALL(*notifier_weak, UnsubscribeFromSigninEvents());
}

TEST_F(PasswordReuseManagerImplTest,
       CheckReuseCalledOnPasteReuseExistsInBothStores) {
  std::vector<PasswordForm> profile_forms = {
      CreateForm("https://www.google.com", u"username1", u"password"),
      CreateForm("https://www.google.com", u"username2", u"secretword")};
  PasswordForm account_form =
      CreateForm("https://www.facebook.com", u"username3", u"password",
                 PasswordForm::Store::kAccountStore);

  for (const auto& form : profile_forms)
    profile_store()->AddLogin(form);
  account_store()->AddLogin(account_form);

  RunUntilIdle();

  MockPasswordReuseDetectorConsumer mock_consumer;
  EXPECT_CALL(
      mock_consumer,
      OnReuseCheckDone(
          /* is_reuse_found=*/true, /*password_length=*/8,
          Matches(absl::nullopt),
          UnorderedElementsAreArray(std::vector<MatchingReusedCredential>{
              {"https://www.google.com", u"username1",
               PasswordForm::Store::kProfileStore},
              {"https://www.facebook.com", u"username3",
               PasswordForm::Store::kAccountStore}}),
          /*saved_passwords=*/3, _, _));
  reuse_manager()->CheckReuse(u"12345password", "https://evil.com",
                              &mock_consumer);
  RunUntilIdle();
}

TEST_F(PasswordReuseManagerImplTest, NoReuseFoundAfterClearingAccountStorage) {
  std::vector<PasswordForm> account_forms = {
      CreateForm("https://www.google.com", u"username1", u"password",
                 PasswordForm::Store::kAccountStore),
      CreateForm("https://www.google.com", u"username2", u"secretword",
                 PasswordForm::Store::kAccountStore)};

  for (const auto& form : account_forms)
    account_store()->AddLogin(form);

  RunUntilIdle();

  account_store()->Clear();
  account_store()->CallSyncEnabledOrDisabledCallbacks();
  MockPasswordReuseDetectorConsumer mock_consumer;
  EXPECT_CALL(mock_consumer,
              OnReuseCheckDone(/* is_reuse_found=*/false, _, _, IsEmpty(),
                               /*saved_passwords=*/0, _, _));
  reuse_manager()->CheckReuse(u"password", "https://evil.com", &mock_consumer);
  RunUntilIdle();
}

}  // namespace

}  // namespace password_manager
