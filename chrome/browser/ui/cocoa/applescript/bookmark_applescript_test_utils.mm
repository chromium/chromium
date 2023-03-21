// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/bookmark_applescript_test_utils.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"

// Represents the current fake command that is executing.
static FakeScriptCommand* kFakeCurrentCommand;

@implementation FakeScriptCommand

- (instancetype)init {
  if ((self = [super init])) {
    _originalMethod = class_getClassMethod([NSScriptCommand class],
                                           @selector(currentCommand));
    _alternateMethod =
        class_getClassMethod([self class], @selector(currentCommand));
    method_exchangeImplementations(_originalMethod, _alternateMethod);
    kFakeCurrentCommand = self;
  }
  return self;
}

+ (NSScriptCommand*)currentCommand {
  return kFakeCurrentCommand;
}

- (void)dealloc {
  method_exchangeImplementations(_originalMethod, _alternateMethod);
  kFakeCurrentCommand = nil;
  [super dealloc];
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
  bookmark_bar_.reset([[BookmarkFolderAppleScript alloc]
      initWithBookmarkNode:model->bookmark_bar_node()]);
}

Profile* BookmarkAppleScriptTest::profile() const {
  return browser()->profile();
}
