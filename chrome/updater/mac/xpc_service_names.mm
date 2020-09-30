// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/mac/xpc_service_names.h"

#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/updater/updater_version.h"

namespace updater {

const char kControlLaunchdName[] =
    MAC_BUNDLE_IDENTIFIER_STRING ".control." UPDATER_VERSION_STRING;

base::ScopedCFTypeRef<CFStringRef> CopyServiceLaunchdName() {
  return base::SysUTF8ToCFStringRef(MAC_BUNDLE_IDENTIFIER_STRING ".service");
}

base::ScopedCFTypeRef<CFStringRef> CopyWakeLaunchdName() {
  return base::SysUTF8ToCFStringRef(MAC_BUNDLE_IDENTIFIER_STRING
                                    ".wake." UPDATER_VERSION_STRING);
}

base::ScopedCFTypeRef<CFStringRef> CopyControlLaunchdName() {
  return base::SysUTF8ToCFStringRef(kControlLaunchdName);
}

base::scoped_nsobject<NSString> GetServiceLaunchdLabel() {
  return base::scoped_nsobject<NSString>(
      base::mac::CFToNSCast(CopyServiceLaunchdName().release()));
}

base::scoped_nsobject<NSString> GetWakeLaunchdLabel() {
  return base::scoped_nsobject<NSString>(
      base::mac::CFToNSCast(CopyWakeLaunchdName().release()));
}

base::scoped_nsobject<NSString> GetControlLaunchdLabel() {
  return base::scoped_nsobject<NSString>(
      base::mac::CFToNSCast(CopyControlLaunchdName().release()));
}

base::scoped_nsobject<NSString> GetServiceMachName(
    base::scoped_nsobject<NSString> name) {
  return base::scoped_nsobject<NSString>(
      [name stringByAppendingFormat:@".%lu", [name hash]],
      base::scoped_policy::RETAIN);
}

base::scoped_nsobject<NSString> GetServiceMachName() {
  return GetServiceMachName(GetServiceLaunchdLabel());
}

base::scoped_nsobject<NSString> GetVersionedServiceMachName() {
  base::scoped_nsobject<NSString> serviceLaunchdLabel(
      GetServiceLaunchdLabel(), base::scoped_policy::RETAIN);
  base::scoped_nsobject<NSString> updaterVersionString(
      base::SysUTF8ToNSString(UPDATER_VERSION_STRING),
      base::scoped_policy::RETAIN);

  base::scoped_nsobject<NSString> name(
      [NSString stringWithFormat:@"%@.%@", serviceLaunchdLabel.get(),
                                 updaterVersionString.get()],
      base::scoped_policy::RETAIN);
  return GetServiceMachName(name);
}

}  // namespace updater
