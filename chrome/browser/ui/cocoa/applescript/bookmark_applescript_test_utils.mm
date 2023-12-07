// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/bookmark_applescript_test_utils.h"

#include <optional>

#include "base/apple/scoped_objc_class_swizzler.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/ui/browser.h"
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

BookmarkAppleScriptTest::BookmarkAppleScriptTest() = default;

BookmarkAppleScriptTest::~BookmarkAppleScriptTest() = default;

void BookmarkAppleScriptTest::SetUpOnMainThread() {
  ASSERT_TRUE(profile());

  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile());
  const bookmarks::BookmarkNode* root = model->bookmark_bar_node();
  const std::string model_string("a f1:[ b d c ] d f2:[ e f g ] h ");
  bookmarks::test::AddNodesFromModelString(model, root, model_string);
  bookmark_bar_ = [[BookmarkFolderAppleScript alloc]
      initWithBookmarkNode:model->bookmark_bar_node()];
}

Profile* BookmarkAppleScriptTest::profile() const {
  return browser()->profile();
}
