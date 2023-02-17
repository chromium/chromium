// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/glue/sync_transport_data_prefs.h"

#include <memory>
#include <utility>

#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class SyncTransportDataPrefsTest : public testing::Test {
 protected:
  SyncTransportDataPrefsTest() {
    SyncTransportDataPrefs::RegisterProfilePrefs(pref_service_.registry());
    sync_prefs_ = std::make_unique<SyncTransportDataPrefs>(&pref_service_);
  }

  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<SyncTransportDataPrefs> sync_prefs_;
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

TEST_F(SyncTransportDataPrefsTest, ClearAll) {
  sync_prefs_->SetLastSyncedTime(base::Time::Now());
  ASSERT_NE(base::Time(), sync_prefs_->GetLastSyncedTime());

  sync_prefs_->ClearAll();

  EXPECT_EQ(base::Time(), sync_prefs_->GetLastSyncedTime());
}

}  // namespace

}  // namespace syncer
