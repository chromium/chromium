// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

using BrowserNativeWidgetMacInteractiveTest = InProcessBrowserTest;

// Tests that closing the browser immediately after toggling fullscreen doesn't
// crash and successfully closes the browser.
IN_PROC_BROWSER_TEST_F(BrowserNativeWidgetMacInteractiveTest,
                       CloseDuringFullscreenTransition) {
  // Ensure the window is active.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Trigger fullscreen. This starts the asynchronous transition.
  chrome::ToggleFullscreenMode(browser());

  // Immediately try to close the browser.
  // We use BrowserDestroyedObserver to wait for it to be fully destroyed.
  ui_test_utils::BrowserDestroyedObserver observer(browser());
  chrome::CloseWindow(browser());

  // Wait for the browser to be destroyed. If there is a crash or if it hangs,
  // the test will fail/timeout.
  observer.Wait();
}
