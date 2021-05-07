// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/mac/xpc_service_names.h"

#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_version.h"

namespace updater {

namespace {

const char kUpdateServiceInternalLaunchdPrefix[] =
    MAC_BUNDLE_IDENTIFIER_STRING ".update-internal.";
const char kUpdateServiceLaunchdName[] = MAC_BUNDLE_IDENTIFIER_STRING ".update";

}  // namespace

std::string GetUpdateServiceLaunchdName() {
  return kUpdateServiceLaunchdName;
}

std::string GetUpdateServiceInternalLaunchdName() {
  return base::StrCat({kUpdateServiceInternalLaunchdPrefix, kUpdaterVersion});
}

base::ScopedCFTypeRef<CFStringRef> CopyUpdateServiceLaunchdName() {
  return base::SysUTF8ToCFStringRef(kUpdateServiceLaunchdName);
}

base::ScopedCFTypeRef<CFStringRef> CopyWakeLaunchdName() {
  return base::SysUTF8ToCFStringRef(
      base::StrCat({MAC_BUNDLE_IDENTIFIER_STRING ".wake.", kUpdaterVersion}));
}

base::ScopedCFTypeRef<CFStringRef> CopyUpdateServiceInternalLaunchdName() {
  return base::SysUTF8ToCFStringRef(GetUpdateServiceInternalLaunchdName());
}

base::scoped_nsobject<NSString> GetUpdateServiceLaunchdLabel() {
  return base::scoped_nsobject<NSString>(
      base::mac::CFToNSCast(CopyUpdateServiceLaunchdName().release()));
}

base::scoped_nsobject<NSString> GetWakeLaunchdLabel() {
  return base::scoped_nsobject<NSString>(
      base::mac::CFToNSCast(CopyWakeLaunchdName().release()));
}

base::scoped_nsobject<NSString> GetUpdateServiceInternalLaunchdLabel() {
  return base::scoped_nsobject<NSString>(
      base::mac::CFToNSCast(CopyUpdateServiceInternalLaunchdName().release()));
}

base::scoped_nsobject<NSString> GetUpdateServiceMachName(
    base::scoped_nsobject<NSString> name) {
  return base::scoped_nsobject<NSString>(
      [name stringByAppendingString:@".mach"], base::scoped_policy::RETAIN);
}

base::scoped_nsobject<NSString> GetUpdateServiceMachName() {
  return GetUpdateServiceMachName(GetUpdateServiceLaunchdLabel());
}

base::scoped_nsobject<NSString> GetUpdateServiceInternalMachName() {
  return GetUpdateServiceMachName(GetUpdateServiceInternalLaunchdLabel());
}

}  // namespace updater
