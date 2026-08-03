// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_view.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_state_controller.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/organizer/layout_constants.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/test/ui_controls.h"
#include "ui/compositor/layer.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/view_shadow.h"

namespace {
constexpr int kBrowserWindowWidth = 1400;
constexpr int kBrowserWindowHeight = 800;
}  // namespace

namespace base::test {

class OrganizerPanelInteractiveUiTest : public InteractiveBrowserTest {
 public:
  OrganizerPanelInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures(
        {tabs::kVerticalTabs, organizer_panel::kOrganizerPanel}, {});
    OrganizerPanelView::disable_animations_for_testing();
    animation_mode_reset_ = gfx::AnimationTestApi::SetRichAnimationRenderMode(
        gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    browser()->GetProfile()->GetPrefs()->SetBoolean(
        prefs::kTabSearchPinnedToTabstrip, true);

    // Resize the window to be wide enough to accommodate a wide vertical tab
    // strip and the toolbar.
    browser()->GetWindow()->SetBounds(
        gfx::Rect(0, 0, kBrowserWindowWidth, kBrowserWindowHeight));

    // Enter Vertical Tabs mode.
    tabs::VerticalTabStripStateController::From(browser())
        ->SetVerticalTabsEnabled(true);
    RunScheduledLayouts();
  }

  auto OpenOrganizerPanel() {
    return Steps(WaitForShow(kTabSearchButtonElementId),
                 PressButton(kTabSearchButtonElementId),
                 Do([this]() { RunScheduledLayouts(); }),
                 WaitForShow(kOrganizerPanelViewElementId),
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
                           ->organizer_panel_container_for_testing()
                           ->bounds()
                           .width();
                     },
                     expected_width),
                 CheckResult(
                     [this]() {
                       return browser_view()
                           ->organizer_panel_container_for_testing()
                           ->content_container_for_testing()
                           ->layer()
                           ->rounded_corner_radii()
                           .IsEmpty();
                     },
                     !should_have_rounded_corners));
  }

  OrganizerPanelStateController* organizer_panel_state_controller() {
    return OrganizerPanelStateController::From(browser());
  }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

 private:
  gfx::AnimationTestApi::RenderModeResetter animation_mode_reset_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test checks that the organizer panel closes when clicking outside.
IN_PROC_BROWSER_TEST_F(OrganizerPanelInteractiveUiTest, CloseOnClickOutside) {
  RunTestSequence(
      // Verify Vertical Tabs is showing.
      WaitForShow(kVerticalTabStripTopContainerElementId),
      // Verify Initial State for Organizer Panel.
      CheckResult(
          [this]() {
            return organizer_panel_state_controller()
                ->IsOrganizerPanelVisible();
          },
          false),
      // Open Organizer Panel and Verify Visibilities.
      OpenOrganizerPanel(),
      CheckResult(
          [this]() {
            return organizer_panel_state_controller()
                ->IsOrganizerPanelVisible();
          },
          true),
      // Click on the Omnibox (outside the panel).
      MoveMouseTo(kOmniboxElementId), ClickMouse(),
      // Verify Organizer Panel is hidden.
      Do([this]() { RunScheduledLayouts(); }),
      WaitForHide(kOrganizerPanelViewElementId),
      CheckResult(
          [this]() {
            return organizer_panel_state_controller()
                ->IsOrganizerPanelVisible();
          },
          false));
}

// This test checks that the organizer panel grabs focus when opened.
IN_PROC_BROWSER_TEST_F(OrganizerPanelInteractiveUiTest, GrabsFocusOnOpen) {
  RunTestSequence(OpenOrganizerPanel(),
                  CheckViewProperty(kOrganizerPanelViewElementId,
                                    &views::View::HasFocus, true));
}

// This test checks that the organizer panel closes when focus is switched to
// another UI element like the omnibox.
IN_PROC_BROWSER_TEST_F(OrganizerPanelInteractiveUiTest, ClosesOnFocusLost) {
  RunTestSequence(OpenOrganizerPanel(),
                  // Focus the omnibox.
                  FocusElement(kOmniboxElementId),
                  // Verify Organizer Panel is hidden.
                  WaitForHide(kOrganizerPanelViewElementId),
                  CheckResult(
                      [this]() {
                        return organizer_panel_state_controller()
                            ->IsOrganizerPanelVisible();
                      },
                      false));
}

// This test checks that focus is restored to the last focused element when the
// panel is closed.
IN_PROC_BROWSER_TEST_F(OrganizerPanelInteractiveUiTest, RestoresFocusOnClose) {
  RunTestSequence(
      // Focus the omnibox.
      FocusElement(kOmniboxElementId),
      CheckViewProperty(kOmniboxElementId, &views::View::HasFocus, true),
      // Open the organizer panel directly to avoid moving focus to the toggle
      // button.
      Do([this]() {
        actions::ActionManager::Get()
            .FindAction(kActionToggleOrganizerPanel,
                        BrowserActions::From(browser())->root_action_item())
            ->InvokeAction();
      }),
      WaitForShow(kOrganizerPanelViewElementId),
      CheckViewProperty(kOrganizerPanelViewElementId, &views::View::HasFocus,
                        true),
      // Close the organizer panel via the toggle action.
      Do([this]() {
        actions::ActionManager::Get()
            .FindAction(kActionToggleOrganizerPanel,
                        BrowserActions::From(browser())->root_action_item())
            ->InvokeAction();
      }),
      // Verify focus is restored to the omnibox.
      WaitForHide(kOrganizerPanelViewElementId),
      CheckViewProperty(kOmniboxElementId, &views::View::HasFocus, true));
}

// This is a regression test that checks that the panel stays open when clicking
// inside (but not on a button or other interactive element).
// TODO(crbug.com/540107609): Re-enable once panel is implemented.
IN_PROC_BROWSER_TEST_F(OrganizerPanelInteractiveUiTest,
                       DISABLED_StaysOpenOnClickInside) {
  RunTestSequence(
      // Verify Vertical Tabs is showing.
      WaitForShow(kVerticalTabStripTopContainerElementId),
      // Verify Initial State for Organizer Panel.
      CheckResult(
          [this]() {
            return organizer_panel_state_controller()
                ->IsOrganizerPanelVisible();
          },
          false),
      // Open Organizer Panel and Verify Visibilities.
      OpenOrganizerPanel(),
      CheckResult(
          [this]() {
            return organizer_panel_state_controller()
                ->IsOrganizerPanelVisible();
          },
          true),
      // Click inside the panel view (background).
      MoveMouseTo(kOrganizerPanelViewElementId), ClickMouse(),
      // Verify Organizer Panel is still shown.
      Do([this]() { RunScheduledLayouts(); }),
      CheckResult(
          [this]() {
            return organizer_panel_state_controller()
                ->IsOrganizerPanelVisible();
          },
          true));
}

// This test checks that the organizer panel closes when pressing Esc.
IN_PROC_BROWSER_TEST_F(OrganizerPanelInteractiveUiTest, CloseOnEsc) {
  RunTestSequence(
      // Verify Vertical Tabs is showing.
      WaitForShow(kVerticalTabStripTopContainerElementId),
      // Verify Initial State for Organizer Panel.
      CheckResult(
          [this]() {
            return organizer_panel_state_controller()
                ->IsOrganizerPanelVisible();
          },
          false),
      // Open Organizer Panel and Verify Visibilities.
      OpenOrganizerPanel(),
      CheckResult(
          [this]() {
            return organizer_panel_state_controller()
                ->IsOrganizerPanelVisible();
          },
          true),
      // Press Esc.
      Do([this]() { RunScheduledLayouts(); }),
      SendKeyPress(kBrowserViewElementId, ui::VKEY_ESCAPE),
      // Verify Organizer Panel is hidden.
      Do([this]() { RunScheduledLayouts(); }),
      WaitForHide(kOrganizerPanelViewElementId),
      CheckResult(
          [this]() {
            return organizer_panel_state_controller()
                ->IsOrganizerPanelVisible();
          },
          false));
}

// This test checks that the organizer panel matches the width of the
// uncollapsed vertical tab strip region when larger than its minimum width.
IN_PROC_BROWSER_TEST_F(OrganizerPanelInteractiveUiTest,
                       MatchesVerticalTabsWidthWhenLargerThanMinWidth) {
  constexpr int kVerticalTabsRegionWidth =
      organizer_panel::kOrganizerPanelMinWidth + 100;
  RunTestSequence(
      // Resize the Vertical Tabs region to a large width.
      ResizeVerticalTabsRegionToWidth(kVerticalTabsRegionWidth),
      Do([this]() { RunScheduledLayouts(); }), OpenOrganizerPanel(),
      // Verify that the panel matches the width of Vertical Tabs and does not
      // have rounded corners.
      CheckPanelHasExpectedWidthAndStyling(
          kVerticalTabsRegionWidth - views::Separator::kThickness,
          /*should_have_rounded_corners=*/false));
}

// This test checks that the organizer panel has its minimum width when the
// vertical tab strip region is smaller than its minimum width.
IN_PROC_BROWSER_TEST_F(OrganizerPanelInteractiveUiTest,
                       MatchesVerticalTabsWidthWhenSmallerThanMinWidth) {
  constexpr int kVerticalTabsRegionWidth =
      organizer_panel::kOrganizerPanelMinWidth - 100;
  RunTestSequence(
      // Resize the Vertical Tabs region to a small width.
      ResizeVerticalTabsRegionToWidth(kVerticalTabsRegionWidth),
      Do([this]() { RunScheduledLayouts(); }), OpenOrganizerPanel(),
      // Verify that the panel is at its minimum width and has rounded corners.
      CheckPanelHasExpectedWidthAndStyling(
          organizer_panel::kOrganizerPanelMinWidth,
          /*should_have_rounded_corners=*/true));
}

// This test checks that the organizer panel matches the width of the
// uncollapsed vertical tab strip region when equal to its minimum width.
IN_PROC_BROWSER_TEST_F(OrganizerPanelInteractiveUiTest,
                       MatchesVerticalTabsWidthWhenEqualToMinWidth) {
  constexpr int kVerticalTabsRegionWidth =
      organizer_panel::kOrganizerPanelMinWidth;
  RunTestSequence(
      // Resize the Vertical Tabs region to a small width.
      ResizeVerticalTabsRegionToWidth(kVerticalTabsRegionWidth),
      Do([this]() { RunScheduledLayouts(); }), OpenOrganizerPanel(),
      // Verify that the panel is at its minimum width and has rounded corners.
      CheckPanelHasExpectedWidthAndStyling(
          organizer_panel::kOrganizerPanelMinWidth -
              views::Separator::kThickness,
          /*should_have_rounded_corners=*/false));
}

}  // namespace base::test
