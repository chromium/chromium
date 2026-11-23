// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/mac/apps_folder_support.h"

#import <Cocoa/Cocoa.h>

#include <atomic>

#import "base/apple/foundation_util.h"
#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/version_info/channel.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/shortcuts/platform_util_mac.h"
#include "chrome/browser/web_applications/os_integration/mac/icon_utils.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/common/channel_info.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace web_app {
namespace {

// Set to true the first time the localized name of the chrome apps dir has been
// updated successfully, as this only needs to be done once.
std::atomic<bool> g_have_localized_app_dir_name{false};

base::FilePath GetLocalizableAppShortcutsSubdirName() {
  static const char kChromiumAppDirName[] = "Chromium Apps.localized";
  static const char kChromeAppDirName[] = "Chrome Apps.localized";
  static const char kChromeCanaryAppDirName[] = "Chrome Canary Apps.localized";

  switch (chrome::GetChannel()) {
    case version_info::Channel::UNKNOWN:
      return base::FilePath(kChromiumAppDirName);

    case version_info::Channel::CANARY:
      return base::FilePath(kChromeCanaryAppDirName);

    default:
      return base::FilePath(kChromeAppDirName);
  }
}

// Returns `~/Applications`, or an empty path on failure. If `create_if_missing`
// is true, creates the folder if it is missing and requires it to be writable.
// Otherwise, creates nothing and does not check that the folder is writable.
base::FilePath GetApplicationsDirectory(bool create_if_missing) {
  base::FilePath path;
  if (!base::apple::GetUserDirectory(NSApplicationDirectory, &path)) {
    return base::FilePath();
  }
  if (!create_if_missing) {
    return path;
  }
  if (!base::DirectoryExists(path)) {
    if (!base::CreateDirectory(path)) {
      return base::FilePath();
    }

    // Create a zero-byte ".localized" file to inherit localizations from
    // macOS for folders that have special meaning.
    base::WriteFile(path.Append(".localized"), "");
  }
  return base::PathIsWritable(path) ? path : base::FilePath();
}

void SetWorkspaceIconOnWorkerThread(const base::FilePath& apps_directory,
                                    const ResourceIDToImage& images) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  NSImage* folder_icon_image = CreateMacAppsFolderIcon(images);

  shortcuts::SetIconForFile(folder_icon_image, apps_directory,
                            base::DoNothing());
}

// Adds a localized strings file for the Chrome Apps directory using the current
// locale. macOS will use this for the display name.
// + Chrome Apps.localized (|apps_directory|)
// | + .localized
// | | en.strings
// | | de.strings
bool UpdateAppShortcutsSubdirLocalizedName(
    const base::FilePath& apps_directory) {
  base::FilePath localized = apps_directory.Append(".localized");
  if (!base::CreateDirectory(localized)) {
    return false;
  }

  base::FilePath directory_name = apps_directory.BaseName().RemoveExtension();
  std::u16string localized_name =
      shell_integration::GetAppShortcutsSubdirName();
  NSDictionary* strings_dict = @{
    base::apple::FilePathToNSString(directory_name) :
        base::SysUTF16ToNSString(localized_name)
  };

  base::i18n::LanguageTag locale_tag =
      base::i18n::LanguageTagConverter::GetInstance()
          .FromString(l10n_util::GetApplicationLocale(std::string()))
          .value_or(base::i18n::GetKnownLanguageTag("und"));
  NSURL* strings_url = base::apple::FilePathToNSURL(
      localized.Append(locale_tag.ToLegacyICUFormat() + ".strings"));
  [strings_dict writeToURL:strings_url error:nil];

  content::GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&GetImageResourcesOnUIThread),
      base::BindOnce(&SetWorkspaceIconOnWorkerThread, apps_directory));
  return true;
}

// The shared implementation of `GetChromeAppsFolder()` and
// `GetOrCreateChromeAppsFolder()`.
base::FilePath GetChromeAppsFolderImpl(bool create_if_missing) {
  base::FilePath path;
  scoped_refptr<OsIntegrationTestOverride> os_override =
      OsIntegrationTestOverride::Get();
  if (os_override) {
    CHECK_IS_TEST();
    if (!os_override->IsChromeAppsValid()) {
      return base::FilePath();
    }
    path = os_override->chrome_apps_folder();
  } else {
    base::FilePath applications_dir =
        GetApplicationsDirectory(create_if_missing);
    if (applications_dir.empty()) {
      return base::FilePath();
    }
    path = applications_dir.Append(GetLocalizableAppShortcutsSubdirName());
  }

  if (!create_if_missing) {
    return base::DirectoryExists(path) ? path : base::FilePath();
  }

  // Create the folder here, because the localization below creates it only
  // until it succeeds once in a process. On failure, still return the path,
  // so that `WebAppShortcutCreator` can update shims in other locations and
  // report the error when it copies a new shim.
  if (!base::CreateDirectory(path)) {
    LOG(ERROR) << "Failed to create " << path;
  }
  UpdateChromeAppsFolderLocalizationIfNeeded(path);
  return path;
}

}  // namespace

base::FilePath GetChromeAppsFolder() {
  return GetChromeAppsFolderImpl(/*create_if_missing=*/false);
}

base::FilePath GetOrCreateChromeAppsFolder() {
  return GetChromeAppsFolderImpl(/*create_if_missing=*/true);
}

void UpdateChromeAppsFolderLocalizationIfNeeded(
    const base::FilePath& chrome_apps_folder) {
  if (chrome_apps_folder.empty()) {
    return;
  }
  // Only set folder icons and a localized name once, as nothing should be
  // changing the folder icon and name.
  if (!g_have_localized_app_dir_name.load()) {
    g_have_localized_app_dir_name.store(
        UpdateAppShortcutsSubdirLocalizedName(chrome_apps_folder));
  }
  LOG_IF(ERROR, !g_have_localized_app_dir_name.load())
      << "Failed to localize " << chrome_apps_folder;
}

void ResetHaveLocalizedAppDirNameForTesting() {
  g_have_localized_app_dir_name.store(false);
}

}  // namespace web_app
