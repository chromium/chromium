// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_storage/app_storage.h"

#include <memory>
#include <set>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_storage/app_storage_file_handler.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

namespace {

constexpr char kAppId1[] = "aaa";
constexpr char kAppId2[] = "bbb";
constexpr char kAppId3[] = "ccc";

constexpr AppType kAppType1 = AppType::kArc;
constexpr AppType kAppType2 = AppType::kWeb;

constexpr char kAppName1[] = "AAA";
constexpr char kAppName2[] = "BBB";

constexpr Readiness kReadiness1 = Readiness::kReady;
constexpr Readiness kReadiness2 = Readiness::kDisabledByUser;

constexpr char kAppShortName1[] = "a";
constexpr char kAppShortName2[] = "b";

}  // namespace

#define MODIFY_FIELD(FIELD, VALUE)                                   \
  void Modify##FIELD() {                                             \
    AppPtr app = std::make_unique<App>(kAppType1, kAppId1);          \
    app->FIELD = VALUE;                                              \
    std::vector<AppPtr> apps;                                        \
    apps.push_back(std::move(app));                                  \
    app_registry_cache_.OnApps(std::move(apps), kAppType1,           \
                               /*should_notify_initialized=*/false); \
  }

#define VERIFY_MODIFY_FIELD(FIELD, VALUE)                     \
  Modify##FIELD();                                            \
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/2); \
  apps[0]->FIELD = VALUE;                                     \
  VerifySavedApps(apps);

// This fake AppStorage is used to test and track all calls to AppStorage.
class FakeAppStorage : public AppStorage {
 public:
  FakeAppStorage(const base::FilePath& base_path,
                 AppRegistryCache& app_registry_cache,
                 std::unique_ptr<base::RunLoop> run_loop)
      : AppStorage(base_path, app_registry_cache, run_loop->QuitClosure()) {
    // Wait for OnGetAppInfoData to be invoked.
    run_loop->Run();
  }

  FakeAppStorage(const FakeAppStorage&) = delete;
  FakeAppStorage& operator=(const FakeAppStorage&) = delete;

  ~FakeAppStorage() override = default;

  std::vector<AppPtr>& GetAppInfo() { return apps_; }
  std::set<AppType>& GetAppTypeInfo() { return app_types_; }
  bool is_app_changed() { return is_app_changed_; }

  void WaitForSaveFinished(size_t expect_app_count) {
    expect_app_count_ = expect_app_count;
    write_result_ = std::make_unique<base::test::TestFuture<void>>();
    EXPECT_TRUE(write_result_->Wait());
  }

 private:
  // Override to call to AppStorage::OnAppUpdate.
  void OnAppUpdate(const apps::AppUpdate& update) override {
    is_app_changed_ = IsAppChanged(update);
    AppStorage::OnAppUpdate(update);
  }

  // Override to call to AppStorage::OnGetAppInfoData.
  void OnGetAppInfoData(base::OnceCallback<void()> callback,
                        std::unique_ptr<AppInfo> app_info) override {
    if (app_info) {
      for (const auto& app : app_info->apps) {
        apps_.push_back(app->Clone());
      }
    }

    AppStorage::OnGetAppInfoData(std::move(callback), std::move(app_info));
  }

  void OnSaveFinished() override {
    AppStorage::OnSaveFinished();

    if (!io_in_progress_ && expect_app_count_ >= 0 &&
        expect_app_count_ == app_registry_cache_->states_.size()) {
      std::move(write_result_->GetCallback()).Run();
    }
  }

  void OnAppTypeInitialized(apps::AppType app_type) override {
    app_types_.insert(app_type);
  }

  std::vector<AppPtr> apps_;
  std::set<AppType> app_types_;

  std::unique_ptr<base::test::TestFuture<void>> write_result_;
  size_t expect_app_count_ = -1;

  bool is_app_changed_ = false;
};

class AppStorageTest : public testing::Test {
 public:
  AppStorageTest() = default;
  ~AppStorageTest() override = default;

  AppStorageTest(const AppStorageTest&) = delete;
  AppStorageTest& operator=(const AppStorageTest&) = delete;

  void SetUp() override { ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir()); }

  void CreateAppStorage() {
    app_storage_ = std::make_unique<FakeAppStorage>(
        tmp_dir_.GetPath(), app_registry_cache_,
        std::make_unique<base::RunLoop>());
  }

  AppPtr CreateApp(AppType app_type, const std::string& app_id) {
    AppPtr app = std::make_unique<App>(app_type, app_id);
    app->readiness = kReadiness1;
    app->has_badge = false;
    app->paused = false;
    return app;
  }

  std::vector<AppPtr> CreateOneApp(AppType app_type,
                                   const std::string& app_id) {
    AppPtr app = CreateApp(app_type, app_id);
    std::vector<AppPtr> apps;
    apps.push_back(std::move(app));
    return apps;
  }

  std::vector<AppPtr> CreateTwoApps() {
    std::vector<AppPtr> apps;

    AppPtr app1 = CreateApp(kAppType1, kAppId1);
    app1->name = kAppName1;
    apps.push_back(std::move(app1));

    AppPtr app2 = CreateApp(kAppType2, kAppId2);
    app2->readiness = kReadiness2;
    app2->name = kAppName2;
    app2->short_name = kAppShortName1;
    app2->publisher_id = "publisher_id";
    app2->description = "description";
    app2->version = "version";
    app2->additional_search_terms = {"term1", "term2"};
    app2->icon_key =
        IconKey(/*resource_id=*/65535, IconEffects::kCrOsStandardIcon);
    app2->last_launch_time = base::Time() + base::Days(2);
    app2->install_time = base::Time() + base::Days(1);

    app2->permissions.push_back(std::make_unique<Permission>(
        PermissionType::kLocation, /*PermissionValue=*/false,
        /*is_managed=*/true, "details"));
    app2->permissions.push_back(std::make_unique<Permission>(
        PermissionType::kPrinting, /*PermissionValue=*/TriState::kBlock,
        /*is_managed=*/false));

    app2->install_reason = InstallReason::kUser;
    app2->install_source = InstallSource::kBrowser;
    app2->policy_ids = {"plicy1", "policy2"};
    app2->is_platform_app = false;
    app2->recommendable = true;
    app2->searchable = true;
    app2->show_in_launcher = true;
    app2->show_in_shelf = true;
    app2->show_in_search = true;
    app2->show_in_management = true;
    app2->handles_intents = false;
    app2->allow_uninstall = false;
    app2->intent_filters.push_back(apps_util::MakeIntentFilterForUrlScope(
        GURL("https://www.google.com/abc")));
    app2->window_mode = WindowMode::kBrowser;
    app2->run_on_os_login = RunOnOsLogin(RunOnOsLoginMode::kNotRun,
                                         /*is_managed=*/false);
    app2->allow_close = false;
    app2->app_size_in_bytes = 1024;
    app2->data_size_in_bytes = 2048;
    app2->supported_locales = {"a", "b", "c"};
    app2->selected_locale = "a";
    app2->SetExtraField("vm_name", "vm_name_value");
    app2->SetExtraField("scales", true);
    app2->SetExtraField("number", 100);

    apps.push_back(std::move(app2));

    // TODO(crbug.com/40247021): Add other files in the App structure.
    return apps;
  }

  MODIFY_FIELD(name, kAppName2)
  MODIFY_FIELD(short_name, kAppShortName2)
  MODIFY_FIELD(description, "description")
  MODIFY_FIELD(publisher_id, "publisher_id")
  MODIFY_FIELD(version, "version")
  MODIFY_FIELD(additional_search_terms, {"term1"})
  MODIFY_FIELD(last_launch_time, base::Time() + base::Days(2))
  MODIFY_FIELD(install_time, base::Time() + base::Days(1))
  MODIFY_FIELD(install_reason, InstallReason::kDefault)
  MODIFY_FIELD(install_source, InstallSource::kSync)
  MODIFY_FIELD(policy_ids, {"policy"})
  MODIFY_FIELD(is_platform_app, true)
  MODIFY_FIELD(recommendable, false)
  MODIFY_FIELD(searchable, false)
  MODIFY_FIELD(show_in_launcher, true)
  MODIFY_FIELD(show_in_shelf, true)
  MODIFY_FIELD(show_in_search, true)
  MODIFY_FIELD(show_in_management, true)
  MODIFY_FIELD(handles_intents, false)
  MODIFY_FIELD(allow_uninstall, false)
  MODIFY_FIELD(window_mode, WindowMode::kWindow)
  MODIFY_FIELD(run_on_os_login,
               RunOnOsLogin(RunOnOsLoginMode::kNotRun, /*is_managed=*/false))
  MODIFY_FIELD(allow_close, false)
  MODIFY_FIELD(app_size_in_bytes, 512)
  MODIFY_FIELD(data_size_in_bytes, 0)
  MODIFY_FIELD(supported_locales, {"aa"})
  MODIFY_FIELD(selected_locale, "aa")

  void ModifyIconKey(std::optional<IconKey> icon_key) {
    AppPtr app = std::make_unique<App>(kAppType1, kAppId1);
    app->icon_key = std::move(icon_key);
    std::vector<AppPtr> apps;
    apps.push_back(std::move(app));
    app_registry_cache_.OnApps(std::move(apps), kAppType1,
                               /*should_notify_initialized=*/false);
  }

  void ModifyPermissions() {
    AppPtr app = std::make_unique<App>(kAppType1, kAppId1);
    app->permissions.push_back(std::make_unique<Permission>(
        PermissionType::kLocation, /*PermissionValue=*/false,
        /*is_managed=*/true, "details"));
    std::vector<AppPtr> apps;
    apps.push_back(std::move(app));
    app_registry_cache_.OnApps(std::move(apps), kAppType1,
                               /*should_notify_initialized=*/false);
  }

  void ModifyIntentFilters() {
    AppPtr app = std::make_unique<App>(kAppType1, kAppId1);
    auto intent_filter = std::make_unique<apps::IntentFilter>();
    intent_filter->activity_name = "activity_name";
    app->intent_filters.push_back(std::move(intent_filter));
    std::vector<AppPtr> apps;
    apps.push_back(std::move(app));
    app_registry_cache_.OnApps(std::move(apps), kAppType1,
                               /*should_notify_initialized=*/false);
  }

  void ModifyExtra() {
    AppPtr app = std::make_unique<App>(kAppType1, kAppId1);
    app->SetExtraField("vm_name", "vm_name_value");
    std::vector<AppPtr> apps;
    apps.push_back(std::move(app));
    app_registry_cache_.OnApps(std::move(apps), kAppType1,
                               /*should_notify_initialized=*/false);
  }

  void RemoveOneApp(AppType app_type, const std::string& app_id) {
    AppPtr app = std::make_unique<App>(app_type, app_id);
    app->readiness = Readiness::kUninstalledByUser;
    std::vector<AppPtr> apps;
    apps.push_back(std::move(app));
    app_registry_cache_.OnApps(std::move(apps), app_type,
                               /*should_notify_initialized=*/false);
  }

  void VerifySavedApps(std::vector<AppPtr>& apps,
                       bool should_verify_app_type_init = false) {
    // Create a new AppStorage to read the AppStorage file to verify the app has
    // been written correctly.
    app_storage_.reset();
    app_storage_ = std::make_unique<FakeAppStorage>(
        tmp_dir().GetPath(), app_registry_cache_,
        std::make_unique<base::RunLoop>());
    EXPECT_TRUE(IsEqual(apps, app_storage_->GetAppInfo()));

    if (!should_verify_app_type_init) {
      return;
    }

    std::set<AppType> app_types;
    for (const auto& app : apps) {
      app_types.insert(app->app_type);
    }

    EXPECT_EQ(app_types.size(), app_storage_->GetAppTypeInfo().size());
    for (AppType app_type : app_types) {
      EXPECT_TRUE(base::Contains(app_storage_->GetAppTypeInfo(), app_type));
    }
  }

  void OnApps(std::vector<AppPtr> deltas,
              apps::AppType app_type,
              bool should_notify_initialized) {
    app_registry_cache_.OnApps(std::move(deltas), app_type,
                               should_notify_initialized);
  }

  const base::ScopedTempDir& tmp_dir() { return tmp_dir_; }

  AppRegistryCache& app_registry_cache() { return app_registry_cache_; }

  FakeAppStorage* app_storage() { return app_storage_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir tmp_dir_;

  AppRegistryCache app_registry_cache_;
  std::unique_ptr<FakeAppStorage> app_storage_;
};

// Test AppStorage can work from an unavailable file.
TEST_F(AppStorageTest, ReadFromNotValidFile) {
  CreateAppStorage();
  EXPECT_TRUE(app_storage()->GetAppInfo().empty());
  EXPECT_TRUE(app_storage()->GetAppTypeInfo().empty());
}

// Test AppStorage can work when there is no app info in the AppStorage file.
TEST_F(AppStorageTest, ReadFromEmptyFile) {
  // Create an empty AppStorage file.
  scoped_refptr<AppStorageFileHandler> file_handler =
      base::MakeRefCounted<AppStorageFileHandler>(tmp_dir().GetPath());
  ASSERT_TRUE(base::CreateDirectory(file_handler->GetFilePath().DirName()));
  ASSERT_TRUE(base::WriteFile(file_handler->GetFilePath(), ""));

  CreateAppStorage();
  EXPECT_TRUE(app_storage()->GetAppInfo().empty());
  EXPECT_TRUE(app_storage()->GetAppTypeInfo().empty());
}

// Test AppStorageTest can read and write one app.
TEST_F(AppStorageTest, ReadAndWriteOneApp) {
  CreateAppStorage();
  EXPECT_TRUE(app_storage()->GetAppInfo().empty());

  // Add 1 app.
  OnApps(CreateOneApp(kAppType1, kAppId1), kAppType1,
         /*should_notify_initialized=*/false);
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/1);

  // Verify the app is saved correctly.
  auto apps1 = CreateOneApp(kAppType1, kAppId1);
  VerifySavedApps(apps1, /*should_verify_app_type_init=*/true);

  // Remove the app.
  RemoveOneApp(kAppType1, kAppId1);
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/1);

  // Verify the app has been removed.
  std::vector<AppPtr> apps2;
  VerifySavedApps(apps2);
}

// Test AppStorageTest can read and write multiple apps.
TEST_F(AppStorageTest, ReadAndWriteMultipleApps) {
  CreateAppStorage();
  EXPECT_TRUE(app_storage()->GetAppInfo().empty());

  // Add 2 apps.
  OnApps(CreateTwoApps(), AppType::kUnknown,
         /*should_notify_initialized=*/false);
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/2);

  // Verify the apps are saved correctly.
  auto apps = CreateTwoApps();
  VerifySavedApps(apps, /*should_verify_app_type_init=*/true);

  VERIFY_MODIFY_FIELD(name, kAppName2);
  VERIFY_MODIFY_FIELD(short_name, kAppShortName2);
  VERIFY_MODIFY_FIELD(publisher_id, "publisher_id");
  VERIFY_MODIFY_FIELD(description, "description");
  VERIFY_MODIFY_FIELD(version, "version");
  VERIFY_MODIFY_FIELD(additional_search_terms, {"term1"});
  VERIFY_MODIFY_FIELD(last_launch_time, base::Time() + base::Days(2));
  VERIFY_MODIFY_FIELD(install_time, base::Time() + base::Days(1));
  VERIFY_MODIFY_FIELD(install_reason, InstallReason::kDefault);
  VERIFY_MODIFY_FIELD(install_source, InstallSource::kSync);
  VERIFY_MODIFY_FIELD(policy_ids, {"policy"});
  VERIFY_MODIFY_FIELD(is_platform_app, true);
  VERIFY_MODIFY_FIELD(recommendable, false);
  VERIFY_MODIFY_FIELD(searchable, false);
  VERIFY_MODIFY_FIELD(show_in_launcher, true);
  VERIFY_MODIFY_FIELD(show_in_shelf, true);
  VERIFY_MODIFY_FIELD(show_in_search, true);
  VERIFY_MODIFY_FIELD(show_in_management, true);
  VERIFY_MODIFY_FIELD(handles_intents, false);
  VERIFY_MODIFY_FIELD(allow_uninstall, false);
  VERIFY_MODIFY_FIELD(window_mode, WindowMode::kWindow);
  VERIFY_MODIFY_FIELD(run_on_os_login, RunOnOsLogin(RunOnOsLoginMode::kNotRun,
                                                    /*is_managed=*/false));
  VERIFY_MODIFY_FIELD(allow_close, false);
  VERIFY_MODIFY_FIELD(app_size_in_bytes, 512);
  VERIFY_MODIFY_FIELD(data_size_in_bytes, 0);
  VERIFY_MODIFY_FIELD(supported_locales, {"aa"});
  VERIFY_MODIFY_FIELD(selected_locale, "aa");

  // Verify for the none icon effect.
  IconKey icon_key1(IconEffects::kNone);
  ModifyIconKey(std::move(*(icon_key1.Clone())));
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/2);
  apps[0]->icon_key = std::move(icon_key1);
  VerifySavedApps(apps);

  // Verify the icon effect modification.
  IconKey icon_key2(IconEffects::kCrOsStandardIcon);
  ModifyIconKey(std::move(*(icon_key2.Clone())));
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/2);
  apps[0]->icon_key = std::move(icon_key2);
  VerifySavedApps(apps);

  // Verify the kPaused icon effect can be filtered out. We don't need to modify
  // `apps`, because the icon key won't be updated, and we don't need to wait
  // for the saving as well. After modifying the intent filters, the apps can be
  // checked again.
  IconKey icon_key3(IconEffects::kCrOsStandardIcon | IconEffects::kPaused);
  ModifyIconKey(std::move(icon_key3));
  EXPECT_FALSE(app_storage()->is_app_changed());

  // Verify the icon_key's `update_version` can be filtered out. We don't need
  // to modify `apps`, because `update_version` won't be saved,. After modifying
  // the intent filters, the apps can be checked again.
  IconKey icon_key4(IconEffects::kCrOsStandardIcon);
  icon_key4.update_version = true;
  ModifyIconKey(std::move(icon_key4));
  EXPECT_FALSE(app_storage()->is_app_changed());

  ModifyPermissions();
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/2);
  apps[0]->permissions.push_back(std::make_unique<Permission>(
      PermissionType::kLocation, /*PermissionValue=*/false,
      /*is_managed=*/true, "details"));
  VerifySavedApps(apps);

  ModifyIntentFilters();
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/2);
  auto intent_filter = std::make_unique<apps::IntentFilter>();
  intent_filter->activity_name = "activity_name";
  apps[0]->intent_filters.push_back(std::move(intent_filter));
  VerifySavedApps(apps);

  ModifyExtra();
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/2);
  apps[0]->SetExtraField("vm_name", "vm_name_value");
  VerifySavedApps(apps);

  // Verify `apps` are not changed.
  AppPtr app = std::make_unique<App>(kAppType1, kAppId1);
  // Set `paused` as true to call `OnAppUpdate`.
  app->paused = true;
  std::vector<AppPtr> deltas;
  deltas.push_back(std::move(app));
  OnApps(std::move(deltas), AppType::kUnknown,
         /*should_notify_initialized=*/false);
  EXPECT_FALSE(app_storage()->is_app_changed());
  VerifySavedApps(apps);

  RemoveOneApp(kAppType1, kAppId1);
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/2);

  // Verify the apps are saved correctly.
  apps.erase(apps.begin());
  VerifySavedApps(apps);

  RemoveOneApp(kAppType2, kAppId2);
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/2);

  apps.clear();
  VerifySavedApps(apps);
}

// Test AppStorageTest can read and write multiple apps at the same time without
// waiting for writing finish.
TEST_F(AppStorageTest, ReadAndWriteMultipleAppsAtSameTime) {
  CreateAppStorage();
  EXPECT_TRUE(app_storage()->GetAppInfo().empty());

  // Add apps.
  OnApps(CreateOneApp(kAppType1, kAppId1), kAppType1,
         /*should_notify_initialized=*/false);
  OnApps(CreateOneApp(kAppType1, kAppId2), kAppType1,
         /*should_notify_initialized=*/false);
  OnApps(CreateOneApp(kAppType1, kAppId3), kAppType1,
         /*should_notify_initialized=*/false);

  Modifyname();
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/3);

  // Verify the apps are saved correctly.
  auto apps1 = CreateOneApp(kAppType1, kAppId1);
  apps1[0]->name = kAppName2;
  auto apps2 = CreateOneApp(kAppType1, kAppId2);
  auto apps3 = CreateOneApp(kAppType1, kAppId3);
  std::vector<AppPtr> apps;
  apps.push_back(std::move(apps1[0]));
  apps.push_back(std::move(apps2[0]));
  apps.push_back(std::move(apps3[0]));
  VerifySavedApps(apps, /*should_verify_app_type_init=*/true);
}

// Test AppStorageTest can handle the removed app case(kRemoved).
TEST_F(AppStorageTest, AddAndRemoveApp) {
  CreateAppStorage();
  EXPECT_TRUE(app_storage()->GetAppInfo().empty());

  // Add 1 app.
  OnApps(CreateOneApp(kAppType1, kAppId1), kAppType1,
         /*should_notify_initialized=*/false);
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/1);

  // Verify the app is saved correctly.
  auto apps1 = CreateOneApp(kAppType1, kAppId1);
  VerifySavedApps(apps1, /*should_verify_app_type_init=*/true);

  // Remove the app.
  std::vector<AppPtr> apps2;
  RemoveOneApp(kAppType1, kAppId1);
  AppPtr app1 = std::make_unique<App>(kAppType1, kAppId1);
  app1->readiness = Readiness::kUninstalledByUser;
  apps2.push_back(std::move(app1));
  AppPtr app2 = std::make_unique<App>(kAppType1, kAppId1);
  app2->readiness = Readiness::kRemoved;
  apps2.push_back(std::move(app2));

  // Add the app back again.
  AppPtr app3 = std::make_unique<App>(kAppType1, kAppId1);
  app3->readiness = kReadiness1;
  apps2.push_back(std::move(app3));
  OnApps(std::move(apps2), kAppType1,
         /*should_notify_initialized=*/false);
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/1);

  // Verify the app has been added.
  auto apps3 = CreateOneApp(kAppType1, kAppId1);
  VerifySavedApps(apps3);
}

// Test AppStorageTest can remove the app when the app is uninstalled.
TEST_F(AppStorageTest, UninstallApp) {
  CreateAppStorage();
  EXPECT_TRUE(app_storage()->GetAppInfo().empty());

  // Add 1 app.
  OnApps(CreateOneApp(kAppType1, kAppId1), kAppType1,
         /*should_notify_initialized=*/false);
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/1);

  // Verify the app is saved correctly.
  auto apps1 = CreateOneApp(kAppType1, kAppId1);
  VerifySavedApps(apps1, /*should_verify_app_type_init=*/true);

  // Uninstall the app.
  RemoveOneApp(kAppType1, kAppId1);
  // Though the app is uninstalled, it is still saved in `app_registry_cache_`,
  // so `expect_app_count` should be 1. But the apps saved in the AppStorage
  // file should be empty, so we verify that with an empty vector, `apps2`.
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/1);
  std::vector<AppPtr> apps2;
  VerifySavedApps(apps2);

  // Reinstall the app again and change the app type to simulate the system
  // restarts.
  app_registry_cache().ReinitializeForTesting();
  OnApps(CreateOneApp(kAppType2, kAppId1), kAppType2,
         /*should_notify_initialized=*/true);
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/1);

  auto apps3 = CreateOneApp(kAppType2, kAppId1);
  VerifySavedApps(apps3);
}

// Test AppStorageTest can remove the app when calling OnAppsInitialized to
// clear uninstalled apps.
TEST_F(AppStorageTest, ClearUnInstalledApps) {
  CreateAppStorage();
  EXPECT_TRUE(app_storage()->GetAppInfo().empty());

  // Add 2 apps.
  {
    std::vector<AppPtr> apps;
    apps.push_back(CreateApp(kAppType1, kAppId1));
    apps.push_back(CreateApp(kAppType1, kAppId2));
    AppPtr app = std::make_unique<App>(kAppType1, kAppId1);
    app->name = kAppName2;
    apps.push_back(std::move(app));
    OnApps(std::move(apps), AppType::kUnknown,
           /*should_notify_initialized=*/false);
    app_storage()->WaitForSaveFinished(/*expect_app_count=*/2);
  }

  // Verify the apps are saved correctly.
  {
    std::vector<AppPtr> apps;
    AppPtr app = CreateApp(kAppType1, kAppId1);
    app->name = kAppName2;
    apps.push_back(std::move(app));
    apps.push_back(CreateApp(kAppType1, kAppId2));
    VerifySavedApps(apps, /*should_verify_app_type_init=*/true);
  }

  // Add 1 app only to uninstall `kAppId2`.
  auto apps1 = CreateOneApp(kAppType1, kAppId1);
  apps1[0]->name = kAppName1;
  OnApps(std::move(apps1), kAppType1,
         /*should_notify_initialized=*/true);
  app_registry_cache().ForOneApp(kAppId2, [&](const apps::AppUpdate& update) {
    EXPECT_FALSE(apps_util::IsInstalled(update.Readiness()));
  });
  // The uninstalled app is still in AppRegistryCache, so the app count is 2.
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/2);

  // Verify `kAppId2` has been removed from AppStorage.
  auto apps2 = CreateOneApp(kAppType1, kAppId1);
  apps2[0]->name = kAppName1;
  app_registry_cache().ReinitializeForTesting();
  VerifySavedApps(apps2);

  // Publish the empty app list to uninstall `kAppId1`.
  OnApps(std::vector<AppPtr>{}, kAppType1,
         /*should_notify_initialized=*/true);
  app_registry_cache().ForOneApp(kAppId1, [&](const apps::AppUpdate& update) {
    EXPECT_FALSE(apps_util::IsInstalled(update.Readiness()));
  });
  // The uninstalled app is still in AppRegistryCache, so the app count is 2.
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/1);

  // Verify `kAppId1` has been removed from AppStorage.
  app_registry_cache().ReinitializeForTesting();
  std::vector<AppPtr> apps3;
  VerifySavedApps(apps3);
}

TEST_F(AppStorageTest, ClearUnInstalledMultipleApps) {
  CreateAppStorage();
  EXPECT_TRUE(app_storage()->GetAppInfo().empty());

  // Add 3 apps.
  {
    std::vector<AppPtr> apps;
    apps.push_back(CreateApp(kAppType1, kAppId1));
    apps.push_back(CreateApp(kAppType1, kAppId2));
    AppPtr app = std::make_unique<App>(kAppType1, kAppId1);
    app->name = kAppName2;
    apps.push_back(std::move(app));
    apps.push_back(CreateApp(kAppType1, kAppId3));
    OnApps(std::move(apps), kAppType1,
           /*should_notify_initialized=*/false);
    app_storage()->WaitForSaveFinished(/*expect_app_count=*/3);
  }

  // Verify the apps are saved correctly.
  {
    std::vector<AppPtr> apps;
    AppPtr app = CreateApp(kAppType1, kAppId1);
    app->name = kAppName2;
    apps.push_back(std::move(app));
    apps.push_back(CreateApp(kAppType1, kAppId2));
    apps.push_back(CreateApp(kAppType1, kAppId3));
    VerifySavedApps(apps, /*should_verify_app_type_init=*/true);
  }

  // Call ClearUnInstalledApps to uninstall `kAppId2`, `kAppId3`.
  std::vector<AppPtr> apps1;
  AppPtr app1 = CreateApp(kAppType1, kAppId1);
  app1->name = kAppName2;
  apps1.push_back(std::move(app1));
  OnApps(std::move(apps1), kAppType1,
         /*should_notify_initialized=*/true);
  app_registry_cache().ForOneApp(kAppId2, [&](const apps::AppUpdate& update) {
    EXPECT_FALSE(apps_util::IsInstalled(update.Readiness()));
  });
  app_registry_cache().ForOneApp(kAppId3, [&](const apps::AppUpdate& update) {
    EXPECT_FALSE(apps_util::IsInstalled(update.Readiness()));
  });
  // The uninstalled app is still in AppRegistryCache, so the app count is 2.
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/3);

  // Verify `kAppId2`, `kAppId3` have been removed from AppStorage.
  app_registry_cache().ReinitializeForTesting();
  std::vector<AppPtr> apps2;
  AppPtr app2 = CreateApp(kAppType1, kAppId1);
  app2->name = kAppName2;
  apps2.push_back(std::move(app2));
  VerifySavedApps(apps2);
  EXPECT_EQ(kAppType1, app_registry_cache().GetAppType(kAppId1));
  EXPECT_EQ(AppType::kUnknown, app_registry_cache().GetAppType(kAppId2));
  EXPECT_EQ(AppType::kUnknown, app_registry_cache().GetAppType(kAppId3));
}

}  // namespace apps
