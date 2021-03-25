// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_update_component_view.h"

#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/crostini/crostini_dialogue_browser_test_util.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class CrostiniUpdateComponentViewBrowserTest
    : public CrostiniDialogBrowserTest {
 public:
  CrostiniUpdateComponentViewBrowserTest()
      : CrostiniDialogBrowserTest(true /*register_termina*/) {
    // TODO(crbug/953544) DLC makes this entire feature redundant, so once we're
    // committed to it delete all of this.
    scoped_feature_list_.InitAndDisableFeature(
        chromeos::features::kCrostiniUseDlc);
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ShowCrostiniUpdateComponentView(browser()->profile(),
                                    crostini::CrostiniUISurface::kAppList);
  }

  CrostiniUpdateComponentView* ActiveView() {
    return CrostiniUpdateComponentView::GetActiveViewForTesting();
  }

  bool HasAcceptButton() { return ActiveView()->GetOkButton() != nullptr; }

  bool HasCancelButton() { return ActiveView()->GetCancelButton() != nullptr; }

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
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniUpdateComponentViewBrowserTest);
};

// Test the dialog is actually launched.
IN_PROC_BROWSER_TEST_F(CrostiniUpdateComponentViewBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(CrostiniUpdateComponentViewBrowserTest, HitOK) {
  base::HistogramTester histogram_tester;

  ShowUi("default");
  ExpectView();
  EXPECT_EQ(ui::DIALOG_BUTTON_OK, ActiveView()->GetDialogButtons());

  EXPECT_TRUE(HasAcceptButton());
  EXPECT_FALSE(HasCancelButton());

  ActiveView()->AcceptDialog();
  EXPECT_TRUE(ActiveView()->GetWidget()->IsClosed());

  WaitForViewDestroyed();

  histogram_tester.ExpectUniqueSample(
      "Crostini.UpgradeSource",
      static_cast<base::HistogramBase::Sample>(
          crostini::CrostiniUISurface::kAppList),
      1);
}

IN_PROC_BROWSER_TEST_F(CrostiniUpdateComponentViewBrowserTest,
                       LaunchAppOnline_UpgradeNeeded) {
  base::HistogramTester histogram_tester;
  crostini::CrostiniManager::GetForProfile(browser()->profile())
      ->MaybeUpdateCrostini();

  ExpectNoView();

  UnregisterTermina();
  crostini::LaunchCrostiniApp(browser()->profile(),
                              crostini::kCrostiniTerminalSystemAppId, 0);
  ExpectNoView();
}

IN_PROC_BROWSER_TEST_F(CrostiniUpdateComponentViewBrowserTest,
                       LaunchAppOffline_UpgradeNeeded) {
  // Ensure Terminal System App is installed.
  web_app::WebAppProvider::Get(browser()->profile())
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();

  base::HistogramTester histogram_tester;
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  crostini::CrostiniManager::GetForProfile(browser()->profile())
      ->MaybeUpdateCrostini();

  ExpectNoView();

  UnregisterTermina();
  crostini::LaunchCrostiniApp(browser()->profile(),
                              crostini::kCrostiniTerminalSystemAppId, 0);

  // For Terminal System App, we must wait for browser to load.
  Browser* terminal_browser = web_app::FindSystemWebAppBrowser(
      browser()->profile(), web_app::SystemAppType::TERMINAL);
  CHECK_NE(nullptr, terminal_browser);
  WaitForLoadFinished(terminal_browser->tab_strip_model()->GetWebContentsAt(0));

  ExpectView();

  ActiveView()->AcceptDialog();
  EXPECT_TRUE(ActiveView()->GetWidget()->IsClosed());

  WaitForViewDestroyed();

  histogram_tester.ExpectUniqueSample(
      "Crostini.UpgradeSource",
      static_cast<base::HistogramBase::Sample>(
          crostini::CrostiniUISurface::kAppList),
      1);
}
