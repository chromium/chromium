// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/fake_camera_roll_download_manager.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "chromeos/ash/components/phonehub/camera_roll_download_manager.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"

namespace ash {
namespace phonehub {

FakeCameraRollDownloadManager::FakeCameraRollDownloadManager() = default;

FakeCameraRollDownloadManager::~FakeCameraRollDownloadManager() = default;

void FakeCameraRollDownloadManager::CreatePayloadFiles(
    int64_t payload_id,
    const phonehub::proto::CameraRollItemMetadata& item_metadata,
    CreatePayloadFilesCallback payload_files_callback) {
  std::optional<secure_channel::mojom::PayloadFilesPtr> payload_files;
  if (expected_create_payload_files_result_ ==
      CreatePayloadFilesResult::kSuccess) {
    payload_files =
        std::make_optional(secure_channel::mojom::PayloadFiles::New());
    payload_update_map_.emplace(
        payload_id,
        std::vector<secure_channel::mojom::FileTransferUpdatePtr>());
  } else {
    payload_files = std::nullopt;
  }
  std::move(payload_files_callback)
      .Run(expected_create_payload_files_result_, std::move(payload_files));
}

void FakeCameraRollDownloadManager::UpdateDownloadProgress(
    secure_channel::mojom::FileTransferUpdatePtr update) {
  payload_update_map_.at(update->payload_id).push_back(std::move(update));
}

void FakeCameraRollDownloadManager::DeleteFile(int64_t payload_id) {
  payload_update_map_.erase(payload_id);
}

const std::vector<secure_channel::mojom::FileTransferUpdatePtr>&
FakeCameraRollDownloadManager::GetFileTransferUpdates(
    int64_t payload_id) const {
  return payload_update_map_.at(payload_id);
}

}  // namespace phonehub
}  // namespace ash
