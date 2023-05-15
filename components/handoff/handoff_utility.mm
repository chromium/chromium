// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/handoff/handoff_utility.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace handoff {

NSString* const kChromeHandoffActivityType = @"com.google.chrome.handoff";
NSString* const kOriginKey = @"kOriginKey";
NSString* const kOriginiOS = @"kOriginiOS";
NSString* const kOriginMac = @"kOriginMac";

Origin OriginFromString(NSString* string) {
  if ([string isEqualToString:kOriginiOS])
    return ORIGIN_IOS;

  if ([string isEqualToString:kOriginMac])
    return ORIGIN_MAC;

  return ORIGIN_UNKNOWN;
}

NSString* StringFromOrigin(Origin origin) {
  switch (origin) {
    case ORIGIN_IOS:
      return kOriginiOS;
    case ORIGIN_MAC:
      return kOriginMac;
    default:
      return nil;
  }
}

}  // namespace handoff
