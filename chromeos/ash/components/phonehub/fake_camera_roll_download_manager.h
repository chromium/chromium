// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_CAMERA_ROLL_DOWNLOAD_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_CAMERA_ROLL_DOWNLOAD_MANAGER_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "chromeos/ash/components/phonehub/camera_roll_download_manager.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"

namespace ash {
namespace phonehub {

class FakeCameraRollDownloadManager : public CameraRollDownloadManager {
 public:
  FakeCameraRollDownloadManager();
  ~FakeCameraRollDownloadManager() override;

  // CameraRollDownloadManager:
  void CreatePayloadFiles(
      int64_t payload_id,
      const phonehub::proto::CameraRollItemMetadata& item_metadata,
      CreatePayloadFilesCallback payload_files_callback) override;
  void UpdateDownloadProgress(
      secure_channel::mojom::FileTransferUpdatePtr update) override;
  void DeleteFile(int64_t payload_id) override;

  void set_expected_create_payload_files_result(
      CreatePayloadFilesResult expected_create_payload_files_result) {
    expected_create_payload_files_result_ =
        expected_create_payload_files_result;
  }

  const std::vector<secure_channel::mojom::FileTransferUpdatePtr>&
  GetFileTransferUpdates(int64_t payload_id) const;

 private:
  CreatePayloadFilesResult expected_create_payload_files_result_ =
      CreatePayloadFilesResult::kSuccess;

  // A map from payload IDs to the list of FileTransferUpdate received for each
  // payload.
  base::flat_map<int64_t,
                 std::vector<secure_channel::mojom::FileTransferUpdatePtr>>
      payload_update_map_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_CAMERA_ROLL_DOWNLOAD_MANAGER_H_
