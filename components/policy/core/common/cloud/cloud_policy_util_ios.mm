// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/policy/core/common/cloud/cloud_policy_util_ios.h"

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"

namespace policy {

std::string GetIOSManagedDeviceName() {
  NSDictionary* managed_config = [[NSUserDefaults standardUserDefaults]
      dictionaryForKey:kPolicyLoaderIOSConfigurationKey];
  // The value is controlled by the MDM administrator and may be of any
  // property list type, so don't assume a string.
  NSString* device_name = base::apple::ObjCCast<NSString>(
      managed_config[kPolicyLoaderIOSDeviceNameKey]);
  if (device_name && device_name.length > 0) {
    return base::SysNSStringToUTF8(device_name);
  }
  return std::string();
}

}  // namespace policy
