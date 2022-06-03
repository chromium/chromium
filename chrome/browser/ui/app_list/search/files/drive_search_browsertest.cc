// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ui/app_list/search/app_list_search_test_helper.h"

namespace app_list {

// This class contains additional logic to set up DriveFS and enable testing for
// Drive file search in the launcher.
class AppListDriveSearchBrowserTest : public AppListSearchBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    create_drive_integration_service_ = base::BindRepeating(
        &AppListDriveSearchBrowserTest::CreateDriveIntegrationService,
        base::Unretained(this));
    service_factory_for_test_ = std::make_unique<
        drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
        &create_drive_integration_service_);
  }

 protected:
  virtual drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath mount_path = profile->GetPath().Append("drivefs");
    fake_drivefs_helpers_[profile] =
        std::make_unique<drive::FakeDriveFsHelper>(profile, mount_path);
    auto* integration_service = new drive::DriveIntegrationService(
        profile, std::string(), mount_path,
        fake_drivefs_helpers_[profile]->CreateFakeDriveFsListenerFactory());
    return integration_service;
  }

 private:
  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
  std::map<Profile*, std::unique_ptr<drive::FakeDriveFsHelper>>
      fake_drivefs_helpers_;
};

// Test that Drive files can be searched.
IN_PROC_BROWSER_TEST_F(AppListDriveSearchBrowserTest, DriveSearchTest) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  drive::DriveIntegrationService* drive_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(GetProfile());
  ASSERT_TRUE(drive_service->IsMounted());
  base::FilePath mount_path = drive_service->GetMountPointPath();

  ASSERT_TRUE(base::WriteFile(mount_path.Append("my_file.gdoc"), "content"));
  ASSERT_TRUE(
      base::WriteFile(mount_path.Append("other_file.gsheet"), "content"));

  SearchAndWaitForProviders("my", {ResultType::kDriveSearch});

  const auto results = PublishedResultsForProvider(ResultType::kDriveSearch);
  ASSERT_EQ(results.size(), 1u);
  ASSERT_TRUE(results[0]);
  EXPECT_EQ(base::UTF16ToASCII(results[0]->title()), "my_file");
}

// Test that Drive folders can be searched.
IN_PROC_BROWSER_TEST_F(AppListDriveSearchBrowserTest, DriveFolderTest) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  drive::DriveIntegrationService* drive_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(GetProfile());
  ASSERT_TRUE(drive_service->IsMounted());
  base::FilePath mount_path = drive_service->GetMountPointPath();

  ASSERT_TRUE(base::CreateDirectory(mount_path.Append("my_folder")));
  ASSERT_TRUE(base::CreateDirectory(mount_path.Append("other_folder")));

  SearchAndWaitForProviders("my", {ResultType::kDriveSearch});

  const auto results = PublishedResultsForProvider(ResultType::kDriveSearch);
  ASSERT_EQ(results.size(), 1u);
  ASSERT_TRUE(results[0]);
  EXPECT_EQ(base::UTF16ToASCII(results[0]->title()), "my_folder");
}

}  // namespace app_list
