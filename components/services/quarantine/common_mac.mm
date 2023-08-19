// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/common_mac.h"

#import <ApplicationServices/ApplicationServices.h>
#include <Foundation/Foundation.h>

#include "base/apple/foundation_util.h"
#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"

namespace quarantine {

NSDictionary* GetQuarantineProperties(const base::FilePath& file) {
  NSURL* file_url = base::apple::FilePathToNSURL(file);
  if (!file_url) {
    return nil;
  }

  NSError* __autoreleasing error = nil;
  id __autoreleasing quarantine_properties = nil;
  BOOL success = [file_url getResourceValue:&quarantine_properties
                                     forKey:NSURLQuarantinePropertiesKey
                                      error:&error];
  if (!success) {
    std::string error_message(error ? error.description.UTF8String : "");
    LOG(WARNING) << "Unable to get quarantine attributes for file "
                 << file.value() << ". Error: " << error_message;
    return nil;
  }

  if (!quarantine_properties) {
    return @{};
  }

  NSDictionary* quarantine_properties_dict =
      base::apple::ObjCCast<NSDictionary>(quarantine_properties);
  if (!quarantine_properties_dict) {
    LOG(WARNING) << "Quarantine properties have wrong class: "
                 << base::SysNSStringToUTF8(
                        [[quarantine_properties class] description]);
    return nil;
  }

  return quarantine_properties_dict;
}

}  // namespace quarantine
