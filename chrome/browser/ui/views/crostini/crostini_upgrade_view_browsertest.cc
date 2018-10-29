// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_upgrade_view.h"

#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/crostini/crostini_browser_test_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "components/crx_file/id_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/window/dialog_client_view.h"

class CrostiniUpgradeViewBrowserTest : public CrostiniDialogBrowserTest {
 public:
  CrostiniUpgradeViewBrowserTest()
      : CrostiniDialogBrowserTest(true /*register_termina*/) {}

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ShowCrostiniUpgradeView(browser()->profile(),
                            crostini::CrostiniUISurface::kAppList);
  }

  CrostiniUpgradeView* ActiveView() {
    return CrostiniUpgradeView::GetActiveViewForTesting();
  }

  bool HasAcceptButton() {
    return ActiveView()->GetDialogClientView()->ok_button() != nullptr;
  }

  bool HasCancelButton() {
    return ActiveView()->GetDialogClientView()->cancel_button() != nullptr;
  }

  void WaitForViewDestroyed() {
    base::RunLoop().RunUntilIdle();
    ExpectNoView();
  }

  void ExpectView() {
    // A new Widget was created in ShowUi() or since the last VerifyUi().
    EXPECT_TRUE(VerifyUi());
    // There is one view, and it's ours.
    EXPECT_NE(nullptr, ActiveView());
  }

  void ExpectNoView() {
    // No new Widget was created in ShowUi() or since the last VerifyUi().
    EXPECT_FALSE(VerifyUi());
    // Our view has really been deleted.
    EXPECT_EQ(nullptr, ActiveView());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CrostiniUpgradeViewBrowserTest);
};

// Test the dialog is actually launched.
IN_PROC_BROWSER_TEST_F(CrostiniUpgradeViewBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(CrostiniUpgradeViewBrowserTest, HitOK) {
  base::HistogramTester histogram_tester;

  ShowUi("default");
  ExpectView();
  EXPECT_EQ(ui::DIALOG_BUTTON_OK, ActiveView()->GetDialogButtons());

  EXPECT_TRUE(HasAcceptButton());
  EXPECT_FALSE(HasCancelButton());

  ActiveView()->GetDialogClientView()->AcceptWindow();
  EXPECT_TRUE(ActiveView()->GetWidget()->IsClosed());

  WaitForViewDestroyed();

  histogram_tester.ExpectUniqueSample(
      "Crostini.UpgradeSource",
      static_cast<base::HistogramBase::Sample>(
          crostini::CrostiniUISurface::kAppList),
      1);
}

IN_PROC_BROWSER_TEST_F(CrostiniUpgradeViewBrowserTest,
                       LaunchAppOnline_UpgradeNeeded) {
  base::HistogramTester histogram_tester;
  crostini::CrostiniManager::GetForProfile(browser()->profile())
      ->MaybeUpgradeCrostini();

  ExpectNoView();

  UnregisterTermina();
  crostini::LaunchCrostiniApp(browser()->profile(),
                              crostini::kCrostiniTerminalId, 0);
  ExpectNoView();
}

IN_PROC_BROWSER_TEST_F(CrostiniUpgradeViewBrowserTest,
                       LaunchAppOffline_UpgradeNeeded) {
  base::HistogramTester histogram_tester;
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);
  crostini::CrostiniManager::GetForProfile(browser()->profile())
      ->MaybeUpgradeCrostini();

  ExpectNoView();

  UnregisterTermina();
  crostini::LaunchCrostiniApp(browser()->profile(),
                              crostini::kCrostiniTerminalId, 0);
  ExpectView();

  ActiveView()->GetDialogClientView()->AcceptWindow();
  EXPECT_TRUE(ActiveView()->GetWidget()->IsClosed());

  WaitForViewDestroyed();

  histogram_tester.ExpectUniqueSample(
      "Crostini.UpgradeSource",
      static_cast<base::HistogramBase::Sample>(
          crostini::CrostiniUISurface::kAppList),
      1);
}
