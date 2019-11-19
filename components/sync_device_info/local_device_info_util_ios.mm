// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include <string>

#include "base/strings/sys_string_conversions.h"

namespace syncer {

std::string GetPersonalizableDeviceNameInternal() {
  return base::SysNSStringToUTF8([[UIDevice currentDevice] name]);
}

}  // namespace syncer
