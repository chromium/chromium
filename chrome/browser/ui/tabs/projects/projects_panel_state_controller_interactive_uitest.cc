// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/projects/projects_panel_state_controller.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/saved_tab_groups/public/features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/interaction/interactive_views_test.h"

namespace base::test {

class ProjectsPanelStateControllerInteractiveUiTest
    : public InteractiveBrowserTest {
 public:
  ProjectsPanelStateControllerInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures(/* enabled_features */
                                          {tabs::kVerticalTabs,
                                           tab_groups::kProjectsPanel},
                                          /* disabled_features */ {});
    ProjectsPanelView::disable_animations_for_testing();
  }
  ~ProjectsPanelStateControllerInteractiveUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    // Enter Vertical Tabs mode.
    tabs::VerticalTabStripStateController::From(browser())
        ->SetVerticalTabsEnabled(true);
    RunScheduledLayouts();
  }

  ProjectsPanelStateController* projects_panel_state_controller() {
    return ProjectsPanelStateController::From(browser());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test checks that we can click the projects panel button.
IN_PROC_BROWSER_TEST_F(ProjectsPanelStateControllerInteractiveUiTest,
                       VerifyProjectsPanelButton) {
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
      // Click Projects Panel Button and Verify Visibilities.
      MoveMouseTo(kProjectsPanelButtonElementId), ClickMouse(),
      CheckResult(
          [this]() {
            return projects_panel_state_controller()->IsProjectsPanelVisible();
          },
          false),
      Do([this]() { RunScheduledLayouts(); }),
      WaitForHide(kProjectsPanelViewElementId),
      WaitForHide(kProjectsPanelButtonElementId));
}

}  // namespace base::test
