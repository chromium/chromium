// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "chrome/grit/generated_resources.h"
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

  bool SystemMenuContainsStringId(int message_id) {
    ui::MenuModel* menu_model =
        browser()->GetBrowserView().browser_widget()->GetSystemMenuModel();
    for (size_t i = 0; i < menu_model->GetItemCount(); i++) {
      if (l10n_util::GetStringUTF16(message_id) == menu_model->GetLabelAt(i)) {
        return true;
      }
    }
    return false;
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
// This test checks that we can click the show tabs to the side button
IN_PROC_BROWSER_TEST_F(VerticalTabStripInteractiveUiTest,
                       MAYBE_VerifyTabsToTheSideButton) {
  EXPECT_TRUE(SystemMenuContainsStringId(IDS_SWITCH_TO_VERTICAL_TAB));

  RunTestSequence(
      WaitForShow(kTabStripFrameGrabHandleElementId),
      EnsurePresent(kTabStripFrameGrabHandleElementId),
      MoveMouseTo(kTabStripFrameGrabHandleElementId),
      MayInvolveNativeContextMenu(
          ClickMouse(ui_controls::RIGHT),
          WaitForShow(SystemMenuModelBuilder::kToggleVerticalTabsElementId),
          SelectMenuItem(SystemMenuModelBuilder::kToggleVerticalTabsElementId)),
      WaitForShow(kVerticalTabStripRegionElementId));

  EXPECT_TRUE(SystemMenuContainsStringId(IDS_SWITCH_TO_HORIZONTAL_TAB));
}

// TODO(crbug.com/441382208): Unable to programmatically click "show tabs on
// top" in Windows
#if BUILDFLAG(IS_WIN)
#define MAYBE_VerifyTabsToTheTopButton DISABLED_VerifyTabsToTheTopButton
#else
#define MAYBE_VerifyTabsToTheTopButton VerifyTabsToTheTopButton
#endif
// This test checks that we can click the show tabs at the top button
IN_PROC_BROWSER_TEST_F(VerticalTabStripInteractiveUiTest,
                       MAYBE_VerifyTabsToTheTopButton) {
  browser()
      ->browser_window_features()
      ->vertical_tab_strip_state_controller()
      ->SetVerticalTabsEnabled(true);

  EXPECT_TRUE(SystemMenuContainsStringId(IDS_SWITCH_TO_HORIZONTAL_TAB));

  RunScheduledLayouts();

  RunTestSequence(
      WaitForShow(kVerticalTabStripTopContainerElementId),
      EnsurePresent(kVerticalTabStripTopContainerElementId),
      MoveMouseTo(kVerticalTabStripTopContainerElementId),
      MayInvolveNativeContextMenu(
          ClickMouse(ui_controls::RIGHT),
          WaitForShow(SystemMenuModelBuilder::kToggleVerticalTabsElementId),
          SelectMenuItem(SystemMenuModelBuilder::kToggleVerticalTabsElementId)),
      WaitForShow(kTabStripFrameGrabHandleElementId));

  EXPECT_TRUE(SystemMenuContainsStringId(IDS_SWITCH_TO_VERTICAL_TAB));
}

}  // namespace base::test
