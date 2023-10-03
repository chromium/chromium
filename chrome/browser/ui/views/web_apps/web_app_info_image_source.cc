// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_info_image_source.h"

#include "ui/gfx/image/image_skia_rep.h"

WebAppInfoImageSource::WebAppInfoImageSource(
    int dip_size,
    std::map<web_app::SquareSizePx, SkBitmap> icons)
    : dip_size_(dip_size), icons_(std::move(icons)) {}

WebAppInfoImageSource::~WebAppInfoImageSource() {}

gfx::ImageSkiaRep WebAppInfoImageSource::GetImageForScale(float scale) {
  int size = base::saturated_cast<int>(dip_size_ * scale);
  auto icon = icons_.find(size);
  if (icon != icons_.end())
    return gfx::ImageSkiaRep(icon->second, scale);
  return gfx::ImageSkiaRep();
}
