// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/store_metrics_reporter.h"

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/password_manager/core/browser/mock_password_reuse_manager.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/sync_username_test_base.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
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

class StoreMetricsReporterTest : public SyncUsernameTestBase {
 public:
  StoreMetricsReporterTest() = default;

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
  }

  void TearDown() override { OSCryptMocker::TearDown(); }

  ~StoreMetricsReporterTest() override = default;

 protected:
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple prefs_;
};

// The test fixture defines two tests, one that doesn't require a password store
// and one that does. Each of these tests depend on two boolean parameters,
// which are declared here. Each test then assigns the desired semantics to
// them.
class StoreMetricsReporterTestWithParams
    : public StoreMetricsReporterTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {};

// Test that store-independent metrics are reported correctly.
TEST_P(StoreMetricsReporterTestWithParams, StoreIndependentMetrics) {
  const bool password_manager_enabled = std::get<0>(GetParam());

  prefs_.SetBoolean(password_manager::prefs::kCredentialsEnableService,
                    password_manager_enabled);
  base::HistogramTester histogram_tester;

  StoreMetricsReporter reporter(
      /*profile_store=*/nullptr, /*account_store=*/nullptr, sync_service(),
      identity_manager(), &prefs_, /*password_reuse_manager=*/nullptr,
      /*is_under_advanced_protection=*/false);

  histogram_tester.ExpectUniqueSample("PasswordManager.Enabled",
                                      password_manager_enabled, 1);
}

// Test that sync username and syncing state are passed correctly to the
// PasswordStore.
TEST_P(StoreMetricsReporterTestWithParams, StoreDependentMetrics) {
  const bool syncing_with_passphrase = std::get<0>(GetParam());
  const bool is_under_advanced_protection = std::get<1>(GetParam());

  test_sync_service()->SetIsUsingExplicitPassphrase(syncing_with_passphrase);

  auto store = base::MakeRefCounted<MockPasswordStore>();
  EXPECT_CALL(*store,
              ReportMetrics("some.user@gmail.com", syncing_with_passphrase,
                            is_under_advanced_protection));

  FakeSigninAs("some.user@gmail.com");

  StoreMetricsReporter reporter(
      /*profile_store=*/store.get(), /*account_store=*/nullptr, sync_service(),
      identity_manager(), &prefs_, /*password_reuse_manager=*/nullptr,
      is_under_advanced_protection);

  store->ShutdownOnUIThread();
}

// A test that covers multi-store metrics, which are recorded by the
// StoreMetricsReporter directly.
TEST_F(StoreMetricsReporterTest, MultiStoreMetrics) {
  // This test is only relevant when the passwords accounts store is enabled.
  if (!base::FeatureList::IsEnabled(features::kEnablePasswordsAccountStorage))
    return;
  prefs_.registry()->RegisterDictionaryPref(
      prefs::kAccountStoragePerAccountSettings);
  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  auto account_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));

  profile_store->Init(&prefs_);
  account_store->Init(&prefs_);

  // Simulate account store active.
  AccountInfo account_info;
  account_info.email = "account@gmail.com";
  account_info.gaia = "account";
  test_sync_service()->SetAuthenticatedAccountInfo(account_info);
  test_sync_service()->SetIsAuthenticatedAccountPrimary(false);

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

  for (bool opted_in : {false, true}) {
    if (opted_in) {
      features_util::OptInToAccountStorage(&prefs_, sync_service());
    } else {
      features_util::OptOutOfAccountStorageAndClearSettings(&prefs_,
                                                            sync_service());
    }

    base::HistogramTester histogram_tester;

    StoreMetricsReporter reporter(profile_store.get(), account_store.get(),
                                  sync_service(), identity_manager(), &prefs_,
                                  /*password_reuse_manager=*/nullptr,
                                  /*is_under_advanced_protection=*/false);

    // Wait for the metrics to get reported, which involves queries to the
    // stores, i.e. to background task runners.
    RunUntilIdle();

    // The original version of the metrics (without "2") is still recorded, even
    // if the user isn't opted in to the account storage.
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.AccountStoreVsProfileStore.Additional", 2, 1);
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.AccountStoreVsProfileStore.Missing", 4, 1);
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.AccountStoreVsProfileStore.Identical", 2, 1);
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.AccountStoreVsProfileStore.Conflicting", 1, 1);

    // Version "2" of the metrics is only recorded if the user is opted in.
    if (opted_in) {
      histogram_tester.ExpectUniqueSample(
          "PasswordManager.AccountStoreVsProfileStore2.Additional", 2, 1);
      histogram_tester.ExpectUniqueSample(
          "PasswordManager.AccountStoreVsProfileStore2.Missing", 4, 1);
      histogram_tester.ExpectUniqueSample(
          "PasswordManager.AccountStoreVsProfileStore2.Identical", 2, 1);
      histogram_tester.ExpectUniqueSample(
          "PasswordManager.AccountStoreVsProfileStore2.Conflicting", 1, 1);
    } else {
      histogram_tester.ExpectTotalCount(
          "PasswordManager.AccountStoreVsProfileStore2.Additional", 0);
      histogram_tester.ExpectTotalCount(
          "PasswordManager.AccountStoreVsProfileStore2.Missing", 0);
      histogram_tester.ExpectTotalCount(
          "PasswordManager.AccountStoreVsProfileStore2.Identical", 0);
      histogram_tester.ExpectTotalCount(
          "PasswordManager.AccountStoreVsProfileStore2.Conflicting", 0);
    }
  }

  account_store->ShutdownOnUIThread();
  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

TEST_F(StoreMetricsReporterTest, ReportMetricsForAdvancedProtection) {
  prefs_.registry()->RegisterListPref(prefs::kPasswordHashDataList,
                                      PrefRegistry::NO_REGISTRATION_FLAGS);
  ASSERT_FALSE(prefs_.HasPrefPath(prefs::kSyncPasswordHash));

  auto store = base::MakeRefCounted<MockPasswordStore>();
  store->Init(nullptr);

  MockPasswordReuseManager reuse_manager;

  const std::string username = "test@google.com";
  SetSyncingPasswords(true);
  FakeSigninAs(username);

  base::HistogramTester histogram_tester;

  EXPECT_CALL(reuse_manager, ReportMetrics(username, true));
  StoreMetricsReporter reporter(/*profile_store=*/store.get(),
                                /*account_store=*/nullptr, sync_service(),
                                identity_manager(), &prefs_, &reuse_manager,
                                /*is_under_advanced_protection=*/true);

  // Wait for the metrics to get reported, which involves queries to the stores,
  // i.e. to background task runners.
  RunUntilIdle();

  store->ShutdownOnUIThread();
}

INSTANTIATE_TEST_SUITE_P(All,
                         StoreMetricsReporterTestWithParams,
                         testing::Combine(Bool(), Bool()));
}  // namespace
}  // namespace password_manager
