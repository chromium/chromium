// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/test/metrics/action_variants_reader.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_test_utils.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_actions.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/toolbar_pref_names.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container_layout.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_button_status_indicator.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_test_helper.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/translate/content/browser/translate_waiter.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"

class PinnedToolbarActionsContainerBrowserTest : public InProcessBrowserTest {
 public:
  PinnedToolbarActionsContainerBrowserTest() = default;

  void SetUpOnMainThread() override {
    PinnedToolbarActionsModel* const actions_model =
        PinnedToolbarActionsModel::Get(browser()->GetProfile());
    actions_model->UpdatePinnedState(kActionShowChromeLabs, false);
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
    CHECK(!features::IsWebUIPinnedToolbarActionsEnabled())
        << "Test needs modification to support WebUIPinnedToolbarActions";
    return static_cast<PinnedToolbarActionsContainer*>(
        browser_view()->toolbar_button_provider()->GetPinnedToolbarActions());
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
    BrowserWindowCreateParams params(browser()->GetProfile(),
                                     /*from_user_gesture=*/true);
    Browser* browser =
        CreateBrowserWindow(std::move(params))->GetBrowserForMigrationOnly();
    browser->GetWindow()->Show();
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
  Browser* incognito_browser =
      CreateBrowserWindow(BrowserWindowCreateParams(
                              browser()->GetProfile()->GetPrimaryOTRProfile(
                                  /*create_if_needed=*/true),
                              /*from_user_gesture=*/true))
          ->GetBrowserForMigrationOnly();
  AddBlankTabAndShow(incognito_browser);
  auto pinned_button = std::make_unique<PinnedActionToolbarButton>(
      incognito_browser, actions::kActionCut,
      container()->GetWeakPtrForTesting());
  EXPECT_FALSE(pinned_button->menu_model()->IsEnabledAt(2));
}

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerBrowserTest,
                       TranslateStatusIndicator) {
  PinnedToolbarActionsModel* const actions_model =
      PinnedToolbarActionsModel::Get(browser()->GetProfile());
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
      PinnedToolbarActionsModel::Get(browser()->GetProfile());
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
  entry->set_should_show_ephemerally_in_toolbar(false);

  // Verify no toolbar button is shown when the bookmarks side panel is opened.
  side_panel_ui->Show(SidePanelEntry::Key(SidePanelEntryId::kBookmarks));
  views::test::WaitForAnimatingLayoutManager(container());
  EXPECT_FALSE(container()->IsActionPinned(kActionSidePanelShowBookmarks));
  EXPECT_FALSE(container()->IsActionPoppedOut(kActionSidePanelShowBookmarks));

  // Set the bookmarks entry back to showing the toolbar button ephemerally if
  // shown.
  side_panel_ui->Close();
  entry->set_should_show_ephemerally_in_toolbar(true);

  // Verify the toolbar button is now ephemerally shown if the bookmarks side
  // panel is opened.
  side_panel_ui->Show(SidePanelEntry::Key(SidePanelEntryId::kBookmarks));
  views::test::WaitForAnimatingLayoutManager(container());
  EXPECT_FALSE(container()->IsActionPinned(kActionSidePanelShowBookmarks));
  EXPECT_TRUE(container()->IsActionPoppedOut(kActionSidePanelShowBookmarks));
}

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerBrowserTest,
                       SidePanelButtonShownActiveStateForPinnedNotEphemeral) {
  // Set the bookmarks side panel entry to not show an ephemeral button but be
  // pinned.
  SidePanelUI* const side_panel_ui = browser()->GetFeatures().side_panel_ui();
  side_panel_ui->SetNoDelaysForTesting(true);
  SidePanelEntry* const entry =
      SidePanelRegistry::From(browser())->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntryId::kBookmarks));
  entry->set_should_show_ephemerally_in_toolbar(false);
  PinnedToolbarActionsModel* const actions_model =
      PinnedToolbarActionsModel::Get(browser()->GetProfile());
  actions_model->UpdatePinnedState(kActionSidePanelShowBookmarks, true);
  views::test::WaitForAnimatingLayoutManager(container());
  EXPECT_TRUE(container()->IsActionPinned(kActionSidePanelShowBookmarks));

  // Verify the pinned toolbar button is active when the side panel
  // is opened.
  side_panel_ui->Show(SidePanelEntry::Key(SidePanelEntryId::kBookmarks));
  views::test::WaitForAnimatingLayoutManager(container());
  auto* pinned_button =
      container()->GetButtonFor(kActionSidePanelShowBookmarks);
  ASSERT_NE(pinned_button, nullptr);
  EXPECT_TRUE(pinned_button->IsActive());
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerBrowserTest,
                       QRCodeUpdatesWithSharingHubPrefChanges) {
  PinnedActionToolbarButton* button =
      container()->GetButtonFor(kActionQrCodeGenerator);
  EXPECT_EQ(button, nullptr);
  PrefService* prefs = browser()->GetProfile()->GetPrefs();
  EXPECT_EQ(true, prefs->GetBoolean(prefs::kDesktopSharingHubEnabled));

  PinnedToolbarActionsModel* const actions_model =
      PinnedToolbarActionsModel::Get(browser()->GetProfile());
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

  PinnedToolbarActionsModel::Get(browser()->GetProfile())
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
      PinnedToolbarActionsModel::Get(browser()->GetProfile());

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
  PinnedToolbarActions* web_app_container =
      toolbar_helper().web_app_frame_toolbar()->GetPinnedToolbarActions();
  EXPECT_EQ(web_app_container->IsActionPinned(kActionShowTranslate), false);
  EXPECT_EQ(web_app_container->IsActionPinned(kActionSidePanelShowBookmarks),
            false);
  EXPECT_EQ(web_app_container->IsActionPinned(kActionPrint), false);
}

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerBrowserTest,
                       PinnedButtonPinningAndUnpinning) {
  PinnedToolbarActionsModel* const actions_model =
      PinnedToolbarActionsModel::Get(browser()->GetProfile());

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

class PinnedToolbarActionsContainerTest
    : public PinnedToolbarActionsContainerBrowserTest {
 public:
  PinnedToolbarActionsContainerTest() = default;

  void SetUpOnMainThread() override {
    PinnedToolbarActionsContainerBrowserTest::SetUpOnMainThread();
    model_ = PinnedToolbarActionsModel::Get(browser()->GetProfile());
    ASSERT_TRUE(model_);
    model_->UpdatePinnedState(kActionShowChromeLabs, false);
    WaitForAnimations();
  }

  void TearDownOnMainThread() override {
    model_ = nullptr;
    PinnedToolbarActionsContainerBrowserTest::TearDownOnMainThread();
  }

  std::vector<PinnedActionToolbarButton*> GetChildToolbarButtons() {
    std::vector<PinnedActionToolbarButton*> result;
    for (views::View* child : container()->children()) {
      if (views::Button::AsButton(child)) {
        result.push_back(static_cast<PinnedActionToolbarButton*>(child));
      }
    }
    return result;
  }

  void CheckIsPoppedOut(actions::ActionId id, bool should_be_popped_out) {
    if (should_be_popped_out) {
      ASSERT_NE(std::ranges::find(container()->popped_out_buttons_, id,
                                  [](PinnedActionToolbarButton* button) {
                                    return button->GetActionId();
                                  }),
                container()->popped_out_buttons_.end());
    } else {
      ASSERT_EQ(std::ranges::find(container()->popped_out_buttons_, id,
                                  [](PinnedActionToolbarButton* button) {
                                    return button->GetActionId();
                                  }),
                container()->popped_out_buttons_.end());
    }
  }

  void CheckIsPinned(actions::ActionId id, bool should_be_pinned) {
    if (should_be_pinned) {
      ASSERT_NE(std::ranges::find(container()->pinned_buttons_, id,
                                  [](PinnedActionToolbarButton* button) {
                                    return button->GetActionId();
                                  }),
                container()->pinned_buttons_.end());
    } else {
      ASSERT_EQ(std::ranges::find(container()->pinned_buttons_, id,
                                  [](PinnedActionToolbarButton* button) {
                                    return button->GetActionId();
                                  }),
                container()->pinned_buttons_.end());
    }
  }

  void UpdateActionItem(const actions::ActionId& id) {
    auto* action = actions::ActionManager::Get().FindAction(
        id, BrowserActions::From(browser())->root_action_item());
    action->SetText(u"Test Action");
    action->SetTooltipText(u"Test Action");
    action->SetImage(ui::ImageModel::FromVectorIcon(
        features::IsRoundedIconsEnabled() ? vector_icons::kPetsIcon
                                          : vector_icons::kDogfoodOldIcon));
    action->SetVisible(true);
    action->SetEnabled(true);
    action->SetProperty(
        actions::kActionItemPinnableKey,
        std::to_underlying(actions::ActionPinnableState::kPinnable));
    action->SetInvokeActionCallback(base::DoNothing());
  }

  void WaitForAnimations() {
    views::test::WaitForAnimatingLayoutManager(container());
  }

  PinnedToolbarActionsModel* model() { return model_.get(); }

 private:
  raw_ptr<PinnedToolbarActionsModel> model_;
};

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerTest, ContainerMargins) {
  ASSERT_EQ(
      static_cast<PinnedToolbarActionsContainerLayout*>(
          container()->GetAnimatingLayoutManager()->target_layout_manager())
          ->interior_margin()
          .left(),
      0);
  ASSERT_EQ(
      static_cast<PinnedToolbarActionsContainerLayout*>(
          container()->GetAnimatingLayoutManager()->target_layout_manager())
          ->interior_margin()
          .right(),
      -GetLayoutConstant(LayoutConstant::kToolbarIconDefaultMargin));
}

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerTest, PinningAndUnpinning) {
  UpdateActionItem(actions::kActionCut);

  // Verify there are no pinned buttons.
  auto pinned_buttons = GetChildToolbarButtons();
  ASSERT_EQ(pinned_buttons.size(), 0u);
  // Verify pinning an action adds a button.
  model()->UpdatePinnedState(actions::kActionCut, true);
  pinned_buttons = GetChildToolbarButtons();
  ASSERT_EQ(pinned_buttons.size(), 1u);
  // Check the context menu. Callback should unpin the button.
  EXPECT_FALSE(pinned_buttons[0]->menu_model()->IsVisibleAt(0));
  EXPECT_TRUE(pinned_buttons[0]->menu_model()->IsVisibleAt(1));
  EXPECT_EQ(
      pinned_buttons[0]->menu_model()->GetLabelAt(1),
      l10n_util::GetStringUTF16(IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_UNPIN));
  // Verify clicking the button invokes the action.
  ASSERT_EQ(actions::ActionManager::Get()
                .FindAction(actions::kActionCut)
                ->GetInvokeCount(),
            0);
  pinned_buttons[0]->button_controller()->NotifyClick();
  ASSERT_EQ(actions::ActionManager::Get()
                .FindAction(actions::kActionCut)
                ->GetInvokeCount(),
            1);
  // Verify unpinning removes the button.
  model()->UpdatePinnedState(actions::kActionCut, false);
  WaitForAnimations();
  pinned_buttons = GetChildToolbarButtons();
  ASSERT_EQ(pinned_buttons.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerTest,
                       UnpinnedToolbarButtonsPoppedOutWhileActive) {
  UpdateActionItem(actions::kActionCut);

  // Verify there are no pinned buttons.
  auto toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 0u);
  // Verify an unpinned active action is popped out.
  container()->UpdateActionState(actions::kActionCut, true);
  CheckIsPoppedOut(actions::kActionCut, true);
  CheckIsPinned(actions::kActionCut, false);
  // Check the context menu. Callback should pin the button.
  toolbar_buttons = GetChildToolbarButtons();
  EXPECT_TRUE(toolbar_buttons[0]->menu_model()->IsVisibleAt(0));
  EXPECT_FALSE(toolbar_buttons[0]->menu_model()->IsVisibleAt(1));
  EXPECT_EQ(
      toolbar_buttons[0]->menu_model()->GetLabelAt(0),
      l10n_util::GetStringUTF16(IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_PIN));
  ASSERT_EQ(toolbar_buttons.size(), 1u);
  // Verify an unpinned inactive action is not popped out.
  container()->UpdateActionState(actions::kActionCut, false);
  WaitForAnimations();
  CheckIsPoppedOut(actions::kActionCut, false);
  CheckIsPinned(actions::kActionCut, false);
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerTest,
                       StateChangesBetweenPinnedandUnpinnedWhileActive) {
  UpdateActionItem(actions::kActionCut);

  // Verify there are no pinned buttons.
  auto toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 0u);
  // Verify an unpinned active action is popped out.
  container()->UpdateActionState(actions::kActionCut, true);
  CheckIsPoppedOut(actions::kActionCut, true);
  CheckIsPinned(actions::kActionCut, false);
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 1u);
  // Verify an active action is pinned if state changes.
  model()->UpdatePinnedState(actions::kActionCut, true);
  CheckIsPoppedOut(actions::kActionCut, false);
  CheckIsPinned(actions::kActionCut, true);
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 1u);
  // Verify an active action is popped out if unpinned.
  model()->UpdatePinnedState(actions::kActionCut, false);
  WaitForAnimations();
  CheckIsPoppedOut(actions::kActionCut, true);
  CheckIsPinned(actions::kActionCut, false);
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 1u);
}

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerTest,
                       PoppedOutButtonsAreAfterPinned) {
  UpdateActionItem(actions::kActionCut);
  UpdateActionItem(actions::kActionCopy);

  // Verify there are no pinned buttons.
  auto toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 0u);
  // Pin both and verify order matches the order they were added.
  model()->UpdatePinnedState(actions::kActionCut, true);
  model()->UpdatePinnedState(actions::kActionCopy, true);
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 2u);
  ASSERT_EQ(toolbar_buttons[0]->GetActionId(), actions::kActionCut);
  ASSERT_EQ(toolbar_buttons[1]->GetActionId(), actions::kActionCopy);
  // Active unpinned action should be placed after pinned actions.
  container()->UpdateActionState(actions::kActionCut, true);
  model()->UpdatePinnedState(actions::kActionCut, false);
  WaitForAnimations();
  CheckIsPoppedOut(actions::kActionCut, true);
  CheckIsPinned(actions::kActionCut, false);
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 2u);
  ASSERT_EQ(toolbar_buttons[0]->GetActionId(), actions::kActionCopy);
  ASSERT_EQ(toolbar_buttons[1]->GetActionId(), actions::kActionCut);
}

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerTest,
                       DividerNotVisibleWhileButtonPoppedOut) {
  UpdateActionItem(actions::kActionCut);

  // Divider should not be visible when no buttons are popped out.
  auto child_views = container()->children();
  ASSERT_EQ(child_views.size(), 1u);
  ASSERT_FALSE(child_views[0]->GetVisible());
  // Divider should still not be visible when a button is popped out.
  container()->UpdateActionState(actions::kActionCut, true);
  CheckIsPoppedOut(actions::kActionCut, true);
  CheckIsPinned(actions::kActionCut, false);
  child_views = container()->children();
  ASSERT_EQ(child_views.size(), 2u);
  ASSERT_EQ(
      static_cast<PinnedActionToolbarButton*>(child_views[1])->GetActionId(),
      actions::kActionCut);
  ASSERT_EQ(child_views[0]->GetProperty(views::kElementIdentifierKey),
            kPinnedToolbarActionsContainerDividerElementId);
  ASSERT_FALSE(child_views[0]->GetVisible());
}

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerTest,
                       AccessibleCheckedState) {
  UpdateActionItem(actions::kActionCut);
  model()->UpdatePinnedState(actions::kActionCut, true);
  auto pinned_action_buttons = GetChildToolbarButtons();

  ui::AXNodeData data;
  pinned_action_buttons[0]->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kFalse);

  data = ui::AXNodeData();
  pinned_action_buttons[0]->AddHighlight();
  pinned_action_buttons[0]->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kTrue);

  data = ui::AXNodeData();
  pinned_action_buttons[0]->ResetHighlight();
  pinned_action_buttons[0]->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kFalse);
}

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerTest, StatusIndicatorTest) {
  UpdateActionItem(actions::kActionCut);

  // Verify there are no pinned buttons.
  auto pinned_buttons = GetChildToolbarButtons();
  ASSERT_EQ(pinned_buttons.size(), 0u);
  // Verify pinning an action adds a button.
  model()->UpdatePinnedState(actions::kActionCut, true);
  pinned_buttons = GetChildToolbarButtons();
  ASSERT_EQ(pinned_buttons.size(), 1u);
  // Check the status indicator. It should not be visible.
  PinnedToolbarButtonStatusIndicator* status_indicator =
      pinned_buttons[0]->GetStatusIndicatorForTesting();
  EXPECT_EQ(status_indicator->GetVisible(), false);
  // Make indicator visible.
  pinned_buttons[0]->SetActionEngaged(true);
  pinned_buttons[0]->UpdateStatusIndicator();
  EXPECT_EQ(status_indicator->GetVisible(), true);
  // Hide indicator.
  pinned_buttons[0]->HideStatusIndicator();
  EXPECT_EQ(status_indicator->GetVisible(), false);
}

// TODO(crbug.com/544873874): Disabled on ChromeOS for reland. Re-enabled with
// fix in follow-up CL.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_MetricsRecordedForPinnableActions \
  DISABLED_MetricsRecordedForPinnableActions
#else
#define MAYBE_MetricsRecordedForPinnableActions \
  MetricsRecordedForPinnableActions
#endif
IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerTest,
                       MAYBE_MetricsRecordedForPinnableActions) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // Verify all pinnable buttons have a suffix listed in actions.xml.
  actions::ActionItemVector action_items;
  actions::ActionManager::Get().GetActions(
      action_items, BrowserActions::From(browser())->root_action_item());
  size_t pinnable_count =
      std::ranges::count_if(action_items, [](actions::ActionItem* action) {
        return action->GetProperty(actions::kActionItemPinnableKey) ==
               std::to_underlying(actions::ActionPinnableState::kPinnable);
      });
  const auto pinnable_action_variants = base::test::ReadActionVariantsForAction(
      "Actions.PinnedToolbarButtonActivation", ".");
  EXPECT_EQ(1U, pinnable_action_variants.size());
  // In browser tests:
  // * Only one of history or history clusters is registered.
  // * The split view action is not available via `root_action_item()`.
  // * Unlike in unit tests, desktop browser features (e.g. Lens Overlay and
  //   Contextual Tasks) are active and registered in `root_action_item()`.
  // * Tabs from other devices is not registered as the feature is disabled
  //   by default.
  // Overall, this results in a net deduction of 2 from the 27 action variants.
  size_t expected_pinnable_count = pinnable_action_variants[0].size() - 2;
#if BUILDFLAG(IS_CHROMEOS)
  // Downloads action item does not exist for ChromeOS.
  expected_pinnable_count -= 1;
#endif  // BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(pinnable_count, expected_pinnable_count);
}

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsContainerTest,
                       PinnedActionToolbarButtonPriorityTest) {
  UpdateActionItem(actions::kActionCut);
  model()->UpdatePinnedState(actions::kActionCut, true);

  auto toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 1u);
  auto* pinned_button = toolbar_buttons[0];

  // Verify that the initial priority is low.
  EXPECT_EQ(static_cast<PinnedToolbarActionFlexPriority>(
                pinned_button->GetProperty(kToolbarButtonFlexPriorityKey)),
            PinnedToolbarActionFlexPriority::kLow);

  // Verify setting the action as engaged updates the priority to medium.
  pinned_button->SetActionEngaged(true);
  EXPECT_EQ(static_cast<PinnedToolbarActionFlexPriority>(
                pinned_button->GetProperty(kToolbarButtonFlexPriorityKey)),
            PinnedToolbarActionFlexPriority::kMedium);

  // Verify that disengaging the action reverts the priority to low.
  pinned_button->SetActionEngaged(false);
  EXPECT_EQ(static_cast<PinnedToolbarActionFlexPriority>(
                pinned_button->GetProperty(kToolbarButtonFlexPriorityKey)),
            PinnedToolbarActionFlexPriority::kLow);

  // Verify that adding an anchor highlight raises the priority to high.
  std::optional<views::Button::ScopedAnchorHighlight> anchor_highlight =
      pinned_button->AddAnchorHighlight();
  EXPECT_EQ(static_cast<PinnedToolbarActionFlexPriority>(
                pinned_button->GetProperty(kToolbarButtonFlexPriorityKey)),
            PinnedToolbarActionFlexPriority::kHigh);

  // Verify setting the action to engaged while anchored stays high priority.
  pinned_button->SetActionEngaged(true);
  EXPECT_EQ(static_cast<PinnedToolbarActionFlexPriority>(
                pinned_button->GetProperty(kToolbarButtonFlexPriorityKey)),
            PinnedToolbarActionFlexPriority::kHigh);

  // Verify disengaging the action while anchored stays high priority.
  pinned_button->SetActionEngaged(false);
  EXPECT_EQ(static_cast<PinnedToolbarActionFlexPriority>(
                pinned_button->GetProperty(kToolbarButtonFlexPriorityKey)),
            PinnedToolbarActionFlexPriority::kHigh);

  // Verify that releasing the anchor returns priority to low.
  anchor_highlight.reset();
  EXPECT_EQ(static_cast<PinnedToolbarActionFlexPriority>(
                pinned_button->GetProperty(kToolbarButtonFlexPriorityKey)),
            PinnedToolbarActionFlexPriority::kLow);

  // Verify toggling the anchoring while the action is engaged ends with medium
  // priority.
  pinned_button->SetActionEngaged(true);
  EXPECT_EQ(static_cast<PinnedToolbarActionFlexPriority>(
                pinned_button->GetProperty(kToolbarButtonFlexPriorityKey)),
            PinnedToolbarActionFlexPriority::kMedium);
  anchor_highlight = pinned_button->AddAnchorHighlight();
  EXPECT_EQ(static_cast<PinnedToolbarActionFlexPriority>(
                pinned_button->GetProperty(kToolbarButtonFlexPriorityKey)),
            PinnedToolbarActionFlexPriority::kHigh);
  anchor_highlight.reset();
  EXPECT_EQ(static_cast<PinnedToolbarActionFlexPriority>(
                pinned_button->GetProperty(kToolbarButtonFlexPriorityKey)),
            PinnedToolbarActionFlexPriority::kMedium);
}
