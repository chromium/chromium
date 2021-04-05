// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"

#include <map>
#include <memory>
#include <utility>

#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
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
#include "extensions/common/manifest_handlers/web_app_shortcut_icons_handler.h"
#include "url/gurl.h"

namespace extensions {
namespace {

// A preference used to indicate that a bookmark apps is fully locally installed
// on this machine. The default value (i.e. if the pref is not set) is to be
// fully locally installed, so that hosted apps or bookmark apps created /
// synced before this pref existed will be treated as locally installed.
const char kPrefLocallyInstalled[] = "locallyInstalled";

}  // namespace

void SetBookmarkAppIsLocallyInstalled(content::BrowserContext* context,
                                      const Extension* extension,
                                      bool is_locally_installed) {
  ExtensionPrefs::Get(context)->UpdateExtensionPref(
      extension->id(), kPrefLocallyInstalled,
      std::make_unique<base::Value>(is_locally_installed));
}

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

int CountUserInstalledBookmarkApps(content::BrowserContext* browser_context) {
  // To avoid data races and inaccurate counting, ensure that ExtensionSystem is
  // always ready at this point.
  DCHECK(extensions::ExtensionSystem::Get(browser_context)->is_ready());

  int num_user_installed = 0;

  const ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context);
  for (scoped_refptr<const Extension> app :
       ExtensionRegistry::Get(browser_context)->enabled_extensions()) {
    if (!app->from_bookmark())
      continue;
    if (!BookmarkAppIsLocallyInstalled(prefs, app.get()))
      continue;
    if (!app->was_installed_by_default())
      ++num_user_installed;
  }

  return num_user_installed;
}

std::vector<SquareSizePx> GetBookmarkAppDownloadedIconSizes(
    const Extension* extension) {
  const ExtensionIconSet& icons = IconsInfo::GetIcons(extension);

  std::vector<SquareSizePx> icon_sizes_in_px;
  icon_sizes_in_px.reserve(icons.map().size());

  for (const ExtensionIconSet::IconMap::value_type& icon_info : icons.map())
    icon_sizes_in_px.push_back(icon_info.first);

  return icon_sizes_in_px;
}

std::vector<IconSizes> GetBookmarkAppDownloadedShortcutsMenuIconsSizes(
    const Extension* extension) {
  std::vector<IconSizes> shortcuts_menu_icons_sizes;

  const std::map<int, ExtensionIconSet>& shortcuts_menu_icons =
      WebAppShortcutIconsInfo::GetShortcutIcons(extension);
  shortcuts_menu_icons_sizes.reserve(shortcuts_menu_icons.size());
  for (const auto& shortcuts_menu_icon : shortcuts_menu_icons) {
    std::vector<SquareSizePx> shortcuts_menu_icon_sizes_any;
    shortcuts_menu_icon_sizes_any.reserve(
        shortcuts_menu_icon.second.map().size());
    for (const auto& icon_info : shortcuts_menu_icon.second.map()) {
      shortcuts_menu_icon_sizes_any.emplace_back(icon_info.first);
    }

    IconSizes icon_sizes;
    icon_sizes.SetSizesForPurpose(IconPurpose::ANY,
                                  std::move(shortcuts_menu_icon_sizes_any));
    shortcuts_menu_icons_sizes.push_back(std::move(icon_sizes));
  }

  return shortcuts_menu_icons_sizes;
}

LaunchContainerAndType GetLaunchContainerAndTypeFromDisplayMode(
    web_app::DisplayMode display_mode) {
  apps::mojom::LaunchContainer apps_launch_container =
      web_app::ConvertDisplayModeToAppLaunchContainer(display_mode);
  switch (apps_launch_container) {
    case apps::mojom::LaunchContainer::kLaunchContainerNone:
      return {extensions::LaunchContainer::kLaunchContainerNone,
              extensions::LaunchType::LAUNCH_TYPE_DEFAULT};
    case apps::mojom::LaunchContainer::kLaunchContainerPanelDeprecated:
      return {extensions::LaunchContainer::kLaunchContainerPanelDeprecated,
              extensions::LaunchType::LAUNCH_TYPE_REGULAR};
    case apps::mojom::LaunchContainer::kLaunchContainerTab:
      return {extensions::LaunchContainer::kLaunchContainerTab,
              extensions::LaunchType::LAUNCH_TYPE_REGULAR};
    case apps::mojom::LaunchContainer::kLaunchContainerWindow:
      return {extensions::LaunchContainer::kLaunchContainerTab,
              display_mode == web_app::DisplayMode::kFullscreen
                  ? extensions::LaunchType::LAUNCH_TYPE_FULLSCREEN
                  : extensions::LaunchType::LAUNCH_TYPE_WINDOW};
  }
  NOTREACHED();
}

}  // namespace extensions
