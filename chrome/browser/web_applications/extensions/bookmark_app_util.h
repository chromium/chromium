// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_UTIL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_UTIL_H_

#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "extensions/common/constants.h"

namespace content {
class BrowserContext;
}

class GURL;

namespace extensions {

class Extension;
class ExtensionPrefs;

// Gets whether the bookmark app is locally installed. Defaults to true if the
// extension pref that stores this isn't set.
// Note this can be called for hosted apps which should use the default.
bool BookmarkAppIsLocallyInstalled(content::BrowserContext* context,
                                   const Extension* extension);
bool BookmarkAppIsLocallyInstalled(const ExtensionPrefs* prefs,
                                   const Extension* extension);

// Generates a scope based on |launch_url| and checks if the |url| falls under
// it. https://www.w3.org/TR/appmanifest/#navigation-scope
bool IsInNavigationScopeForLaunchUrl(const GURL& launch_url, const GURL& url);

struct LaunchContainerAndType {
  apps::LaunchContainer launch_container;
  extensions::LaunchType launch_type;
};

LaunchContainerAndType GetLaunchContainerAndTypeFromDisplayMode(
    web_app::DisplayMode display_mode);

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_UTIL_H_
