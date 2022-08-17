// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/components/drivefs/fake_drivefs.h"
#include "ash/webui/projector_app/buildflags.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "ash/webui/projector_app/projector_screencast.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "ash/webui/web_applications/test/sandboxed_web_ui_test_base.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_type.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/web_applications/test/profile_test_helper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/drive/file_errors.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/test_data_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kVideoFileName[] = "MyTestScreencast.webm";
constexpr char kVideoFileId[] = "videoFileId";
constexpr char kResourceKey[] = "resourceKey";
constexpr char kTestFileContents[] = "This is some test content.";

// Name and duration of a real video file located at //media/test/data.
constexpr char kTestVideoFile[] = "tulip2.webm";
constexpr char kTestVideoDurationMilliesecond[] = "16682";

void VerifyResponse(const content::EvalJsResult& result) {
  EXPECT_TRUE(result.error.empty());

  const base::Value::Dict& dict = result.value.GetDict();
  const std::string* file_id = dict.FindString("fileId");
  ASSERT_TRUE(file_id);
  EXPECT_EQ(*file_id, kVideoFileId);
  const std::string* src_url = dict.FindString("srcUrl");
  ASSERT_TRUE(src_url);
  // We can't verify the entire video src url because the random hash at the end
  // differs across test runs, even for the same file. Just check that the url
  // begins with blob:chrome-untrusted://projector/.
  EXPECT_EQ(src_url->rfind("blob:chrome-untrusted://projector/", 0), 0);
  const std::string* duration_millis = dict.FindString("durationMillis");
  ASSERT_TRUE(duration_millis);
  EXPECT_EQ(*duration_millis, kTestVideoDurationMilliesecond);
}

}  // namespace

using ScreencastManagerTest = SystemWebAppIntegrationTest;

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

  // ScreencastManagerTest:
  void SetUpOnMainThread() override {
    ScreencastManagerTest::SetUpOnMainThread();
    WaitForTestSystemAppInstall();
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
    // Writes a file with `kTestFileContents` if path doesn't exist.
    if (!base::PathExists(absolute_path))
      EXPECT_TRUE(base::WriteFile(absolute_path, kTestFileContents));

    const base::FilePath& relative_path = GetTestFile(title, /*relative=*/true);
    fake->SetMetadata(relative_path, content_type, title, false, shared_with_me,
                      {}, {}, file_id, "");
  }

  // Copies a file from //media/test/data with `original_name` to default test
  // folder with `dest_name`.
  void AddTestMediaFileToDefaultFolder(const std::string& original_name,
                                       const std::string& dest_name,
                                       const std::string& content_type,
                                       bool shared_with_me) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::CopyFile(media::GetTestDataFilePath(original_name),
                               GetTestFile(dest_name, /*relative=*/false)));
    AddFileToDefaultFolder(kVideoFileId, content_type, dest_name,
                           /*shared_with_me=*/shared_with_me);
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
IN_PROC_BROWSER_TEST_P(ScreencastManagerTest, NoDriveFsMountPoint) {
  base::RunLoop run_loop;
  ProjectorAppClient::Get()->GetVideo(
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
IN_PROC_BROWSER_TEST_P(ScreencastManagerTestWithDriveFs, FileNotFound) {
  base::RunLoop run_loop;
  ProjectorAppClient::Get()->GetVideo(
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
IN_PROC_BROWSER_TEST_P(ScreencastManagerTestWithDriveFs, NotAVideo) {
  AddFileToDefaultFolder(kVideoFileId, kProjectorMediaMimeType,
                         "MyTestScreencast.exe",
                         /*shared_with_me=*/true);

  base::RunLoop run_loop;
  ProjectorAppClient::Get()->GetVideo(
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

IN_PROC_BROWSER_TEST_P(ScreencastManagerTestWithDriveFs, GetVideoSuccess) {
  // Uses a real webm video file for this test and renames it to
  // `kVideoFileName`.
  AddTestMediaFileToDefaultFolder(kTestVideoFile, kVideoFileName,
                                  kProjectorMediaMimeType, false);
  base::RunLoop run_loop;
  ProjectorAppClient::Get()->GetVideo(
      kVideoFileId, kResourceKey,
      base::BindLambdaForTesting(
          [&run_loop](std::unique_ptr<ProjectorScreencastVideo> video,
                      const std::string& error_message) {
            EXPECT_EQ(video->file_id, kVideoFileId);
            EXPECT_EQ(video->duration_millis, kTestVideoDurationMilliesecond);
            EXPECT_TRUE(error_message.empty());
            run_loop.Quit();
          }));
  run_loop.Run();
}

// Tests that the ScreencastManager rejects malformed video files.
IN_PROC_BROWSER_TEST_P(ScreencastManagerTestWithDriveFs,
                       GetMalFormedVideoFail) {
  // Uses a binary file for this test and renames it to `kVideoFileName`.
  AddTestMediaFileToDefaultFolder("bear-audio-mp4a.69.ts", kVideoFileName,
                                  kProjectorMediaMimeType, true);
  base::RunLoop run_loop;
  ProjectorAppClient::Get()->GetVideo(
      kVideoFileId, kResourceKey,
      base::BindLambdaForTesting(
          [&run_loop](std::unique_ptr<ProjectorScreencastVideo> video,
                      const std::string& error_message) {
            EXPECT_EQ(error_message,
                      base::StringPrintf(
                          "Media might be malformed with video file id=%s",
                          kVideoFileId));
            run_loop.Quit();
          }));
  run_loop.Run();
}

#if !BUILDFLAG(ENABLE_CROS_PROJECTOR_APP)

constexpr char kGetVideoScript[] = R"(
      (async function getVideo() {
        const projectorApp = document.querySelector('projector-app');
        const clientDelegate = projectorApp.getClientDelegateForTesting();
        return await clientDelegate.getVideo('%s');
      })();
      )";

// The following tests only run in the unbranded build (is_chrome_branded =
// false) because they rely on the mock app for testing. The script calls
// projectorApp.getClientDelegateForTesting(), which only exists in the mock
// version of the app.

// There is a necessary race condition between getVideo() and onFileLoaded()
// because they occur on different channels. It shouldn't matter which one
// returns first because we wait for both promises before returning the
// assembled video object. This test covers the scenario where onFileLoaded()
// returns before getVideo().
IN_PROC_BROWSER_TEST_P(ScreencastManagerTestWithDriveFs,
                       LoadFileBeforeGetVideo) {
  // Uses a real webm video file for this test and renames it to
  // `kVideoFileName`.
  AddTestMediaFileToDefaultFolder(kTestVideoFile, kVideoFileName,
                                  kProjectorMediaMimeType, true);

  // Launch the app for the first time.
  content::WebContents* app = LaunchApp(SystemWebAppType::PROJECTOR);
  EXPECT_TRUE(WaitForLoadStop(app));
  Browser* first_browser = chrome::FindBrowserWithActiveWindow();
  // Verify that Projector App is opened.
  ASSERT_TRUE(first_browser);
  EXPECT_EQ(first_browser->tab_strip_model()->GetActiveWebContents(), app);

  // Use LaunchAppWithFileWithoutWaiting() instead of LaunchAppWithFile()
  // because the second one waits for the app to finish loading, but we don't
  // reload the app when we send files. Thus, LaunchAppWithFile() would time out
  // and we should use LaunchAppWithFileWithoutWaiting() instead.
  base::FilePath absolute_path =
      GetTestFile(kVideoFileName, /*relative=*/false);
  app = LaunchAppWithFileWithoutWaiting(SystemWebAppType::PROJECTOR,
                                        absolute_path);
  Browser* second_browser = chrome::FindBrowserWithActiveWindow();
  // Launching the app with files should not open a new window.
  EXPECT_EQ(first_browser, second_browser);

  const std::string& script = base::StringPrintf(kGetVideoScript, kVideoFileId);
  content::EvalJsResult result =
      EvalJs(SandboxedWebUiAppTestBase::GetAppFrame(app), script);
  VerifyResponse(result);
}

// There is a necessary race condition between getVideo() and onFileLoaded()
// because they occur on different channels. It shouldn't matter which one
// returns first because we wait for both promises before returning the
// assembled video object. This test covers the scenario where
// getVideo() returns before onFileLoaded().
IN_PROC_BROWSER_TEST_P(ScreencastManagerTestWithDriveFs,
                       GetVideoBeforeLoadFile) {
  // Uses a real webm video file for this test and renames it to
  // `kVideoFileName`.
  AddTestMediaFileToDefaultFolder(kTestVideoFile, kVideoFileName,
                                  kProjectorMediaMimeType,
                                  /*shared_with_me=*/false);

  // Launch the app for the first time.
  content::WebContents* app = LaunchApp(ash::SystemWebAppType::PROJECTOR);
  EXPECT_TRUE(WaitForLoadStop(app));

  const std::string& script = base::StringPrintf(kGetVideoScript, kVideoFileId);
  content::EvalJsResult result =
      EvalJs(SandboxedWebUiAppTestBase::GetAppFrame(app), script);
  VerifyResponse(result);
}

// The following situation can happen if the user requests a video file id that
// doesn't exist in DriveFS. For example, the user could be on a new device and
// the items haven't synced yet.
IN_PROC_BROWSER_TEST_P(ScreencastManagerTestWithDriveFs,
                       FileNotFoundInDriveFS) {
  // Launch the app for the first time.
  content::WebContents* app = LaunchApp(ash::SystemWebAppType::PROJECTOR);
  EXPECT_TRUE(WaitForLoadStop(app));

  const std::string& script = base::StringPrintf(kGetVideoScript, kVideoFileId);
  content::EvalJsResult result =
      EvalJs(SandboxedWebUiAppTestBase::GetAppFrame(app), script);
  const std::string& expected_error = base::StringPrintf(
      "a JavaScript error: \"Failed to fetch DriveFS file with video file "
      "id=%s and error code=%d\"\n",
      kVideoFileId, drive::FILE_ERROR_NOT_FOUND);
  EXPECT_EQ(result.error, expected_error);
}

// Tests a disk I/O error when trying to access the file handle in launch.js.
IN_PROC_BROWSER_TEST_P(ScreencastManagerTestWithDriveFs,
                       NotFoundErrorDOMException) {
  // Uses a real webm video file for this test and renames it to
  // `kVideoFileName`.
  AddTestMediaFileToDefaultFolder(kTestVideoFile, kVideoFileName,
                                  kProjectorMediaMimeType,
                                  /*shared_with_me=*/true);

  // Launch the app for the first time.
  content::WebContents* app = LaunchApp(ash::SystemWebAppType::PROJECTOR);
  EXPECT_TRUE(WaitForLoadStop(app));
  base::FilePath absolute_path =
      GetTestFile("NotFoundError.file", /*relative=*/false);
  app = LaunchAppWithFileWithoutWaiting(SystemWebAppType::PROJECTOR,
                                        absolute_path);

  const std::string& script = base::StringPrintf(kGetVideoScript, kVideoFileId);
  content::EvalJsResult result =
      EvalJs(SandboxedWebUiAppTestBase::GetAppFrame(app), script);
  EXPECT_EQ(
      result.error,
      "a JavaScript error: \"NotFoundError: A requested file or directory "
      "could not be found at the time an operation was processed.\"\n");
}

// Tests throwing an error instead of sending the file to the untrusted context
// if the retrieved video file doesn't have a video MIME type.
IN_PROC_BROWSER_TEST_P(ScreencastManagerTestWithDriveFs, NotAVideoMimeType) {
  AddFileToDefaultFolder("driveItemId", "text/plain", "MyTestScreencast.txt",
                         /*shared_with_me=*/false);

  // Launch the app for the first time.
  content::WebContents* app = LaunchApp(ash::SystemWebAppType::PROJECTOR);
  EXPECT_TRUE(WaitForLoadStop(app));
  base::FilePath absolute_path =
      GetTestFile("MyTestScreencast.txt", /*relative=*/false);
  app = LaunchAppWithFileWithoutWaiting(SystemWebAppType::PROJECTOR,
                                        absolute_path);

  AddTestMediaFileToDefaultFolder(kTestVideoFile, kVideoFileName, "text/plain",
                                  /*shared_with_me=*/true);

  const std::string& script = base::StringPrintf(kGetVideoScript, kVideoFileId);
  content::EvalJsResult result =
      EvalJs(SandboxedWebUiAppTestBase::GetAppFrame(app), script);
  EXPECT_EQ(result.error, "a JavaScript error: \"NotAVideo: Not a video.\"\n");
}

#endif  // !BUILDFLAG(ENABLE_CROS_PROJECTOR_APP)

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    ScreencastManagerTest);
INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    ScreencastManagerTestWithDriveFs);

}  // namespace ash
