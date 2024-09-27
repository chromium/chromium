// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/store_metrics_reporter.h"

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/mock_password_manager_settings_service.h"
#include "components/password_manager/core/browser/mock_password_reuse_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/sync_username_test_base.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Bool;
using ::testing::Range;
using ::testing::Return;

namespace password_manager {
namespace {

PasswordForm CreateForm(const std::string& signon_realm,
                        const std::string& username,
                        const std::string& password) {
  PasswordForm form;
  form.signon_realm = signon_realm;
  form.username_value = base::ASCIIToUTF16(username);
  form.password_value = base::ASCIIToUTF16(password);
  return form;
}

void AddMetricsTestData(TestPasswordStore* store) {
  PasswordForm password_form;
  password_form.url = GURL("http://example.com");
  password_form.username_value = u"test1@gmail.com";
  password_form.password_value = u"test";
  password_form.signon_realm = "http://example.com/";
  password_form.times_used_in_html_form = 0;
  store->AddLogin(password_form);

  password_form.username_value = u"test2@gmail.com";
  password_form.times_used_in_html_form = 1;
  store->AddLogin(password_form);

  password_form.url = GURL("http://second.example.com");
  password_form.signon_realm = "http://second.example.com";
  password_form.times_used_in_html_form = 3;
  store->AddLogin(password_form);

  password_form.username_value = u"test3@gmail.com";
  password_form.type = PasswordForm::Type::kGenerated;
  password_form.times_used_in_html_form = 2;
  store->AddLogin(password_form);

  password_form.url = GURL("ftp://third.example.com/");
  password_form.signon_realm = "ftp://third.example.com/";
  password_form.times_used_in_html_form = 4;
  password_form.scheme = PasswordForm::Scheme::kOther;
  store->AddLogin(password_form);

  password_form.url = GURL("http://second.example.com");
  password_form.username_value = u"shared@gmail.com";
  password_form.type = PasswordForm::Type::kReceivedViaSharing;
  password_form.scheme = PasswordForm::Scheme::kHtml;
  password_form.times_used_in_html_form = 20;
  store->AddLogin(password_form);

  password_form.url = GURL("http://fourth.example.com/");
  password_form.signon_realm = "http://fourth.example.com/";
  password_form.type = PasswordForm::Type::kFormSubmission;
  password_form.username_value = u"";
  password_form.times_used_in_html_form = 10;
  password_form.scheme = PasswordForm::Scheme::kHtml;
  store->AddLogin(password_form);

  password_form.url = GURL("https://fifth.example.com/");
  password_form.signon_realm = "https://fifth.example.com/";
  password_form.username_value = u"";
  password_form.password_value = u"";
  password_form.blocked_by_user = true;
  store->AddLogin(password_form);

  password_form.url = GURL("https://sixth.example.com/");
  password_form.signon_realm = "https://sixth.example.com/";
  password_form.username_value = u"my_username";
  password_form.password_value = u"my_password";
  password_form.blocked_by_user = false;
  store->AddLogin(password_form);

  password_form.url = GURL();
  password_form.signon_realm = "android://hash@com.example.android/";
  password_form.username_value = u"JohnDoe";
  password_form.password_value = u"my_password";
  password_form.blocked_by_user = false;
  store->AddLogin(password_form);

  password_form.username_value = u"JaneDoe";
  store->AddLogin(password_form);

  password_form.url = GURL("http://rsolomakhin.github.io/autofill/");
  password_form.signon_realm = "http://rsolomakhin.github.io/";
  password_form.username_value = u"";
  password_form.password_value = u"";
  password_form.blocked_by_user = true;
  store->AddLogin(password_form);

  password_form.url = GURL("https://rsolomakhin.github.io/autofill/");
  password_form.signon_realm = "https://rsolomakhin.github.io/";
  password_form.blocked_by_user = true;
  store->AddLogin(password_form);

  password_form.url = GURL("http://rsolomakhin.github.io/autofill/123");
  password_form.signon_realm = "http://rsolomakhin.github.io/";
  password_form.blocked_by_user = true;
  store->AddLogin(password_form);

  password_form.url = GURL("https://rsolomakhin.github.io/autofill/1234");
  password_form.signon_realm = "https://rsolomakhin.github.io/";
  password_form.blocked_by_user = true;
  store->AddLogin(password_form);
}

class StoreMetricsReporterTest : public SyncUsernameTestBase {
 public:
  StoreMetricsReporterTest() = default;

  ~StoreMetricsReporterTest() override = default;

  void SetUp() override {
    // Mock OSCrypt. There is a call to OSCrypt inside HashPasswordManager so it
    // should be mocked.
    OSCryptMocker::SetUp();

    feature_list_.InitWithFeatures({features::kPasswordReuseDetectionEnabled},
                                   {});

    prefs_.registry()->RegisterBooleanPref(prefs::kCredentialsEnableService,
                                           false);
    prefs_.registry()->RegisterBooleanPref(
        password_manager::prefs::kWasAutoSignInFirstRunExperienceShown, false);
    prefs_.registry()->RegisterBooleanPref(prefs::kWereOldGoogleLoginsRemoved,
                                           false);
    prefs_.registry()->RegisterDoublePref(
        prefs::kLastTimePasswordStoreMetricsReported, 0.0);
    prefs_.registry()->RegisterBooleanPref(::prefs::kSafeBrowsingEnabled,
                                           false);
    prefs_.registry()->RegisterIntegerPref(
        prefs::kTotalPasswordsAvailableForAccount, 0);
    prefs_.registry()->RegisterIntegerPref(
        prefs::kTotalPasswordsAvailableForProfile, 0);
    prefs_.registry()->RegisterIntegerPref(
        prefs::kPasswordRemovalReasonForAccount, 0);
    prefs_.registry()->RegisterIntegerPref(
        prefs::kPasswordRemovalReasonForProfile, 0);
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
    prefs_.registry()->RegisterDictionaryPref(
        prefs::kAccountStoragePerAccountSettings);
#endif
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
    prefs_.registry()->RegisterBooleanPref(
        prefs::kBiometricAuthenticationBeforeFilling, false);
#endif
#if BUILDFLAG(IS_ANDROID)
    prefs_.registry()->RegisterIntegerPref(
        prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(
            password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff));
#endif
  }

  void TearDown() override { OSCryptMocker::TearDown(); }

  PrefService* pref_service() { return &prefs_; }

  MockPasswordManagerSettingsService& settings_service() {
    return settings_service_;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple prefs_;
  testing::NiceMock<MockPasswordManagerSettingsService> settings_service_;
};

TEST_F(StoreMetricsReporterTest, ReportMetricsPasswordManagerEnabledDefault) {
  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(
      /*profile_store=*/nullptr, /*account_store=*/nullptr, sync_service(),
      &prefs_, /*password_reuse_manager=*/nullptr, &settings_service(),
      /*done_callback=*/base::DoNothing());

  histogram_tester.ExpectUniqueSample("PasswordManager.EnableState", 0, 1);
}

TEST_F(StoreMetricsReporterTest, ReportMetricsForAutoSignIn) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(settings_service(),
              IsSettingEnabled(PasswordManagerSetting::kAutoSignIn))
      .WillOnce(Return(true));

  StoreMetricsReporter reporter(
      /*profile_store=*/nullptr, /*account_store=*/nullptr, sync_service(),
      &prefs_, /*password_reuse_manager=*/nullptr, &settings_service(),
      /*done_callback=*/base::DoNothing());

  histogram_tester.ExpectUniqueSample("PasswordManager.AutoSignin", 1, 1);
}

enum class EnableSettingManageState {
  kUser,
  kExtension,
  kPolicy,
  kRecommended,
  kOther,
};

struct EnableStateParam {
  bool test_pref_value;
  EnableSettingManageState test_setting_manage_state;
  int expected_histogram_value;
};

class StoreMetricsReporterTestWithEnableStateParams
    : public StoreMetricsReporterTest,
      public ::testing::WithParamInterface<EnableStateParam> {};

TEST_P(StoreMetricsReporterTestWithEnableStateParams,
       ReportMetricsPasswordManagerEnableStateTest) {
  const EnableStateParam param = GetParam();

  switch (param.test_setting_manage_state) {
    case EnableSettingManageState::kUser:
      prefs_.SetUserPref(password_manager::prefs::kCredentialsEnableService,
                         std::make_unique<base::Value>(param.test_pref_value));
      break;
    case EnableSettingManageState::kExtension:
      prefs_.SetExtensionPref(
          password_manager::prefs::kCredentialsEnableService,
          std::make_unique<base::Value>(param.test_pref_value));
      break;
    case EnableSettingManageState::kPolicy:
      prefs_.SetManagedPref(
          password_manager::prefs::kCredentialsEnableService,
          std::make_unique<base::Value>(param.test_pref_value));
      break;
    case EnableSettingManageState::kRecommended:
      prefs_.SetRecommendedPref(
          password_manager::prefs::kCredentialsEnableService,
          std::make_unique<base::Value>(param.test_pref_value));
      break;
    default:

      break;
  }

  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(
      /*profile_store=*/nullptr, /*account_store=*/nullptr, sync_service(),
      &prefs_, /*password_reuse_manager=*/nullptr, &settings_service(),
      /*done_callback*/ base::DoNothing());

  histogram_tester.ExpectUniqueSample("PasswordManager.EnableState",
                                      param.expected_histogram_value, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    StoreMetricsReporterTestWithEnableStateParams,
    ::testing::Values(
        EnableStateParam(true, EnableSettingManageState::kUser, 1),
        EnableStateParam(true, EnableSettingManageState::kExtension, 2),
        EnableStateParam(true, EnableSettingManageState::kPolicy, 3),
        EnableStateParam(true, EnableSettingManageState::kRecommended, 4),
        EnableStateParam(false, EnableSettingManageState::kUser, 6),
        EnableStateParam(false, EnableSettingManageState::kExtension, 7),
        EnableStateParam(false, EnableSettingManageState::kPolicy, 8),
        EnableStateParam(false, EnableSettingManageState::kRecommended, 9)));

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)

// The test fixture is used to test StoreIndependentMetrics. Depending on the
// test, the parameter defines whether password manager or
// kBiometricAuthenticationBeforeFilling pref is enabled.
class StoreMetricsReporterTestWithParams
    : public StoreMetricsReporterTest,
      public ::testing::WithParamInterface<bool> {};

TEST_P(StoreMetricsReporterTestWithParams,
       ReportMetricsBiometricAuthBeforeFilling) {
  const bool biometric_auth_before_filling_enabled = GetParam();

  prefs_.SetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling,
      biometric_auth_before_filling_enabled);
  base::HistogramTester histogram_tester;

  StoreMetricsReporter reporter(
      /*profile_store=*/nullptr, /*account_store=*/nullptr, sync_service(),
      &prefs_, /*password_reuse_manager=*/nullptr, &settings_service(),
      /*done_callback*/ base::DoNothing());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.BiometricAuthBeforeFillingEnabled2",
      biometric_auth_before_filling_enabled, 1);
}

INSTANTIATE_TEST_SUITE_P(All, StoreMetricsReporterTestWithParams, Bool());

#endif

TEST_F(StoreMetricsReporterTest, ReportMetricsAtMostOncePerDay) {
  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  auto account_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  account_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);

  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> done_callback_future;
  StoreMetricsReporter reporter(
      profile_store.get(), account_store.get(), sync_service(), &prefs_,
      /*password_reuse_manager=*/nullptr, &settings_service(),
      done_callback_future.GetCallback());
  histogram_tester.ExpectTotalCount("PasswordManager.EnableState", 1);
  ASSERT_TRUE(done_callback_future.Wait());

  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForAccount), 0);
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForProfile), 0);

  // Add new logins and immediately try to report metrics again, no metrics
  // should be reported since not enough time has passwed, but the done_callback
  // should be invoked nevertheless.
  const std::string kRealm = "https://example1.com";
  profile_store->AddLogin(CreateForm(kRealm, "aprofileuser", "aprofilepass"));
  account_store->AddLogin(CreateForm(kRealm, "anaccountuser", "anaccountpass"));

  base::HistogramTester histogram_tester2;
  base::test::TestFuture<void> done_callback_future2;
  StoreMetricsReporter reporter2(
      profile_store.get(), account_store.get(), sync_service(), &prefs_,
      /*password_reuse_manager=*/nullptr, &settings_service(),
      done_callback_future2.GetCallback());
  histogram_tester2.ExpectTotalCount("PasswordManager.Enabled4", 0);
  ASSERT_TRUE(done_callback_future2.Wait());

  // The total passwords count wasn't updated because it's too soon.
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForAccount), 0);
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForProfile), 0);

  profile_store->ShutdownOnUIThread();
  account_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest, ReportPasswordLossMetricForAccount) {
  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  auto account_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  account_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);

  // Setting up the previous password counts.
  pref_service()->SetInteger(prefs::kTotalPasswordsAvailableForAccount, 10);
  pref_service()->SetInteger(prefs::kTotalPasswordsAvailableForProfile, 0);

  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> done_callback_future;
  StoreMetricsReporter reporter(
      profile_store.get(), account_store.get(), sync_service(), &prefs_,
      /*password_reuse_manager=*/nullptr, &settings_service(),
      done_callback_future.GetCallback());
  ASSERT_TRUE(done_callback_future.Wait());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.PasswordLoss", 10, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.PasswordLossPotentialReasonBitmask", 0, 1);
  histogram_tester.ExpectTotalCount("PasswordManager.ProfileStore.PasswordLoss",
                                    0);

  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForAccount), 0);
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForProfile), 0);

  profile_store->ShutdownOnUIThread();
  account_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest, ReportPasswordLossMetricForProfile) {
  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  auto account_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  account_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);

  // Setting up the previous password counts.
  pref_service()->SetInteger(prefs::kTotalPasswordsAvailableForAccount, 0);
  pref_service()->SetInteger(prefs::kTotalPasswordsAvailableForProfile, 10);

  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> done_callback_future;
  StoreMetricsReporter reporter(
      profile_store.get(), account_store.get(), sync_service(), &prefs_,
      /*password_reuse_manager=*/nullptr, &settings_service(),
      done_callback_future.GetCallback());
  ASSERT_TRUE(done_callback_future.Wait());

  histogram_tester.ExpectTotalCount("PasswordManager.AccountStore.PasswordLoss",
                                    0);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProfileStore.PasswordLoss", 10, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProfileStore.PasswordLossPotentialReasonBitmask", 0, 1);

  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForAccount), 0);
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForProfile), 0);

  profile_store->ShutdownOnUIThread();
  account_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest, ReportAccountsPerSiteHiResMetricsTest) {
  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(profile_store.get());
  // Note: We also create and populate an account store here and instruct it to
  // report metrics, even though all the checks below only test the profile DB.
  // This is to make sure that the account DB doesn't write to any of the same
  // histograms.
  auto account_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  account_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(account_store.get());

  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(
      profile_store.get(), account_store.get(), sync_service(), &prefs_,
      /*password_reuse_manager=*/nullptr, &settings_service(),
      /*done_callback*/ base::DoNothing());
  // Wait for the metrics to get reported, which involves queries to the
  // stores, i.e. to background task runners.
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProfileStore.AccountsPerSiteHiRes3."
      "AutoGenerated."
      "WithoutCustomPassphrase",
      1, 2);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProfileStore.AccountsPerSiteHiRes3."
      "ReceivedViaSharing."
      "WithoutCustomPassphrase",
      1, 1);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.ProfileStore.AccountsPerSiteHiRes3."
      "UserCreated."
      "WithoutCustomPassphrase",
      1, 3);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.ProfileStore.AccountsPerSiteHiRes3."
      "UserCreated."
      "WithoutCustomPassphrase",
      2, 2);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.ProfileStore.AccountsPerSiteHiRes3."
      "Overall."
      "WithoutCustomPassphrase",
      1, 6);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.ProfileStore.AccountsPerSiteHiRes3."
      "Overall."
      "WithoutCustomPassphrase",
      2, 2);

  // In this test both profile and account store contained the same 10 test
  // credentials.
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForAccount),
      10);
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForProfile),
      10);

  account_store->ShutdownOnUIThread();
  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest, ReportPasswordProtectedMetricsTest) {
  // Set up custom preferences for this test because we want safe browsing
  // enabled
  prefs_.SetBoolean(::prefs::kSafeBrowsingEnabled, true);

  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_,
                      /*affiliated_match_helper=*/nullptr);
  auto account_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  account_store->Init(&prefs_,
                      /*affiliated_match_helper=*/nullptr);

  // Fill Password Store with 1000 account and profile logins
  const std::string kRealm = "https://example.com";
  const int kNumberOfLogins = 1000;
  const int kHalfOfLogins = kNumberOfLogins / 2;
  for (int protected_num = 0; protected_num < kHalfOfLogins; ++protected_num) {
    account_store->AddLogin(CreateForm(
        kRealm, "protectedaccount" + base::NumberToString(protected_num),
        "protectedaccountpass" + base::NumberToString(protected_num)));
    profile_store->AddLogin(CreateForm(
        kRealm, "protectedprofile" + base::NumberToString(protected_num),
        "protectedprofilepass" + base::NumberToString(protected_num)));
  }
  for (int unprotected_num = 0; unprotected_num < kHalfOfLogins;
       ++unprotected_num) {
    account_store->AddLogin(CreateForm(
        kRealm, "unprotectedaccount" + base::NumberToString(unprotected_num),
        base::NumberToString(unprotected_num)));
    profile_store->AddLogin(CreateForm(
        kRealm, "unprotectedprofile" + base::NumberToString(unprotected_num),
        base::NumberToString(unprotected_num)));
  }

  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(
      profile_store.get(), account_store.get(), sync_service(), &prefs_,
      /*password_reuse_manager=*/nullptr, &settings_service(),
      /*done_callback*/ base::DoNothing());
  // Wait for the metrics to get reported, which involves queries to the
  // stores, i.e. to background task runners.
  RunUntilIdle();

  const int kTotalAccountAndProfileLogins = 2 * kNumberOfLogins;
  // Since there is 10% noise in this histogram, we can't deterministically say
  // what the exact sample count for each bucket will be. Instead, test that the
  // number of samples is within a reasonable range.
  histogram_tester.ExpectTotalCount("PasswordManager.IsPasswordProtected2",
                                    kTotalAccountAndProfileLogins);
  EXPECT_GE(histogram_tester.GetBucketCount(
                "PasswordManager.IsPasswordProtected2", true),
            0.4 * kTotalAccountAndProfileLogins);
  EXPECT_LE(histogram_tester.GetBucketCount(
                "PasswordManager.IsPasswordProtected2", true),
            0.6 * kTotalAccountAndProfileLogins);
  EXPECT_GE(histogram_tester.GetBucketCount(
                "PasswordManager.IsPasswordProtected2", false),
            0.4 * kTotalAccountAndProfileLogins);
  EXPECT_LE(histogram_tester.GetBucketCount(
                "PasswordManager.IsPasswordProtected2", false),
            0.6 * kTotalAccountAndProfileLogins);

  // In this test both profile and account store contained the same 1000 test
  // credentials.
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForAccount),
      1000);
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForProfile),
      1000);

  profile_store->ShutdownOnUIThread();
  account_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest,
       ReportPasswordProtectedMetricsTestWithBlockedPasswords) {
  // Set up custom preferences for this test because we want safe browsing
  // enabled
  prefs_.SetBoolean(::prefs::kSafeBrowsingEnabled, true);

  // Add both non-blocking and blocking credentials to stores
  const std::string kRealm1 = "https://example1.com";
  const std::string kRealm2 = "https://example2.com";
  const std::string kRealm3 = "https://example3.com";
  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_,
                      /*affiliated_match_helper=*/nullptr);
  profile_store->AddLogin(CreateForm(kRealm1, "aprofileuser", "aprofilepass"));
  profile_store->AddLogin(password_manager_util::MakeNormalizedBlocklistedForm(
      PasswordFormDigest(PasswordForm::Scheme::kHtml, kRealm2, GURL(kRealm2))));
  auto account_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  account_store->Init(&prefs_,
                      /*affiliated_match_helper=*/nullptr);
  account_store->AddLogin(
      CreateForm(kRealm1, "anaccountuser", "anaccountpass"));
  account_store->AddLogin(password_manager_util::MakeNormalizedBlocklistedForm(
      PasswordFormDigest(PasswordForm::Scheme::kHtml, kRealm3, GURL(kRealm3))));

  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(
      profile_store.get(), account_store.get(), sync_service(), &prefs_,
      /*password_reuse_manager=*/nullptr, &settings_service(),
      /*done_callback*/ base::DoNothing());
  // Wait for the metrics to get reported, which involves queries to the
  // stores, i.e. to background task runners.
  RunUntilIdle();

  // We expect that our histogram logs for only non-blocking credentials.
  histogram_tester.ExpectTotalCount("PasswordManager.IsPasswordProtected2", 2);

  // In this test profile and account store have 2 credentials each.
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForAccount), 1);
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForProfile), 1);

  profile_store->ShutdownOnUIThread();
  account_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest, ReportTotalAccountsHiResMetricsTest) {
  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(profile_store.get());
  // Note: We also create and populate an account store here and instruct it to
  // report metrics, even though all the checks below only test the profile DB.
  // This is to make sure that the account DB doesn't write to any of the same
  // histograms.
  auto account_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  account_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(account_store.get());

  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(
      profile_store.get(), account_store.get(), sync_service(), &prefs_,
      /*password_reuse_manager=*/nullptr, &settings_service(),
      /*done_callback*/ base::DoNothing());

  // Wait for the metrics to get reported, which involves queries to the
  // stores, i.e. to background task runners.
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProfileStore.TotalAccountsHiRes3."
      "ByType."
      "AutoGenerated."
      "WithoutCustomPassphrase",
      2, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProfileStore.TotalAccountsHiRes3."
      "ByType."
      "UserCreated."
      "WithoutCustomPassphrase",
      7, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProfileStore.TotalAccountsHiRes3."
      "ByType."
      "ReceivedViaSharing."
      "WithoutCustomPassphrase",
      1, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProfileStore.TotalAccountsHiRes3."
      "ByType.Overall."
      "WithoutCustomPassphrase",
      10, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProfileStore.TotalAccountsHiRes3."
      "ByType.Overall",
      10, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProfileStore.TotalAccountsHiRes3."
      "WithScheme."
      "Android",
      2, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProfileStore.TotalAccountsHiRes3."
      "WithScheme.Ftp",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProfileStore.TotalAccountsHiRes3."
      "WithScheme.Http",
      6, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProfileStore.TotalAccountsHiRes3."
      "WithScheme.Https",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProfileStore.TotalAccountsHiRes3."
      "WithScheme.Other",
      0, 1);

  // In this test both profile and account store contained the same 10 test
  // credentials.
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForAccount),
      10);
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForProfile),
      10);

  account_store->ShutdownOnUIThread();
  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest, ReportTimesPasswordUsedMetricsTest) {
  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(profile_store.get());
  // Note: We also create and populate an account store here and instruct it to
  // report metrics, even though all the checks below only test the profile DB.
  // This is to make sure that the account DB doesn't write to any of the same
  // histograms.
  auto account_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  account_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(account_store.get());

  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(
      profile_store.get(), account_store.get(), sync_service(), &prefs_,
      /*password_reuse_manager=*/nullptr, &settings_service(),
      /*done_callback*/ base::DoNothing());

  // Wait for the metrics to get reported, which involves queries to the
  // stores, i.e. to background task runners.
  RunUntilIdle();

  histogram_tester.ExpectBucketCount(
      "PasswordManager.ProfileStore.TimesPasswordUsed3."
      "AutoGenerated."
      "WithoutCustomPassphrase",
      2, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.ProfileStore.TimesPasswordUsed3."
      "AutoGenerated."
      "WithoutCustomPassphrase",
      4, 1);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.ProfileStore.TimesPasswordUsed3."
      "UserCreated."
      "WithoutCustomPassphrase",
      0, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.ProfileStore.TimesPasswordUsed3."
      "UserCreated."
      "WithoutCustomPassphrase",
      1, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.ProfileStore.TimesPasswordUsed3."
      "UserCreated."
      "WithoutCustomPassphrase",
      3, 1);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.ProfileStore.TimesPasswordUsed3."
      "ReceivedViaSharing."
      "WithoutCustomPassphrase",
      20, 1);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.ProfileStore.TimesPasswordUsed3."
      "Overall."
      "WithoutCustomPassphrase",
      0, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.ProfileStore.TimesPasswordUsed3."
      "Overall."
      "WithoutCustomPassphrase",
      1, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.ProfileStore.TimesPasswordUsed3."
      "Overall."
      "WithoutCustomPassphrase",
      2, 1);
  // The bucket for 3 and 4 is the same. Thus we expect two samples here.
  histogram_tester.ExpectBucketCount(
      "PasswordManager.ProfileStore.TimesPasswordUsed3."
      "Overall."
      "WithoutCustomPassphrase",
      3, 2);

  // In this test both profile and account store contained the same 10 test
  // credentials.
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForAccount),
      10);
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForProfile),
      10);

  account_store->ShutdownOnUIThread();
  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

// The following tests are mostly a copy of Report*MetricsTest, but covering
// the account store instead of the profile store. All the metrics that *are*
// covered have
// ".AccountStore" in their names.
TEST_F(StoreMetricsReporterTest,
       ReportAccountStoreAccountsPerSiteHiResMetricsTest) {
  // Note: We also populate the profile store here and instruct it to report
  // metrics, even though all the checks below only test the account DB. This is
  // to make sure that the profile DB doesn't write to any of the same
  // histograms.

  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(profile_store.get());
  // Note: We also create and populate an account store here and instruct it to
  // report metrics, even though all the checks below only test the profile DB.
  // This is to make sure that the account DB doesn't write to any of the same
  // histograms.
  auto account_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  account_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(account_store.get());

  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(
      profile_store.get(), account_store.get(), sync_service(), &prefs_,
      /*password_reuse_manager=*/nullptr, &settings_service(),
      /*done_callback*/ base::DoNothing());

  // Wait for the metrics to get reported, which involves queries to the
  // stores, i.e. to background task runners.
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.AccountsPerSiteHiRes3."
      "AutoGenerated."
      "WithoutCustomPassphrase",
      1, 2);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.AccountsPerSiteHiRes3."
      "UserCreated."
      "WithoutCustomPassphrase",
      1, 3);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.AccountsPerSiteHiRes3."
      "UserCreated."
      "WithoutCustomPassphrase",
      2, 2);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.AccountsPerSiteHiRes3."
      "ReceivedViaSharing."
      "WithoutCustomPassphrase",
      1, 1);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.AccountsPerSiteHiRes3."
      "Overall."
      "WithoutCustomPassphrase",
      1, 6);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.AccountsPerSiteHiRes3."
      "Overall."
      "WithoutCustomPassphrase",
      2, 2);

  // In this test both profile and account store contained the same 10 test
  // credentials.
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForAccount),
      10);
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForProfile),
      10);

  account_store->ShutdownOnUIThread();
  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest,
       ReportAccountStoreTotalAccountsHiResMetricsTest) {
  // Note: We also populate the profile store here and instruct it to report
  // metrics, even though all the checks below only test the account DB. This is
  // to make sure that the profile DB doesn't write to any of the same
  // histograms.

  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(profile_store.get());
  // Note: We also create and populate an account store here and instruct it to
  // report metrics, even though all the checks below only test the profile DB.
  // This is to make sure that the account DB doesn't write to any of the same
  // histograms.
  auto account_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  account_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(account_store.get());

  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(
      profile_store.get(), account_store.get(), sync_service(), &prefs_,
      /*password_reuse_manager=*/nullptr, &settings_service(),
      /*done_callback*/ base::DoNothing());

  // Wait for the metrics to get reported, which involves queries to the
  // stores, i.e. to background task runners.
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes3."
      "ByType.AutoGenerated."
      "WithoutCustomPassphrase",
      2, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes3."
      "ByType.UserCreated."
      "WithoutCustomPassphrase",
      7, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes3."
      "ByType.ReceivedViaSharing."
      "WithoutCustomPassphrase",
      1, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes3."
      "ByType.Overall."
      "WithoutCustomPassphrase",
      10, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes3."
      "ByType.Overall",
      10, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes3."
      "WithScheme.Android",
      2, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes3."
      "WithScheme.Ftp",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes3."
      "WithScheme.Http",
      6, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes3."
      "WithScheme.Https",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStore.TotalAccountsHiRes3."
      "WithScheme.Other",
      0, 1);

  // In this test both profile and account store contained the same 10 test
  // credentials.
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForAccount),
      10);
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForProfile),
      10);

  account_store->ShutdownOnUIThread();
  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest,
       ReportAccountStoreTimesPasswordUsedMetricsTest) {
  // Note: We also populate the profile store here and instruct it to report
  // metrics, even though all the checks below only test the account DB. This is
  // to make sure that the profile DB doesn't write to any of the same
  // histograms.

  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(profile_store.get());
  // Note: We also create and populate an account store here and instruct it to
  // report metrics, even though all the checks below only test the profile DB.
  // This is to make sure that the account DB doesn't write to any of the same
  // histograms.
  auto account_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  account_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  AddMetricsTestData(account_store.get());

  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(
      profile_store.get(), account_store.get(), sync_service(), &prefs_,
      /*password_reuse_manager=*/nullptr, &settings_service(),
      /*done_callback*/ base::DoNothing());

  // Wait for the metrics to get reported, which involves queries to the
  // stores, i.e. to background task runners.
  RunUntilIdle();

  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed3."
      "AutoGenerated."
      "WithoutCustomPassphrase",
      2, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed3."
      "AutoGenerated."
      "WithoutCustomPassphrase",
      4, 1);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed3."
      "UserCreated."
      "WithoutCustomPassphrase",
      0, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed3."
      "UserCreated."
      "WithoutCustomPassphrase",
      1, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed3."
      "UserCreated."
      "WithoutCustomPassphrase",
      3, 1);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed3."
      "ReceivedViaSharing."
      "WithoutCustomPassphrase",
      20, 1);

  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed3."
      "Overall."
      "WithoutCustomPassphrase",
      0, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed3."
      "Overall."
      "WithoutCustomPassphrase",
      1, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed3."
      "Overall."
      "WithoutCustomPassphrase",
      2, 1);
  // The bucket for 3 and 4 is the same. Thus we expect two samples here.
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.TimesPasswordUsed3."
      "Overall."
      "WithoutCustomPassphrase",
      3, 2);

  // In this test both profile and account store contained the same 10 test
  // credentials.
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForAccount),
      10);
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForProfile),
      10);

  account_store->ShutdownOnUIThread();
  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest, DuplicatesMetrics_NoDuplicates) {
  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);

  // No duplicate.
  PasswordForm password_form;
  password_form.signon_realm = "http://example1.com/";
  password_form.url = GURL("http://example1.com/");
  password_form.username_element = u"userelem_1";
  password_form.username_value = u"username_1";
  password_form.password_value = u"password_1";
  profile_store->AddLogin(password_form);

  // Different username -> no duplicate.
  password_form.signon_realm = "http://example2.com/";
  password_form.url = GURL("http://example2.com/");
  password_form.username_value = u"username_1";
  profile_store->AddLogin(password_form);
  password_form.username_value = u"username_2";
  profile_store->AddLogin(password_form);

  // Blocklisted forms don't count as duplicates (neither against other
  // blocklisted forms nor against actual saved credentials).
  password_form.signon_realm = "http://example3.com/";
  password_form.url = GURL("http://example3.com/");
  password_form.username_value = u"username_1";
  profile_store->AddLogin(password_form);
  password_form.blocked_by_user = true;
  password_form.username_value = u"";
  password_form.password_value = u"";
  profile_store->AddLogin(password_form);

  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(
      profile_store.get(), /*account_store=*/nullptr, sync_service(), &prefs_,
      /*password_reuse_manager=*/nullptr, &settings_service(),
      /*done_callback*/ base::DoNothing());

  // Wait for the metrics to get reported, which involves queries to the
  // stores, i.e. to background task runners.
  RunUntilIdle();

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "PasswordManager.CredentialsWithDuplicates3"),
              testing::ElementsAre(base::Bucket(0, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("PasswordManager."
                                     "CredentialsWithMismatchedDuplicates3"),
      testing::ElementsAre(base::Bucket(0, 1)));

  // In this test only the profile store contains credentials to be counted.
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForProfile), 4);

  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest, DuplicatesMetrics_ExactDuplicates) {
  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);

  // Add some PasswordForms that are "exact" duplicates (only the
  // username_element is different, which doesn't matter).
  PasswordForm password_form;
  password_form.signon_realm = "http://example1.com/";
  password_form.url = GURL("http://example1.com/");
  password_form.username_element = u"userelem_1";
  password_form.username_value = u"username_1";
  profile_store->AddLogin(password_form);
  password_form.username_element = u"userelem_2";
  profile_store->AddLogin(password_form);
  // The number of "identical" credentials doesn't matter; we count the *sets*
  // of duplicates.
  password_form.username_element = u"userelem_3";
  profile_store->AddLogin(password_form);

  // Similarly, origin doesn't make forms "different" either.
  password_form.signon_realm = "http://example2.com/";
  password_form.url = GURL("http://example2.com/path1");
  profile_store->AddLogin(password_form);
  password_form.url = GURL("http://example2.com/path2");
  profile_store->AddLogin(password_form);

  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(
      profile_store.get(), /*account_store=*/nullptr, sync_service(), &prefs_,
      /*password_reuse_manager=*/nullptr, &settings_service(),
      /*done_callback*/ base::DoNothing());

  // Wait for the metrics to get reported, which involves queries to the
  // stores, i.e. to background task runners.
  RunUntilIdle();

  // There should be 2 groups of "exact" duplicates.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "PasswordManager.CredentialsWithDuplicates3"),
              testing::ElementsAre(base::Bucket(2, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("PasswordManager."
                                     "CredentialsWithMismatchedDuplicates3"),
      testing::ElementsAre(base::Bucket(0, 1)));

  // In this test only the profile store contains credentials to be counted.
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForProfile), 5);

  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest, DuplicatesMetrics_MismatchedDuplicates) {
  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);

  // Mismatched duplicates: Identical except for the password.
  PasswordForm password_form;
  password_form.signon_realm = "http://example1.com/";
  password_form.url = GURL("http://example1.com/");
  password_form.username_element = u"userelem_1";
  password_form.username_value = u"username_1";
  password_form.password_element = u"passelem_1";
  password_form.password_value = u"password_1";
  profile_store->AddLogin(password_form);
  // Note: password_value is not part of the unique key, so we need to change
  // some other value to be able to insert the duplicate into the DB.
  password_form.password_element = u"passelem_2";
  password_form.password_value = u"password_2";
  profile_store->AddLogin(password_form);
  // The number of "identical" credentials doesn't matter; we count the *sets*
  // of duplicates.
  password_form.password_element = u"passelem_3";
  password_form.password_value = u"password_3";
  profile_store->AddLogin(password_form);

  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(
      profile_store.get(), /*account_store=*/nullptr, sync_service(), &prefs_,
      /*password_reuse_manager=*/nullptr, &settings_service(),
      /*done_callback*/ base::DoNothing());

  // Wait for the metrics to get reported, which involves queries to the
  // stores, i.e. to background task runners.
  RunUntilIdle();

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "PasswordManager.CredentialsWithDuplicates3"),
              testing::ElementsAre(base::Bucket(0, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("PasswordManager."
                                     "CredentialsWithMismatchedDuplicates3"),
      testing::ElementsAre(base::Bucket(1, 1)));

  // In this test only profile store contains credentials to be counted.
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForProfile), 3);

  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

// A test that covers multi-store metrics, which are recorded by the
// StoreMetricsReporter directly.
TEST_F(StoreMetricsReporterTest, MultiStoreMetrics) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {syncer::kEnablePasswordsAccountStorageForNonSyncingUsers,
       syncer::kEnablePasswordsAccountStorageForSyncingUsers},
      {});
#if BUILDFLAG(IS_ANDROID)
  prefs_.SetInteger(
      prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));
#endif  // BUILDFLAG(IS_ANDROID)

  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  auto account_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));

  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);
  account_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);

  // Simulate account store active.
  test_sync_service()->SetSignedIn(signin::ConsentLevel::kSignin);

  const std::string kRealm1 = "https://example.com";
  const std::string kRealm2 = "https://example2.com";

  // Add test data to the profile store:
  // - 3 credentials that don't exist in the account store
  // - 1 credential that conflicts with the account store (exists there with the
  //   same username but different password)
  // - 2 credentials with identical copies in the account store
  // Note: In the implementation, the credentials are processed in alphabetical
  // order of usernames. Choose usernames here so that some profile-store-only
  // credentials end up at both the start and the end of the list, to make sure
  // these cases are handled correctly.
  profile_store->AddLogin(
      CreateForm(kRealm1, "aprofileuser1", "aprofilepass1"));
  profile_store->AddLogin(
      CreateForm(kRealm1, "aprofileuser2", "aprofilepass2"));
  profile_store->AddLogin(
      CreateForm(kRealm1, "zprofileuser3", "zprofilepass3"));
  profile_store->AddLogin(CreateForm(kRealm1, "conflictinguser", "localpass"));
  profile_store->AddLogin(
      CreateForm(kRealm1, "identicaluser1", "identicalpass1"));
  profile_store->AddLogin(
      CreateForm(kRealm1, "identicaluser2", "identicalpass2"));

  // Add test data to the account store:
  // - 2 credentials that don't exist in the account store
  // - 1 credential that conflicts with the profile store (exists there with the
  //   same username but different password)
  // - 2 credentials with identical copies in the profile store
  account_store->AddLogin(CreateForm(kRealm1, "accountuser1", "accountpass1"));
  account_store->AddLogin(
      CreateForm(kRealm1, "zaccountuser2", "zaccountpass2"));
  account_store->AddLogin(
      CreateForm(kRealm1, "conflictinguser", "accountpass"));
  account_store->AddLogin(
      CreateForm(kRealm1, "identicaluser1", "identicalpass1"));
  account_store->AddLogin(
      CreateForm(kRealm1, "identicaluser2", "identicalpass2"));

  // Finally, add one more identical credential to the profile store. However
  // this one is on a different signon realm, so should be counted as just
  // another (4th) credential that's missing in the account store.
  profile_store->AddLogin(
      CreateForm(kRealm2, "identicaluser1", "identicalpass1"));

  for (bool syncing : {false, true}) {
    for (bool opted_in : {false, true}) {
      test_sync_service()->SetSignedIn(syncing ? signin::ConsentLevel::kSync
                                               : signin::ConsentLevel::kSignin);
      ASSERT_EQ(test_sync_service()->IsSyncFeatureEnabled(), syncing);
      if (opted_in) {
        test_sync_service()->GetUserSettings()->SetSelectedTypes(
            /*sync_everything=*/true, syncer::UserSelectableTypeSet::All());
      } else {
        test_sync_service()->GetUserSettings()->SetSelectedTypes(
            /*sync_everything=*/false, syncer::UserSelectableTypeSet());
      }
      ASSERT_EQ(features_util::IsOptedInForAccountStorage(pref_service(),
                                                          sync_service()),
                opted_in);

      // In every pass in the loop, StoreMetricsReporter uses the same pref
      // service. Set the kLastTimePasswordStoreMetricsReported to make sure
      // metrics will be reported in the second pass too.
      prefs_.SetDouble(
          password_manager::prefs::kLastTimePasswordStoreMetricsReported, 0.0);

      base::HistogramTester histogram_tester;

      StoreMetricsReporter reporter(
          profile_store.get(), account_store.get(), sync_service(), &prefs_,
          /*password_reuse_manager=*/nullptr, &settings_service(),
          /*done_callback*/ base::DoNothing());

      // Wait for the metrics to get reported, which involves queries to the
      // stores, i.e. to background task runners.
      RunUntilIdle();

      if (opted_in) {
        histogram_tester.ExpectUniqueSample(
            "PasswordManager.AccountStoreVsProfileStore4."
            "Additional",
            2, 1);
        histogram_tester.ExpectUniqueSample(
            "PasswordManager.AccountStoreVsProfileStore4."
            "Missing",
            4, 1);
        histogram_tester.ExpectUniqueSample(
            "PasswordManager.AccountStoreVsProfileStore4."
            "Identical",
            2, 1);
        histogram_tester.ExpectUniqueSample(
            "PasswordManager.AccountStoreVsProfileStore4."
            "Conflicting",
            1, 1);
      } else {
        histogram_tester.ExpectTotalCount(
            "PasswordManager.AccountStoreVsProfileStore4."
            "Additional",
            0);
        histogram_tester.ExpectTotalCount(
            "PasswordManager.AccountStoreVsProfileStore4."
            "Missing",
            0);
        histogram_tester.ExpectTotalCount(
            "PasswordManager.AccountStoreVsProfileStore4."
            "Identical",
            0);
        histogram_tester.ExpectTotalCount(
            "PasswordManager.AccountStoreVsProfileStore4."
            "Conflicting",
            0);
      }
    }
  }

  // In this test the account store contains 5 and the local store contains 7
  // credentials.
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForAccount), 5);
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForProfile), 7);

  account_store->ShutdownOnUIThread();
  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest, ReportMetricsForAdvancedProtection) {
  prefs_.registry()->RegisterListPref(prefs::kPasswordHashDataList,
                                      PrefRegistry::NO_REGISTRATION_FLAGS);
  auto store = base::MakeRefCounted<MockPasswordStoreInterface>();

  MockPasswordReuseManager reuse_manager;

  const std::string username = "test@google.com";
  SetSyncingPasswords(true);
  FakeSigninAs(username, signin::ConsentLevel::kSync);

  base::HistogramTester histogram_tester;

  EXPECT_CALL(reuse_manager, ReportMetrics(username));
  StoreMetricsReporter reporter(/*profile_store=*/store.get(),
                                /*account_store=*/nullptr, sync_service(),
                                &prefs_, &reuse_manager, &settings_service(),
                                /*done_callback*/ base::DoNothing());

  // In this test there are no saved credentials.
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForAccount), 0);
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForProfile), 0);

  // Wait for the metrics to get reported, which involves queries to the stores,
  // i.e. to background task runners.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest, ReportPasswordNoteMetrics) {
  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);

  PasswordForm password_form;
  password_form.url = GURL("http://example.com");
  password_form.username_value = u"test1@gmail.com";
  password_form.notes = {PasswordNote(u"note", base::Time::Now())};
  profile_store->AddLogin(password_form);
  // ProfileStore - CountCredentialsWithNonEmptyNotes2: 1

  password_form.username_value = u"test2@gmail.com";
  password_form.notes = {PasswordNote(u"another note", base::Time::Now()),
                         PasswordNote(std::u16string(), base::Time::Now())};
  profile_store->AddLogin(password_form);
  // ProfileStore - CountCredentialsWithNonEmptyNotes2: 2

  password_form.username_value = u"test3@gmail.com";
  password_form.notes = {PasswordNote(std::u16string(), base::Time::Now()),
                         PasswordNote(u"some note", base::Time::Now())};
  profile_store->AddLogin(password_form);
  // ProfileStore -  CountCredentialsWithNonEmptyNotes2: 3

  password_form.username_value = u"test4@gmail.com";
  password_form.notes = {PasswordNote(std::u16string(), base::Time::Now())};
  profile_store->AddLogin(password_form);
  // ProfileStore - CountCredentialsWithNonEmptyNotes2: 3

  password_form.username_value = u"test5@gmail.com";
  password_form.notes = {};
  profile_store->AddLogin(password_form);
  // ProfileStore - CountCredentialsWithNonEmptyNotes2: 3

  auto account_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  account_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);

  account_store->AddLogin(password_form);
  // AccountStore - CountCredentialsWithNonEmptyNotes2: 0

  password_form.username_value = u"test6@gmail.com";
  password_form.notes = {PasswordNote(std::u16string(), base::Time::Now())};
  account_store->AddLogin(password_form);
  // AccountStore - CountCredentialsWithNonEmptyNotes2: 0

  password_form.username_value = u"test7@gmail.com";
  password_form.notes = {PasswordNote(u"note", base::Time::Now())};
  account_store->AddLogin(password_form);
  // AccountStore - CountCredentialsWithNonEmptyNotes2: 1

  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(
      profile_store.get(), account_store.get(), sync_service(), &prefs_,
      /*password_reuse_manager=*/nullptr, &settings_service(),
      /*done_callback*/ base::DoNothing());

  RunUntilIdle();

  EXPECT_THAT(
      histogram_tester.GetAllSamples("PasswordManager.ProfileStore."
                                     "PasswordNotes.CountNotesPerCredential3"),
      BucketsAre(base::Bucket(0, 0), base::Bucket(1, 2), base::Bucket(2, 2)));
  histogram_tester.ExpectBucketCount(
      "PasswordManager.ProfileStore.PasswordNotes."
      "CountCredentialsWithNonEmptyNotes2",
      3, 1);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("PasswordManager.AccountStore."
                                     "PasswordNotes.CountNotesPerCredential3"),
      BucketsAre(base::Bucket(0, 0), base::Bucket(1, 2)));
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountStore.PasswordNotes."
      "CountCredentialsWithNonEmptyNotes2",
      1, 1);

  // In this test there are 5 local and 3 account credentials.
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForProfile), 5);
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForAccount), 3);

  account_store->ShutdownOnUIThread();
  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest, ReportPasswordInsecureCredentialMetrics) {
  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  profile_store->Init(&prefs_, /*affiliated_match_helper=*/nullptr);

  const std::string kRealm1 = "https://example.com";

  PasswordForm secure_password = CreateForm(kRealm1, "user", "pass");
  profile_store->AddLogin(secure_password);

  PasswordForm leaked_password = CreateForm(kRealm1, "user2", "pass");
  leaked_password.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});
  profile_store->AddLogin(leaked_password);

  PasswordForm phished_and_leaked_password =
      CreateForm(kRealm1, "user3", "pass");
  phished_and_leaked_password.password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});
  phished_and_leaked_password.password_issues.insert(
      {InsecureType::kPhished, InsecurityMetadata()});
  profile_store->AddLogin(phished_and_leaked_password);

  base::HistogramTester histogram_tester;
  StoreMetricsReporter reporter(
      profile_store.get(), /*account_store=*/nullptr, sync_service(), &prefs_,
      /*password_reuse_manager=*/nullptr, &settings_service(),
      /*done_callback*/ base::DoNothing());

  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.CompromisedCredentials3.CountPhished", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.CompromisedCredentials3.CountLeaked", 2, 1);

  // In this test only the profile store contains credentials to be counted.
  EXPECT_EQ(
      pref_service()->GetInteger(prefs::kTotalPasswordsAvailableForProfile), 3);

  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

}  // namespace
}  // namespace password_manager
