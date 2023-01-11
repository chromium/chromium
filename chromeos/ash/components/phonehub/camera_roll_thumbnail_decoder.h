// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_CAMERA_ROLL_THUMBNAIL_DECODER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_CAMERA_ROLL_THUMBNAIL_DECODER_H_

#include "base/functional/callback.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"

namespace ash {
namespace phonehub {

class CameraRollItem;

// Decodes camera roll item thumbnails received from
// |FetchCameraRollItemsResponse| in batches, and converts decoded items into
// |CameraRollItem| objects that can be displayed on the UI.
class CameraRollThumbnailDecoder {
 public:
  // Result of a |BatchDecode| operation.
  enum class BatchDecodeResult {
    // All items in the batch have been completed.
    kCompleted = 0,
    // The decode requests for this batch has been cancelled.
    kCancelled = 1,
    kMaxValue = kCancelled
  };

  CameraRollThumbnailDecoder(const CameraRollThumbnailDecoder&) = delete;
  CameraRollThumbnailDecoder& operator=(const CameraRollThumbnailDecoder&) =
      delete;
  virtual ~CameraRollThumbnailDecoder() = default;

  // Loads thumbnails of the batch of camera roll items either using encoded
  // thumbnail bytes in the |FetchCameraRollItemsResponse|, or from existing
  // images in |current_items| if an item has not changed.
  //
  // Returns decoded items through |callback| if the full batch can be decoded;
  // otherwise an empty list will be returned with appropriate result code.
  virtual void BatchDecode(
      const proto::FetchCameraRollItemsResponse& response,
      const std::vector<CameraRollItem>& current_items,
      base::OnceCallback<void(BatchDecodeResult result,
                              const std::vector<CameraRollItem>&)>
          callback) = 0;

 protected:
  CameraRollThumbnailDecoder() = default;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_CAMERA_ROLL_THUMBNAIL_DECODER_H_
