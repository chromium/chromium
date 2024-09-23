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

class SyncTransportDataPrefsTest : public testing::Test {
 protected:
  SyncTransportDataPrefsTest() {
    SyncTransportDataPrefs::RegisterProfilePrefs(pref_service_.registry());
    sync_prefs_ = std::make_unique<SyncTransportDataPrefs>(
        &pref_service_, signin::GaiaIdHash::FromGaiaId("gaia_id"));
    sync_prefs_2_ = std::make_unique<SyncTransportDataPrefs>(
        &pref_service_, signin::GaiaIdHash::FromGaiaId("gaia_id_2"));
  }

  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<SyncTransportDataPrefs> sync_prefs_;
  std::unique_ptr<SyncTransportDataPrefs> sync_prefs_2_;
};

TEST_F(SyncTransportDataPrefsTest, PollInterval) {
  EXPECT_TRUE(sync_prefs_->GetPollInterval().is_zero());
  sync_prefs_->SetPollInterval(base::Minutes(30));
  EXPECT_FALSE(sync_prefs_->GetPollInterval().is_zero());
  EXPECT_EQ(sync_prefs_->GetPollInterval().InMinutes(), 30);
}

TEST_F(SyncTransportDataPrefsTest, ResetsVeryShortPollInterval) {
  // Set the poll interval to something unreasonably short.
  sync_prefs_->SetPollInterval(base::Milliseconds(100));
  // This should reset the pref to "empty", so that callers will use a
  // reasonable default value.
  EXPECT_TRUE(sync_prefs_->GetPollInterval().is_zero());
}

TEST_F(SyncTransportDataPrefsTest, LastSyncTime) {
  EXPECT_EQ(base::Time(), sync_prefs_->GetLastSyncedTime());
  const base::Time now = base::Time::Now();
  sync_prefs_->SetLastSyncedTime(now);
  EXPECT_EQ(now, sync_prefs_->GetLastSyncedTime());
}

TEST_F(SyncTransportDataPrefsTest, Clear) {
  sync_prefs_->SetLastSyncedTime(base::Time::Now());
  ASSERT_NE(base::Time(), sync_prefs_->GetLastSyncedTime());

  // ClearForCurrentAccount() should clear the persisted value.
  sync_prefs_->ClearForCurrentAccount();
  EXPECT_EQ(base::Time(), sync_prefs_->GetLastSyncedTime());
}

TEST_F(SyncTransportDataPrefsTest, ValuesAreAccountScoped) {
  const base::Time now = base::Time::Now();

  // Set some values for the first account.
  sync_prefs_->SetCacheGuid("cache_guid_1");
  sync_prefs_->SetBirthday("birthday_1");
  sync_prefs_->SetBagOfChips("bag_of_chips_1");
  sync_prefs_->SetLastSyncedTime(now - base::Seconds(1));
  sync_prefs_->SetLastPollTime(now - base::Minutes(1));
  sync_prefs_->SetPollInterval(base::Hours(1));

  ASSERT_EQ(sync_prefs_->GetCacheGuid(), "cache_guid_1");
  ASSERT_EQ(sync_prefs_->GetBirthday(), "birthday_1");
  ASSERT_EQ(sync_prefs_->GetBagOfChips(), "bag_of_chips_1");
  ASSERT_EQ(sync_prefs_->GetLastSyncedTime(), now - base::Seconds(1));
  ASSERT_EQ(sync_prefs_->GetLastPollTime(), now - base::Minutes(1));
  ASSERT_EQ(sync_prefs_->GetPollInterval(), base::Hours(1));

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
  EXPECT_EQ(sync_prefs_->GetCacheGuid(), "cache_guid_1");
  EXPECT_EQ(sync_prefs_->GetBirthday(), "birthday_1");
  EXPECT_EQ(sync_prefs_->GetBagOfChips(), "bag_of_chips_1");
  EXPECT_EQ(sync_prefs_->GetLastSyncedTime(), now - base::Seconds(1));
  EXPECT_EQ(sync_prefs_->GetLastPollTime(), now - base::Minutes(1));
  EXPECT_EQ(sync_prefs_->GetPollInterval(), base::Hours(1));

  // Clear the values for the first account.
  sync_prefs_->ClearForCurrentAccount();

  EXPECT_TRUE(sync_prefs_->GetCacheGuid().empty());
  EXPECT_TRUE(sync_prefs_->GetBirthday().empty());
  EXPECT_TRUE(sync_prefs_->GetBagOfChips().empty());
  EXPECT_EQ(sync_prefs_->GetLastSyncedTime(), base::Time());
  EXPECT_EQ(sync_prefs_->GetLastPollTime(), base::Time());
  EXPECT_EQ(sync_prefs_->GetPollInterval(), base::TimeDelta());

  // The second account's values should be unchanged.
  EXPECT_EQ(sync_prefs_2_->GetCacheGuid(), "cache_guid_2");
  EXPECT_EQ(sync_prefs_2_->GetBirthday(), "birthday_2");
  EXPECT_EQ(sync_prefs_2_->GetBagOfChips(), "bag_of_chips_2");
  EXPECT_EQ(sync_prefs_2_->GetLastSyncedTime(), now - base::Seconds(2));
  EXPECT_EQ(sync_prefs_2_->GetLastPollTime(), now - base::Minutes(2));
  EXPECT_EQ(sync_prefs_2_->GetPollInterval(), base::Hours(2));
}

}  // namespace

}  // namespace syncer
