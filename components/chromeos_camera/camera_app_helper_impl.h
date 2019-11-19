// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CHROMEOS_CAMERA_CAMERA_APP_HELPER_IMPL_H_
#define COMPONENTS_CHROMEOS_CAMERA_CAMERA_APP_HELPER_IMPL_H_

#include <vector>

#include "components/chromeos_camera/common/camera_app_helper.mojom.h"

namespace chromeos_camera {

class CameraAppHelperImpl : public chromeos_camera::mojom::CameraAppHelper {
 public:
  using CameraResultCallback =
      base::RepeatingCallback<void(uint32_t,
                                   arc::mojom::CameraIntentAction,
                                   const std::vector<uint8_t>&,
                                   HandleCameraResultCallback)>;

  explicit CameraAppHelperImpl(CameraResultCallback camera_result_callback);
  ~CameraAppHelperImpl() override;

  // chromeos_camera::mojom::CameraAppHelper implementations.
  void HandleCameraResult(uint32_t intent_id,
                          arc::mojom::CameraIntentAction action,
                          const std::vector<uint8_t>& data,
                          HandleCameraResultCallback callback) override;
  void IsTabletMode(IsTabletModeCallback callback) override;

 private:
  CameraResultCallback camera_result_callback_;

  DISALLOW_COPY_AND_ASSIGN(CameraAppHelperImpl);
};

}  // namespace chromeos_camera

#endif  // COMPONENTS_CHROMEOS_CAMERA_CAMERA_APP_HELPER_IMPL_H_
