// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"

#import <AppKit/AppKit.h>

#include "base/test/run_until.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/base_window.h"

// Tests that the body of an in-process browser test runs inside
// -[NSApplication run], like all UI code in production. AppKit behaves
// differently when NSApp is not running. See https://crbug.com/570104905.
using InProcessBrowserTestMacInteractiveTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(InProcessBrowserTestMacInteractiveTest,
                       TestBodyRunsInsideNSApplicationRun) {
  EXPECT_TRUE(NSApp.running);
}

// AppKit only picks a new key window when the key window is ordered out if
// NSApp is running.
IN_PROC_BROWSER_TEST_F(InProcessBrowserTestMacInteractiveTest,
                       ParentBecomesKeyWhenKeyChildIsOrderedOut) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  NSWindow* parent =
      browser()->GetWindow()->GetNativeWindow().GetNativeNSWindow();

  NSWindow* child =
      [[NSWindow alloc] initWithContentRect:NSMakeRect(100, 100, 300, 200)
                                  styleMask:NSWindowStyleMaskTitled
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  child.releasedWhenClosed = NO;
  [parent addChildWindow:child ordered:NSWindowAbove];
  [child makeKeyAndOrderFront:nil];
  ASSERT_TRUE(base::test::RunUntil([&] { return child.keyWindow; }));

  [child orderOut:nil];
  EXPECT_TRUE(parent.keyWindow);

  [child close];
}
