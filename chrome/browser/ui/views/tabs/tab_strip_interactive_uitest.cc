// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/test/tab_strip_interactive_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/interaction/polling_view_observer.h"
#include "url/gurl.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabContents);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabContents);

constexpr char kDocumentWithTitle[] = "/title3.html";

}  // namespace

class TabStripInteractiveUiTest
    : public TabStripInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  ~TabStripInteractiveUiTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  BrowserView* GetBrowserView() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }
};

IN_PROC_BROWSER_TEST_F(TabStripInteractiveUiTest, HoverEffectShowsOnMouseOver) {
  using Observer = views::test::PollingViewObserver<bool, TabStrip>;
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(Observer, kTabStripHoverState);
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents,
                          embedded_test_server()->GetURL(kDocumentWithTitle)),
      AddInstrumentedTab(kSecondTabContents,
                         embedded_test_server()->GetURL(kDocumentWithTitle)),
      HoverTabAt(0), FinishTabstripAnimations(),
      PollView(kTabStripHoverState, kTabStripElementId,
               [](const TabStrip* tab_strip) -> bool {
                 return tab_strip->tab_at(0)
                            ->tab_style_views()
                            ->GetHoverAnimationValue() > 0;
               }),
      WaitForState(kTabStripHoverState, true));
}

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<int>,
                                    kTabCountState);

class TabStripTabGroupMenuMoreEntryPointsEnabled
    : public TabStripInteractiveUiTest {
 public:
  TabStripTabGroupMenuMoreEntryPointsEnabled() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kTabGroupMenuMoreEntryPoints);
  }

  TabStrip* tabstrip() {
    return views::AsViewClass<TabStripRegionView>(
               browser()->GetBrowserView().tab_strip_view())
        ->tab_strip();
  }
  TabStripController* controller() { return tabstrip()->controller(); }

  auto WaitForTabCount(Browser* browser, int expected_count) {
    return Steps(
        PollState(kTabCountState,
                  [browser]() { return browser->tab_strip_model()->count(); }),
        WaitForState(kTabCountState, expected_count),
        StopObservingState(kTabCountState));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO (crbug.com/447617263) rewrite these tests so that they work on mac and
// enable them there so that it works on mac and re-enable it.
#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(TabStripTabGroupMenuMoreEntryPointsEnabled,
                       VerifyNewTabButtonContextMenu) {
  RunTestSequence(
      FinishTabstripAnimations(), EnsurePresent(kNewTabButtonElementId),
      MoveMouseTo(kNewTabButtonElementId),
      MayInvolveNativeContextMenu(
          ClickMouse(ui_controls::RIGHT),
          EnsurePresent(NewTabButtonMenuModel::kNewTab),
          EnsurePresent(NewTabButtonMenuModel::kNewTabInGroup),
          EnsurePresent(NewTabButtonMenuModel::kCreateNewTabGroup),
          SendAccelerator(NewTabButtonMenuModel::kNewTab,
                          ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE))));
}

IN_PROC_BROWSER_TEST_F(TabStripTabGroupMenuMoreEntryPointsEnabled,
                       NewTabButtonNewTabInGroupDisabledWhenNoOpenGroups) {
  RunTestSequence(
      EnsurePresent(kNewTabButtonElementId),
      MoveMouseTo(kNewTabButtonElementId),
      MayInvolveNativeContextMenu(
          ClickMouse(ui_controls::RIGHT),
          WaitForShow(NewTabButtonMenuModel::kNewTabInGroup),
          WaitForViewProperty(NewTabButtonMenuModel::kNewTabInGroup,
                              views::View, Enabled, false),
          SendAccelerator(NewTabButtonMenuModel::kNewTab,
                          ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE))));
}

IN_PROC_BROWSER_TEST_F(TabStripTabGroupMenuMoreEntryPointsEnabled,
                       NewTabButtonNewTabInMostRecentGroup) {
  controller()->CreateNewTab();
  controller()->CreateNewTab();
  controller()->CreateNewTab();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 4);

  browser()->tab_strip_model()->AddToNewGroup({1});
  tab_groups::TabGroupId group =
      browser()->tab_strip_model()->AddToNewGroup({2});
  browser()->tab_strip_model()->AddToNewGroup({3});

  RunTestSequence(
      FinishTabstripAnimations(), SelectTab(kTabStripElementId, 1),
      SelectTab(kTabStripElementId, 2), SelectTab(kTabStripElementId, 3),
      SelectTab(kTabStripElementId, 2), SelectTab(kTabStripElementId, 0),
      MoveMouseTo(kNewTabButtonElementId),
      MayInvolveNativeContextMenu(
          ClickMouse(ui_controls::RIGHT),
          SelectMenuItem(NewTabButtonMenuModel::kNewTabInGroup)),
      WaitForTabCount(browser(), 5),
      CheckResult(
          [&]() {
            // Check that the most recent group got an extra tab.
            return tab_strip_model->group_model()
                ->GetTabGroup(group)
                ->tab_count();
          },
          2));
}
#endif  // !BUILDFLAG(IS_MAC)
