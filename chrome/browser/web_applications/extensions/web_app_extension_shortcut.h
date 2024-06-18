// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_WEB_APP_EXTENSION_SHORTCUT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_WEB_APP_EXTENSION_SHORTCUT_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace extensions {
class Extension;
}

namespace web_app {

// Called by GetShortcutInfoForApp after fetching the ShortcutInfo.
using ShortcutInfoCallback =
    base::OnceCallback<void(std::unique_ptr<ShortcutInfo>)>;

// Create shortcuts for web application based on given shortcut data.
// |shortcut_info| contains information about the shortcuts to create,
// |locations| contains information about where to create them, and
// |callback| is a callback that is made when completed, indicating success
// or failure of the operation.
void CreateShortcutsWithInfo(ShortcutCreationReason reason,
                             const ShortcutLocations& locations,
                             CreateShortcutsCallback callback,
                             std::unique_ptr<ShortcutInfo> shortcut_info);

// Populates a ShortcutInfo for the given |extension| in |profile| and passes
// it to |callback| after asynchronously loading all icon representations.
void GetShortcutInfoForApp(const extensions::Extension* extension,
                           Profile* profile,
                           ShortcutInfoCallback callback);

std::unique_ptr<ShortcutInfo> ShortcutInfoForExtensionAndProfile(
    const extensions::Extension* app,
    Profile* profile);

// Whether to create a shortcut for this type of extension.
bool ShouldCreateShortcutFor(ShortcutCreationReason reason,
                             Profile* profile,
                             const extensions::Extension* extension);

// Creates shortcuts for an app. This loads the app's icon from disk, and calls
// CreateShortcutsWithInfo(). If you already have a ShortcutInfo with the app's
// icon loaded, you should use CreateShortcutsWithInfo() directly.
void CreateShortcuts(ShortcutCreationReason reason,
                     const ShortcutLocations& locations,
                     Profile* profile,
                     const extensions::Extension* app,
                     CreateShortcutsCallback callback);

// Delete all shortcuts that have been created for the given profile and
// extension.
void DeleteAllShortcuts(Profile* profile, const extensions::Extension* app);

// Register a callback that will be run once |app_id|'s shortcuts have been
// deleted.
void WaitForExtensionShortcutsDeleted(const webapps::AppId& app_id,
                                      base::OnceClosure callback);

// Updates shortcuts for |app|, but does not create new ones if shortcuts are
// not present in user-facing locations. Some platforms may still (re)create
// hidden shortcuts to interact correctly with the system shelf.
// |old_app_title| contains the title of the app prior to this update.
// |callback| is invoked once the FILE thread tasks have completed.
void UpdateAllShortcuts(const std::u16string& old_app_title,
                        Profile* profile,
                        const extensions::Extension* app,
                        base::OnceClosure callback);

// Updates shortcuts for all apps in this profile. This is expected to be called
// on the UI thread.
void UpdateShortcutsForAllApps(Profile* profile, base::OnceClosure callback);

#if BUILDFLAG(IS_WIN)
// Update the relaunch details for the given app's window, making the taskbar
// group's "Pin to the taskbar" button function correctly.
void UpdateRelaunchDetailsForApp(Profile* profile,
                                 const extensions::Extension* extension,
                                 HWND hwnd);
#endif  // BUILDFLAG(IS_WIN)

SynchronizeOsOptions ConvertShortcutLocationsToSynchronizeOptions(
    const ShortcutLocations& locations,
    ShortcutCreationReason reason);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_WEB_APP_EXTENSION_SHORTCUT_H_
