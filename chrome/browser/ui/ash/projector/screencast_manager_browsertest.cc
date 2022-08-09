// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/screencast_manager.h"

#include <memory>

#include "ash/components/drivefs/fake_drivefs.h"
#include "ash/webui/projector_app/projector_screencast.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/drive/file_errors.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kVideoFileName[] = "MyTestScreencast.webm";
constexpr char kVideoFileId[] = "videoFileId";
constexpr char kResourceKey[] = "resourceKey";
constexpr char kTestFileContents[] = "This is some test content.";

}  // namespace

class ScreencastManagerTest : public InProcessBrowserTest {
 protected:
  ScreencastManager& screencast_manager() { return screencast_manager_; }

 private:
  ScreencastManager screencast_manager_;
};

class ScreencastManagerTestWithDriveFs : public ScreencastManagerTest {
 public:
  // ScreencastManagerTest:
  void SetUpInProcessBrowserTestFixture() override {
    ScreencastManagerTest::SetUpInProcessBrowserTestFixture();
    create_drive_integration_service_ = base::BindRepeating(
        &ScreencastManagerTestWithDriveFs::CreateDriveIntegrationService,
        base::Unretained(this));
    service_factory_for_test_ = std::make_unique<
        drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
        &create_drive_integration_service_);
  }

  // Gets the file path for a named file in the test folder. If `relative`
  // is true, then returns the file path relative to the DriveFS mount point.
  // Otherwise, returns the absolute file path.
  base::FilePath GetTestFile(const std::string& title, bool relative) {
    auto* drive_service = drive::DriveIntegrationServiceFactory::FindForProfile(
        browser()->profile());
    base::FilePath mount_path = drive_service->GetMountPointPath();
    base::FilePath file_path = mount_path.Append(title);
    if (!relative) {
      return file_path;
    }
    base::FilePath relative_path("/");
    EXPECT_TRUE(mount_path.AppendRelativePath(file_path, &relative_path));
    return relative_path;
  }

  void AddFileToDefaultFolder(const std::string& file_id,
                              const std::string& content_type,
                              const std::string& title,
                              bool shared_with_me) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    drivefs::FakeDriveFs* fake = GetFakeDriveFsForProfile(browser()->profile());

    const base::FilePath& absolute_path =
        GetTestFile(title, /*relative=*/false);
    EXPECT_TRUE(base::WriteFile(absolute_path, kTestFileContents));

    const base::FilePath& relative_path = GetTestFile(title, /*relative=*/true);
    fake->SetMetadata(relative_path, content_type, title, false, shared_with_me,
                      {}, {}, file_id, "");
  }

 protected:
  drivefs::FakeDriveFs* GetFakeDriveFsForProfile(Profile* profile) {
    return &fake_drivefs_helpers_[profile]->fake_drivefs();
  }

  drive::DriveIntegrationService* CreateDriveIntegrationService(
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

// Tests that GetDriveFsFile() fails with an appropriate error message when
// there's no DriveFS mount point available.
IN_PROC_BROWSER_TEST_F(ScreencastManagerTest, NoDriveFsMountPoint) {
  base::RunLoop run_loop;
  screencast_manager().GetVideo(
      kVideoFileId, /*resource_key=*/"",
      base::BindLambdaForTesting(
          [&run_loop](std::unique_ptr<ProjectorScreencastVideo> video,
                      const std::string& error_message) {
            EXPECT_EQ(error_message,
                      base::StringPrintf(
                          "Failed to find DriveFS path with video file id=%s",
                          kVideoFileId));
            // Quits the run loop.
            run_loop.Quit();
          }));
  run_loop.Run();
}

// Tests that GetDriveFsFile() fails with an appropriate error message when the
// files don't exist in DriveFS. This scenario can happen right after the user
// logs in on a new device, before the files have fully synced.
IN_PROC_BROWSER_TEST_F(ScreencastManagerTestWithDriveFs, FileNotFound) {
  base::RunLoop run_loop;
  screencast_manager().GetVideo(
      kVideoFileId, kResourceKey,
      base::BindLambdaForTesting(
          [&run_loop](std::unique_ptr<ProjectorScreencastVideo> video,
                      const std::string& error_message) {
            EXPECT_EQ(
                error_message,
                base::StringPrintf("Failed to fetch DriveFS file with video "
                                   "file id=%s and error code=%d",
                                   kVideoFileId, drive::FILE_ERROR_NOT_FOUND));
            // Quits the run loop.
            run_loop.Quit();
          }));
  run_loop.Run();
}

// Tests that the ScreencastManager rejects files that don't look like a video.
IN_PROC_BROWSER_TEST_F(ScreencastManagerTestWithDriveFs, NotAVideo) {
  AddFileToDefaultFolder(kVideoFileId, "video/webm", "MyTestScreencast.exe",
                         /*shared_with_me=*/true);

  base::RunLoop run_loop;
  screencast_manager().GetVideo(
      kVideoFileId, /*resource_key=*/"",
      base::BindLambdaForTesting(
          [&run_loop](std::unique_ptr<ProjectorScreencastVideo> video,
                      const std::string& error_message) {
            EXPECT_EQ(error_message,
                      base::StringPrintf(
                          "Failed to fetch video file with video file id=%s",
                          kVideoFileId));
            // Quits the run loop.
            run_loop.Quit();
          }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ScreencastManagerTestWithDriveFs, GetVideoSuccess) {
  AddFileToDefaultFolder(kVideoFileId, "video/webm", kVideoFileName,
                         /*shared_with_me=*/false);

  base::RunLoop run_loop;
  screencast_manager().GetVideo(
      kVideoFileId, kResourceKey,
      base::BindLambdaForTesting(
          [&run_loop](std::unique_ptr<ProjectorScreencastVideo> video,
                      const std::string& error_message) {
            EXPECT_EQ(video->file_id, kVideoFileId);
            EXPECT_TRUE(error_message.empty());
            // Quits the run loop.
            run_loop.Quit();
          }));
  run_loop.Run();
}

}  // namespace ash
