// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/enterprise_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/version.h"
#include "components/update_client/updater_state.h"

namespace update_client {

namespace {

const base::FilePath::CharType kKeystonePlist[] = FILE_PATH_LITERAL(
    "Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/"
    "Contents/Info.plist");

// Gets a value from the updater settings. Returns a retained object.
// T should be a toll-free Foundation framework type. See Apple's
// documentation for toll-free bridging.
template<class T>
base::scoped_nsobject<T> GetUpdaterSettingsValue(NSString* value_name) {
  CFStringRef app_id = CFSTR("com.google.Keystone.Agent");
  base::ScopedCFTypeRef<CFPropertyListRef> plist(
      CFPreferencesCopyAppValue(base::mac::NSToCFCast(value_name), app_id));
  return base::scoped_nsobject<T>(
      base::mac::ObjCCastStrict<T>(static_cast<id>(plist.get())),
      base::scoped_policy::RETAIN);
}

base::Time GetUpdaterSettingsTime(NSString* value_name) {
  base::scoped_nsobject<NSDate> date =
      GetUpdaterSettingsValue<NSDate>(value_name);
  base::Time result =
      base::Time::FromCFAbsoluteTime([date timeIntervalSinceReferenceDate]);

  return result;
}

base::Version GetVersionFromPlist(const base::FilePath& info_plist) {
  @autoreleasepool {
    NSData* data = [NSData
        dataWithContentsOfFile:base::mac::FilePathToNSString(info_plist)];
    if ([data length] == 0) {
      return base::Version();
    }
    NSDictionary* all_keys =
        base::mac::ObjCCastStrict<NSDictionary>([NSPropertyListSerialization
            propertyListWithData:data
                         options:NSPropertyListImmutable
                          format:nil
                           error:nil]);
    if (all_keys == nil) {
      return base::Version();
    }
    CFStringRef version = base::mac::GetValueFromDictionary<CFStringRef>(
        base::mac::NSToCFCast(all_keys), kCFBundleVersionKey);
    if (version == NULL) {
      return base::Version();
    }
    return base::Version(base::SysCFStringRefToUTF8(version));
  }
}

}  // namespace

std::string UpdaterState::GetUpdaterName() {
  return std::string("Keystone");
}

base::Version UpdaterState::GetUpdaterVersion(bool /*is_machine*/) {
  // System Keystone trumps user one, so check this one first
  base::FilePath local_library;
  bool success = base::mac::GetLocalDirectory(NSLibraryDirectory,
                                              &local_library);
  DCHECK(success);
  base::FilePath system_bundle_plist = local_library.Append(kKeystonePlist);
  base::Version system_keystone = GetVersionFromPlist(system_bundle_plist);
  if (system_keystone.IsValid()) {
    return system_keystone;
  }

  base::FilePath user_bundle_plist =
      base::mac::GetUserLibraryPath().Append(kKeystonePlist);
  return GetVersionFromPlist(user_bundle_plist);
}

base::Time UpdaterState::GetUpdaterLastStartedAU(bool /*is_machine*/) {
  return GetUpdaterSettingsTime(@"lastCheckStartDate");
}

base::Time UpdaterState::GetUpdaterLastChecked(bool /*is_machine*/) {
  return GetUpdaterSettingsTime(@"lastServerCheckDate");
}

bool UpdaterState::IsAutoupdateCheckEnabled() {
  // Auto-update check period override (in seconds).
  // Applies only to older versions of Keystone.
  base::scoped_nsobject<NSNumber> timeInterval =
      GetUpdaterSettingsValue<NSNumber>(@"checkInterval");
  if (!timeInterval.get()) return true;
  int value = [timeInterval intValue];

  return 0 < value && value < (24 * 60 * 60);
}

int UpdaterState::GetUpdatePolicy() {
  return -1;  // Keystone does not support update policies.
}

}  // namespace update_client
