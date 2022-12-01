// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/camera_roll_item.h"

#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace phonehub {

CameraRollItem::CameraRollItem(const proto::CameraRollItemMetadata& metadata,
                               const gfx::Image& thumbnail)
    : metadata_(metadata), thumbnail_(thumbnail) {}

CameraRollItem::CameraRollItem(const CameraRollItem&) = default;

CameraRollItem& CameraRollItem::operator=(const CameraRollItem&) = default;

CameraRollItem::~CameraRollItem() = default;

bool CameraRollItem::operator==(const CameraRollItem& other) const {
  return metadata_.key() == other.metadata().key() &&
         metadata_.mime_type() == other.metadata().mime_type() &&
         metadata_.last_modified_millis() ==
             other.metadata().last_modified_millis() &&
         metadata_.file_size_bytes() == other.metadata().file_size_bytes() &&
         metadata_.file_name() == other.metadata().file_name();
}

bool CameraRollItem::operator!=(const CameraRollItem& other) const {
  return !operator==(other);
}

}  // namespace phonehub
}  // namespace ash
