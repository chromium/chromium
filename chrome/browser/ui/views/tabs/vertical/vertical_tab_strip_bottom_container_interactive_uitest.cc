// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_op.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/interaction/polling_state_observer.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/view_utils.h"

namespace base::test {

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<int>,
                                    kTabCountState);

class VerticalTabStripBottomContainerInteractiveUiTest
    : public VerticalTabsInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  using VerticalTabsInteractiveTestMixin::VerticalTabsInteractiveTestMixin;

  const std::vector<base::test::FeatureRef> GetDisabledFeatures() override {
    return {tab_groups::kProjectsPanel};
  }

 private:
  const gfx::AnimationTestApi::RenderModeResetter disable_rich_animations_ =
      gfx::AnimationTestApi::SetRichAnimationRenderMode(
          gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
};

// This test checks that we can click the new tab button in the bottom container
// of the vertical tab strip
IN_PROC_BROWSER_TEST_F(VerticalTabStripBottomContainerInteractiveUiTest,
                       VerifyNewTabButton) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  RunTestSequence(
      CheckResult([this]() { return browser()->tab_strip_model()->count(); },
                  1),
      Do([&]() {
        histogram_tester.ExpectTotalCount(
            "TabStrip.TimeToCreateNewTabFromPress", 0);
        EXPECT_EQ(0, user_action_tester.GetActionCount("NewTab_Button"));
      }),
      WaitForShow(kVerticalTabStripBottomContainerElementId),
      EnsurePresent(kNewTabButtonElementId),
      PressButton(kNewTabButtonElementId,
                  ui::test::InteractionTestUtil::InputType::kDontCare),
      CheckResult([this]() { return browser()->tab_strip_model()->count(); },
                  2),
      Do([&]() {
        histogram_tester.ExpectTotalCount(
            "TabStrip.TimeToCreateNewTabFromPress", 1);
        EXPECT_EQ(1, user_action_tester.GetActionCount("NewTab_Button"));
      }));
}

// This functionality is only defined on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_MiddleClickPasteAndNavigate MiddleClickPasteAndNavigate
#else
#define MAYBE_MiddleClickPasteAndNavigate DISABLED_MiddleClickPasteAndNavigate
#endif
// This test checks that middle-clicking the new tab button in the bottom
// container of the vertical tab strip pastes text from the selection clipboard
// and navigates to it.
IN_PROC_BROWSER_TEST_F(VerticalTabStripBottomContainerInteractiveUiTest,
                       MAYBE_MiddleClickPasteAndNavigate) {
  if (!ui::Clipboard::IsSupportedClipboardBuffer(
          ui::ClipboardBuffer::kSelection) ||
      !ui::Clipboard::IsMiddleClickPasteEnabled()) {
    GTEST_SKIP() << "Middle click paste or kSelection not supported";
  }

  const std::u16string kPasteText = u"https://www.google.com/";
  base::UserActionTester user_action_tester;

  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kSelection);
    writer.WriteText(kPasteText);
  }

  RunTestSequence(
      CheckResult([this]() { return browser()->tab_strip_model()->count(); },
                  1),
      WaitForShow(kNewTabButtonElementId), MoveMouseTo(kNewTabButtonElementId),
      ClickMouse(ui_controls::MIDDLE),
      PollState(kTabCountState,
                [this]() { return browser()->tab_strip_model()->count(); }),
      WaitForState(kTabCountState, 2), StopObservingState(kTabCountState),
      Do([&]() {
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "NewTabButton_PasteAndNavigate"));
      }));
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
      WaitForShow(kTabGroupHeaderElementId),
      WaitForShow(kTabGroupEditorBubbleId),
      WaitForShow(kTabGroupEditorBubbleButtonElementId),
      CheckResult([this]() { return browser()->tab_strip_model()->count(); },
                  2));
}

// This test checks that clicking the new tab button doesn't expose window
// caption on the vertical tab strip as it is moving down.
IN_PROC_BROWSER_TEST_F(VerticalTabStripBottomContainerInteractiveUiTest,
                       DoubleClickOnNewTabButtonDoesNotMaximizeWindow) {
  gfx::Point new_tab_button_center;
  gfx::Point point_above_new_tab_button;
  RunTestSequence(
      WaitForShow(kVerticalTabStripBottomContainerElementId),
      EnsurePresent(kNewTabButtonElementId),
      WithView(kNewTabButtonElementId,
               [&new_tab_button_center,
                &point_above_new_tab_button](views::View* button) {
                 new_tab_button_center =
                     button->GetBoundsInScreen().CenterPoint();
                 gfx::Rect bounds = button->GetBoundsInScreen();
                 gfx::Insets insets = button->GetInsets();
                 // A point just above the contents of the button, but within
                 // the extended hit test area (which includes the top inset).
                 point_above_new_tab_button = gfx::Point(
                     bounds.CenterPoint().x(), bounds.y() + insets.top() / 2);
               }),
      // Move mouse to the new tab button
      MoveMouseTo(kNewTabButtonElementId),
      // Click the new tab button
      ClickMouse(),
      // Check that the points are NO LONGER considered hit test caption
      CheckView(
          kTabStripRegionElementId,
          [&new_tab_button_center,
           &point_above_new_tab_button](views::View* region_view) {
            auto* vt_region_view =
                views::AsViewClass<VerticalTabStripRegionView>(region_view);

            gfx::Point pt_center = new_tab_button_center;
            views::View::ConvertPointFromScreen(vt_region_view, &pt_center);

            gfx::Point pt_above = point_above_new_tab_button;
            views::View::ConvertPointFromScreen(vt_region_view, &pt_above);
            return !vt_region_view->IsPositionInWindowCaption(pt_center) &&
                   !vt_region_view->IsPositionInWindowCaption(pt_above);
          },
          "Check that clicking new tab does not expose caption space"));
}

class NewTabButtonContextMenuInteractiveUITest
    : public VerticalTabStripBottomContainerInteractiveUiTest {
 public:
  NewTabButtonContextMenuInteractiveUITest() = default;
  ~NewTabButtonContextMenuInteractiveUITest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    VerticalTabStripBottomContainerInteractiveUiTest::
        SetUpInProcessBrowserTestFixture();
    feature_list_.InitAndEnableFeature(features::kTabGroupMenuMoreEntryPoints);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(NewTabButtonContextMenuInteractiveUITest,
                       VerifyNewTabButtonContextMenu) {
  RunTestSequence(
      WaitForShow(kNewTabButtonElementId), MoveMouseTo(kNewTabButtonElementId),
      MayInvolveNativeContextMenu(
          ClickMouse(ui_controls::RIGHT),
          WaitForShow(NewTabButtonMenuModel::kNewTab),
          SelectMenuItem(NewTabButtonMenuModel::kNewTab),
          CheckResult(
              [this]() { return browser()->tab_strip_model()->count(); }, 2)));
}

}  // namespace base::test
