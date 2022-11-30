// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/test/thumbnail.h"

#include <memory>

#include "components/history/core/test/thumbnail-inl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image.h"

namespace history {

gfx::Image CreateGoogleThumbnailForTest() {
  // Returned image takes ownership of decoded SkBitmap.
  std::unique_ptr<SkBitmap> thumbnail_bitmap(
      gfx::JPEGCodec::Decode(kGoogleThumbnail, sizeof(kGoogleThumbnail)));
  return gfx::Image::CreateFrom1xBitmap(*thumbnail_bitmap);
}

}  // namespace
