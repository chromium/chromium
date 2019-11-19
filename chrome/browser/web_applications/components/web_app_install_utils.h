// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_INSTALL_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_INSTALL_UTILS_H_

#include <map>
#include <set>
#include <vector>

#include "url/gurl.h"

enum class WebappInstallSource;
struct WebApplicationInfo;
class SkBitmap;

namespace blink {
struct Manifest;
}

namespace content {
class WebContents;
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
void UpdateWebAppInfoFromManifest(const blink::Manifest& manifest,
                                  WebApplicationInfo* web_app_info,
                                  ForInstallableSite installable_site);

// Form a list of icons to download: Remove icons with invalid urls.
std::vector<GURL> GetValidIconUrlsToDownload(
    const WebApplicationInfo& web_app_info);

// A map of icon urls to the bitmaps provided by that url.
using IconsMap = std::map<GURL, std::vector<SkBitmap>>;

// Filter out square icons, ensure that the necessary-sized icons are available
// by resizing larger icons down to smaller sizes, and generating icons for
// sizes where resizing is not possible. |icons_map| is optional.
//
// Historically, |is_for_sync| is a hack for the old |ExtensionSyncService|
// system to avoid sync wars. It is important that the linked app information in
// any web app that gets created from sync matches the linked app information
// that came from sync. If there are any changes, they will be synced back to
// other devices and could potentially create a never ending sync cycle. If
// |is_for_sync| is true then icon links won't be changed.
void FilterAndResizeIconsGenerateMissing(WebApplicationInfo* web_app_info,
                                         const IconsMap* icons_map,
                                         bool is_for_sync);

// Record an app banner added to homescreen event to ensure banners are not
// shown for this app.
void RecordAppBanner(content::WebContents* contents, const GURL& app_url);

WebappInstallSource ConvertExternalInstallSourceToInstallSource(
    ExternalInstallSource external_install_source);

void RecordExternalAppInstallResultCode(
    const char* histogram_name,
    std::map<GURL, InstallResultCode> install_results);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_INSTALL_UTILS_H_
