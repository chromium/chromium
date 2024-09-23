// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/web_applications/os_integration/mac/bundle_info_plist.h"

#import <Cocoa/Cocoa.h>

#include <list>
#include <string>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/version.h"
#include "chrome/common/chrome_paths.h"
#import "chrome/common/mac/app_mode_common.h"
#include "url/gurl.h"

namespace web_app {

// static
std::list<BundleInfoPlist> BundleInfoPlist::GetAllInPath(
    const base::FilePath& apps_path,
    bool recursive) {
  std::list<BundleInfoPlist> bundles;
  base::FileEnumerator enumerator(apps_path, recursive,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath shim_path = enumerator.Next(); !shim_path.empty();
       shim_path = enumerator.Next()) {
    bundles.emplace_back(shim_path);
  }
  return bundles;
}

// static
std::list<BundleInfoPlist> BundleInfoPlist::SearchForBundlesById(
    const std::string& bundle_id,
    const base::FilePath& apps_path) {
  std::list<BundleInfoPlist> infos;

  // First search using LaunchServices.
  NSArray* bundle_urls;
  if (@available(macOS 12.0, *)) {
    bundle_urls = [NSWorkspace.sharedWorkspace
        URLsForApplicationsWithBundleIdentifier:base::SysUTF8ToNSString(
                                                    bundle_id)];
  } else {
    bundle_urls = base::apple::CFToNSOwnershipCast(
        LSCopyApplicationURLsForBundleIdentifier(
            base::SysUTF8ToCFStringRef(bundle_id).get(), /*outError=*/nullptr));
  }
  for (NSURL* url in bundle_urls) {
    base::FilePath bundle_path = base::apple::NSURLToFilePath(url);
    BundleInfoPlist info(bundle_path);
    if (!info.IsForCurrentUserDataDir()) {
      continue;
    }
    infos.push_back(info);
  }
  if (!infos.empty()) {
    return infos;
  }

  // LaunchServices can fail to locate a recently-created bundle. Search
  // for an app in the applications folder to handle this case.
  // https://crbug.com/937703
  infos = GetAllInPath(apps_path,
                       /*recursive=*/true);
  for (auto it = infos.begin(); it != infos.end();) {
    const BundleInfoPlist& info = *it;
    if (info.GetBundleId() == bundle_id && info.IsForCurrentUserDataDir()) {
      ++it;
    } else {
      infos.erase(it++);
    }
  }
  return infos;
}

BundleInfoPlist::BundleInfoPlist(const base::FilePath& bundle_path)
    : bundle_path_(bundle_path) {
  plist_ = [NSDictionary dictionaryWithContentsOfURL:GetPlistURL(bundle_path_)
                                               error:nil];
}
BundleInfoPlist::BundleInfoPlist(const BundleInfoPlist& other) = default;
BundleInfoPlist& BundleInfoPlist::operator=(const BundleInfoPlist& other) =
    default;
BundleInfoPlist::~BundleInfoPlist() = default;

bool BundleInfoPlist::IsForCurrentUserDataDir() const {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(!user_data_dir.empty());
  return base::StartsWith(
      base::SysNSStringToUTF8(plist_[app_mode::kCrAppModeUserDataDirKey]),
      user_data_dir.value(), base::CompareCase::SENSITIVE);
}

bool BundleInfoPlist::IsForProfile(
    const base::FilePath& test_profile_path) const {
  std::string profile_path =
      base::SysNSStringToUTF8(plist_[app_mode::kCrAppModeProfileDirKey]);
  return profile_path == test_profile_path.BaseName().value();
}

base::FilePath BundleInfoPlist::GetFullProfilePath() const {
  // Figure out the profile_path. Since the user_data_dir could contain the
  // path to the web app data dir.
  base::FilePath user_data_dir = base::apple::NSStringToFilePath(
      plist_[app_mode::kCrAppModeUserDataDirKey]);
  base::FilePath profile_base_name = base::apple::NSStringToFilePath(
      plist_[app_mode::kCrAppModeProfileDirKey]);
  if (user_data_dir.DirName().DirName().BaseName() == profile_base_name) {
    return user_data_dir.DirName().DirName();
  }
  return user_data_dir.Append(profile_base_name);
}

std::string BundleInfoPlist::GetExtensionId() const {
  return base::SysNSStringToUTF8(plist_[app_mode::kCrAppModeShortcutIDKey]);
}

std::string BundleInfoPlist::GetProfileName() const {
  return base::SysNSStringToUTF8(plist_[app_mode::kCrAppModeProfileNameKey]);
}

GURL BundleInfoPlist::GetURL() const {
  return GURL(
      base::SysNSStringToUTF8(plist_[app_mode::kCrAppModeShortcutURLKey]));
}

std::u16string BundleInfoPlist::GetTitle() const {
  return base::SysNSStringToUTF16(plist_[app_mode::kCrAppModeShortcutNameKey]);
}

base::Version BundleInfoPlist::GetVersion() const {
  NSString* version_string = plist_[app_mode::kCrBundleVersionKey];
  if (!version_string) {
    // Older bundles have the Chrome version in the following key.
    version_string = plist_[app_mode::kCFBundleShortVersionStringKey];
  }
  return base::Version(base::SysNSStringToUTF8(version_string));
}

std::string BundleInfoPlist::GetBundleId() const {
  return base::SysNSStringToUTF8(
      plist_[base::apple::CFToNSPtrCast(kCFBundleIdentifierKey)]);
}

// static
NSURL* BundleInfoPlist::GetPlistURL(const base::FilePath& bundle_path) {
  return base::apple::FilePathToNSURL(
      bundle_path.Append("Contents").Append("Info.plist"));
}

}  // namespace web_app
