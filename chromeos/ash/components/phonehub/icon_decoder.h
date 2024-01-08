// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_ICON_DECODER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_ICON_DECODER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace phonehub {

// Decodes icons in batches.
// TODO(b/233279034): There are three decoders now: `CameraRollThumbnailDecoder`
// `IconDecoder`, and `NotificationProcessor`. It may makes sense
// to abstract out the redundant logic in a single place.
class IconDecoder {
 public:
  // Each decoding operation is associated with a unique id that later is used
  // to identify the result in the batch output.
  struct DecodingData {
    DecodingData(unsigned long id, const std::string& input_data);

    const unsigned long id;
    const raw_ref<const std::string> input_data;
    gfx::Image result;
  };

  IconDecoder(const IconDecoder&) = delete;
  IconDecoder& operator=(const IconDecoder&) = delete;
  virtual ~IconDecoder() = default;

  // Decodes the `input_data` in each item in `decide_items` and places the
  // result inside the `result` field in the `decode_items`. At the end,
  // `finished_callback` is called and the list of `DecodingData` items is
  // returned back to the callee.
  // Important note: If this method is called "BEFORE" the result of the
  // previous call is ready, the previous call is cancelled without
  // calling `finished_callback`.
  virtual void BatchDecode(
      std::unique_ptr<std::vector<DecodingData>> decode_items,
      base::OnceCallback<void(std::unique_ptr<std::vector<DecodingData>>)>
          finished_callback) = 0;

 protected:
  IconDecoder() = default;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_ICON_DECODER_H_
