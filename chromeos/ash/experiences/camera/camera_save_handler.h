// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_CAMERA_CAMERA_SAVE_HANDLER_H_
#define CHROMEOS_ASH_EXPERIENCES_CAMERA_CAMERA_SAVE_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "ui/gfx/image/image.h"

namespace ash {
class CameraSaveHandlerTest;
}

namespace base {
class FilePath;
}  // namespace base

namespace gfx {
class Image;
}  // namespace gfx

class CameraUploadNotification;
class CancelCameraUploadDialog;

// Browser-side handler for camera save operations.
class CameraSaveHandler : public base::SupportsUserData::Data {
 public:
  enum class FileSaveDestination {
    kLocal,
    kGoogleDrive,
    kOneDrive,
  };

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns the local or cloud destination type for saving files.
    virtual FileSaveDestination GetDestination() const = 0;

    // Returns the MyFiles folder on the local file system.
    virtual base::FilePath GetMyFilesFolder() const = 0;

    // Returns the OneDrive folder path where files will be uploaded.
    virtual base::FilePath GetOneDriveUploadFolder() const = 0;

    // Returns the Google Drive root folder path where Camera app will create a
    // subfolder and files will be saved there.
    virtual base::FilePath GetGoogleDriveRoot() const = 0;

    // Returns the final path relative to the root folder where files will be
    // saved.
    virtual base::FilePath GetFinalPathRelativeToRoot() const = 0;

    // Deletes the file with path `file_path` from OneDrive.
    virtual void DeleteFileOnOneDrive(
        const base::FilePath& file_path,
        base::OnceCallback<void(bool)> callback) = 0;

    // Uploads the file with path `upload_from_path` to the cloud destination.
    // `file_size` is the size of the file in bytes.
    // `thumbnail` is an image for notifications that may be presented before
    // upload (e.g. sign-in notification)
    // `progress_callback` is called periodically during upload with the number
    // of bytes uploaded.
    // `done_callback` is called when the upload is finished with a boolean
    // indicating success or failure and optionally the uploaded file path.
    virtual void PerformUpload(
        const base::FilePath& upload_from_path,
        int64_t file_size,
        const gfx::Image& thumbnail,
        base::RepeatingCallback<void(int64_t)> progress_callback,
        base::OnceCallback<void(bool,
                                std::optional<base::FilePath> uploaded_path)>
            done_callback) = 0;

    // Cancels all ongoing uploads.
    virtual void CancelUploads() = 0;

    // Opens the file for editing from the upload done notification.
    virtual void OpenFileInImageEditor(const base::FilePath& file_path) = 0;

    // Opens the camera app from the upload error notification retake button.
    virtual void OpenCameraApp() = 0;
  };

  CameraSaveHandler(const CameraSaveHandler&) = delete;
  CameraSaveHandler& operator=(const CameraSaveHandler&) = delete;

  ~CameraSaveHandler() override;

  // Returns the CameraSaveHandler for this `context`. Can be null if it hasn't
  // been created yet.
  static CameraSaveHandler* Get(const base::SupportsUserData& context);

  // Creates a CameraSaveHandler for this `context` with the provided
  // `delegate`. This should be called only if there is no existing
  // CameraSaveHandler for the `context`.
  static void Create(base::SupportsUserData& context,
                     std::unique_ptr<Delegate> delegate);

  // Returns the local or cloud destination type for saving files.
  FileSaveDestination GetDestination() const;

  // Returns the root folder where the camera app will create a subfolder
  // and files will be written there before upload.
  base::FilePath GetWritableRoot() const;

  // Returns the subfolder path relative to the writable root where files will
  // be saved by the app before upload.
  base::FilePath GetWritablePathRelativeToRoot() const;

  // Returns the final folder where the camera app files will be saved.
  base::FilePath GetFinalPath() const;

  // Upload the file to the cloud.
  void UploadFile(const std::string& name,
                  const gfx::Image& thumbnail,
                  base::OnceCallback<void(bool)> callback);

 private:
  friend class ash::CameraSaveHandlerTest;

  // Data to track upload progress per file.
  struct Upload {
    Upload(base::OnceCallback<void(bool)> done_callback, int64_t file_size);
    ~Upload();
    base::OnceCallback<void(bool)> done_callback;
    int64_t file_size;           // Size of the file in bytes.
    int64_t bytes_uploaded = 0;  // Bytes uploaded so far.
  };

  explicit CameraSaveHandler(std::unique_ptr<Delegate> delegate);

  // Returns the initial folder where the camera app will write files to.
  base::FilePath GetWritablePath() const;
  // Returns the full path of the file with `base_name` in the writable folder
  // returned by `GetWritablePath()`.
  base::FilePath GetFilePathBeforeUpload(const base::FilePath& base_name) const;
  void PerformUpload(const base::FilePath& upload_from_path,
                     const gfx::Image& thumbnail,
                     base::OnceCallback<void(bool)> callback,
                     std::optional<int64_t> file_size)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  // Tracks the size and bytes uploaded of this file at `upload_from_path` to
  // show a single progress notification across all uploads.
  void TrackUpload(const base::FilePath& upload_from_path,
                   std::unique_ptr<Upload> upload)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  // Removes this file at `upload_from_path` from being tracked for upload
  // progress.
  void UntrackUpload(const base::FilePath& upload_from_path, bool success)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  // Creates, modifies or resets the progress notification based on current
  // upload progress.
  void UpdateProgressNotification() VALID_CONTEXT_REQUIRED(sequence_checker_);
  // Shows a dialog to confirm cancellation of ongoing uploads.
  void ShowCancelDialog();
  // Handles user action on the cancel upload confirmation dialog.
  void OnCancelDialogClicked(bool cancel, bool skip_dialog_next_time)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  // Cancels all ongoing uploads and resets progress tracking.
  void CancelUploads() VALID_CONTEXT_REQUIRED(sequence_checker_);
  // Gets called for each file when its upload progress is updated.
  void OnUploadProgress(const base::FilePath&, int64_t)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  // Gets called for each file when it is no longer being uploaded.
  void OnUploadDone(const base::FilePath& upload_from_path,
                    const gfx::Image& thumbnail,
                    bool success,
                    std::optional<base::FilePath> uploaded_path)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void OpenFileInImageEditor(const base::FilePath& file_path);
  void DeleteFileAfterUpload(const base::FilePath& file_path);
  void OnUploadErrorRetake();

  std::unique_ptr<Delegate> delegate_;
  std::unique_ptr<CameraUploadNotification> progress_notification_;
  // Map from file base name to its upload tracking data.
  std::map<base::FilePath, std::unique_ptr<Upload>> uploads_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<CancelCameraUploadDialog> cancel_dialog_;
  // Total bytes that have been uploaded so far across all uploads.
  int64_t total_bytes_uploaded_ = 0;
  // Total size of all uploads in bytes.
  int64_t total_size_of_uploads_ = 0;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CameraSaveHandler> weak_ptr_factory_{this};
};

#endif  // CHROMEOS_ASH_EXPERIENCES_CAMERA_CAMERA_SAVE_HANDLER_H_
