// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_CAMERA_ROLL_DOWNLOAD_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_CAMERA_ROLL_DOWNLOAD_MANAGER_H_

#include <optional>

#include "base/functional/callback.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"

namespace ash {
namespace phonehub {

// Manages photo and videos files downloaded via Camera Roll. Files will be
// created under the Downloads folder and added to the Holding Space tray.
class CameraRollDownloadManager {
 public:
  CameraRollDownloadManager(const CameraRollDownloadManager&) = delete;
  CameraRollDownloadManager& operator=(const CameraRollDownloadManager&) =
      delete;
  virtual ~CameraRollDownloadManager() = default;

  enum class CreatePayloadFilesResult {
    // The payload files are created successfully.
    kSuccess,
    // The payload files cannot be created because the file name provided was
    // invalid.
    kInvalidFileName,
    // The payload files cannot be created because they have already been
    // created for the provided payload ID.
    kPayloadAlreadyExists,
    // The payload files cannot be created because there is not enough free disk
    // space for the item requested.
    kInsufficientDiskSpace,
    // The payload files cannot be created because a file already exists at the
    // target path, likely a result of some race conditions.
    kNotUniqueFilePath,
  };

  // Creates payload files that can be used to receive an incoming file transfer
  // for the given |payload_id|. The file will be created under the Downloads
  // folder with the file name provided in the |item_metadata|. If the file
  // creation succeeds, the file will be passed back via
  // |payload_files_callback| with the result code |kSuccess|. Otherwise an
  // empty optional will be passed back along with a result code indicating the
  // error.
  using CreatePayloadFilesCallback = base::OnceCallback<void(
      CreatePayloadFilesResult,
      std::optional<secure_channel::mojom::PayloadFilesPtr>)>;
  virtual void CreatePayloadFiles(
      int64_t payload_id,
      const proto::CameraRollItemMetadata& item_metadata,
      CreatePayloadFilesCallback payload_files_callback) = 0;

  // Updates the download progress for the file transfer associated with the
  // |update| in the Holding Space tray. The backfile file will be deleted if
  // the transfer was canceled or has failed.
  virtual void UpdateDownloadProgress(
      secure_channel::mojom::FileTransferUpdatePtr update) = 0;

  // Deletes the file created for the given |payload_id|.
  virtual void DeleteFile(int64_t payload_id) = 0;

 protected:
  CameraRollDownloadManager() = default;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_CAMERA_ROLL_DOWNLOAD_MANAGER_H_
