// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_INFO_IMAGE_SOURCE_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_INFO_IMAGE_SOURCE_H_

#include "chrome/browser/web_applications/model/web_app_icon_types.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia_source.h"

// An image source which draws from a WebAppInstallInfo icons list.
class WebAppInfoImageSource : public gfx::ImageSkiaSource {
 public:
  WebAppInfoImageSource(int dip_size, web_app::UnorderedSizeToBitmap icons);

  WebAppInfoImageSource(const WebAppInfoImageSource&) = delete;
  WebAppInfoImageSource& operator=(const WebAppInfoImageSource&) = delete;

  ~WebAppInfoImageSource() override;

 private:
  // gfx::ImageSkiaSource:
  gfx::ImageSkiaRep GetImageForScale(float scale) override;

  int dip_size_;
  web_app::UnorderedSizeToBitmap icons_;
};

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_INFO_IMAGE_SOURCE_H_
