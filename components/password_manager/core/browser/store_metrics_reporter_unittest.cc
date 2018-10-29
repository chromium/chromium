// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/store_metrics_reporter.h"

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/sync_username_test_base.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace password_manager {
namespace {

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_CONST_METHOD0(GetPasswordStore, PasswordStore*());
  MOCK_CONST_METHOD0(GetPasswordSyncState, SyncState());
  MOCK_CONST_METHOD0(IsUnderAdvancedProtection, bool());
};

class StoreMetricsReporterTest : public SyncUsernameTestBase {
 public:
  StoreMetricsReporterTest() {
    prefs_.registry()->RegisterBooleanPref(
        password_manager::prefs::kWasAutoSignInFirstRunExperienceShown, false,
        PrefRegistry::NO_REGISTRATION_FLAGS);
  }

  ~StoreMetricsReporterTest() override = default;

 protected:
  MockPasswordManagerClient client_;
  TestingPrefServiceSimple prefs_;
  DISALLOW_COPY_AND_ASSIGN(StoreMetricsReporterTest);
};

// Test that store-independent metrics are reported correctly.
TEST_F(StoreMetricsReporterTest, StoreIndependentMetrics) {
  for (const bool password_manager_enabled : {true, false}) {
    for (const bool first_run_ui_shown : {true, false}) {
      SCOPED_TRACE(testing::Message()
                   << "password_manager_enabled=" << password_manager_enabled
                   << ", first_run_ui_shown=" << first_run_ui_shown);

      prefs_.SetBoolean(
          password_manager::prefs::kWasAutoSignInFirstRunExperienceShown,
          first_run_ui_shown);
      base::HistogramTester histogram_tester;
      EXPECT_CALL(client_, GetPasswordStore()).WillOnce(Return(nullptr));
      StoreMetricsReporter reporter(password_manager_enabled, &client_,
                                    sync_service(), signin_manager(), &prefs_);

      histogram_tester.ExpectBucketCount("PasswordManager.Enabled",
                                         password_manager_enabled, 1);
      histogram_tester.ExpectBucketCount(
          "PasswordManager.ShouldShowAutoSignInFirstRunExperience",
          !first_run_ui_shown, 1);
    }
  }
}

// Test that sync username and syncing state are passed correctly to the
// PasswordStore when not under advanced protection.
TEST_F(StoreMetricsReporterTest, PasswordStore) {
  for (const bool syncing_with_passphrase : {true, false}) {
    SCOPED_TRACE(testing::Message()
                 << "syncing_with_passphrase=" << syncing_with_passphrase);

    auto store = base::MakeRefCounted<MockPasswordStore>();
    const auto sync_state =
        syncing_with_passphrase
            ? password_manager::SYNCING_WITH_CUSTOM_PASSPHRASE
            : password_manager::SYNCING_NORMAL_ENCRYPTION;
    EXPECT_CALL(client_, GetPasswordSyncState()).WillOnce(Return(sync_state));
    EXPECT_CALL(client_, GetPasswordStore()).WillOnce(Return(store.get()));
    EXPECT_CALL(client_, IsUnderAdvancedProtection()).WillOnce(Return(false));
    EXPECT_CALL(*store, ReportMetrics("some.user@gmail.com",
                                      syncing_with_passphrase, false));
    FakeSigninAs("some.user@gmail.com");

    StoreMetricsReporter reporter(true, &client_, sync_service(),
                                  signin_manager(), &prefs_);
    store->ShutdownOnUIThread();
  }
}

// Test that sync username and syncing state are passed correctly to the
// PasswordStore when under advanced protection.
TEST_F(StoreMetricsReporterTest, PasswordStoreForUnderAdvancedProtection) {
  for (const bool syncing_with_passphrase : {true, false}) {
    SCOPED_TRACE(testing::Message()
                 << "syncing_with_passphrase=" << syncing_with_passphrase);

    auto store = base::MakeRefCounted<MockPasswordStore>();
    const auto sync_state =
        syncing_with_passphrase
            ? password_manager::SYNCING_WITH_CUSTOM_PASSPHRASE
            : password_manager::SYNCING_NORMAL_ENCRYPTION;
    EXPECT_CALL(client_, GetPasswordSyncState()).WillOnce(Return(sync_state));
    EXPECT_CALL(client_, GetPasswordStore()).WillOnce(Return(store.get()));
    EXPECT_CALL(client_, IsUnderAdvancedProtection()).WillOnce(Return(true));
    EXPECT_CALL(*store, ReportMetrics("some.user@gmail.com",
                                      syncing_with_passphrase, true));
    FakeSigninAs("some.user@gmail.com");

    StoreMetricsReporter reporter(true, &client_, sync_service(),
                                  signin_manager(), &prefs_);
    store->ShutdownOnUIThread();
  }
}

}  // namespace
}  // namespace password_manager
