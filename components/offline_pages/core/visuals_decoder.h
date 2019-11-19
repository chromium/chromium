// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_VISUALS_DECODER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_VISUALS_DECODER_H_

#include <string>
#include "base/callback.h"

namespace gfx {
class Image;
}

namespace offline_pages {

// Decodes thumbnails. Provided so that this component does not need to
// depend on ImageSkiaOperations which isn't available everywhere.
class VisualsDecoder {
 public:
  using DecodeComplete = base::OnceCallback<void(const gfx::Image&)>;
  virtual ~VisualsDecoder() {}

  // Decode a thumbnail or favicon image and crop it square. Calls
  // complete_callback when decoding completes successfully or otherwise. If
  // decoding fails, the returned image is empty. In the case of .ICO favicons,
  // chooses the frame whose size matches our preferred favicon size or the next
  // one larger if there is no exact match.
  virtual void DecodeAndCropImage(const std::string& thumbnail_data,
                                  DecodeComplete complete_callback) = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_VISUALS_DECODER_H_
