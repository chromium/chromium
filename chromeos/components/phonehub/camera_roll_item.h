// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_CAMERA_ROLL_ITEM_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_CAMERA_ROLL_ITEM_H_

#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"

namespace chromeos {
namespace phonehub {

// Data related to a photo or video taken on an Android device, including
// metadata and thumbnail.
class CameraRollItem {
 public:
  CameraRollItem(const proto::CameraRollItemMetadata& metadata);
  CameraRollItem(const CameraRollItem&) = delete;
  CameraRollItem& operator=(const CameraRollItem&) = delete;
  virtual ~CameraRollItem();

  // Returns the metadata of this item.
  const proto::CameraRollItemMetadata& metadata() const { return metadata_; }

 private:
  proto::CameraRollItemMetadata metadata_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_CAMERA_ROLL_ITEM_H_
