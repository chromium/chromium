// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"

#include <map>
#include <memory>
#include <utility>

#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "url/gurl.h"

namespace extensions {
namespace {

// A preference used to indicate that a bookmark apps is fully locally installed
// on this machine. The default value (i.e. if the pref is not set) is to be
// fully locally installed, so that hosted apps or bookmark apps created /
// synced before this pref existed will be treated as locally installed.
const char kPrefLocallyInstalled[] = "locallyInstalled";

}  // namespace

bool BookmarkAppIsLocallyInstalled(content::BrowserContext* context,
                                   const Extension* extension) {
  return BookmarkAppIsLocallyInstalled(ExtensionPrefs::Get(context), extension);
}

bool BookmarkAppIsLocallyInstalled(const ExtensionPrefs* prefs,
                                   const Extension* extension) {
  bool locally_installed;
  if (prefs->ReadPrefAsBoolean(extension->id(), kPrefLocallyInstalled,
                               &locally_installed)) {
    return locally_installed;
  }

  return true;
}

bool IsInNavigationScopeForLaunchUrl(const GURL& launch_url, const GURL& url) {
  // Drop any "suffix" components after the path (Resolve "."):
  const GURL nav_scope = launch_url.GetWithoutFilename();

  const int scope_str_length = nav_scope.spec().size();
  return base::StringPiece(nav_scope.spec()) ==
         base::StringPiece(url.spec()).substr(0, scope_str_length);
}

LaunchContainerAndType GetLaunchContainerAndTypeFromDisplayMode(
    web_app::DisplayMode display_mode) {
  apps::LaunchContainer apps_launch_container =
      web_app::ConvertDisplayModeToAppLaunchContainer(display_mode);
  switch (apps_launch_container) {
    case apps::LaunchContainer::kLaunchContainerNone:
      return {apps::LaunchContainer::kLaunchContainerNone,
              extensions::LaunchType::LAUNCH_TYPE_DEFAULT};
    case apps::LaunchContainer::kLaunchContainerPanelDeprecated:
      return {apps::LaunchContainer::kLaunchContainerPanelDeprecated,
              extensions::LaunchType::LAUNCH_TYPE_REGULAR};
    case apps::LaunchContainer::kLaunchContainerTab:
      return {apps::LaunchContainer::kLaunchContainerTab,
              extensions::LaunchType::LAUNCH_TYPE_REGULAR};
    case apps::LaunchContainer::kLaunchContainerWindow:
      return {apps::LaunchContainer::kLaunchContainerTab,
              display_mode == web_app::DisplayMode::kFullscreen
                  ? extensions::LaunchType::LAUNCH_TYPE_FULLSCREEN
                  : extensions::LaunchType::LAUNCH_TYPE_WINDOW};
  }
  NOTREACHED();
}

}  // namespace extensions
