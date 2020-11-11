// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/browser_updater_client_util.h"

#include <Foundation/Foundation.h>

#include "base/files/file_path.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/chrome_version.h"

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
