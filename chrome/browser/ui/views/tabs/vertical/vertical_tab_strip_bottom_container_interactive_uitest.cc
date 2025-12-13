// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_op.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/views/controls/menu/menu_item_view.h"

namespace base::test {

class VerticalTabStripBottomContainerInteractiveUiTest
    : public VerticalTabsInteractiveTestMixin<InteractiveBrowserTest> {};

// This test checks that we can click the new tab button in the bottom container
// of the vertical tab strip
IN_PROC_BROWSER_TEST_F(VerticalTabStripBottomContainerInteractiveUiTest,
                       VerifyNewTabButton) {
  RunTestSequence(
      CheckResult([this]() { return browser()->tab_strip_model()->count(); },
                  1),
      WaitForShow(kVerticalTabStripBottomContainerElementId),
      EnsurePresent(kNewTabButtonElementId),
      PressButton(kNewTabButtonElementId,
                  ui::test::InteractionTestUtil::InputType::kDontCare),
      CheckResult([this]() { return browser()->tab_strip_model()->count(); },
                  2));
}

// This test checks that we can click the tab group button in the bottom
// container of the vertical tab strip
IN_PROC_BROWSER_TEST_F(VerticalTabStripBottomContainerInteractiveUiTest,
                       VerifyTabGroupButton) {
  RunTestSequence(
      CheckResult([this]() { return browser()->tab_strip_model()->count(); },
                  1),
      WaitForShow(kVerticalTabStripBottomContainerElementId),
      EnsurePresent(kSavedTabGroupButtonElementId),
      PressButton(kSavedTabGroupButtonElementId,
                  ui::test::InteractionTestUtil::InputType::kDontCare),
      EnsurePresent(tab_groups::STGEverythingMenu::kCreateNewTabGroup),
      SelectMenuItem(tab_groups::STGEverythingMenu::kCreateNewTabGroup),
      WaitForShow(kTabGroupEditorBubbleId),
      CheckResult([this]() { return browser()->tab_strip_model()->count(); },
                  2));
}

}  // namespace base::test
