// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/unified_consent_service.h"

#include <map>
#include <memory>
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_prefs.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unified_consent {

class UnifiedConsentServiceTest : public testing::Test {
 public:
  using SyncState = UnifiedConsentService::SyncState;

  static bool ShouldEnableUrlKeyedAnonymizedDataCollection(
      SyncState old_sync_state,
      SyncState new_sync_state) {
    return UnifiedConsentService::ShouldEnableUrlKeyedAnonymizedDataCollection(
        old_sync_state, new_sync_state);
  }
  static bool ShouldDisableUrlKeyedAnonymizedDataCollection(
      SyncState old_sync_state,
      SyncState new_sync_state) {
    return UnifiedConsentService::ShouldDisableUrlKeyedAnonymizedDataCollection(
        old_sync_state, new_sync_state);
  }
  static const char* SyncStateToString(SyncState state) {
    switch (state) {
      case SyncState::kSignedOut:
        return "kSignedOut";
      case SyncState::kSignedInWithoutHistory:
        return "kSignedInWithoutHistory";
      case SyncState::kSignedInWithHistoryWaitingForPassphrase:
        return "kSignedInWithHistoryWaitingForPassphrase";
      case SyncState::kSignedInWithHistoryAndExplicitPassphrase:
        return "kSignedInWithHistoryAndExplicitPassphrase";
      case SyncState::kSignedInWithHistoryAndNoPassphrase:
        return "kSignedInWithHistoryAndNoPassphrase";
    }
    NOTREACHED();
  }

  UnifiedConsentServiceTest() {
    UnifiedConsentService::RegisterPrefs(pref_service_.registry());
    syncer::SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
    // `identity_test_environment_` starts signed-out while `sync_service_`
    // starts signed-in, make them consistent.
    sync_service_.SetSignedOut();
  }

  UnifiedConsentServiceTest(const UnifiedConsentServiceTest&) = delete;
  UnifiedConsentServiceTest& operator=(const UnifiedConsentServiceTest&) =
      delete;

  ~UnifiedConsentServiceTest() override {
    if (consent_service_)
      consent_service_->Shutdown();
  }

  void CreateConsentService() {
    consent_service_ = std::make_unique<UnifiedConsentService>(
        &pref_service_, identity_test_environment_.identity_manager(),
        &sync_service_, std::vector<std::string>());

    sync_service_.FireStateChanged();
    // Run until idle so the migration can finish.
    base::RunLoop().RunUntilIdle();
  }

  void ResetConsentService() {
    if (consent_service_) {
      consent_service_->Shutdown();
      consent_service_.reset();
    }
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  unified_consent::MigrationState GetMigrationState() {
    int migration_state_int =
        pref_service_.GetInteger(prefs::kUnifiedConsentMigrationState);
    return static_cast<unified_consent::MigrationState>(migration_state_int);
  }
#endif

  void SignIn(signin::ConsentLevel consent_level) {
    CoreAccountInfo account_info = identity_test_environment_.SetPrimaryAccount(
        "testaccount@gmail.com", consent_level);
    sync_service_.SetSignedIn(consent_level, account_info);
  }

  void SignOut() {
    identity_test_environment_.ClearPrimaryAccount();
    sync_service_.SetSignedOut();
  }

  void SetHistorySyncEnabled(bool enabled) {
    sync_service_.GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kHistory, enabled);
  }

  void FireSyncStateChanged() { sync_service_.FireStateChanged(); }

  PrefService* pref_service() { return &pref_service_; }
  UnifiedConsentService* consent_service() { return consent_service_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  // identity_test_environment_ and sync_service_ must stay private and be
  // controlled together by SignIn() and SignOut().
  signin::IdentityTestEnvironment identity_test_environment_;
  syncer::TestSyncService sync_service_;
  std::unique_ptr<UnifiedConsentService> consent_service_;
};

TEST_F(UnifiedConsentServiceTest, DefaultValuesWhenSignedOut) {
  CreateConsentService();
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
}

TEST_F(UnifiedConsentServiceTest, EnableUrlKeyedAnonymizedDataCollection) {
  CreateConsentService();
  SignIn(signin::ConsentLevel::kSync);
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));

  // Enable services and check expectations.
  consent_service()->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
  EXPECT_TRUE(pref_service()->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
}

// Tests that in all cases, initializing the UnifiedConsentService does not
// affect the UrlKeyedAnonymizedDataCollectionEnabled state.
TEST_F(UnifiedConsentServiceTest,
       ReplaceSync_InitializeNoChangeToUrlKeyedAnonymizedDataCollection) {
  base::test::ScopedFeatureList scoped_feature_list(
      syncer::kReplaceSyncPromosWithSignInPromos);

  CreateConsentService();
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  ResetConsentService();

  SignIn(signin::ConsentLevel::kSignin);
  CreateConsentService();
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  ResetConsentService();

  SetHistorySyncEnabled(true);
  CreateConsentService();
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  ResetConsentService();

  SignIn(signin::ConsentLevel::kSync);
  CreateConsentService();
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
}

// Tests that `kUrlKeyedAnonymizedDataCollectionEnabled` does not change for
// sync users when history sync opt-in state changes.
TEST_F(UnifiedConsentServiceTest, ReplaceSync_HistorySyncIgnoredForSyncUsers) {
  SignIn(signin::ConsentLevel::kSync);
  CreateConsentService();
  consent_service()->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
  ASSERT_TRUE(pref_service()->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  ResetConsentService();

  base::test::ScopedFeatureList scoped_feature_list(
      syncer::kReplaceSyncPromosWithSignInPromos);
  CreateConsentService();
  ASSERT_TRUE(pref_service()->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  SetHistorySyncEnabled(false);
  FireSyncStateChanged();
  EXPECT_TRUE(pref_service()->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Tests that kUrlKeyedAnonymizedDataCollectionEnabled is enabled after
// syncing user signs out, then in again and enabled history sync opt-in.
TEST_F(UnifiedConsentServiceTest,
       ReplaceSync_SyncUserMovesToSigninWithHistorySyncEnabled) {
  SignIn(signin::ConsentLevel::kSync);
  CreateConsentService();
  consent_service()->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
  ASSERT_TRUE(pref_service()->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  ResetConsentService();

  // Clearing the primary account and resetting history sync should disable
  // `kUrlKeyedAnonymizedDataCollectionEnabled`.
  base::test::ScopedFeatureList scoped_feature_list(
      syncer::kReplaceSyncPromosWithSignInPromos);
  CreateConsentService();
  ASSERT_TRUE(pref_service()->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  SignOut();
  FireSyncStateChanged();
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));

  // Enabling history sync should enable
  // `kUrlKeyedAnonymizedDataCollectionEnabled`.
  SignIn(signin::ConsentLevel::kSignin);
  SetHistorySyncEnabled(true);
  FireSyncStateChanged();
  EXPECT_TRUE(pref_service()->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));

  // Disabling history sync should disable
  // `kUrlKeyedAnonymizedDataCollectionEnabled`.
  SetHistorySyncEnabled(false);
  FireSyncStateChanged();
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Tests that any change to history sync opt-in, is reflected in the state
// of `kUrlKeyedAnonymizedDataCollectionEnabled`.
TEST_F(UnifiedConsentServiceTest,
       ReplaceSync_HistorySyncEnablesUrlKeyedAnonymizedDataCollection) {
  base::test::ScopedFeatureList scoped_feature_list(
      syncer::kReplaceSyncPromosWithSignInPromos);

  SignIn(signin::ConsentLevel::kSignin);
  SetHistorySyncEnabled(false);

  CreateConsentService();
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));

  SetHistorySyncEnabled(true);
  FireSyncStateChanged();
  EXPECT_TRUE(pref_service()->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));

  SetHistorySyncEnabled(false);
  FireSyncStateChanged();
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
}

// This low-level unit test explicitly checks all state transitions. Most
// practical cases are (also) covered by browser tests, see
// UnifiedConsentSyncToSigninBrowserTest.
TEST_F(UnifiedConsentServiceTest, ReplaceSync_StateTransitions) {
  base::test::ScopedFeatureList scoped_feature_list(
      syncer::kReplaceSyncPromosWithSignInPromos);

  enum Outcome {
    kNoChange,
    kEnable,
    kDisable,
  };

  struct TestCase {
    const char* description;
    SyncState old_state;
    SyncState new_state;
    Outcome expected_outcome;
  } test_cases[] = {
      // From kSignedOut:
      {"No-op", SyncState::kSignedOut, SyncState::kSignedOut, kNoChange},
      {"Sign-in without history", SyncState::kSignedOut,
       SyncState::kSignedInWithoutHistory, kNoChange},
      {"Sign-in with history", SyncState::kSignedOut,
       SyncState::kSignedInWithHistoryWaitingForPassphrase, kEnable},
      {"Sign-in with passphrase", SyncState::kSignedOut,
       SyncState::kSignedInWithHistoryAndExplicitPassphrase, kNoChange},
      {"Sign-in with history", SyncState::kSignedOut,
       SyncState::kSignedInWithHistoryAndNoPassphrase, kEnable},
      // From kSignedInWithoutHistory:
      {"Sign-out without history", SyncState::kSignedInWithoutHistory,
       SyncState::kSignedOut, kNoChange},
      {"No-op", SyncState::kSignedInWithoutHistory,
       SyncState::kSignedInWithoutHistory, kNoChange},
      {"History opt-in", SyncState::kSignedInWithoutHistory,
       SyncState::kSignedInWithHistoryWaitingForPassphrase, kEnable},
      {"History opt-in but also passphrase", SyncState::kSignedInWithoutHistory,
       SyncState::kSignedInWithHistoryAndExplicitPassphrase, kNoChange},
      {"History opt-in", SyncState::kSignedInWithoutHistory,
       SyncState::kSignedInWithHistoryAndNoPassphrase, kEnable},
      // From kSignedInWithHistoryWaitingForPassphrase:
      {"Sign-out with history",
       SyncState::kSignedInWithHistoryWaitingForPassphrase,
       SyncState::kSignedOut, kDisable},
      {"History opt-out", SyncState::kSignedInWithHistoryWaitingForPassphrase,
       SyncState::kSignedInWithoutHistory, kDisable},
      {"No-op", SyncState::kSignedInWithHistoryWaitingForPassphrase,
       SyncState::kSignedInWithHistoryWaitingForPassphrase, kNoChange},
      {"Explicit passphrase determined",
       SyncState::kSignedInWithHistoryWaitingForPassphrase,
       SyncState::kSignedInWithHistoryAndExplicitPassphrase, kDisable},
      {"No passphrase determined",
       SyncState::kSignedInWithHistoryWaitingForPassphrase,
       SyncState::kSignedInWithHistoryAndNoPassphrase, kNoChange},
      // From kSignedInWithHistoryAndExplicitPassphrase:
      {"Sign-out with passphrase",
       SyncState::kSignedInWithHistoryAndExplicitPassphrase,
       SyncState::kSignedOut, kNoChange},
      {"History opt-out with passphrase",
       SyncState::kSignedInWithHistoryAndExplicitPassphrase,
       SyncState::kSignedInWithoutHistory, kNoChange},
      {"Passphrase became unknown (e.g. dashboard reset)",
       SyncState::kSignedInWithHistoryAndExplicitPassphrase,
       SyncState::kSignedInWithHistoryWaitingForPassphrase, kEnable},
      {"No-op", SyncState::kSignedInWithHistoryAndExplicitPassphrase,
       SyncState::kSignedInWithHistoryAndExplicitPassphrase, kNoChange},
      {"Passphrase turned off (e.g. dashboard reset)",
       SyncState::kSignedInWithHistoryAndExplicitPassphrase,
       SyncState::kSignedInWithHistoryAndNoPassphrase, kEnable},
      // From kSignedInWithHistoryAndNoPassphrase:
      {"Sign-out with history", SyncState::kSignedInWithHistoryAndNoPassphrase,
       SyncState::kSignedOut, kDisable},
      {"History opt-out", SyncState::kSignedInWithHistoryAndNoPassphrase,
       SyncState::kSignedInWithoutHistory, kDisable},
      {"Passphrase became unknown (e.g. dashboard reset)",
       SyncState::kSignedInWithHistoryAndNoPassphrase,
       SyncState::kSignedInWithHistoryWaitingForPassphrase, kNoChange},
      {"Passphrase turned on", SyncState::kSignedInWithHistoryAndNoPassphrase,
       SyncState::kSignedInWithHistoryAndExplicitPassphrase, kDisable},
      {"No-op", SyncState::kSignedInWithHistoryAndNoPassphrase,
       SyncState::kSignedInWithHistoryAndNoPassphrase, kNoChange},
  };

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf(
        "Test case: %s -> %s, %s", SyncStateToString(test_case.old_state),
        SyncStateToString(test_case.new_state), test_case.description));
    EXPECT_EQ(ShouldEnableUrlKeyedAnonymizedDataCollection(test_case.old_state,
                                                           test_case.new_state),
              test_case.expected_outcome == kEnable);
    EXPECT_EQ(ShouldDisableUrlKeyedAnonymizedDataCollection(
                  test_case.old_state, test_case.new_state),
              test_case.expected_outcome == kDisable);
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(UnifiedConsentServiceTest, Migration_UpdateSettings) {
  // Create user that syncs history and has no custom passphrase.
  SignIn(signin::ConsentLevel::kSync);
  // Url keyed data collection is off before the migration.
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));

  CreateConsentService();
  EXPECT_EQ(GetMigrationState(), unified_consent::MigrationState::kCompleted);
  // During the migration Url keyed data collection is enabled.
  EXPECT_TRUE(pref_service()->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
}

TEST_F(UnifiedConsentServiceTest, Migration_NotSignedIn) {
  base::HistogramTester histogram_tester;

  CreateConsentService();
  // The user is signed out, so the migration is completed after the
  // creation of the consent service.
  EXPECT_EQ(GetMigrationState(), unified_consent::MigrationState::kCompleted);
}
#else
TEST_F(UnifiedConsentServiceTest, ClearPrimaryAccountDisablesSomeServices) {
  base::HistogramTester histogram_tester;

  CreateConsentService();
  SignIn(signin::ConsentLevel::kSync);

  // Precondition: Enable unified consent.
  consent_service()->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
  EXPECT_TRUE(pref_service()->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));

  // Clearing primary account revokes unfied consent and a couple of other
  // non-personalized services.
  SignOut();
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace unified_consent
