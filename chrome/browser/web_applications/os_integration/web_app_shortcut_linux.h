// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUT_LINUX_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUT_LINUX_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "url/gurl.h"

namespace base {
class FilePath;
class Environment;
}  // namespace base

namespace web_app {

struct ShortcutInfo;
struct ShortcutLocations;

// Represents the info required to create a desktop action for an app.
// See
// https://specifications.freedesktop.org/desktop-entry-spec/latest/ar01s11.html.
struct DesktopActionInfo {
  DesktopActionInfo();
  DesktopActionInfo(const std::string& id,
                    const std::string& name,
                    const GURL& exec_launch_url);
  DesktopActionInfo(const DesktopActionInfo&);
  DesktopActionInfo& operator=(const DesktopActionInfo&) = delete;
  ~DesktopActionInfo();

  // Unique ID for this action. Can only contain characters A-Za-z0-9-.
  std::string id;
  // Action name to be displayed in the context menu.
  std::string name;
  // The URL to launch the app in for this action.
  GURL exec_launch_url;
  // TODO(crbug.com/40683945): Process icons.

  bool operator<(const DesktopActionInfo& other) const;
};

using LaunchXdgUtilityForTesting =
    base::RepeatingCallback<bool(const std::vector<std::string>&, int*)>;

// Test helper that hooking calls to shell_integration_linux::LaunchXdgUtility
void SetLaunchXdgUtilityForTesting(
    LaunchXdgUtilityForTesting launchXdgUtilityForTesting);

// Create shortcuts on the desktop or in the application menu (as specified by
// |shortcut_info|), for the web page or extension in |shortcut_info|.
// For extensions, duplicate shortcuts are avoided, so if a requested shortcut
// already exists it is deleted first.
bool CreateDesktopShortcut(base::Environment* env,
                           const ShortcutInfo& shortcut_info,
                           const ShortcutLocations& creation_locations);

// Returns filename for .desktop file based on |profile_path| and
// |app_id|, sanitized for security.
base::FilePath GetAppDesktopShortcutFilename(const base::FilePath& profile_path,
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

bool UpdateDesktopShortcuts(
    base::Environment* env,
    const ShortcutInfo& shortcut_info,
    std::optional<ShortcutLocations> user_specified_locations);

// Delete any desktop shortcuts on desktop or in the application menu that have
// been added for the extension with |extension_id| in |profile_path|. Returns
// true on successful deletion.
bool DeleteDesktopShortcuts(base::Environment* env,
                            const base::FilePath& profile_path,
                            const std::string& extension_id);

// Delete any desktop shortcuts on desktop or in the application menu that have
// for the profile in |profile_path|. Returns true on successful deletion.
bool DeleteAllDesktopShortcuts(base::Environment* env,
                               const base::FilePath& profile_path);

// Returns the shortcuts that match profile for |profile_path| and
// web app with id |appid|.
std::vector<base::FilePath> GetShortcutLocations(
    base::Environment* env,
    const ShortcutLocations& locations,
    const base::FilePath& profile_path,
    const std::string& app_id);

namespace internals {

// Returns the shortcuts that match profile for |profile_path| and
// web app with id |appid| using the default environment.
std::vector<base::FilePath> GetShortcutLocations(
    const ShortcutLocations& locations,
    const base::FilePath& profile_path,
    const std::string& app_id);

}  // namespace internals

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUT_LINUX_H_
