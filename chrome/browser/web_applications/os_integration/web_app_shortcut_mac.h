// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUT_MAC_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUT_MAC_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"

// Shortcuts on Mac are different from shortcuts on some other platforms, in
// that -- behind the scenes -- they consist of, not a single file (as on some
// other platforms), but a folder structure, which can be viewed and explored
// with 'ls' in a terminal. This folder structure is not visible in Finder
// (unless revealed with 'Show Package Contents' in the Finder context menu),
// and is normally represented in Finder as a single item (a shortcut, if you
// will).
//
// The shortcut structure, often also referred to as an 'app bundle', generally
// resides in:
//   ~/Applications/[Chrome|Chrome Canary|Chromium] Apps.localized/
// ... but can be freely renamed and/or moved around the OS by the user.
//
// When an app (let's call it 'Killer Marmot') is installed for the first time,
// an app bundle is created with that same name inside the directory above. When
// browsing to it with Finder, all the user sees is a single icon with the title
// 'Killer Marmot'. The folder structure, that is hidden behind it, looks
// something like this:
//
// Killer Marmot.app/          <- The app bundle folder (aka. the shortcut.)
// - Contents/
//   - Info.plist              <- General information about the app bundle.
//   - PkgInfo                 <- A file containing simply 'APPL????'.
//   - MacOS/app_mode_loader   <- A generic binary that launches the app.
//   - Resources/app.icons     <- The icons used by the shortcut.
//   - Resources/en.proj/      <- One of potentially many resources directories
//                                (swap out [en] for your locale).
//     - InfoPlist.strings     <- The localized resources string file.
//
// When determining which display name Finder should show for this app, it
// considers information from up to three different sources:
// - The filename of the App on disk, minus the .app suffix (which in this case
//   would be: 'Killer Marmot').
// - The CFBundleName value inside the Contents/Info.plist file.
// - The CFBundleDisplayName value inside the InfoPlist.strings (from the right
//   resource folder).
//   - Note: Confusingly, a value for CFBundleName, is also written to the
//   InfoPList.strings file, but it seems to not be used for anything and can
//   therefore be ignored.
//
// Of those three sources mentioned, Finder will start by considering the first
// two (the filename on disk and CFBundleName from Info.plist) and if (and only
// if) they match exactly will the display name (CFBundleDisplayName inside the
// resource file) be used in Finder. If they don't match, however, Finder will
// simply stop using the localized version (read: ignore the
// CFBundleDisplayName) and instead use the filename of the app bundle folder on
// disk (again, minus the .app suffix).
//
// Note: When testing this locally, it is important to realise that manual
// changes to the CFBundleName and/or CFBundleDisplayName values will be ignored
// by Finder, even across reboots. Changes to those values therefore would need
// to be done through the code that builds the shortcut. In contrast to that,
// changes to the *filename* of the app bundle take effect immediately. So if,
// after manually renaming, the new app folder name no longer matches
// CFBundleName, Finder will immediately stop using CFBundleDisplayName.
// Conversely, renaming the app folder back to its original name will cause
// Finder to start using the localized name again (CFBundleDisplayName).
//
// OS-specific gotchas related to using paths/urls as (or part of) App names:
//
// When MacOS decides to show the localized name (read CFBundleDisplayName) in
// Finder, it will collapse multiple consecutive forward-slashes (/) found
// within that value into a single forward-slash. This means that if
// CFBundleDisplayName contains 'https://foo.com', it will be shown in Finder as
// 'https:/foo.com' [sic]. Also, even though colon (:) is not valid input when
// the user specifies app bundle filenames manually (i.e. during rename), it can
// be programmatically added to the CFBundleDisplayName which _will_ then be
// shown in Finder. That presents a problem, however, when the user tries to
// rename the app bundle, because MacOS won't accept the new name unless the
// user manually removes the colon from the name, which is less than ideal UX.
// Therefore, the use of colons in the display name should be discouraged.
//
// Also, Chrome converts forward-slashes in the app title to colons before using
// it as the filename for the app bundle. But if MacOS decides the filename (and
// not the localized value) should be used as the display name, it will
// automatically convert any colons it finds in the filename into '/' before
// displaying. This means that if a url for foo.com is used as an app title, it
// will be written to disk as 'https:::foo.com' but displayed as
// 'https///foo.com' [sic].

// -----------------------------------------------------------------------------

// Whether to enable update and launch of app shims in tests. (Normally shims
// are never created or launched in tests). Note that update only creates
// internal shim bundles, i.e. it does not create new shims in ~/Applications.
extern bool g_app_shims_allow_update_and_launch_in_tests;

namespace web_app {

enum class LaunchShimUpdateBehavior {
  DO_NOT_RECREATE,
  RECREATE_IF_INSTALLED,
  RECREATE_UNCONDITIONALLY,
};

// Callback type for LaunchShim. If |shim_process| is valid then the
// app shim was launched.
using ShimLaunchedCallback =
    base::OnceCallback<void(base::Process shim_process)>;

// Callback on termination takes no arguments.
using ShimTerminatedCallback = base::OnceClosure;

// Launch the shim specified by |shortcut_info|. Update the shim prior to launch
// if requested. Return in |launched_callback| the pid that was launched (or an
// invalid pid if none was launched). If |launched_callback| returns a valid
// pid, then |terminated_callback| will be called when that process terminates.
void LaunchShim(LaunchShimUpdateBehavior update_behavior,
                ShimLaunchedCallback launched_callback,
                ShimTerminatedCallback terminated_callback,
                std::unique_ptr<ShortcutInfo> shortcut_info);

// Launch the shim specified by `shim_path` as if the user launched it directly,
// except making sure that it connects to the currently running chrome or
// browser_test instance.
// If `urls` is not empty, the app is launched to handle those urls.
// Return in `launched_callback` the pid that was launched (or an invalid pid
// if none was launched). If `launched_callback` returns a valid pid, then
// `terminated_callback` will be called when that process terminates.
void LaunchShimForTesting(const base::FilePath& shim_path,
                          const std::vector<GURL>& urls,
                          ShimLaunchedCallback launched_callback,
                          ShimTerminatedCallback terminated_callback);

// Waits for the shim with the given `app_id` and `shim_path` to terminate. If
// there is no running application matching `app_id` and `shim_path` returns
// immediately.
void WaitForShimToQuitForTesting(const base::FilePath& shim_path,
                                 const std::string& app_id);

// Return true if launching and updating app shims will fail because of the
// testing environment.
bool AppShimLaunchDisabled();

// Returns a path to the Chrome Apps folder in ~/Applications.
base::FilePath GetChromeAppsFolder();

// Remove the specified app from the OS login item list.
void RemoveAppShimFromLoginItems(const std::string& app_id);

class WebAppAutoLoginUtil {
 public:
  WebAppAutoLoginUtil() = default;
  WebAppAutoLoginUtil(const WebAppAutoLoginUtil&) = delete;
  WebAppAutoLoginUtil& operator=(const WebAppAutoLoginUtil&) = delete;

  static WebAppAutoLoginUtil* GetInstance();

  static void SetInstanceForTesting(WebAppAutoLoginUtil* auto_login_util);

  // Adds the specified app to the list of login items.
  virtual void AddToLoginItems(const base::FilePath& app_bundle_path,
                               bool hide_on_startup);

  // Removes the specified app from the list of login items.
  virtual void RemoveFromLoginItems(const base::FilePath& app_bundle_path);

 protected:
  virtual ~WebAppAutoLoginUtil() = default;
};

// Creates a shortcut for a web application. The shortcut is a stub app
// that simply loads the browser framework and runs the given app.
class WebAppShortcutCreator {
 public:
  // Creates a new shortcut based on information in |shortcut_info|.
  // A copy of the shortcut is placed in |app_data_dir|.
  // |chrome_bundle_id| is the CFBundleIdentifier of the Chrome browser bundle.
  // Retains the pointer |shortcut_info|; the ShortcutInfo object must outlive
  // the WebAppShortcutCreator.
  WebAppShortcutCreator(const base::FilePath& app_data_dir,
                        const ShortcutInfo* shortcut_info);
  WebAppShortcutCreator(const WebAppShortcutCreator&) = delete;
  WebAppShortcutCreator& operator=(const WebAppShortcutCreator&) = delete;

  virtual ~WebAppShortcutCreator();

  // This allows UpdateAppShortcutsSubdirLocalizedName to be called multiple
  // times in a process, for unit tests.
  static void ResetHaveLocalizedAppDirNameForTesting();

  // Returns the base name for the shortcut. This will be a sanitized version
  // of the application title. If |copy_number| is not 1, then append it before
  // the .app part of the extension.
  virtual base::FilePath GetShortcutBasename(int copy_number = 1) const;

  // Returns the fallback name for the shortcut. This name will be a combination
  // of the profile name and extension id. This is used if the app title is
  // unable to be used for the bundle path (e.g: "...").
  base::FilePath GetFallbackBasename() const;

  // The full path to the app bundle under the relevant Applications folder.
  // If |avoid_conflicts| is true then return a path that does not yet exist (by
  // appending " 2", " 3", etc, to the end of the file name).
  base::FilePath GetApplicationsShortcutPath(bool avoid_conflicts) const;

  // Returns the paths to app bundles with the given id as found by launch
  // services, sorted by preference.
  std::vector<base::FilePath> GetAppBundlesById() const;

  bool CreateShortcuts(ShortcutCreationReason creation_reason,
                       ShortcutLocations creation_locations);

  // Recreate the shortcuts where they are found on disk and in the profile
  // path. If |create_if_needed| is true, then create the shortcuts if no
  // matching shortcuts are found on disk. Populate |updated_paths| with the
  // paths that were updated. Return false if no paths were updated or if there
  // exist paths that failed to update.
  bool UpdateShortcuts(bool create_if_needed,
                       std::vector<base::FilePath>* updated_paths);

  // Show the bundle we just generated in the Finder.
  virtual void RevealAppShimInFinder(const base::FilePath& app_path) const;

 protected:
  virtual std::vector<base::FilePath> GetAppBundlesByIdUnsorted() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebAppShortcutCreatorTest, DeleteShortcuts);
  FRIEND_TEST_ALL_PREFIXES(WebAppShortcutCreatorTest, UpdateIcon);
  FRIEND_TEST_ALL_PREFIXES(WebAppShortcutCreatorTest, UpdateShortcuts);
  FRIEND_TEST_ALL_PREFIXES(WebAppShortcutCreatorTest,
                           UpdateBookmarkAppShortcut);
  FRIEND_TEST_ALL_PREFIXES(WebAppShortcutCreatorTest,
                           UpdateShortcutsWithTitleChange);
  FRIEND_TEST_ALL_PREFIXES(WebAppShortcutCreatorTest,
                           NormalizeColonsInDisplayName);

  // Return true if the bundle for this app should be profile-agnostic.
  bool IsMultiProfile() const;

  // Copies the app loader template into a temporary directory and fills in all
  // relevant information. This works around a Finder bug where the app's icon
  // doesn't properly update.
  bool BuildShortcut(const base::FilePath& staging_path) const;

  // Builds a shortcut and copies it to the specified app paths. Populates
  // |updated_paths| with the paths that were successfully updated.
  void CreateShortcutsAt(const std::vector<base::FilePath>& app_paths,
                         std::vector<base::FilePath>* updated_paths) const;

  // Updates the InfoPlist.string inside |app_path| with the display name for
  // the app.
  bool UpdateDisplayName(const base::FilePath& app_path) const;

  // Updates the bundle id of the internal copy of the app shim bundle.
  bool UpdateInternalBundleIdentifier() const;

  // Updates the plist inside |app_path| with information about the app.
  bool UpdatePlist(const base::FilePath& app_path) const;

  // Updates the icon for the shortcut.
  bool UpdateIcon(const base::FilePath& app_path) const;

  // Path to the data directory for this app. For example:
  // ~/Library/Application Support/Chromium/Default/Web Applications/_crx_abc/
  const base::FilePath app_data_dir_;

  // Information about the app. Owned by the caller of the constructor.
  const raw_ptr<const ShortcutInfo> info_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_SHORTCUT_MAC_H_
