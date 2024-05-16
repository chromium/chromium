// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/glue/sync_transport_data_prefs.h"

#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class SyncTransportDataPrefsTestBase {
 protected:
  explicit SyncTransportDataPrefsTestBase(bool enable_account_keyed_prefs) {
    if (enable_account_keyed_prefs) {
      features_.InitAndEnableFeature(kSyncAccountKeyedTransportPrefs);
    } else {
      features_.InitAndDisableFeature(kSyncAccountKeyedTransportPrefs);
    }

    SyncTransportDataPrefs::RegisterProfilePrefs(pref_service_.registry());
  }

  base::test::ScopedFeatureList features_;

  TestingPrefServiceSimple pref_service_;
};

class SyncTransportDataPrefsTest : public SyncTransportDataPrefsTestBase,
                                   public testing::TestWithParam<bool> {
 protected:
  SyncTransportDataPrefsTest()
      : SyncTransportDataPrefsTestBase(ArePrefsAccountKeyed()) {
    sync_prefs_ = std::make_unique<SyncTransportDataPrefs>(
        &pref_service_, signin::GaiaIdHash::FromGaiaId("gaia_id"));
  }

  bool ArePrefsAccountKeyed() const { return GetParam(); }

  std::unique_ptr<SyncTransportDataPrefs> sync_prefs_;
};

TEST_P(SyncTransportDataPrefsTest, PollInterval) {
  EXPECT_TRUE(sync_prefs_->GetPollInterval().is_zero());
  sync_prefs_->SetPollInterval(base::Minutes(30));
  EXPECT_FALSE(sync_prefs_->GetPollInterval().is_zero());
  EXPECT_EQ(sync_prefs_->GetPollInterval().InMinutes(), 30);
}

TEST_P(SyncTransportDataPrefsTest, ResetsVeryShortPollInterval) {
  // Set the poll interval to something unreasonably short.
  sync_prefs_->SetPollInterval(base::Milliseconds(100));
  // This should reset the pref to "empty", so that callers will use a
  // reasonable default value.
  EXPECT_TRUE(sync_prefs_->GetPollInterval().is_zero());
}

TEST_P(SyncTransportDataPrefsTest, LastSyncTime) {
  EXPECT_EQ(base::Time(), sync_prefs_->GetLastSyncedTime());
  const base::Time now = base::Time::Now();
  sync_prefs_->SetLastSyncedTime(now);
  EXPECT_EQ(now, sync_prefs_->GetLastSyncedTime());
}

TEST_P(SyncTransportDataPrefsTest, ClearAll) {
  sync_prefs_->SetLastSyncedTime(base::Time::Now());
  ASSERT_NE(base::Time(), sync_prefs_->GetLastSyncedTime());

  sync_prefs_->ClearAllLegacy();

  // ClearAllLegacy() should have had an effect on the visible value iff the
  // account-keying is disabled.
  if (ArePrefsAccountKeyed()) {
    EXPECT_NE(base::Time(), sync_prefs_->GetLastSyncedTime());
  } else {
    EXPECT_EQ(base::Time(), sync_prefs_->GetLastSyncedTime());
  }
}

INSTANTIATE_TEST_SUITE_P(, SyncTransportDataPrefsTest, testing::Bool());

class SyncTransportDataPrefsAccountScopedTest
    : public SyncTransportDataPrefsTestBase,
      public testing::Test {
 protected:
  SyncTransportDataPrefsAccountScopedTest()
      : SyncTransportDataPrefsTestBase(true) {
    sync_prefs_1_ = std::make_unique<SyncTransportDataPrefs>(
        &pref_service_, signin::GaiaIdHash::FromGaiaId("gaia_id"));
    sync_prefs_2_ = std::make_unique<SyncTransportDataPrefs>(
        &pref_service_, signin::GaiaIdHash::FromGaiaId("gaia_id_2"));
  }

  std::unique_ptr<SyncTransportDataPrefs> sync_prefs_1_;
  std::unique_ptr<SyncTransportDataPrefs> sync_prefs_2_;
};

TEST_F(SyncTransportDataPrefsAccountScopedTest, Clear) {
  sync_prefs_1_->SetLastSyncedTime(base::Time::Now());
  ASSERT_NE(base::Time(), sync_prefs_1_->GetLastSyncedTime());

  // ClearAllLegacy() should *not* affect the persisted value.
  sync_prefs_1_->ClearAllLegacy();
  EXPECT_NE(base::Time(), sync_prefs_1_->GetLastSyncedTime());

  // ClearForCurrentAccount() should affect it though.
  sync_prefs_1_->ClearForCurrentAccount();
  EXPECT_EQ(base::Time(), sync_prefs_1_->GetLastSyncedTime());
}

TEST_F(SyncTransportDataPrefsAccountScopedTest, ValuesAreAccountScoped) {
  const base::Time now = base::Time::Now();

  // Set some values for the first account.
  sync_prefs_1_->SetCacheGuid("cache_guid_1");
  sync_prefs_1_->SetBirthday("birthday_1");
  sync_prefs_1_->SetBagOfChips("bag_of_chips_1");
  sync_prefs_1_->SetLastSyncedTime(now - base::Seconds(1));
  sync_prefs_1_->SetLastPollTime(now - base::Minutes(1));
  sync_prefs_1_->SetPollInterval(base::Hours(1));

  ASSERT_EQ(sync_prefs_1_->GetCacheGuid(), "cache_guid_1");
  ASSERT_EQ(sync_prefs_1_->GetBirthday(), "birthday_1");
  ASSERT_EQ(sync_prefs_1_->GetBagOfChips(), "bag_of_chips_1");
  ASSERT_EQ(sync_prefs_1_->GetLastSyncedTime(), now - base::Seconds(1));
  ASSERT_EQ(sync_prefs_1_->GetLastPollTime(), now - base::Minutes(1));
  ASSERT_EQ(sync_prefs_1_->GetPollInterval(), base::Hours(1));

  // The second account's values should still be empty.
  EXPECT_TRUE(sync_prefs_2_->GetCacheGuid().empty());
  EXPECT_TRUE(sync_prefs_2_->GetBirthday().empty());
  EXPECT_TRUE(sync_prefs_2_->GetBagOfChips().empty());
  EXPECT_EQ(sync_prefs_2_->GetLastSyncedTime(), base::Time());
  EXPECT_EQ(sync_prefs_2_->GetLastPollTime(), base::Time());
  EXPECT_EQ(sync_prefs_2_->GetPollInterval(), base::TimeDelta());

  // Set some values for the second account.
  sync_prefs_2_->SetCacheGuid("cache_guid_2");
  sync_prefs_2_->SetBirthday("birthday_2");
  sync_prefs_2_->SetBagOfChips("bag_of_chips_2");
  sync_prefs_2_->SetLastSyncedTime(now - base::Seconds(2));
  sync_prefs_2_->SetLastPollTime(now - base::Minutes(2));
  sync_prefs_2_->SetPollInterval(base::Hours(2));

  ASSERT_EQ(sync_prefs_2_->GetCacheGuid(), "cache_guid_2");
  ASSERT_EQ(sync_prefs_2_->GetBirthday(), "birthday_2");
  ASSERT_EQ(sync_prefs_2_->GetBagOfChips(), "bag_of_chips_2");
  ASSERT_EQ(sync_prefs_2_->GetLastSyncedTime(), now - base::Seconds(2));
  ASSERT_EQ(sync_prefs_2_->GetLastPollTime(), now - base::Minutes(2));
  ASSERT_EQ(sync_prefs_2_->GetPollInterval(), base::Hours(2));

  // The first account's values should be unchanged.
  EXPECT_EQ(sync_prefs_1_->GetCacheGuid(), "cache_guid_1");
  EXPECT_EQ(sync_prefs_1_->GetBirthday(), "birthday_1");
  EXPECT_EQ(sync_prefs_1_->GetBagOfChips(), "bag_of_chips_1");
  EXPECT_EQ(sync_prefs_1_->GetLastSyncedTime(), now - base::Seconds(1));
  EXPECT_EQ(sync_prefs_1_->GetLastPollTime(), now - base::Minutes(1));
  EXPECT_EQ(sync_prefs_1_->GetPollInterval(), base::Hours(1));

  // Clear the values for the first account.
  sync_prefs_1_->ClearForCurrentAccount();

  EXPECT_TRUE(sync_prefs_1_->GetCacheGuid().empty());
  EXPECT_TRUE(sync_prefs_1_->GetBirthday().empty());
  EXPECT_TRUE(sync_prefs_1_->GetBagOfChips().empty());
  EXPECT_EQ(sync_prefs_1_->GetLastSyncedTime(), base::Time());
  EXPECT_EQ(sync_prefs_1_->GetLastPollTime(), base::Time());
  EXPECT_EQ(sync_prefs_1_->GetPollInterval(), base::TimeDelta());

  // The second account's values should be unchanged.
  EXPECT_EQ(sync_prefs_2_->GetCacheGuid(), "cache_guid_2");
  EXPECT_EQ(sync_prefs_2_->GetBirthday(), "birthday_2");
  EXPECT_EQ(sync_prefs_2_->GetBagOfChips(), "bag_of_chips_2");
  EXPECT_EQ(sync_prefs_2_->GetLastSyncedTime(), now - base::Seconds(2));
  EXPECT_EQ(sync_prefs_2_->GetLastPollTime(), now - base::Minutes(2));
  EXPECT_EQ(sync_prefs_2_->GetPollInterval(), base::Hours(2));
}

TEST(SyncTransportDataPrefsAccountScopedMigrationTest,
     MigrationToAccountScoped) {
  // Pref name for the account-keyed stuff, duplicated from
  // sync_transport_data_prefs.cc, used here for sanity checking that it gets
  // populated at the right point.
  const char kSyncTransportDataPerAccount[] = "sync.transport_data_per_account";

  TestingPrefServiceSimple pref_service;
  SyncTransportDataPrefs::RegisterProfilePrefs(pref_service.registry());

  const base::Time now = base::Time::Now();

  const std::string kGaiaId = "gaia_id";
  const signin::GaiaIdHash gaia_id_hash =
      signin::GaiaIdHash::FromGaiaId(kGaiaId);

  // Setup: Populate the legacy prefs.
  {
    base::test::ScopedFeatureList disable_account_scoped;
    disable_account_scoped.InitAndDisableFeature(
        kSyncAccountKeyedTransportPrefs);

    SyncTransportDataPrefs sync_prefs(&pref_service, gaia_id_hash);
    sync_prefs.SetCurrentSyncingGaiaId(kGaiaId);
    sync_prefs.SetCacheGuid("cache_guid_1");
    sync_prefs.SetBirthday("birthday_1");
    sync_prefs.SetBagOfChips("bag_of_chips_1");
    sync_prefs.SetLastSyncedTime(now - base::Seconds(1));
    sync_prefs.SetLastPollTime(now - base::Minutes(1));
    sync_prefs.SetPollInterval(base::Hours(1));

    // Nothing should have been written to the account-scoped pref yet.
    ASSERT_FALSE(pref_service.GetUserPrefValue(kSyncTransportDataPerAccount));
  }

  {
    base::test::ScopedFeatureList enable_account_scoped;
    enable_account_scoped.InitAndEnableFeature(kSyncAccountKeyedTransportPrefs);

    // Creating a SyncTransportDataPrefs instance (with the flag enabled)
    // triggers the migration.
    SyncTransportDataPrefs sync_prefs(&pref_service, gaia_id_hash);

    // The account-scoped dict pref is now populated.
    ASSERT_TRUE(pref_service.GetUserPrefValue(kSyncTransportDataPerAccount));

    // The visible values should be unchanged.
    EXPECT_EQ(sync_prefs.GetCacheGuid(), "cache_guid_1");
    EXPECT_EQ(sync_prefs.GetBirthday(), "birthday_1");
    EXPECT_EQ(sync_prefs.GetBagOfChips(), "bag_of_chips_1");
    EXPECT_EQ(sync_prefs.GetLastSyncedTime(), now - base::Seconds(1));
    EXPECT_EQ(sync_prefs.GetLastPollTime(), now - base::Minutes(1));
    EXPECT_EQ(sync_prefs.GetPollInterval(), base::Hours(1));
  }
}

}  // namespace

}  // namespace syncer
