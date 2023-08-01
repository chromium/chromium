// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/ref_counted.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/clipboard/clipboard_util_mac.h"
#import "ui/base/cocoa/find_pasteboard.h"

// A subclass of FindPasteboard that doesn't write to the real find pasteboard.
@interface FindPasteboardTesting : FindPasteboard {
 @private
  scoped_refptr<ui::UniquePasteboard> _pasteboard;
}
- (NSPasteboard*)findPasteboard;

// These are for checking that pasteboard content is copied to/from the
// FindPasteboard correctly.
- (NSString*)findPasteboardText;
- (void)setFindPasteboardText:(NSString*)text;
@end

@implementation FindPasteboardTesting

- (NSPasteboard*)findPasteboard {
  // This method is called by the super class's -init, otherwise initialization
  // would go into this class's -init.
  if (!_pasteboard) {
    _pasteboard = new ui::UniquePasteboard;
  }
  return _pasteboard->get();
}

- (void)setFindPasteboardText:(NSString*)text {
  NSPasteboard* pasteboard = _pasteboard->get();
  [pasteboard clearContents];
  [pasteboard writeObjects:@[ text ]];
}

- (NSString*)findPasteboardText {
  NSArray* objects =
      [_pasteboard->get() readObjectsForClasses:@[ [NSString class] ]
                                        options:nil];
  return objects.firstObject;
}
@end

namespace {

class FindPasteboardTest : public CocoaTest {
 public:
  FindPasteboardTest() = default;

  void SetUp() override {
    CocoaTest::SetUp();
    pasteboard_ = [[FindPasteboardTesting alloc] init];
    ASSERT_TRUE(pasteboard_);
  }

  void TearDown() override {
    pasteboard_ = nil;
    CocoaTest::TearDown();
  }

 protected:
  FindPasteboardTesting* __strong pasteboard_;
};

TEST_F(FindPasteboardTest, SettingTextUpdatesPboard) {
  [pasteboard_ setFindText:@"text"];
  EXPECT_EQ(NSOrderedSame, [[pasteboard_ findPasteboardText] compare:@"text"]);
}

TEST_F(FindPasteboardTest, ReadingFromPboardUpdatesFindText) {
  [pasteboard_ setFindPasteboardText:@"text"];
  [pasteboard_ loadTextFromPasteboard:nil];
  EXPECT_EQ(NSOrderedSame, [[pasteboard_ findText] compare:@"text"]);
}

TEST_F(FindPasteboardTest, SendsNotificationWhenTextChanges) {
  __block int notification_count = 0;
  [NSNotificationCenter.defaultCenter
      addObserverForName:kFindPasteboardChangedNotification
                  object:pasteboard_
                   queue:nil
              usingBlock:^(NSNotification* note) {
                ++notification_count;
              }];
  EXPECT_EQ(0, notification_count);
  [pasteboard_ setFindText:@"text"];
  EXPECT_EQ(1, notification_count);
  [pasteboard_ setFindText:@"text"];
  EXPECT_EQ(1, notification_count);
  [pasteboard_ setFindText:@"other text"];
  EXPECT_EQ(2, notification_count);

  [pasteboard_ setFindPasteboardText:@"other text"];
  [pasteboard_ loadTextFromPasteboard:nil];
  EXPECT_EQ(2, notification_count);

  [pasteboard_ setFindPasteboardText:@"otherer text"];
  [pasteboard_ loadTextFromPasteboard:nil];
  EXPECT_EQ(3, notification_count);

  [NSNotificationCenter.defaultCenter removeObserver:pasteboard_];
}

}  // namespace
