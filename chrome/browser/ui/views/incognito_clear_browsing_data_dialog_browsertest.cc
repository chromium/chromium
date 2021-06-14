// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/incognito_clear_browsing_data_dialog.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/views/controls/button/label_button.h"

class IncognitoClearBrowsingDataDialogBrowserTest
    : public InProcessBrowserTest {
 public:
  void OpenDialog() {
    incognito_browser_ = CreateIncognitoBrowser(browser()->profile());
    views::View* view = static_cast<views::View*>(
        BrowserView::GetBrowserViewForBrowser(incognito_browser_)
            ->toolbar_button_provider()
            ->GetAvatarToolbarButton());
    IncognitoClearBrowsingDataDialog::Show(view, incognito_browser_->profile());
    EXPECT_TRUE(IncognitoClearBrowsingDataDialog::IsShowing());
  }

  Browser* GetIncognitoBrowser() { return incognito_browser_; }

  IncognitoClearBrowsingDataDialog* GetDialogView() {
    return IncognitoClearBrowsingDataDialog::
        GetIncognitoClearBrowsingDataDialogForTesting();
  }

 private:
  Browser* incognito_browser_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogBrowserTest,
                       TestDialogIsShown) {
  OpenDialog();
  auto* incognito_cbd_dialog_view = GetDialogView();

  ASSERT_TRUE(IncognitoClearBrowsingDataDialog::IsShowing());
  ASSERT_EQ(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
            incognito_cbd_dialog_view->GetDialogButtons());
  ASSERT_TRUE(
      incognito_cbd_dialog_view->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  ASSERT_TRUE(incognito_cbd_dialog_view->IsDialogButtonEnabled(
      ui::DIALOG_BUTTON_CANCEL));
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogBrowserTest,
                       TestCloseWindowsButton) {
  OpenDialog();

  GetDialogView()->AcceptDialog();
  ui_test_utils::WaitForBrowserToClose(GetIncognitoBrowser());
  ASSERT_EQ(0UL, BrowserList::GetIncognitoBrowserCount());
  ASSERT_TRUE(GetDialogView() == nullptr);
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogBrowserTest,
                       TestCancelButton) {
  OpenDialog();

  base::RunLoop run_loop;
  GetDialogView()->SetDestructorCallbackForTesting(
      base::BindLambdaForTesting([&]() {
        run_loop.Quit();
        ASSERT_FALSE(IncognitoClearBrowsingDataDialog::IsShowing());
        ASSERT_TRUE(GetDialogView() == nullptr);
      }));

  GetDialogView()->Cancel();
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogBrowserTest,
                       TestBrowserCloseEventClosesDialogFirst) {
  OpenDialog();

  GetDialogView()->SetDestructorCallbackForTesting(
      base::BindLambdaForTesting([&]() {
        ASSERT_FALSE(IncognitoClearBrowsingDataDialog::IsShowing());
        ASSERT_TRUE(GetDialogView() == nullptr);
        ASSERT_TRUE(BrowserList::GetIncognitoBrowserCount() > 0);
      }));

  CloseBrowserSynchronously(GetIncognitoBrowser());
}
