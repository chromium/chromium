// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_BUNDLE_INFO_PLIST_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_BUNDLE_INFO_PLIST_H_

#include <list>
#include <string>

#include "base/files/file_path.h"

@class NSDictionary;
@class NSURL;

namespace base {
class Version;
}
class GURL;

namespace web_app {

// Data and helpers for an Info.plist under a given bundle path.
class BundleInfoPlist {
 public:
  // Retrieve info from all app shims found in `apps_path`.
  static std::list<BundleInfoPlist> GetAllInPath(
      const base::FilePath& apps_path,
      bool recursive);

  // Return all bundles with the specified `bundle_id` which are for the current
  // user data dir, also including all app shims found in `apps_path`.
  static std::list<BundleInfoPlist> SearchForBundlesById(
      const std::string& bundle_id,
      const base::FilePath& apps_path);

  // Retrieve info from the specified app shim in `bundle_path`.
  explicit BundleInfoPlist(const base::FilePath& bundle_path);
  BundleInfoPlist(const BundleInfoPlist& other);
  BundleInfoPlist& operator=(const BundleInfoPlist& other);
  ~BundleInfoPlist();

  const base::FilePath& bundle_path() const { return bundle_path_; }

  // Checks that the CrAppModeUserDataDir in the Info.plist is, or is a subpath
  // of the current user_data_dir. This uses starts with instead of equals
  // because the CrAppModeUserDataDir could be the user_data_dir or the
  // app data dir (which would be a subdirectory of the user_data_dir).
  bool IsForCurrentUserDataDir() const;

  // Checks if kCrAppModeProfileDirKey corresponds to the specified profile
  // path.
  bool IsForProfile(const base::FilePath& test_profile_path) const;

  // Return the full profile path (as a subpath of the user_data_dir).
  base::FilePath GetFullProfilePath() const;

  std::string GetExtensionId() const;
  std::string GetProfileName() const;
  GURL GetURL() const;
  std::u16string GetTitle() const;
  base::Version GetVersion() const;
  std::string GetBundleId() const;

  // Given the path to an app bundle, return the URL of the Info.plist file.
  static NSURL* GetPlistURL(const base::FilePath& bundle_path);

 private:
  // The path of the app bundle from this this info was read.
  base::FilePath bundle_path_;

  // Data read from the Info.plist.
  NSDictionary* __strong plist_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_BUNDLE_INFO_PLIST_H_
