// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/one_drive_upload_handler.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
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
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_dir));
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

  // Creates mount point for MyFiles and registers local filesystem.
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
        file_manager::test::MountFakeProvidedFileSystemOneDrive(profile());
  }

  // Copy the test file with `test_file_name` into the directory `target_dir`,
  // optionally renaming it to `renamed_file_name`. Return the FileSystemURL of
  // the new (and maybe renamed) file.
  FileSystemURL CopyTestFile(
      const std::string& test_file_name,
      base::FilePath target_dir,
      std::optional<const std::string> renamed_file_name = std::nullopt) {
    const base::FilePath copied_file_path = target_dir.AppendASCII(
        renamed_file_name ? *renamed_file_name : test_file_name);
    // Copy the test file into `target_dir`.
    const base::FilePath test_file_path = GetTestFilePath(test_file_name);
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      CHECK(base::CopyFile(test_file_path, copied_file_path));
    }

    // Check that the copied file exists at the intended location and is not on
    // ODFS.
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(base::PathExists(copied_file_path));
      CheckPathNotFoundOnODFS(base::FilePath("/").AppendASCII(test_file_name));
    }

    FileSystemURL copied_file_url = FilePathToFileSystemURL(
        profile(),
        file_manager::util::GetFileManagerFileSystemContext(profile()),
        copied_file_path);

    return copied_file_url;
  }

  void SetUpObservers() {
    // Subscribe to Notification updates to track copy/move ODFS notifications.
    NotificationDisplayServiceFactory::GetForProfile(profile())->AddObserver(
        this);
  }

  void RemoveObservers() {
    NotificationDisplayServiceFactory::GetForProfile(browser()->profile())
        ->RemoveObserver(this);
  }

  void SetUpRunLoop(int conditions_to_end_wait = 1) {
    conditions_to_end_wait_ = conditions_to_end_wait;
    ASSERT_FALSE(run_loop_);
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void Wait() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    run_loop_->Run();
    run_loop_ = nullptr;
  }

  void SetUpRunLoopAndWait(int conditions_to_end_wait = 1) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    SetUpRunLoop(conditions_to_end_wait);
    Wait();
  }

  // Quits the run loop started with `Wait()` once `EndWait()` is called
  // `conditions_to_end_wait_` number of times.
  void EndWait() {
    conditions_to_end_wait_--;
    ASSERT_TRUE(run_loop_);
    if (conditions_to_end_wait_ == 0) {
      run_loop_->Quit();
    }
  }

  void CheckPathExistsOnODFS(const base::FilePath& path) {
    ASSERT_TRUE(provided_file_system_);
    provided_file_system_->GetMetadata(
        path, {},
        base::BindOnce(&OneDriveUploadHandlerTest::OnGetMetadataExpectSuccess,
                       base::Unretained(this)));
    base::ScopedAllowBlockingForTesting allow_blocking;
    SetUpRunLoopAndWait();
  }

  void CheckPathNotFoundOnODFS(const base::FilePath& path) {
    ASSERT_TRUE(provided_file_system_);
    provided_file_system_->GetMetadata(
        path, {},
        base::BindOnce(&OneDriveUploadHandlerTest::OnGetMetadataExpectNotFound,
                       base::Unretained(this)));
    base::ScopedAllowBlockingForTesting allow_blocking;
    SetUpRunLoopAndWait();
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
  void OnUploadSuccessful(
      OfficeTaskResult expected_task_result,
      OfficeTaskResult task_result,
      std::optional<storage::FileSystemURL> uploaded_file_url,
      int64_t size) {
    ASSERT_TRUE(uploaded_file_url.has_value());
    ASSERT_EQ(expected_task_result, task_result);
    EndWait();
  }

  // Watch for an invalid `uploaded_file_url`.
  void OnUploadFailedOrAbandoned(
      OfficeTaskResult expected_task_result,
      OfficeTaskResult task_result,
      std::optional<storage::FileSystemURL> uploaded_file_url,
      int64_t size) {
    ASSERT_FALSE(uploaded_file_url.has_value());
    ASSERT_EQ(expected_task_result, task_result);
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
  raw_ptr<file_manager::test::FakeProvidedFileSystemOneDrive,
          DanglingUntriaged>
      provided_file_system_;  // Owned by Service.
  std::unique_ptr<ash::cloud_upload::CloudOpenMetrics> cloud_open_metrics_ =
      std::make_unique<CloudOpenMetrics>(CloudProvider::kOneDrive,
                                         /*file_count=*/1);
  base::SafeRef<CloudOpenMetrics> cloud_open_metrics_ref_ =
      cloud_open_metrics_->GetSafeRef();
  base::HistogramTester histogram_;
  base::test::ScopedFeatureList feature_list_;

 private:
  base::ScopedTempDir temp_dir_;
  int conditions_to_end_wait_;
  std::unique_ptr<base::RunLoop> run_loop_;
  // Used to observe upload notifications during the tests.
  base::RepeatingCallback<void(const message_center::Notification&)>
      on_notification_displayed_callback_;
};

IN_PROC_BROWSER_TEST_F(OneDriveUploadHandlerTest, UploadFromMyFiles) {
  SetUpMyFiles();
  SetUpODFS();
  const std::string test_file_name = "text.docx";
  FileSystemURL source_file_url = CopyTestFile(test_file_name, my_files_dir_);

  // Start the upload workflow and end the test once the upload has completed
  // successfully.
  auto one_drive_upload_handler = std::make_unique<OneDriveUploadHandler>(
      profile(), source_file_url,
      base::BindOnce(&OneDriveUploadHandlerTest::OnUploadSuccessful,
                     base::Unretained(this),
                     /*expected_task_result=*/OfficeTaskResult::kMoved),
      cloud_open_metrics_ref_);
  one_drive_upload_handler->Run();
  SetUpRunLoopAndWait();

  // Check that the source file has been moved to OneDrive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_FALSE(base::PathExists(my_files_dir_.AppendASCII(test_file_name)));
    CheckPathExistsOnODFS(base::FilePath("/").AppendASCII(test_file_name));
  }

  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricName,
                                OfficeFilesUploadResult::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(OneDriveUploadHandlerTest, UploadTrimsFileName) {
  SetUpMyFiles();
  SetUpODFS();
  const std::string test_file_name = "text.docx";
  FileSystemURL source_file_url =
      CopyTestFile(test_file_name, my_files_dir_, "   text.docx");

  // Start the upload workflow and end the test once the upload has completed
  // successfully.
  auto one_drive_upload_handler = std::make_unique<OneDriveUploadHandler>(
      profile(), source_file_url,
      base::BindOnce(&OneDriveUploadHandlerTest::OnUploadSuccessful,
                     base::Unretained(this),
                     /*expected_task_result=*/OfficeTaskResult::kMoved),
      cloud_open_metrics_ref_);
  one_drive_upload_handler->Run();
  SetUpRunLoopAndWait();

  // Check that the source file has been moved to OneDrive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_FALSE(base::PathExists(my_files_dir_.AppendASCII("text.docx")));
    EXPECT_FALSE(base::PathExists(my_files_dir_.AppendASCII("   text.docx")));
    CheckPathExistsOnODFS(base::FilePath("/").AppendASCII("text.docx"));
  }

  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricName,
                                OfficeFilesUploadResult::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(OneDriveUploadHandlerTest,
                       UploadFromReadOnlyFileSystem) {
  SetUpReadOnlyLocation();
  SetUpODFS();
  const std::string test_file_name = "text.docx";
  FileSystemURL source_file_url = CopyTestFile(test_file_name, read_only_dir_);

  // Start the upload workflow and end the test once the upload has completed
  // successfully.
  auto one_drive_upload_handler = std::make_unique<OneDriveUploadHandler>(
      profile(), source_file_url,
      base::BindOnce(&OneDriveUploadHandlerTest::OnUploadSuccessful,
                     base::Unretained(this),
                     /*expected_task_result=*/OfficeTaskResult::kCopied),
      cloud_open_metrics_ref_);
  one_drive_upload_handler->Run();
  SetUpRunLoopAndWait();

  // Check that the source file has been copied to OneDrive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(read_only_dir_.AppendASCII(test_file_name)));
    CheckPathExistsOnODFS(base::FilePath("/").AppendASCII(test_file_name));
  }

  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricName,
                                OfficeFilesUploadResult::kSuccess, 1);
}

// Test that when the upload to ODFS fails due reauthentication to OneDrive
// being required, the reauthentication required notification is shown.
IN_PROC_BROWSER_TEST_F(OneDriveUploadHandlerTest,
                       FailToUploadDueToReauthenticationRequired) {
  SetUpObservers();
  SetUpMyFiles();
  SetUpODFS();
  provided_file_system_->SetReauthenticationRequired(false);
  // Ensure upload fails due to reauthentication to OneDrive being required. We
  // want to test ReauthRequired being set after we begin uploading.
  provided_file_system_->SetCreateFileCallback(
      base::BindLambdaForTesting([&]() {
        // Wait until we try to create the file, and then set reauth required to
        // emulate auth being lost during upload.
        provided_file_system_->SetCreateFileError(
            base::File::Error::FILE_ERROR_ACCESS_DENIED);
        provided_file_system_->SetReauthenticationRequired(true);
      }));

  // Expect the reauthentication required notification.
  const std::string test_file_name = "text.docx";
  FileSystemURL source_file_url = CopyTestFile(test_file_name, my_files_dir_);

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
  auto one_drive_upload_handler = std::make_unique<OneDriveUploadHandler>(
      profile(), source_file_url,
      base::BindOnce(
          &OneDriveUploadHandlerTest::OnUploadFailedOrAbandoned,
          base::Unretained(this),
          /*expected_task_result=*/OfficeTaskResult::kFailedToUpload),
      cloud_open_metrics_ref_);
  one_drive_upload_handler->Run();
  SetUpRunLoopAndWait(/*conditions_to_end_wait=*/2);

  // Check that the source file still exists only at the intended source
  // location and did not get uploaded to ODFS.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(my_files_dir_.AppendASCII(test_file_name)));
    CheckPathNotFoundOnODFS(base::FilePath("/").AppendASCII(test_file_name));
  }

  histogram_.ExpectUniqueSample(kOneDriveMoveErrorMetricName,
                                -base::File::FILE_ERROR_ACCESS_DENIED, 1);
  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricName,
                                OfficeFilesUploadResult::kCloudReauthRequired,
                                1);
}

// Test that when the upload to ODFS fails due reauthentication to OneDrive
// being required (before starting the upload), the reauthentication required
// notification is shown.
IN_PROC_BROWSER_TEST_F(OneDriveUploadHandlerTest,
                       FailToStartUploadDueToReauthenticationRequired) {
  SetUpObservers();
  SetUpMyFiles();
  SetUpODFS();
  // Ensure upload fails due to reauthentication to OneDrive being required.
  provided_file_system_->SetReauthenticationRequired(true);
  // Expect the reauthentication required notification.
  const std::string test_file_name = "text.docx";
  FileSystemURL source_file_url = CopyTestFile(test_file_name, my_files_dir_);

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
  SetUpRunLoop(/*conditions_to_end_wait=*/2);
  auto one_drive_upload_handler = std::make_unique<OneDriveUploadHandler>(
      profile(), source_file_url,
      base::BindOnce(
          &OneDriveUploadHandlerTest::OnUploadFailedOrAbandoned,
          base::Unretained(this),
          /*expected_task_result=*/OfficeTaskResult::kFailedToUpload),
      cloud_open_metrics_ref_);
  one_drive_upload_handler->Run();
  Wait();

  // Check that the source file still exists only at the intended source
  // location and did not get uploaded to ODFS.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(my_files_dir_.AppendASCII(test_file_name)));
    CheckPathNotFoundOnODFS(base::FilePath("/").AppendASCII(test_file_name));
  }

  histogram_.ExpectTotalCount(kOneDriveMoveErrorMetricName, 0);
  histogram_.ExpectUniqueSample(
      kOneDriveUploadResultMetricName,
      OfficeFilesUploadResult::kUploadNotStartedReauthenticationRequired, 1);
}

// Tests that an appropriate error is shown when INVALID_URL is returned
// (matches to a rejected request in ODFS).
IN_PROC_BROWSER_TEST_F(OneDriveUploadHandlerTest, FailToUploadDueToInvalidUrl) {
  SetUpObservers();
  SetUpMyFiles();
  SetUpODFS();
  // Set up upload to fail with INVALID_URL.
  provided_file_system_->SetCreateFileError(
      base::File::Error::FILE_ERROR_INVALID_URL);
  // Set up a test source file.
  const std::string test_file_name = "text.docx";
  FileSystemURL source_file_url = CopyTestFile(test_file_name, my_files_dir_);

  // Start the upload workflow and end once the error notification is shown.
  std::u16string message;
  SetOnNotificationDisplayedCallback(base::BindLambdaForTesting(
      [&](const message_center::Notification& notification) {
        message = notification.message();
        EndWait();
      }));
  SetUpRunLoop(/*conditions_to_end_wait=*/2);
  auto one_drive_upload_handler = std::make_unique<OneDriveUploadHandler>(
      profile(), source_file_url,
      base::BindOnce(
          &OneDriveUploadHandlerTest::OnUploadFailedOrAbandoned,
          base::Unretained(this),
          /*expected_task_result=*/OfficeTaskResult::kFailedToUpload),
      cloud_open_metrics_ref_);
  one_drive_upload_handler->Run();
  Wait();
  EXPECT_EQ(
      message,
      u"Microsoft OneDrive rejected the request. Please try again later.");

  // Check that the source file still exists only at the intended source
  // location and did not get uploaded to ODFS.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(my_files_dir_.AppendASCII(test_file_name)));
    CheckPathNotFoundOnODFS(base::FilePath("/").AppendASCII(test_file_name));
  }

  histogram_.ExpectTotalCount(kOneDriveMoveErrorMetricName, 1);
  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricName,
                                OfficeFilesUploadResult::kMoveOperationError,
                                1);
}

class OneDriveUploadHandlerTest_ReauthEnabled
    : public OneDriveUploadHandlerTest {
 public:
  OneDriveUploadHandlerTest_ReauthEnabled() {
    feature_list_.Reset();
    feature_list_.InitWithFeatures({chromeos::features::kUploadOfficeToCloud,
                                    features::kOneDriveUploadImmediateReauth},
                                   {});
  }
};

// Test that when reauthentication to OneDrive is required (before starting the
// upload) interactive auth is requested without a prompt. And test that when
// the auth succeeds, the upload succeeds.
IN_PROC_BROWSER_TEST_F(OneDriveUploadHandlerTest_ReauthEnabled,
                       UploadSucceedsAfterReauth) {
  SetUpMyFiles();
  SetUpODFS();
  // Ensure the first check of reauth required fails due to reauthentication to
  // OneDrive being required.
  provided_file_system_->SetReauthenticationRequired(true);
  const std::string test_file_name = "text.docx";
  FileSystemURL source_file_url = CopyTestFile(test_file_name, my_files_dir_);

  // Start the upload workflow and simulate a successful mount() request
  // (indicating interactive auth has succeeded).
  file_manager::test::GetFakeProviderOneDrive(profile())->SetRequestMountImpl(
      base::BindLambdaForTesting(
          [&](ash::file_system_provider::RequestMountCallback callback) {
            // The second check of reauth required after the mount succeeds
            // should be OK so we attempt upload.
            provided_file_system_->SetReauthenticationRequired(false);
            std::move(callback).Run(base::File::Error::FILE_OK);
          }));

  auto one_drive_upload_handler = std::make_unique<OneDriveUploadHandler>(
      profile(), source_file_url,
      base::BindOnce(&OneDriveUploadHandlerTest::OnUploadSuccessful,
                     base::Unretained(this),
                     /*expected_task_result=*/OfficeTaskResult::kMoved),
      cloud_open_metrics_ref_);
  one_drive_upload_handler->Run();
  SetUpRunLoopAndWait();

  // Check that the source file has been moved to OneDrive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_FALSE(base::PathExists(my_files_dir_.AppendASCII(test_file_name)));
    CheckPathExistsOnODFS(base::FilePath("/").AppendASCII(test_file_name));
  }

  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricName,
                                OfficeFilesUploadResult::kSuccessAfterReauth,
                                1);
}

// Test that when reauthentication to OneDrive is required (before starting the
// upload) interactive auth is requested without a prompt. And test that when
// the auth fails, the upload is not attempted and instead the sign-in
// notification is shown.
IN_PROC_BROWSER_TEST_F(OneDriveUploadHandlerTest,
                       UploadNotAttemptedAfterFailedReauth) {
  SetUpObservers();
  SetUpMyFiles();
  SetUpODFS();
  // Ensure the first check of reauth required fails due to reauthentication to
  // OneDrive being required.
  provided_file_system_->SetReauthenticationRequired(true);
  const std::string test_file_name = "text.docx";
  FileSystemURL source_file_url = CopyTestFile(test_file_name, my_files_dir_);

  // Start the upload workflow and simulate a failed mount() request (indicating
  // interactive auth has failed).
  file_manager::test::GetFakeProviderOneDrive(profile())->SetRequestMountImpl(
      base::BindLambdaForTesting(
          [&](ash::file_system_provider::RequestMountCallback callback) {
            std::move(callback).Run(base::File::Error::FILE_ERROR_FAILED);
          }));

  auto on_notification = base::BindLambdaForTesting(
      [&](const message_center::Notification& notification) {
        if (notification.message() ==
            base::UTF8ToUTF16(GetReauthenticationRequiredMessage())) {
          EndWait();
        }
      });
  SetOnNotificationDisplayedCallback(std::move(on_notification));
  SetUpRunLoop(/*conditions_to_end_wait=*/2);
  auto one_drive_upload_handler = std::make_unique<OneDriveUploadHandler>(
      profile(), source_file_url,
      base::BindOnce(
          &OneDriveUploadHandlerTest::OnUploadFailedOrAbandoned,
          base::Unretained(this),
          /*expected_task_result=*/OfficeTaskResult::kFailedToUpload),
      cloud_open_metrics_ref_);
  one_drive_upload_handler->Run();
  Wait();
  // Check that the source file still exists only at the intended source
  // location and did not get uploaded to ODFS.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(my_files_dir_.AppendASCII(test_file_name)));
    CheckPathNotFoundOnODFS(base::FilePath("/").AppendASCII(test_file_name));
  }

  histogram_.ExpectTotalCount(kOneDriveMoveErrorMetricName, 0);
  histogram_.ExpectUniqueSample(
      kOneDriveUploadResultMetricName,
      OfficeFilesUploadResult::kUploadNotStartedReauthenticationRequired, 1);
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
  FileSystemURL source_file_url = CopyTestFile(test_file_name, my_files_dir_);

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
  auto one_drive_upload_handler = std::make_unique<OneDriveUploadHandler>(
      profile(), source_file_url,
      base::BindOnce(
          &OneDriveUploadHandlerTest::OnUploadFailedOrAbandoned,
          base::Unretained(this),
          /*expected_task_result=*/OfficeTaskResult::kFailedToUpload),
      cloud_open_metrics_ref_);
  one_drive_upload_handler->Run();
  SetUpRunLoopAndWait(/*conditions_to_end_wait=*/2);

  // Check that the source file still exists only at the intended source
  // location and did not get uploaded to ODFS.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(my_files_dir_.AppendASCII(test_file_name)));
    CheckPathNotFoundOnODFS(base::FilePath("/").AppendASCII(test_file_name));
  }

  histogram_.ExpectUniqueSample(kOneDriveMoveErrorMetricName,
                                -base::File::FILE_ERROR_ACCESS_DENIED, 1);
  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricName,
                                OfficeFilesUploadResult::kCloudAccessDenied, 1);
}

// Tests that when there is an upload occurring followed by a second, unrelated
// upload, both are successful.
IN_PROC_BROWSER_TEST_F(OneDriveUploadHandlerTest, UnrelatedUploads) {
  SetUpObservers();
  SetUpMyFiles();
  SetUpODFS();
  const std::string test_file_name1 = "text.docx";
  FileSystemURL source_file_url1 = CopyTestFile(test_file_name1, my_files_dir_);
  const std::string test_file_name2 = "presentation.pptx";
  FileSystemURL source_file_url2 = CopyTestFile(test_file_name2, my_files_dir_);

  // Start the second, unrelated, upload after the first one starts.
  std::unique_ptr<OneDriveUploadHandler> one_drive_upload_handler2;
  provided_file_system_->SetCreateFileCallback(
      base::BindLambdaForTesting([&]() {
        one_drive_upload_handler2 = std::make_unique<OneDriveUploadHandler>(
            profile(), source_file_url2,
            base::BindOnce(&OneDriveUploadHandlerTest::OnUploadSuccessful,
                           base::Unretained(this),
                           /*expected_task_result=*/OfficeTaskResult::kMoved),
            cloud_open_metrics_ref_);
        one_drive_upload_handler2->Run();
      }));

  // Start the first upload.
  auto one_drive_upload_handler1 = std::make_unique<OneDriveUploadHandler>(
      profile(), source_file_url1,
      base::BindOnce(&OneDriveUploadHandlerTest::OnUploadSuccessful,
                     base::Unretained(this),
                     /*expected_task_result=*/OfficeTaskResult::kMoved),
      cloud_open_metrics_ref_);
  one_drive_upload_handler1->Run();

  SetUpRunLoopAndWait(/*conditions_to_end_wait=*/2);

  // Check that the first source file has been moved to OneDrive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_FALSE(base::PathExists(my_files_dir_.AppendASCII(test_file_name1)));
    CheckPathExistsOnODFS(base::FilePath("/").AppendASCII(test_file_name1));
  }

  // Check that the second source file has been moved to OneDrive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_FALSE(base::PathExists(my_files_dir_.AppendASCII(test_file_name2)));
    CheckPathExistsOnODFS(base::FilePath("/").AppendASCII(test_file_name2));
  }

  // There should be two UploadResults.
  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricName,
                                OfficeFilesUploadResult::kSuccess, 2);
}

}  // namespace ash::cloud_upload
