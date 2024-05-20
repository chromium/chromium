// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/confirm_quit_panel_controller.h"

#include "chrome/browser/ui/cocoa/confirm_quit.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "testing/gtest_mac.h"

namespace {

using ConfirmQuitPanelControllerTest = CocoaTest;

TEST_F(ConfirmQuitPanelControllerTest, ShowAndDismiss) {
  ConfirmQuitPanelController* controller =
      ConfirmQuitPanelController.sharedController;
  // Test singleton.
  EXPECT_EQ(controller, ConfirmQuitPanelController.sharedController);
  [controller showWindow:nil];
  [controller dismissPanel];  // Releases self.
  // The controller should still be the singleton instance until after the
  // animation runs and the window closes. That will happen after this test body
  // finishes executing.
  EXPECT_EQ(controller, ConfirmQuitPanelController.sharedController);
}

}  // namespace
