// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/l10n_util.h"

#include "base/i18n/rtl.h"
#include "base/mac/mac_util.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"

namespace cocoa_l10n_util {

NSString* TooltipForURLAndTitle(NSString* url, NSString* title) {
  if ([title length] == 0)
    return url;
  else if ([url length] == 0 || [url isEqualToString:title])
    return title;
  else
    return [NSString stringWithFormat:@"%@\n%@", title, url];
}

void ApplyForcedRTL() {
  NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;

  // -registerDefaults: won't do the trick here because these defaults exist
  // (in the global domain) to reflect the system locale. They need to be set
  // in Chrome's domain to supersede the system value.
  switch (base::i18n::GetForcedTextDirection()) {
    case base::i18n::RIGHT_TO_LEFT:
      [defaults setBool:YES forKey:@"AppleTextDirection"];
      [defaults setBool:YES forKey:@"NSForceRightToLeftWritingDirection"];
      break;
    case base::i18n::LEFT_TO_RIGHT:
      [defaults setBool:YES forKey:@"AppleTextDirection"];
      [defaults setBool:NO forKey:@"NSForceRightToLeftWritingDirection"];
      break;
    default:
      [defaults removeObjectForKey:@"AppleTextDirection"];
      [defaults removeObjectForKey:@"NSForceRightToLeftWritingDirection"];
      break;
  }
}

}  // namespace cocoa_l10n_util
