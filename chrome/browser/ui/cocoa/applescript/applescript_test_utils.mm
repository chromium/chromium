// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/applescript_test_utils.h"

#include <optional>

#include "base/apple/scoped_objc_class_swizzler.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"

// Represents the current fake command that is executing.
static FakeScriptCommand* gFakeCurrentCommand;

@implementation FakeScriptCommand {
  std::optional<base::apple::ScopedObjCClassSwizzler> swizzler;
}

- (instancetype)init {
  if ((self = [super init])) {
    swizzler.emplace([NSScriptCommand class], [FakeScriptCommand class],
                     @selector(currentCommand));
    gFakeCurrentCommand = self;
  }
  return self;
}

+ (NSScriptCommand*)currentCommand {
  return gFakeCurrentCommand;
}

- (void)dealloc {
  swizzler.reset();
  gFakeCurrentCommand = nil;
}

@end
