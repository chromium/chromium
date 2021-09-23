// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_FAKE_CAMERA_ROLL_MANAGER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_FAKE_CAMERA_ROLL_MANAGER_H_

#include "chromeos/components/phonehub/camera_roll_manager.h"

namespace chromeos {
namespace phonehub {

class FakeCameraRollManager : public CameraRollManager {
 public:
  FakeCameraRollManager();
  ~FakeCameraRollManager() override;

  using CameraRollManager::SetCurrentItems;

  using CameraRollManager::ClearCurrentItems;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_FAKE_CAMERA_ROLL_MANAGER_H_
