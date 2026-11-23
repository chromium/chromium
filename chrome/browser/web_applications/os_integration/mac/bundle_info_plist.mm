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
#include "base/strings/sys_string_conversions.h"
#include "base/version.h"
#include "chrome/common/chrome_paths.h"
#import "chrome/common/mac/app_mode_common.h"
#include "url/gurl.h"

namespace web_app {

namespace {

// Returns the value for `key` in `plist`, or nil if the value is missing or is
// not a string. `Info.plist` files are on disk and can hold any type.
NSString* GetPlistString(NSDictionary* plist, NSString* key) {
  return base::apple::ObjCCast<NSString>(plist[key]);
}

}  // namespace

struct BundleInfoPlist::ObjCStorage {
  NSDictionary* __strong plist;
};

// static
std::list<BundleInfoPlist> BundleInfoPlist::GetAllInPath(
    const base::FilePath& apps_path,
    bool recursive) {
  if (apps_path.empty()) {
    return {};
  }
  std::list<BundleInfoPlist> bundles;
  base::FileEnumerator enumerator(apps_path, recursive,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath shim_path = enumerator.Next(); !shim_path.empty();
       shim_path = enumerator.Next()) {
    // Skip directories that are not bundles, such as `.localized` and the
    // directories inside each `.app`, so that their `Info.plist` is not read.
    // The extension check is case-insensitive.
    if (!shim_path.MatchesFinalExtension(FILE_PATH_LITERAL(".app"))) {
      continue;
    }
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
  NSArray* bundle_urls = [NSWorkspace.sharedWorkspace
      URLsForApplicationsWithBundleIdentifier:base::SysUTF8ToNSString(
                                                  bundle_id)];
  for (NSURL* url in bundle_urls) {
    base::FilePath bundle_path = base::apple::NSURLToFilePath(url);
    // Skip Trash before the constructor reads `Info.plist`, because macOS can
    // deny reads inside `~/.Trash`.
    if (IsPathInTrash(bundle_path)) {
      continue;
    }
    BundleInfoPlist info(bundle_path);
    if (!info.IsForCurrentUserDataDir()) {
      continue;
    }
    infos.push_back(std::move(info));
  }
  if (!infos.empty()) {
    return infos;
  }

  // LaunchServices can fail to locate a recently-created bundle. Search
  // for an app in the applications folder to handle this case.
  // https://crbug.com/41444624
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

// static
bool BundleInfoPlist::IsPathInTrash(const base::FilePath& bundle_path) {
  if (bundle_path.empty()) {
    return false;
  }
  for (const base::FilePath::StringType& component :
       bundle_path.GetComponents()) {
    if (component == FILE_PATH_LITERAL(".Trash") ||
        component == FILE_PATH_LITERAL(".Trashes")) {
      return true;
    }
  }
  NSURL* bundle_url = base::apple::FilePathToNSURL(bundle_path);
  if (!bundle_url) {
    return false;
  }
  // A domain of 0 makes Foundation use the Trash of the volume that holds
  // `bundle_url`. `NSAllDomainsMask` is not valid for `NSTrashDirectory`.
  NSURLRelationship relationship = NSURLRelationshipOther;
  return [NSFileManager.defaultManager
             getRelationship:&relationship
                 ofDirectory:NSTrashDirectory
                    inDomain:static_cast<NSSearchPathDomainMask>(0)
                 toItemAtURL:bundle_url
                       error:nil] &&
         relationship == NSURLRelationshipContains;
}

BundleInfoPlist::BundleInfoPlist(const base::FilePath& bundle_path)
    : bundle_path_(bundle_path),
      objc_storage_(std::make_unique<ObjCStorage>()) {
  objc_storage_->plist =
      [NSDictionary dictionaryWithContentsOfURL:GetPlistURL(bundle_path)
                                          error:nil];
}

BundleInfoPlist::BundleInfoPlist(BundleInfoPlist&& other) = default;
BundleInfoPlist& BundleInfoPlist::operator=(BundleInfoPlist&& other) = default;
BundleInfoPlist::~BundleInfoPlist() = default;

bool BundleInfoPlist::IsForCurrentUserDataDir() const {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  CHECK(!user_data_dir.empty(), base::NotFatalUntil::M162);
  user_data_dir = user_data_dir.StripTrailingSeparators();
  base::FilePath plist_user_data_dir = base::apple::NSStringToFilePath(
      GetPlistString(objc_storage_->plist, app_mode::kCrAppModeUserDataDirKey));
  plist_user_data_dir = plist_user_data_dir.StripTrailingSeparators();
  if (plist_user_data_dir.empty()) {
    return false;
  }
  // `IsParent()` compares path components and does not resolve "..", so
  // "<user_data_dir>/../Chrome Beta" would otherwise match.
  if (plist_user_data_dir.ReferencesParent()) {
    return false;
  }
  // Chrome writes the app data dir to this key, not the user data dir:
  // "<user_data_dir>/<profile dir or '-'>/Web Applications/_crx_<app_id>".
  // The app shim removes the last three components to get the user data dir
  // (see app_mode_loader_mac.mm), so a valid value is inside
  // `user_data_dir`. An exact match is also accepted, as the old
  // `StartsWith()` check did.
  return plist_user_data_dir == user_data_dir ||
         user_data_dir.IsParent(plist_user_data_dir);
}

bool BundleInfoPlist::IsForProfile(
    const base::FilePath& test_profile_path) const {
  std::string profile_path = base::SysNSStringToUTF8(
      GetPlistString(objc_storage_->plist, app_mode::kCrAppModeProfileDirKey));
  return profile_path == test_profile_path.BaseName().value();
}

std::string BundleInfoPlist::GetExtensionId() const {
  return base::SysNSStringToUTF8(
      GetPlistString(objc_storage_->plist, app_mode::kCrAppModeShortcutIDKey));
}

std::string BundleInfoPlist::GetProfileName() const {
  return base::SysNSStringToUTF8(
      GetPlistString(objc_storage_->plist, app_mode::kCrAppModeProfileNameKey));
}

GURL BundleInfoPlist::GetURL() const {
  return GURL(base::SysNSStringToUTF8(GetPlistString(
      objc_storage_->plist, app_mode::kCrAppModeShortcutURLKey)));
}

std::u16string BundleInfoPlist::GetTitle() const {
  return base::SysNSStringToUTF16(GetPlistString(
      objc_storage_->plist, app_mode::kCrAppModeShortcutNameKey));
}

base::Version BundleInfoPlist::GetVersion() const {
  NSString* version_string =
      GetPlistString(objc_storage_->plist, app_mode::kCrBundleVersionKey);
  if (!version_string) {
    // Older bundles have the Chrome version in the following key.
    version_string = GetPlistString(objc_storage_->plist,
                                    app_mode::kCFBundleShortVersionStringKey);
  }
  return base::Version(base::SysNSStringToUTF8(version_string));
}

std::string BundleInfoPlist::GetBundleId() const {
  return base::SysNSStringToUTF8(
      GetPlistString(objc_storage_->plist,
                     base::apple::CFToNSPtrCast(kCFBundleIdentifierKey)));
}

// static
NSURL* BundleInfoPlist::GetPlistURL(const base::FilePath& bundle_path) {
  return base::apple::FilePathToNSURL(
      bundle_path.Append("Contents").Append("Info.plist"));
}

}  // namespace web_app
