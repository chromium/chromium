// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_ICON_TEST_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_ICON_TEST_UTILS_H_

#include <map>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

class GURL;
class Profile;

namespace web_app {

class FileUtilsWrapper;

SkBitmap CreateSquareIcon(int size_px, SkColor solid_color);

void AddGeneratedIcon(std::map<SquareSizePx, SkBitmap>* icon_bitmaps,
                      int size_px,
                      SkColor solid_color);

void AddIconToIconsMap(const GURL& icon_url,
                       int size_px,
                       SkColor solid_color,
                       IconsMap* icons_map);

bool AreColorsEqual(SkColor expected_color,
                    SkColor actual_color,
                    int threshold);

base::FilePath GetAppIconsAnyDir(Profile* profile, const AppId& app_id);

base::FilePath GetAppIconsMaskableDir(Profile* profile, const AppId& app_id);

bool ReadBitmap(FileUtilsWrapper* utils,
                const base::FilePath& file_path,
                SkBitmap* bitmap);

base::span<const int> GetIconSizes();

bool ContainsOneIconOfEachSize(
    const std::map<SquareSizePx, SkBitmap>& icon_bitmaps);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_ICON_TEST_UTILS_H_
