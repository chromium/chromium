// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/projects/projects_panel_state_controller.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/saved_tab_groups/public/features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/test/ui_controls.h"
#include "ui/compositor/layer.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/view_shadow.h"

namespace base::test {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabElementId);

class ProjectsPanelInteractiveUiTest : public InteractiveBrowserTest {
 public:
  ProjectsPanelInteractiveUiTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{tabs::kVerticalTabs, {}},
         {tab_groups::kProjectsPanel,
          {{tab_groups::kProjectsPanelWithThreads.name, "true"}}}},
        {});
    ProjectsPanelView::set_threads_visible_for_testing(true);
    ProjectsPanelView::disable_animations_for_testing();
  }
  ~ProjectsPanelInteractiveUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    // Enter Vertical Tabs mode.
    tabs::VerticalTabStripStateController::From(browser())
        ->SetVerticalTabsEnabled(true);
    RunScheduledLayouts();
  }

  auto OpenProjectsPanel() {
    return Steps(WaitForShow(kVerticalTabStripProjectsButtonElementId),
                 PressButton(kVerticalTabStripProjectsButtonElementId),
                 Do([this]() { RunScheduledLayouts(); }),
                 WaitForShow(kProjectsPanelViewElementId),
                 Do([this]() { RunScheduledLayouts(); }));
  }

  auto ResizeVerticalTabsRegionToWidth(int width) {
    return Do([this, width]() {
      auto* vt_region_view =
          browser_view()->vertical_tab_strip_region_view_for_testing();
      vt_region_view->OnResize(width - vt_region_view->width(), true);
    });
  }

  auto CheckPanelHasExpectedWidthAndStyling(int expected_width,
                                            bool should_have_rounded_corners) {
    return Steps(CheckResult(
                     [this]() {
                       return browser_view()
                           ->projects_panel_container_for_testing()
                           ->bounds()
                           .width();
                     },
                     expected_width),
                 CheckResult(
                     [this]() {
                       return browser_view()
                           ->projects_panel_container_for_testing()
                           ->content_container_for_testing()
                           ->layer()
                           ->rounded_corner_radii()
                           .IsEmpty();
                     },
                     !should_have_rounded_corners));
  }

  ProjectsPanelStateController* projects_panel_state_controller() {
    return ProjectsPanelStateController::From(browser());
  }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test checks that the projects panel closes when clicking outside.
IN_PROC_BROWSER_TEST_F(ProjectsPanelInteractiveUiTest, CloseOnClickOutside) {
  RunTestSequence(
      // Verify Vertical Tabs is showing.
      WaitForShow(kVerticalTabStripTopContainerElementId),
      // Verify Initial State for Projects Panel.
      CheckResult(
          [this]() {
            return projects_panel_state_controller()->IsProjectsPanelVisible();
          },
          false),
      // Click Projects Panel Button and Verify Visibilities.
      EnsurePresent(kVerticalTabStripProjectsButtonElementId),
      MoveMouseTo(kVerticalTabStripProjectsButtonElementId), ClickMouse(),
      CheckResult(
          [this]() {
            return projects_panel_state_controller()->IsProjectsPanelVisible();
          },
          true),
      Do([this]() { RunScheduledLayouts(); }),
      WaitForShow(kProjectsPanelViewElementId),
      // Click on the Omnibox (outside the panel).
      MoveMouseTo(kOmniboxElementId), ClickMouse(),
      // Verify Projects Panel is hidden.
      Do([this]() { RunScheduledLayouts(); }),
      WaitForHide(kProjectsPanelViewElementId),
      CheckResult(
          [this]() {
            return projects_panel_state_controller()->IsProjectsPanelVisible();
          },
          false));
}

// This test checks that the projects panel grabs focus when opened.
IN_PROC_BROWSER_TEST_F(ProjectsPanelInteractiveUiTest, GrabsFocusOnOpen) {
  RunTestSequence(OpenProjectsPanel(),
                  CheckViewProperty(kProjectsPanelViewElementId,
                                    &views::View::HasFocus, true));
}

// This test checks that the projects panel closes when focus is switched to
// another UI element like the omnibox.
IN_PROC_BROWSER_TEST_F(ProjectsPanelInteractiveUiTest, ClosesOnFocusLost) {
  RunTestSequence(
      OpenProjectsPanel(),
      // Focus the omnibox.
      FocusElement(kOmniboxElementId),
      // Verify Projects Panel is hidden.
      WaitForHide(kProjectsPanelViewElementId),
      CheckResult(
          [this]() {
            return projects_panel_state_controller()->IsProjectsPanelVisible();
          },
          false));
}

// This is a regression test that checks that the panel stays open when clicking
// inside (but not on a button or other interactive element).
IN_PROC_BROWSER_TEST_F(ProjectsPanelInteractiveUiTest, StaysOpenOnClickInside) {
  RunTestSequence(
      // Verify Vertical Tabs is showing.
      WaitForShow(kVerticalTabStripTopContainerElementId),
      // Verify Initial State for Projects Panel.
      CheckResult(
          [this]() {
            return projects_panel_state_controller()->IsProjectsPanelVisible();
          },
          false),
      // Click Projects Panel Button and Verify Visibilities.
      EnsurePresent(kVerticalTabStripProjectsButtonElementId),
      MoveMouseTo(kVerticalTabStripProjectsButtonElementId), ClickMouse(),
      CheckResult(
          [this]() {
            return projects_panel_state_controller()->IsProjectsPanelVisible();
          },
          true),
      Do([this]() { RunScheduledLayouts(); }),
      WaitForShow(kProjectsPanelViewElementId),
      // Click on the Tab groups list title (inside the panel).
      MoveMouseTo(kProjectsPanelTabGroupsListTitleElementId), ClickMouse(),
      // Verify Projects Panel is still shown.
      Do([this]() { RunScheduledLayouts(); }),
      CheckResult(
          [this]() {
            return projects_panel_state_controller()->IsProjectsPanelVisible();
          },
          true));
}

// This test checks that the projects panel closes when pressing Esc.
IN_PROC_BROWSER_TEST_F(ProjectsPanelInteractiveUiTest, CloseOnEsc) {
  RunTestSequence(
      // Verify Vertical Tabs is showing.
      WaitForShow(kVerticalTabStripTopContainerElementId),
      // Verify Initial State for Projects Panel.
      CheckResult(
          [this]() {
            return projects_panel_state_controller()->IsProjectsPanelVisible();
          },
          false),
      // Click Projects Panel Button and Verify Visibilities.
      EnsurePresent(kVerticalTabStripProjectsButtonElementId),
      MoveMouseTo(kVerticalTabStripProjectsButtonElementId), ClickMouse(),
      CheckResult(
          [this]() {
            return projects_panel_state_controller()->IsProjectsPanelVisible();
          },
          true),
      Do([this]() { RunScheduledLayouts(); }),
      WaitForShow(kProjectsPanelViewElementId),
      // Press Esc.
      Do([this]() { RunScheduledLayouts(); }),
      SendKeyPress(kBrowserViewElementId, ui::VKEY_ESCAPE),
      // Verify Projects Panel is hidden.
      Do([this]() { RunScheduledLayouts(); }),
      WaitForHide(kProjectsPanelViewElementId),
      CheckResult(
          [this]() {
            return projects_panel_state_controller()->IsProjectsPanelVisible();
          },
          false));
}

// This test checks that the projects panel matches the width of the uncollapsed
// vertical tab strip region when larger than its minimum width.
IN_PROC_BROWSER_TEST_F(ProjectsPanelInteractiveUiTest,
                       MatchesVerticalTabsWidthWhenLargerThanMinWidth) {
  constexpr int kVerticalTabsRegionWidth =
      projects_panel::kProjectsPanelMinWidth + 100;
  RunTestSequence(
      // Resize the Vertical Tabs region to a large width.
      ResizeVerticalTabsRegionToWidth(kVerticalTabsRegionWidth),
      Do([this]() { RunScheduledLayouts(); }),
      // Open the Projects Panel.
      EnsurePresent(kVerticalTabStripProjectsButtonElementId),
      MoveMouseTo(kVerticalTabStripProjectsButtonElementId), ClickMouse(),
      Do([this]() { RunScheduledLayouts(); }),
      WaitForShow(kProjectsPanelViewElementId),
      // Verify that the panel matches the width of Vertical Tabs and does not
      // have rounded corners.
      CheckPanelHasExpectedWidthAndStyling(
          kVerticalTabsRegionWidth - views::Separator::kThickness,
          /*should_have_rounded_corners=*/false));
}

// This test checks that the projects panel has its minimum width when the
// vertical tab strip region is smaller than its minimum width.
IN_PROC_BROWSER_TEST_F(ProjectsPanelInteractiveUiTest,
                       MatchesVerticalTabsWidthWhenSmallerThanMinWidth) {
  constexpr int kVerticalTabsRegionWidth =
      projects_panel::kProjectsPanelMinWidth - 100;
  RunTestSequence(
      // Resize the Vertical Tabs region to a small width.
      ResizeVerticalTabsRegionToWidth(kVerticalTabsRegionWidth),
      Do([this]() { RunScheduledLayouts(); }),
      // Open the Projects Panel.
      EnsurePresent(kVerticalTabStripProjectsButtonElementId),
      MoveMouseTo(kVerticalTabStripProjectsButtonElementId), ClickMouse(),
      Do([this]() { RunScheduledLayouts(); }),
      WaitForShow(kProjectsPanelViewElementId),
      // Verify that the panel is at its minimum width and has rounded corners.
      CheckPanelHasExpectedWidthAndStyling(
          projects_panel::kProjectsPanelMinWidth,
          /*should_have_rounded_corners=*/true));
}

// This test checks that the projects panel matches the width of the uncollapsed
// vertical tab strip region when equal to its minimum width.
IN_PROC_BROWSER_TEST_F(ProjectsPanelInteractiveUiTest,
                       MatchesVerticalTabsWidthWhenEqualToMinWidth) {
  constexpr int kVerticalTabsRegionWidth =
      projects_panel::kProjectsPanelMinWidth;
  RunTestSequence(
      // Resize the Vertical Tabs region to a small width.
      ResizeVerticalTabsRegionToWidth(kVerticalTabsRegionWidth),
      Do([this]() { RunScheduledLayouts(); }),
      // Open the Projects Panel.
      EnsurePresent(kVerticalTabStripProjectsButtonElementId),
      MoveMouseTo(kVerticalTabStripProjectsButtonElementId), ClickMouse(),
      Do([this]() { RunScheduledLayouts(); }),
      WaitForShow(kProjectsPanelViewElementId),
      // Verify that the panel is at its minimum width and has rounded corners.
      CheckPanelHasExpectedWidthAndStyling(
          projects_panel::kProjectsPanelMinWidth - views::Separator::kThickness,
          /*should_have_rounded_corners=*/false));
}

IN_PROC_BROWSER_TEST_F(ProjectsPanelInteractiveUiTest,
                       ThreadsActivityMenu_GeminiActivityOpensURL) {
  RunTestSequence(
      OpenProjectsPanel(),
      // Click the threads activity menu button.
      MoveMouseTo(kProjectsPanelThreadsActivityButtonElementId), ClickMouse(),
      InstrumentNextTab(kNewTabElementId),
      // Select "Gemini app activity" from the menu.
      SelectMenuItem(kProjectsPanelThreadsActivityGeminiItemElementId),
      // Verify that a new tab was opened with the correct URL.
      WaitForWebContentsNavigation(kNewTabElementId,
                                   GURL(chrome::kMyActivityGeminiAppsUrl)),
      // Verify Projects Panel is hidden.
      WaitForHide(kProjectsPanelViewElementId),
      CheckResult(
          [this]() {
            return projects_panel_state_controller()->IsProjectsPanelVisible();
          },
          false));
}

IN_PROC_BROWSER_TEST_F(ProjectsPanelInteractiveUiTest,
                       ThreadsActivityMenu_AiModeActivityOpensURL) {
  RunTestSequence(
      OpenProjectsPanel(),
      // Click the threads activity menu button.
      MoveMouseTo(kProjectsPanelThreadsActivityButtonElementId), ClickMouse(),
      InstrumentNextTab(kNewTabElementId),
      // Select "AI Mode activity" from the menu.
      SelectMenuItem(kProjectsPanelThreadsActivityAiModeItemElementId),
      // Verify that a new tab was opened with the correct URL.
      WaitForWebContentsNavigation(kNewTabElementId,
                                   GURL(chrome::kMyActivityAiModeUrl)),
      // Verify Projects Panel is hidden.
      WaitForHide(kProjectsPanelViewElementId),
      CheckResult(
          [this]() {
            return projects_panel_state_controller()->IsProjectsPanelVisible();
          },
          false));
}

// This test checks that clicking the "Create new tab group" button creates a
// new tab group and closes the panel.
IN_PROC_BROWSER_TEST_F(ProjectsPanelInteractiveUiTest, CreateNewTabGroup) {
  RunTestSequence(
      // Verify Vertical Tabs is showing.
      WaitForShow(kVerticalTabStripTopContainerElementId),
      // Open the Projects Panel.
      EnsurePresent(kVerticalTabStripProjectsButtonElementId),
      MoveMouseTo(kVerticalTabStripProjectsButtonElementId), ClickMouse(),
      Do([this]() { RunScheduledLayouts(); }),
      WaitForShow(kProjectsPanelViewElementId),
      WaitForShow(kProjectsPanelNewTabGroupButtonElementId),
      // Click the Create New Tab Group button.
      MoveMouseTo(kProjectsPanelNewTabGroupButtonElementId),
      ClickMouse().SetMustRemainVisible(false),
      // Verify the panel closes.
      WaitForHide(kProjectsPanelViewElementId),
      // Verify a new tab group is created.
      CheckResult(
          [this]() {
            return browser()
                ->tab_strip_model()
                ->group_model()
                ->ListTabGroups()
                .size();
          },
          1u));
}

}  // namespace base::test
