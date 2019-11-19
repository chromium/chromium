// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/web_app_info_image_source.h"

#include "ui/gfx/image/image_skia.h"

WebAppInfoImageSource::WebAppInfoImageSource(
    int dip_size,
    const std::vector<WebApplicationIconInfo>& icons)
    : dip_size_(dip_size), icons_(icons) {}

WebAppInfoImageSource::~WebAppInfoImageSource() {}

gfx::ImageSkiaRep WebAppInfoImageSource::GetImageForScale(float scale) {
  int size = base::saturated_cast<int>(dip_size_ * scale);
  for (const auto& icon_info : icons_) {
    if (icon_info.width == size)
      return gfx::ImageSkiaRep(icon_info.data, scale);
  }
  return gfx::ImageSkiaRep();
}
