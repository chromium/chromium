// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/common_mac.h"

#import <ApplicationServices/ApplicationServices.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"

namespace quarantine {

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

}  // namespace quarantine
