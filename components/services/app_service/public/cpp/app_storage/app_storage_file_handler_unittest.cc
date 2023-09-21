// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_storage/app_storage_file_handler.h"

#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

namespace {

constexpr char kAppId1[] = "aaa";
constexpr char kAppId2[] = "bbb";

constexpr AppType kAppType1 = AppType::kArc;
constexpr AppType kAppType2 = AppType::kWeb;

constexpr char kAppName1[] = "AAA";
constexpr char kAppName2[] = "BBB";

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
  std::vector<AppPtr> ReadFromFile() {
    base::test::TestFuture<std::vector<AppPtr>> result;
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
    apps.push_back(std::make_unique<App>(kAppType1, kAppId1));
    return apps;
  }

  std::vector<AppPtr> CreateTwoApps() {
    std::vector<AppPtr> apps;

    AppPtr app1 = std::make_unique<App>(kAppType1, kAppId1);
    app1->name = kAppName1;
    app1->readiness = Readiness::kReady;
    apps.push_back(std::move(app1));

    AppPtr app2 = std::make_unique<App>(kAppType2, kAppId2);
    app2->name = kAppName2;
    app2->readiness = Readiness::kDisabledByUser;
    apps.push_back(std::move(app2));

    // TODO(crbug.com/1385932): Add other files in the App structure.
    return apps;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir tmp_dir_;

  scoped_refptr<AppStorageFileHandler> file_handler_;
};

// Test AppStorageFileHandler can work from an unavailable file.
TEST_F(AppStorageFileHandlerTest, ReadFromNotValidFile) {
  auto apps = ReadFromFile();
  EXPECT_TRUE(apps.empty());
}

// Test AppStorageFileHandler won't crash when the file is empty.
TEST_F(AppStorageFileHandlerTest, ReadFromEmptyFile) {
  WriteToFile("");
  auto apps = ReadFromFile();
  EXPECT_TRUE(apps.empty());
}

// Test AppStorageFileHandler won't crash when the file isn't a json format.
TEST_F(AppStorageFileHandlerTest, ReadFromWrongJSONFile) {
  const char kAppInfoData[] = "\"abc\":{\"type\":5}";
  WriteToFile(kAppInfoData);
  auto apps = ReadFromFile();
  EXPECT_TRUE(apps.empty());
}

// Test AppStorageFileHandler can work when the data format isn't correct.
TEST_F(AppStorageFileHandlerTest, ReadFromWrongDataFile) {
  const char kAppInfoData[] =
      "{\"abc\":{}, \"aaa\":{\"type\":2, \"readiness\":100}}";
  WriteToFile(kAppInfoData);
  auto apps = ReadFromFile();

  // The app type for "abc" is empty, so we can get one app only {app_id =
  // "aaa", app_type = kBuiltIn}.
  EXPECT_EQ(1u, apps.size());
  EXPECT_EQ("aaa", apps[0]->app_id);
  EXPECT_EQ(AppType::kBuiltIn, apps[0]->app_type);
  // The readiness for the app "aaa" is wrong, so readiness is set as the
  // default value.
  EXPECT_EQ(Readiness::kUnknown, apps[0]->readiness);
}

// Test AppStorageFileHandler can work when the app type isn't correct.
TEST_F(AppStorageFileHandlerTest, ReadFromWrongAppType) {
  const char kAppInfoData[] = "{\"abc\":{\"type\":100}, \"aaa\":{\"type\":2}}";
  WriteToFile(kAppInfoData);
  auto apps = ReadFromFile();

  // The app type for "abc" is wrong, so we can get one app only {app_id =
  // "aaa", app_type = kBuiltIn}.
  EXPECT_EQ(1u, apps.size());
  EXPECT_EQ("aaa", apps[0]->app_id);
  EXPECT_EQ(AppType::kBuiltIn, apps[0]->app_type);
}

// Test AppStorageFileHandler can read and write the empty app info data.
TEST_F(AppStorageFileHandlerTest, ReadAndWriteEmptyData) {
  WriteToFile(std::vector<AppPtr>());
  auto apps = ReadFromFile();
  EXPECT_TRUE(apps.empty());
}

// Test AppStorageFileHandler can read and write one app.
TEST_F(AppStorageFileHandlerTest, ReadAndWriteOneApp) {
  WriteToFile(CreateOneApp());
  EXPECT_TRUE(IsEqual(CreateOneApp(), ReadFromFile()));
}

// Test AppStorageFileHandler can read and write multiple apps.
TEST_F(AppStorageFileHandlerTest, ReadAndWriteMultipleApps) {
  WriteToFile(CreateTwoApps());
  EXPECT_TRUE(IsEqual(CreateTwoApps(), ReadFromFile()));
}

}  // namespace apps
