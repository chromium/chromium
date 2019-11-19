// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/store_metrics_reporter.h"

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/sync_username_test_base.h"
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

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_CONST_METHOD0(GetProfilePasswordStore, PasswordStore*());
  MOCK_CONST_METHOD0(GetPasswordSyncState, SyncState());
  MOCK_CONST_METHOD0(IsUnderAdvancedProtection, bool());
};

// The test fixture defines two tests, one that doesn't require a password store
// and one that does. Each of these tests depend on two boolean parameters,
// which are declared here. Each test then assigns the desired semantics to
// them.
class StoreMetricsReporterTest
    : public SyncUsernameTestBase,
      public ::testing::WithParamInterface<std::tuple<bool, bool, int>> {
 public:
  StoreMetricsReporterTest() {
    prefs_.registry()->RegisterBooleanPref(prefs::kCredentialsEnableService,
                                           false);
    prefs_.registry()->RegisterBooleanPref(prefs::kPasswordLeakDetectionEnabled,
                                           false);
    prefs_.registry()->RegisterBooleanPref(
        password_manager::prefs::kWasAutoSignInFirstRunExperienceShown, false);
    prefs_.registry()->RegisterIntegerPref(
        password_manager::prefs::kPasswordManagerOnboardingState,
        static_cast<int>(
            password_manager::metrics_util::OnboardingState::kDoNotShow));
  }

  ~StoreMetricsReporterTest() override = default;

 protected:
  MockPasswordManagerClient client_;
  TestingPrefServiceSimple prefs_;
  DISALLOW_COPY_AND_ASSIGN(StoreMetricsReporterTest);
};

// Test that store-independent metrics are reported correctly.
TEST_P(StoreMetricsReporterTest, StoreIndependentMetrics) {
  const bool password_manager_enabled = std::get<0>(GetParam());
  const bool leak_detection_enabled = std::get<1>(GetParam());
  const int onboarding_state = std::get<2>(GetParam());

  prefs_.SetBoolean(password_manager::prefs::kCredentialsEnableService,
                    password_manager_enabled);
  prefs_.SetBoolean(password_manager::prefs::kPasswordLeakDetectionEnabled,
                    leak_detection_enabled);
  prefs_.SetInteger(password_manager::prefs::kPasswordManagerOnboardingState,
                    onboarding_state);
  base::HistogramTester histogram_tester;
  EXPECT_CALL(client_, GetProfilePasswordStore()).WillOnce(Return(nullptr));
  StoreMetricsReporter reporter(&client_, sync_service(), identity_manager(),
                                &prefs_);

  histogram_tester.ExpectUniqueSample("PasswordManager.Enabled",
                                      password_manager_enabled, 1);
  histogram_tester.ExpectUniqueSample("PasswordManager.LeakDetection.Enabled",
                                      leak_detection_enabled, 1);
  histogram_tester.ExpectUniqueSample("PasswordManager.Onboarding.State",
                                      onboarding_state, 1);
}

// Test that sync username and syncing state are passed correctly to the
// PasswordStore.
TEST_P(StoreMetricsReporterTest, StoreDependentMetrics) {
  const bool syncing_with_passphrase = std::get<0>(GetParam());
  const bool is_under_advanced_protection = std::get<1>(GetParam());

  auto store = base::MakeRefCounted<MockPasswordStore>();
  const auto sync_state = syncing_with_passphrase
                              ? password_manager::SYNCING_WITH_CUSTOM_PASSPHRASE
                              : password_manager::SYNCING_NORMAL_ENCRYPTION;
  EXPECT_CALL(client_, GetPasswordSyncState()).WillOnce(Return(sync_state));
  EXPECT_CALL(client_, GetProfilePasswordStore()).WillOnce(Return(store.get()));
  EXPECT_CALL(client_, IsUnderAdvancedProtection())
      .WillOnce(Return(is_under_advanced_protection));
  EXPECT_CALL(*store,
              ReportMetrics("some.user@gmail.com", syncing_with_passphrase,
                            is_under_advanced_protection));
  FakeSigninAs("some.user@gmail.com");

  StoreMetricsReporter reporter(&client_, sync_service(), identity_manager(),
                                &prefs_);
  store->ShutdownOnUIThread();
}

INSTANTIATE_TEST_SUITE_P(
    /*InstantiationName*/,
    StoreMetricsReporterTest,
    testing::Combine(
        Bool(),
        Bool(),
        Range(0,
              static_cast<int>(
                  password_manager::metrics_util::OnboardingState::kMaxValue) +
                  1)));

}  // namespace
}  // namespace password_manager
