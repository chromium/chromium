// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organizer/organizer_panel_state_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_state_controller.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/tabs/organizer/layout_constants.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/saved_tab_groups/public/features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/interaction/interactive_views_test.h"

namespace base::test {

class OrganizerPanelStateControllerInteractiveUiTest
    : public InteractiveBrowserTest {
 public:
  OrganizerPanelStateControllerInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures(/* enabled_features */
                                          {tabs::kVerticalTabs,
                                           tab_groups::kOrganizerPanel},
                                          /* disabled_features */ {});
    OrganizerPanelView::disable_animations_for_testing();
  }
  ~OrganizerPanelStateControllerInteractiveUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    // Enter Vertical Tabs mode.
    tabs::VerticalTabStripStateController::From(browser())
        ->SetVerticalTabsEnabled(true);
    RunScheduledLayouts();
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
      CheckResult(
          [this]() {
            return organizer_panel_state_controller()
                ->IsOrganizerPanelVisible();
          },
          false),
      // Click Tab Search Button and Verify Visibilities.
      EnsurePresent(kTabSearchButtonElementId),
      MoveMouseTo(kTabSearchButtonElementId), ClickMouse(),
      CheckResult(
          [this]() {
            return organizer_panel_state_controller()
                ->IsOrganizerPanelVisible();
          },
          true),
      Do([this]() { RunScheduledLayouts(); }),
      WaitForShow(kOrganizerPanelViewElementId),
      // Click Organizer Panel Button and Verify Visibilities.
      MoveMouseTo(kOrganizerPanelButtonElementId), ClickMouse(),
      CheckResult(
          [this]() {
            return organizer_panel_state_controller()
                ->IsOrganizerPanelVisible();
          },
          false),
      Do([this]() { RunScheduledLayouts(); }),
      WaitForHide(kOrganizerPanelViewElementId),
      WaitForHide(kOrganizerPanelButtonElementId));
}

// This test checks that clicking the tab search button opens the organizer
// panel in vertical tabs mode.
IN_PROC_BROWSER_TEST_F(OrganizerPanelStateControllerInteractiveUiTest,
                       VerifyTabSearchButtonInVerticalTabs) {
  RunTestSequence(WaitForShow(kVerticalTabStripTopContainerElementId),
                  CheckResult(
                      [this]() {
                        return organizer_panel_state_controller()
                            ->IsOrganizerPanelVisible();
                      },
                      false),
                  EnsurePresent(kTabSearchButtonElementId),
                  MoveMouseTo(kTabSearchButtonElementId), ClickMouse(),
                  CheckResult(
                      [this]() {
                        return organizer_panel_state_controller()
                            ->IsOrganizerPanelVisible();
                      },
                      true),
                  Do([this]() { RunScheduledLayouts(); }),
                  WaitForShow(kOrganizerPanelViewElementId));
}

}  // namespace base::test
