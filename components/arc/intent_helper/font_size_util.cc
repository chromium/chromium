// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/font_size_util.h"

#include <algorithm>

namespace arc {

double ConvertFontSizeChromeToAndroid(int default_size,
                                      int default_fixed_size,
                                      int minimum_size) {
  // kWebKitDefaultFixedFontSize is automatically set to be 3 pixels smaller
  // than kWebKitDefaultFontSize when Chrome's settings page's main font
  // dropdown control is adjusted.  If the user specifically sets a higher
  // fixed font size we will want to take into account the adjustment.
  default_fixed_size += 3;
  int max_chrome_size =
      std::max({default_fixed_size, default_size, minimum_size});

  double android_scale = kAndroidFontScaleSmall;
  if (max_chrome_size >= kChromeFontSizeVeryLarge) {
    android_scale = kAndroidFontScaleHuge;
  } else if (max_chrome_size >= kChromeFontSizeLarge) {
    android_scale = kAndroidFontScaleLarge;
  } else if (max_chrome_size >= kChromeFontSizeNormal) {
    android_scale = kAndroidFontScaleNormal;
  }

  return android_scale;
}

}  // namespace arc
