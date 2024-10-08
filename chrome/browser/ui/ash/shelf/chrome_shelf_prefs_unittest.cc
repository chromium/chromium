// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/chrome_shelf_prefs.h"

#include <map>
#include <memory>
#include <vector>

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/webui/mall/app_id.h"
#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_constants.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using SyncItem = app_list::AppListSyncableService::SyncItem;

std::unique_ptr<SyncItem> MakeSyncItem(
    const std::string& id,
    const syncer::StringOrdinal& pin_ordinal,
    std::optional<bool> is_user_pinned = std::nullopt) {
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
    ChromeShelfPrefs::SetShouldAddDefaultAppsForTest(true);
    shelf_prefs_ = std::make_unique<ChromeShelfPrefs>(profile_.get());
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<ash::FakeChromeUserManager>());
    helper_ = std::make_unique<ShelfControllerHelperFake>(profile_.get());
  }

  void TearDown() override {
    shelf_prefs_.reset();
    scoped_user_manager_.reset();
    ChromeShelfPrefs::SetShouldAddDefaultAppsForTest(false);
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

  void InstallApp(apps::AppPtr app) {
    std::vector<apps::AppPtr> deltas;
    deltas.push_back(std::move(app));
    apps::AppServiceProxyFactory::GetForProfile(profile_.get())
        ->OnApps(std::move(deltas), apps::AppType::kUnknown,
                 /*should_notify_initialized=*/false);
  }

  void InstallApp(const apps::PackageId& package_id) {
    auto app_type = apps::AppType::kChromeApp;
    auto app_id = package_id.identifier();
    if (package_id.package_type() == apps::PackageType::kWeb) {
      app_type = apps::AppType::kWeb;
      app_id =
          web_app::GenerateAppId(std::nullopt, GURL(package_id.identifier()));
    }
    apps::AppPtr app = std::make_unique<apps::App>(app_type, app_id);
    app->readiness = apps::Readiness::kReady;
    app->name = package_id.identifier();
    app->installer_package_id = package_id;

    InstallApp(std::move(app));
  }

  AppListSyncableServiceFake& syncable_service() {
    return *static_cast<AppListSyncableServiceFake*>(
        app_list::AppListSyncableServiceFactory::GetForProfile(profile_.get()));
  }

  std::vector<std::string> GetPinnedAppIds() const {
    return base::ToVector(shelf_prefs_->GetPinnedAppsFromSync(helper_.get()),
                          &ash::ShelfID::app_id);
  }

  std::string GetPinned() {
    static const base::NoDestructor<std::map<std::string, std::string>> kAppMap(
        {
            {app_constants::kChromeAppId, "chrome"},
            {web_app::kContainerAppId, "container"},
            {web_app::kGmailAppId, "gmail"},
            {web_app::kGoogleCalendarAppId, "cal"},
            {file_manager::kFileManagerSwaAppId, "files"},
            {web_app::kMessagesAppId, "messages"},
            {web_app::kGoogleMeetAppId, "meet"},
            {arc::kPlayStoreAppId, "play"},
            {web_app::kYoutubeAppId, "youtube"},
            {arc::kGooglePhotosAppId, "photos"},
        });
    std::vector<std::string> apps;
    for (const auto& app_id : GetPinnedAppIds()) {
      auto it = kAppMap->find(app_id);
      apps.push_back(it != kAppMap->end() ? it->second : app_id);
    }
    return base::JoinString(apps, ", ");
  }

  bool IsGoogleChromeBranded() const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    return true;
#else
    return false;
#endif
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
                  .item_map_[web_app::kGmailAppId]
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
  EXPECT_TRUE(base::Contains(pinned_apps_strs, web_app::kGmailAppId));

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
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kEnableLacrosForTesting);
  AddRegularUser("test@test.com");

  // Migration is necessary to begin with.
  ASSERT_TRUE(shelf_prefs_->ShouldPerformConsistencyMigrations());
  std::vector<std::string> pinned_apps_strs = GetPinnedAppIds();

  // Pinned apps should have the chrome app as the first item.
  ASSERT_GE(pinned_apps_strs.size(), 1u);
  EXPECT_EQ(pinned_apps_strs[0], app_constants::kLacrosAppId);

  // Pinned apps should have the gmail app.
  EXPECT_TRUE(base::Contains(pinned_apps_strs, web_app::kGmailAppId));
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
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kEnableLacrosForTesting);
  AddRegularUser("test@test.com");

  // Perform migration
  std::vector<std::string> pinned_apps_strs = GetPinnedAppIds();

  // Confirm that the ash-chrome position gets replaced by lacros-chrome.
  EXPECT_TRUE(base::Contains(pinned_apps_strs, app_constants::kLacrosAppId));
  EXPECT_FALSE(base::Contains(pinned_apps_strs, app_constants::kChromeAppId));
}

TEST_F(ChromeShelfPrefsTest, PinMallBeforeDefaultApps) {
  std::string second_pin_app_id;
  {
    std::vector<std::string> pinned_apps_strs = GetPinnedAppIds();
    second_pin_app_id = pinned_apps_strs[1];
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kCrosMall},
        /*disabled_features=*/{chromeos::features::kCrosMallSwa});

    std::vector<std::string> pinned_apps_strs = GetPinnedAppIds();
    EXPECT_EQ(pinned_apps_strs[1], web_app::kMallAppId);
    // Mall should have pushed back any default apps.
    EXPECT_EQ(pinned_apps_strs[2], second_pin_app_id);
  }
}

TEST_F(ChromeShelfPrefsTest, PinMallSystemApp) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kCrosMall,
                            chromeos::features::kCrosMallSwa},
      /*disabled_features=*/{});

  std::string second_pin_app_id;
  {
    std::vector<std::string> pinned_apps_strs = GetPinnedAppIds();
    second_pin_app_id = pinned_apps_strs[1];
    // Mall should not be pinned unless it is installed.
    EXPECT_NE(second_pin_app_id, ash::kMallSystemAppId);
  }

  apps::AppPtr app = std::make_unique<apps::App>(apps::AppType::kSystemWeb,
                                                 ash::kMallSystemAppId);
  app->readiness = apps::Readiness::kReady;
  app->install_reason = apps::InstallReason::kSystem;
  InstallApp(std::move(app));

  {
    std::vector<std::string> pinned_apps_strs = GetPinnedAppIds();
    EXPECT_EQ(pinned_apps_strs[1], ash::kMallSystemAppId);
    // Mall should have pushed back any default apps.
    EXPECT_EQ(pinned_apps_strs[2], second_pin_app_id);
  }
}

TEST_F(ChromeShelfPrefsTest, PinMallSystemAppOnceOnly) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kCrosMall,
                            chromeos::features::kCrosMallSwa},
      /*disabled_features=*/{});

  apps::AppPtr app = std::make_unique<apps::App>(apps::AppType::kSystemWeb,
                                                 ash::kMallSystemAppId);
  app->readiness = apps::Readiness::kReady;
  app->install_reason = apps::InstallReason::kSystem;
  InstallApp(std::move(app));

  std::vector<std::string> pinned_apps_strs = GetPinnedAppIds();
  EXPECT_EQ(pinned_apps_strs[1], ash::kMallSystemAppId);

  shelf_prefs_->RemovePinPosition(ash::ShelfID(ash::kMallSystemAppId));

  // The Mall app must not reappear in the pinned apps list.
  EXPECT_THAT(GetPinnedAppIds(),
              testing::Not(testing::Contains(ash::kMallSystemAppId)));
}

TEST_F(ChromeShelfPrefsTest, PinPreloadApps) {
  apps::PackageId chrome = *apps::PackageId::FromString(
      "chromeapp:" + std::string(app_constants::kChromeAppId));
  apps::PackageId gmail = *apps::PackageId::FromString(
      "web:https://mail.google.com/mail/?usp=installed_webapp");
  apps::PackageId youtube =
      *apps::PackageId::FromString("web:https://www.youtube.com/?feature=ytca");
  apps::PackageId app1 = *apps::PackageId::FromString("chromeapp:app1");
  apps::PackageId app2 = *apps::PackageId::FromString("chromeapp:app2");
  apps::PackageId app3 = *apps::PackageId::FromString("chromeapp:app3");
  // App4 is not listed in ShelfConfig and should be added to the end.
  apps::PackageId app4 = *apps::PackageId::FromString("chromeapp:app4");
  // App5 should not get pinned.
  apps::PackageId app5 = *apps::PackageId::FromString("chromeapp:app5");

  std::vector<apps::PackageId> apps_to_pin({app1, app2, app3, app4});
  // Intentionally switch order of gmail and youtube.
  std::vector<apps::PackageId> pin_order(
      {app4, chrome, app1, app2, youtube, gmail, app3});

  // Register chrome app and some other default pins as installed
  InstallApp(chrome);
  InstallApp(gmail);
  InstallApp(youtube);

  EXPECT_EQ(GetPinned(),
            base::StrCat(
                {"chrome, ", IsGoogleChromeBranded() ? "container, " : "",
                 "gmail, cal, files, messages, meet, play, youtube, photos"}));

  // Simulate installation finishing in unpredictable order.
  // Install app2, comes after chrome since app1 is not installed yet.
  InstallApp(app2);

  // Install app4 which will go before chrome.
  InstallApp(app4);

  // Installed apps (app2 and app4) should pin immediately.
  shelf_prefs_->OnGetPinPreloadApps(apps_to_pin, pin_order);
  EXPECT_EQ(
      GetPinned(),
      base::StrCat(
          {"app4, chrome, app2, ", IsGoogleChromeBranded() ? "container, " : "",
           "gmail, cal, files, messages, meet, play, youtube, photos"}));

  // Install app3, comes after gmail.
  InstallApp(app3);
  EXPECT_EQ(
      GetPinned(),
      base::StrCat(
          {"app4, chrome, app2, ", IsGoogleChromeBranded() ? "container, " : "",
           "gmail, app3, cal, files, messages, meet, play, youtube, photos"}));

  // Install app5, which should not get pinned since it is not in first list.
  InstallApp(app5);
  EXPECT_EQ(
      GetPinned(),
      base::StrCat(
          {"app4, chrome, app2, ", IsGoogleChromeBranded() ? "container, " : "",
           "gmail, app3, cal, files, messages, meet, play, youtube, photos"}));

  // Install app1, comes after chrome.
  InstallApp(app1);
  EXPECT_EQ(
      GetPinned(),
      base::StrCat(
          {"app4, chrome, app1, app2, ",
           IsGoogleChromeBranded() ? "container, " : "",
           "gmail, app3, cal, files, messages, meet, play, youtube, photos"}));
}

TEST_F(ChromeShelfPrefsTest, PinPreloadRepeats) {
  apps::PackageId chrome = *apps::PackageId::FromString(
      "chromeapp:" + std::string(app_constants::kChromeAppId));
  apps::PackageId app1 = *apps::PackageId::FromString("chromeapp:app1");
  apps::PackageId app2 = *apps::PackageId::FromString("chromeapp:app2");
  apps::PackageId app3 = *apps::PackageId::FromString("chromeapp:app3");
  InstallApp(chrome);

  std::vector<apps::PackageId> pin_order({app1, app2, app3, chrome});
  std::string default_apps = base::StrCat(
      {"chrome, ", IsGoogleChromeBranded() ? "container, " : "",
       "gmail, cal, files, messages, meet, play, youtube, photos"});

  // Request to pin app1, and app2, but only install app1.
  shelf_prefs_->OnGetPinPreloadApps({app1, app2}, pin_order);
  InstallApp(app1);
  EXPECT_EQ(GetPinned(), "app1, " + default_apps);

  // Pin should continue if it is called again before it is complete.
  shelf_prefs_->OnGetPinPreloadApps({app2}, pin_order);
  InstallApp(app2);
  EXPECT_EQ(GetPinned(), "app1, app2, " + default_apps);

  // Pin should only run once per user once it completes, app3 should not pin.
  shelf_prefs_->OnGetPinPreloadApps({app3}, pin_order);
  InstallApp(app3);
  EXPECT_EQ(GetPinned(), "app1, app2, " + default_apps);
}

TEST_F(ChromeShelfPrefsTest, PinPreloadEmpty) {
  apps::PackageId chrome = *apps::PackageId::FromString(
      "chromeapp:" + std::string(app_constants::kChromeAppId));
  apps::PackageId app1 = *apps::PackageId::FromString("chromeapp:app1");
  InstallApp(chrome);
  EXPECT_EQ(GetPinned(),
            base::StrCat(
                {"chrome, ", IsGoogleChromeBranded() ? "container, " : "",
                 "gmail, cal, files, messages, meet, play, youtube, photos"}));
  auto get_prefs = [&]() {
    return profile_->GetPrefs()
        ->GetList(prefs::kShelfDefaultPinLayoutRolls)
        .DebugString();
  };

  std::vector<apps::PackageId> pin_order({app1, chrome});

  // Pin should be considered complete if it is requested to pin no apps.
  EXPECT_FALSE(shelf_prefs_->DidAddPreloadApps());
  EXPECT_EQ(get_prefs(), "[ \"default\" ]\n");
  shelf_prefs_->OnGetPinPreloadApps({}, pin_order);
  EXPECT_TRUE(shelf_prefs_->DidAddPreloadApps());
  EXPECT_EQ(get_prefs(), "[ \"default\", \"preload\" ]\n");
  shelf_prefs_->OnGetPinPreloadApps({app1}, pin_order);
  InstallApp(app1);
  EXPECT_EQ(GetPinned(),
            base::StrCat(
                {"chrome, ", IsGoogleChromeBranded() ? "container, " : "",
                 "gmail, cal, files, messages, meet, play, youtube, photos"}));

  // Further calls to OnGetPinPreloadApps() should not write additional values
  // of 'preload' to prefs (crbug.com/350769496).
  shelf_prefs_->OnGetPinPreloadApps({}, pin_order);
  EXPECT_TRUE(shelf_prefs_->DidAddPreloadApps());
  EXPECT_EQ(get_prefs(), "[ \"default\", \"preload\" ]\n");
}

// Cleanup duplicate values of 'preload' in prefs (crbug.com/350769496).
TEST_F(ChromeShelfPrefsTest, CleanupPreloadPrefs) {
  PrefService* prefs = profile_->GetPrefs();
  std::vector<std::string> pref_names = {
      prefs::kShelfDefaultPinLayoutRolls,
      prefs::kShelfDefaultPinLayoutRollsForTabletFormFactor};

  const struct {
    std::vector<std::string> pref_list;
    std::string expected;
  } tests[] = {
      {{}, R"([  ])"},
      {{"default"}, R"([ "default" ])"},
      {{"default", "preload"}, R"([ "default", "preload" ])"},
      {{"default", "preload", "preload"}, R"([ "default", "preload" ])"},
      {{"preload", "default", "preload"}, R"([ "default", "preload" ])"},
      {{"preload"}, R"([ "preload" ])"},
      {{"preload", "preload"}, R"([ "preload" ])"},
  };

  for (const auto& test : tests) {
    for (const auto& pref_name : pref_names) {
      base::Value::List list;
      for (const auto& item : test.pref_list) {
        list.Append(item);
      }
      prefs->SetList(pref_name, std::move(list));
      ChromeShelfPrefs::CleanupPreloadPrefs(prefs);
      EXPECT_EQ(test.expected + "\n", prefs->GetList(pref_name).DebugString());
    }
  }
}
