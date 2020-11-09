// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#import "testing/gtest_mac.h"

using WebContentsViewMacInteractiveTest = InProcessBrowserTest;

// Integration test for a <select> popup run by the Mac-specific
// content::PopupMenuHelper, owned by a WebContentsViewMac.
IN_PROC_BROWSER_TEST_F(WebContentsViewMacInteractiveTest, SelectMenuLifetime) {
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  EXPECT_TRUE(embedded_test_server()->Start());

  // Open a tab with a <select> element, which starts focused.
  AddTabAtIndex(1, GURL(embedded_test_server()->GetURL("/select.html")),
                ui::PAGE_TRANSITION_LINK);

  base::RunLoop outer_run_loop;
  base::RunLoop* outer_run_loop_for_block = &outer_run_loop;
  __block base::scoped_nsobject<NSString> first_item;

  // Wait for a native menu to open.
  id token = [[NSNotificationCenter defaultCenter]
      addObserverForName:NSMenuDidBeginTrackingNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* notification) {
                first_item.reset(
                    [[[[notification object] itemAtIndex:0] title] copy]);
                // We can't cancel tracking until after
                // NSMenuDidBeginTrackingNotification is processed (i.e. after
                // this block returns). So post a task to run on the inner run
                // loop which will close the tab (and cancel tracking in
                // ~PopupMenuHelper()) and quit the outer run loop to continue
                // the test.
                base::ThreadTaskRunnerHandle::Get()->PostTask(
                    FROM_HERE, base::BindLambdaForTesting([&] {
                      browser()->tab_strip_model()->CloseWebContentsAt(1, 0);
                      outer_run_loop_for_block->Quit();
                    }));
              }];

  // Send a space key to open the <select>.
  content::NativeWebKeyboardEvent event(
      blink::WebKeyboardEvent::Type::kChar, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.text[0] = ' ';
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetRenderWidgetHostView()
      ->GetRenderWidgetHost()
      ->ForwardKeyboardEvent(event);

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  outer_run_loop.Run();

  [[NSNotificationCenter defaultCenter] removeObserver:token];

  EXPECT_NSEQ(@"Apple", first_item);  // Was it the menu we expected?
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
}
