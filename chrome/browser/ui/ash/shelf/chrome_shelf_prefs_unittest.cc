// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/chrome_shelf_prefs.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/values.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/model/string_ordinal.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using SyncItem = app_list::AppListSyncableService::SyncItem;

std::unique_ptr<SyncItem> MakeSyncItem(
    const std::string& id,
    const syncer::StringOrdinal& pin_ordinal) {
  auto item =
      std::make_unique<SyncItem>(id, sync_pb::AppListSpecifics::TYPE_APP);
  item->item_pin_ordinal = pin_ordinal;
  return item;
}

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

  // Adds a new pin if it does not already exist.
  void SetPinPosition(const std::string& app_id,
                      const syncer::StringOrdinal& item_pin_ordinal) override {
    auto it = item_map_.find(app_id);
    if (it == item_map_.end()) {
      item_map_[app_id] = MakeSyncItem(app_id, item_pin_ordinal);
      return;
    }
    it->second->item_pin_ordinal = item_pin_ordinal;
  }
  const SyncItemMap& sync_items() const override { return item_map_; }

  const SyncItem* GetSyncItem(const std::string& id) const override {
    auto it = item_map_.find(id);
    if (it == item_map_.end())
      return nullptr;
    return it->second.get();
  }

  bool IsInitialized() const override { return true; }

  SyncItemMap item_map_;
};

// A fake that stubs in functionality for testing.
class ChromeShelfPrefsFake : public ChromeShelfPrefs {
 public:
  ChromeShelfPrefsFake(Profile* profile,
                       AppListSyncableServiceFake* syncable_service,
                       TestingPrefServiceSimple* pref_service)
      : ChromeShelfPrefs(profile),
        pref_service_(pref_service),
        syncable_service_(syncable_service) {}
  ~ChromeShelfPrefsFake() override {}
  ChromeShelfPrefsFake(const ChromeShelfPrefsFake&) = delete;
  ChromeShelfPrefsFake& operator=(const ChromeShelfPrefsFake&) = delete;

  app_list::AppListSyncableService* const GetSyncableService() override {
    return syncable_service_;
  }

  PrefService* GetPrefs() override { return pref_service_; }

  bool IsSyncItemValid(const std::string& id,
                       ShelfControllerHelper* helper) override {
    return true;
  }

  bool ShouldAddDefaultApps(PrefService* pref_service) override { return true; }

  void ObserveSyncService() override {}

  TestingPrefServiceSimple* const pref_service_;
  AppListSyncableServiceFake* const syncable_service_;
};

// Unit tests for ChromeShelfPrefs
class ChromeShelfPrefsTest : public testing::Test {
 public:
  ChromeShelfPrefsTest() = default;
  ~ChromeShelfPrefsTest() override {}
  ChromeShelfPrefsTest(const ChromeShelfPrefsTest&) = delete;
  ChromeShelfPrefsTest& operator=(const ChromeShelfPrefsTest&) = delete;

  void SetUp() override {
    shelf_prefs_ = std::make_unique<ChromeShelfPrefsFake>(
        nullptr, &syncable_service_, &pref_service_);
    pref_service_.registry()->RegisterListPref(
        prefs::kShelfDefaultPinLayoutRolls);
    pref_service_.registry()->RegisterListPref(
        prefs::kPolicyPinnedLauncherApps);
  }

  void TearDown() override { shelf_prefs_.reset(); }

 protected:
  TestingPrefServiceSimple pref_service_;
  AppListSyncableServiceFake syncable_service_;
  std::unique_ptr<ChromeShelfPrefsFake> shelf_prefs_;
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
  shelf_prefs_->MigrateLegacyCameraApp(&syncable_service_);

  // Check that the legacy camera app now has an invalid ordinal.
  EXPECT_FALSE(syncable_service_.item_map_[kLegacyCameraAppId]
                   ->item_pin_ordinal.IsValid());

  // Check that the new camera app has the ordinal of the legacy camera app.
  auto& pin_ordinal = syncable_service_.item_map_[extension_misc::kCameraAppId]
                          ->item_pin_ordinal;
  EXPECT_TRUE(pin_ordinal.Equals(next_ordinal));
}

TEST_F(ChromeShelfPrefsTest, AddChromePinNoExistingOrdinal) {
  shelf_prefs_->EnsureChromePinned(&syncable_service_);

  // Check that chrome now has a valid ordinal.
  EXPECT_TRUE(syncable_service_.item_map_[extension_misc::kChromeAppId]
                  ->item_pin_ordinal.IsValid());
}

TEST_F(ChromeShelfPrefsTest, AddChromePinExistingOrdinal) {
  // Set up the initial ordinals.
  syncer::StringOrdinal initial_ordinal =
      syncer::StringOrdinal::CreateInitialOrdinal();
  syncable_service_.item_map_[extension_misc::kChromeAppId] =
      MakeSyncItem(extension_misc::kChromeAppId, initial_ordinal);

  shelf_prefs_->EnsureChromePinned(&syncable_service_);

  // Check that the chrome ordinal did not change.
  ASSERT_TRUE(syncable_service_.item_map_[extension_misc::kChromeAppId]
                  ->item_pin_ordinal.IsValid());
  auto& pin_ordinal = syncable_service_.item_map_[extension_misc::kChromeAppId]
                          ->item_pin_ordinal;
  EXPECT_TRUE(pin_ordinal.Equals(initial_ordinal));
}

TEST_F(ChromeShelfPrefsTest, AddDefaultApps) {
  shelf_prefs_->EnsureChromePinned(&syncable_service_);
  shelf_prefs_->AddDefaultApps(&pref_service_, &syncable_service_);

  ASSERT_TRUE(syncable_service_.item_map_[extension_misc::kChromeAppId]
                  ->item_pin_ordinal.IsValid());

  // Check that a pin was added for the gmail app.
  ASSERT_TRUE(syncable_service_.item_map_[extension_misc::kGmailAppId]
                  ->item_pin_ordinal.IsValid());
}

// If the profile changes, then migrations should be run again.
TEST_F(ChromeShelfPrefsTest, ProfileChanged) {
  // Migration is necessary to begin with.
  ASSERT_TRUE(shelf_prefs_->ShouldPerformConsistencyMigrations());
  std::vector<ash::ShelfID> pinned_apps =
      shelf_prefs_->GetPinnedAppsFromSync(nullptr);
  std::vector<std::string> pinned_apps_strs;
  for (auto& shelf_id : pinned_apps)
    pinned_apps_strs.push_back(shelf_id.app_id);

  // Pinned apps should have the chrome app as the first item.
  ASSERT_GE(pinned_apps_strs.size(), 1u);
  EXPECT_EQ(pinned_apps_strs[0], extension_misc::kChromeAppId);

  // Pinned apps should have the gmail app.
  EXPECT_TRUE(base::Contains(pinned_apps_strs, extension_misc::kGmailAppId));

  // Migration is no longer necessary.
  ASSERT_FALSE(shelf_prefs_->ShouldPerformConsistencyMigrations());

  // Change the profile. Migration is necessary again!
  shelf_prefs_->AttachProfile(nullptr);
  ASSERT_TRUE(shelf_prefs_->ShouldPerformConsistencyMigrations());
}

}  // namespace
