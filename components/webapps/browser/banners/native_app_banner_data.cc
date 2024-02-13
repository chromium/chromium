// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/banners/native_app_banner_data.h"

#include <string>

#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace webapps {

NativeAppBannerData::NativeAppBannerData(std::string app_package,
                                         std::u16string app_title,
                                         GURL primary_icon_url,
                                         SkBitmap primary_icon)
    : app_package(std::move(app_package)),
      app_title(std::move(app_title)),
      primary_icon_url(std::move(primary_icon_url)),
      primary_icon(std::move(primary_icon)) {}
NativeAppBannerData::NativeAppBannerData(const NativeAppBannerData& other) =
    default;
NativeAppBannerData::~NativeAppBannerData() = default;

}  // namespace webapps
