// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/web_applications/components/web_app_shortcut_mac.h"

#import <Cocoa/Cocoa.h>
#include <stdint.h>

#include <map>
#include <utility>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/mac/foundation_util.h"
#import "base/mac/launch_services_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/version.h"
#import "chrome/browser/mac/dock.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#import "chrome/common/mac/app_mode_common.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "components/crx_file/id_util.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#import "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/utils/mac/SkCGUtils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_family.h"

bool g_app_shims_allow_update_and_launch_in_tests = false;

namespace {

// Launch Services Key to run as an agent app, which doesn't launch in the dock.
NSString* const kLSUIElement = @"LSUIElement";

// Writes |icons| to |path| in .icns format.
bool WriteIconsToFile(const std::vector<gfx::Image>& icons,
                      const base::FilePath& path) {
  base::scoped_nsobject<NSMutableData> data(
      [[NSMutableData alloc] initWithCapacity:0]);
  base::ScopedCFTypeRef<CGImageDestinationRef> image_destination(
      CGImageDestinationCreateWithData(base::mac::NSToCFCast(data),
                                       kUTTypeAppleICNS, icons.size(),
                                       nullptr));
  DCHECK(image_destination);
  for (const gfx::Image& image : icons) {
    base::ScopedCFTypeRef<CGImageRef> cg_image(SkCreateCGImageRefWithColorspace(
        image.AsBitmap(), base::mac::GetSRGBColorSpace()));
    CGImageDestinationAddImage(image_destination, cg_image, nullptr);
  }
  if (!CGImageDestinationFinalize(image_destination)) {
    NOTREACHED() << "CGImageDestinationFinalize failed.";
    return false;
  }
  return [data writeToFile:base::mac::FilePathToNSString(path) atomically:NO];
}

// Returns true if |image| can be used for an icon resource.
bool IsImageValidForIcon(const gfx::Image& image) {
  if (image.IsEmpty())
    return false;

  // When called via ShowCreateChromeAppShortcutsDialog the ImageFamily will
  // have all the representations desired here for mac, from the kDesiredSizes
  // array in web_app.cc.
  SkBitmap bitmap = image.AsBitmap();
  if (bitmap.colorType() != kN32_SkColorType ||
      bitmap.width() != bitmap.height()) {
    return false;
  }

  switch (bitmap.width()) {
    case 512:
    case 256:
    case 128:
    case 48:
    case 32:
    case 16:
      return true;
  }
  return false;
}

bool AppShimsDisabledForTest() {
  // Disable app shims in tests because shims created in ~/Applications will not
  // be cleaned up.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType);
}

base::FilePath GetWritableApplicationsDirectory() {
  base::FilePath path;
  if (base::mac::GetUserDirectory(NSApplicationDirectory, &path)) {
    if (!base::DirectoryExists(path)) {
      if (!base::CreateDirectory(path))
        return base::FilePath();

      // Create a zero-byte ".localized" file to inherit localizations from OSX
      // for folders that have special meaning.
      base::WriteFile(path.Append(".localized"), NULL, 0);
    }
    return base::PathIsWritable(path) ? path : base::FilePath();
  }
  return base::FilePath();
}

// Given the path to an app bundle, return the resources directory.
base::FilePath GetResourcesPath(const base::FilePath& app_path) {
  return app_path.Append("Contents").Append("Resources");
}

bool HasExistingExtensionShim(const base::FilePath& destination_directory,
                              const std::string& extension_id,
                              const base::FilePath& own_basename) {
  // Check if there any any other shims for the same extension.
  base::FileEnumerator enumerator(destination_directory, false /* recursive */,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath shim_path = enumerator.Next(); !shim_path.empty();
       shim_path = enumerator.Next()) {
    if (shim_path.BaseName() != own_basename &&
        base::EndsWith(shim_path.RemoveExtension().value(), extension_id,
                       base::CompareCase::SENSITIVE)) {
      return true;
    }
  }

  return false;
}

// Given the path to an app bundle, return the path to the Info.plist file.
NSString* GetPlistPath(const base::FilePath& bundle_path) {
  return base::mac::FilePathToNSString(
      bundle_path.Append("Contents").Append("Info.plist"));
}

NSMutableDictionary* ReadPlist(NSString* plist_path) {
  return [NSMutableDictionary dictionaryWithContentsOfFile:plist_path];
}

// Takes the path to an app bundle and checks that the CrAppModeUserDataDir in
// the Info.plist starts with the current user_data_dir. This uses starts with
// instead of equals because the CrAppModeUserDataDir could be the user_data_dir
// or the |app_data_dir_|.
bool HasSameUserDataDir(const base::FilePath& bundle_path) {
  NSDictionary* plist = ReadPlist(GetPlistPath(bundle_path));
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(!user_data_dir.empty());
  return base::StartsWith(base::SysNSStringToUTF8([plist
                              valueForKey:app_mode::kCrAppModeUserDataDirKey]),
                          user_data_dir.value(), base::CompareCase::SENSITIVE);
}

void LaunchShimOnFileThread(bool launched_after_rebuild,
                            const web_app::ShortcutInfo& shortcut_info) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);
  base::FilePath shim_path = web_app::GetAppInstallPath(shortcut_info);

  if (shim_path.empty() || !base::PathExists(shim_path) ||
      !HasSameUserDataDir(shim_path)) {
    // The user may have deleted the copy in the Applications folder, use the
    // one in the web app's |app_data_dir_|.
    base::FilePath app_data_dir = web_app::GetWebAppDataDirectory(
        shortcut_info.profile_path, shortcut_info.extension_id, GURL());
    shim_path = app_data_dir.Append(shim_path.BaseName());
  }

  if (!base::PathExists(shim_path))
    return;

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(app_mode::kLaunchedByChromeProcessId,
                                 base::IntToString(base::GetCurrentProcId()));
  if (launched_after_rebuild)
    command_line.AppendSwitch(app_mode::kLaunchedAfterRebuild);
  // Launch without activating (NSWorkspaceLaunchWithoutActivation).
  base::mac::OpenApplicationWithPath(
      shim_path, command_line,
      NSWorkspaceLaunchDefault | NSWorkspaceLaunchWithoutActivation);
}

base::FilePath GetAppLoaderPath() {
  return base::mac::PathForFrameworkBundleResource(
      base::mac::NSToCFCast(@"app_mode_loader.app"));
}

void UpdatePlatformShortcutsInternal(
    const base::FilePath& app_data_path,
    const base::string16& old_app_title,
    const web_app::ShortcutInfo& shortcut_info) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);
  if (AppShimsDisabledForTest() &&
      !g_app_shims_allow_update_and_launch_in_tests) {
    return;
  }

  web_app::WebAppShortcutCreator shortcut_creator(app_data_path,
                                                  &shortcut_info);
  shortcut_creator.UpdateShortcuts();
}

void UpdateAndLaunchShimOnFileThread(
    const web_app::ShortcutInfo& shortcut_info) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);
  base::FilePath shortcut_data_dir = web_app::GetWebAppDataDirectory(
      shortcut_info.profile_path, shortcut_info.extension_id, GURL());
  UpdatePlatformShortcutsInternal(shortcut_data_dir, base::string16(),
                                  shortcut_info);
  LaunchShimOnFileThread(true, shortcut_info);
}

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

// Creates a canvas the same size as |overlay|, copies the appropriate
// representation from |backgound| into it (according to Cocoa), then draws
// |overlay| over it using NSCompositeSourceOver.
NSImageRep* OverlayImageRep(NSImage* background, NSImageRep* overlay) {
  DCHECK(background);
  NSInteger dimension = [overlay pixelsWide];
  DCHECK_EQ(dimension, [overlay pixelsHigh]);
  base::scoped_nsobject<NSBitmapImageRep> canvas([[NSBitmapImageRep alloc]
      initWithBitmapDataPlanes:NULL
                    pixelsWide:dimension
                    pixelsHigh:dimension
                 bitsPerSample:8
               samplesPerPixel:4
                      hasAlpha:YES
                      isPlanar:NO
                colorSpaceName:NSCalibratedRGBColorSpace
                   bytesPerRow:0
                  bitsPerPixel:0]);

  // There isn't a colorspace name constant for sRGB, so retag.
  NSBitmapImageRep* srgb_canvas = [canvas
      bitmapImageRepByRetaggingWithColorSpace:[NSColorSpace sRGBColorSpace]];
  canvas.reset([srgb_canvas retain]);

  // Communicate the DIP scale (1.0). TODO(tapted): Investigate HiDPI.
  [canvas setSize:NSMakeSize(dimension, dimension)];

  NSGraphicsContext* drawing_context =
      [NSGraphicsContext graphicsContextWithBitmapImageRep:canvas];
  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext:drawing_context];
  [background drawInRect:NSMakeRect(0, 0, dimension, dimension)
                fromRect:NSZeroRect
               operation:NSCompositeCopy
                fraction:1.0];
  [overlay drawInRect:NSMakeRect(0, 0, dimension, dimension)
             fromRect:NSZeroRect
            operation:NSCompositeSourceOver
             fraction:1.0
       respectFlipped:NO
                hints:0];
  [NSGraphicsContext restoreGraphicsState];
  return canvas.autorelease();
}

// Helper function to extract the single NSImageRep held in a resource bundle
// image.
base::scoped_nsobject<NSImageRep> ImageRepForGFXImage(const gfx::Image& image) {
  NSArray* image_reps = [image.AsNSImage() representations];
  DCHECK_EQ(1u, [image_reps count]);
  return base::scoped_nsobject<NSImageRep>([image_reps objectAtIndex:0],
                                           base::scoped_policy::RETAIN);
}

using ResourceIDToImage = std::map<int, base::scoped_nsobject<NSImageRep>>;

// Generates a map of NSImageReps used by SetWorkspaceIconOnFILEThread and
// passes it to |io_task|. Since ui::ResourceBundle can only be used on UI
// thread, this function also needs to run on UI thread, and the gfx::Images
// need to be converted to NSImageReps on the UI thread due to non-thread-safety
// of gfx::Image.
void GetImageResourcesOnUIThread(
    base::OnceCallback<void(std::unique_ptr<ResourceIDToImage>)> io_task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();
  std::unique_ptr<ResourceIDToImage> result =
      std::make_unique<ResourceIDToImage>();

  // These resource ID should match to the ones used by
  // SetWorkspaceIconOnFILEThread below.
  for (int id : {IDR_APPS_FOLDER_16, IDR_APPS_FOLDER_32,
                 IDR_APPS_FOLDER_OVERLAY_128, IDR_APPS_FOLDER_OVERLAY_512}) {
    gfx::Image image = resource_bundle.GetNativeImageNamed(id);
    (*result)[id] = ImageRepForGFXImage(image);
  }

  base::PostTaskWithTraits(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(std::move(io_task), std::move(result)));
}

void SetWorkspaceIconOnWorkerThread(const base::FilePath& apps_directory,
                                    std::unique_ptr<ResourceIDToImage> images) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);

  base::scoped_nsobject<NSImage> folder_icon_image([[NSImage alloc] init]);
  // Use complete assets for the small icon sizes. -[NSWorkspace setIcon:] has a
  // bug when dealing with named NSImages where it incorrectly handles alpha
  // premultiplication. This is most noticable with small assets since the 1px
  // border is a much larger component of the small icons.
  // See http://crbug.com/305373 for details.
  for (int id : {IDR_APPS_FOLDER_16, IDR_APPS_FOLDER_32}) {
    const auto& found = images->find(id);
    DCHECK(found != images->end());
    [folder_icon_image addRepresentation:found->second];
  }

  // Brand larger folder assets with an embossed app launcher logo to
  // conserve distro size and for better consistency with changing hue
  // across OSX versions. The folder is textured, so compresses poorly
  // without this.
  NSImage* base_image = [NSImage imageNamed:NSImageNameFolder];
  for (int id : {IDR_APPS_FOLDER_OVERLAY_128, IDR_APPS_FOLDER_OVERLAY_512}) {
    const auto& found = images->find(id);
    DCHECK(found != images->end());
    NSImageRep* with_overlay = OverlayImageRep(base_image, found->second);
    DCHECK(with_overlay);
    if (with_overlay)
      [folder_icon_image addRepresentation:with_overlay];
  }
  [[NSWorkspace sharedWorkspace]
      setIcon:folder_icon_image
      forFile:base::mac::FilePathToNSString(apps_directory)
      options:0];
}

// Adds a localized strings file for the Chrome Apps directory using the current
// locale. OSX will use this for the display name.
// + Chrome Apps.localized (|apps_directory|)
// | + .localized
// | | en.strings
// | | de.strings
bool UpdateAppShortcutsSubdirLocalizedName(
    const base::FilePath& apps_directory) {
  base::FilePath localized = apps_directory.Append(".localized");
  if (!base::CreateDirectory(localized))
    return false;

  base::FilePath directory_name = apps_directory.BaseName().RemoveExtension();
  base::string16 localized_name =
      shell_integration::GetAppShortcutsSubdirName();
  NSDictionary* strings_dict = @{
    base::mac::FilePathToNSString(directory_name) :
        base::SysUTF16ToNSString(localized_name)
  };

  std::string locale = l10n_util::NormalizeLocale(
      l10n_util::GetApplicationLocale(std::string()));

  NSString* strings_path =
      base::mac::FilePathToNSString(localized.Append(locale + ".strings"));
  [strings_dict writeToFile:strings_path atomically:YES];

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &GetImageResourcesOnUIThread,
          base::BindOnce(&SetWorkspaceIconOnWorkerThread, apps_directory)));
  return true;
}

void DeletePathAndParentIfEmpty(const base::FilePath& app_path) {
  DCHECK(!app_path.empty());
  base::DeleteFile(app_path, true);
  base::FilePath apps_folder = app_path.DirName();
  if (base::IsDirectoryEmpty(apps_folder))
    base::DeleteFile(apps_folder, false);
}

bool IsShimForProfile(const base::FilePath& base_name,
                      const std::string& profile_base_name) {
  if (!base::StartsWith(base_name.value(), profile_base_name,
                        base::CompareCase::SENSITIVE))
    return false;

  if (base_name.Extension() != ".app")
    return false;

  std::string app_id = base_name.RemoveExtension().value();
  // Strip (profile_base_name + " ") from the start.
  app_id = app_id.substr(profile_base_name.size() + 1);
  return crx_file::id_util::IdIsValid(app_id);
}

std::vector<base::FilePath> GetAllAppBundlesInPath(
    const base::FilePath& internal_shortcut_path,
    const std::string& profile_base_name) {
  std::vector<base::FilePath> bundle_paths;

  base::FileEnumerator enumerator(internal_shortcut_path, true /* recursive */,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath bundle_path = enumerator.Next(); !bundle_path.empty();
       bundle_path = enumerator.Next()) {
    if (IsShimForProfile(bundle_path.BaseName(), profile_base_name))
      bundle_paths.push_back(bundle_path);
  }

  return bundle_paths;
}

std::unique_ptr<web_app::ShortcutInfo> BuildShortcutInfoFromBundle(
    const base::FilePath& bundle_path) {
  NSDictionary* plist = ReadPlist(GetPlistPath(bundle_path));

  std::unique_ptr<web_app::ShortcutInfo> shortcut_info(
      new web_app::ShortcutInfo);
  shortcut_info->extension_id = base::SysNSStringToUTF8(
      [plist valueForKey:app_mode::kCrAppModeShortcutIDKey]);
  shortcut_info->is_platform_app = true;
  shortcut_info->url = GURL(base::SysNSStringToUTF8(
      [plist valueForKey:app_mode::kCrAppModeShortcutURLKey]));
  shortcut_info->title = base::SysNSStringToUTF16(
      [plist valueForKey:app_mode::kCrAppModeShortcutNameKey]);
  shortcut_info->profile_name = base::SysNSStringToUTF8(
      [plist valueForKey:app_mode::kCrAppModeProfileNameKey]);

  // Figure out the profile_path. Since the user_data_dir could contain the
  // path to the web app data dir.
  base::FilePath user_data_dir = base::mac::NSStringToFilePath(
      [plist valueForKey:app_mode::kCrAppModeUserDataDirKey]);
  base::FilePath profile_base_name = base::mac::NSStringToFilePath(
      [plist valueForKey:app_mode::kCrAppModeProfileDirKey]);
  if (user_data_dir.DirName().DirName().BaseName() == profile_base_name)
    shortcut_info->profile_path = user_data_dir.DirName().DirName();
  else
    shortcut_info->profile_path = user_data_dir.Append(profile_base_name);

  return shortcut_info;
}

}  // namespace

namespace web_app {

void UpdateAndLaunchShim(std::unique_ptr<web_app::ShortcutInfo> shortcut_info) {
  web_app::internals::PostShortcutIOTask(
      base::BindOnce(&UpdateAndLaunchShimOnFileThread),
      std::move(shortcut_info));
}

std::unique_ptr<web_app::ShortcutInfo> RecordAppShimErrorAndBuildShortcutInfo(
    const base::FilePath& bundle_path) {
  NSDictionary* plist = ReadPlist(GetPlistPath(bundle_path));
  NSString* version_string = [plist valueForKey:app_mode::kCrBundleVersionKey];
  if (!version_string) {
    // Older bundles have the Chrome version in the following key.
    version_string =
        [plist valueForKey:app_mode::kCFBundleShortVersionStringKey];
  }
  base::Version full_version(base::SysNSStringToUTF8(version_string));
  uint32_t major_version = 0;
  if (full_version.IsValid())
    major_version = full_version.components()[0];
  base::UmaHistogramSparse("Apps.AppShimErrorVersion", major_version);

  return BuildShortcutInfoFromBundle(bundle_path);
}

WebAppShortcutCreator::WebAppShortcutCreator(const base::FilePath& app_data_dir,
                                             const ShortcutInfo* shortcut_info)
    : app_data_dir_(app_data_dir), info_(shortcut_info) {
  DCHECK(shortcut_info);
}

WebAppShortcutCreator::~WebAppShortcutCreator() {}

base::FilePath WebAppShortcutCreator::GetApplicationsShortcutPath() const {
  base::FilePath applications_dir = GetApplicationsDirname();
  return applications_dir.empty()
             ? base::FilePath()
             : applications_dir.Append(GetShortcutBasename());
}

base::FilePath WebAppShortcutCreator::GetInternalShortcutPath() const {
  return app_data_dir_.Append(GetShortcutBasename());
}

base::FilePath WebAppShortcutCreator::GetShortcutBasename() const {
  std::string app_name;
  // Check if there should be a separate shortcut made for different profiles.
  // Such shortcuts will have a |profile_name| set on the ShortcutInfo,
  // otherwise it will be empty.
  if (!info_->profile_name.empty()) {
    app_name += info_->profile_path.BaseName().value();
    app_name += ' ';
  }
  app_name += info_->extension_id;
  return base::FilePath(app_name).ReplaceExtension("app");
}

bool WebAppShortcutCreator::BuildShortcut(
    const base::FilePath& staging_path) const {
  // Update the app's plist and icon in a temp directory. This works around
  // a Finder bug where the app's icon doesn't properly update.
  if (!base::CopyDirectory(GetAppLoaderPath(), staging_path, true)) {
    LOG(ERROR) << "Copying app to staging path: " << staging_path.value()
               << " failed.";
    return false;
  }

  return UpdatePlist(staging_path) && UpdateDisplayName(staging_path) &&
         UpdateIcon(staging_path);
}

size_t WebAppShortcutCreator::CreateShortcutsIn(
    const std::vector<base::FilePath>& folders) const {
  size_t succeeded = 0;

  base::ScopedTempDir scoped_temp_dir;
  if (!scoped_temp_dir.CreateUniqueTempDir())
    return 0;

  base::FilePath app_name = GetShortcutBasename();
  base::FilePath staging_path = scoped_temp_dir.GetPath().Append(app_name);
  if (!BuildShortcut(staging_path))
    return 0;

  for (std::vector<base::FilePath>::const_iterator it = folders.begin();
       it != folders.end(); ++it) {
    const base::FilePath& dst_path = *it;
    if (!base::CreateDirectory(dst_path)) {
      LOG(ERROR) << "Creating directory " << dst_path.value() << " failed.";
      return succeeded;
    }

    // Ensure the copy does not merge with stale info.
    base::DeleteFile(dst_path.Append(app_name), true);

    if (!base::CopyDirectory(staging_path, dst_path, true)) {
      LOG(ERROR) << "Copying app to dst path: " << dst_path.value()
                 << " failed";
      return succeeded;
    }

    // Remove the quarantine attribute from both the bundle and the executable.
    base::mac::RemoveQuarantineAttribute(dst_path.Append(app_name));
    base::mac::RemoveQuarantineAttribute(dst_path.Append(app_name)
                                             .Append("Contents")
                                             .Append("MacOS")
                                             .Append("app_mode_loader"));
    ++succeeded;
  }

  return succeeded;
}

bool WebAppShortcutCreator::CreateShortcuts(
    ShortcutCreationReason creation_reason,
    ShortcutLocations creation_locations) {
  const base::FilePath applications_dir = GetApplicationsDirname();
  if (applications_dir.empty() ||
      !base::DirectoryExists(applications_dir.DirName())) {
    LOG(ERROR) << "Couldn't find an Applications directory to copy app to.";
    return false;
  }

  // Only set folder icons and a localized name once. This avoids concurrent
  // calls to -[NSWorkspace setIcon:..], which is not reentrant.
  static bool once = UpdateAppShortcutsSubdirLocalizedName(applications_dir);
  if (!once)
    LOG(ERROR) << "Failed to localize " << applications_dir.value();

  // If non-nil, this path is added to the OSX Dock after creating shortcuts.
  NSString* path_to_add_to_dock = nil;

  std::vector<base::FilePath> paths;

  // The app list shim is not tied to a particular profile, so omit the copy
  // placed under the profile path. For shims, this copy is used when the
  // version under Applications is removed, and not needed for app list because
  // setting LSUIElement means there is no Dock "running" status to show.
  const bool is_app_list = info_->extension_id == app_mode::kAppListModeId;
  if (is_app_list) {
    path_to_add_to_dock = base::SysUTF8ToNSString(
        applications_dir.Append(GetShortcutBasename()).AsUTF8Unsafe());
  } else {
    paths.push_back(app_data_dir_);
  }

  bool shortcut_visible =
      creation_locations.applications_menu_location != APP_MENU_LOCATION_HIDDEN;
  if (shortcut_visible)
    paths.push_back(applications_dir);

  DCHECK(!paths.empty());
  size_t success_count = CreateShortcutsIn(paths);
  if (success_count == 0)
    return false;

  if (!is_app_list)
    UpdateInternalBundleIdentifier();

  if (success_count != paths.size())
    return false;

  if (creation_locations.in_quick_launch_bar && path_to_add_to_dock &&
      shortcut_visible) {
    switch (dock::AddIcon(path_to_add_to_dock, nil)) {
      case dock::IconAddFailure:
        // If adding the icon failed, instead reveal the Finder window.
        RevealAppShimInFinder();
        break;
      case dock::IconAddSuccess:
      case dock::IconAlreadyPresent:
        break;
    }
    return true;
  }

  if (creation_reason == SHORTCUT_CREATION_BY_USER)
    RevealAppShimInFinder();

  return true;
}

void WebAppShortcutCreator::DeleteShortcuts() {
  base::FilePath app_path = GetApplicationsShortcutPath();
  if (!app_path.empty() && HasSameUserDataDir(app_path))
    DeletePathAndParentIfEmpty(app_path);

  // In case the user has moved/renamed/copied the app bundle.
  base::FilePath bundle_path = GetAppBundleById(GetBundleIdentifier());
  if (!bundle_path.empty() && HasSameUserDataDir(bundle_path))
    base::DeleteFile(bundle_path, true);

  // Delete the internal one.
  DeletePathAndParentIfEmpty(GetInternalShortcutPath());
}

bool WebAppShortcutCreator::UpdateShortcuts() {
  std::vector<base::FilePath> paths;
  paths.push_back(app_data_dir_);

  // Remember whether the copy in the profile directory exists. If it doesn't
  // and others do, it should be re-created. Otherwise, it's a signal that a
  // shortcut has never been created.
  bool profile_copy_exists = base::PathExists(GetInternalShortcutPath());

  // Try to update the copy under ~/Applications. If that does not exist, check
  // if a matching bundle can be found elsewhere.
  base::FilePath app_path = GetApplicationsShortcutPath();

  // Never look in ~/Applications or search the system for a bundle ID in a test
  // since that relies on global system state and potentially cruft that may be
  // leftover from prior/crashed test runs.
  // TODO(tapted): Remove this check when tests that arrive here via setting
  // |g_app_shims_allow_update_and_launch_in_tests| can properly mock out all
  // the calls below.
  if (!g_app_shims_allow_update_and_launch_in_tests) {
    if (app_path.empty() || !base::PathExists(app_path))
      app_path = GetAppBundleById(GetBundleIdentifier());

    if (app_path.empty()) {
      if (profile_copy_exists && info_->from_bookmark) {
        // The bookmark app shortcut has been deleted by the user. Restore it,
        // as the Mac UI for bookmark apps creates the expectation that the app
        // will be added to Applications.
        app_path = GetApplicationsDirname();
        paths.push_back(app_path);
      }
    } else {
      paths.push_back(app_path.DirName());
    }
  } else {
    // If a test has set g_app_shims_allow_update_and_launch_in_tests, it means
    // it relies on UpdateShortcuts() to create shortcuts. (Tests can't rely on
    // install-triggered shortcut creation because they can't synchronize with
    // the UI thread). So, allow shortcuts to be created for this case, even if
    // none currently exist. TODO(tapted): Remove this when tests are properly
    // mocked.
    profile_copy_exists = true;
  }

  // When upgrading, if no shim exists anywhere on disk, don't create the copy
  // under the profile directory for the first time. The best way to tell
  // whether a shortcut has been created is to poke around on disk, so the
  // upgrade process must send all candidate extensions to the FILE thread.
  // Then those without shortcuts will get culled here.
  if (paths.size() == 1 && !profile_copy_exists)
    return false;

  size_t success_count = CreateShortcutsIn(paths);
  if (success_count == 0)
    return false;

  UpdateInternalBundleIdentifier();
  return success_count == paths.size() && !app_path.empty();
}

base::FilePath WebAppShortcutCreator::GetApplicationsDirname() const {
  base::FilePath path = GetWritableApplicationsDirectory();
  if (path.empty())
    return path;

  return path.Append(GetLocalizableAppShortcutsSubdirName());
}

bool WebAppShortcutCreator::UpdatePlist(const base::FilePath& app_path) const {
  NSString* extension_id = base::SysUTF8ToNSString(info_->extension_id);
  NSString* extension_title = base::SysUTF16ToNSString(info_->title);
  NSString* extension_url = base::SysUTF8ToNSString(info_->url.spec());
  NSString* chrome_bundle_id =
      base::SysUTF8ToNSString(base::mac::BaseBundleID());
  NSDictionary* replacement_dict = [NSDictionary
      dictionaryWithObjectsAndKeys:
          extension_id, app_mode::kShortcutIdPlaceholder, extension_title,
          app_mode::kShortcutNamePlaceholder, extension_url,
          app_mode::kShortcutURLPlaceholder, chrome_bundle_id,
          app_mode::kShortcutBrowserBundleIDPlaceholder, nil];

  NSString* plist_path = GetPlistPath(app_path);
  NSMutableDictionary* plist = ReadPlist(plist_path);
  NSArray* keys = [plist allKeys];

  // 1. Fill in variables.
  for (id key in keys) {
    NSString* value = [plist valueForKey:key];
    if (![value isKindOfClass:[NSString class]] || [value length] < 2)
      continue;

    // Remove leading and trailing '@'s.
    NSString* variable =
        [value substringWithRange:NSMakeRange(1, [value length] - 2)];

    NSString* substitution = [replacement_dict valueForKey:variable];
    if (substitution)
      [plist setObject:substitution forKey:key];
  }

  // 2. Fill in other values.
  [plist setObject:base::SysUTF8ToNSString(version_info::GetVersionNumber())
            forKey:app_mode::kCrBundleVersionKey];
  [plist setObject:base::SysUTF8ToNSString(info_->version_for_display)
            forKey:app_mode::kCFBundleShortVersionStringKey];
  [plist setObject:base::SysUTF8ToNSString(GetBundleIdentifier())
            forKey:base::mac::CFToNSCast(kCFBundleIdentifierKey)];
  [plist setObject:base::mac::FilePathToNSString(app_data_dir_)
            forKey:app_mode::kCrAppModeUserDataDirKey];
  [plist setObject:base::mac::FilePathToNSString(info_->profile_path.BaseName())
            forKey:app_mode::kCrAppModeProfileDirKey];
  [plist setObject:base::SysUTF8ToNSString(info_->profile_name)
            forKey:app_mode::kCrAppModeProfileNameKey];
  [plist setObject:[NSNumber numberWithBool:YES]
            forKey:app_mode::kLSHasLocalizedDisplayNameKey];
  [plist setObject:[NSNumber numberWithBool:YES]
            forKey:app_mode::kNSHighResolutionCapableKey];
  if (info_->extension_id == app_mode::kAppListModeId) {
    // Prevent the app list from bouncing in the dock, and getting a run light.
    [plist setObject:[NSNumber numberWithBool:YES] forKey:kLSUIElement];
  }

  base::FilePath app_name = app_path.BaseName().RemoveExtension();
  [plist setObject:base::mac::FilePathToNSString(app_name)
            forKey:base::mac::CFToNSCast(kCFBundleNameKey)];

  return [plist writeToFile:plist_path atomically:YES];
}

bool WebAppShortcutCreator::UpdateDisplayName(
    const base::FilePath& app_path) const {
  // Localization is used to display the app name (rather than the bundle
  // filename). OSX searches for the best language in the order of preferred
  // languages, but one of them must be found otherwise it will default to
  // the filename.
  NSString* language = [[NSLocale preferredLanguages] objectAtIndex:0];
  base::FilePath localized_dir = GetResourcesPath(app_path).Append(
      base::SysNSStringToUTF8(language) + ".lproj");
  if (!base::CreateDirectory(localized_dir))
    return false;

  NSString* bundle_name = base::SysUTF16ToNSString(info_->title);
  NSString* display_name = base::SysUTF16ToNSString(info_->title);
  if (HasExistingExtensionShim(GetApplicationsDirname(), info_->extension_id,
                               app_path.BaseName())) {
    display_name = [bundle_name
        stringByAppendingString:base::SysUTF8ToNSString(
                                    " (" + info_->profile_name + ")")];
  }

  NSDictionary* strings_plist = @{
    base::mac::CFToNSCast(kCFBundleNameKey) : bundle_name,
    app_mode::kCFBundleDisplayNameKey : display_name
  };

  NSString* localized_path =
      base::mac::FilePathToNSString(localized_dir.Append("InfoPlist.strings"));
  return [strings_plist writeToFile:localized_path atomically:YES];
}

bool WebAppShortcutCreator::UpdateIcon(const base::FilePath& app_path) const {
  if (info_->favicon.empty())
    return true;

  std::vector<gfx::Image> valid_icons;
  for (gfx::ImageFamily::const_iterator it = info_->favicon.begin();
       it != info_->favicon.end(); ++it) {
    if (IsImageValidForIcon(*it))
      valid_icons.push_back(*it);
  }
  if (valid_icons.empty())
    return false;

  base::FilePath resources_path = GetResourcesPath(app_path);
  if (!base::CreateDirectory(resources_path))
    return false;

  return WriteIconsToFile(valid_icons, resources_path.Append("app.icns"));
}

bool WebAppShortcutCreator::UpdateInternalBundleIdentifier() const {
  NSString* plist_path = GetPlistPath(GetInternalShortcutPath());
  NSMutableDictionary* plist = ReadPlist(plist_path);

  [plist setObject:base::SysUTF8ToNSString(GetInternalBundleIdentifier())
            forKey:base::mac::CFToNSCast(kCFBundleIdentifierKey)];
  return [plist writeToFile:plist_path atomically:YES];
}

base::FilePath WebAppShortcutCreator::GetAppBundleById(
    const std::string& bundle_id) const {
  base::ScopedCFTypeRef<CFStringRef> bundle_id_cf(
      base::SysUTF8ToCFStringRef(bundle_id));
  CFURLRef url_ref = NULL;
  OSStatus status = LSFindApplicationForInfo(
      kLSUnknownCreator, bundle_id_cf.get(), NULL, NULL, &url_ref);
  if (status != noErr)
    return base::FilePath();

  base::ScopedCFTypeRef<CFURLRef> url(url_ref);
  NSString* path_string = [base::mac::CFToNSCast(url.get()) path];
  return base::FilePath([path_string fileSystemRepresentation]);
}

std::string WebAppShortcutCreator::GetBundleIdentifier() const {
  // Replace spaces in the profile path with hyphen.
  std::string normalized_profile_path;
  base::ReplaceChars(info_->profile_path.BaseName().value(), " ", "-",
                     &normalized_profile_path);

  // This matches APP_MODE_APP_BUNDLE_ID in chrome/chrome.gyp.
  std::string bundle_id = base::mac::BaseBundleID() + std::string(".app.") +
                          normalized_profile_path + "-" + info_->extension_id;

  return bundle_id;
}

std::string WebAppShortcutCreator::GetInternalBundleIdentifier() const {
  return GetBundleIdentifier() + "-internal";
}

void WebAppShortcutCreator::RevealAppShimInFinder() const {
  base::FilePath app_path = GetApplicationsShortcutPath();
  if (app_path.empty())
    return;

  // Check if the app shim exists.
  if (base::PathExists(app_path)) {
    // Use selectFile to show the contents of parent directory with the app
    // shim selected.
    [[NSWorkspace sharedWorkspace]
                      selectFile:base::mac::FilePathToNSString(app_path)
        inFileViewerRootedAtPath:@""];
    return;
  }

  // Otherwise, go up a directory.
  app_path = app_path.DirName();
  // Check if the Chrome apps folder exists, otherwise go up to ~/Applications.
  if (!base::PathExists(app_path))
    app_path = app_path.DirName();
  // Since |app_path| is a directory, use openFile to show the contents of
  // that directory in Finder.
  [[NSWorkspace sharedWorkspace]
      openFile:base::mac::FilePathToNSString(app_path)];
}

base::FilePath GetAppInstallPath(const ShortcutInfo& shortcut_info) {
  WebAppShortcutCreator shortcut_creator(base::FilePath(), &shortcut_info);
  return shortcut_creator.GetApplicationsShortcutPath();
}

void MaybeLaunchShortcut(std::unique_ptr<ShortcutInfo> shortcut_info) {
  if (AppShimsDisabledForTest() &&
      !g_app_shims_allow_update_and_launch_in_tests) {
    return;
  }

  web_app::internals::PostShortcutIOTask(
      base::BindOnce(&LaunchShimOnFileThread, false), std::move(shortcut_info));
}

namespace internals {

bool CreatePlatformShortcuts(const base::FilePath& app_data_path,
                             const ShortcutLocations& creation_locations,
                             ShortcutCreationReason creation_reason,
                             const ShortcutInfo& shortcut_info) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);
  if (AppShimsDisabledForTest())
    return true;

  WebAppShortcutCreator shortcut_creator(app_data_path, &shortcut_info);
  return shortcut_creator.CreateShortcuts(creation_reason, creation_locations);
}

void DeletePlatformShortcuts(const base::FilePath& app_data_path,
                             const ShortcutInfo& shortcut_info) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);
  WebAppShortcutCreator shortcut_creator(app_data_path, &shortcut_info);
  shortcut_creator.DeleteShortcuts();
}

void UpdatePlatformShortcuts(const base::FilePath& app_data_path,
                             const base::string16& old_app_title,
                             const ShortcutInfo& shortcut_info) {
  UpdatePlatformShortcutsInternal(app_data_path, old_app_title, shortcut_info);
}

void DeleteAllShortcutsForProfile(const base::FilePath& profile_path) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);
  const std::string profile_base_name = profile_path.BaseName().value();
  std::vector<base::FilePath> bundles = GetAllAppBundlesInPath(
      profile_path.Append(chrome::kWebAppDirname), profile_base_name);

  for (std::vector<base::FilePath>::const_iterator it = bundles.begin();
       it != bundles.end(); ++it) {
    std::unique_ptr<web_app::ShortcutInfo> shortcut_info =
        BuildShortcutInfoFromBundle(*it);
    WebAppShortcutCreator shortcut_creator(it->DirName(), shortcut_info.get());
    shortcut_creator.DeleteShortcuts();
  }
}

}  // namespace internals

}  // namespace web_app
