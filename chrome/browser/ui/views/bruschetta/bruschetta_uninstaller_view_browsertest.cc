// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/bruschetta/bruschetta_uninstaller_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class BruschettaUninstallerViewBrowserTest : public DialogBrowserTest {
 public:
  BruschettaUninstallerViewBrowserTest() {
    feature_list_.InitWithFeatures({ash::features::kBruschetta}, {});
  }

  BruschettaUninstallerViewBrowserTest(
      const BruschettaUninstallerViewBrowserTest&) = delete;
  BruschettaUninstallerViewBrowserTest& operator=(
      const BruschettaUninstallerViewBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    BruschettaUninstallerView::Show(browser()->profile(),
                                    bruschetta::GetBruschettaAlphaId());
  }

  BruschettaUninstallerView* ActiveView() {
    return BruschettaUninstallerView::GetActiveViewForTesting();
  }

  bool HasAcceptButton() { return ActiveView()->GetOkButton() != nullptr; }

  bool HasCancelButton() { return ActiveView()->GetCancelButton() != nullptr; }

  void WaitForViewDestroyed() {
    base::RunLoop run_loop;
    ActiveView()->set_destructor_callback_for_testing(run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_EQ(nullptr, ActiveView());
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(BruschettaUninstallerViewBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BruschettaUninstallerViewBrowserTest, UninstallFlow) {
  ShowUi("default");
  EXPECT_NE(nullptr, ActiveView());
  EXPECT_EQ(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
            ActiveView()->GetDialogButtons());

  EXPECT_TRUE(HasAcceptButton());
  EXPECT_TRUE(HasCancelButton());

  ActiveView()->AcceptDialog();
  EXPECT_FALSE(ActiveView()->GetWidget()->IsClosed());
  EXPECT_FALSE(HasAcceptButton());
  EXPECT_FALSE(HasCancelButton());

  WaitForViewDestroyed();
}
