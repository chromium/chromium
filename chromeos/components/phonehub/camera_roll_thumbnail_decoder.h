// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_CAMERA_ROLL_THUMBNAIL_DECODER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_CAMERA_ROLL_THUMBNAIL_DECODER_H_

#include "base/callback.h"
#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"

namespace chromeos {
namespace phonehub {

class CameraRollItem;

// Decodes camera roll item thumbnails received from
// |FetchCameraRollItemsResponse| in batches, and converts decoded items into
// |CameraRollItem| objects that can be displayed on the UI.
class CameraRollThumbnailDecoder {
 public:
  // Result of a |BatchDecode| operation.
  enum class BatchDecodeResult {
    // All items in the batch have been successfully decoded.
    kSuccess = 0,
    // Failed to decode at least one item in the batch.
    kError = 1,
    // The decode requests for this batch has been cancelled.
    kCancelled = 2,
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
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_CAMERA_ROLL_THUMBNAIL_DECODER_H_
