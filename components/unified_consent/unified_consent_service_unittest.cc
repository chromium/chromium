// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/unified_consent_service.h"

#include <map>
#include <memory>
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unified_consent {
namespace {

class TestSyncService : public syncer::TestSyncService {
 public:
  TestSyncService() = default;

  TestSyncService(const TestSyncService&) = delete;
  TestSyncService& operator=(const TestSyncService&) = delete;

  void AddObserver(syncer::SyncServiceObserver* observer) override {
    observer_ = observer;
  }

  void FireStateChanged() {
    if (observer_)
      observer_->OnStateChanged(this);
  }

 private:
  raw_ptr<syncer::SyncServiceObserver, DanglingUntriaged> observer_ = nullptr;
};

}  // namespace

class UnifiedConsentServiceTest : public testing::Test {
 public:
  UnifiedConsentServiceTest() {
    UnifiedConsentService::RegisterPrefs(pref_service_.registry());
    syncer::SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
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

  unified_consent::MigrationState GetMigrationState() {
    int migration_state_int =
        pref_service_.GetInteger(prefs::kUnifiedConsentMigrationState);
    return static_cast<unified_consent::MigrationState>(migration_state_int);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  signin::IdentityTestEnvironment identity_test_environment_;
  TestSyncService sync_service_;
  std::unique_ptr<UnifiedConsentService> consent_service_;
};

TEST_F(UnifiedConsentServiceTest, DefaultValuesWhenSignedOut) {
  CreateConsentService();
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
}

TEST_F(UnifiedConsentServiceTest, EnableUrlKeyedAnonymizedDataCollection) {
  CreateConsentService();
  identity_test_environment_.SetPrimaryAccount("testaccount@gmail.com",
                                               signin::ConsentLevel::kSync);
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));

  // Enable services and check expectations.
  consent_service_->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
  EXPECT_TRUE(pref_service_.GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
}

TEST_F(UnifiedConsentServiceTest, Migration_UpdateSettings) {
  // Create user that syncs history and has no custom passphrase.
  identity_test_environment_.SetPrimaryAccount("testaccount@gmail.com",
                                               signin::ConsentLevel::kSync);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      false, {syncer::UserSelectableType::kHistory});
  EXPECT_TRUE(sync_service_.IsSyncFeatureActive());
  // Url keyed data collection is off before the migration.
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));

  CreateConsentService();
  EXPECT_EQ(GetMigrationState(), unified_consent::MigrationState::kCompleted);
  // During the migration Url keyed data collection is enabled.
  EXPECT_TRUE(pref_service_.GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(UnifiedConsentServiceTest, ClearPrimaryAccountDisablesSomeServices) {
  base::HistogramTester histogram_tester;

  CreateConsentService();
  identity_test_environment_.SetPrimaryAccount("testaccount@gmail.com",
                                               signin::ConsentLevel::kSync);

  // Precondition: Enable unified consent.
  consent_service_->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
  EXPECT_TRUE(pref_service_.GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));

  // Clearing primary account revokes unfied consent and a couple of other
  // non-personalized services.
  identity_test_environment_.ClearPrimaryAccount();
  EXPECT_FALSE(pref_service_.GetBoolean(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
}

TEST_F(UnifiedConsentServiceTest, Migration_NotSignedIn) {
  base::HistogramTester histogram_tester;

  CreateConsentService();
  // The user is signed out, so the migration is completed after the
  // creation of the consent service.
  EXPECT_EQ(GetMigrationState(), unified_consent::MigrationState::kCompleted);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace unified_consent
