// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_CAMERA_ROLL_ITEM_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_CAMERA_ROLL_ITEM_H_

#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace phonehub {

// Data related to a photo or video taken on an Android device, including
// metadata and thumbnail.
class CameraRollItem {
 public:
  CameraRollItem(const proto::CameraRollItemMetadata& metadata,
                 const gfx::Image& thumbnail);
  CameraRollItem(const CameraRollItem&);
  CameraRollItem& operator=(const CameraRollItem&);
  virtual ~CameraRollItem();

  // True iff both both items have the same metadata values.
  bool operator==(const CameraRollItem& other) const;
  bool operator!=(const CameraRollItem& other) const;

  // Returns the metadata of this item.
  const proto::CameraRollItemMetadata& metadata() const { return metadata_; }
  // Returns the decoded thumbnail of this item.
  const gfx::Image& thumbnail() const { return thumbnail_; }

 private:
  proto::CameraRollItemMetadata metadata_;
  gfx::Image thumbnail_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_CAMERA_ROLL_ITEM_H_
