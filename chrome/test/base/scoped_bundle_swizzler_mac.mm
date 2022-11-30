// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/scoped_bundle_swizzler_mac.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/check.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/scoped_objc_class_swizzler.h"
#include "base/strings/sys_string_conversions.h"

static id g_swizzled_main_bundle = nil;

// A donor class that provides a +[NSBundle mainBundle] method that can be
// swapped with NSBundle.
@interface TestBundle : NSProxy
- (instancetype)initWithRealBundle:(NSBundle*)bundle;
+ (NSBundle*)mainBundle;
@end

@implementation TestBundle {
  base::scoped_nsobject<NSBundle> _mainBundle;
}

+ (NSBundle*)mainBundle {
  return g_swizzled_main_bundle;
}

- (instancetype)initWithRealBundle:(NSBundle*)bundle {
  _mainBundle.reset([bundle retain]);
  return self;
}

- (NSString*)bundleIdentifier {
  return base::SysUTF8ToNSString(base::mac::BaseBundleID());
}

- (void)forwardInvocation:(NSInvocation*)invocation {
  invocation.target = _mainBundle.get();
  [invocation invoke];
}

- (NSMethodSignature*)methodSignatureForSelector:(SEL)sel {
  return [_mainBundle methodSignatureForSelector:sel];
}

@end

ScopedBundleSwizzlerMac::ScopedBundleSwizzlerMac() {
  CHECK(!g_swizzled_main_bundle);

  NSBundle* original_main_bundle = [NSBundle mainBundle];
  g_swizzled_main_bundle =
      [[TestBundle alloc] initWithRealBundle:original_main_bundle];

  class_swizzler_ = std::make_unique<base::mac::ScopedObjCClassSwizzler>(
      [NSBundle class], [TestBundle class], @selector(mainBundle));
}

ScopedBundleSwizzlerMac::~ScopedBundleSwizzlerMac() {
  [g_swizzled_main_bundle release];
  g_swizzled_main_bundle = nil;
}
