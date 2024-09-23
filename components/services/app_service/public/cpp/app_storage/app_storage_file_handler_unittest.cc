// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_storage/app_storage_file_handler.h"

#include <limits.h>
#include <memory>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

using AppInfo = AppStorageFileHandler::AppInfo;

namespace {

constexpr char kAppId1[] = "aaa";
constexpr char kAppId2[] = "bbb";

constexpr AppType kAppType1 = AppType::kArc;
constexpr AppType kAppType2 = AppType::kWeb;

constexpr char kAppName1[] = "AAA";
constexpr char kAppName2[] = "BBB";

constexpr char kAppShortName[] = "b";

}  // namespace

class AppStorageFileHandlerTest : public testing::Test {
 public:
  AppStorageFileHandlerTest() = default;
  ~AppStorageFileHandlerTest() override = default;

  AppStorageFileHandlerTest(const AppStorageFileHandlerTest&) = delete;
  AppStorageFileHandlerTest& operator=(const AppStorageFileHandlerTest&) =
      delete;

  void SetUp() override {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    file_handler_ =
        base::MakeRefCounted<AppStorageFileHandler>(tmp_dir_.GetPath());
  }

  // Call AppStorageFileHandler::ReadFromFile to read the app info data from the
  // AppStorage file.
  std::unique_ptr<AppInfo> ReadFromFile() {
    base::test::TestFuture<std::unique_ptr<AppInfo>> result;
    file_handler_->owning_task_runner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&AppStorageFileHandler::ReadFromFile,
                       file_handler_.get()),
        result.GetCallback());
    return result.Take();
  }

  // Call AppStorageFileHandler::WriteToFile to write `apps` to the AppStorage
  // file.
  void WriteToFile(std::vector<AppPtr> apps) {
    base::test::TestFuture<void> result;
    file_handler_->owning_task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&AppStorageFileHandler::WriteToFile, file_handler_.get(),
                       std::move(apps)),
        result.GetCallback());
    EXPECT_TRUE(result.Wait());
  }

  // Call base::WriteFile directly to create the fake AppStorage file. E.g.
  // create the AppStorage file with the wrong JSON format, or the wrong app
  // info data.
  void WriteToFile(const std::string& data) {
    ASSERT_TRUE(base::CreateDirectory(file_handler_->GetFilePath().DirName()));
    ASSERT_TRUE(base::WriteFile(file_handler_->GetFilePath(), data));
  }

  std::vector<AppPtr> CreateOneApp() {
    std::vector<AppPtr> apps;
    AppPtr app = std::make_unique<App>(kAppType1, kAppId1);
    app->has_badge = false;
    app->paused = false;
    apps.push_back(std::move(app));
    return apps;
  }

  std::vector<AppPtr> CreateTwoApps() {
    std::vector<AppPtr> apps;

    AppPtr app1 = std::make_unique<App>(kAppType1, kAppId1);
    app1->readiness = Readiness::kReady;
    app1->name = kAppName1;
    app1->has_badge = false;
    app1->paused = false;
    apps.push_back(std::move(app1));

    AppPtr app2 = std::make_unique<App>(kAppType2, kAppId2);
    app2->readiness = Readiness::kDisabledByUser;
    app2->name = kAppName2;
    app2->short_name = kAppShortName;
    app2->publisher_id = "publisher_id";
    app2->installer_package_id = PackageId(PackageType::kWeb, "publisher_id");
    app2->description = "description";
    app2->version = "version";
    app2->additional_search_terms = {"item1", "item2"};
    app2->icon_key =
        apps::IconKey(/*resource_id=*/65535, apps::IconEffects::kNone);
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
    app2->has_badge = false;
    app2->paused = false;
    app2->intent_filters.push_back(apps_util::MakeIntentFilterForUrlScope(
        GURL("https://www.google.com/abc")));
    app2->window_mode = WindowMode::kBrowser;
    app2->run_on_os_login = RunOnOsLogin(RunOnOsLoginMode::kNotRun,
                                         /*is_managed=*/true);
    app2->allow_close = false;
    app2->app_size_in_bytes = ULLONG_MAX;
    app2->data_size_in_bytes = ULLONG_MAX - 1;
    app2->supported_locales = {"a", "b", "c"};
    app2->selected_locale = "c";
    app2->SetExtraField("vm_name", "vm_name_value");
    app2->SetExtraField("scales", true);
    app2->SetExtraField("number", 100);
    apps.push_back(std::move(app2));

    return apps;
  }

  void VerifyIconKey(IconKey& icon_key) {
    std::vector<AppPtr> apps = CreateOneApp();
    apps[0]->icon_key = std::move(*icon_key.Clone());
    WriteToFile(std::move(apps));
    auto app_info = ReadFromFile();
    ASSERT_TRUE(app_info);
    EXPECT_EQ(1u, app_info->apps.size());
    // Set `update_version` to the init false value.
    icon_key.update_version = false;
    // Clear the kPaused icon effect.
    icon_key.icon_effects = icon_key.icon_effects & (~IconEffects::kPaused);
    EXPECT_EQ(icon_key, app_info->apps[0]->icon_key.value());
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir tmp_dir_;

  scoped_refptr<AppStorageFileHandler> file_handler_;
};

// Test AppStorageFileHandler can work from an unavailable file.
TEST_F(AppStorageFileHandlerTest, ReadFromNotValidFile) {
  auto app_info = ReadFromFile();
  EXPECT_FALSE(app_info);
}

// Test AppStorageFileHandler won't crash when the file is empty.
TEST_F(AppStorageFileHandlerTest, ReadFromEmptyFile) {
  WriteToFile("");
  auto app_info = ReadFromFile();
  EXPECT_FALSE(app_info);
}

// Test AppStorageFileHandler won't crash when the file isn't a json format.
TEST_F(AppStorageFileHandlerTest, ReadFromWrongJSONFile) {
  const char kAppInfoData[] = "\"abc\":{\"type\":5}";
  WriteToFile(kAppInfoData);
  auto app_info = ReadFromFile();
  EXPECT_FALSE(app_info);
}

// Test AppStorageFileHandler can work when the data format isn't correct.
TEST_F(AppStorageFileHandlerTest, ReadFromWrongDataFile) {
  const char kAppInfoData[] =
      "{\"abc\":{}, \"aaa\":{\"type\":2, \"readiness\":100}}";
  WriteToFile(kAppInfoData);
  auto app_info = ReadFromFile();

  // The app type for "abc" is empty, so we can get one app only {app_id =
  // "aaa", app_type = kBuiltIn}.
  ASSERT_TRUE(app_info);
  EXPECT_EQ(1u, app_info->apps.size());
  EXPECT_EQ("aaa", app_info->apps[0]->app_id);
  EXPECT_EQ(AppType::kBuiltIn, app_info->apps[0]->app_type);
  // The readiness for the app "aaa" is wrong, so readiness is set as the
  // default value.
  EXPECT_EQ(Readiness::kUnknown, app_info->apps[0]->readiness);

  EXPECT_EQ(1u, app_info->app_types.size());
  EXPECT_TRUE(base::Contains(app_info->app_types, AppType::kBuiltIn));
}

// Test AppStorageFileHandler can work when the app type isn't correct.
TEST_F(AppStorageFileHandlerTest, ReadFromWrongAppType) {
  const char kAppInfoData[] = "{\"abc\":{\"type\":100}, \"aaa\":{\"type\":2}}";
  WriteToFile(kAppInfoData);
  auto app_info = ReadFromFile();

  // The app type for "abc" is wrong, so we can get one app only {app_id =
  // "aaa", app_type = kBuiltIn}.
  ASSERT_TRUE(app_info);
  EXPECT_EQ(1u, app_info->apps.size());
  EXPECT_EQ("aaa", app_info->apps[0]->app_id);
  EXPECT_EQ(AppType::kBuiltIn, app_info->apps[0]->app_type);

  EXPECT_EQ(1u, app_info->app_types.size());
  EXPECT_TRUE(base::Contains(app_info->app_types, AppType::kBuiltIn));
}

// Test AppStorageFileHandler can read and write the empty app info data.
TEST_F(AppStorageFileHandlerTest, ReadAndWriteEmptyData) {
  WriteToFile(std::vector<AppPtr>());
  auto app_info = ReadFromFile();
  ASSERT_TRUE(app_info);
  EXPECT_TRUE(app_info->apps.empty());
}

// Test AppStorageFileHandler can read and write one app.
TEST_F(AppStorageFileHandlerTest, ReadAndWriteOneApp) {
  WriteToFile(CreateOneApp());
  auto app_info = ReadFromFile();
  ASSERT_TRUE(app_info);
  EXPECT_TRUE(IsEqual(CreateOneApp(), app_info->apps));
  EXPECT_EQ(1u, app_info->app_types.size());
  EXPECT_TRUE(base::Contains(app_info->app_types, kAppType1));
}

// Test AppStorageFileHandler can read and write multiple apps.
TEST_F(AppStorageFileHandlerTest, ReadAndWriteMultipleApps) {
  WriteToFile(CreateTwoApps());
  auto app_info = ReadFromFile();
  ASSERT_TRUE(app_info);
  EXPECT_TRUE(IsEqual(CreateTwoApps(), app_info->apps));
  EXPECT_EQ(2u, app_info->app_types.size());
  EXPECT_TRUE(base::Contains(app_info->app_types, kAppType1));
  EXPECT_TRUE(base::Contains(app_info->app_types, kAppType2));
}

// Test AppStorageFileHandler can read and write the app info for icon updates.
TEST_F(AppStorageFileHandlerTest, VerifyIconUpdates) {
  {
    // Verify for the none icon effect.
    IconKey icon_key;
    VerifyIconKey(icon_key);
  }
  {
    // Verify for the multiple icon effects.
    IconKey icon_key =
        IconKey(/*resource_id=*/65535,
                IconEffects::kCrOsStandardIcon | IconEffects::kBlocked);
    VerifyIconKey(icon_key);
  }
  {
    // Verify the kPaused icon effect can be cleared.
    IconKey icon_key =
        IconKey(IconEffects::kCrOsStandardIcon | IconEffects::kPaused);
    VerifyIconKey(icon_key);
  }
  {
    // Verify `update_version` isn't saved, and it can be set as the init false
    // value.
    IconKey icon_key = IconKey(IconEffects::kCrOsStandardIcon);
    icon_key.update_version = 10;
    VerifyIconKey(icon_key);
  }
}

}  // namespace apps
