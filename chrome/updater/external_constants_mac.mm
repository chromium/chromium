// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/external_constants.h"

#import <Foundation/Foundation.h>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants_impl.h"
#include "url/gurl.h"

#include "base/logging.h"

namespace updater {

std::vector<GURL> DevOverrideProvider::UpdateURL() const {
  @autoreleasepool {
    NSUserDefaults* userDefaults = [[NSUserDefaults alloc]
        initWithSuiteName:[NSString
                              stringWithUTF8String:kUserDefaultsSuiteName]];
    NSURL* url = [userDefaults
        URLForKey:[NSString stringWithUTF8String:kDevOverrideKeyUrl]];
    if (url == nil)
      return next_provider_->UpdateURL();
    return {GURL(base::SysNSStringToUTF8([url absoluteString]))};
  }
}

bool DevOverrideProvider::UseCUP() const {
  @autoreleasepool {
    NSUserDefaults* userDefaults = [[NSUserDefaults alloc]
        initWithSuiteName:[NSString
                              stringWithUTF8String:kUserDefaultsSuiteName]];
    id use_cup = [userDefaults
        objectForKey:[NSString stringWithUTF8String:kDevOverrideKeyUseCUP]];
    if (use_cup)
      return [use_cup boolValue];
    return next_provider_->UseCUP();
  }
}

}  // namespace updater
