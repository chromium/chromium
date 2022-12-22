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

// Verify that invalidation versions are persisted and loaded correctly.
TEST_F(SyncTransportDataPrefsTest, InvalidationVersions) {
  std::map<ModelType, int64_t> versions;
  versions[BOOKMARKS] = 10;
  versions[SESSIONS] = 20;
  versions[PREFERENCES] = 30;

  sync_prefs_->UpdateInvalidationVersions(versions);

  std::map<ModelType, int64_t> versions2 =
      sync_prefs_->GetInvalidationVersions();

  EXPECT_EQ(versions.size(), versions2.size());
  for (auto [type, version] : versions2) {
    EXPECT_EQ(versions[type], version);
  }
}

TEST_F(SyncTransportDataPrefsTest, MigrateInvalidationVersions) {
  // Set up entries for all data types in the legacy pref.
  base::Value::Dict legacy_invalidation_versions;
  legacy_invalidation_versions.SetByDottedPath("Bookmarks", "11");
  legacy_invalidation_versions.SetByDottedPath("Preferences", "12");
  legacy_invalidation_versions.SetByDottedPath("Passwords", "13");
  legacy_invalidation_versions.SetByDottedPath("Autofill Profiles", "14");
  legacy_invalidation_versions.SetByDottedPath("Autofill", "15");
  legacy_invalidation_versions.SetByDottedPath("Autofill Wallet", "16");
  legacy_invalidation_versions.SetByDottedPath("Autofill Wallet Metadata",
                                               "17");
  legacy_invalidation_versions.SetByDottedPath("Autofill Wallet Offer", "18");
  legacy_invalidation_versions.SetByDottedPath("Themes", "19");
  legacy_invalidation_versions.SetByDottedPath("Typed URLs", "20");
  legacy_invalidation_versions.SetByDottedPath("Extensions", "21");
  legacy_invalidation_versions.SetByDottedPath("Search Engines", "22");
  legacy_invalidation_versions.SetByDottedPath("Sessions", "23");
  legacy_invalidation_versions.SetByDottedPath("Apps", "24");
  legacy_invalidation_versions.SetByDottedPath("App settings", "25");
  legacy_invalidation_versions.SetByDottedPath("Extension settings", "26");
  legacy_invalidation_versions.SetByDottedPath("History Delete Directives",
                                               "27");
  legacy_invalidation_versions.SetByDottedPath("Dictionary", "28");
  legacy_invalidation_versions.SetByDottedPath("Device Info", "29");
  legacy_invalidation_versions.SetByDottedPath("Priority Preferences", "30");
  legacy_invalidation_versions.SetByDottedPath("Managed User Settings", "31");
  legacy_invalidation_versions.SetByDottedPath("App List", "32");
  legacy_invalidation_versions.SetByDottedPath("Arc Package", "33");
  legacy_invalidation_versions.SetByDottedPath("Printers", "34");
  legacy_invalidation_versions.SetByDottedPath("Reading List", "35");
  legacy_invalidation_versions.SetByDottedPath("Send Tab To Self", "36");
  legacy_invalidation_versions.SetByDottedPath("Wifi Configurations", "37");
  legacy_invalidation_versions.SetByDottedPath("Web Apps", "38");
  legacy_invalidation_versions.SetByDottedPath("OS Preferences", "39");
  legacy_invalidation_versions.SetByDottedPath("OS Priority Preferences", "40");
  legacy_invalidation_versions.SetByDottedPath("Workspace Desk", "41");
  legacy_invalidation_versions.SetByDottedPath("Encryption Keys", "42");

  const size_t data_type_count = legacy_invalidation_versions.size();
  pref_service_.SetDict("sync.invalidation_versions",
                        std::move(legacy_invalidation_versions));

  // The legacy pref should not be used by GetInvalidationVersions().
  ASSERT_TRUE(sync_prefs_->GetInvalidationVersions().empty());

  // Run the migration!
  SyncTransportDataPrefs::MigrateInvalidationVersions(&pref_service_);

  // Make sure the entries were properly migrated.
  std::map<ModelType, int64_t> versions =
      sync_prefs_->GetInvalidationVersions();
  EXPECT_EQ(versions.size(), data_type_count);
  // Just spot-check the actual values for a few types.
  EXPECT_EQ(versions[BOOKMARKS], 11);
  EXPECT_EQ(versions[EXTENSION_SETTINGS], 26);
  EXPECT_EQ(versions[NIGORI], 42);

  // Make some changes to the invalidation versions, then run the migration
  // again. This should *not* overwrite the new values.
  versions[BOOKMARKS] = 50;
  versions[EXTENSIONS] = 51;
  versions.erase(OS_PREFERENCES);
  sync_prefs_->UpdateInvalidationVersions(versions);

  SyncTransportDataPrefs::MigrateInvalidationVersions(&pref_service_);

  versions = sync_prefs_->GetInvalidationVersions();
  EXPECT_EQ(versions.size(), data_type_count - 1);
  EXPECT_EQ(versions[BOOKMARKS], 50);
  EXPECT_EQ(versions[EXTENSIONS], 51);
}

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
