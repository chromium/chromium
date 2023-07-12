// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_forward.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/one_drive_upload_handler.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_url.h"

using storage::FileSystemURL;

namespace ash::cloud_upload {
namespace {

// Returns full test file path to the given |file_name|.
base::FilePath GetTestFilePath(const std::string& file_name) {
  // Get the path to file manager's test data directory.
  base::FilePath source_dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_dir));
  base::FilePath test_data_dir = source_dir.AppendASCII("chrome")
                                     .AppendASCII("test")
                                     .AppendASCII("data")
                                     .AppendASCII("chromeos")
                                     .AppendASCII("file_manager");
  return test_data_dir.Append(base::FilePath::FromUTF8Unsafe(file_name));
}

}  // namespace

// Tests the OneDrive upload workflow using the static
// `OneDriveUploadHandler::Upload` method. Ensures that the upload completes
// with the expected results.
class OneDriveUploadHandlerTest : public InProcessBrowserTest,
                                  public NotificationDisplayService::Observer {
 public:
  OneDriveUploadHandlerTest() {
    feature_list_.InitAndEnableFeature(
        chromeos::features::kUploadOfficeToCloud);
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    my_files_dir_ = temp_dir_.GetPath().Append("myfiles");
    read_only_dir_ = temp_dir_.GetPath().Append("readonly");
  }

  OneDriveUploadHandlerTest(const OneDriveUploadHandlerTest&) = delete;
  OneDriveUploadHandlerTest& operator=(const OneDriveUploadHandlerTest&) =
      delete;

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
  }

  void TearDownOnMainThread() override {
    RemoveObservers();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  // Creates mount point for My files and registers local filesystem.
  void SetUpMyFiles() {
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::CreateDirectory(my_files_dir_));
    }
    std::string mount_point_name =
        file_manager::util::GetDownloadsMountPointName(profile());
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        mount_point_name);
    CHECK(storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        mount_point_name, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), my_files_dir_));
    file_manager::VolumeManager::Get(profile())
        ->RegisterDownloadsDirectoryForTesting(my_files_dir_);
  }

  // Creates a new filesystem which represents a read-only location, files
  // cannot be moved from it.
  void SetUpReadOnlyLocation() {
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::CreateDirectory(read_only_dir_));
    }
    std::string mount_point_name = "readonly";
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        mount_point_name);
    EXPECT_TRUE(profile()->GetMountPoints()->RegisterFileSystem(
        mount_point_name, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), read_only_dir_));
    file_manager::VolumeManager::Get(profile())->AddVolumeForTesting(
        read_only_dir_, file_manager::VOLUME_TYPE_TESTING,
        ash::DeviceType::kUnknown, true /* read_only */);
  }

  // Creates and mounts fake provided file system for OneDrive.
  void SetUpODFS() {
    provided_file_system_ =
        file_manager::test::CreateFakeProvidedFileSystemOneDrive(profile());
  }

  // Create and add a file with |test_file_name| to the file system
  // |source_path|. Return the created |source_file_url|.
  FileSystemURL SetUpSourceFile(const std::string& test_file_name,
                                base::FilePath source_path) {
    const base::FilePath source_file_path =
        source_path.AppendASCII(test_file_name);
    // Create test docx file within My files.
    const base::FilePath test_file_path = GetTestFilePath(test_file_name);
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      CHECK(base::CopyFile(test_file_path, source_file_path));
    }

    // Check that the source file exists at the intended source location and is
    // not in ODFS.
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(base::PathExists(source_path.AppendASCII(test_file_name)));
      CheckPathNotFoundOnODFS(base::FilePath("/").AppendASCII(test_file_name));
    }

    FileSystemURL source_file_url = FilePathToFileSystemURL(
        profile(),
        file_manager::util::GetFileManagerFileSystemContext(profile()),
        source_file_path);

    return source_file_url;
  }

  void SetUpObservers() {
    // Subscribe to Notification updates to track copy/move ODFS notifications.
    NotificationDisplayService::GetForProfile(profile())->AddObserver(this);
  }

  void RemoveObservers() {
    NotificationDisplayService::GetForProfile(browser()->profile())
        ->RemoveObserver(this);
  }

  void Wait() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_FALSE(run_loop_);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_ = nullptr;
  }

  void EndWait() {
    ASSERT_TRUE(run_loop_);
    run_loop_->Quit();
  }

  void CheckPathExistsOnODFS(const base::FilePath& path) {
    ASSERT_TRUE(provided_file_system_);
    provided_file_system_->GetMetadata(
        path, storage::FileSystemOperation::GET_METADATA_FIELD_NONE,
        base::BindOnce(&OneDriveUploadHandlerTest::OnGetMetadataExpectSuccess,
                       base::Unretained(this)));
    base::ScopedAllowBlockingForTesting allow_blocking;
    Wait();
  }

  void CheckPathNotFoundOnODFS(const base::FilePath& path) {
    ASSERT_TRUE(provided_file_system_);
    provided_file_system_->GetMetadata(
        path, storage::FileSystemOperation::GET_METADATA_FIELD_NONE,
        base::BindOnce(&OneDriveUploadHandlerTest::OnGetMetadataExpectNotFound,
                       base::Unretained(this)));
    base::ScopedAllowBlockingForTesting allow_blocking;
    Wait();
  }

  void OnGetMetadataExpectSuccess(
      std::unique_ptr<file_system_provider::EntryMetadata> entry_metadata,
      base::File::Error result) {
    EXPECT_EQ(base::File::Error::FILE_OK, result);
    EndWait();
  }

  void OnGetMetadataExpectNotFound(
      std::unique_ptr<file_system_provider::EntryMetadata> entry_metadata,
      base::File::Error result) {
    EXPECT_EQ(base::File::Error::FILE_ERROR_NOT_FOUND, result);
    EndWait();
  }

  // Watch for a valid `uploaded_file_url`.
  void OnUploadDone(const storage::FileSystemURL& uploaded_file_url,
                    int64_t size) {
    ASSERT_TRUE(uploaded_file_url.is_valid());
    EndWait();
  }

  // Run |on_notification_displayed_callback_| with observed |notification|.
  void OnNotificationDisplayed(
      const message_center::Notification& notification,
      const NotificationCommon::Metadata* const metadata) override {
    if (on_notification_displayed_callback_) {
      std::move(on_notification_displayed_callback_).Run(notification);
    }
  }

  void OnNotificationClosed(const std::string& notification_id) override {}
  void OnNotificationDisplayServiceDestroyed(
      NotificationDisplayService* service) override {}

  void SetOnNotificationDisplayedCallback(
      base::RepeatingCallback<void(const message_center::Notification&)>
          callback) {
    on_notification_displayed_callback_ = std::move(callback);
  }

  Profile* profile() { return browser()->profile(); }

 protected:
  base::FilePath my_files_dir_;
  base::FilePath read_only_dir_;
  raw_ptr<file_manager::test::FakeProvidedFileSystemOneDrive, ExperimentalAsh>
      provided_file_system_;  // Owned by Service.

 private:
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::RunLoop> run_loop_;
  // Used to observe upload notifications during the tests.
  base::RepeatingCallback<void(const message_center::Notification&)>
      on_notification_displayed_callback_;
};

IN_PROC_BROWSER_TEST_F(OneDriveUploadHandlerTest, UploadFromMyFiles) {
  SetUpMyFiles();
  SetUpODFS();
  const std::string test_file_name = "text.docx";
  FileSystemURL source_file_url =
      SetUpSourceFile(test_file_name, my_files_dir_);

  // Start the upload workflow and end the test once the upload has completed
  // successfully.
  OneDriveUploadHandler::Upload(
      profile(), source_file_url,
      base::BindOnce(&OneDriveUploadHandlerTest::OnUploadDone,
                     base::Unretained(this)));
  Wait();

  // Check that the source file has been moved to OneDrive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_FALSE(base::PathExists(my_files_dir_.AppendASCII(test_file_name)));
    CheckPathExistsOnODFS(base::FilePath("/").AppendASCII(test_file_name));
  }
}

IN_PROC_BROWSER_TEST_F(OneDriveUploadHandlerTest,
                       UploadFromReadOnlyFileSystem) {
  SetUpReadOnlyLocation();
  SetUpODFS();
  const std::string test_file_name = "text.docx";
  FileSystemURL source_file_url =
      SetUpSourceFile(test_file_name, read_only_dir_);

  // Start the upload workflow and end the test once the upload has completed
  // successfully.
  OneDriveUploadHandler::Upload(
      profile(), source_file_url,
      base::BindOnce(&OneDriveUploadHandlerTest::OnUploadDone,
                     base::Unretained(this)));
  Wait();

  // Check that the source file has been copied to OneDrive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(read_only_dir_.AppendASCII(test_file_name)));
    CheckPathExistsOnODFS(base::FilePath("/").AppendASCII(test_file_name));
  }
}

// Test that when the upload to ODFS fails due reauthentication to OneDrive
// being required, the reauthentication required notification is shown.
IN_PROC_BROWSER_TEST_F(OneDriveUploadHandlerTest,
                       FailToUploadDueToReauthenticationRequired) {
  SetUpObservers();
  SetUpMyFiles();
  SetUpODFS();
  // Ensure upload fails due to reauthentication to OneDrive being required.
  provided_file_system_->SetCreateFileError(
      base::File::Error::FILE_ERROR_ACCESS_DENIED);
  provided_file_system_->SetReauthenticationRequired(true);
  // Expect the reauthentication required notification.
  const std::string test_file_name = "text.docx";
  FileSystemURL source_file_url =
      SetUpSourceFile(test_file_name, my_files_dir_);

  // Start the upload workflow and end the test once the upload has failed due
  // to reauthentication being required.
  auto on_notification = base::BindLambdaForTesting(
      [&](const message_center::Notification& notification) {
        if (notification.message() ==
            base::UTF8ToUTF16(GetReauthenticationRequiredMessage())) {
          EndWait();
        }
      });
  SetOnNotificationDisplayedCallback(std::move(on_notification));
  OneDriveUploadHandler::Upload(profile(), source_file_url, base::DoNothing());
  Wait();

  // Check that the source file still exists only at the intended source
  // location and did not get uploaded to ODFS.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(my_files_dir_.AppendASCII(test_file_name)));
    CheckPathNotFoundOnODFS(base::FilePath("/").AppendASCII(test_file_name));
  }
}

// Test that when the upload to ODFS fails due an access error that is not
// because reauthentication to OneDrive is required, the generic error
// notification is shown.
IN_PROC_BROWSER_TEST_F(OneDriveUploadHandlerTest,
                       FailToUploadDueToOtherAccessError) {
  SetUpObservers();
  SetUpMyFiles();
  SetUpODFS();
  // Ensure Upload fails due to some access error which is not because
  // reauthentication to OneDrive is required.
  provided_file_system_->SetCreateFileError(
      base::File::Error::FILE_ERROR_ACCESS_DENIED);
  provided_file_system_->SetReauthenticationRequired(false);
  const std::string test_file_name = "text.docx";
  FileSystemURL source_file_url =
      SetUpSourceFile(test_file_name, my_files_dir_);

  // Start the upload workflow and end the test once the upload has failed due
  // to some access error.
  auto on_notification = base::BindLambdaForTesting(
      [&](const message_center::Notification& notification) {
        if (notification.message() ==
            base::UTF8ToUTF16(GetGenericErrorMessage())) {
          EndWait();
        }
      });
  SetOnNotificationDisplayedCallback(std::move(on_notification));
  OneDriveUploadHandler::Upload(profile(), source_file_url, base::DoNothing());
  Wait();

  // Check that the source file still exists only at the intended source
  // location and did not get uploaded to ODFS.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(my_files_dir_.AppendASCII(test_file_name)));
    CheckPathNotFoundOnODFS(base::FilePath("/").AppendASCII(test_file_name));
  }
}

}  // namespace ash::cloud_upload
