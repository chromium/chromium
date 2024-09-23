// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/security_dialog_tracker.h"

#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

using SecurityDialogTrackerTest = InProcessBrowserTest;

namespace {

// Create a dialog widget as a child of `parent` widget.
views::UniqueWidgetPtr CreateTestDialogWidget(views::Widget* parent) {
  auto dialog_delegate = std::make_unique<views::DialogDelegateView>();
  return std::unique_ptr<views::Widget>(
      views::DialogDelegate::CreateDialogWidget(
          dialog_delegate.release(), nullptr, parent->GetNativeView()));
}

}  // namespace

namespace extensions {

IN_PROC_BROWSER_TEST_F(SecurityDialogTrackerTest, Basic) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  SecurityDialogTracker* tracker = SecurityDialogTracker::GetInstance();
  views::UniqueWidgetPtr security_widget =
      CreateTestDialogWidget(browser_view->GetWidget());

  // No security dialogs.
  EXPECT_FALSE(tracker->BrowserHasVisibleSecurityDialogs(browser()));

  tracker->AddSecurityDialog(security_widget.get());
  // Security dialog is not yet visible.
  EXPECT_FALSE(tracker->BrowserHasVisibleSecurityDialogs(browser()));

  browser()->window()->Show();
  security_widget->Show();
  views::test::WidgetVisibleWaiter(security_widget.get()).Wait();
  // Security dialog is now visible.
  EXPECT_TRUE(tracker->BrowserHasVisibleSecurityDialogs(browser()));

  Browser* new_browser = CreateBrowser(browser()->profile());
  // No security dialogs under a different browser.
  EXPECT_FALSE(tracker->BrowserHasVisibleSecurityDialogs(new_browser));

  // Untrack the security dialog.
  tracker->RemoveSecurityDialog(security_widget.get());
  EXPECT_FALSE(tracker->BrowserHasVisibleSecurityDialogs(browser()));

  // Re-track the security dialog.
  tracker->AddSecurityDialog(security_widget.get());
  EXPECT_TRUE(tracker->BrowserHasVisibleSecurityDialogs(browser()));

  // Close the security dialog.
  security_widget->Close();
  views::test::WidgetDestroyedWaiter(security_widget.get()).Wait();
  EXPECT_FALSE(tracker->BrowserHasVisibleSecurityDialogs(browser()));
}

}  // namespace extensions
