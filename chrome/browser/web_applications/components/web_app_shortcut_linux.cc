// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_shortcut_linux.h"

#include <fcntl.h>

#include "base/base_paths.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/file_util_icu.h"
#include "base/nix/xdg_util.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"

namespace {

const char kDirectoryFilename[] = "chrome-apps.directory";

std::string CreateShortcutIcon(const gfx::ImageFamily& icon_images,
                               const base::FilePath& shortcut_filename) {
  if (icon_images.empty())
    return std::string();

  // TODO(phajdan.jr): Report errors from this function, possibly as infobars.
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir())
    return std::string();

  base::FilePath temp_file_path =
      temp_dir.GetPath().Append(shortcut_filename.ReplaceExtension("png"));
  std::string icon_name = temp_file_path.BaseName().RemoveExtension().value();

  for (gfx::ImageFamily::const_iterator it = icon_images.begin();
       it != icon_images.end(); ++it) {
    int width = it->Width();
    scoped_refptr<base::RefCountedMemory> png_data = it->As1xPNGBytes();
    if (png_data->size() == 0) {
      // If the bitmap could not be encoded to PNG format, skip it.
      LOG(WARNING) << "Could not encode icon " << icon_name << ".png at size "
                   << width << ".";
      continue;
    }
    int bytes_written = base::WriteFile(
        temp_file_path, png_data->front_as<char>(), png_data->size());

    if (bytes_written != static_cast<int>(png_data->size()))
      return std::string();

    std::vector<std::string> argv;
    argv.push_back("xdg-icon-resource");
    argv.push_back("install");

    // Always install in user mode, even if someone runs the browser as root
    // (people do that).
    argv.push_back("--mode");
    argv.push_back("user");

    argv.push_back("--size");
    argv.push_back(base::NumberToString(width));

    argv.push_back(temp_file_path.value());
    argv.push_back(icon_name);
    int exit_code;
    if (!shell_integration_linux::LaunchXdgUtility(argv, &exit_code) ||
        exit_code) {
      LOG(WARNING) << "Could not install icon " << icon_name << ".png at size "
                   << width << ".";
    }
  }
  return icon_name;
}

bool CreateShortcutOnDesktop(const base::FilePath& shortcut_filename,
                             const std::string& contents) {
  // Make sure that we will later call openat in a secure way.
  DCHECK_EQ(shortcut_filename.BaseName().value(), shortcut_filename.value());

  base::FilePath desktop_path;
  if (!base::PathService::Get(base::DIR_USER_DESKTOP, &desktop_path))
    return false;

  int desktop_fd = open(desktop_path.value().c_str(), O_RDONLY | O_DIRECTORY);
  if (desktop_fd < 0)
    return false;

  int fd = openat(desktop_fd, shortcut_filename.value().c_str(),
                  O_CREAT | O_EXCL | O_WRONLY,
                  S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
  if (fd < 0) {
    if (IGNORE_EINTR(close(desktop_fd)) < 0)
      PLOG(ERROR) << "close";
    return false;
  }

  if (!base::WriteFileDescriptor(fd, contents.c_str(), contents.size())) {
    // Delete the file. No shortuct is better than corrupted one. Use unlinkat
    // to make sure we're deleting the file in the directory we think we are.
    // Even if an attacker manager to put something other at
    // |shortcut_filename| we'll just undo their action.
    unlinkat(desktop_fd, shortcut_filename.value().c_str(), 0);
  }

  if (IGNORE_EINTR(close(fd)) < 0)
    PLOG(ERROR) << "close";

  if (IGNORE_EINTR(close(desktop_fd)) < 0)
    PLOG(ERROR) << "close";

  return true;
}

// Creates a shortcut with |shortcut_filename| and |contents| in the system
// applications menu. If |directory_filename| is non-empty, creates a sub-menu
// with |directory_filename| and |directory_contents|, and stores the shortcut
// under the sub-menu.
bool CreateShortcutInApplicationsMenu(const base::FilePath& shortcut_filename,
                                      const std::string& contents,
                                      const base::FilePath& directory_filename,
                                      const std::string& directory_contents) {
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir())
    return false;

  base::FilePath temp_directory_path;
  if (!directory_filename.empty()) {
    temp_directory_path = temp_dir.GetPath().Append(directory_filename);

    int bytes_written =
        base::WriteFile(temp_directory_path, directory_contents.data(),
                        directory_contents.length());

    if (bytes_written != static_cast<int>(directory_contents.length()))
      return false;
  }

  base::FilePath temp_file_path = temp_dir.GetPath().Append(shortcut_filename);

  int bytes_written =
      base::WriteFile(temp_file_path, contents.data(), contents.length());

  if (bytes_written != static_cast<int>(contents.length()))
    return false;

  std::vector<std::string> argv;
  argv.push_back("xdg-desktop-menu");
  argv.push_back("install");

  // Always install in user mode, even if someone runs the browser as root
  // (people do that).
  argv.push_back("--mode");
  argv.push_back("user");

  // If provided, install the shortcut file inside the given directory.
  if (!directory_filename.empty())
    argv.push_back(temp_directory_path.value());
  argv.push_back(temp_file_path.value());
  int exit_code;
  shell_integration_linux::LaunchXdgUtility(argv, &exit_code);
  return exit_code == 0;
}

}  // namespace

namespace web_app {

base::FilePath GetAppShortcutFilename(const base::FilePath& profile_path,
                                      const std::string& app_id) {
  DCHECK(!app_id.empty());

  // Use a prefix, because xdg-desktop-menu requires it.
  std::string filename(chrome::kBrowserProcessExecutableName);
  filename.append("-").append(app_id).append("-").append(
      profile_path.BaseName().value());
  base::i18n::ReplaceIllegalCharactersInPath(&filename, '_');
  // Spaces in filenames break xdg-desktop-menu
  // (see https://bugs.freedesktop.org/show_bug.cgi?id=66605).
  base::ReplaceChars(filename, " ", "_", &filename);
  return base::FilePath(filename.append(".desktop"));
}

void DeleteShortcutOnDesktop(const base::FilePath& shortcut_filename) {
  base::FilePath desktop_path;
  if (base::PathService::Get(base::DIR_USER_DESKTOP, &desktop_path))
    base::DeleteFile(desktop_path.Append(shortcut_filename), false);
}

void DeleteShortcutInApplicationsMenu(
    const base::FilePath& shortcut_filename,
    const base::FilePath& directory_filename) {
  std::vector<std::string> argv;
  argv.push_back("xdg-desktop-menu");
  argv.push_back("uninstall");

  // Uninstall in user mode, to match the install.
  argv.push_back("--mode");
  argv.push_back("user");

  // The file does not need to exist anywhere - xdg-desktop-menu will uninstall
  // items from the menu with a matching name.
  // If |directory_filename| is supplied, this will also remove the item from
  // the directory, and remove the directory if it is empty.
  if (!directory_filename.empty())
    argv.push_back(directory_filename.value());
  argv.push_back(shortcut_filename.value());
  int exit_code;
  shell_integration_linux::LaunchXdgUtility(argv, &exit_code);
}

bool CreateDesktopShortcut(const ShortcutInfo& shortcut_info,
                           const ShortcutLocations& creation_locations) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::FilePath shortcut_filename;
  if (!shortcut_info.extension_id.empty()) {
    shortcut_filename = GetAppShortcutFilename(shortcut_info.profile_path,
                                               shortcut_info.extension_id);
    // For extensions we do not want duplicate shortcuts. So, delete any that
    // already exist and replace them.
    if (creation_locations.on_desktop)
      DeleteShortcutOnDesktop(shortcut_filename);

    if (creation_locations.applications_menu_location !=
        APP_MENU_LOCATION_NONE) {
      DeleteShortcutInApplicationsMenu(shortcut_filename, base::FilePath());
    }
  } else {
    shortcut_filename =
        shell_integration_linux::GetWebShortcutFilename(shortcut_info.url);
  }
  if (shortcut_filename.empty())
    return false;

  std::string icon_name =
      CreateShortcutIcon(shortcut_info.favicon, shortcut_filename);

  std::string app_name = GenerateApplicationNameFromInfo(shortcut_info);

  bool success = true;

  base::FilePath chrome_exe_path =
      shell_integration_linux::internal::GetChromeExePath();
  if (chrome_exe_path.empty()) {
    NOTREACHED();
    return false;
  }

  if (creation_locations.on_desktop) {
    std::string contents = shell_integration_linux::GetDesktopFileContents(
        chrome_exe_path, app_name, shortcut_info.url,
        shortcut_info.extension_id, shortcut_info.title, icon_name,
        shortcut_info.profile_path, "", "", false);
    success = CreateShortcutOnDesktop(shortcut_filename, contents);
  }

  if (creation_locations.applications_menu_location == APP_MENU_LOCATION_NONE) {
    return success;
  }

  base::FilePath directory_filename;
  std::string directory_contents;
  switch (creation_locations.applications_menu_location) {
    case APP_MENU_LOCATION_HIDDEN:
      break;
    case APP_MENU_LOCATION_SUBDIR_CHROMEAPPS:
      directory_filename = base::FilePath(kDirectoryFilename);
      directory_contents = shell_integration_linux::GetDirectoryFileContents(
          shell_integration::GetAppShortcutsSubdirName(), "");
      break;
    default:
      NOTREACHED();
      break;
  }

  // Set NoDisplay=true if hidden. This will hide the application from
  // user-facing menus.
  std::string contents = shell_integration_linux::GetDesktopFileContents(
      chrome_exe_path, app_name, shortcut_info.url, shortcut_info.extension_id,
      shortcut_info.title, icon_name, shortcut_info.profile_path, "",
      base::JoinString(shortcut_info.mime_types, ";"),
      creation_locations.applications_menu_location ==
          APP_MENU_LOCATION_HIDDEN);
  success = CreateShortcutInApplicationsMenu(shortcut_filename, contents,
                                             directory_filename,
                                             directory_contents) &&
            success;

  return success;
}

ShortcutLocations GetExistingShortcutLocations(
    base::Environment* env,
    const base::FilePath& profile_path,
    const std::string& extension_id) {
  base::FilePath desktop_path;
  // If Get returns false, just leave desktop_path empty.
  base::PathService::Get(base::DIR_USER_DESKTOP, &desktop_path);
  return GetExistingShortcutLocations(env, profile_path, extension_id,
                                      desktop_path);
}

ShortcutLocations GetExistingShortcutLocations(
    base::Environment* env,
    const base::FilePath& profile_path,
    const std::string& extension_id,
    const base::FilePath& desktop_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::FilePath shortcut_filename =
      GetAppShortcutFilename(profile_path, extension_id);
  DCHECK(!shortcut_filename.empty());
  ShortcutLocations locations;

  // Determine whether there is a shortcut on desktop.
  if (!desktop_path.empty()) {
    locations.on_desktop =
        base::PathExists(desktop_path.Append(shortcut_filename));
  }

  // Determine whether there is a shortcut in the applications directory.
  std::string shortcut_contents;
  if (shell_integration_linux::GetExistingShortcutContents(
          env, shortcut_filename, &shortcut_contents)) {
    // If the shortcut contents contain NoDisplay=true, it should be hidden.
    // Otherwise since these shortcuts are for apps, they are always in the
    // "Chrome Apps" directory.
    locations.applications_menu_location =
        shell_integration_linux::internal::GetNoDisplayFromDesktopFile(
            shortcut_contents)
            ? APP_MENU_LOCATION_HIDDEN
            : APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;
  }

  return locations;
}

void DeleteDesktopShortcuts(const base::FilePath& profile_path,
                            const std::string& extension_id) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::FilePath shortcut_filename =
      GetAppShortcutFilename(profile_path, extension_id);
  DCHECK(!shortcut_filename.empty());

  DeleteShortcutOnDesktop(shortcut_filename);
  // Delete shortcuts from |kDirectoryFilename|.
  // Note that it is possible that shortcuts were not created in the Chrome Apps
  // directory. It doesn't matter: this will still delete the shortcut even if
  // it isn't in the directory.
  DeleteShortcutInApplicationsMenu(shortcut_filename,
                                   base::FilePath(kDirectoryFilename));
}

void DeleteAllDesktopShortcuts(const base::FilePath& profile_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::unique_ptr<base::Environment> env(base::Environment::Create());

  // Delete shortcuts from Desktop.
  base::FilePath desktop_path;
  if (base::PathService::Get(base::DIR_USER_DESKTOP, &desktop_path)) {
    std::vector<base::FilePath> shortcut_filenames_desktop =
        shell_integration_linux::GetExistingProfileShortcutFilenames(
            profile_path, desktop_path);
    for (const auto& shortcut : shortcut_filenames_desktop) {
      DeleteShortcutOnDesktop(shortcut);
    }
  }

  // Delete shortcuts from |kDirectoryFilename|.
  base::FilePath applications_menu =
      shell_integration_linux::GetDataWriteLocation(env.get());
  applications_menu = applications_menu.AppendASCII("applications");
  std::vector<base::FilePath> shortcut_filenames_app_menu =
      shell_integration_linux::GetExistingProfileShortcutFilenames(
          profile_path, applications_menu);
  for (const auto& menu : shortcut_filenames_app_menu) {
    DeleteShortcutInApplicationsMenu(menu, base::FilePath(kDirectoryFilename));
  }
}

namespace internals {

bool CreatePlatformShortcuts(const base::FilePath& web_app_path,
                             const ShortcutLocations& creation_locations,
                             ShortcutCreationReason /*creation_reason*/,
                             const ShortcutInfo& shortcut_info) {
#if !defined(OS_CHROMEOS)
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  return CreateDesktopShortcut(shortcut_info, creation_locations);
#else
  return false;
#endif
}

void DeletePlatformShortcuts(const base::FilePath& web_app_path,
                             const ShortcutInfo& shortcut_info) {
#if !defined(OS_CHROMEOS)
  DeleteDesktopShortcuts(shortcut_info.profile_path,
                         shortcut_info.extension_id);
#endif
}

void UpdatePlatformShortcuts(const base::FilePath& web_app_path,
                             const base::string16& /*old_app_title*/,
                             const ShortcutInfo& shortcut_info) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::unique_ptr<base::Environment> env(base::Environment::Create());

  // Find out whether shortcuts are already installed.
  ShortcutLocations creation_locations = GetExistingShortcutLocations(
      env.get(), shortcut_info.profile_path, shortcut_info.extension_id);

  // Always create a hidden shortcut in applications if a visible one is not
  // being created. This allows the operating system to identify the app, but
  // not show it in the menu.
  if (creation_locations.applications_menu_location == APP_MENU_LOCATION_NONE)
    creation_locations.applications_menu_location = APP_MENU_LOCATION_HIDDEN;

  CreatePlatformShortcuts(web_app_path, creation_locations,
                          SHORTCUT_CREATION_AUTOMATED, shortcut_info);
}

void DeleteAllShortcutsForProfile(const base::FilePath& profile_path) {
#if !defined(OS_CHROMEOS)
  DeleteAllDesktopShortcuts(profile_path);
#endif
}

}  // namespace internals

}  // namespace web_app
