// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/quarantine/common_mac.h"

#import <ApplicationServices/ApplicationServices.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/availability.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"

namespace download {

// Once Chrome no longer supports macOS 10.9, this code will no longer be
// necessary. Note that LSCopyItemAttribute was deprecated in macOS 10.8, but
// the replacement to kLSItemQuarantineProperties did not exist until macOS
// 10.10.
#if !defined(MAC_OS_X_VERSION_10_10) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_10
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
bool GetQuarantinePropertiesDeprecated(
    const base::FilePath& file,
    base::scoped_nsobject<NSMutableDictionary>* properties) {
  const UInt8* path = reinterpret_cast<const UInt8*>(file.value().c_str());
  FSRef file_ref;
  if (FSPathMakeRef(path, &file_ref, nullptr) != noErr)
    return false;

  base::ScopedCFTypeRef<CFTypeRef> quarantine_properties;
  OSStatus status =
      LSCopyItemAttribute(&file_ref, kLSRolesAll, kLSItemQuarantineProperties,
                          quarantine_properties.InitializeInto());
  if (status != noErr)
    return true;

  CFDictionaryRef quarantine_properties_dict =
      base::mac::CFCast<CFDictionaryRef>(quarantine_properties.get());
  if (!quarantine_properties_dict) {
    LOG(WARNING) << "kLSItemQuarantineProperties is not a dictionary on file "
                 << file.value();
    return false;
  }

  properties->reset(
      [base::mac::CFToNSCast(quarantine_properties_dict) mutableCopy]);
  return true;
}

#pragma clang diagnostic pop
#endif

API_AVAILABLE(macos(10.10))
bool GetQuarantineProperties(
    const base::FilePath& file,
    base::scoped_nsobject<NSMutableDictionary>* properties) {
  base::scoped_nsobject<NSURL> file_url([[NSURL alloc]
      initFileURLWithPath:base::SysUTF8ToNSString(file.value())]);
  if (!file_url)
    return false;

  NSError* error = nil;
  id quarantine_properties = nil;
  BOOL success = [file_url getResourceValue:&quarantine_properties
                                     forKey:NSURLQuarantinePropertiesKey
                                      error:&error];
  if (!success) {
    std::string error_message(error ? error.description.UTF8String : "");
    LOG(WARNING) << "Unable to get quarantine attributes for file "
                 << file.value() << ". Error: " << error_message;
    return false;
  }

  if (!quarantine_properties)
    return true;

  NSDictionary* quarantine_properties_dict =
      base::mac::ObjCCast<NSDictionary>(quarantine_properties);
  if (!quarantine_properties_dict) {
    LOG(WARNING) << "Quarantine properties have wrong class: "
                 << base::SysNSStringToUTF8(
                        [[quarantine_properties class] description]);
    return false;
  }

  properties->reset([quarantine_properties_dict mutableCopy]);
  return true;
}

}  // namespace download
