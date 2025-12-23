// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_CAMERA_CAMERA_SAVE_HANDLER_H_
#define CHROMEOS_ASH_EXPERIENCES_CAMERA_CAMERA_SAVE_HANDLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/supports_user_data.h"
#include "ui/gfx/image/image.h"

namespace base {
class FilePath;
}  // namespace base

namespace gfx {
class Image;
}  // namespace gfx

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

    // Uploads the file with path `upload_from_path` to the cloud destination.
    // `file_size` is the size of the file in bytes.
    // `thumbnail` is an image for notifications that may be presented before
    // upload (e.g. sign-in notification)
    // `progress_callback` is called periodically during upload with the number
    // of bytes uploaded.
    // `done_callback` is called when the upload is finished with a boolean
    // indicating success or failure.
    virtual void PerformUpload(
        const base::FilePath& upload_from_path,
        int64_t file_size,
        const gfx::Image& thumbnail,
        base::RepeatingCallback<void(int64_t)> progress_callback,
        base::OnceCallback<void(bool)> done_callback) = 0;
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

  // Returns the root folder where the camera app will create a subfolder and
  // files will be written there before upload.
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
  explicit CameraSaveHandler(std::unique_ptr<Delegate> delegate);

  // Returns the initial folder where the camera app will write files to.
  base::FilePath GetWritablePath() const;
  void PerformUpload(const base::FilePath& upload_from_path,
                     const gfx::Image& thumbnail,
                     base::OnceCallback<void(bool)> callback,
                     std::optional<int64_t> file_size);
  void OnUploadProgress(const base::FilePath&, int64_t);
  void OnUploadDone(const base::FilePath&,
                    base::OnceCallback<void(bool)> callback,
                    bool);

  std::unique_ptr<Delegate> delegate_;
  base::WeakPtrFactory<CameraSaveHandler> weak_ptr_factory_{this};
};

#endif  // CHROMEOS_ASH_EXPERIENCES_CAMERA_CAMERA_SAVE_HANDLER_H_
