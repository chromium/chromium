// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/drive_upload_handler.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gmock/include/gmock/gmock.h"

using storage::FileSystemURL;

namespace ash::cloud_upload {

using ::base::test::RunOnceCallback;
using testing::_;

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

// Tests the Drive upload workflow using the static `DriveUploadHandler::Upload`
// method. Ensures that the upload completes with the expected results.
class DriveUploadHandlerTest
    : public InProcessBrowserTest,
      public file_manager::io_task::IOTaskController::Observer {
 public:
  DriveUploadHandlerTest() {
    feature_list_.InitAndEnableFeature(
        chromeos::features::kUploadOfficeToCloud);
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    drive_mount_point_ = temp_dir_.GetPath().Append("drivefs");
    drive_root_dir_ = drive_mount_point_.AppendASCII("root");
    my_files_dir_ = temp_dir_.GetPath().Append("myfiles");
    read_only_dir_ = temp_dir_.GetPath().Append("readonly");
  }

  DriveUploadHandlerTest(const DriveUploadHandlerTest&) = delete;
  DriveUploadHandlerTest& operator=(const DriveUploadHandlerTest&) = delete;

  void SetUpInProcessBrowserTestFixture() override {
    // Setup drive integration service.
    create_drive_integration_service_ = base::BindRepeating(
        &DriveUploadHandlerTest::CreateDriveIntegrationService,
        base::Unretained(this));
    service_factory_for_test_ = std::make_unique<
        drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
        &create_drive_integration_service_);
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
  }

  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    fake_drivefs_helpers_[profile] =
        std::make_unique<drive::FakeDriveFsHelper>(profile, drive_mount_point_);
    auto* integration_service = new drive::DriveIntegrationService(
        profile, "", drive_mount_point_,
        fake_drivefs_helpers_[profile]->CreateFakeDriveFsListenerFactory());
    return integration_service;
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
        ash::DeviceType::kUnknown, /*read_only=*/true);
  }

  void SetUpSourceFile(const std::string& test_file_name,
                       base::FilePath source_path) {
    test_file_name_ = test_file_name;
    source_file_path_ = source_path.AppendASCII(test_file_name);
    const base::FilePath test_file_path = GetTestFilePath(test_file_name_);
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      CHECK(base::CopyFile(test_file_path, source_file_path_));
    }
  }

  // Starts the upload flow.
  void InitiateUpload() {
    // Subscribe to IOTasks updates to track the copy/move to Drive progress.
    file_manager::VolumeManager::Get(profile())
        ->io_task_controller()
        ->AddObserver(this);

    FileSystemURL source_file_url = FilePathToFileSystemURL(
        profile(),
        file_manager::util::GetFileManagerFileSystemContext(profile()),
        source_file_path_);
    DriveUploadHandler::Upload(
        profile(), source_file_url,
        base::BindOnce(&DriveUploadHandlerTest::OnUploadDone,
                       base::Unretained(this)));
  }

  // Resolves once the `OnUploadDone` callback is called with a valid URL, which
  // indicates the successful completion of the upload flow.
  void WaitForUploadComplete() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_FALSE(run_loop_);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_ = nullptr;
  }

  Profile* profile() { return browser()->profile(); }

  const base::FilePath source_file_path() { return source_file_path_; }

  mojo::Remote<drivefs::mojom::DriveFsDelegate>& drivefs_delegate() {
    return fake_drivefs().delegate();
  }

  base::FilePath observed_relative_drive_path() {
    drive::DriveIntegrationService* drive_integration_service =
        drive::DriveIntegrationServiceFactory::FindForProfile(profile());
    base::FilePath observed_relative_drive_path;
    drive_integration_service->GetRelativeDrivePath(
        drive_root_dir_.AppendASCII(test_file_name_),
        &observed_relative_drive_path);
    return observed_relative_drive_path;
  }

 protected:
  drivefs::FakeDriveFs& fake_drivefs() {
    return fake_drivefs_helpers_[profile()]->fake_drivefs();
  }

  base::FilePath my_files_dir_;
  base::FilePath read_only_dir_;
  base::FilePath drive_mount_point_;
  base::FilePath drive_root_dir_;

 private:
  // IOTaskController::Observer:
  void OnIOTaskStatus(
      const file_manager::io_task::ProgressStatus& status) override {
    if (status.sources.size() == 1 &&
        status.sources[0].url.path() == source_file_path_ &&
        status.state == file_manager::io_task::State::kSuccess) {
      SimulateDriveUploadEvents();
    }
  }

  // Simulates the upload of the file to Drive by sending a series of fake
  // signals to the DriveFs delegate.
  void SimulateDriveUploadEvents() {
    // Set file metadata for `drivefs::mojom::DriveFs::GetMetadata`.
    drivefs::FakeMetadata metadata;
    metadata.path = observed_relative_drive_path();
    metadata.mime_type =
        "application/"
        "vnd.openxmlformats-officedocument.wordprocessingml.document";
    metadata.original_name = test_file_name_;
    metadata.doc_id = "abc123";
    metadata.alternate_url =
        "https://docs.google.com/document/d/"
        "smalldocxid?rtpof=true&usp=drive_fs";
    fake_drivefs().SetMetadata(std::move(metadata));

    // Simulate server sync events.
    drivefs::mojom::SyncingStatusPtr status =
        drivefs::mojom::SyncingStatus::New();
    status->item_events.emplace_back(
        absl::in_place, 12, 34, observed_relative_drive_path().value(),
        drivefs::mojom::ItemEvent::State::kQueued, 123, 456,
        drivefs::mojom::ItemEventReason::kTransfer);
    drivefs_delegate()->OnSyncingStatusUpdate(status.Clone());
    drivefs_delegate().FlushForTesting();

    status = drivefs::mojom::SyncingStatus::New();
    status->item_events.emplace_back(
        absl::in_place, 12, 34, observed_relative_drive_path().value(),
        drivefs::mojom::ItemEvent::State::kCompleted, 123, 456,
        drivefs::mojom::ItemEventReason::kTransfer);
    drivefs_delegate()->OnSyncingStatusUpdate(status.Clone());
    drivefs_delegate().FlushForTesting();
  }

  // The exit point of the test. `WaitForUploadComplete` will not complete until
  // this is called.
  void OnUploadDone(const GURL& url) {
    ASSERT_FALSE(url.is_empty());
    ASSERT_TRUE(run_loop_);
    run_loop_->Quit();
  }

  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::RunLoop> run_loop_;

  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
  std::map<Profile*, std::unique_ptr<drive::FakeDriveFsHelper>>
      fake_drivefs_helpers_;

  // Used to track the upload progress during the tests.
  std::string test_file_name_;
  base::FilePath source_file_path_;
};

IN_PROC_BROWSER_TEST_F(DriveUploadHandlerTest, UploadFromMyFiles) {
  const std::string test_file_name = "text.docx";
  SetUpMyFiles();

  // Define the source file as a test docx file within My files.
  SetUpSourceFile(test_file_name, my_files_dir_);

  // Create Drive root directory.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::CreateDirectory(drive_root_dir_));
  }

  // Check that the source file exists at the intended source location.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(my_files_dir_.AppendASCII(test_file_name)));
    EXPECT_FALSE(base::PathExists(drive_root_dir_.AppendASCII(test_file_name)));
  }

  EXPECT_CALL(fake_drivefs(), ImmediatelyUpload(_, _))
      .WillOnce(RunOnceCallback<1>(drive::FileError::FILE_ERROR_OK));

  InitiateUpload();
  WaitForUploadComplete();

  // Check that the source file has been moved to Drive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_FALSE(base::PathExists(my_files_dir_.AppendASCII(test_file_name)));
    EXPECT_TRUE(base::PathExists(drive_root_dir_.AppendASCII(test_file_name)));
  }
}

IN_PROC_BROWSER_TEST_F(DriveUploadHandlerTest, UploadFromReadOnlyFileSystem) {
  const std::string test_file_name = "text.docx";
  SetUpReadOnlyLocation();

  // Define the source file as a test docx file within My files.
  SetUpSourceFile(test_file_name, read_only_dir_);

  // Create Drive root directory.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::CreateDirectory(drive_root_dir_));
  }

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(read_only_dir_.AppendASCII(test_file_name)));
    EXPECT_FALSE(base::PathExists(drive_root_dir_.AppendASCII(test_file_name)));
  }

  EXPECT_CALL(fake_drivefs(), ImmediatelyUpload(_, _))
      .WillOnce(RunOnceCallback<1>(drive::FileError::FILE_ERROR_OK));

  InitiateUpload();
  WaitForUploadComplete();

  // Check that the source file has been copied to Drive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(read_only_dir_.AppendASCII(test_file_name)));
    EXPECT_TRUE(base::PathExists(drive_root_dir_.AppendASCII(test_file_name)));
  }
}

}  // namespace ash::cloud_upload
