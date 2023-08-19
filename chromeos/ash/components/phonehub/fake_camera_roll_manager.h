// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_CAMERA_ROLL_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_CAMERA_ROLL_MANAGER_H_

#include "chromeos/ash/components/phonehub/camera_roll_manager.h"

#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"

namespace ash::phonehub {

class FakeCameraRollManager : public CameraRollManager {
 public:
  FakeCameraRollManager();
  ~FakeCameraRollManager() override;

  void SetIsCameraRollAvailableToBeEnabled(bool can_enable);
  void SetIsCameraRollAccessible(bool accessiable);
  void SetIsAndroidStorageGranted(bool granted);
  void SetSimulatedDownloadError(bool has_error);
  void SetSimulatedErrorType(Observer::DownloadErrorType error_type);
  int GetDownloadRequestCount();

  bool is_camera_roll_enabled() const { return !is_avaiable_to_be_enabled_; }

  using CameraRollManager::SetCurrentItems;

  using CameraRollManager::ClearCurrentItems;

 private:
  void ComputeAndUpdateUiState() override;
  // CameraRollManager:
  void DownloadItem(
      const proto::CameraRollItemMetadata& item_metadata) override;
  int download_request_count_ = 0;
  bool is_avaiable_to_be_enabled_ = true;
  bool is_camera_roll_accessible_ = true;
  bool is_android_storage_granted_ = true;
  bool is_simulating_error_ = false;
  Observer::DownloadErrorType simulated_error_type_ =
      Observer::DownloadErrorType::kGenericError;
};

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_CAMERA_ROLL_MANAGER_H_
