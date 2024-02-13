// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_BANNERS_NATIVE_APP_BANNER_DATA_H_
#define COMPONENTS_WEBAPPS_BROWSER_BANNERS_NATIVE_APP_BANNER_DATA_H_

#include <string>

#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace webapps {

// Holds information about a native app being considered for an installation
// banner / onbeforeinstallprompt event.
struct NativeAppBannerData {
  NativeAppBannerData() = delete;
  NativeAppBannerData(std::string app_package,
                      std::u16string app_title,
                      GURL primary_icon_url,
                      SkBitmap primary_icon);
  NativeAppBannerData(const NativeAppBannerData& other);
  ~NativeAppBannerData();

  const std::string app_package;
  const std::u16string app_title;

  // The URL of the primary icon.
  const GURL primary_icon_url;

  // The primary icon object.
  const SkBitmap primary_icon;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_BANNERS_NATIVE_APP_BANNER_DATA_H_
