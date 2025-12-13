// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

class RollBackModeBInfoBarControllerBrowserTest : public InProcessBrowserTest {
 protected:
  RollBackModeBInfoBarControllerBrowserTest() {
    feature_list_.InitWithFeatures(
        {privacy_sandbox::kRollBackModeB},
        {content_settings::features::kTrackingProtection3pcd});
  }
  void SetUpOnMainThread() override {
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowRollbackUiModeB,
                                                 true);
  }

  bool HasRollBackModeBInfoBar(Browser* browser) {
    auto* infobar_manager = infobars::ContentInfoBarManager::FromWebContents(
        browser->tab_strip_model()->GetActiveWebContents());
    return !infobar_manager->infobars().empty() &&
           infobar_manager->infobars()[0]->GetIdentifier() ==
               infobars::InfoBarDelegate::ROLL_BACK_MODE_B_INFOBAR_DELEGATE;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(RollBackModeBInfoBarControllerBrowserTest,
                       SwitchingTabsClosesInfoBar) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("https://test")));
  EXPECT_TRUE(HasRollBackModeBInfoBar(browser()));
  chrome::NewTab(browser());
  ASSERT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_FALSE(HasRollBackModeBInfoBar(browser()));
}

IN_PROC_BROWSER_TEST_F(RollBackModeBInfoBarControllerBrowserTest,
                       MinimizingWindowClosesInfoBar) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("https://test")));
  EXPECT_TRUE(HasRollBackModeBInfoBar(browser()));
  browser()->window()->Minimize();
  ASSERT_TRUE(ui_test_utils::WaitForMinimized(browser()));
  EXPECT_FALSE(HasRollBackModeBInfoBar(browser()));
}

IN_PROC_BROWSER_TEST_F(RollBackModeBInfoBarControllerBrowserTest,
                       NewWindowDoesNotCloseInfoBar) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("https://test")));
  EXPECT_TRUE(HasRollBackModeBInfoBar(browser()));

  Browser* new_browser = CreateBrowser(browser()->profile());
  chrome::NewTab(new_browser);
  ASSERT_TRUE(content::WaitForLoadStop(
      new_browser->tab_strip_model()->GetActiveWebContents()));
  ui_test_utils::WaitUntilBrowserBecomeActive(new_browser);

  EXPECT_FALSE(browser()->IsActive());
  EXPECT_TRUE(HasRollBackModeBInfoBar(browser()));
  EXPECT_TRUE(new_browser->IsActive());
  EXPECT_FALSE(HasRollBackModeBInfoBar(new_browser));
}

}  // namespace
