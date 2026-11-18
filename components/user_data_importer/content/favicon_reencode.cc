// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/content/favicon_reencode.h"

#include <utility>

#include "content/public/child/image_decoder_utils.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/size.h"

namespace importer {

std::optional<std::vector<uint8_t>> ReencodeFavicon(
    base::span<const uint8_t> src) {
  // Decode the favicon using WebKit's image decoder.
  SkBitmap decoded = content::DecodeImage(
      src, gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize));
  if (decoded.empty()) {
    return std::nullopt;  // Unable to decode.
  }

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
  return gfx::PNGCodec::EncodeBGRASkBitmap(decoded,
                                           /*discard_transparency=*/false);
}

base::flat_map<uint32_t, std::vector<uint8_t>> ReencodeFavicons(
    const base::flat_map<uint32_t, std::vector<uint8_t>>& raw_favicons) {
  std::vector<std::pair<uint32_t, std::vector<uint8_t>>> results;
  results.reserve(raw_favicons.size());
  for (const auto& [id, raw_data] : raw_favicons) {
    if (std::optional<std::vector<uint8_t>> png_data =
            ReencodeFavicon(raw_data)) {
      results.emplace_back(id, std::move(*png_data));
    }
  }
  return base::flat_map<uint32_t, std::vector<uint8_t>>(base::sorted_unique,
                                                        std::move(results));
}

}  // namespace importer
