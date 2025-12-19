// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_CAMERA_CAMERA_SAVE_HANDLER_H_
#define CHROMEOS_ASH_EXPERIENCES_CAMERA_CAMERA_SAVE_HANDLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"

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

  std::unique_ptr<Delegate> delegate_;
  base::WeakPtrFactory<CameraSaveHandler> weak_ptr_factory_{this};
};

#endif  // CHROMEOS_ASH_EXPERIENCES_CAMERA_CAMERA_SAVE_HANDLER_H_
