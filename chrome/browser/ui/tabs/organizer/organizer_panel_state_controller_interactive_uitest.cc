// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organizer/organizer_panel_state_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_state_controller.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/tabs/organizer/layout_constants.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_view.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_tray_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/interaction/interactive_views_test.h"

namespace base::test {

class OrganizerPanelStateControllerInteractiveUiTest
    : public InteractiveBrowserTest {
 public:
  OrganizerPanelStateControllerInteractiveUiTest() {
    scoped_feature_list_.InitAndEnableFeature(organizer_panel::kOrganizerPanel);
  }
  ~OrganizerPanelStateControllerInteractiveUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    // Enter Vertical Tabs mode.
    tabs::VerticalTabStripStateController::From(browser())
        ->SetVerticalTabsEnabled(true);
    RunScheduledLayouts();
  }

  auto ExpectControllerState(bool open) {
    return CheckResult(
               [this]() {
                 return organizer_panel_state_controller()
                     ->IsOrganizerPanelVisible();
               },
               open)
        .AddDescriptionPrefix("ExpectControllerState()");
  }

  auto WaitForPanelShow() {
    auto steps = Steps(InParallel(
        RunSubsequence(ExpectControllerState(true)),
        RunSubsequence(WaitForEvent(OrganizerTrayView::kTrayElementId,
                                    OrganizerTrayView::kOpenAnimationComplete)),
        RunSubsequence(WaitForShow(kOrganizerPanelViewElementId))));
    AddDescriptionPrefix(steps, "WaitForPanelShow()");
    return steps;
  }

  auto WaitForPanelHide() {
    auto steps = Steps(
        InParallel(RunSubsequence(ExpectControllerState(false)),
                   RunSubsequence(WaitForEvent(
                       OrganizerTrayView::kTrayElementId,
                       OrganizerTrayView::kCloseAnimationComplete)),
                   RunSubsequence(WaitForHide(kOrganizerPanelViewElementId))));
    AddDescriptionPrefix(steps, "WaitForPanelHide()");
    return steps;
  }

  OrganizerPanelStateController* organizer_panel_state_controller() {
    return OrganizerPanelStateController::From(browser());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test checks that we can click the tab search button to toggle the
// organizer panel.
IN_PROC_BROWSER_TEST_F(OrganizerPanelStateControllerInteractiveUiTest,
                       VerifyOrganizerPanelButton) {
  RunTestSequence(
      // Verify Vertical Tabs is showing.
      WaitForShow(kVerticalTabStripTopContainerElementId),
      // Verify Initial State for Organizer Panel.
      ExpectControllerState(false),
      // Click Tab Search Button and Verify Visibilities.
      EnsurePresent(kTabSearchButtonElementId),
      MoveMouseTo(kTabSearchButtonElementId), ClickMouse(), WaitForPanelShow(),
      // Click Organizer Panel Button and Verify Visibilities.
      MoveMouseTo(kOrganizerPanelButtonElementId), ClickMouse(),
      WaitForPanelHide());
}

// This test checks that clicking the tab search button opens the organizer
// panel in vertical tabs mode.
IN_PROC_BROWSER_TEST_F(OrganizerPanelStateControllerInteractiveUiTest,
                       VerifyTabSearchButtonInVerticalTabs) {
  RunTestSequence(
      WaitForShow(kVerticalTabStripTopContainerElementId),
      ExpectControllerState(false), EnsurePresent(kTabSearchButtonElementId),
      MoveMouseTo(kTabSearchButtonElementId), ClickMouse(), WaitForPanelShow());
}

}  // namespace base::test
