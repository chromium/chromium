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
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_search/side_search_config.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
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
                  second_tab_url));

  // Register side search entry to second_tab.
  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* registry = SidePanelRegistry::Get(active_contents);
  registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kSideSearch, u"testing1", ui::ImageModel(),
      base::BindRepeating([]() { return std::make_unique<views::View>(); })));

  // Toggle side search entry to show on second_tab.
  auto* coordinator =
      SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
  coordinator->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_EQ(coordinator->GetComboboxDisplayedEntryIdForTesting(),
            SidePanelEntry::Id::kSideSearch);

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

IN_PROC_BROWSER_TEST_F(SidePanelInteractiveTest, ToggleSidePanelVisibility) {
  if (features::IsSidePanelPinningEnabled()) {
    GTEST_SKIP()
        << "Default sidepanel button is not present with pinning feature.";
  }

  RunTestSequence(
      // Ensure the side panel isn't open
      EnsureNotPresent(kSidePanelElementId),
      // Click on the toolbar button to open the side panel
      PressButton(kToolbarSidePanelButtonElementId),
      WaitForShow(kSidePanelElementId), FlushEvents(),
      // Click on the toolbar button to close the side panel
      PressButton(kToolbarSidePanelButtonElementId),
      WaitForHide(kSidePanelElementId),
      // Click on the toolbar button again open the side panel
      PressButton(kToolbarSidePanelButtonElementId),
      WaitForShow(kSidePanelElementId), FlushEvents(),
      // Click on the close button to dismiss the side panel
      PressButton(kSidePanelCloseButtonElementId),
      WaitForHide(kSidePanelElementId));
}

IN_PROC_BROWSER_TEST_F(SidePanelInteractiveTest,
                       SwitchBetweenDifferentEntries) {
  if (features::IsSidePanelPinningEnabled()) {
    GTEST_SKIP()
        << "Default sidepanel button is not present with pinning feature.";
  }

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBookmarksWebContentsId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kReadLaterWebContentsId);

  RunTestSequence(
      // Ensure the side panel isn't open
      EnsureNotPresent(kSidePanelElementId),
      // Click the toolbar button to open the side panel
      PressButton(kToolbarSidePanelButtonElementId),
      WaitForShow(kSidePanelElementId),
      // Switch to the bookmarks entry using the header combobox
      SelectDropdownItem(kSidePanelComboboxElementId,
                         static_cast<int>(SidePanelEntry::Id::kReadingList)),
      InstrumentNonTabWebView(kReadLaterWebContentsId,
                              kReadLaterSidePanelWebViewElementId),
      FlushEvents(),
      // Switch to the reading list entry using the header combobox
      SelectDropdownItem(kSidePanelComboboxElementId,
                         static_cast<int>(SidePanelEntry::Id::kBookmarks)),
      InstrumentNonTabWebView(kBookmarksWebContentsId,
                              kBookmarkSidePanelWebViewElementId),
      // Click on the close button to dismiss the side panel
      PressButton(kSidePanelCloseButtonElementId),
      WaitForHide(kSidePanelElementId),
      EnsureNotPresent(kBookmarkSidePanelWebViewElementId));
}

IN_PROC_BROWSER_TEST_F(SidePanelInteractiveTest,
                       StaysOpenOnTabSwitchWithActiveGlobalEntry) {
  if (features::IsSidePanelPinningEnabled()) {
    GTEST_SKIP()
        << "Default sidepanel button is not present with pinning feature.";
  }

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
      PressButton(kToolbarSidePanelButtonElementId),
      WaitForShow(kSidePanelElementId), FlushEvents(),
      // Switch to the first tab again with the side panel open
      SelectTab(kTabStripElementId, 0),
      // Ensure the side panel is still visible
      CheckViewProperty(kSidePanelElementId, &views::View::GetVisible, true),
      // Click on the close button to dismiss the side panel
      PressButton(kSidePanelCloseButtonElementId),
      WaitForHide(kSidePanelElementId));
}

IN_PROC_BROWSER_TEST_F(SidePanelInteractiveTest,
                       ReopensToLastActiveGlobalEntry) {
  // This test does not make sense with pinned feature the default toolbar
  // sidepanel button is not present to show the last active global entry.
  // A particular sidepanel has to be opened.
  if (features::IsSidePanelPinningEnabled()) {
    GTEST_SKIP()
        << "Default sidepanel button is not present with pinning feature.";
  }

  RunTestSequence(
      // Ensure the side panel isn't open
      EnsureNotPresent(kSidePanelElementId),
      // Click the toolbar button to open the side panel
      PressButton(kToolbarSidePanelButtonElementId),
      WaitForShow(kSidePanelElementId),
      // Switch to the bookmarks entry using the header combobox
      SelectDropdownItem(kSidePanelComboboxElementId,
                         static_cast<int>(SidePanelEntry::Id::kBookmarks)),
      WaitForShow(kBookmarkSidePanelWebViewElementId), FlushEvents(),
      // Click on the close button to dismiss the side panel
      PressButton(kSidePanelCloseButtonElementId),
      WaitForHide(kSidePanelElementId), FlushEvents(),
      // Click on the toolbar button again open the side panel
      PressButton(kToolbarSidePanelButtonElementId),
      // Verify the bookmarks side panel entry is shown (last seen)
      WaitForShow(kBookmarkSidePanelWebViewElementId),
      EnsureNotPresent(kReadLaterSidePanelWebViewElementId));
}

// Test case for menus that only appear with the kSidePanelPinning feature
// enabled.
class PinnedSidePanelInteractiveTest : public InteractiveBrowserTest {
 public:
  PinnedSidePanelInteractiveTest() = default;
  ~PinnedSidePanelInteractiveTest() override = default;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    scoped_feature_list_.InitWithFeatures(
        {features::kSidePanelPinning, features::kChromeRefresh2023,
         features::kReadAnything},
        {});
    InteractiveBrowserTest::SetUp();
  }

  auto OpenBookmarksSidePanel() {
    // TODO(crbug/1495440): When initially writing this step, opening the
    // bookmarks submenu is flaky and sometimes causes a crash but the crash
    // doesn't seem reproducible anymore. Unsure if the crash was fixed so will
    // need to track down cause of crash if this step becomes flaky again.
    return Steps(
        PressButton(kToolbarAppMenuButtonElementId),
        SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
        SelectMenuItem(BookmarkSubMenuModel::kShowBookmarkSidePanelItem),
        WaitForShow(kSidePanelElementId), FlushEvents());
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
    return Steps(Do(base::BindLambdaForTesting([=]() {
                   chrome::ExecuteCommand(browser(),
                                          IDC_SHOW_READING_MODE_SIDE_PANEL);
                 })),
                 WaitForShow(kSidePanelElementId), FlushEvents());
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verify that we can open the ReadingMode side panel from the 3dot -> More
// tools context menu.
IN_PROC_BROWSER_TEST_F(PinnedSidePanelInteractiveTest,
                       OpenReadingModeSidePanel) {
  // Replace the contents of the ReadingMode side panel with an empty view so it
  // loads faster.
  auto* registry = SidePanelCoordinator::GetGlobalSidePanelRegistry(browser());
  registry->Deregister(SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));
  registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kReadAnything, u"testing1", ui::ImageModel(),
      base::BindRepeating([]() { return std::make_unique<views::View>(); })));

  SidePanelCoordinator* const coordinator =
      SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());

  chrome::ExecuteCommand(browser(), IDC_SHOW_READING_MODE_SIDE_PANEL);

  EXPECT_EQ(SidePanelEntryKey(SidePanelEntryId::kReadAnything),
            coordinator->GetCurrentSidePanelEntryForTesting()->key());
}

// Verify that we can open the history cluster side panel from the app menu.
IN_PROC_BROWSER_TEST_F(PinnedSidePanelInteractiveTest,
                       OpenHistoryClusterSidePanel) {
  auto* registry = SidePanelCoordinator::GetGlobalSidePanelRegistry(browser());
  registry->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kHistoryClusters));
  registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kHistoryClusters, u"testing1", ui::ImageModel(),
      base::BindRepeating([]() { return std::make_unique<views::View>(); })));

  SidePanelCoordinator* const coordinator =
      SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());

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
      CheckPinButtonToggleState(true),
      PressButton(kSidePanelCloseButtonElementId),
      WaitForHide(kSidePanelElementId), FlushEvents(),
      CheckActionPinnedToToolbar(kActionSidePanelShowBookmarks, true),
      OpenBookmarksSidePanel(),
      CheckActionPinnedToToolbar(kActionSidePanelShowBookmarks, true),
      CheckPinButtonToggleState(true),
      // Unpin the bookmarks side panel
      PressButton(kSidePanelPinButtonElementId),
      CheckActionPinnedToToolbar(kActionSidePanelShowBookmarks, false),
      CheckPinButtonToggleState(false));
}

IN_PROC_BROWSER_TEST_F(PinnedSidePanelInteractiveTest,
                       SidePanelPinButtonsHideInIncognitoMode) {
  Browser* const incognito = CreateIncognitoBrowser();
  RunTestSequence(
      InContext(incognito->window()->GetElementContext(),
                WaitForShow(kBrowserViewElementId)),
      InSameContext(Steps(ActivateSurface(kBrowserViewElementId), FlushEvents(),
                          EnsureNotPresent(kSidePanelElementId),
                          OpenBookmarksSidePanel(),
                          EnsureNotPresent(kSidePanelPinButtonElementId))));
}

IN_PROC_BROWSER_TEST_F(PinnedSidePanelInteractiveTest,
                       PinnedToolbarButtonsHighlightWhileSidePanelVisible) {
  // Replace the contents of the ReadingMode side panel with an empty view so it
  // loads faster.
  auto* registry = SidePanelCoordinator::GetGlobalSidePanelRegistry(browser());
  registry->Deregister(SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));
  registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kReadAnything, u"testing1", ui::ImageModel(),
      base::BindRepeating([]() { return std::make_unique<views::View>(); })));

  PinnedToolbarActionsModel* const actions_model =
      PinnedToolbarActionsModel::Get(browser()->profile());

  actions_model->UpdatePinnedState(kActionSidePanelShowBookmarks, true);

  RunTestSequence(
      // Verify side panel is closed.
      EnsureNotPresent(kSidePanelElementId),
      // Verify bookmarks is pinned to the toolbar.
      Check(base::BindLambdaForTesting([=]() {
        return actions_model->Contains(kActionSidePanelShowBookmarks);
      })),
      CheckView(
          kPinnedToolbarActionsContainerElementId,
          [](views::View* view) { return view->children().size() == 2u; }),
      CheckViewProperty(kPinnedToolbarActionsContainerDividerElementId,
                        &views::View::GetVisible, true),
      // Verify the bookmarks pinned toolbar button is not highlighted.
      CheckPinnedToolbarActionsContainerChildInkDropState(0, false),
      // Open the bookmarks side panel.
      OpenBookmarksSidePanel(),
      // Verify the bookmarks pinned toolbar button is highlighted.
      CheckPinnedToolbarActionsContainerChildInkDropState(0, true),
      // Close the side panel.
      PressButton(kSidePanelCloseButtonElementId),
      WaitForHide(kSidePanelElementId), FlushEvents(),
      // Verify the bookmarks pinned toolbar button is not highlighted.
      CheckPinnedToolbarActionsContainerChildInkDropState(0, false),
      // Open non-bookmarks side panel.
      OpenReadingModeSidePanel(),
      // Verify the bookmarks pinned toolbar button is not highlighted.
      CheckPinnedToolbarActionsContainerChildInkDropState(0, false));
}
