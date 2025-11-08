// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_test_utils.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_test_helper.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/translate/content/browser/translate_waiter.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"

class PinnedToolbarActionsContainerBrowserTest : public InProcessBrowserTest {
 public:
  PinnedToolbarActionsContainerBrowserTest() = default;

  void SetUpOnMainThread() override {
    PinnedToolbarActionsModel* const actions_model =
        PinnedToolbarActionsModel::Get(browser()->profile());
    actions_model->UpdatePinnedState(kActionShowChromeLabs, false);
    if (features::HasTabSearchToolbarButton()) {
      actions_model->UpdatePinnedState(kActionTabSearch, false);
    }
    views::test::WaitForAnimatingLayoutManager(container());
    // OS integration is needed to be able to launch web applications. This
    // override ensures OS integration doesn't leave any traces.
    override_registration_ =
        web_app::OsIntegrationTestOverrideImpl::OverrideForTesting();
  }

  void TearDownOnMainThread() override {
    for (Profile* profile :
         g_browser_process->profile_manager()->GetLoadedProfiles()) {
      web_app::test::UninstallAllWebApps(profile);
    }
    override_registration_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
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

  WebAppFrameToolbarTestHelper& toolbar_helper() {
    return web_app_frame_toolbar_helper_;
  }

 protected:
  // OS integration is needed to be able to launch web applications. This
  // override ensures OS integration doesn't leave any traces.
  std::unique_ptr<web_app::OsIntegrationTestOverrideImpl::BlockingRegistration>
      override_registration_;
  WebAppFrameToolbarTestHelper web_app_frame_toolbar_helper_;
};

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerBrowserTest,
                       CustomizeToolbarCanBeCalledFromNewTabPage) {
  auto pinned_button = std::make_unique<PinnedActionToolbarButton>(
      browser(), actions::kActionCut, container()->GetWeakPtrForTesting());
  pinned_button->menu_model()->ActivatedAt(2);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(content::NavigateToURL(web_contents, GURL("chrome://newtab/")));
  content::WaitForLoadStop(web_contents);
  EXPECT_EQ(web_contents->GetURL().possibly_invalid_spec(), "chrome://newtab/");
  EXPECT_TRUE(browser()->GetFeatures().side_panel_ui()->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kCustomizeChrome)));
}

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerBrowserTest,
                       CustomizeToolbarCanBeCalledFromNonNewTabPage) {
  auto pinned_button = std::make_unique<PinnedActionToolbarButton>(
      browser(), actions::kActionCut, container()->GetWeakPtrForTesting());
  pinned_button->menu_model()->ActivatedAt(2);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(web_contents);
  EXPECT_NE(web_contents->GetURL().possibly_invalid_spec(), "chrome://newtab/");
  EXPECT_TRUE(browser()->GetFeatures().side_panel_ui()->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kCustomizeChrome)));
}

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerBrowserTest,
                       CustomizeToolbarCanNotBeCalledFromIncognitoWindow) {
  Browser* incognito_browser = Browser::Create(Browser::CreateParams(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      true));
  AddBlankTabAndShow(incognito_browser);
  auto pinned_button = std::make_unique<PinnedActionToolbarButton>(
      incognito_browser, actions::kActionCut,
      container()->GetWeakPtrForTesting());
  EXPECT_FALSE(pinned_button->menu_model()->IsEnabledAt(2));
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

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerBrowserTest,
                       ButtonNotSeenWhenHiddenForSidePanelEntry) {
  // Set the bookmarks side panel entry to not show an ephemeral button.
  SidePanelUI* const side_panel_ui = browser()->GetFeatures().side_panel_ui();
  side_panel_ui->SetNoDelaysForTesting(true);
  SidePanelEntry* const entry =
      SidePanelRegistry::From(browser())->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntryId::kBookmarks));
  SidePanelEntry::PanelType panel_type = entry->type();
  entry->set_should_show_ephemerally_in_toolbar(false);

  // Verify no toolbar button is shown when the bookmarks side panel is opened.
  side_panel_ui->Show(SidePanelEntry::Key(SidePanelEntryId::kBookmarks));
  views::test::WaitForAnimatingLayoutManager(container());
  EXPECT_FALSE(container()->IsActionPinned(kActionSidePanelShowBookmarks));
  EXPECT_FALSE(container()->IsActionPoppedOut(kActionSidePanelShowBookmarks));

  // Set the bookmarks entry back to showing the toolbar button ephemerally if
  // shown.
  side_panel_ui->Close(panel_type);
  entry->set_should_show_ephemerally_in_toolbar(true);

  // Verify the toolbar button is now ephemerally shown if the bookmarks side
  // panel is opened.
  side_panel_ui->Show(SidePanelEntry::Key(SidePanelEntryId::kBookmarks));
  views::test::WaitForAnimatingLayoutManager(container());
  EXPECT_FALSE(container()->IsActionPinned(kActionSidePanelShowBookmarks));
  EXPECT_TRUE(container()->IsActionPoppedOut(kActionSidePanelShowBookmarks));
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerBrowserTest,
                       QRCodeUpdatesWithSharingHubPrefChanges) {
  PinnedActionToolbarButton* button =
      container()->GetButtonFor(kActionQrCodeGenerator);
  EXPECT_EQ(button, nullptr);
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_EQ(true, prefs->GetBoolean(prefs::kDesktopSharingHubEnabled));

  PinnedToolbarActionsModel* const actions_model =
      PinnedToolbarActionsModel::Get(browser()->profile());
  actions_model->UpdatePinnedState(kActionQrCodeGenerator, true);
  views::test::WaitForAnimatingLayoutManager(container());

  EXPECT_EQ(container()->IsActionPinned(kActionQrCodeGenerator), true);

  auto* pinned_button = container()->GetButtonFor(kActionQrCodeGenerator);
  EXPECT_NE(pinned_button, nullptr);
  EXPECT_EQ(pinned_button->GetVisible(), true);

  prefs->SetBoolean(prefs::kDesktopSharingHubEnabled, false);
  views::test::WaitForAnimatingLayoutManager(container());
  EXPECT_EQ(pinned_button->GetVisible(), false);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerBrowserTest,
                       QRCodeUpdatesWithPolicyPrefChanges) {
  PinnedActionToolbarButton* button =
      container()->GetButtonFor(kActionQrCodeGenerator);
  EXPECT_EQ(button, nullptr);
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kQRCodeGeneratorEnabled, true);

  PinnedToolbarActionsModel::Get(browser()->profile())
      ->UpdatePinnedState(kActionQrCodeGenerator, true);
  button = container()->GetButtonFor(kActionQrCodeGenerator);
  EXPECT_NE(button, nullptr);
  EXPECT_EQ(button->GetEnabled(), true);

  prefs->SetBoolean(prefs::kQRCodeGeneratorEnabled, false);
  EXPECT_EQ(button->GetEnabled(), false);
}

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerBrowserTest,
                       NoPinnedButtonsInWebApps) {
  PinnedToolbarActionsModel* const actions_model =
      PinnedToolbarActionsModel::Get(browser()->profile());

  // Pin a few buttons and verify they exist.
  actions_model->UpdatePinnedState(kActionShowTranslate, true);
  actions_model->UpdatePinnedState(kActionSidePanelShowBookmarks, true);
  actions_model->UpdatePinnedState(kActionPrint, true);
  views::test::WaitForAnimatingLayoutManager(container());
  EXPECT_EQ(container()->IsActionPinned(kActionShowTranslate), true);
  EXPECT_EQ(container()->IsActionPinned(kActionSidePanelShowBookmarks), true);
  EXPECT_EQ(container()->IsActionPinned(kActionPrint), true);

  // Open a web app and verify none of the buttons previously pinned exist.
  const GURL app_url("https://test.org");
  toolbar_helper().InstallAndLaunchWebApp(browser(), app_url);
  PinnedToolbarActionsContainer* web_app_container =
      toolbar_helper()
          .web_app_frame_toolbar()
          ->GetPinnedToolbarActionsContainer();
  EXPECT_EQ(web_app_container->IsActionPinned(kActionShowTranslate), false);
  EXPECT_EQ(web_app_container->IsActionPinned(kActionSidePanelShowBookmarks),
            false);
  EXPECT_EQ(web_app_container->IsActionPinned(kActionPrint), false);
}

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerBrowserTest,
                       PinnedButtonPinningAndUnpinning) {
  PinnedToolbarActionsModel* const actions_model =
      PinnedToolbarActionsModel::Get(browser()->profile());

  actions::ActionItem* action_item =
      actions::ActionManager::Get().FindAction(kActionShowTranslate);

  // Verify button is visible when pinned.
  action_item->SetProperty(
      actions::kActionItemPinnableKey,
      static_cast<int>(actions::ActionPinnableState::kPinnable));
  actions_model->UpdatePinnedState(kActionShowTranslate, true);
  views::test::WaitForAnimatingLayoutManager(container());
  auto* button_before = container()->GetButtonFor(kActionShowTranslate);
  EXPECT_EQ(button_before->GetVisible(), true);

  // Verify button is no longer visible after setting to not pinnable.
  action_item->SetProperty(
      actions::kActionItemPinnableKey,
      static_cast<int>(actions::ActionPinnableState::kNotPinnable));
  views::test::WaitForAnimatingLayoutManager(container());
  auto* button_during = container()->GetButtonFor(kActionShowTranslate);
  views::test::WaitForAnimatingLayoutManager(container());
  EXPECT_EQ(button_during->GetVisible(), false);

  // Verify button is longer visible after setting back to pinnable.
  action_item->SetProperty(
      actions::kActionItemPinnableKey,
      static_cast<int>(actions::ActionPinnableState::kPinnable));
  views::test::WaitForAnimatingLayoutManager(container());
  auto* button_after = container()->GetButtonFor(kActionShowTranslate);
  views::test::WaitForAnimatingLayoutManager(container());
  EXPECT_EQ(button_after->GetVisible(), true);
}
