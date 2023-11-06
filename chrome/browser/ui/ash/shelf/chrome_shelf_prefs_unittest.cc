// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/chrome_shelf_prefs.h"

#include <map>
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/to_vector.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using SyncItem = app_list::AppListSyncableService::SyncItem;

std::unique_ptr<SyncItem> MakeSyncItem(
    const std::string& id,
    const syncer::StringOrdinal& pin_ordinal,
    absl::optional<bool> is_user_pinned = absl::nullopt) {
  auto item = std::make_unique<SyncItem>(
      id, sync_pb::AppListSpecifics::TYPE_APP, /*is_new=*/false);
  item->item_pin_ordinal = pin_ordinal;
  item->is_user_pinned = is_user_pinned;
  return item;
}

class ShelfControllerHelperFake : public ShelfControllerHelper {
 public:
  explicit ShelfControllerHelperFake(Profile* profile)
      : ShelfControllerHelper(profile) {}
  ~ShelfControllerHelperFake() override = default;
  ShelfControllerHelperFake(const ShelfControllerHelperFake&) = delete;
  ShelfControllerHelperFake& operator=(const ShelfControllerHelperFake&) =
      delete;

  bool IsValidIDForCurrentUser(const std::string& app_id) const override {
    // ash-chrome is never a valid app ids as it is never exposed to the app
    // service.
    return app_id != app_constants::kChromeAppId;
  }
};

// A fake for AppListSyncableService that allows easy modifications.
class AppListSyncableServiceFake : public app_list::AppListSyncableService {
 public:
  explicit AppListSyncableServiceFake(Profile* profile)
      : app_list::AppListSyncableService(profile) {}
  ~AppListSyncableServiceFake() override = default;
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
                      const syncer::StringOrdinal& item_pin_ordinal,
                      bool pinned_by_policy) override {
    auto it = item_map_.find(app_id);
    if (it == item_map_.end()) {
      item_map_[app_id] = MakeSyncItem(app_id, item_pin_ordinal,
                                       /*is_user_pinned=*/!pinned_by_policy);
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

}  // namespace

// Unit tests for ChromeShelfPrefs
class ChromeShelfPrefsTest : public testing::Test {
 public:
  ChromeShelfPrefsTest() = default;
  ~ChromeShelfPrefsTest() override = default;
  ChromeShelfPrefsTest(const ChromeShelfPrefsTest&) = delete;
  ChromeShelfPrefsTest& operator=(const ChromeShelfPrefsTest&) = delete;

  void SetUp() override {
    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    auto* registry = prefs->registry();
    RegisterUserProfilePrefs(registry);
    prefs->SetBoolean(ash::prefs::kFilesAppUIPrefsMigrated, true);
    prefs->SetBoolean(ash::prefs::kProjectorSWAUIPrefsMigrated, true);
    profile_ =
        TestingProfile::Builder()
            .SetProfileName("Test")
            .SetPrefService(std::move(prefs))
            .AddTestingFactory(
                app_list::AppListSyncableServiceFactory::GetInstance(),
                base::BindRepeating([](content::BrowserContext* browser_context)
                                        -> std::unique_ptr<KeyedService> {
                  return std::make_unique<AppListSyncableServiceFake>(
                      Profile::FromBrowserContext(browser_context));
                }))
            .Build();
    ChromeShelfPrefs::SetShouldAddDefaultAppsForTest();
    shelf_prefs_ = std::make_unique<ChromeShelfPrefs>(profile_.get());
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<ash::FakeChromeUserManager>());
    helper_ = std::make_unique<ShelfControllerHelperFake>(profile_.get());
  }

  void TearDown() override {
    shelf_prefs_.reset();
    scoped_user_manager_.reset();
    profile_.reset();
  }

  void AddRegularUser(const std::string& email) {
    AccountId account_id = AccountId::FromUserEmail(email);
    auto* fake_user_manager = static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
    const user_manager::User* user = fake_user_manager->AddUser(account_id);
    fake_user_manager->UserLoggedIn(account_id, user->username_hash(),
                                    /*browser_restart=*/false,
                                    /*is_child=*/false);
  }

  AppListSyncableServiceFake& syncable_service() {
    return *static_cast<AppListSyncableServiceFake*>(
        app_list::AppListSyncableServiceFactory::GetForProfile(profile_.get()));
  }

  std::vector<std::string> GetPinnedAppIds() const {
    return base::test::ToVector(
        shelf_prefs_->GetPinnedAppsFromSync(helper_.get()),
        &ash::ShelfID::app_id);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<ChromeShelfPrefs> shelf_prefs_;
  std::unique_ptr<ShelfControllerHelperFake> helper_;

  std::unique_ptr<Profile> profile_;
};

TEST_F(ChromeShelfPrefsTest, AddChromePinNoExistingOrdinal) {
  shelf_prefs_->EnsureChromePinned();

  // Check that chrome now has a valid ordinal.
  EXPECT_TRUE(syncable_service()
                  .item_map_[app_constants::kChromeAppId]
                  ->item_pin_ordinal.IsValid());
}

TEST_F(ChromeShelfPrefsTest, AddChromePinExistingOrdinal) {
  // Set up the initial ordinals.
  syncer::StringOrdinal initial_ordinal =
      syncer::StringOrdinal::CreateInitialOrdinal();
  syncable_service().item_map_[app_constants::kChromeAppId] =
      MakeSyncItem(app_constants::kChromeAppId, initial_ordinal);

  shelf_prefs_->EnsureChromePinned();

  // Check that the chrome ordinal did not change.
  ASSERT_TRUE(syncable_service()
                  .item_map_[app_constants::kChromeAppId]
                  ->item_pin_ordinal.IsValid());
  auto& pin_ordinal = syncable_service()
                          .item_map_[app_constants::kChromeAppId]
                          ->item_pin_ordinal;
  EXPECT_TRUE(pin_ordinal.Equals(initial_ordinal));
}

TEST_F(ChromeShelfPrefsTest, AddDefaultApps) {
  shelf_prefs_->EnsureChromePinned();
  shelf_prefs_->AddDefaultApps();

  ASSERT_TRUE(syncable_service()
                  .item_map_[app_constants::kChromeAppId]
                  ->item_pin_ordinal.IsValid());

  // Check that a pin was added for the gmail app.
  ASSERT_TRUE(syncable_service()
                  .item_map_[extension_misc::kGmailAppId]
                  ->item_pin_ordinal.IsValid());
}

// If the profile changes, then migrations should be run again.
TEST_F(ChromeShelfPrefsTest, ProfileChanged) {
  // Migration is necessary to begin with.
  ASSERT_TRUE(shelf_prefs_->ShouldPerformConsistencyMigrations());
  std::vector<std::string> pinned_apps_strs = GetPinnedAppIds();

  // Pinned apps should have the chrome app as the first item.
  ASSERT_GE(pinned_apps_strs.size(), 1u);
  EXPECT_EQ(pinned_apps_strs[0], app_constants::kChromeAppId);

  // Pinned apps should have the gmail app.
  EXPECT_TRUE(base::Contains(pinned_apps_strs, extension_misc::kGmailAppId));

  // Migration is no longer necessary.
  ASSERT_FALSE(shelf_prefs_->ShouldPerformConsistencyMigrations());

  // Change the profile. Migration is necessary again!
  shelf_prefs_->AttachProfile(nullptr);
  ASSERT_TRUE(shelf_prefs_->ShouldPerformConsistencyMigrations());
}

// If Lacros is the only browser, then it should be pinned instead of ash.
TEST_F(ChromeShelfPrefsTest, LacrosOnlyPinnedApp) {
  // Enable lacros-only.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(ash::standalone_browser::GetFeatureRefs(), {});
  AddRegularUser("test@test.com");

  // Migration is necessary to begin with.
  ASSERT_TRUE(shelf_prefs_->ShouldPerformConsistencyMigrations());
  std::vector<std::string> pinned_apps_strs = GetPinnedAppIds();

  // Pinned apps should have the chrome app as the first item.
  ASSERT_GE(pinned_apps_strs.size(), 1u);
  EXPECT_EQ(pinned_apps_strs[0], app_constants::kLacrosAppId);

  // Pinned apps should have the gmail app.
  EXPECT_TRUE(base::Contains(pinned_apps_strs, extension_misc::kGmailAppId));
}

// When moving from ash-only to lacros-only, the shelf position of the chrome
// app should stay constant.
TEST_F(ChromeShelfPrefsTest, ShelfPositionAfterLacrosMigration) {
  // Set up ash-chrome in the middle position.
  syncer::StringOrdinal ordinal1 =
      syncer::StringOrdinal::CreateInitialOrdinal();
  syncer::StringOrdinal ordinal2 = ordinal1.CreateAfter();
  syncer::StringOrdinal ordinal3 = ordinal2.CreateAfter();

  syncable_service().item_map_["dummy1"] = MakeSyncItem("dummy1", ordinal1);
  syncable_service().item_map_[app_constants::kChromeAppId] =
      MakeSyncItem(app_constants::kChromeAppId, ordinal2);
  syncable_service().item_map_["dummy2"] = MakeSyncItem("dummy2", ordinal3);

  // Enable lacros-only.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(ash::standalone_browser::GetFeatureRefs(), {});
  AddRegularUser("test@test.com");

  // Perform migration
  std::vector<std::string> pinned_apps_strs = GetPinnedAppIds();

  // Confirm that the ash-chrome position gets replaced by lacros-chrome.
  EXPECT_TRUE(base::Contains(pinned_apps_strs, app_constants::kLacrosAppId));
  EXPECT_FALSE(base::Contains(pinned_apps_strs, app_constants::kChromeAppId));
}
