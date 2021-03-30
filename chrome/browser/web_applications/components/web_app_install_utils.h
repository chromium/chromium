// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_INSTALL_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_INSTALL_UTILS_H_

#include <map>
#include <set>
#include <vector>

#include "url/gurl.h"

struct WebApplicationInfo;
class SkBitmap;

namespace blink {
struct Manifest;
}

namespace content {
class WebContents;
}

namespace webapps {
enum class WebappInstallSource;
}

namespace web_app {

enum class ExternalInstallSource;
enum class InstallResultCode;

enum class ForInstallableSite {
  kYes,
  kNo,
  kUnknown,
};

// Update the given WebApplicationInfo with information from the manifest.
// Will sanitise the manifest fields to be suitable for installation to prevent
// sites from using arbitrarily large amounts of disk space.
void UpdateWebAppInfoFromManifest(const blink::Manifest& manifest,
                                  const GURL& manifest_url,
                                  WebApplicationInfo* web_app_info);

// Form a list of icons to download: Remove icons with invalid urls.
std::vector<GURL> GetValidIconUrlsToDownload(
    const WebApplicationInfo& web_app_info);

// A map of icon urls to the bitmaps provided by that url.
using IconsMap = std::map<GURL, std::vector<SkBitmap>>;

// Populate shortcut item icon maps in WebApplicationInfo using the IconsMap.
void PopulateShortcutItemIcons(WebApplicationInfo* web_app_info,
                               const IconsMap* icons_map);

// Filter to only square icons, ensure that the necessary-sized icons are
// available by resizing larger icons down to smaller sizes, and generating
// icons for sizes where resizing is not possible. |icons_map| is optional.
void FilterAndResizeIconsGenerateMissing(WebApplicationInfo* web_app_info,
                                         const IconsMap* icons_map);

// Record an app banner added to homescreen event to ensure banners are not
// shown for this app.
void RecordAppBanner(content::WebContents* contents, const GURL& app_url);

webapps::WebappInstallSource ConvertExternalInstallSourceToInstallSource(
    ExternalInstallSource external_install_source);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_INSTALL_UTILS_H_
