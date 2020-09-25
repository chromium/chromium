// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/store_metrics_reporter.h"

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/sync_username_test_base.h"
#include "components/password_manager/core/browser/test_password_store.h"
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

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_CONST_METHOD0(GetProfilePasswordStore, PasswordStore*());
  MOCK_CONST_METHOD0(GetAccountPasswordStore, PasswordStore*());
  MOCK_CONST_METHOD0(GetPasswordSyncState, SyncState());
  MOCK_CONST_METHOD0(IsUnderAdvancedProtection, bool());
};

class StoreMetricsReporterTest : public SyncUsernameTestBase {
 public:
  StoreMetricsReporterTest() {
    prefs_.registry()->RegisterBooleanPref(prefs::kCredentialsEnableService,
                                           false);
    prefs_.registry()->RegisterBooleanPref(prefs::kPasswordLeakDetectionEnabled,
                                           false);
    prefs_.registry()->RegisterBooleanPref(
        password_manager::prefs::kWasAutoSignInFirstRunExperienceShown, false);
  }

  ~StoreMetricsReporterTest() override = default;

 protected:
  MockPasswordManagerClient client_;
  TestingPrefServiceSimple prefs_;
  DISALLOW_COPY_AND_ASSIGN(StoreMetricsReporterTest);
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
  const bool leak_detection_enabled = std::get<1>(GetParam());

  prefs_.SetBoolean(password_manager::prefs::kCredentialsEnableService,
                    password_manager_enabled);
  prefs_.SetBoolean(password_manager::prefs::kPasswordLeakDetectionEnabled,
                    leak_detection_enabled);
  base::HistogramTester histogram_tester;
  EXPECT_CALL(client_, GetProfilePasswordStore())
      .WillRepeatedly(Return(nullptr));
  StoreMetricsReporter reporter(&client_, sync_service(), identity_manager(),
                                &prefs_);

  histogram_tester.ExpectUniqueSample("PasswordManager.Enabled",
                                      password_manager_enabled, 1);
  histogram_tester.ExpectUniqueSample("PasswordManager.LeakDetection.Enabled",
                                      leak_detection_enabled, 1);
}

// Test that sync username and syncing state are passed correctly to the
// PasswordStore.
TEST_P(StoreMetricsReporterTestWithParams, StoreDependentMetrics) {
  const bool syncing_with_passphrase = std::get<0>(GetParam());
  const bool is_under_advanced_protection = std::get<1>(GetParam());

  auto store = base::MakeRefCounted<MockPasswordStore>();
  const auto sync_state = syncing_with_passphrase
                              ? password_manager::SYNCING_WITH_CUSTOM_PASSPHRASE
                              : password_manager::SYNCING_NORMAL_ENCRYPTION;
  EXPECT_CALL(client_, GetPasswordSyncState())
      .WillRepeatedly(Return(sync_state));
  EXPECT_CALL(client_, GetProfilePasswordStore())
      .WillRepeatedly(Return(store.get()));
  EXPECT_CALL(client_, IsUnderAdvancedProtection())
      .WillRepeatedly(Return(is_under_advanced_protection));
  EXPECT_CALL(*store,
              ReportMetrics("some.user@gmail.com", syncing_with_passphrase,
                            is_under_advanced_protection));
  FakeSigninAs("some.user@gmail.com");

  StoreMetricsReporter reporter(&client_, sync_service(), identity_manager(),
                                &prefs_);
  store->ShutdownOnUIThread();
}

// A test that covers multi-store metrics, which are recorded by the
// StoreMetricsReporter directly.
TEST_F(StoreMetricsReporterTest, MultiStoreMetrics) {
  auto profile_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  auto account_store =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  profile_store->Init(&prefs_);
  account_store->Init(&prefs_);

  EXPECT_CALL(client_, GetPasswordSyncState())
      .WillRepeatedly(
          Return(password_manager::ACCOUNT_PASSWORDS_ACTIVE_NORMAL_ENCRYPTION));
  EXPECT_CALL(client_, IsUnderAdvancedProtection())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(client_, GetProfilePasswordStore())
      .WillRepeatedly(Return(profile_store.get()));
  EXPECT_CALL(client_, GetAccountPasswordStore())
      .WillRepeatedly(Return(account_store.get()));

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

  base::HistogramTester histogram_tester;

  StoreMetricsReporter reporter(&client_, sync_service(), identity_manager(),
                                &prefs_);
  // Wait for the metrics to get reported. This is delayed by 30 seconds, and
  // then involves queries to the stores, i.e. to background task runners.
  FastForwardBy(base::TimeDelta::FromSeconds(30));
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStoreVsProfileStore.Additional", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStoreVsProfileStore.Missing", 4, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStoreVsProfileStore.Identical", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStoreVsProfileStore.Conflicting", 1, 1);

  account_store->ShutdownOnUIThread();
  profile_store->ShutdownOnUIThread();
  // Make sure the PasswordStore destruction parts on the background sequence
  // finish, otherwise we get memory leak reports.
  RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(All,
                         StoreMetricsReporterTestWithParams,
                         testing::Combine(Bool(), Bool()));
}  // namespace
}  // namespace password_manager
