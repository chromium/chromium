// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/test/tab_strip_interactive_test_mixin.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button_menu_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/test/views_test_utils.h"

namespace {

class TabSearchToolbarButtonInteractiveUiTest : public InteractiveBrowserTest {
 public:
  TabSearchToolbarButtonInteractiveUiTest() = default;
  ~TabSearchToolbarButtonInteractiveUiTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kTabstripComboButton);
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

// This test verifies the TabSearch functionality when pinned.
IN_PROC_BROWSER_TEST_F(TabSearchToolbarButtonInteractiveUiTest,
                       VerifyTabSearchWhenPinned) {
  RunTestSequence(
      // Clicking Tab Search Button.
      WaitForShow(kTabSearchButtonElementId),
      EnsurePresent(kTabSearchButtonElementId),
      MoveMouseTo(kTabSearchButtonElementId), ClickMouse(),
      WaitForShow(kTabSearchBubbleElementId),
      // Closing Tab Search Bubble.
      SendKeyPress(kTabSearchButtonElementId, ui::VKEY_ESCAPE),
      WaitForHide(kTabSearchBubbleElementId));
}
// This test verifies the TabSearch functionality after unpinning.
IN_PROC_BROWSER_TEST_F(TabSearchToolbarButtonInteractiveUiTest,
                       VerifyTabSearchWhenUnpinned) {
  RunTestSequence(
      // Unpinning Tab Search Button
      MoveMouseTo(kTabSearchButtonElementId),
      MayInvolveNativeContextMenu(
          ClickMouse(ui_controls::RIGHT),
          WaitForShow(kPinnedActionToolbarUnpinElementId),
          SelectMenuItem(kPinnedActionToolbarUnpinElementId)),
      // Verifying that it is no longer present.
      WaitForHide(kTabSearchButtonElementId),
      // Clicking the Tab Search Button.
      SendTabSearchKeyPress(kTabStripElementId),
      WaitForShow(kTabSearchBubbleElementId),
      EnsurePresent(kTabSearchBubbleElementId),
      // Closing Tab Search Bubble.
      SendKeyPress(kTabSearchBubbleElementId, ui::VKEY_ESCAPE),
      WaitForHide(kTabSearchBubbleElementId));
}

}  // namespace
