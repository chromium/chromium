// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin_view_controller.h"
#include "chrome/browser/ui/views/profiles/signin_view_controller_delegate_views.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

class SigninViewControllerDelegateViewsBrowserTest
    : public InProcessBrowserTest {
 public:
  SigninViewControllerDelegateViewsBrowserTest() = default;
  ~SigninViewControllerDelegateViewsBrowserTest() override = default;

  // The delegate deletes itself when closed.
  SigninViewControllerDelegateViews* CreateDelegate(bool show_immediately) {
    return new SigninViewControllerDelegateViews(
        SigninViewControllerDelegateViews::CreateSyncConfirmationWebView(
            browser()),
        browser(), ui::MODAL_TYPE_WINDOW, /*wait_for_size=*/!show_immediately,
        false);
  }

  // Closes the dialog and checks that the web contents were not leaked.
  void CloseDialog(SigninViewControllerDelegateViews* delegate) {
    content::WebContentsDestroyedWatcher watcher(delegate->GetWebContents());
    delegate->CloseModalSignin();
    watcher.Wait();
    EXPECT_TRUE(watcher.IsDestroyed());
  }
};

// Opens and closes the dialog immediately. This is a basic test at the
// `SigninViewController` API level, but does not test all the edge cases.
// Regression test for https://crbug.com/1233030.
IN_PROC_BROWSER_TEST_F(SigninViewControllerDelegateViewsBrowserTest,
                       CloseImmediately) {
  SigninViewController* controller = browser()->signin_view_controller();
  controller->ShowModalSyncConfirmationDialog();
  content::WebContentsDestroyedWatcher watcher(
      controller->GetModalDialogWebContentsForTesting());
  // Close the dialog before it was displayed, this should not crash.
  controller->CloseModalSignin();
  // The web contents were destroyed.
  EXPECT_TRUE(watcher.IsDestroyed());
}

// Similar to SigninViewControllerDelegateViewsBrowserTest.CloseImmediately but
// specifically tests the case where the dialog is not shown at creation.
// Regression test for https://crbug.com/1233030.
IN_PROC_BROWSER_TEST_F(SigninViewControllerDelegateViewsBrowserTest,
                       CloseBeforeDisplay) {
  SigninViewControllerDelegateViews* delegate =
      CreateDelegate(/*show_immediately=*/false);
  // The dialog is not shown.
  EXPECT_FALSE(delegate->GetWidget());
  CloseDialog(delegate);
}

// Similar to SigninViewControllerDelegateViewsBrowserTest.CloseImmediately but
// specifically tests the case where the dialog is shown at creation.
// Regression test for https://crbug.com/1233030.
IN_PROC_BROWSER_TEST_F(SigninViewControllerDelegateViewsBrowserTest,
                       CloseAfterDisplay) {
  SigninViewControllerDelegateViews* delegate =
      CreateDelegate(/*show_immediately=*/true);
  // The dialog is shown.
  EXPECT_TRUE(delegate->GetWidget());
  CloseDialog(delegate);
}
