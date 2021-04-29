// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/clipboard/clipboard_util_mac.h"
#import "ui/base/cocoa/find_pasteboard.h"

// A subclass of FindPasteboard that doesn't write to the real find pasteboard.
@interface FindPasteboardTesting : FindPasteboard {
 @public
  int _notificationCount;
 @private
  scoped_refptr<ui::UniquePasteboard> _pboard;
}
- (NSPasteboard*)findPboard;

- (void)callback:(id)sender;

// These are for checking that pasteboard content is copied to/from the
// FindPasteboard correctly.
- (NSString*)findPboardText;
- (void)setFindPboardText:(NSString*)text;
@end

@implementation FindPasteboardTesting

- (NSPasteboard*)findPboard {
  // This method is called by the super class's -init, otherwise initialization
  // would go into this class's -init.
  if (!_pboard)
    _pboard = new ui::UniquePasteboard;
  return _pboard->get();
}

- (void)callback:(id)sender {
  ++_notificationCount;
}

- (void)setFindPboardText:(NSString*)text {
  [_pboard->get() declareTypes:@[ NSStringPboardType ] owner:nil];
  [_pboard->get() setString:text forType:NSStringPboardType];
}

- (NSString*)findPboardText {
  return [_pboard->get() stringForType:NSStringPboardType];
}
@end

namespace {

class FindPasteboardTest : public CocoaTest {
 public:
  FindPasteboardTest() {}

  void SetUp() override {
    CocoaTest::SetUp();
    pboard_.reset([[FindPasteboardTesting alloc] init]);
    ASSERT_TRUE(pboard_.get());
  }

  void TearDown() override {
    pboard_.reset();
    CocoaTest::TearDown();
  }

 protected:
  base::scoped_nsobject<FindPasteboardTesting> pboard_;
};

TEST_F(FindPasteboardTest, SettingTextUpdatesPboard) {
  [pboard_.get() setFindText:@"text"];
  EXPECT_EQ(
      NSOrderedSame,
      [[pboard_.get() findPboardText] compare:@"text"]);
}

TEST_F(FindPasteboardTest, ReadingFromPboardUpdatesFindText) {
  [pboard_.get() setFindPboardText:@"text"];
  [pboard_.get() loadTextFromPasteboard:nil];
  EXPECT_EQ(
      NSOrderedSame,
      [[pboard_.get() findText] compare:@"text"]);
}

TEST_F(FindPasteboardTest, SendsNotificationWhenTextChanges) {
  [[NSNotificationCenter defaultCenter]
      addObserver:pboard_.get()
         selector:@selector(callback:)
             name:kFindPasteboardChangedNotification
           object:pboard_.get()];
  EXPECT_EQ(0, pboard_.get()->_notificationCount);
  [pboard_.get() setFindText:@"text"];
  EXPECT_EQ(1, pboard_.get()->_notificationCount);
  [pboard_.get() setFindText:@"text"];
  EXPECT_EQ(1, pboard_.get()->_notificationCount);
  [pboard_.get() setFindText:@"other text"];
  EXPECT_EQ(2, pboard_.get()->_notificationCount);

  [pboard_.get() setFindPboardText:@"other text"];
  [pboard_.get() loadTextFromPasteboard:nil];
  EXPECT_EQ(2, pboard_.get()->_notificationCount);

  [pboard_.get() setFindPboardText:@"otherer text"];
  [pboard_.get() loadTextFromPasteboard:nil];
  EXPECT_EQ(3, pboard_.get()->_notificationCount);

  [[NSNotificationCenter defaultCenter] removeObserver:pboard_.get()];
}


}  // namespace
