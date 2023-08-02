// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <SystemConfiguration/SystemConfiguration.h>
#include <stddef.h>
#include <sys/sysctl.h>

#include <string>

#include "base/mac/scoped_cftyperef.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"

namespace syncer {

// Returns the Hardware model name, without trailing numbers, if
// possible.  See http://www.cocoadev.com/index.pl?MacintoshModels for
// an example list of models. If an error occurs trying to read the
// model, this simply returns "Unknown".
std::string GetPersonalizableDeviceNameInternal() {
  // Do not use NSHost currentHost, as it's very slow. http://crbug.com/138570
  SCDynamicStoreContext context = {0};
  base::ScopedCFTypeRef<SCDynamicStoreRef> store(
      SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("chrome_sync"),
                           /*callout=*/nullptr, &context));
  base::ScopedCFTypeRef<CFStringRef> machine_name(
      SCDynamicStoreCopyLocalHostName(store));
  if (machine_name) {
    return base::SysCFStringRefToUTF8(machine_name);
  }

  // Fall back to get computer name.
  base::ScopedCFTypeRef<CFStringRef> computer_name(
      SCDynamicStoreCopyComputerName(store, /*nameEncoding=*/nullptr));
  if (computer_name) {
    return base::SysCFStringRefToUTF8(computer_name);
  }

  // If all else fails, return to using a slightly nicer version of the
  // hardware model.
  char model_buffer[256];
  size_t length = sizeof(model_buffer);
  if (!sysctlbyname("hw.model", model_buffer, &length, nullptr, 0)) {
    for (size_t i = 0; i < length; i++) {
      if (base::IsAsciiDigit(model_buffer[i])) {
        return std::string(model_buffer, 0, i);
      }
    }
    return std::string(model_buffer, 0, length);
  }
  return "Unknown";
}

}  // namespace syncer
