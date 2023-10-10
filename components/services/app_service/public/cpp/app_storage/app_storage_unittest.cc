// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_storage/app_storage.h"

#include <memory>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_storage/app_storage_file_handler.h"
#include "components/services/app_service/public/cpp/app_types.h"
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

}  // namespace

// This fake AppStorage is used to test and track all calls to AppStorage.
class FakeAppStorage : public AppStorage {
 public:
  FakeAppStorage(const base::FilePath& base_path,
                 AppRegistryCache& app_registry_cache)
      : AppStorage(base_path, app_registry_cache) {
    // Wait for OnGetAppInfoData to be invoked.
    EXPECT_TRUE(read_result_.Wait());
  }

  FakeAppStorage(const FakeAppStorage&) = delete;
  FakeAppStorage& operator=(const FakeAppStorage&) = delete;

  ~FakeAppStorage() override = default;

  std::vector<AppPtr>& GetAppInfo() { return apps_; }

  void WaitForSaveFinished(size_t expect_app_count) {
    expect_app_count_ = expect_app_count;
    write_result_ = std::make_unique<base::test::TestFuture<void>>();
    EXPECT_TRUE(write_result_->Wait());
  }

 private:
  // Override to call to AppStorage::OnGetAppInfoData.
  void OnGetAppInfoData(std::vector<AppPtr> apps) override {
    for (const auto& app : apps) {
      apps_.push_back(app->Clone());
    }

    AppStorage::OnGetAppInfoData(std::move(apps));
    std::move(read_result_.GetCallback()).Run();
  }

  void OnSaveFinished() override {
    AppStorage::OnSaveFinished();

    if (!io_in_progress_ && expect_app_count_ >= 0 &&
        expect_app_count_ == app_registry_cache_->states_.size()) {
      std::move(write_result_->GetCallback()).Run();
    }
  }

  std::vector<AppPtr> apps_;

  base::test::TestFuture<void> read_result_;
  std::unique_ptr<base::test::TestFuture<void>> write_result_;
  size_t expect_app_count_ = -1;
};

class AppStorageTest : public testing::Test {
 public:
  AppStorageTest() = default;
  ~AppStorageTest() override = default;

  AppStorageTest(const AppStorageTest&) = delete;
  AppStorageTest& operator=(const AppStorageTest&) = delete;

  void SetUp() override { ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir()); }

  void CreateAppStorage() {
    app_storage_ = std::make_unique<FakeAppStorage>(tmp_dir_.GetPath(),
                                                    app_registry_cache_);
  }

  std::vector<AppPtr> CreateOneApp(AppType app_type,
                                   const std::string& app_id) {
    AppPtr app = std::make_unique<App>(app_type, app_id);
    app->readiness = kReadiness1;
    std::vector<AppPtr> apps;
    apps.push_back(std::move(app));
    return apps;
  }

  std::vector<AppPtr> CreateTwoApps() {
    std::vector<AppPtr> apps;

    AppPtr app1 = std::make_unique<App>(kAppType1, kAppId1);
    app1->readiness = kReadiness1;
    app1->name = kAppName1;
    apps.push_back(std::move(app1));

    AppPtr app2 = std::make_unique<App>(kAppType2, kAppId2);
    app2->readiness = kReadiness2;
    app2->name = kAppName2;
    apps.push_back(std::move(app2));

    // TODO(crbug.com/1385932): Add other files in the App structure.
    return apps;
  }

  void ModifyOneApp() {
    AppPtr app = std::make_unique<App>(kAppType1, kAppId1);
    app->name = kAppName2;
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

  void VerifySavedApps(std::vector<AppPtr>& apps) {
    // Create a new AppStorage to read the AppStorage file to verify the app has
    // been written correctly.
    auto app_storage = std::make_unique<FakeAppStorage>(tmp_dir().GetPath(),
                                                        app_registry_cache());
    EXPECT_TRUE(IsEqual(apps, app_storage->GetAppInfo()));
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
}

// Test AppStorageTest can read and write one app.
TEST_F(AppStorageTest, ReadAndWriteOneApp) {
  CreateAppStorage();
  EXPECT_TRUE(app_storage()->GetAppInfo().empty());

  // Add 1 app.
  app_registry_cache().OnApps(CreateOneApp(kAppType1, kAppId1), kAppType1,
                              /*should_notify_initialized=*/false);
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/1);

  // Verify the app is saved correctly.
  auto apps1 = CreateOneApp(kAppType1, kAppId1);
  VerifySavedApps(apps1);

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
  app_registry_cache().OnApps(CreateTwoApps(), AppType::kUnknown,
                              /*should_notify_initialized=*/false);
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/2);

  // Verify the apps are saved correctly.
  auto apps = CreateTwoApps();
  VerifySavedApps(apps);

  ModifyOneApp();
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/2);

  // Verify the apps are saved correctly.
  apps[0]->name = kAppName2;
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
  app_registry_cache().OnApps(CreateOneApp(kAppType1, kAppId1), kAppType1,
                              /*should_notify_initialized=*/false);
  app_registry_cache().OnApps(CreateOneApp(kAppType1, kAppId2), kAppType1,
                              /*should_notify_initialized=*/false);
  app_registry_cache().OnApps(CreateOneApp(kAppType1, kAppId3), kAppType1,
                              /*should_notify_initialized=*/false);

  ModifyOneApp();
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
  VerifySavedApps(apps);
}

// Test AppStorageTest can handle the removed app case(kRemoved).
TEST_F(AppStorageTest, AddAndRemoveApp) {
  CreateAppStorage();
  EXPECT_TRUE(app_storage()->GetAppInfo().empty());

  // Add 1 app.
  app_registry_cache().OnApps(CreateOneApp(kAppType1, kAppId1), kAppType1,
                              /*should_notify_initialized=*/false);
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/1);

  // Verify the app is saved correctly.
  auto apps1 = CreateOneApp(kAppType1, kAppId1);
  VerifySavedApps(apps1);

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
  app_registry_cache().OnApps(std::move(apps2), kAppType1,
                              /*should_notify_initialized=*/false);
  app_storage()->WaitForSaveFinished(/*expect_app_count=*/1);

  // Verify the app has been added.
  auto apps3 = CreateOneApp(kAppType1, kAppId1);
  VerifySavedApps(apps3);
}

}  // namespace apps
