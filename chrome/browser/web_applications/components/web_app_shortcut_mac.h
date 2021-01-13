// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SHORTCUT_MAC_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SHORTCUT_MAC_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/process/process.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"

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

std::unique_ptr<ShortcutInfo> RecordAppShimErrorAndBuildShortcutInfo(
    const base::FilePath& bundle_path);

// Return true if launching and updating app shims will fail because of the
// testing environment.
bool AppShimLaunchDisabled();

// Returns a path to the Chrome Apps folder in ~/Applications.
base::FilePath GetChromeAppsFolder();

// Testing method to override calls to GetChromeAppsFolder.
void SetChromeAppsFolderForTesting(const base::FilePath& path);

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
  const ShortcutInfo* const info_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SHORTCUT_MAC_H_
