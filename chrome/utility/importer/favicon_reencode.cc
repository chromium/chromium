// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/favicon_reencode.h"

#include "content/public/child/image_decoder_utils.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/size.h"

namespace importer {

bool ReencodeFavicon(const unsigned char* src_data,
                     size_t src_len,
                     std::vector<unsigned char>* png_data) {
  // Decode the favicon using WebKit's image decoder.
  SkBitmap decoded = content::DecodeImage(
      src_data,
      gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize),
      src_len);
  if (decoded.empty())
    return false;  // Unable to decode.

  if (decoded.width() != gfx::kFaviconSize ||
      decoded.height() != gfx::kFaviconSize) {
    // The bitmap is not the correct size, re-sample.
    int new_width = decoded.width();
    int new_height = decoded.height();
    gfx::CalculateFaviconTargetSize(&new_width, &new_height);
    decoded = skia::ImageOperations::Resize(
        decoded, skia::ImageOperations::RESIZE_LANCZOS3, new_width, new_height);
  }

  // Encode our bitmap as a PNG.
  gfx::PNGCodec::EncodeBGRASkBitmap(decoded, false, png_data);
  return true;
}

}  // namespace importer
