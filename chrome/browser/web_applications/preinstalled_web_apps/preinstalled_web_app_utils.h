// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APPS_PREINSTALLED_WEB_APP_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APPS_PREINSTALLED_WEB_APP_UTILS_H_

#include "chrome/browser/web_applications/components/web_application_info.h"

namespace web_app {

std::map<SquareSizePx, SkBitmap> LoadBundledIcons(
    const std::initializer_list<int>& icon_resource_ids);

struct Translation {
  const char* bcp47_locale;
  const char* utf8_translation;
};

const char* GetTranslatedName(const char* utf8_default_name,
                              base::span<const Translation> translations);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APPS_PREINSTALLED_WEB_APP_UTILS_H_
