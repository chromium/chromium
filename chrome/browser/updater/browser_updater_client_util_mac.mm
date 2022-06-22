// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/browser_updater_client_util.h"

#include <Foundation/Foundation.h>
#import <OpenDirectory/OpenDirectory.h>

#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/chrome_version.h"
#include "chrome/updater/updater_scope.h"

namespace {

bool BundleOwnedByUser(uid_t user_uid) {
  const base::FilePath path = base::mac::OuterBundlePath();
  base::stat_wrapper_t stat_info = {};
  if (base::File::Lstat(path.value().c_str(), &stat_info) != 0) {
    VPLOG(2) << "Failed to get information on path " << path.value();
    return false;
  }

  if (S_ISLNK(stat_info.st_mode)) {
    VLOG(2) << "Path " << path.value() << " is a symbolic link.";
    return false;
  }

  return stat_info.st_uid == user_uid;
}

bool BundleOwnedByRoot() {
  return BundleOwnedByUser(0);
}

bool BundleOwnedByCurrentUser() {
  return BundleOwnedByUser(geteuid());
}

bool IsEffectiveUserAdmin() {
  NSError* error;
  ODNode* search_node = [ODNode nodeWithSession:[ODSession defaultSession]
                                           type:kODNodeTypeLocalNodes
                                          error:&error];
  if (!search_node) {
    VLOG(2) << "Error creating ODNode: " << search_node;
    return false;
  }
  ODQuery* query =
      [ODQuery queryWithNode:search_node
              forRecordTypes:kODRecordTypeUsers
                   attribute:kODAttributeTypeUniqueID
                   matchType:kODMatchEqualTo
                 queryValues:[NSString stringWithFormat:@"%d", geteuid()]
            returnAttributes:kODAttributeTypeStandardOnly
              maximumResults:1
                       error:&error];
  if (!query) {
    VLOG(2) << "Error constructing query: " << error;
    return false;
  }

  NSArray<ODRecord*>* results = [query resultsAllowingPartial:NO error:&error];
  if (!results) {
    VLOG(2) << "Error executing query: " << error;
    return false;
  }

  ODRecord* admin_group = [search_node recordWithRecordType:kODRecordTypeGroups
                                                       name:@"admin"
                                                 attributes:nil
                                                      error:&error];
  if (!admin_group) {
    VLOG(2) << "Failed to get 'admin' group: " << error;
    return false;
  }

  bool result = [admin_group isMemberRecord:results.firstObject error:&error];
  VLOG_IF(2, error) << "Failed to get member record: " << error;

  return result;
}

}  // namespace

base::FilePath GetUpdaterFolderName() {
  return base::FilePath(COMPANY_SHORTNAME_STRING).Append(kUpdaterName);
}

std::string CurrentlyInstalledVersion() {
  base::FilePath outer_bundle = base::mac::OuterBundlePath();
  base::FilePath plist_path =
      outer_bundle.Append("Contents").Append("Info.plist");
  NSDictionary* info_plist = [NSDictionary
      dictionaryWithContentsOfFile:base::mac::FilePathToNSString(plist_path)];
  return base::SysNSStringToUTF8(
      base::mac::ObjCCast<NSString>(info_plist[@"CFBundleShortVersionString"]));
}

base::FilePath GetUpdaterExecutablePath() {
  return base::FilePath(base::StrCat({kUpdaterName, ".app"}))
      .Append(FILE_PATH_LITERAL("Contents"))
      .Append(FILE_PATH_LITERAL("MacOS"))
      .Append(kUpdaterName);
}

bool CanInstallUpdater() {
  return BundleOwnedByCurrentUser() && geteuid() != 0;
}

updater::UpdaterScope GetUpdaterScope() {
  return BundleOwnedByRoot() ? updater::UpdaterScope::kSystem
                             : updater::UpdaterScope::kUser;
}

bool ShouldPromoteUpdater() {
  // 1) Should promote if browser is owned by root and not installed. The not
  // installed part of this case is handled in version_updater_mac.mm
  if (BundleOwnedByRoot())
    return true;

  // 2) If the effective user is root and the browser is not owned by root (i.e.
  // if the current user has run with sudo).
  if (geteuid() == 0)
    return true;

  // 3) If effective user is not the owner of the browser and is an
  // administrator.
  return !BundleOwnedByCurrentUser() && IsEffectiveUserAdmin();
}
