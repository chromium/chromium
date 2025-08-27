// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/system_menu_model_builder.h"
#include "chrome/browser/ui/views/test/tab_strip_interactive_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/interaction/interactive_views_test.h"

namespace base::test {

class VerticalTabStripInteractiveUiTest : public InteractiveBrowserTest {
 public:
  VerticalTabStripInteractiveUiTest() = default;
  ~VerticalTabStripInteractiveUiTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(tabs::kVerticalTabs);
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    InteractiveBrowserTest::TearDownOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/441382208): Unable to programmatically click "show tabs on
// side" in Windows
#if BUILDFLAG(IS_WIN)
#define MAYBE_VerifyTabsToTheSideButton DISABLED_VerifyTabsToTheSideButton
#else
#define MAYBE_VerifyTabsToTheSideButton VerifyTabsToTheSideButton
#endif
// This test checks that we can click the switch to vertical tab view button
IN_PROC_BROWSER_TEST_F(VerticalTabStripInteractiveUiTest,
                       MAYBE_VerifyTabsToTheSideButton) {
  browser()
      ->browser_window_features()
      ->vertical_tab_strip_state_controller()
      ->SetVerticalTabsEnabled(false);

  RunTestSequence(
      WaitForShow(kTabStripFrameGrabHandleElementId),
      EnsurePresent(kTabStripFrameGrabHandleElementId),
      MoveMouseTo(kTabStripFrameGrabHandleElementId),
      MayInvolveNativeContextMenu(ClickMouse(ui_controls::RIGHT)),
      SelectMenuItem(SystemMenuModelBuilder::kSwitchTabToSideElementId),
      WaitForShow(kVerticalTabStripRegionElementId));
}

}  // namespace base::test
