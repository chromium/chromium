// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ICONS_TRUSTED_ICON_FILTER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ICONS_TRUSTED_ICON_FILTER_H_

#include <optional>
#include <vector>

#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/services/app_service/public/cpp/icon_info.h"

namespace web_app {

// Construct the primary icon to be used as provided in the manifest.
// On MacOS and ChromeOS, a maskable icon of size 256 or more is preferred,
// while on other platforms, the icon of largest size is preferred. If no icons
// are found, and only SVG icons of no size are available in the manifest, that
// is the last fallback option, of size 512.
std::optional<apps::IconInfo> GetTrustedIconsFromManifest(
    const std::vector<blink::Manifest::ImageResource>& icons);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ICONS_TRUSTED_ICON_FILTER_H_
