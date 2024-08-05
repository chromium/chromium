// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/web_applications/os_integration/web_app_shortcut_win.h"

#include <shlobj.h>
#include <stddef.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/md5.h"
#include "base/i18n/file_util_icu.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/shortcut.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/shell_integration_win.h"
#include "chrome/browser/shortcuts/platform_util_win.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcuts_menu_win.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/taskbar_util.h"
#include "chrome/installer/util/util_constants.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/win/shell.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_family.h"

namespace web_app {
namespace {

constexpr base::FilePath::CharType kIconChecksumFileExt[] =
    FILE_PATH_LITERAL(".ico.md5");

}  // namespace

namespace internals {
namespace {

// Calculates checksum of an icon family using MD5.
// The checksum is derived from all of the icons in the family.
void GetImageCheckSum(const gfx::ImageFamily& image, base::MD5Digest* digest) {
  DCHECK(digest);
  base::MD5Context md5_context;
  base::MD5Init(&md5_context);

  for (gfx::ImageFamily::const_iterator it = image.begin(); it != image.end();
       ++it) {
    SkBitmap bitmap = it->AsBitmap();

    std::string_view image_data(
        reinterpret_cast<const char*>(bitmap.getPixels()),
        bitmap.computeByteSize());
    base::MD5Update(&md5_context, image_data);
  }

  base::MD5Final(digest, &md5_context);
}

// Saves |image| as an |icon_file| with the checksum.
bool SaveIconWithCheckSum(const base::FilePath& icon_file,
                          const gfx::ImageFamily& image) {
  if (!IconUtil::CreateIconFileFromImageFamily(image, icon_file))
    return false;

  base::MD5Digest digest;
  GetImageCheckSum(image, &digest);

  base::FilePath cheksum_file(icon_file.ReplaceExtension(kIconChecksumFileExt));
  // Passing digest as one element in a span of digest fields, therefore the 1u,
  // and then having as_bytes converting it to a new span of uint8_t's.
  return base::WriteFile(cheksum_file, base::byte_span_from_ref(digest));
}

// Returns true if |icon_file| is missing or different from |image|.
bool ShouldUpdateIcon(const base::FilePath& icon_file,
                      const gfx::ImageFamily& image) {
  base::FilePath checksum_file(
      icon_file.ReplaceExtension(kIconChecksumFileExt));

  // Returns true if icon_file or checksum file is missing.
  if (!base::PathExists(icon_file) || !base::PathExists(checksum_file))
    return true;

  base::MD5Digest persisted_image_checksum;
  if (sizeof(persisted_image_checksum) !=
      base::ReadFile(checksum_file,
                     reinterpret_cast<char*>(&persisted_image_checksum),
                     sizeof(persisted_image_checksum)))
    return true;

  base::MD5Digest downloaded_image_checksum;
  GetImageCheckSum(image, &downloaded_image_checksum);

  // Update icon if checksums are not equal.
  return memcmp(&persisted_image_checksum, &downloaded_image_checksum,
                sizeof(base::MD5Digest)) != 0;
}

// Returns true if |shortcut_file_name| matches profile |profile_path|, and has
// an --app-id flag.
bool IsAppShortcutForProfile(const base::FilePath& shortcut_file_name,
                             const base::FilePath& profile_path) {
  std::wstring cmd_line_string;
  if (base::win::ResolveShortcut(shortcut_file_name, nullptr,
                                 &cmd_line_string)) {
    cmd_line_string = L"program " + cmd_line_string;
    base::CommandLine shortcut_cmd_line =
        base::CommandLine::FromString(cmd_line_string);
    return shortcut_cmd_line.HasSwitch(switches::kProfileDirectory) &&
           shortcut_cmd_line.GetSwitchValuePath(switches::kProfileDirectory) ==
               profile_path.BaseName() &&
           shortcut_cmd_line.HasSwitch(switches::kAppId);
  }

  return false;
}

// Creates application shortcuts in a given set of paths.
// |shortcut_paths| is a list of directories in which shortcuts should be
// created. If |creation_reason| is SHORTCUT_CREATION_AUTOMATED and there is an
// existing shortcut to this app for this profile, does nothing (succeeding).
// Returns true on success, false on failure.
// Must be called on a task runner that allows blocking.
bool CreateShortcutsInPaths(const base::FilePath& web_app_path,
                            const ShortcutInfo& shortcut_info,
                            const std::vector<base::FilePath>& shortcut_paths,
                            ShortcutCreationReason creation_reason,
                            const std::string& run_on_os_login_mode) {
  // Generates file name to use with persisted ico and shortcut file.
  base::FilePath icon_file = GetIconFilePath(web_app_path, shortcut_info.title);
  if (!CheckAndSaveIcon(icon_file, shortcut_info.favicon, false)) {
    return false;
  }

  base::FilePath chrome_proxy_path = shortcuts::GetChromeProxyPath();

  // Working directory.
  base::FilePath working_dir(chrome_proxy_path.DirName());

  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  cmd_line = shell_integration::CommandLineArgsForLauncher(
      shortcut_info.url, shortcut_info.app_id, shortcut_info.profile_path,
      run_on_os_login_mode);

  // TODO(evan): we rely on the fact that command_line_string() is
  // properly quoted for a Windows command line.  The method on
  // base::CommandLine should probably be renamed to better reflect that
  // fact.
  std::wstring wide_switches(cmd_line.GetCommandLineString());

  // Sanitize description.
  std::u16string description = shortcut_info.description;
  if (description.length() >= MAX_PATH)
    description.resize(MAX_PATH - 1);

  // Generates app id from the browser's appid, and the app's app_id or
  // web app url, and the profile path.
  std::string app_name(GenerateApplicationNameFromInfo(shortcut_info));
  std::wstring win_app_id(shell_integration::win::GetAppUserModelIdForApp(
      base::UTF8ToWide(app_name), shortcut_info.profile_path));

  bool success = true;
  for (const auto& shortcut_path : shortcut_paths) {
    base::FilePath shortcut_file =
        shortcut_path.Append(GetSanitizedFileName(shortcut_info.title))
            .AddExtension(installer::kLnkExt);
    if (creation_reason == SHORTCUT_CREATION_AUTOMATED) {
      // Check whether there is an existing shortcut to this app.
      std::vector<base::FilePath> shortcut_files =
          FindAppShortcutsByProfileAndTitle(
              shortcut_path, shortcut_info.profile_path, shortcut_info.title);
      if (!shortcut_files.empty())
        continue;
    }
    if (shortcut_path != web_app_path) {
      shortcut_file = base::GetUniquePath(shortcut_file);
      if (shortcut_file.empty()) {
        success = false;
        continue;
      }
    }
    base::win::ShortcutProperties shortcut_properties;
    // Target a proxy executable instead of Chrome directly to ensure start menu
    // pinning uses the correct icon. See https://crbug.com/732357 for details.
    shortcut_properties.set_target(chrome_proxy_path);
    shortcut_properties.set_working_dir(working_dir);
    shortcut_properties.set_arguments(wide_switches);
    shortcut_properties.set_description(base::AsWString(description));
    shortcut_properties.set_icon(icon_file, 0);
    shortcut_properties.set_app_id(win_app_id);
    shortcut_properties.set_dual_mode(false);
    if (!base::PathExists(shortcut_file.DirName()) &&
        !base::CreateDirectory(shortcut_file.DirName())) {
      success = false;
      break;
    }
    success = base::win::CreateOrUpdateShortcutLink(
                  shortcut_file, shortcut_properties,
                  base::win::ShortcutOperation::kCreateAlways) &&
              success;
  }

  return success;
}

void DeleteShortcuts(std::vector<base::FilePath> all_shortcuts,
                     DeleteShortcutsCallback result_callback) {
  bool result = true;
  for (const auto& shortcut : all_shortcuts) {
    if (!base::DeleteFile(shortcut))
      result = false;
    SHChangeNotify(SHCNE_DELETE, SHCNF_PATH, shortcut.value().c_str(), nullptr);
  }
  std::move(result_callback).Run(result);
}

// Returns a vector of shortcuts that match |app_title| and the profile name
// specified in |profile_path|.
// If |web_app_path| is not empty, it will also search in the web app install
// dir.
std::vector<base::FilePath> FindMatchingShortcuts(
    const base::FilePath& web_app_path,
    const base::FilePath& profile_path,
    const std::u16string& app_title) {
  // Get all possible locations for shortcuts.
  ShortcutLocations all_shortcut_locations;
  all_shortcut_locations.in_quick_launch_bar = true;
  all_shortcut_locations.on_desktop = true;
  // Rename shortcuts from the Chrome Apps subdirectory.
  // This matches the subdir name set by CreateApplicationShortcutView::Accept
  // for Chrome apps (not URL apps, but this function does not apply for them).
  all_shortcut_locations.applications_menu_location =
      APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;
  std::vector<base::FilePath> all_paths =
      GetShortcutPaths(all_shortcut_locations);
  if (!web_app_path.empty())
    all_paths.push_back(web_app_path);

  std::vector<base::FilePath> matching_shortcuts;
  for (const auto& path : all_paths) {
    std::vector<base::FilePath> shortcut_files =
        FindAppShortcutsByProfileAndTitle(path, profile_path, app_title);
    matching_shortcuts.insert(matching_shortcuts.end(), shortcut_files.begin(),
                              shortcut_files.end());
  }
  return matching_shortcuts;
}

void UpdateIconFileForShortcut(const base::FilePath& web_app_path,
                               const base::FilePath& shortcut,
                               const std::u16string& new_app_title) {
  const base::FilePath icon_file = GetIconFilePath(web_app_path, new_app_title);
  base::win::ShortcutProperties shortcut_properties;
  shortcut_properties.set_icon(icon_file, 0);
  if (!base::win::CreateOrUpdateShortcutLink(
          shortcut, shortcut_properties,
          base::win::ShortcutOperation::kUpdateExisting)) {
    DVLOG(1) << "Error updating icon for shortcut " << new_app_title;
  }
}

Result UpdateShortcuts(const base::FilePath& web_app_path,
                       const base::FilePath& profile_path,
                       const std::u16string& old_app_title,
                       const ShortcutInfo& shortcut_info) {
  // Empty titles match all shortcuts, which we don't want, so if we somehow
  // get an empty app title, ignore the update.
  if (old_app_title.empty())
    return Result::kOk;

  const std::vector<base::FilePath> all_shortcuts =
      FindMatchingShortcuts(web_app_path, profile_path, old_app_title);

  const bool title_change = old_app_title != shortcut_info.title;
  Result result = Result::kOk;
  for (const auto& shortcut : all_shortcuts) {
    const base::FilePath new_shortcut =
        shortcut.DirName()
            .Append(GetSanitizedFileName(shortcut_info.title))
            .AddExtension(installer::kLnkExt);
    if (title_change) {
      // When the title changes, it is not enough to rename the shortcut file,
      // because it still points to the old icon. Update the icon file before
      // renaming.
      UpdateIconFileForShortcut(web_app_path, shortcut, shortcut_info.title);
      // Let the Windows shell know the item has been updated. SHCNF_FLUSH must
      // be used, because we will rename the icon below (thereby sending back to
      // back SHChangeNotify events for the same file) and if the image hasn't
      // had a chance to update, the end result might be a blank image on the
      // shortcut.
      SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATH | SHCNF_FLUSH,
                     shortcut.value().c_str(), nullptr);
    }

    base::File::Error error = base::File::Error::FILE_OK;
    bool success = base::ReplaceFile(shortcut, new_shortcut, &error);
    if (success) {
      SHChangeNotify(SHCNE_RENAMEITEM, SHCNF_PATH | SHCNF_FLUSHNOWAIT,
                     shortcut.value().c_str(), new_shortcut.value().c_str());
    } else {
      DVLOG(1) << "Error renaming shortcut " << shortcut_info.title
               << " error code " << std::hex << error;
      result = Result::kError;
    }
  }

  std::vector<base::FilePath> pinned_shortcuts;
  // Find matching shortcuts in taskbar pin directories.
  base::FilePath taskbar_pins_dir;
  if (base::PathService::Get(base::DIR_TASKBAR_PINS, &taskbar_pins_dir)) {
    const std::vector<base::FilePath> shortcut_files =
        FindAppShortcutsByProfileAndTitle(taskbar_pins_dir, profile_path,
                                          old_app_title);
    pinned_shortcuts.insert(pinned_shortcuts.end(), shortcut_files.begin(),
                            shortcut_files.end());
  }

  // Check all folders in ImplicitAppShortcuts.
  base::FilePath implicit_app_shortcuts_dir;
  if (base::PathService::Get(base::DIR_IMPLICIT_APP_SHORTCUTS,
                             &implicit_app_shortcuts_dir)) {
    base::FileEnumerator directory_enum(implicit_app_shortcuts_dir, false,
                                        base::FileEnumerator::DIRECTORIES);
    for (base::FilePath directory = directory_enum.Next(); !directory.empty();
         directory = directory_enum.Next()) {
      const std::vector<base::FilePath> shortcut_files =
          FindAppShortcutsByProfileAndTitle(directory, profile_path,
                                            old_app_title);
      pinned_shortcuts.insert(pinned_shortcuts.end(), shortcut_files.begin(),
                              shortcut_files.end());
    }
  }
  if (pinned_shortcuts.empty())
    return result;

  // Rename the pinned shortcuts. The shortcut filename is used to determine the
  // display name for a pinned icon, so renaming the shortcut file in the
  // taskbar dir is the only way to update the display name. Setting properties
  // like PKEY_ItemName on the shortcut does not seem to change the shortcut's
  // properties, as determined by shortcut_properties.py.
  for (const auto& shortcut : pinned_shortcuts) {
    const base::FilePath new_shortcut =
        shortcut.DirName()
            .Append(GetSanitizedFileName(shortcut_info.title))
            .AddExtension(installer::kLnkExt);

    if (title_change) {
      UpdateIconFileForShortcut(web_app_path, shortcut, shortcut_info.title);
      SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATH | SHCNF_FLUSH,
                     shortcut.value().c_str(), nullptr);
    }

    base::File::Error error = base::File::Error::FILE_OK;
    bool success = base::ReplaceFile(shortcut, new_shortcut, &error);
    if (success) {
      // Tell the Windows shell the shortcut has been renamed. Using SHCNF_FLUSH
      // also works, but blocking is probably a bad idea.
      SHChangeNotify(SHCNE_RENAMEITEM, SHCNF_PATH | SHCNF_FLUSHNOWAIT,
                     shortcut.value().c_str(), new_shortcut.value().c_str());
    } else {
      DVLOG(1) << "Error renaming shortcut " << shortcut_info.title
               << " error code " << std::hex << error;
      result = Result::kError;
    }
  }
  // SHCNE_ALLEVENTS prevents the WebApp icon on the taskbar from becoming a
  // blank document. It's not needed if we use SHCNF_FLUSH above.
  // This fixes the name of the web app in the pinned icon context menu.
  // However, the tooltip will show the old web app name until the user logs
  // out of Windows and logs back in.
  SHChangeNotify(SHCNE_ASSOCCHANGED,
                 SHCNF_IDLIST | SHCNE_ALLEVENTS | SHCNF_FLUSHNOWAIT, nullptr,
                 nullptr);
  return result;
}

// Gets the directories with shortcuts for an app, and deletes the shortcuts.
// This will search the standard locations for shortcuts named |title| that open
// in the profile with |profile_path|.
// If |web_app_path| is empty, this will not delete shortcuts from the web app
// directory. If |title| is empty, all shortcuts for this profile will be
// deleted.
void GetShortcutLocationsAndDeleteShortcuts(
    const base::FilePath& web_app_path,
    const base::FilePath& profile_path,
    const std::u16string& title,
    DeleteShortcutsCallback result_callback) {
  const std::vector<base::FilePath> all_shortcuts =
      FindMatchingShortcuts(web_app_path, profile_path, title);

  if (all_shortcuts.empty()) {
    std::move(result_callback).Run(/*shortcut_deleted=*/true);
    return;
  }

  // Calling UnpinShortcuts in unit-tests currently crashes the test, so skip it
  // for now using the shortcut override mechanism.
  if (OsIntegrationTestOverride::Get()) {
    CHECK_IS_TEST();
    DeleteShortcuts(all_shortcuts, std::move(result_callback));
    return;
  }

  // TODO(crbug.com/40250252): Figure out how to make this call not crash &
  // incorporate unpin / pin methods in unit-tests.
  shell_integration::win::UnpinShortcuts(
      all_shortcuts, base::BindOnce(&DeleteShortcuts, all_shortcuts,
                                    std::move(result_callback)));
}

void CreateIconAndSetRelaunchDetails(const base::FilePath& web_app_path,
                                     const base::FilePath& icon_file,
                                     HWND hwnd,
                                     const ShortcutInfo& shortcut_info) {
  base::CommandLine command_line =
      shell_integration::CommandLineArgsForLauncher(
          shortcut_info.url, shortcut_info.app_id, shortcut_info.profile_path,
          "");

  command_line.SetProgram(shortcuts::GetChromeProxyPath());
  ui::win::SetRelaunchDetailsForWindow(command_line.GetCommandLineString(),
                                       base::AsWString(shortcut_info.title),
                                       hwnd);

  ui::win::SetAppIconForWindow(icon_file, 0, hwnd);
  CheckAndSaveIcon(icon_file, shortcut_info.favicon, true);
}

// Looks for a shortcut at "|shortcut_path|/|sanitized_shortcut_name|.lnk", plus
// any duplicates of it (i.e., ending in (1), (2), etc.). Appends any that are
// app shortcuts for the profile at |profile_path| to |shortcut_paths|.
void AppendShortcutsMatchingName(
    std::vector<base::FilePath>& shortcut_paths,
    const base::FilePath& shortcut_path,
    const base::FilePath& profile_path,
    const base::FilePath& sanitized_shortcut_name) {
  const base::FilePath shortcut_filename =
      sanitized_shortcut_name.AddExtension(FILE_PATH_LITERAL(".lnk"));
  const base::FilePath shortcut_file_path =
      shortcut_path.Append(shortcut_filename);
  if (base::PathExists(shortcut_file_path) &&
      IsAppShortcutForProfile(shortcut_file_path, profile_path)) {
    shortcut_paths.push_back(shortcut_file_path);
  }

  base::FileEnumerator files(
      shortcut_path, false, base::FileEnumerator::FILES,
      shortcut_filename.InsertBeforeExtension(FILE_PATH_LITERAL(" (*)"))
          .value());
  base::FilePath shortcut_file = files.Next();
  while (!shortcut_file.empty()) {
    if (IsAppShortcutForProfile(shortcut_file, profile_path))
      shortcut_paths.push_back(shortcut_file);
    shortcut_file = files.Next();
  }
}

bool CreatePlatformShortcuts(const base::FilePath& web_app_path,
                             const ShortcutLocations& creation_locations,
                             ShortcutCreationReason creation_reason,
                             const ShortcutInfo& shortcut_info) {
  // Nothing to do on Windows for hidden apps.
  if (creation_locations.applications_menu_location ==
      APP_MENU_LOCATION_HIDDEN) {
    return true;
  }

  // If this is set, then keeping this as a local variable ensures it is not
  // destroyed while we use state from it (retrieved in `GetShortcutPaths()`).
  scoped_refptr<OsIntegrationTestOverride> test_override =
      OsIntegrationTestOverride::Get();

  bool pin_to_taskbar = false;
  // PinShortcutToTaskbar in unit-tests are not preferred as unpinning causes
  // crashes, so use the shortcut override for testing to not pin to taskbar.
  // TODO(crbug.com/40250252): Figure out how to make this call not crash &
  // incorporate unpin / pin methods in unit-tests.
  if (!test_override) {
    pin_to_taskbar =
        creation_locations.in_quick_launch_bar && CanPinShortcutToTaskbar();
  }

  // We don't want to actually create shortcuts in the quick launch directory.
  // Those are created by Windows as a side effect of pinning a shortcut to
  // the taskbar, e.g., a desktop shortcut. So, create a copy of
  // shortcut_locations with in_quick_launch_bar turned off and pass that
  // to GetShortcutPaths.
  ShortcutLocations shortcut_locations_wo_quick_launch(creation_locations);
  shortcut_locations_wo_quick_launch.in_quick_launch_bar = false;

  // Shortcut paths under which to create shortcuts.
  std::vector<base::FilePath> shortcut_paths =
      GetShortcutPaths(shortcut_locations_wo_quick_launch);

  // Create/update the shortcut in the web app path for the "Pin To Taskbar"
  // option in Win7 and Win10 versions that support pinning. We use the web app
  // path shortcut because we will overwrite it rather than appending unique
  // numbers if the shortcut already exists. This prevents pinned apps from
  // having unique numbers in their names.
  if (pin_to_taskbar) {
    shortcut_paths.push_back(web_app_path);
  }

  if (shortcut_paths.empty()) {
    return false;
  }

  if (!CreateShortcutsInPaths(
          web_app_path, shortcut_info, shortcut_paths, creation_reason,
          creation_locations.in_startup ? kRunOnOsLoginModeWindowed : "")) {
    return false;
  }

  if (pin_to_taskbar) {
    base::FilePath file_name = GetSanitizedFileName(shortcut_info.title);
    // Use the web app path shortcut for pinning to avoid having unique numbers
    // in the application name.
    base::FilePath shortcut_to_pin =
        web_app_path.Append(file_name).AddExtension(installer::kLnkExt);
    if (!PinShortcutToTaskbar(shortcut_to_pin)) {
      return false;
    }

    // This invalidates the Windows icon cache and causes the icon changes to
    // register with the taskbar and desktop.
    ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
  }

  return true;
}

}  // namespace

base::FilePath GetSanitizedFileName(const std::u16string& name) {
  std::wstring file_name = base::AsWString(name);
  base::i18n::ReplaceIllegalCharactersInPath(&file_name, ' ');
  return base::FilePath(file_name);
}

std::vector<base::FilePath> FindAppShortcutsByProfileAndTitle(
    const base::FilePath& shortcut_path,
    const base::FilePath& profile_path,
    const std::u16string& shortcut_name) {
  std::vector<base::FilePath> shortcut_paths;

  if (shortcut_name.empty()) {
    // Find all shortcuts for this profile.
    base::FileEnumerator files(shortcut_path, false,
                               base::FileEnumerator::FILES,
                               FILE_PATH_LITERAL("*.lnk"));
    base::FilePath shortcut_file = files.Next();
    while (!shortcut_file.empty()) {
      if (IsAppShortcutForProfile(shortcut_file, profile_path))
        shortcut_paths.push_back(shortcut_file);
      shortcut_file = files.Next();
    }
  } else {
    // Find all shortcuts matching |shortcut_name|. Includes duplicates, if any
    // exist (e.g., "|shortcut_name| (2).lnk").
    AppendShortcutsMatchingName(shortcut_paths, shortcut_path, profile_path,
                                GetSanitizedFileName(shortcut_name));
  }
  return shortcut_paths;
}

void OnShortcutInfoLoadedForSetRelaunchDetails(
    HWND hwnd,
    std::unique_ptr<ShortcutInfo> shortcut_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Set window's icon to the one we're about to create/update in the web app
  // path. The icon cache will refresh on icon creation.
  base::FilePath web_app_path = GetOsIntegrationResourcesDirectoryForApp(
      shortcut_info->profile_path, shortcut_info->app_id, shortcut_info->url);
  base::FilePath icon_file =
      GetIconFilePath(web_app_path, shortcut_info->title);

  PostShortcutIOTask(base::BindOnce(&CreateIconAndSetRelaunchDetails,
                                    web_app_path, icon_file, hwnd),
                     std::move(shortcut_info));
}

bool CheckAndSaveIcon(const base::FilePath& icon_file,
                      const gfx::ImageFamily& image,
                      bool refresh_shell_icon_cache) {
  if (!ShouldUpdateIcon(icon_file, image))
    return true;

  if (!base::CreateDirectory(icon_file.DirName()))
    return false;

  if (!SaveIconWithCheckSum(icon_file, image))
    return false;

  if (refresh_shell_icon_cache) {
    // Refresh shell's icon cache. This call is quite disruptive as user would
    // see explorer rebuilding the icon cache. It would be great that we find
    // a better way to achieve this.
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSHNOWAIT,
                   nullptr, nullptr);
  }
  return true;
}

void CreatePlatformShortcuts(const base::FilePath& web_app_path,
                             const ShortcutLocations& creation_locations,
                             ShortcutCreationReason creation_reason,
                             const ShortcutInfo& shortcut_info,
                             CreateShortcutsCallback callback) {
  bool result = CreatePlatformShortcuts(web_app_path, creation_locations,
                                        creation_reason, shortcut_info);
  std::move(callback).Run(result);
}

void UpdatePlatformShortcuts(
    const base::FilePath& web_app_path,
    const std::u16string& old_app_title,
    std::optional<ShortcutLocations> user_specified_locations,
    ResultCallback callback,
    const ShortcutInfo& shortcut_info) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // If this is set, then keeping this as a local variable ensures it is not
  // destroyed while we use state from it (retrieved in `GetShortcutPaths()`).
  scoped_refptr<OsIntegrationTestOverride> test_override =
      OsIntegrationTestOverride::Get();

  // Update the icon if necessary.
  const base::FilePath icon_file =
      GetIconFilePath(web_app_path, shortcut_info.title);
  bool success_updating_icon =
      CheckAndSaveIcon(icon_file, shortcut_info.favicon, true);

  ShortcutLocations existing_locations =
      GetAppExistingShortCutLocationImpl(shortcut_info);

  bool require_creation_in_different_places =
      user_specified_locations.has_value() &&
      (user_specified_locations.value() != existing_locations);

  // If an update is triggered due to stacked installation calls, then ensure
  // that new shortcuts are created in user specified locations before
  // triggering a name update.
  if (require_creation_in_different_places) {
    ShortcutLocations creation_locations =
        MergeLocations(user_specified_locations.value(), existing_locations);
    CreatePlatformShortcuts(web_app_path, creation_locations,
                            SHORTCUT_CREATION_BY_USER, shortcut_info);
  }

  if (old_app_title != shortcut_info.title) {
    // The app's title has changed. Rename existing shortcuts.
    if (UpdateShortcuts(web_app_path, shortcut_info.profile_path, old_app_title,
                        shortcut_info) == Result::kError) {
      success_updating_icon = false;
    }

    // Also delete the old icon file and checksum file, to avoid leaving
    // orphaned files on disk. The new one was recreated above.
    const base::FilePath old_icon_file =
        GetIconFilePath(web_app_path, old_app_title);
    const base::FilePath old_checksum_file(
        old_icon_file.ReplaceExtension(kIconChecksumFileExt));
    base::DeleteFile(old_icon_file);
    base::DeleteFile(old_checksum_file);
  }
  Result result = (success_updating_icon ? Result::kOk : Result::kError);
  std::move(callback).Run(result);
}

ShortcutLocations GetAppExistingShortCutLocationImpl(
    const ShortcutInfo& shortcut_info) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // If this is set, then keeping this as a local variable ensures it is not
  // destroyed while we use state from it (retrieved in `GetShortcutPaths()`).
  scoped_refptr<OsIntegrationTestOverride> test_override =
      OsIntegrationTestOverride::Get();
  ShortcutLocations result;
  ShortcutLocations desktop;
  desktop.on_desktop = true;
  auto shortcut_paths = GetShortcutPaths(desktop);
  if (!shortcut_paths.empty() &&
      !FindAppShortcutsByProfileAndTitle(shortcut_paths.front(),
                                         shortcut_info.profile_path,
                                         shortcut_info.title)
           .empty()) {
    result.on_desktop = true;
  }

  ShortcutLocations app_menu;
  app_menu.applications_menu_location = APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;
  shortcut_paths = GetShortcutPaths(app_menu);
  if (!shortcut_paths.empty() &&
      !FindAppShortcutsByProfileAndTitle(shortcut_paths.front(),
                                         shortcut_info.profile_path,
                                         shortcut_info.title)
           .empty()) {
    result.applications_menu_location = APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;
  }

  ShortcutLocations quick_launch;
  quick_launch.in_quick_launch_bar = true;
  shortcut_paths = GetShortcutPaths(quick_launch);
  if (!shortcut_paths.empty() &&
      !FindAppShortcutsByProfileAndTitle(shortcut_paths.front(),
                                         shortcut_info.profile_path,
                                         shortcut_info.title)
           .empty()) {
    result.in_quick_launch_bar = true;
  }

  ShortcutLocations start_up;
  start_up.in_startup = true;
  shortcut_paths = GetShortcutPaths(start_up);
  if (!shortcut_paths.empty() &&
      !FindAppShortcutsByProfileAndTitle(shortcut_paths.front(),
                                         shortcut_info.profile_path,
                                         shortcut_info.title)
           .empty()) {
    result.in_startup = true;
  }
  return result;
}

void FinishDeletingPlatformShortcuts(
    const base::FilePath& web_app_path,
    scoped_refptr<base::TaskRunner> result_runner,
    DeleteShortcutsCallback result_callback,
    bool delete_result) {
  // If there are no more shortcuts in the Chrome Apps subdirectory, remove it.
  base::FilePath chrome_apps_dir;
  if (ShellUtil::GetShortcutPath(
          ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR,
          ShellUtil::CURRENT_USER, &chrome_apps_dir)) {
    if (base::IsDirectoryEmpty(chrome_apps_dir))
      base::DeleteFile(chrome_apps_dir);
  }
  bool result = delete_result;
  // Delete downloaded shortcut icons for the web app.
  if (!DeleteShortcutsMenuIcons(web_app_path))
    result = false;
  result_runner->PostTask(FROM_HERE,
                          BindOnce(std::move(result_callback), result));
}

void DeletePlatformShortcuts(const base::FilePath& web_app_path,
                             const ShortcutInfo& shortcut_info,
                             scoped_refptr<base::TaskRunner> result_runner,
                             DeleteShortcutsCallback callback) {
  // If this is set, then keeping this as a local variable ensures it is not
  // destroyed while we use state from it (retrieved in `GetShortcutPaths()`).
  scoped_refptr<OsIntegrationTestOverride> test_override =
      OsIntegrationTestOverride::Get();
  GetShortcutLocationsAndDeleteShortcuts(
      web_app_path, shortcut_info.profile_path, shortcut_info.title,
      base::BindOnce(&FinishDeletingPlatformShortcuts, web_app_path,
                     std::move(result_runner), std::move(callback)));
}

void FinishDeletingAllShortcutsForProfile(bool result) {
  // If there are no more shortcuts in the Chrome Apps subdirectory, remove it.
  base::FilePath chrome_apps_dir;
  if (ShellUtil::GetShortcutPath(
          ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR,
          ShellUtil::CURRENT_USER, &chrome_apps_dir)) {
    // This will fail if dir is not empty, so no need to check if it's empty.
    base::DeleteFile(chrome_apps_dir);
  }
}

void DeleteAllShortcutsForProfile(const base::FilePath& profile_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // If this is set, then keeping this as a local variable ensures it is not
  // destroyed while we use state from it (retrieved in `GetShortcutPaths()`).
  scoped_refptr<OsIntegrationTestOverride> test_override =
      OsIntegrationTestOverride::Get();
  GetShortcutLocationsAndDeleteShortcuts(
      base::FilePath(), profile_path, std::u16string(),
      base::BindOnce(&FinishDeletingAllShortcutsForProfile));
}

std::vector<base::FilePath> GetShortcutPaths(
    const ShortcutLocations& creation_locations) {
  // Shortcut paths under which to create shortcuts.
  std::vector<base::FilePath> shortcut_paths;
  // if there is no ShortcutOverrirdeForTesting, set it to empty.
  scoped_refptr<OsIntegrationTestOverride> testing_shortcuts =
      OsIntegrationTestOverride::Get();
  // Locations to add to shortcut_paths.
  struct {
    bool use_this_location;
    ShellUtil::ShortcutLocation location_id;
  } locations[] = {
      {creation_locations.on_desktop, ShellUtil::SHORTCUT_LOCATION_DESKTOP},
      {creation_locations.applications_menu_location ==
           APP_MENU_LOCATION_SUBDIR_CHROMEAPPS,
       ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR},
      {// For some versions of Windows, `in_quick_launch_bar` indicates that we
       // are pinning to taskbar. This needs to be handled by callers.
       creation_locations.in_quick_launch_bar && CanPinShortcutToTaskbar(),
       ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH},
      {creation_locations.in_startup, ShellUtil::SHORTCUT_LOCATION_STARTUP}};

  // Populate shortcut_paths.
  base::FilePath path;
  for (auto location : locations) {
    if (location.use_this_location) {
      if (ShellUtil::GetShortcutPath(location.location_id,
                                     ShellUtil::CURRENT_USER, &path)) {
        shortcut_paths.push_back(path);
      }
    }
  }
  return shortcut_paths;
}

base::FilePath GetIconFilePath(const base::FilePath& web_app_path,
                               const std::u16string& title) {
  return web_app_path.Append(GetSanitizedFileName(title))
      .AddExtension(FILE_PATH_LITERAL(".ico"));
}

}  // namespace internals

}  // namespace web_app
