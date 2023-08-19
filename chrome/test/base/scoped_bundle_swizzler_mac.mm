// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/scoped_bundle_swizzler_mac.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_objc_class_swizzler.h"
#include "base/check.h"
#include "base/strings/sys_string_conversions.h"

static id __strong g_swizzled_main_bundle = nil;

// A donor class that provides a +[NSBundle mainBundle] method that can be
// swapped with NSBundle.
@interface TestBundle : NSProxy
- (instancetype)initWithRealBundle:(NSBundle*)bundle;
+ (NSBundle*)mainBundle;
@end

@implementation TestBundle {
  NSBundle* __strong _mainBundle;
  NSString* __strong _bundleID;
}

+ (NSBundle*)mainBundle {
  return g_swizzled_main_bundle;
}

- (instancetype)initWithRealBundle:(NSBundle*)bundle {
  _mainBundle = bundle;
  _bundleID = base::SysUTF8ToNSString(base::apple::BaseBundleID());
  return self;
}

- (NSString*)bundleIdentifier {
  return _bundleID;
}

- (void)forwardInvocation:(NSInvocation*)invocation {
  invocation.target = _mainBundle;
  [invocation invoke];
}

- (NSMethodSignature*)methodSignatureForSelector:(SEL)sel {
  return [_mainBundle methodSignatureForSelector:sel];
}

@end

ScopedBundleSwizzlerMac::ScopedBundleSwizzlerMac() {
  CHECK(!g_swizzled_main_bundle);

  NSBundle* original_main_bundle = NSBundle.mainBundle;
  g_swizzled_main_bundle =
      [[TestBundle alloc] initWithRealBundle:original_main_bundle];

  class_swizzler_ = std::make_unique<base::apple::ScopedObjCClassSwizzler>(
      [NSBundle class], [TestBundle class], @selector(mainBundle));
}

ScopedBundleSwizzlerMac::~ScopedBundleSwizzlerMac() {
  g_swizzled_main_bundle = nil;
}
