// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_INSTALL_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_INSTALL_UTILS_H_

#include <map>
#include <set>
#include <vector>

#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace webapps {
enum class WebappInstallSource;
enum class WebappUninstallSource;
}

namespace web_app {

enum class ForInstallableSite {
  kYes,
  kNo,
  kUnknown,
};

// Converts from the manifest type to the Chrome type.
apps::FileHandlers CreateFileHandlersFromManifest(
    const std::vector<blink::mojom::ManifestFileHandlerPtr>&
        manifest_file_handlers,
    const GURL& app_scope);

// Update the given WebApplicationInfo with information from the manifest.
// Will sanitise the manifest fields to be suitable for installation to prevent
// sites from using arbitrarily large amounts of disk space.
void UpdateWebAppInfoFromManifest(const blink::mojom::Manifest& manifest,
                                  const GURL& manifest_url,
                                  WebApplicationInfo* web_app_info);

// Form a list of icons to download: Remove icons with invalid urls.
std::vector<GURL> GetValidIconUrlsToDownload(
    const WebApplicationInfo& web_app_info);

// Populate shortcut item icon maps in WebApplicationInfo using the IconsMap.
// This ignores icons that might have already existed in `web_app_info`.
// TODO(estade): also save bitmaps in `icons_map` that are relevant to file
// handling in `web_app_info->other_icon_bitmaps`.
void PopulateShortcutItemIcons(WebApplicationInfo* web_app_info,
                               const IconsMap& icons_map);

// Populates main product icons into `web_app_info`. This method filters icons
// from `icons_map` to only square icons and ensures that the necessary-sized
// icons are available by resizing larger icons down to smaller sizes. When
// `icons_map` is null or missing icons, it will generate icons for sizes where
// resizing is not possible. Icons which were already populated in
// `web_app_info` may be retained, and even used to generate missing icons.
void PopulateProductIcons(WebApplicationInfo* web_app_info,
                          const IconsMap* icons_map);

// Record an app banner added to homescreen event to ensure banners are not
// shown for this app.
void RecordAppBanner(content::WebContents* contents, const GURL& app_url);

webapps::WebappInstallSource ConvertExternalInstallSourceToInstallSource(
    ExternalInstallSource external_install_source);

webapps::WebappUninstallSource ConvertExternalInstallSourceToUninstallSource(
    ExternalInstallSource external_install_source);

Source::Type InferSourceFromMetricsInstallSource(
    webapps::WebappInstallSource install_source);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_INSTALL_UTILS_H_
