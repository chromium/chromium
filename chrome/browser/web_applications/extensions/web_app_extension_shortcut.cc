// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "skia/ext/image_operations.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

#if defined(OS_WIN)
#include "chrome/browser/web_applications/components/web_app_shortcut_win.h"
#include "ui/gfx/icon_util.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/common/chrome_switches.h"
#endif

using content::BrowserThread;

namespace web_app {

namespace {

#if defined(OS_MACOSX)
const int kDesiredSizes[] = {16, 32, 128, 256, 512};
const size_t kNumDesiredSizes = base::size(kDesiredSizes);
#elif defined(OS_LINUX)
// Linux supports icons of any size. FreeDesktop Icon Theme Specification states
// that "Minimally you should install a 48x48 icon in the hicolor theme."
const int kDesiredSizes[] = {16, 32, 48, 128, 256, 512};
const size_t kNumDesiredSizes = base::size(kDesiredSizes);
#elif defined(OS_WIN)
const int* kDesiredSizes = IconUtil::kIconDimensions;
const size_t kNumDesiredSizes = IconUtil::kNumIconDimensions;
#else
const int kDesiredSizes[] = {32};
const size_t kNumDesiredSizes = base::size(kDesiredSizes);
#endif

void OnImageLoaded(std::unique_ptr<ShortcutInfo> shortcut_info,
                   ShortcutInfoCallback callback,
                   gfx::ImageFamily image_family) {
  // If the image failed to load (e.g. if the resource being loaded was empty)
  // use the standard application icon.
  if (image_family.empty()) {
    gfx::Image default_icon =
        ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            IDR_APP_DEFAULT_ICON);
    int size = kDesiredSizes[kNumDesiredSizes - 1];
    SkBitmap bmp = skia::ImageOperations::Resize(
        *default_icon.ToSkBitmap(), skia::ImageOperations::RESIZE_BEST, size,
        size);
    gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(bmp);
    // We are on the UI thread, and this image is needed from the FILE thread,
    // for creating shortcut icon files.
    image_skia.MakeThreadSafe();
    shortcut_info->favicon.Add(gfx::Image(image_skia));
  } else {
    shortcut_info->favicon = std::move(image_family);
  }

  std::move(callback).Run(std::move(shortcut_info));
}

void UpdateAllShortcutsForShortcutInfo(
    const base::string16& old_app_title,
    base::OnceClosure callback,
    std::unique_ptr<ShortcutInfo> shortcut_info) {
  base::FilePath shortcut_data_dir =
      internals::GetShortcutDataDir(*shortcut_info);
  internals::PostShortcutIOTaskAndReply(
      base::BindOnce(&internals::UpdatePlatformShortcuts, shortcut_data_dir,
                     old_app_title),
      std::move(shortcut_info), std::move(callback));
}

void ScheduleCreatePlatformShortcut(
    ShortcutCreationReason reason,
    const ShortcutLocations& locations,
    std::unique_ptr<ShortcutInfo> shortcut_info) {
  base::FilePath shortcut_data_dir =
      internals::GetShortcutDataDir(*shortcut_info);
  internals::PostShortcutIOTask(
      base::BindOnce(base::IgnoreResult(&internals::CreatePlatformShortcuts),
                     shortcut_data_dir, locations, reason),
      std::move(shortcut_info));
}

}  // namespace

void CreateShortcutsWithInfo(ShortcutCreationReason reason,
                             const ShortcutLocations& locations,
                             std::unique_ptr<ShortcutInfo> shortcut_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the shortcut is for an application shortcut with the new bookmark app
  // flow disabled, there will be no corresponding extension.
  if (!shortcut_info->extension_id.empty()) {
    // The profile manager does not exist in some unit tests.
    if (!g_browser_process->profile_manager())
      return;

    // It's possible for the extension to be deleted before we get here.
    // For example, creating a hosted app from a website. Double check that
    // it still exists.
    Profile* profile = g_browser_process->profile_manager()->GetProfileByPath(
        shortcut_info->profile_path);
    if (!profile)
      return;

    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile);
    const extensions::Extension* extension = registry->GetExtensionById(
        shortcut_info->extension_id, extensions::ExtensionRegistry::EVERYTHING);
    if (!extension)
      return;
  }

  ScheduleCreatePlatformShortcut(reason, locations, std::move(shortcut_info));
}

void GetShortcutInfoForApp(const extensions::Extension* extension,
                           Profile* profile,
                           ShortcutInfoCallback callback) {
  std::unique_ptr<ShortcutInfo> shortcut_info(
      ShortcutInfoForExtensionAndProfile(extension, profile));

  std::vector<extensions::ImageLoader::ImageRepresentation> info_list;
  for (size_t i = 0; i < kNumDesiredSizes; ++i) {
    int size = kDesiredSizes[i];
    extensions::ExtensionResource resource =
        extensions::IconsInfo::GetIconResource(extension, size,
                                               ExtensionIconSet::MATCH_EXACTLY);
    if (!resource.empty()) {
      info_list.push_back(extensions::ImageLoader::ImageRepresentation(
          resource, extensions::ImageLoader::ImageRepresentation::ALWAYS_RESIZE,
          gfx::Size(size, size), ui::SCALE_FACTOR_100P));
    }
  }

  if (info_list.empty()) {
    size_t i = kNumDesiredSizes - 1;
    int size = kDesiredSizes[i];

    // If there is no icon at the desired sizes, we will resize what we can get.
    // Making a large icon smaller is preferred to making a small icon larger,
    // so look for a larger icon first:
    extensions::ExtensionResource resource =
        extensions::IconsInfo::GetIconResource(extension, size,
                                               ExtensionIconSet::MATCH_BIGGER);
    if (resource.empty()) {
      resource = extensions::IconsInfo::GetIconResource(
          extension, size, ExtensionIconSet::MATCH_SMALLER);
    }
    info_list.push_back(extensions::ImageLoader::ImageRepresentation(
        resource, extensions::ImageLoader::ImageRepresentation::ALWAYS_RESIZE,
        gfx::Size(size, size), ui::SCALE_FACTOR_100P));
  }

  // |info_list| may still be empty at this point, in which case
  // LoadImageFamilyAsync will call the OnImageLoaded callback with an empty
  // image and exit immediately.
  extensions::ImageLoader::Get(profile)->LoadImageFamilyAsync(
      extension, info_list,
      base::Bind(&OnImageLoaded, base::Passed(&shortcut_info),
                 base::Passed(&callback)));
}

std::unique_ptr<ShortcutInfo> ShortcutInfoForExtensionAndProfile(
    const extensions::Extension* app,
    Profile* profile) {
  std::unique_ptr<ShortcutInfo> shortcut_info(new ShortcutInfo);
  shortcut_info->extension_id = app->id();
  shortcut_info->is_platform_app = app->is_platform_app();

  // Some default-installed apps are converted into bookmark apps on Chrome
  // first run. These should not be considered as being created (by the user)
  // from a web page.
  shortcut_info->from_bookmark =
      app->from_bookmark() && !app->was_installed_by_default();

  shortcut_info->url = extensions::AppLaunchInfo::GetLaunchWebURL(app);
  shortcut_info->title = base::UTF8ToUTF16(app->name());
  shortcut_info->description = base::UTF8ToUTF16(app->description());
  shortcut_info->extension_path = app->path();
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
  if (extension->location() == extensions::Manifest::COMPONENT ||
      !extensions::ui_util::CanDisplayInAppLauncher(extension, profile)) {
    return false;
  }

  // Always create shortcuts for v2 packaged apps.
  if (extension->is_platform_app())
    return true;

#if defined(OS_MACOSX)
  // A bookmark app installs itself as an extension, then automatically triggers
  // a shortcut request with SHORTCUT_CREATION_AUTOMATED. Allow this flow, but
  // do not automatically create shortcuts for default-installed extensions,
  // until it is explicitly requested by the user.
  if (extension->was_installed_by_default() &&
      reason == SHORTCUT_CREATION_AUTOMATED)
    return false;

  if (extension->from_bookmark())
    return true;

  // Otherwise, don't create shortcuts for automated codepaths.
  if (reason == SHORTCUT_CREATION_AUTOMATED)
    return false;

  if (extension->is_hosted_app()) {
    return !base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kDisableHostedAppShimCreation);
  }

  // Only reached for "legacy" packaged apps. Default to false on Mac.
  return false;
#else
  // For other platforms, allow shortcut creation if it was explicitly
  // requested by the user (i.e. is not automatic).
  return reason == SHORTCUT_CREATION_BY_USER;
#endif
}

base::FilePath GetWebAppDataDirectory(const base::FilePath& profile_path,
                                      const extensions::Extension& extension) {
  return GetWebAppDataDirectory(
      profile_path, extension.id(),
      GURL(extensions::AppLaunchInfo::GetLaunchWebURL(&extension)));
}

void CreateShortcuts(ShortcutCreationReason reason,
                     const ShortcutLocations& locations,
                     Profile* profile,
                     const extensions::Extension* app) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!ShouldCreateShortcutFor(reason, profile, app))
    return;

  GetShortcutInfoForApp(
      app, profile,
      base::BindOnce(&CreateShortcutsWithInfo, reason, locations));
}

void DeleteAllShortcuts(Profile* profile, const extensions::Extension* app) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::unique_ptr<ShortcutInfo> shortcut_info(
      ShortcutInfoForExtensionAndProfile(app, profile));
  base::FilePath shortcut_data_dir =
      internals::GetShortcutDataDir(*shortcut_info);
  internals::PostShortcutIOTask(
      base::BindOnce(&internals::DeletePlatformShortcuts, shortcut_data_dir),
      std::move(shortcut_info));
}

void UpdateAllShortcuts(const base::string16& old_app_title,
                        Profile* profile,
                        const extensions::Extension* app,
                        base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GetShortcutInfoForApp(app, profile,
                        base::BindOnce(&UpdateAllShortcutsForShortcutInfo,
                                       old_app_title, std::move(callback)));
}

#if !defined(OS_MACOSX)
void UpdateShortcutsForAllApps(Profile* profile, base::OnceClosure callback) {
  std::move(callback).Run();
}
#endif

#if defined(OS_WIN)
void UpdateRelaunchDetailsForApp(Profile* profile,
                                 const extensions::Extension* extension,
                                 HWND hwnd) {
  GetShortcutInfoForApp(
      extension, profile,
      base::BindOnce(&internals::OnShortcutInfoLoadedForSetRelaunchDetails,
                     hwnd));
}
#endif

}  // namespace web_app
