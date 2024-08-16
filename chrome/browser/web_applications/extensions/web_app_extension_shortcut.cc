// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/image/image_skia.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_win.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/common/chrome_switches.h"
#endif

using content::BrowserThread;

namespace web_app {

namespace {

void OnImageLoaded(std::unique_ptr<ShortcutInfo> shortcut_info,
                   ShortcutInfoCallback callback,
                   gfx::ImageFamily image_family) {
  // If the image failed to load (e.g. if the resource being loaded was empty)
  // use the standard application icon.
  if (image_family.empty()) {
    int size = GetDesiredIconSizesForShortcut().back();
    gfx::ImageSkia image_skia = CreateDefaultApplicationIcon(size);
    shortcut_info->favicon.Add(gfx::Image(image_skia));
  } else {
    shortcut_info->favicon = std::move(image_family);
  }

  std::move(callback).Run(std::move(shortcut_info));
}

void UpdateAllShortcutsForShortcutInfo(
    const std::u16string& old_app_title,
    ResultCallback callback,
    std::unique_ptr<ShortcutInfo> shortcut_info) {
  base::FilePath shortcut_data_dir =
      internals::GetShortcutDataDir(*shortcut_info);
  internals::ScheduleUpdatePlatformShortcuts(
      std::move(shortcut_data_dir), old_app_title,
      /*user_specified_locations=*/std::nullopt, std::move(callback),
      std::move(shortcut_info));
}

using AppCallbackMap =
    base::flat_map<webapps::AppId, std::vector<base::OnceClosure>>;
AppCallbackMap& GetShortcutsDeletedCallbackMap() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static base::NoDestructor<AppCallbackMap> map;
  return *map;
}

void ShortcutsDeleted(const webapps::AppId& app_id, bool /*shortcut_deleted*/) {
  auto& map = GetShortcutsDeletedCallbackMap();
  auto it = map.find(app_id);
  if (it == map.end())
    return;
  std::vector<base::OnceClosure> callbacks = std::move(it->second);
  map.erase(it);
  for (base::OnceClosure& callback : callbacks) {
    std::move(callback).Run();
  }
}

}  // namespace

void CreateShortcutsWithInfo(ShortcutCreationReason reason,
                             const ShortcutLocations& locations,
                             CreateShortcutsCallback callback,
                             std::unique_ptr<ShortcutInfo> shortcut_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (shortcut_info == nullptr) {
    std::move(callback).Run(/*created_shortcut=*/false);
    return;
  }

  // If the shortcut is for an application shortcut with the new bookmark app
  // flow disabled, there will be no corresponding extension.
  if (!shortcut_info->app_id.empty()) {
    // The profile manager does not exist in some unit tests.
    if (!g_browser_process->profile_manager()) {
      std::move(callback).Run(false /* created_shortcut */);
      return;
    }

    // It's possible for the extension to be deleted before we get here.
    // For example, creating a hosted app from a website. Double check that
    // it still exists.
    Profile* profile = g_browser_process->profile_manager()->GetProfileByPath(
        shortcut_info->profile_path);
    if (!profile) {
      std::move(callback).Run(false /* created_shortcut */);
      return;
    }

    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile);
    const extensions::Extension* extension = registry->GetExtensionById(
        shortcut_info->app_id, extensions::ExtensionRegistry::EVERYTHING);
    bool is_app_installed = false;
    auto* app_provider = WebAppProvider::GetForWebApps(profile);
    if (app_provider &&
        app_provider->registrar_unsafe().IsInstalled(shortcut_info->app_id)) {
      is_app_installed = true;
    }

    if (!extension && !is_app_installed) {
      std::move(callback).Run(false /* created_shortcut */);
      return;
    }
  }

  base::FilePath shortcut_data_dir =
      internals::GetShortcutDataDir(*shortcut_info);
  internals::ScheduleCreatePlatformShortcuts(shortcut_data_dir, locations,
                                             reason, std::move(shortcut_info),
                                             std::move(callback));
}

void GetShortcutInfoForApp(const extensions::Extension* extension,
                           Profile* profile,
                           ShortcutInfoCallback callback) {
  std::unique_ptr<ShortcutInfo> shortcut_info(
      ShortcutInfoForExtensionAndProfile(extension, profile));

  std::vector<extensions::ImageLoader::ImageRepresentation> info_list;

  for (int size : GetDesiredIconSizesForShortcut()) {
    extensions::ExtensionResource resource =
        extensions::IconsInfo::GetIconResource(
            extension, size, ExtensionIconSet::Match::kExactly);
    if (!resource.empty()) {
      info_list.emplace_back(
          resource, extensions::ImageLoader::ImageRepresentation::ALWAYS_RESIZE,
          gfx::Size(size, size),
          ui::GetScaleForResourceScaleFactor(ui::k100Percent));
    }
  }

  if (info_list.empty()) {
    int size = GetDesiredIconSizesForShortcut().back();

    // If there is no icon at the desired sizes, we will resize what we can get.
    // Making a large icon smaller is preferred to making a small icon larger,
    // so look for a larger icon first.
    // TODO(crbug.com/329953472): Use a predefined threshold.
    extensions::ExtensionResource resource =
        extensions::IconsInfo::GetIconResource(
            extension, size, ExtensionIconSet::Match::kBigger);
    if (resource.empty()) {
      resource = extensions::IconsInfo::GetIconResource(
          extension, size, ExtensionIconSet::Match::kSmaller);
    }
    info_list.emplace_back(
        resource, extensions::ImageLoader::ImageRepresentation::ALWAYS_RESIZE,
        gfx::Size(size, size),
        ui::GetScaleForResourceScaleFactor(ui::k100Percent));
  }

  // |info_list| may still be empty at this point, in which case
  // LoadImageFamilyAsync will call the OnImageLoaded callback with an empty
  // image and exit immediately.
  extensions::ImageLoader::Get(profile)->LoadImageFamilyAsync(
      extension, info_list,
      base::BindOnce(&OnImageLoaded, std::move(shortcut_info),
                     std::move(callback)));
}

std::unique_ptr<ShortcutInfo> ShortcutInfoForExtensionAndProfile(
    const extensions::Extension* app,
    Profile* profile) {
  auto shortcut_info = std::make_unique<ShortcutInfo>();

  shortcut_info->app_id = app->id();
  shortcut_info->url = extensions::AppLaunchInfo::GetLaunchWebURL(app);
  shortcut_info->title = base::UTF8ToUTF16(app->name());
  shortcut_info->description = base::UTF8ToUTF16(app->description());
  shortcut_info->profile_path = profile->GetPath();
  shortcut_info->profile_name =
      profile->GetPrefs()->GetString(prefs::kProfileName);
  shortcut_info->version_for_display = app->GetVersionForDisplay();

  return shortcut_info;
}

bool ShouldCreateShortcutFor(ShortcutCreationReason reason,
                             Profile* profile,
                             const extensions::Extension* extension) {
  // Shortcuts should never be created for component apps, or for apps that
  // cannot be shown in the launcher.
  if (extension->location() ==
          extensions::mojom::ManifestLocation::kComponent ||
      !extensions::ui_util::CanDisplayInAppLauncher(extension, profile)) {
    return false;
  }

  // Always create shortcuts for v2 packaged apps.
  if (extension->is_platform_app())
    return true;

  // Allow shortcut creation if it was explicitly requested by the user (i.e. is
  // not automatic).
  return reason == SHORTCUT_CREATION_BY_USER;
}

void CreateShortcuts(ShortcutCreationReason reason,
                     const ShortcutLocations& locations,
                     Profile* profile,
                     const extensions::Extension* app,
                     CreateShortcutsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!ShouldCreateShortcutFor(reason, profile, app)) {
    std::move(callback).Run(false /* created_shortcut */);
    return;
  }

  GetShortcutInfoForApp(app, profile,
                        base::BindOnce(&CreateShortcutsWithInfo, reason,
                                       locations, std::move(callback)));
}

void DeleteAllShortcuts(Profile* profile, const extensions::Extension* app) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::unique_ptr<ShortcutInfo> shortcut_info(
      ShortcutInfoForExtensionAndProfile(app, profile));
  base::FilePath shortcut_data_dir =
      internals::GetShortcutDataDir(*shortcut_info);
  internals::ScheduleDeletePlatformShortcuts(
      shortcut_data_dir, std::move(shortcut_info),
      base::BindOnce(ShortcutsDeleted, app->id()));
}

void WaitForExtensionShortcutsDeleted(const webapps::AppId& app_id,
                                      base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetShortcutsDeletedCallbackMap()[app_id].push_back(std::move(callback));
}

void UpdateAllShortcuts(const std::u16string& old_app_title,
                        Profile* profile,
                        const extensions::Extension* app,
                        base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ResultCallback metrics_callback =
      base::BindOnce([](Result result) {
        base::UmaHistogramBoolean("WebApp.Shortcuts.Update.Result",
                                  (result == Result::kOk));
      }).Then(std::move(callback));

  GetShortcutInfoForApp(
      app, profile,
      base::BindOnce(&UpdateAllShortcutsForShortcutInfo, old_app_title,
                     std::move(metrics_callback)));
}

#if !BUILDFLAG(IS_MAC)
void UpdateShortcutsForAllApps(Profile* profile, base::OnceClosure callback) {
  std::move(callback).Run();
}
#endif

#if BUILDFLAG(IS_WIN)
void UpdateRelaunchDetailsForApp(Profile* profile,
                                 const extensions::Extension* extension,
                                 HWND hwnd) {
  GetShortcutInfoForApp(
      extension, profile,
      base::BindOnce(&internals::OnShortcutInfoLoadedForSetRelaunchDetails,
                     hwnd));
}
#endif

SynchronizeOsOptions ConvertShortcutLocationsToSynchronizeOptions(
    const ShortcutLocations& locations,
    ShortcutCreationReason reason) {
  SynchronizeOsOptions options;
  options.reason = reason;
  options.add_shortcut_to_desktop = locations.on_desktop;
  options.add_to_quick_launch_bar = locations.in_quick_launch_bar;
  // Since shortcuts can be manually deleted by thd end user, there
  // is no way to listen to that information and update that in the
  // web_app DB. Setting this flag allows shortcuts to always
  // be force created.
  options.force_create_shortcuts = true;
  return options;
}

}  // namespace web_app
