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
#include "ui/events/event_constants.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/interaction/interactive_views_test.h"

namespace {

class VerticalTabStripTopContainerInteractiveUiTest
    : public InteractiveBrowserTest {
 public:
  VerticalTabStripTopContainerInteractiveUiTest() = default;
  ~VerticalTabStripTopContainerInteractiveUiTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(tabs::kVerticalTabs);
    InteractiveBrowserTest::SetUp();
  }

  auto SendTabSearchKeyPress(ui::ElementIdentifier target) {
#if BUILDFLAG(IS_MAC)
    return SendKeyPress(target, ui::VKEY_A,
                        ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
#else
    return SendKeyPress(target, ui::VKEY_A,
                        ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
#endif
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test checks that we can click the tab search button starting from the
// vertical tab strip and then switching to the horizontal layout.
IN_PROC_BROWSER_TEST_F(VerticalTabStripTopContainerInteractiveUiTest,
                       VerifyTabSearchVerticalToHorizontal) {
  browser()
      ->browser_window_features()
      ->vertical_tab_strip_state_controller()
      ->SetVerticalTabsEnabled(true);
  RunScheduledLayouts();

  RunTestSequence(WaitForShow(kVerticalTabStripTopContainerElementId),
                  EnsurePresent(kTabSearchButtonElementId),
                  SendTabSearchKeyPress(kTabSearchButtonElementId),
                  WaitForShow(kTabSearchBubbleElementId),
                  SendKeyPress(kTabSearchBubbleElementId, ui::VKEY_ESCAPE),
                  WaitForHide(kTabSearchBubbleElementId));

  browser()
      ->browser_window_features()
      ->vertical_tab_strip_state_controller()
      ->SetVerticalTabsEnabled(false);
  RunScheduledLayouts();

  RunTestSequence(WaitForShow(kTabStripFrameGrabHandleElementId),
                  EnsurePresent(kTabStripFrameGrabHandleElementId),
                  SendTabSearchKeyPress(kTabStripFrameGrabHandleElementId),
                  WaitForShow(kTabSearchBubbleElementId));
}

// This test checks that we can click the tab search button starting from the
// horizontal tab strip and then switching to the vertical layout.
IN_PROC_BROWSER_TEST_F(VerticalTabStripTopContainerInteractiveUiTest,
                       VerifyTabSearchHorizontalToVertical) {
  RunScheduledLayouts();

  RunTestSequence(WaitForShow(kTabStripFrameGrabHandleElementId),
                  EnsurePresent(kTabStripFrameGrabHandleElementId),
                  SendTabSearchKeyPress(kTabStripFrameGrabHandleElementId),
                  WaitForShow(kTabSearchBubbleElementId),
                  SendKeyPress(kTabSearchBubbleElementId, ui::VKEY_ESCAPE),
                  WaitForHide(kTabSearchBubbleElementId));

  browser()
      ->browser_window_features()
      ->vertical_tab_strip_state_controller()
      ->SetVerticalTabsEnabled(true);
  RunScheduledLayouts();

  RunTestSequence(WaitForShow(kVerticalTabStripTopContainerElementId),
                  EnsurePresent(kTabSearchButtonElementId),
                  SendTabSearchKeyPress(kTabSearchButtonElementId),
                  WaitForShow(kTabSearchBubbleElementId));
}

}  // namespace
