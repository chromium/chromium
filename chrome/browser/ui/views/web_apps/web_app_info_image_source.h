// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INFO_IMAGE_SOURCE_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INFO_IMAGE_SOURCE_H_

#include <map>

#include "chrome/browser/web_applications/components/web_application_info.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia_source.h"

// An image source which draws from a WebApplicationInfo icons list.
class WebAppInfoImageSource : public gfx::ImageSkiaSource {
 public:
  WebAppInfoImageSource(int dip_size,
                        const std::map<SquareSizePx, SkBitmap>& icons);
  ~WebAppInfoImageSource() override;

 private:
  // gfx::ImageSkiaSource:
  gfx::ImageSkiaRep GetImageForScale(float scale) override;

  int dip_size_;
  std::map<SquareSizePx, SkBitmap> icons_;

  DISALLOW_COPY_AND_ASSIGN(WebAppInfoImageSource);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INFO_IMAGE_SOURCE_H_
