// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_WEB_APP_SHORTCUT_CREATOR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_WEB_APP_SHORTCUT_CREATOR_H_

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"

namespace web_app {

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
                        const base::FilePath& chrome_apps_dir,
                        const ShortcutInfo* shortcut_info,
                        bool use_ad_hoc_signing_for_web_app_shims);
  WebAppShortcutCreator(const WebAppShortcutCreator&) = delete;
  WebAppShortcutCreator& operator=(const WebAppShortcutCreator&) = delete;

  virtual ~WebAppShortcutCreator();

  // Returns the base name for the shortcut. This will be a sanitized version
  // of the application title.
  virtual base::FilePath GetShortcutBasename() const;

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

  std::string GetAppBundleId() const;

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

  // Return true if ad-hoc signing should be used for web app shims.
  bool UseAdHocSigningForWebAppShims() const;

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

  // Updates the code signature of |app_path|.
  bool UpdateSignature(const base::FilePath& app_path) const;

  // Path to the data directory for this app. For example:
  // ~/Library/Application Support/Chromium/Default/Web Applications/_crx_abc/
  const base::FilePath app_data_dir_;

  // Path to the directory where shortcuts are installed. For example:
  // ~/Applications/Chrome Apps/
  const base::FilePath chrome_apps_dir_;

  // Information about the app. Owned by the caller of the constructor.
  const raw_ptr<const ShortcutInfo> info_;

  const bool use_ad_hoc_signing_for_web_app_shims_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_WEB_APP_SHORTCUT_CREATOR_H_
