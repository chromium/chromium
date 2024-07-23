// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/translate/translate_test_utils.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/translate/content/browser/translate_waiter.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

class PinnedToolbarActionsContainerBrowserTest : public InProcessBrowserTest {
 public:
  PinnedToolbarActionsContainerBrowserTest() = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kToolbarPinning);
    InProcessBrowserTest::SetUp();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  PinnedToolbarActionsContainer* container() {
    return browser_view()->toolbar()->pinned_toolbar_actions_container();
  }

  void TranslatePage(content::WebContents* web_contents) {
    ChromeTranslateClient* chrome_translate_client =
        ChromeTranslateClient::FromWebContents(web_contents);

    chrome_translate_client->GetTranslateManager()
        ->GetLanguageState()
        ->SetSourceLanguage("fr");

    chrome_translate_client->GetTranslateManager()
        ->GetLanguageState()
        ->SetCurrentLanguage("en");
  }

  Browser* CreateBrowser() {
    Browser::CreateParams params(browser()->profile(), true /* user_gesture */);
    Browser* browser = Browser::Create(params);
    browser->window()->Show();
    return browser;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerBrowserTest,
                       CustomizeToolbarCanBeCalledFromNewTabPage) {
  auto pinned_button = std::make_unique<PinnedActionToolbarButton>(
      browser(), actions::kActionCut, container());
  pinned_button->ExecuteCommand(IDC_SHOW_CUSTOMIZE_CHROME_TOOLBAR, 0);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(content::NavigateToURL(web_contents, GURL("chrome://newtab/")));
  content::WaitForLoadStop(web_contents);
  EXPECT_EQ(web_contents->GetURL().possibly_invalid_spec(), "chrome://newtab/");
  const std::optional<SidePanelEntryId> current_entry =
      browser()->GetFeatures().side_panel_ui()->GetCurrentEntryId();
  EXPECT_TRUE(current_entry.has_value());
  EXPECT_EQ(SidePanelEntryId::kCustomizeChrome, current_entry.value());
}

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerBrowserTest,
                       CustomizeToolbarCanBeCalledFromNonNewTabPage) {
  auto pinned_button = std::make_unique<PinnedActionToolbarButton>(
      browser(), actions::kActionCut, container());
  pinned_button->ExecuteCommand(IDC_SHOW_CUSTOMIZE_CHROME_TOOLBAR, 0);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(web_contents);
  EXPECT_NE(web_contents->GetURL().possibly_invalid_spec(), "chrome://newtab/");
  const std::optional<SidePanelEntryId> current_entry =
      browser()->GetFeatures().side_panel_ui()->GetCurrentEntryId();
  EXPECT_TRUE(current_entry.has_value());
  EXPECT_EQ(SidePanelEntryId::kCustomizeChrome, current_entry.value());
}

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerBrowserTest,
                       TranslateStatusIndicator) {
  PinnedToolbarActionsModel* const actions_model =
      PinnedToolbarActionsModel::Get(browser()->profile());
  actions_model->UpdatePinnedState(kActionShowTranslate, true);

  EXPECT_EQ(container()->IsActionPinned(kActionShowTranslate), true);

  auto* pinned_button = container()->GetButtonFor(kActionShowTranslate);
  EXPECT_EQ(pinned_button->GetVisible(), true);
  EXPECT_EQ(pinned_button->GetEnabled(), false);
  EXPECT_EQ(pinned_button->GetStatusIndicatorForTesting()->GetVisible(), false);

  ASSERT_TRUE(embedded_test_server()->Start());

  // Open a new tab with a page in French.
  ASSERT_TRUE(AddTabAtIndex(
      0, GURL(embedded_test_server()->GetURL("/french_page.html")),
      ui::PAGE_TRANSITION_TYPED));

  TranslatePage(browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(pinned_button->GetStatusIndicatorForTesting()->GetVisible(), true);

  // Status indicator should still be visible after creating a new browser.
  CreateBrowser();
  EXPECT_EQ(pinned_button->GetStatusIndicatorForTesting()->GetVisible(), true);

  // Navigate to non-translated page.
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(pinned_button->GetStatusIndicatorForTesting()->GetVisible(), false);
}

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerBrowserTest,
                       ButtonsSetToNotVisibleNotSeenAfterLayout) {
  PinnedToolbarActionsModel* const actions_model =
      PinnedToolbarActionsModel::Get(browser()->profile());
  actions_model->UpdatePinnedState(kActionShowTranslate, true);

  EXPECT_EQ(container()->IsActionPinned(kActionShowTranslate), true);

  auto* pinned_button = container()->GetButtonFor(kActionShowTranslate);
  EXPECT_EQ(pinned_button->GetVisible(), true);
  pinned_button->SetVisible(false);
  container()->InvalidateLayout();
  EXPECT_EQ(pinned_button->GetVisible(), false);
}
