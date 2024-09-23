// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/mac/apps_folder_support.h"

#import <Cocoa/Cocoa.h>

#import "base/apple/foundation_util.h"
#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
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
// updated sucessfully, as this only needs to be done once.
bool g_have_localized_app_dir_name = false;

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

base::FilePath GetWritableApplicationsDirectory() {
  base::FilePath path;
  if (base::apple::GetUserDirectory(NSApplicationDirectory, &path)) {
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
  return base::FilePath();
}

base::FilePath GetChromeAppsFolderImpl() {
  scoped_refptr<OsIntegrationTestOverride> os_override =
      OsIntegrationTestOverride::Get();
  if (os_override) {
    CHECK_IS_TEST();
    if (os_override->IsChromeAppsValid()) {
      return os_override->chrome_apps_folder();
    }
    return base::FilePath();
  }

  base::FilePath path = GetWritableApplicationsDirectory();
  if (path.empty()) {
    return path;
  }

  return path.Append(GetLocalizableAppShortcutsSubdirName());
}

// Helper function to extract the single NSImageRep held in a resource bundle
// image.
NSImageRep* ImageRepForGFXImage(const gfx::Image& image) {
  NSArray* image_reps = image.AsNSImage().representations;
  DCHECK_EQ(1u, image_reps.count);
  return image_reps[0];
}

using ResourceIDToImage = std::map<int, NSImageRep*>;

// Generates a map of NSImageReps used by SetWorkspaceIconOnWorkerThread and
// passes it to |io_task|. Since ui::ResourceBundle can only be used on UI
// thread, this function also needs to run on UI thread, and the gfx::Images
// need to be converted to NSImageReps on the UI thread due to non-thread-safety
// of gfx::Image.
ResourceIDToImage GetImageResourcesOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();
  ResourceIDToImage result;

  // These resource ID should match to the ones used by
  // SetWorkspaceIconOnWorkerThread below.
  for (int id : {IDR_APPS_FOLDER_16, IDR_APPS_FOLDER_32,
                 IDR_APPS_FOLDER_OVERLAY_128, IDR_APPS_FOLDER_OVERLAY_512}) {
    gfx::Image image = resource_bundle.GetNativeImageNamed(id);
    result[id] = ImageRepForGFXImage(image);
  }

  return result;
}

void SetWorkspaceIconOnWorkerThread(const base::FilePath& apps_directory,
                                    const ResourceIDToImage& images) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  NSImage* folder_icon_image = [[NSImage alloc] init];
  // Use complete assets for the small icon sizes. -[NSWorkspace setIcon:] has a
  // bug when dealing with named NSImages where it incorrectly handles alpha
  // premultiplication. This is most noticeable with small assets since the 1px
  // border is a much larger component of the small icons.
  // See http://crbug.com/305373 for details.
  for (int id : {IDR_APPS_FOLDER_16, IDR_APPS_FOLDER_32}) {
    const auto& found = images.find(id);
    DCHECK(found != images.end());
    [folder_icon_image addRepresentation:found->second];
  }

  // Brand larger folder assets with an embossed app launcher logo to
  // conserve distro size and for better consistency with changing hue
  // across macOS versions. The folder is textured, so compresses poorly
  // without this.
  NSImage* base_image = [NSImage imageNamed:NSImageNameFolder];
  for (int id : {IDR_APPS_FOLDER_OVERLAY_128, IDR_APPS_FOLDER_OVERLAY_512}) {
    const auto& found = images.find(id);
    DCHECK(found != images.end());
    NSImageRep* with_overlay = OverlayImageRep(base_image, found->second);
    DCHECK(with_overlay);
    if (with_overlay) {
      [folder_icon_image addRepresentation:with_overlay];
    }
  }
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

  std::string locale = l10n_util::NormalizeLocale(
      l10n_util::GetApplicationLocale(std::string()));

  NSURL* strings_url =
      base::apple::FilePathToNSURL(localized.Append(locale + ".strings"));
  [strings_dict writeToURL:strings_url error:nil];

  content::GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&GetImageResourcesOnUIThread),
      base::BindOnce(&SetWorkspaceIconOnWorkerThread, apps_directory));
  return true;
}

}  // namespace

base::FilePath GetChromeAppsFolder() {
  base::FilePath path = GetChromeAppsFolderImpl();

  if (path.empty()) {
    return path;
  }

  // Only set folder icons and a localized name once, as nothing should be
  // changing the folder icon and name.
  if (!g_have_localized_app_dir_name) {
    g_have_localized_app_dir_name = UpdateAppShortcutsSubdirLocalizedName(path);
  }
  if (!g_have_localized_app_dir_name) {
    LOG(ERROR) << "Failed to localize " << path;
  }

  return path;
}

void ResetHaveLocalizedAppDirNameForTesting() {
  g_have_localized_app_dir_name = false;
}

}  // namespace web_app
