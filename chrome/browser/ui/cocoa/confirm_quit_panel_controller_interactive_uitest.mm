// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/test/run_until.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/confirm_quit.h"
#import "chrome/browser/ui/cocoa/confirm_quit_panel_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

namespace {
using ConfirmQuitControllerPanelInteractiveUITest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(ConfirmQuitControllerPanelInteractiveUITest, Cancel) {
  NSWindow* window = browser()->window()->GetNativeWindow().GetNativeNSWindow();
  [[ConfirmQuitPanelController sharedController] simulateQuitForTesting];
  ASSERT_EQ(window.alphaValue, 0.0);
  [[ConfirmQuitPanelController sharedController] cancel];
  base::TimeTicks now = base::TimeTicks::Now();
  // Gross, but if we mock this part out, what are even testing?
  ASSERT_TRUE(base::test::RunUntil([=] {
    return (base::TimeTicks::Now() - now) > confirm_quit::kWindowFadeDuration;
  }));
  ASSERT_EQ(window.alphaValue, 1.0);
}

}  // namespace
