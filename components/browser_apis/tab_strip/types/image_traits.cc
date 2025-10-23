// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_strip/types/image_traits.h"

#include "base/base64.h"
#include "base/containers/span.h"
#include "net/base/data_url.h"
#include "skia/ext/codec_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"

GURL mojo::StructTraits<MojoImageView, NativeImage>::data_url(
    const NativeImage& native) {
  if (native.isNull()) {
    return GURL();
  }
  const SkBitmap* bitmap = native.bitmap();
  if (!bitmap || bitmap->isNull()) {
    return GURL();
  }
  return GURL(skia::EncodePngAsDataUri(bitmap->pixmap()));
}

bool mojo::StructTraits<MojoImageView, NativeImage>::Read(MojoImageView view,
                                                          NativeImage* out) {
  GURL data_url;
  if (!view.ReadDataUrl(&data_url)) {
    return false;
  }

  if (data_url.is_empty()) {
    *out = gfx::ImageSkia();
    return true;
  }

  if (!data_url.is_valid()) {
    return false;
  }

  std::string mime_type;
  std::string charset;
  std::string image_data;
  if (!net::DataURL::Parse(data_url, &mime_type, &charset, &image_data)) {
    return false;
  }

  SkBitmap bitmap =
      gfx::PNGCodec::Decode(base::as_bytes(base::span(image_data)));
  if (bitmap.isNull()) {
    return false;
  }

  *out = gfx::ImageSkia::CreateFromBitmap(bitmap, 1.0f);
  return true;
}
