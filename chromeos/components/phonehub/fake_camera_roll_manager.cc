// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/fake_camera_roll_manager.h"

#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"

namespace chromeos {
namespace phonehub {

FakeCameraRollManager::FakeCameraRollManager() = default;

FakeCameraRollManager::~FakeCameraRollManager() = default;

void FakeCameraRollManager::DownloadItem(
    const proto::CameraRollItemMetadata& item_metadata) {}

}  // namespace phonehub
}  // namespace chromeos
