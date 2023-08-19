// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/history_overlay_controller.h"

#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"

class HistoryOverlayControllerTest : public CocoaTest {
 public:
  void SetUp() override {
    CocoaTest::SetUp();

    // The overlay controller shows the panel as a subview of the given view.
    test_view_ = [[NSView alloc] initWithFrame:NSMakeRect(10, 10, 10, 10)];

    // We add it to the test_window for authenticity.
    [[test_window() contentView] addSubview:test_view_];
  }

  NSView* test_view() {
    return test_view_;
  }

 private:
  NSView* __strong test_view_;
};

// Tests that the overlay view gets added and removed at the appropriate times.
TEST_F(HistoryOverlayControllerTest, DismissClearsAnimationsAndRemovesView) {
  EXPECT_EQ(0u, [[test_view() subviews] count]);

  HistoryOverlayController* controller =
      [[HistoryOverlayController alloc] initForMode:kHistoryOverlayModeBack];
  [controller showPanelForView:test_view()];
  EXPECT_EQ(1u, [[test_view() subviews] count]);

  // We expect the view to be removed from its superview immediately
  // after dismiss, even though it may still be animating a fade out.
  [controller dismiss];
  EXPECT_EQ(0u, [[test_view() subviews] count]);
}
