// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/side_search/side_search_config.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/polling_state_observer.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "url/gurl.h"

class SidePanelInteractiveTest : public InteractiveBrowserTest {
 public:
  SidePanelInteractiveTest() = default;
  ~SidePanelInteractiveTest() override = default;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    InteractiveBrowserTest::SetUp();
  }
};

// This test is specifically to guard against this regression
// (crbug.com/1428606).
IN_PROC_BROWSER_TEST_F(SidePanelInteractiveTest, SidePanelNotShownOnPwa) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabElementId);
  GURL second_tab_url("https://test.com");
  auto* coordinator = browser()->GetFeatures().side_panel_coordinator();

  RunTestSequence(
      // Add a second tab to the tab strip
      AddInstrumentedTab(kSecondTabElementId, second_tab_url),
      CheckResult(base::BindLambdaForTesting([this]() {
                    return browser()->tab_strip_model()->active_index();
                  }),
                  testing::Eq(1)),
      // Ensure the side panel isn't open
      EnsureNotPresent(kSidePanelElementId),
      CheckResult(base::BindLambdaForTesting([this]() {
                    return browser()
                        ->tab_strip_model()
                        ->GetActiveWebContents()
                        ->GetLastCommittedURL();
                  }),
                  second_tab_url),
      Do(base::BindLambdaForTesting([=, this]() {
        auto* registry = browser()
                             ->GetActiveTabInterface()
                             ->GetTabFeatures()
                             ->side_panel_registry();
        registry->Register(std::make_unique<SidePanelEntry>(
            SidePanelEntry::Id::kCustomizeChrome, base::BindRepeating([]() {
              return std::make_unique<views::View>();
            })));
        coordinator->Show(SidePanelEntry::Id::kCustomizeChrome);
      })),
      WaitForShow(kSidePanelElementId),
      CheckResult(base::BindLambdaForTesting([coordinator]() {
                    return coordinator->IsSidePanelEntryShowing(
                        SidePanelEntryKey(SidePanelEntryId::kCustomizeChrome));
                  }),
                  true));

  // Install an app using second_tab_url.
  auto app_id = web_app::test::InstallDummyWebApp(browser()->profile(),
                                                  "App Name", second_tab_url);

  // Move second_tab contents to app, simulating open pwa from omnibox intent
  // picker.
  Browser* app_browser = web_app::ReparentWebContentsIntoAppBrowser(
      browser()->tab_strip_model()->GetActiveWebContents(), app_id);
  EXPECT_TRUE(app_browser->is_type_app());

  // App does not show side panel.
  EXPECT_FALSE(BrowserView::GetBrowserViewForBrowser(app_browser)
                   ->unified_side_panel()
                   ->GetVisible());
}

// Test case for menus that only appear with the kSidePanelPinning feature
// enabled.
class PinnedSidePanelInteractiveTest : public InteractiveBrowserTest {
 public:
  PinnedSidePanelInteractiveTest() = default;
  ~PinnedSidePanelInteractiveTest() override = default;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    PinnedToolbarActionsModel* const actions_model =
        PinnedToolbarActionsModel::Get(browser()->profile());
    actions_model->UpdatePinnedState(kActionShowChromeLabs, false);
    views::test::WaitForAnimatingLayoutManager(
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->pinned_toolbar_actions_container());
  }

  auto OpenBookmarksSidePanel() {
    // TODO(crbug.com/40286543): When initially writing this step, opening the
    // bookmarks submenu is flaky and sometimes causes a crash but the crash
    // doesn't seem reproducible anymore. Unsure if the crash was fixed so will
    // need to track down cause of crash if this step becomes flaky again.
    return Steps(
        PressButton(kToolbarAppMenuButtonElementId),
        SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
        SelectMenuItem(BookmarkSubMenuModel::kShowBookmarkSidePanelItem),
        WaitForShow(kSidePanelElementId));
  }

  auto CheckActionPinnedToToolbar(const actions::ActionId& id,
                                  bool should_pin) {
    return CheckResult(
        [&]() {
          PinnedToolbarActionsContainer* const
              pinned_toolbar_actions_container =
                  BrowserView::GetBrowserViewForBrowser(browser())
                      ->toolbar()
                      ->pinned_toolbar_actions_container();
          return pinned_toolbar_actions_container->GetPinnedButtonFor(id) !=
                 nullptr;
        },
        should_pin);
  }

  auto CheckPinButtonToggleState(bool should_toggle) {
    return CheckViewProperty(kSidePanelPinButtonElementId,
                             &views::ToggleImageButton::GetToggled,
                             should_toggle);
  }

  PinnedToolbarActionsContainer* GetPinnedToolbarActionsContainer() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->pinned_toolbar_actions_container();
  }

  auto OpenReadingModeSidePanel() {
    return Steps(Do(base::BindLambdaForTesting([=, this]() {
                   chrome::ExecuteCommand(browser(),
                                          IDC_SHOW_READING_MODE_SIDE_PANEL);
                 })),
                 WaitForShow(kSidePanelElementId));
  }

  auto OpenCustomizeChromeSidePanel() {
    return Steps(Do(base::BindLambdaForTesting([=, this]() {
                   chrome::ExecuteCommand(browser(),
                                          IDC_SHOW_CUSTOMIZE_CHROME_SIDE_PANEL);
                 })),
                 WaitForShow(kSidePanelElementId));
  }

  auto CheckPinnedToolbarActionsContainerChildInkDropState(int child_index,
                                                           bool is_active) {
    return Steps(CheckResult(base::BindLambdaForTesting([this, child_index]() {
                               return views::InkDrop::Get(
                                          GetPinnedToolbarActionsContainer()
                                              ->children()[child_index])
                                          ->GetInkDrop()
                                          ->GetTargetInkDropState() ==
                                      views::InkDropState::ACTIVATED;
                             }),
                             is_active));
  }

  auto ShowSidePanelForKey(SidePanelEntryKey key) {
    return Do(base::BindLambdaForTesting([=, this]() {
      browser()->GetFeatures().side_panel_coordinator()->Show(key);
    }));
  }
};

// Verify that we can open the ReadingMode side panel from the 3dot -> More
// tools context menu.
IN_PROC_BROWSER_TEST_F(PinnedSidePanelInteractiveTest,
                       OpenReadingModeSidePanel) {
  // Replace the contents of the ReadingMode side panel with an empty view so it
  // loads faster.
  auto* registry =
      browser()->GetFeatures().side_panel_coordinator()->GetWindowRegistry();
  registry->Deregister(SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));
  registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kReadAnything,
      base::BindRepeating([]() { return std::make_unique<views::View>(); })));

  SidePanelCoordinator* const coordinator =
      browser()->GetFeatures().side_panel_coordinator();
  coordinator->SetNoDelaysForTesting(true);

  chrome::ExecuteCommand(browser(), IDC_SHOW_READING_MODE_SIDE_PANEL);

  EXPECT_EQ(SidePanelEntryKey(SidePanelEntryId::kReadAnything),
            coordinator->GetCurrentSidePanelEntryForTesting()->key());
}

// Verify that we can open the CustomizeChrome side panel from the 3dot -> More
// tools context menu.
IN_PROC_BROWSER_TEST_F(PinnedSidePanelInteractiveTest,
                       OpenCustomizeChromeSidePanel) {
  // Replace the contents of the CustomizeChrome side panel with an empty view
  // so it loads faster.
  auto* registry = browser()
                       ->GetActiveTabInterface()
                       ->GetTabFeatures()
                       ->side_panel_registry();
  registry->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kCustomizeChrome));
  registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kCustomizeChrome,
      base::BindRepeating([]() { return std::make_unique<views::View>(); })));

  SidePanelCoordinator* const coordinator =
      browser()->GetFeatures().side_panel_coordinator();
  coordinator->SetNoDelaysForTesting(true);

  RunTestSequence(
      EnsureNotPresent(kSidePanelElementId), OpenCustomizeChromeSidePanel(),
      CheckResult(base::BindLambdaForTesting([coordinator]() {
                    return coordinator->IsSidePanelEntryShowing(
                        SidePanelEntryKey(SidePanelEntryId::kCustomizeChrome));
                  }),
                  true));
}

// Verify that we can open the history cluster side panel from the app menu.
IN_PROC_BROWSER_TEST_F(PinnedSidePanelInteractiveTest,
                       OpenHistoryClusterSidePanel) {
  auto* registry =
      browser()->GetFeatures().side_panel_coordinator()->GetWindowRegistry();
  registry->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kHistoryClusters));
  registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kHistoryClusters,
      base::BindRepeating([]() { return std::make_unique<views::View>(); })));

  SidePanelCoordinator* const coordinator =
      browser()->GetFeatures().side_panel_coordinator();

  chrome::ExecuteCommand(browser(), IDC_SHOW_HISTORY_CLUSTERS_SIDE_PANEL);

  EXPECT_EQ(SidePanelEntryKey(SidePanelEntryId::kHistoryClusters),
            coordinator->GetCurrentSidePanelEntryForTesting()->key());
}

IN_PROC_BROWSER_TEST_F(PinnedSidePanelInteractiveTest,
                       PanelPinnedStateUpdatesOnPinButtonPress) {
  RunTestSequence(
      EnsureNotPresent(kSidePanelElementId), OpenBookmarksSidePanel(),
      CheckPinButtonToggleState(false),
      // Pin the bookmarks side panel
      PressButton(kSidePanelPinButtonElementId),
      CheckActionPinnedToToolbar(kActionSidePanelShowBookmarks, true),
      WaitForShow(kPinnedToolbarActionsContainerDividerElementId),
      CheckPinButtonToggleState(true),
      PressButton(kSidePanelCloseButtonElementId),
      WaitForHide(kSidePanelElementId),
      CheckActionPinnedToToolbar(kActionSidePanelShowBookmarks, true),
      OpenBookmarksSidePanel(),
      CheckActionPinnedToToolbar(kActionSidePanelShowBookmarks, true),
      CheckPinButtonToggleState(true),
      // Unpin the bookmarks side panel
      PressButton(kSidePanelPinButtonElementId),
      CheckActionPinnedToToolbar(kActionSidePanelShowBookmarks, false),
      WaitForHide(kPinnedToolbarActionsContainerDividerElementId),
      CheckPinButtonToggleState(false));
}

IN_PROC_BROWSER_TEST_F(PinnedSidePanelInteractiveTest,
                       SidePanelPinButtonsHideInIncognitoMode) {
  Browser* const incognito = CreateIncognitoBrowser();
  RunTestSequence(
      InContext(incognito->window()->GetElementContext(),
                WaitForShow(kBrowserViewElementId)),
      InSameContext(Steps(ActivateSurface(kBrowserViewElementId),
                          EnsureNotPresent(kSidePanelElementId),
                          OpenBookmarksSidePanel(),
                          EnsureNotPresent(kSidePanelPinButtonElementId))));
}

IN_PROC_BROWSER_TEST_F(PinnedSidePanelInteractiveTest,
                       PinnedToolbarButtonsHighlightWhileSidePanelVisible) {
  // Replace the contents of the ReadingMode side panel with an empty view so it
  // loads faster.
  auto* registry =
      browser()->GetFeatures().side_panel_coordinator()->GetWindowRegistry();
  registry->Deregister(SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));
  registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kReadAnything,
      base::BindRepeating([]() { return std::make_unique<views::View>(); })));

  PinnedToolbarActionsModel* const actions_model =
      PinnedToolbarActionsModel::Get(browser()->profile());

  actions_model->UpdatePinnedState(kActionSidePanelShowBookmarks, true);

  RunTestSequence(
      // Verify side panel is closed.
      EnsureNotPresent(kSidePanelElementId),
      // Verify the bookmarks pinned toolbar button is not highlighted.
      CheckPinnedToolbarActionsContainerChildInkDropState(0, false),
      // Open the bookmarks side panel.
      OpenBookmarksSidePanel(),
      // Verify bookmarks is pinned to the toolbar.
      Check(base::BindLambdaForTesting([=]() {
        return actions_model->Contains(kActionSidePanelShowBookmarks);
      })),
      WaitForShow(kPinnedActionToolbarButtonElementId),
      CheckView(
          kPinnedToolbarActionsContainerElementId,
          [](views::View* view) { return view->children().size() == 2u; }),
      // Verify the bookmarks pinned toolbar button is highlighted.
      CheckPinnedToolbarActionsContainerChildInkDropState(0, true),
      CheckViewProperty(kPinnedToolbarActionsContainerDividerElementId,
                        &views::View::GetVisible, true),
      // Close the side panel.
      PressButton(kSidePanelCloseButtonElementId),
      WaitForHide(kSidePanelElementId),
      // Verify the bookmarks pinned toolbar button is not highlighted.
      CheckPinnedToolbarActionsContainerChildInkDropState(0, false),
      // Open non-bookmarks side panel.
      OpenReadingModeSidePanel(),
      // Verify the bookmarks pinned toolbar button is not highlighted.
      CheckPinnedToolbarActionsContainerChildInkDropState(0, false));
}

IN_PROC_BROWSER_TEST_F(PinnedSidePanelInteractiveTest,
                       ToggleSidePanelVisibility) {
  RunTestSequence(
      // Ensure the side panel isn't open
      EnsureNotPresent(kSidePanelElementId),
      EnsureNotPresent(kPinnedActionToolbarButtonElementId),
      // Open bookmarks sidepanel
      OpenBookmarksSidePanel(), WaitForShow(kSidePanelElementId),
      WaitForShow(kPinnedToolbarActionsContainerElementId),
      // Pin the button
      CheckPinButtonToggleState(false),
      PressButton(kSidePanelPinButtonElementId),
      CheckActionPinnedToToolbar(kActionSidePanelShowBookmarks, true),
      EnsurePresent(kPinnedToolbarActionsContainerElementId),
      WaitForShow(kPinnedActionToolbarButtonElementId),
      // Toggle side panel
      PressButton(kPinnedActionToolbarButtonElementId),
      WaitForHide(kSidePanelElementId));
}

IN_PROC_BROWSER_TEST_F(PinnedSidePanelInteractiveTest,
                       SwitchBetweenDifferentEntries) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBookmarksWebContentsId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kReadLaterWebContentsId);
  auto* coordinator = browser()->GetFeatures().side_panel_coordinator();

  RunTestSequence(
      // Ensure the side panel isn't open
      EnsureNotPresent(kSidePanelElementId),
      // Click the toolbar button to open the side panel
      OpenBookmarksSidePanel(), WaitForShow(kSidePanelElementId),
      InstrumentNonTabWebView(kBookmarksWebContentsId,
                              kBookmarkSidePanelWebViewElementId),

      CheckResult(base::BindLambdaForTesting([coordinator]() {
                    return coordinator->IsSidePanelEntryShowing(
                        SidePanelEntryKey(SidePanelEntryId::kBookmarks));
                  }),
                  true),
      // Switch to the reading list entry.
      ShowSidePanelForKey(SidePanelEntryKey(SidePanelEntry::Id::kReadingList)),
      InstrumentNonTabWebView(kReadLaterWebContentsId,
                              kReadLaterSidePanelWebViewElementId),

      CheckResult(base::BindLambdaForTesting([coordinator]() {
                    return coordinator->IsSidePanelEntryShowing(
                        SidePanelEntryKey(SidePanelEntryId::kReadingList));
                  }),
                  true),
      // Click on the close button to dismiss the side panel
      PressButton(kSidePanelCloseButtonElementId),
      WaitForHide(kSidePanelElementId));
}

IN_PROC_BROWSER_TEST_F(PinnedSidePanelInteractiveTest,
                       StaysOpenOnTabSwitchWithActiveGlobalEntry) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabElementId);

  RunTestSequence(
      // Add a second tab to the tab strip
      AddInstrumentedTab(kSecondTabElementId, GURL(url::kAboutBlankURL)),
      CheckResult(base::BindLambdaForTesting([this]() {
                    return browser()->tab_strip_model()->active_index();
                  }),
                  testing::Eq(1)),
      // Ensure the side panel isn't open
      EnsureNotPresent(kSidePanelElementId),
      // Click the toolbar button to open the side panel
      OpenBookmarksSidePanel(), WaitForShow(kSidePanelElementId),
      // Switch to the first tab again with the side panel open
      SelectTab(kTabStripElementId, 0),
      // Ensure the side panel is still visible
      CheckViewProperty(kSidePanelElementId, &views::View::GetVisible, true),
      // Click on the close button to dismiss the side panel
      PressButton(kSidePanelCloseButtonElementId),
      WaitForHide(kSidePanelElementId));
}

IN_PROC_BROWSER_TEST_F(PinnedSidePanelInteractiveTest,
                       ToolbarButtonDisappearsOnEntryDeregister) {
  constexpr char kBookmarksButton[] = "bookmarks_button";
  RunTestSequence(
      // Ensure the side panel isn't open
      EnsureNotPresent(kSidePanelElementId),
      EnsureNotPresent(kPinnedActionToolbarButtonElementId),
      // Open bookmarks sidepanel
      OpenBookmarksSidePanel(), WaitForShow(kSidePanelElementId),
      WaitForShow(kPinnedToolbarActionsContainerElementId),
      WaitForShow(kPinnedActionToolbarButtonElementId),
      NameChildViewByType<PinnedActionToolbarButton>(
          kPinnedToolbarActionsContainerElementId, kBookmarksButton),
      WaitForShow(kBookmarksButton),
      // Deregister the entry and verify the side panel and ephemeral toolbar
      // button are hidden.
      Do([this]() {
        browser()
            ->GetFeatures()
            .side_panel_coordinator()
            ->GetWindowRegistry()
            ->Deregister(SidePanelEntry::Key(SidePanelEntry::Id::kBookmarks));
      }),
      WaitForHide(kSidePanelElementId), WaitForHide(kBookmarksButton));
}
