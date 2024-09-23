// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/util/image_util.h"

#include <string_view>

#include "base/base64.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"
#include "ui/gfx/image/buffer_w_stream.h"
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
  gfx::BufferWStream stream;
  const bool encoding_succeeded =
      SkPngEncoder::Encode(&stream, bitmap.pixmap(), {});
  DCHECK(encoding_succeeded);
  return MakeDataURIForImage(
      base::as_bytes(base::make_span(stream.TakeBuffer())), "png");
}

}  // namespace webui
