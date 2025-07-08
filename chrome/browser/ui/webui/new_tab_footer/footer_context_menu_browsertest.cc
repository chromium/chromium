// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_footer/footer_context_menu.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/actions/actions.h"

class FooterContextMenuBrowserTest : public InProcessBrowserTest {
 public:
  FooterContextMenuBrowserTest() = default;
  ~FooterContextMenuBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    BrowserWindowInterface* bwi = webui::GetBrowserWindowInterface(
        chrome_test_utils::GetActiveWebContents(this));
    menu_ = std::make_unique<FooterContextMenu>(bwi);
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDownOnMainThread() override {
    menu_.reset();
    histogram_tester_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }
  FooterContextMenu* menu() { return menu_.get(); }
  Profile* profile() { return browser()->profile(); }

 private:
  std::unique_ptr<FooterContextMenu> menu_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(FooterContextMenuBrowserTest, HidesFooter) {
  ASSERT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kNtpFooterVisible));
  const std::string& hide_footer = "NewTabPage.Footer.ContextMenuClicked";
  histogram_tester()->ExpectTotalCount(hide_footer, 0);

  auto menu_item = new_tab_footer::FooterContextMenuItem::kHideFooter;
  menu()->ExecuteCommand(static_cast<int>(menu_item), /*event_flags=*/0);

  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kNtpFooterVisible));
  histogram_tester()->ExpectTotalCount(hide_footer, 1);
  histogram_tester()->ExpectBucketCount(hide_footer, menu_item, 1);
}

IN_PROC_BROWSER_TEST_F(FooterContextMenuBrowserTest, OpensCustomizeChrome) {
  ASSERT_EQ(actions::ActionManager::Get()
                .FindAction(kActionSidePanelShowCustomizeChrome)
                ->GetInvokeCount(),
            0);
  const std::string& customize_chrome = "NewTabPage.Footer.ContextMenuClicked";
  histogram_tester()->ExpectTotalCount(customize_chrome, 0);

  auto menu_item = new_tab_footer::FooterContextMenuItem::kCustomizeChrome;
  menu()->ExecuteCommand(static_cast<int>(menu_item), /*event_flags=*/0);

  ASSERT_EQ(actions::ActionManager::Get()
                .FindAction(kActionSidePanelShowCustomizeChrome)
                ->GetInvokeCount(),
            1);
  histogram_tester()->ExpectTotalCount(customize_chrome, 1);
  histogram_tester()->ExpectBucketCount(customize_chrome, menu_item, 1);
}
