// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HANDOFF_HANDOFF_UTILITY_H_
#define COMPONENTS_HANDOFF_HANDOFF_UTILITY_H_

#import <Foundation/Foundation.h>

namespace handoff {

// The activity type that Chrome uses to pass a Handoff to itself.
extern NSString* const kChromeHandoffActivityType;

// The value of this key in the userInfo dictionary of an NSUserActivity
// indicates the origin. The value should not be used for any privacy or
// security sensitive operations, since any application can set the key/value
// pair.
extern NSString* const kOriginKey;

// This value indicates that an NSUserActivity originated from Chrome on iOS.
extern NSString* const kOriginiOS;

// This value indicates that an NSUserActivity originated from Chrome on Mac.
extern NSString* const kOriginMac;

// Used for UMA metrics.
enum Origin {
  ORIGIN_UNKNOWN = 0,
  ORIGIN_IOS = 1,
  ORIGIN_MAC = 2,
  ORIGIN_COUNT
};

// Returns ORIGIN_UNKNOWN if |string| is nil or unrecognized.
Origin OriginFromString(NSString* string);

// Returns nil if |origin| is not ORIGIN_IOS or ORIGIN_MAC.
NSString* StringFromOrigin(Origin origin);

}  // namespace handoff

#endif  // COMPONENTS_HANDOFF_HANDOFF_UTILITY_H_
