// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/util/image_util.h"

#include <string_view>

#include "base/base64.h"
#include "base/containers/span.h"
#include "skia/ext/codec_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace webui {

std::string MakeDataURIForImage(base::span<const uint8_t> image_data,
                                std::string_view mime_subtype) {
  std::string result = "data:image/";
  result.append(mime_subtype.begin(), mime_subtype.end());
  result += ";base64,";
  result += base::Base64Encode(image_data);
  return result;
}

std::string EncodePNGAndMakeDataURI(gfx::ImageSkia image, float scale_factor) {
  const SkBitmap& bitmap = image.GetRepresentation(scale_factor).GetBitmap();
  return skia::EncodePngAsDataUri(bitmap.pixmap());
}

}  // namespace webui
