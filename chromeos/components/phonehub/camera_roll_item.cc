// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/camera_roll_item.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"

namespace chromeos {
namespace phonehub {

CameraRollItem::CameraRollItem(const proto::CameraRollItemMetadata& metadata)
    : metadata_(metadata) {}

CameraRollItem::~CameraRollItem() = default;

}  // namespace phonehub
}  // namespace chromeos
