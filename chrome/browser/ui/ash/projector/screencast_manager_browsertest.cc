// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/screencast_manager.h"

#include <memory>
#include <string>

#include "ash/webui/projector_app/projector_screencast.h"
#include "base/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/drive/service/fake_drive_service.h"
#include "content/public/test/browser_test.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/drive/drive_api_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr char kScreencastId[] = "screencastId";
constexpr char kVideoFileId[] = "videoFileId";
constexpr char kMetadataFileId[] = "metadataFileId";

}  // namespace

class ScreencastManagerTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    auto fake_drive_service = std::make_unique<drive::FakeDriveService>();
    drive_service_ = fake_drive_service.get();
    screencast_manager_.SetDriveAPIServiceForTest(
        std::move(fake_drive_service));
  }

 protected:
  // Creates a default screencast folder with item id = |kScreencastId| for
  // DriveAPIService.
  void AddDefaultScreencastFolder() {
    base::RunLoop run_loop;
    drive_service()->AddNewDirectoryWithResourceId(
        kScreencastId,
        /*parent_resource_id=*/"",
        /*directory_title=*/"Screencast", drive::AddNewDirectoryOptions(),
        base::BindLambdaForTesting(
            [&run_loop](
                google_apis::ApiErrorCode drive_error,
                std::unique_ptr<google_apis::FileResource> drive_entry) {
              ASSERT_EQ(drive_error, google_apis::ApiErrorCode::HTTP_CREATED);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Creates a file under the default screencast folder for DriveAPIService.
  void AddFileToDefaultScreencastFolder(const std::string& file_id,
                                        const std::string& content_type,
                                        const std::string& title,
                                        bool shared_with_me) {
    base::RunLoop run_loop;
    drive_service()->AddNewFileWithResourceId(
        file_id, content_type, "This is some test content.",
        /*parent_resource_id=*/kScreencastId, title, shared_with_me,
        base::BindLambdaForTesting(
            [&run_loop](
                google_apis::ApiErrorCode drive_error,
                std::unique_ptr<google_apis::FileResource> drive_entry) {
              ASSERT_EQ(drive_error, google_apis::ApiErrorCode::HTTP_CREATED);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  ScreencastManager& screencast_manager() { return screencast_manager_; }

  drive::FakeDriveService* drive_service() { return drive_service_; }

 private:
  drive::FakeDriveService* drive_service_ = nullptr;

  ScreencastManager screencast_manager_;
};

IN_PROC_BROWSER_TEST_F(ScreencastManagerTest, GetScreencastSuccess) {
  // Creates default screencast folder.
  AddDefaultScreencastFolder();

  // Creates screencasts files.
  AddFileToDefaultScreencastFolder(kVideoFileId, "video/webm",
                                   "MyTestScreencast.webm", true);
  AddFileToDefaultScreencastFolder(kMetadataFileId, "text/plain",
                                   "MyTestScreencast.projector", true);

  base::RunLoop run_loop;
  screencast_manager().GetScreencast(
      kScreencastId,
      base::BindLambdaForTesting(
          [&run_loop](std::unique_ptr<ash::ProjectorScreencast> screencast,
                      const std::string& error) {
            EXPECT_EQ(screencast->container_folder_id, kScreencastId);
            // Expects video file id and metadata file id are populated.
            EXPECT_EQ(screencast->video.file_id, kVideoFileId);
            EXPECT_EQ(screencast->metadata_file_id, kMetadataFileId);
            EXPECT_TRUE(error.empty());
            // Quits the run loop.
            run_loop.Quit();
          }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ScreencastManagerTest,
                       GetScreencastInvalidScreencastError) {
  AddDefaultScreencastFolder();
  // Creates invalid screencast without video file.
  AddFileToDefaultScreencastFolder("metadataFileId", "text/plain",
                                   "MyTestScreencast.projector", true);

  base::RunLoop run_loop;
  screencast_manager().GetScreencast(
      kScreencastId,
      base::BindLambdaForTesting(
          [&run_loop](std::unique_ptr<ash::ProjectorScreencast> screencast,
                      const std::string& error) {
            // Expects missing file error.
            EXPECT_EQ(
                error,
                "Invalid screencast. Missing video file or metadata file. "
                "ScreencastId=screencastId");
            // Quits the run loop.
            run_loop.Quit();
          }));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ScreencastManagerTest, GetScreencastHttpError) {
  // Mocks offline.
  drive_service()->set_offline(true);

  base::RunLoop run_loop;
  screencast_manager().GetScreencast(
      kScreencastId,
      base::BindLambdaForTesting(
          [&run_loop](std::unique_ptr<ash::ProjectorScreencast> screencast,
                      const std::string& error) {
            // Expects search file error.
            EXPECT_EQ(error,
                      "Search Drive files error. ScreencastId=screencastId, "
                      "ErrorCode=900");
            // Quits the run loop.
            run_loop.Quit();
          }));
  run_loop.Run();
}

}  // namespace ash
