// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/camera/camera_save_handler.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/experiences/camera/cancel_camera_upload_dialog.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/constrained_window/constrained_window_views_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"

using FileSaveDestination = CameraSaveHandler::FileSaveDestination;
using testing::_;

namespace {

class MockCameraSaveHandlerDelegate : public CameraSaveHandler::Delegate {
 public:
  MOCK_METHOD(FileSaveDestination, GetDestination, (), (const, override));

  MOCK_METHOD(base::FilePath, GetMyFilesFolder, (), (const, override));
  MOCK_METHOD(base::FilePath, GetOneDriveUploadFolder, (), (const, override));
  MOCK_METHOD(base::FilePath, GetGoogleDriveRoot, (), (const, override));
  MOCK_METHOD(base::FilePath,
              GetFinalPathRelativeToRoot,
              (),
              (const, override));

  MOCK_METHOD(void,
              DeleteFileOnOneDrive,
              (const base::FilePath& file_path,
               base::OnceCallback<void(bool)> callback)),
      (override);
  MOCK_METHOD(void,
              PerformUpload,
              (const base::FilePath& upload_from_path,
               int64_t file_size,
               const gfx::Image& thumbnail,
               base::RepeatingCallback<void(int64_t)> progress_callback,
               base::OnceCallback<void(bool, std::optional<base::FilePath>)>
                   done_callback),
              (override));
  MOCK_METHOD(void, CancelUploads, (), (override));

  MOCK_METHOD(void,
              OpenFileInImageEditor,
              (const base::FilePath& file_path),
              (override));

  MOCK_METHOD(void, OpenCameraApp, (), (override));
};

class TestConstrainedWindowViewsClient
    : public constrained_window::ConstrainedWindowViewsClient {
 public:
  TestConstrainedWindowViewsClient() = default;

  TestConstrainedWindowViewsClient(const TestConstrainedWindowViewsClient&) =
      delete;
  TestConstrainedWindowViewsClient& operator=(
      const TestConstrainedWindowViewsClient&) = delete;

  // ConstrainedWindowViewsClient:
  web_modal::ModalDialogHost* GetModalDialogHost(
      gfx::NativeWindow parent) override {
    return nullptr;
  }
  gfx::NativeView GetDialogHostView(gfx::NativeWindow parent) override {
    return gfx::NativeView();
  }
};

constexpr char kMyFilesFolder[] = "/home/user/abcdef/MyFiles";
constexpr char kOneDriveUploadFolder[] =
    "/media/fuse/fusebox/onedrive_root/Camera";
constexpr char kOneDriveUploadedPath[] = "/uploaded/photo.jpg";
constexpr std::string kFileContents = "0123456789";  // 10 bytes

}  // namespace

namespace ash {

class CameraSaveHandlerTest : public AshTestBase {
 protected:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    auto mock_delegate = std::make_unique<MockCameraSaveHandlerDelegate>();
    mock_delegate_ = mock_delegate.get();
    camera_save_handler_ =
        base::WrapUnique(new CameraSaveHandler(std::move(mock_delegate)));
  }

  // AshTestBase:
  void TearDown() override {
    mock_delegate_ = nullptr;
    camera_save_handler_.reset();
    AshTestBase::TearDown();
  }

  void SetupDestination(FileSaveDestination destination) const {
    EXPECT_CALL(*mock_delegate_, GetDestination())
        .WillRepeatedly(testing::Return(destination));
  }

  void ValidateUploadProgressNotification(int expected_progress,
                                          int expected_count = 1) const {
    auto* notification =
        message_center::MessageCenter::Get()->FindVisibleNotificationById(
            "skyvault_camera_upload_notification");
    EXPECT_TRUE(!!notification);
    if (expected_count > 1) {
      EXPECT_TRUE(notification->progress_status().contains(
          base::NumberToString16(expected_count)));
    }
    EXPECT_EQ(notification->progress(), expected_progress);
  }

  bool UploadProgressNotificationShown() const {
    return !!message_center::MessageCenter::Get()->FindVisibleNotificationById(
        "skyvault_camera_upload_notification");
  }

  bool UploadDoneNotificationShown() const {
    return !!message_center::MessageCenter::Get()->FindVisibleNotificationById(
        "skyvault_camera_upload_done_notification");
  }

  bool UploadErrorNotificationShown() const {
    return !!message_center::MessageCenter::Get()->FindVisibleNotificationById(
        "skyvault_camera_upload_error_notification");
  }

  CancelCameraUploadDialog* cancel_upload_dialog() const {
    return camera_save_handler_->cancel_dialog_.get();
  }

  void ResetCancelUploadDialog() {
    camera_save_handler_->cancel_dialog_.reset();
  }

  std::unique_ptr<CameraSaveHandler> camera_save_handler_;
  raw_ptr<MockCameraSaveHandlerDelegate> mock_delegate_;
  const gfx::Image thumbnail_;
};

TEST_F(CameraSaveHandlerTest, GetPaths_Local) {
  SetupDestination(FileSaveDestination::kLocal);
  const base::FilePath my_files_folder(kMyFilesFolder);
  EXPECT_CALL(*mock_delegate_, GetMyFilesFolder())
      .WillRepeatedly(testing::Return(my_files_folder));
  EXPECT_EQ(camera_save_handler_->GetWritableRoot(), my_files_folder);
  EXPECT_EQ(camera_save_handler_->GetWritablePathRelativeToRoot(),
            base::FilePath("Camera"));
  EXPECT_EQ(camera_save_handler_->GetFinalPath(),
            my_files_folder.Append("Camera"));
}

TEST_F(CameraSaveHandlerTest, GetPaths_OneDrive) {
  SetupDestination(FileSaveDestination::kOneDrive);
  base::FilePath tmp_dir;
  base::GetTempDir(&tmp_dir);
  EXPECT_EQ(camera_save_handler_->GetWritableRoot(), tmp_dir);
  EXPECT_EQ(camera_save_handler_->GetWritablePathRelativeToRoot(),
            base::FilePath("CameraOneDriveCache"));
  const base::FilePath one_drive_upload_path(kOneDriveUploadFolder);
  EXPECT_CALL(*mock_delegate_, GetOneDriveUploadFolder())
      .WillOnce(testing::Return(one_drive_upload_path));
  EXPECT_EQ(camera_save_handler_->GetFinalPath(), one_drive_upload_path);
}

TEST_F(CameraSaveHandlerTest, GetPaths_GoogleDrive) {
  SetupDestination(FileSaveDestination::kGoogleDrive);
  const base::FilePath google_drive_root("/media/fuse/drivefs-abcdef/root/");
  EXPECT_CALL(*mock_delegate_, GetGoogleDriveRoot())
      .WillRepeatedly(testing::Return(google_drive_root));
  EXPECT_EQ(camera_save_handler_->GetWritableRoot(), google_drive_root);

  // By default the Camera folder should be used.
  EXPECT_CALL(*mock_delegate_, GetFinalPathRelativeToRoot())
      .WillRepeatedly(testing::Return(base::FilePath("")));
  EXPECT_EQ(camera_save_handler_->GetWritablePathRelativeToRoot(),
            base::FilePath("Camera"));
  EXPECT_EQ(camera_save_handler_->GetFinalPath(),
            google_drive_root.Append("Camera"));

  // Test custom subfolder.
  EXPECT_CALL(*mock_delegate_, GetFinalPathRelativeToRoot())
      .WillRepeatedly(testing::Return(base::FilePath("foo")));
  EXPECT_EQ(camera_save_handler_->GetWritablePathRelativeToRoot(),
            base::FilePath("foo"));
  EXPECT_EQ(camera_save_handler_->GetFinalPath(),
            google_drive_root.Append("foo"));
}

TEST_F(CameraSaveHandlerTest, UploadFile_GoogleDrive_MultiUpload_Success) {
  SetupDestination(FileSaveDestination::kGoogleDrive);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath& google_drive_root = temp_dir.GetPath();
  EXPECT_CALL(*mock_delegate_, GetGoogleDriveRoot())
      .WillRepeatedly(testing::Return(google_drive_root));
  EXPECT_CALL(*mock_delegate_, GetFinalPathRelativeToRoot())
      .WillRepeatedly(testing::Return(base::FilePath(".")));

  const base::FilePath upload_from_path =
      google_drive_root.Append("./photo.jpg");

  ASSERT_TRUE(base::WriteFile(upload_from_path, kFileContents));

  base::MockOnceCallback<void(bool)> done_callback;

  // Test successful upload.
  base::RepeatingCallback<void(int64_t)>
      first_progress_callback_passed_to_delegate;
  base::OnceCallback<void(bool, std::optional<base::FilePath>)>
      first_done_callback_passed_to_delegate;
  EXPECT_CALL(
      *mock_delegate_,
      PerformUpload(upload_from_path, kFileContents.size(), thumbnail_, _, _))
      .WillOnce([&](const base::FilePath&, int64_t, const gfx::Image&,
                    base::RepeatingCallback<void(int64_t)> progress_callback,
                    base::OnceCallback<void(
                        bool, std::optional<base::FilePath>)> done_callback) {
        // Initially there should be no progress shown.
        ValidateUploadProgressNotification(0);

        // Simulate some progress.
        progress_callback.Run(5);
        ValidateUploadProgressNotification(50);

        first_progress_callback_passed_to_delegate =
            std::move(progress_callback);
        first_done_callback_passed_to_delegate = std::move(done_callback);
      });
  EXPECT_CALL(done_callback, Run(_)).Times(0);
  camera_save_handler_->UploadFile("photo.jpg", thumbnail_,
                                   done_callback.Get());
  task_environment()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&done_callback);
  testing::Mock::VerifyAndClearExpectations(&mock_delegate_);
  EXPECT_TRUE(!!first_progress_callback_passed_to_delegate);
  EXPECT_TRUE(!!first_done_callback_passed_to_delegate);

  const base::FilePath upload_from_path2 =
      google_drive_root.Append("./photo2.jpg");
  ASSERT_TRUE(base::WriteFile(upload_from_path2, kFileContents));
  EXPECT_CALL(
      *mock_delegate_,
      PerformUpload(upload_from_path2, kFileContents.size(), thumbnail_, _, _))
      .WillOnce(
          [&](const base::FilePath&, int64_t, const gfx::Image&,
              base::RepeatingCallback<void(int64_t)> progress_callback2,
              base::OnceCallback<void(bool, std::optional<base::FilePath>)>
                  done_callback2) {
            // When second upload starts, progress should drop to 25 to reflect
            // both uploads.
            ValidateUploadProgressNotification(25, 2);

            progress_callback2.Run(5);
            ValidateUploadProgressNotification(50, 2);
            progress_callback2.Run(10);
            ValidateUploadProgressNotification(75, 2);

            // Complete second upload.
            std::move(done_callback2).Run(true, std::nullopt);

            // Once the second upload is complete, the progress should reflect
            // only the first upload.
            ValidateUploadProgressNotification(50);
          });
  base::MockOnceCallback<void(bool)> done_callback2;
  EXPECT_CALL(done_callback, Run(_)).Times(0);
  EXPECT_CALL(done_callback2, Run(true));
  camera_save_handler_->UploadFile("photo2.jpg", thumbnail_,
                                   done_callback2.Get());
  task_environment()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&done_callback);
  testing::Mock::VerifyAndClearExpectations(&done_callback2);
  testing::Mock::VerifyAndClearExpectations(&mock_delegate_);
  // Upload done notification should be shown after the second upload completes.
  EXPECT_TRUE(UploadDoneNotificationShown());
  EXPECT_FALSE(UploadErrorNotificationShown());
  // Now remove the upload done notification.
  message_center::MessageCenter::Get()->RemoveNotification(
      "skyvault_camera_upload_done_notification", false);
  // Update first upload progress to 100%.
  first_progress_callback_passed_to_delegate.Run(10);
  ValidateUploadProgressNotification(100);
  // Complete first upload.
  std::move(first_done_callback_passed_to_delegate).Run(true, std::nullopt);
  // No progress notification should remain after all uploads complete.
  EXPECT_FALSE(UploadProgressNotificationShown());
  // Upload done notification should be shown again after the first upload
  // completes.
  EXPECT_TRUE(UploadDoneNotificationShown());
  EXPECT_FALSE(UploadErrorNotificationShown());
}

TEST_F(CameraSaveHandlerTest, UploadFile_GoogleDrive_Failure) {
  SetupDestination(FileSaveDestination::kGoogleDrive);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath& google_drive_root = temp_dir.GetPath();
  EXPECT_CALL(*mock_delegate_, GetGoogleDriveRoot())
      .WillRepeatedly(testing::Return(google_drive_root));
  EXPECT_CALL(*mock_delegate_, GetFinalPathRelativeToRoot())
      .WillRepeatedly(testing::Return(base::FilePath(".")));

  const base::FilePath upload_from_path =
      google_drive_root.Append("./photo.jpg");

  ASSERT_TRUE(base::WriteFile(upload_from_path, kFileContents));

  base::MockOnceCallback<void(bool)> done_callback;

  EXPECT_CALL(
      *mock_delegate_,
      PerformUpload(upload_from_path, kFileContents.size(), thumbnail_, _, _))
      .WillOnce([](const base::FilePath&, int64_t, const gfx::Image&,
                   base::RepeatingCallback<void(int64_t)> progress_callback,
                   base::OnceCallback<void(bool, std::optional<base::FilePath>)>
                       done_callback) {
        std::move(done_callback).Run(false, std::nullopt);
      });
  EXPECT_CALL(done_callback, Run(false));
  camera_save_handler_->UploadFile("photo.jpg", thumbnail_,
                                   done_callback.Get());
  task_environment()->RunUntilIdle();
}

TEST_F(CameraSaveHandlerTest, UploadFile_GoogleDrive_FailedToGetFileSize) {
  SetupDestination(FileSaveDestination::kGoogleDrive);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath& google_drive_root = temp_dir.GetPath();
  EXPECT_CALL(*mock_delegate_, GetGoogleDriveRoot())
      .WillRepeatedly(testing::Return(google_drive_root));
  EXPECT_CALL(*mock_delegate_, GetFinalPathRelativeToRoot())
      .WillRepeatedly(testing::Return(base::FilePath(".")));

  const base::FilePath upload_from_path =
      google_drive_root.Append("./photo.jpg");

  ASSERT_TRUE(base::WriteFile(upload_from_path, kFileContents));

  base::MockOnceCallback<void(bool)> done_callback;

  EXPECT_TRUE(base::DeleteFile(upload_from_path));
  // No upload request or cancellation expected as upload is not started.
  EXPECT_CALL(*mock_delegate_, PerformUpload(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*mock_delegate_, CancelUploads()).Times(0);
  // Callback should be invoked with failure when upload is not performed.
  EXPECT_CALL(done_callback, Run(false));
  camera_save_handler_->UploadFile("photo.jpg", thumbnail_,
                                   done_callback.Get());
  task_environment()->RunUntilIdle();
}

TEST_F(CameraSaveHandlerTest, UploadFile_GoogleDrive_ExitDuringUpload) {
  SetupDestination(FileSaveDestination::kGoogleDrive);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath& google_drive_root = temp_dir.GetPath();
  EXPECT_CALL(*mock_delegate_, GetGoogleDriveRoot())
      .WillRepeatedly(testing::Return(google_drive_root));
  EXPECT_CALL(*mock_delegate_, GetFinalPathRelativeToRoot())
      .WillRepeatedly(testing::Return(base::FilePath(".")));

  const base::FilePath upload_from_path =
      google_drive_root.Append("./photo.jpg");

  ASSERT_TRUE(base::WriteFile(upload_from_path, kFileContents));

  base::MockOnceCallback<void(bool)> done_callback;

  EXPECT_CALL(
      *mock_delegate_,
      PerformUpload(upload_from_path, kFileContents.size(), thumbnail_, _, _));
  EXPECT_CALL(*mock_delegate_, CancelUploads());
  // Callback should be invoked with failure when upload is not performed.
  EXPECT_CALL(done_callback, Run(false));
  camera_save_handler_->UploadFile("photo.jpg", thumbnail_,
                                   done_callback.Get());
  task_environment()->RunUntilIdle();
  // Test upload callback is still invoked if CameraSaveHandler is destroyed
  // before upload completes.
  mock_delegate_ = nullptr;
  camera_save_handler_.reset();
}

TEST_F(CameraSaveHandlerTest, UploadFile_OneDrive_Success) {
  SetupDestination(FileSaveDestination::kOneDrive);

  EXPECT_CALL(*mock_delegate_, GetOneDriveUploadFolder())
      .WillRepeatedly(testing::Return(base::FilePath(kOneDriveUploadFolder)));

  base::FilePath tmp_dir;
  base::GetTempDir(&tmp_dir);
  base::FilePath one_drive_cache_dir = tmp_dir.Append("CameraOneDriveCache");
  EXPECT_TRUE(base::CreateDirectory(one_drive_cache_dir));
  base::ScopedClosureRunner scoped_cache_dir_deleter(base::BindOnce(
      [](const base::FilePath& path) { base::DeletePathRecursively(path); },
      one_drive_cache_dir));
  const base::FilePath upload_from_path =
      one_drive_cache_dir.Append("photo.jpg");

  ASSERT_TRUE(base::WriteFile(upload_from_path, kFileContents));

  base::MockOnceCallback<void(bool)> done_callback;

  // Test successful upload.
  EXPECT_CALL(
      *mock_delegate_,
      PerformUpload(upload_from_path, kFileContents.size(), thumbnail_, _, _))
      .WillOnce([](const base::FilePath&, int64_t, const gfx::Image&,
                   base::RepeatingCallback<void(int64_t)> progress_callback,
                   base::OnceCallback<void(bool, std::optional<base::FilePath>)>
                       done_callback) {
        progress_callback.Run(5);
        std::move(done_callback)
            .Run(true, base::FilePath(kOneDriveUploadedPath));
      });
  EXPECT_CALL(done_callback, Run(true));
  camera_save_handler_->UploadFile("photo.jpg", thumbnail_,
                                   done_callback.Get());
  task_environment()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&done_callback);
  testing::Mock::VerifyAndClearExpectations(&mock_delegate_);
  EXPECT_TRUE(UploadDoneNotificationShown());
  EXPECT_FALSE(UploadErrorNotificationShown());
}

TEST_F(CameraSaveHandlerTest, UploadFile_OneDrive_Failure) {
  SetupDestination(FileSaveDestination::kOneDrive);

  EXPECT_CALL(*mock_delegate_, GetOneDriveUploadFolder())
      .WillRepeatedly(testing::Return(base::FilePath(kOneDriveUploadFolder)));

  base::FilePath tmp_dir;
  base::GetTempDir(&tmp_dir);
  base::FilePath one_drive_cache_dir = tmp_dir.Append("CameraOneDriveCache");
  EXPECT_TRUE(base::CreateDirectory(one_drive_cache_dir));
  base::ScopedClosureRunner scoped_cache_dir_deleter(base::BindOnce(
      [](const base::FilePath& path) { base::DeletePathRecursively(path); },
      one_drive_cache_dir));
  const base::FilePath upload_from_path =
      one_drive_cache_dir.Append("photo.jpg");

  ASSERT_TRUE(base::WriteFile(upload_from_path, kFileContents));

  base::MockOnceCallback<void(bool)> done_callback;

  EXPECT_CALL(
      *mock_delegate_,
      PerformUpload(upload_from_path, kFileContents.size(), thumbnail_, _, _))
      .WillOnce([](const base::FilePath&, int64_t, const gfx::Image&,
                   base::RepeatingCallback<void(int64_t)> progress_callback,
                   base::OnceCallback<void(bool, std::optional<base::FilePath>)>
                       done_callback) {
        std::move(done_callback)
            .Run(false, base::FilePath(kOneDriveUploadedPath));
      });
  EXPECT_CALL(done_callback, Run(false));
  camera_save_handler_->UploadFile("photo.jpg", thumbnail_,
                                   done_callback.Get());
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(upload_from_path));
  EXPECT_FALSE(UploadDoneNotificationShown());
  EXPECT_TRUE(UploadErrorNotificationShown());
}

TEST_F(CameraSaveHandlerTest, CancelUpload) {
  SetupDestination(FileSaveDestination::kGoogleDrive);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath& google_drive_root = temp_dir.GetPath();
  EXPECT_CALL(*mock_delegate_, GetGoogleDriveRoot())
      .WillRepeatedly(testing::Return(google_drive_root));
  EXPECT_CALL(*mock_delegate_, GetFinalPathRelativeToRoot())
      .WillRepeatedly(testing::Return(base::FilePath(".")));

  const base::FilePath upload_from_path =
      google_drive_root.Append("./photo.jpg");

  ASSERT_TRUE(base::WriteFile(upload_from_path, kFileContents));

  auto* pref_service =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  std::optional<bool> original_pref_value;
  if (pref_service->FindPreference(prefs::kCameraAppSkipCancelUploadDialog)) {
    original_pref_value =
        pref_service->GetBoolean(prefs::kCameraAppSkipCancelUploadDialog);
  }
  base::ScopedClosureRunner restore_pref_on_exit(base::BindOnce(
      [](PrefService* prefs, std::optional<bool> original_value) {
        if (original_value.has_value()) {
          prefs->SetBoolean(prefs::kCameraAppSkipCancelUploadDialog,
                            original_value.value());
        } else {
          prefs->ClearPref(prefs::kCameraAppSkipCancelUploadDialog);
        }
      },
      pref_service, original_pref_value));

  base::MockOnceCallback<void(bool)> done_callback;

  // First test with the dialog shown.
  pref_service->SetBoolean(prefs::kCameraAppSkipCancelUploadDialog, false);
  SetConstrainedWindowViewsClient(
      std::make_unique<TestConstrainedWindowViewsClient>());
  EXPECT_CALL(
      *mock_delegate_,
      PerformUpload(upload_from_path, kFileContents.size(), thumbnail_, _, _))
      .WillOnce([&](const base::FilePath&, int64_t, const gfx::Image&,
                    base::RepeatingCallback<void(int64_t)> progress_callback,
                    base::OnceCallback<void(
                        bool, std::optional<base::FilePath>)> done_callback) {
        progress_callback.Run(5);
        EXPECT_CALL(*mock_delegate_, CancelUploads());
        // Press the cancel button.
        message_center::MessageCenter::Get()->ClickOnNotificationButton(
            "skyvault_camera_upload_notification", 0);
        EXPECT_TRUE(!!cancel_upload_dialog());
        cancel_upload_dialog()->clicked_callback_.Run(
            true,  // confirm cancellation
            true   // skip dialog next time
        );
      });
  EXPECT_CALL(done_callback, Run(false));
  camera_save_handler_->UploadFile("photo.jpg", thumbnail_,
                                   done_callback.Get());
  task_environment()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&done_callback);
  testing::Mock::VerifyAndClearExpectations(&mock_delegate_);
  EXPECT_FALSE(UploadProgressNotificationShown());
  EXPECT_FALSE(UploadDoneNotificationShown());
  EXPECT_FALSE(UploadErrorNotificationShown());

  // Now test with the dialog skipped.
  ResetCancelUploadDialog();
  EXPECT_CALL(
      *mock_delegate_,
      PerformUpload(upload_from_path, kFileContents.size(), thumbnail_, _, _))
      .WillOnce([&](const base::FilePath&, int64_t, const gfx::Image&,
                    base::RepeatingCallback<void(int64_t)> progress_callback,
                    base::OnceCallback<void(
                        bool, std::optional<base::FilePath>)> done_callback) {
        progress_callback.Run(5);
        EXPECT_CALL(*mock_delegate_, CancelUploads());
        // Press the cancel button.
        message_center::MessageCenter::Get()->ClickOnNotificationButton(
            "skyvault_camera_upload_notification", 0);
        // The dialog should be skipped this time.
        EXPECT_FALSE(!!cancel_upload_dialog());
      });
  EXPECT_CALL(done_callback, Run(false));
  camera_save_handler_->UploadFile("photo.jpg", thumbnail_,
                                   done_callback.Get());
  task_environment()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&done_callback);
  testing::Mock::VerifyAndClearExpectations(&mock_delegate_);
  EXPECT_FALSE(UploadProgressNotificationShown());
  EXPECT_FALSE(UploadDoneNotificationShown());
  EXPECT_FALSE(UploadErrorNotificationShown());
}

}  // namespace ash
