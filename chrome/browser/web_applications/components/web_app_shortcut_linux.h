// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SHORTCUT_LINUX_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SHORTCUT_LINUX_H_

#include <string>

namespace base {
class FilePath;
class Environment;
}  // namespace base

namespace web_app {

struct ShortcutInfo;
struct ShortcutLocations;

// Create shortcuts on the desktop or in the application menu (as specified by
// |shortcut_info|), for the web page or extension in |shortcut_info|.
// For extensions, duplicate shortcuts are avoided, so if a requested shortcut
// already exists it is deleted first.
bool CreateDesktopShortcut(const ShortcutInfo& shortcut_info,
                           const ShortcutLocations& creation_locations);

// Returns filename for .desktop file based on |profile_path| and
// |app_id|, sanitized for security.
base::FilePath GetAppShortcutFilename(const base::FilePath& profile_path,
                                      const std::string& app_id);

// Returns the set of locations in which shortcuts are installed for the
// extension with |extension_id| in |profile_path|.
// This searches the file system for .desktop files in appropriate locations. A
// shortcut with NoDisplay=true causes hidden to become true, instead of
// creating at APP_MENU_LOCATIONS_SUBDIR_CHROMEAPPS.
ShortcutLocations GetExistingShortcutLocations(
    base::Environment* env,
    const base::FilePath& profile_path,
    const std::string& extension_id);

// Version of GetExistingShortcutLocations which takes an explicit path
// to the user's desktop directory. Useful for testing.
// If |desktop_path| is empty, the desktop is not searched.
ShortcutLocations GetExistingShortcutLocations(
    base::Environment* env,
    const base::FilePath& profile_path,
    const std::string& extension_id,
    const base::FilePath& desktop_path);

// Delete any desktop shortcuts on desktop or in the application menu that have
// been added for the extension with |extension_id| in |profile_path|.
void DeleteDesktopShortcuts(const base::FilePath& profile_path,
                            const std::string& extension_id);

// Delete any desktop shortcuts on desktop or in the application menu that have
// for the profile in |profile_path|.
void DeleteAllDesktopShortcuts(const base::FilePath& profile_path);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SHORTCUT_LINUX_H_
