// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/one_shot_event.h"
#include "chrome/browser/extensions/convert_web_app.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "chrome/common/extensions/manifest_handlers/app_display_mode_info.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_handlers/app_theme_color_info.h"
#include "chrome/common/extensions/manifest_handlers/linked_app_icons.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/web_app_linked_shortcut_items.h"
#include "url/gurl.h"

using web_app::DisplayMode;

namespace extensions {
namespace {
// These need to match the keys in
// chrome/browser/extensions/chrome_app_sorting.cc
const char kPrefAppLaunchOrdinal[] = "app_launcher_ordinal";
const char kPrefPageOrdinal[] = "page_ordinal";
}  // namespace

BookmarkAppRegistrar::BookmarkAppRegistrar(Profile* profile)
    : AppRegistrar(profile) {
}

BookmarkAppRegistrar::~BookmarkAppRegistrar() = default;

void BookmarkAppRegistrar::Start() {
  extension_observer_.Add(ExtensionRegistry::Get(profile()));
}

bool BookmarkAppRegistrar::IsInstalled(const web_app::AppId& app_id) const {
  const Extension* extension = GetEnabledExtension(app_id);
  return extension && extension->from_bookmark();
}

bool BookmarkAppRegistrar::IsLocallyInstalled(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetEnabledExtension(app_id);
  return extension && extension->from_bookmark() &&
         BookmarkAppIsLocallyInstalled(profile(), extension);
}

bool BookmarkAppRegistrar::WasInstalledByUser(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetBookmarkAppDchecked(app_id);
  return extension && !extension->was_installed_by_default();
}

bool BookmarkAppRegistrar::WasInstalledByOem(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetEnabledExtension(app_id);
  return extension && extension->from_bookmark() &&
         extension->was_installed_by_oem();
}

int BookmarkAppRegistrar::CountUserInstalledApps() const {
  return CountUserInstalledBookmarkApps(profile());
}

void BookmarkAppRegistrar::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason reason) {
  DCHECK_EQ(browser_context, profile());
  if (extension->from_bookmark()) {
    DCHECK(!bookmark_app_being_observed_);
    bookmark_app_being_observed_ = extension;

    NotifyWebAppWillBeUninstalled(extension->id());
    os_integration_manager().UninstallAllOsHooks(extension->id(),
                                                  base::DoNothing());

    bookmark_app_being_observed_ = nullptr;
  }
}

void BookmarkAppRegistrar::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  DCHECK_EQ(browser_context, profile());
  if (!extension->from_bookmark())
    return;

  DCHECK(!bookmark_app_being_observed_);
  bookmark_app_being_observed_ = extension;

  // If a profile is removed, notify the web app that it is uninstalled, so it
  // can cleanup any state outside the profile dir (e.g., registry settings).
  if (reason == UnloadedExtensionReason::PROFILE_SHUTDOWN) {
    NotifyWebAppProfileWillBeDeleted(extension->id());
    os_integration_manager().UninstallAllOsHooks(extension->id(),
                                                  base::DoNothing());
  }

  bookmark_app_being_observed_ = nullptr;
}

void BookmarkAppRegistrar::OnShutdown(ExtensionRegistry* registry) {
  NotifyAppRegistrarShutdown();
  extension_observer_.RemoveAll();
}

std::string BookmarkAppRegistrar::GetAppShortName(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetBookmarkAppDchecked(app_id);
  return extension ? extension->short_name() : std::string();
}

std::string BookmarkAppRegistrar::GetAppDescription(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetBookmarkAppDchecked(app_id);
  return extension ? extension->description() : std::string();
}

base::Optional<SkColor> BookmarkAppRegistrar::GetAppThemeColor(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetBookmarkAppDchecked(app_id);
  if (!extension)
    return base::nullopt;

  return AppThemeColorInfo::GetThemeColor(extension);
}

base::Optional<SkColor> BookmarkAppRegistrar::GetAppBackgroundColor(
    const web_app::AppId& app_id) const {
  // Only implemented for WebApp. Bookmark apps are going away.
  return base::nullopt;
}

const GURL& BookmarkAppRegistrar::GetAppStartUrl(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetBookmarkAppDchecked(app_id);
  return extension ? AppLaunchInfo::GetLaunchWebURL(extension)
                   : GURL::EmptyGURL();
}

// Only implemented for WebApp. Bookmark apps are going away.
const std::string* BookmarkAppRegistrar::GetAppLaunchQueryParams(
    const web_app::AppId& app_id) const {
  return nullptr;
}

// Only implemented for WebApp. Bookmark apps are going away.
const apps::ShareTarget* BookmarkAppRegistrar::GetAppShareTarget(
    const web_app::AppId& app_id) const {
  return nullptr;
}

// Only implemented for WebApp. Bookmark apps are going away.
blink::mojom::CaptureLinks BookmarkAppRegistrar::GetAppCaptureLinks(
    const web_app::AppId& app_id) const {
  return blink::mojom::CaptureLinks::kUndefined;
}

const apps::FileHandlers* BookmarkAppRegistrar::GetAppFileHandlers(
    const web_app::AppId& app_id) const {
  return nullptr;
}

base::Optional<GURL> BookmarkAppRegistrar::GetAppScopeInternal(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetBookmarkAppDchecked(app_id);
  if (!extension)
    return base::nullopt;

  GURL scope_url = GetScopeURLFromBookmarkApp(GetBookmarkAppDchecked(app_id));
  if (scope_url.is_valid())
    return scope_url;

  return base::nullopt;
}

DisplayMode BookmarkAppRegistrar::GetAppDisplayMode(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetBookmarkAppDchecked(app_id);
  if (!extension)
    return DisplayMode::kUndefined;

  return AppDisplayModeInfo::GetDisplayMode(extension);
}

DisplayMode BookmarkAppRegistrar::GetAppUserDisplayMode(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetBookmarkAppDchecked(app_id);
  if (!extension)
    return DisplayMode::kStandalone;

  switch (extensions::GetLaunchContainer(
      extensions::ExtensionPrefs::Get(profile()), extension)) {
    case LaunchContainer::kLaunchContainerWindow:
    case LaunchContainer::kLaunchContainerPanelDeprecated:
      return DisplayMode::kStandalone;
    case LaunchContainer::kLaunchContainerTab:
      return DisplayMode::kBrowser;
    case LaunchContainer::kLaunchContainerNone:
      NOTREACHED();
      return DisplayMode::kUndefined;
  }
}

DisplayMode BookmarkAppRegistrar::GetAppUserDisplayModeForMigration(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetBookmarkAppDchecked(app_id);
  if (!extension)
    return DisplayMode::kStandalone;

  LaunchType launch_type = LAUNCH_TYPE_WINDOW;
  int launch_type_value = GetLaunchTypePrefValue(
      extensions::ExtensionPrefs::Get(profile()), app_id);
  if (launch_type_value >= LAUNCH_TYPE_FIRST &&
      launch_type_value < NUM_LAUNCH_TYPES) {
    launch_type = static_cast<LaunchType>(launch_type_value);
  }

  DisplayMode user_display_mode = DisplayMode::kStandalone;
  switch (launch_type) {
    case LAUNCH_TYPE_PINNED:
    case LAUNCH_TYPE_REGULAR:
      user_display_mode = DisplayMode::kBrowser;
      break;
    case LAUNCH_TYPE_FULLSCREEN:
    case LAUNCH_TYPE_WINDOW:
    case LAUNCH_TYPE_INVALID:
    case NUM_LAUNCH_TYPES:
      user_display_mode = DisplayMode::kStandalone;
      break;
  }

  return user_display_mode;
}

std::vector<DisplayMode> BookmarkAppRegistrar::GetAppDisplayModeOverride(
    const web_app::AppId& app_id) const {
  NOTIMPLEMENTED();
  return std::vector<DisplayMode>();
}

apps::UrlHandlers BookmarkAppRegistrar::GetAppUrlHandlers(
    const web_app::AppId& app_id) const {
  NOTIMPLEMENTED();
  return std::vector<apps::UrlHandlerInfo>();
}

GURL BookmarkAppRegistrar::GetAppManifestUrl(
    const web_app::AppId& app_id) const {
  NOTIMPLEMENTED();
  return GURL::EmptyGURL();
}

base::Time BookmarkAppRegistrar::GetAppLastBadgingTime(
    const web_app::AppId& app_id) const {
  NOTIMPLEMENTED();
  return base::Time();
}

base::Time BookmarkAppRegistrar::GetAppLastLaunchTime(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetBookmarkAppDchecked(app_id);
  return extension
             ? extensions::ExtensionPrefs::Get(profile())->GetLastLaunchTime(
                   extension->id())
             : base::Time();
}

base::Time BookmarkAppRegistrar::GetAppInstallTime(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetBookmarkAppDchecked(app_id);
  return extension ? extensions::ExtensionPrefs::Get(profile())->GetInstallTime(
                         extension->id())
                   : base::Time();
}

std::vector<WebApplicationIconInfo> BookmarkAppRegistrar::GetAppIconInfos(
    const web_app::AppId& app_id) const {
  std::vector<WebApplicationIconInfo> result;
  const Extension* extension = GetBookmarkAppDchecked(app_id);
  if (!extension)
    return result;
  for (const LinkedAppIcons::IconInfo& icon_info :
       LinkedAppIcons::GetLinkedAppIcons(extension).icons) {
    WebApplicationIconInfo web_app_icon_info;
    web_app_icon_info.url = icon_info.url;
    if (icon_info.size != LinkedAppIcons::kAnySize)
      web_app_icon_info.square_size_px = icon_info.size;
    // Legacy bookmark apps only have Purpose::ANY icons.
    web_app_icon_info.purpose =
        blink::mojom::ManifestImageResource_Purpose::ANY;
    result.push_back(std::move(web_app_icon_info));
  }
  return result;
}

SortedSizesPx BookmarkAppRegistrar::GetAppDownloadedIconSizesAny(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetBookmarkAppDchecked(app_id);
  return extension ? GetBookmarkAppDownloadedIconSizes(extension)
                   : SortedSizesPx();
}

std::vector<WebApplicationShortcutsMenuItemInfo>
BookmarkAppRegistrar::GetAppShortcutsMenuItemInfos(
    const web_app::AppId& app_id) const {
  std::vector<WebApplicationShortcutsMenuItemInfo> result;
  const Extension* extension = GetBookmarkAppDchecked(app_id);
  if (!extension)
    return result;

  const WebAppLinkedShortcutItems& linked_shortcut_items =
      WebAppLinkedShortcutItems::GetWebAppLinkedShortcutItems(extension);
  result.reserve(linked_shortcut_items.shortcut_item_infos.size());
  for (const WebAppLinkedShortcutItems::ShortcutItemInfo& linked_item_info :
       linked_shortcut_items.shortcut_item_infos) {
    WebApplicationShortcutsMenuItemInfo shortcut_item_info;
    shortcut_item_info.name = linked_item_info.name;
    shortcut_item_info.url = linked_item_info.url;

    std::vector<WebApplicationShortcutsMenuItemInfo::Icon> shortcut_icon_infos;
    shortcut_icon_infos.reserve(
        linked_item_info.shortcut_item_icon_infos.size());
    for (const WebAppLinkedShortcutItems::ShortcutItemInfo::IconInfo&
             shortcut_item_icon_info :
         linked_item_info.shortcut_item_icon_infos) {
      WebApplicationShortcutsMenuItemInfo::Icon shortcut_icon_info;
      shortcut_icon_info.square_size_px = shortcut_item_icon_info.size;
      shortcut_icon_info.url = shortcut_item_icon_info.url;
      shortcut_icon_infos.emplace_back(std::move(shortcut_icon_info));
    }
    shortcut_item_info.SetShortcutIconInfosForPurpose(
        IconPurpose::ANY, std::move(shortcut_icon_infos));
    result.emplace_back(std::move(shortcut_item_info));
  }
  return result;
}

std::vector<IconSizes>
BookmarkAppRegistrar::GetAppDownloadedShortcutsMenuIconsSizes(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetBookmarkAppDchecked(app_id);
  return extension ? GetBookmarkAppDownloadedShortcutsMenuIconsSizes(extension)
                   : std::vector<IconSizes>();
}

web_app::RunOnOsLoginMode BookmarkAppRegistrar::GetAppRunOnOsLoginMode(
    const web_app::AppId& app_id) const {
  NOTIMPLEMENTED();
  return web_app::RunOnOsLoginMode::kNotRun;
}

std::vector<web_app::AppId> BookmarkAppRegistrar::GetAppIds() const {
  std::vector<web_app::AppId> app_ids;
  for (scoped_refptr<const Extension> app :
       ExtensionRegistry::Get(profile())->enabled_extensions()) {
    if (app->from_bookmark()) {
      app_ids.push_back(app->id());
    }
  }
  return app_ids;
}

web_app::WebAppRegistrar* BookmarkAppRegistrar::AsWebAppRegistrar() {
  return nullptr;
}

BookmarkAppRegistrar* BookmarkAppRegistrar::AsBookmarkAppRegistrar() {
  return this;
}

syncer::StringOrdinal BookmarkAppRegistrar::GetUserPageOrdinal(
    const web_app::AppId& app_id) const {
  std::string page_ordinal_raw_data;
  // If the preference read fails then raw_data will still be unset and we
  // will return an invalid StringOrdinal to signal that no page ordinal was
  // found.
  extensions::ExtensionPrefs::Get(profile())->ReadPrefAsString(
      app_id, kPrefPageOrdinal, &page_ordinal_raw_data);
  return syncer::StringOrdinal(page_ordinal_raw_data);
}

syncer::StringOrdinal BookmarkAppRegistrar::GetUserLaunchOrdinal(
    const web_app::AppId& app_id) const {
  std::string launch_ordinal_raw_data;
  // If the preference read fails then raw_data will still be unset and we
  // will return an invalid StringOrdinal to signal that no page ordinal was
  // found.
  extensions::ExtensionPrefs::Get(profile())->ReadPrefAsString(
      app_id, kPrefAppLaunchOrdinal, &launch_ordinal_raw_data);
  return syncer::StringOrdinal(launch_ordinal_raw_data);
}

const Extension* BookmarkAppRegistrar::FindExtension(
    const web_app::AppId& app_id) const {
  if (bookmark_app_being_observed_ &&
      bookmark_app_being_observed_->id() == app_id) {
    return bookmark_app_being_observed_;
  }

  return ExtensionRegistry::Get(profile())->GetInstalledExtension(app_id);
}

const Extension* BookmarkAppRegistrar::GetBookmarkAppDchecked(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetEnabledExtension(app_id);
  DCHECK(!extension || extension->from_bookmark());
  return extension;
}

const Extension* BookmarkAppRegistrar::GetEnabledExtension(
    const web_app::AppId& app_id) const {
  return ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
      app_id);
}

}  // namespace extensions
