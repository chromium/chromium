// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <SystemConfiguration/SystemConfiguration.h>
#include <stddef.h>

#include <string>

#include "base/apple/scoped_cftyperef.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/system/sys_info.h"

namespace syncer {

// Returns the Hardware model name, without trailing numbers, if possible. See
// https://everymac.com/systems/by_capability/mac-specs-by-machine-model-machine-id.html
// for example. If an error occurs trying to read the model, this simply returns
// "Unknown".
std::string GetPersonalizableDeviceNameInternal() {
  // Do not use NSHost currentHost, as it's very slow. http://crbug.com/138570
  SCDynamicStoreContext context = {0};
  base::apple::ScopedCFTypeRef<SCDynamicStoreRef> store(
      SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("chrome_sync"),
                           /*callout=*/nullptr, &context));
  base::apple::ScopedCFTypeRef<CFStringRef> machine_name(
      SCDynamicStoreCopyLocalHostName(store.get()));
  if (machine_name) {
    return base::SysCFStringRefToUTF8(machine_name.get());
  }

  // Fall back to get computer name.
  base::apple::ScopedCFTypeRef<CFStringRef> computer_name(
      SCDynamicStoreCopyComputerName(store.get(), /*nameEncoding=*/nullptr));
  if (computer_name) {
    return base::SysCFStringRefToUTF8(computer_name.get());
  }

  // If all else fails, return to using a slightly nicer version of the hardware
  // model. Warning: This will soon return just a useless "Mac" string.
  std::string model = base::SysInfo::HardwareModelName();
  std::optional<base::SysInfo::HardwareModelNameSplit> split =
      base::SysInfo::SplitHardwareModelNameDoNotUse(model);

  if (!split) {
    if (model.empty()) {
      return "Unknown";
    }
    return model;
  }

  return split.value().category;
}

}  // namespace syncer
