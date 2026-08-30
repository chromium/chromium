// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_infobar_manager.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class DefaultBrowserInfoBarManagerBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  DefaultBrowserInfoBarManagerBrowserTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeatureWithParameters(
          infobars::kCentralizedInfoBarFramework,
          {{"MigratedDefaultBrowser", "true"}});
    } else {
      feature_list_.InitAndDisableFeature(
          infobars::kCentralizedInfoBarFramework);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         DefaultBrowserInfoBarManagerBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "MigratedInfobar"
                                             : "LegacyInfobar";
                         });

namespace {

size_t InfoBarCountInActiveTab(BrowserWindowInterface* browser) {
  return infobars::ContentInfoBarManager::FromWebContents(
             browser->GetTabStripModel()->GetActiveWebContents())
      ->infobars()
      .size();
}

}  // namespace

IN_PROC_BROWSER_TEST_P(DefaultBrowserInfoBarManagerBrowserTest,
                       ShowShowsThePromptInEveryWindow) {
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  BrowserWindowInterface* browser2 = CreateBrowser(browser()->GetProfile());

  DefaultBrowserInfoBarManager infobar_manager;
  infobar_manager.Show(/*can_pin_to_taskbar=*/false);

  // Whatever tab is active in a window shows the prompt.
  ASSERT_EQ(1u, InfoBarCountInActiveTab(browser()));
  ASSERT_EQ(1u, InfoBarCountInActiveTab(browser2));
  EXPECT_EQ(infobars::InfoBarDelegate::DEFAULT_BROWSER_INFOBAR_DELEGATE,
            infobars::ContentInfoBarManager::FromWebContents(
                browser()->tab_strip_model()->GetActiveWebContents())
                ->infobars()[0]
                ->delegate()
                ->GetIdentifier());

  // Including after switching tabs.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(1u, InfoBarCountInActiveTab(browser()));

  // Drop the instances (and, in the legacy path, their observers) before the
  // stack-scoped manager goes away.
  infobar_manager.CloseAll();
  EXPECT_EQ(0u, InfoBarCountInActiveTab(browser()));
  EXPECT_EQ(0u, InfoBarCountInActiveTab(browser2));
}

IN_PROC_BROWSER_TEST_P(DefaultBrowserInfoBarManagerBrowserTest,
                       DismissRecordsTheInteractionAndPrefs) {
  DefaultBrowserInfoBarManager infobar_manager;
  infobar_manager.Show(/*can_pin_to_taskbar=*/false);

  auto* content_infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_EQ(1u, content_infobar_manager->infobars().size());

  base::HistogramTester histogram_tester;

  infobars::InfoBar* infobar = content_infobar_manager->infobars()[0];
  infobar->delegate()->InfoBarDismissed();
  infobar->RemoveSelf();

  // DISMISS_INFO_BAR from the manager's InfoBarUserInteraction enum.
  histogram_tester.ExpectUniqueSample("DefaultBrowser.InfoBar.UserInteraction",
                                      3, 1);
  EXPECT_EQ(1, g_browser_process->local_state()->GetInteger(
                   prefs::kDefaultBrowserInfobarDeclinedCount));
  EXPECT_EQ(0u, content_infobar_manager->infobars().size());
}
