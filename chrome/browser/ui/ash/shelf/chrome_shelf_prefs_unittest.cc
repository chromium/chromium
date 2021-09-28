// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/chrome_shelf_prefs.h"

#include <memory>

#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/model/string_ordinal.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kLegacyCameraAppId[] = "ngmkobaiicipbagcngcmilfkhejlnfci";

// A fake for AppListSyncableService that allows easy modifications.
class AppListSyncableServiceFake : public app_list::AppListSyncableService {
 public:
  AppListSyncableServiceFake() {}
  ~AppListSyncableServiceFake() override {}
  AppListSyncableServiceFake(const AppListSyncableServiceFake&) = delete;
  AppListSyncableServiceFake& operator=(const AppListSyncableServiceFake&) =
      delete;

  syncer::StringOrdinal GetPinPosition(const std::string& app_id) override {
    const SyncItem* item = GetSyncItem(app_id);
    if (!item)
      return syncer::StringOrdinal();
    return item->item_pin_ordinal;
  }

  void SetPinPosition(const std::string& app_id,
                      const syncer::StringOrdinal& item_pin_ordinal) override {
    auto it = item_map_.find(app_id);
    if (it == item_map_.end())
      return;
    it->second->item_pin_ordinal = item_pin_ordinal;
  }
  const SyncItemMap& sync_items() const override { return item_map_; }

  const SyncItem* GetSyncItem(const std::string& id) const override {
    auto it = item_map_.find(id);
    if (it == item_map_.end())
      return nullptr;
    return it->second.get();
  }

  // using SyncItemMap = std::map<std::string, std::unique_ptr<SyncItem>>;
  SyncItemMap item_map_;
};

// Unit tests for ChromeShelfPrefs
class ChromeShelfPrefsTest : public testing::Test {
 public:
  ChromeShelfPrefsTest() {}
  ~ChromeShelfPrefsTest() override {}
  ChromeShelfPrefsTest(const ChromeShelfPrefsTest&) = delete;
  ChromeShelfPrefsTest& operator=(const ChromeShelfPrefsTest&) = delete;

  using SyncItem = app_list::AppListSyncableService::SyncItem;

  std::unique_ptr<SyncItem> MakeSyncItem(
      const std::string& id,
      const syncer::StringOrdinal& pin_ordinal) {
    auto item =
        std::make_unique<SyncItem>(id, sync_pb::AppListSpecifics::TYPE_APP);
    item->item_pin_ordinal = pin_ordinal;
    return item;
  }

 protected:
  TestingPrefServiceSimple pref_service_;
  ChromeShelfPrefs shelf_prefs_;
  AppListSyncableServiceFake syncable_service_;
};

TEST_F(ChromeShelfPrefsTest, MigrateLegacyCameraApp) {
  // Set up the initial ordinals.
  syncer::StringOrdinal initial_ordinal =
      syncer::StringOrdinal::CreateInitialOrdinal();
  syncer::StringOrdinal next_ordinal = initial_ordinal.CreateAfter();
  syncable_service_.item_map_[kLegacyCameraAppId] =
      MakeSyncItem(kLegacyCameraAppId, next_ordinal);
  syncable_service_.item_map_[extension_misc::kCameraAppId] =
      MakeSyncItem(kLegacyCameraAppId, syncer::StringOrdinal());

  // Migrate.
  shelf_prefs_.MigrateLegacyCameraApp(&syncable_service_, &pref_service_);

  // Check that the legacy camera app now has an invalid ordinal.
  EXPECT_FALSE(syncable_service_.item_map_[kLegacyCameraAppId]
                   ->item_pin_ordinal.IsValid());

  // Check that the new camera app has the ordinal of the legacy camera app.
  auto& pin_ordinal = syncable_service_.item_map_[extension_misc::kCameraAppId]
                          ->item_pin_ordinal;
  EXPECT_TRUE(pin_ordinal.Equals(next_ordinal));
}

}  // namespace
