// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_ICON_TEST_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_ICON_TEST_UTILS_H_

#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/common/web_application_info.h"
#include "third_party/skia/include/core/SkColor.h"

class GURL;
class SkBitmap;

namespace web_app {

SkBitmap CreateSquareIcon(int size_px, SkColor solid_color);

WebApplicationIconInfo GenerateIconInfo(const GURL& url,
                                        int size_px,
                                        SkColor solid_color);

void AddIconToIconsMap(const GURL& icon_url,
                       int size_px,
                       SkColor solid_color,
                       IconsMap* icons_map);

bool AreColorsEqual(SkColor expected_color,
                    SkColor actual_color,
                    int threshold);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_ICON_TEST_UTILS_H_
