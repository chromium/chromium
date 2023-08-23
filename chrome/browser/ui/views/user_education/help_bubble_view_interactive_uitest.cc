// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"

using user_education::HelpBubbleArrow;
using user_education::HelpBubbleParams;
using user_education::HelpBubbleView;

class HelpBubbleViewInteractiveUiTest : public InteractiveBrowserTest {
 public:
  HelpBubbleViewInteractiveUiTest() = default;
  ~HelpBubbleViewInteractiveUiTest() override = default;

 protected:
  static HelpBubbleParams GetBubbleParams() {
    HelpBubbleParams params;
    params.body_text = u"To X, do Y";
    params.arrow = HelpBubbleArrow::kTopRight;
    return params;
  }

  // Shows the help anchored to the view `anchor`, and waits for it to appear.
  auto ShowHelpBubble(ElementSpecifier anchor,
                      HelpBubbleParams params = GetBubbleParams()) {
    return Steps(
        WithView(anchor,
                 [params = std::move(params)](views::View* anchor) mutable {
                   new HelpBubbleView(GetHelpBubbleDelegate(), {anchor},
                                      std::move(params));
                 }),
        std::move(WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting)
                      .SetTransitionOnlyOnEvent(true)),
        // Prevent direct chaining off the show event.
        FlushEvents());
  }

  // Closes the current help bubble and waits for it to hide.
  auto CloseHelpBubble() {
    return Steps(
        WithView(HelpBubbleView::kHelpBubbleElementIdForTesting,
                 [](HelpBubbleView* bubble) { bubble->GetWidget()->Close(); }),
        std::move(WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting)
                      .SetTransitionOnlyOnEvent(true)),
        // Prevent direct chaining off the hide event.
        FlushEvents());
  }
};

IN_PROC_BROWSER_TEST_F(HelpBubbleViewInteractiveUiTest,
                       WidgetNotActivatedByDefault) {
  RunTestSequence(
      // The browser should be the active window.
      CheckViewProperty(kBrowserViewElementId, &BrowserView::IsActive, true),
      // Focus the toolbar and verify that it is focused.
      WithView(kBrowserViewElementId,
               [](BrowserView* view) { view->FocusToolbar(); }),
      CheckView(
          kBrowserViewElementId,
          [](BrowserView* view) {
            return view->GetFocusManager()->GetFocusedView();
          },
          testing::Ne(nullptr)),
      // Show the help bubble on the app menu and verify that it appears as
      // expected.
      ShowHelpBubble(kToolbarAppMenuButtonElementId),
      CheckView(HelpBubbleView::kHelpBubbleElementIdForTesting,
                [](HelpBubbleView* bubble) {
                  return bubble->GetWidget()->IsVisible();
                }),
      // The browser should still be the active window.
      CheckViewProperty(kBrowserViewElementId, &BrowserView::IsActive, true),
      // The help bubble widget should not steal focus.
      CheckView(
          HelpBubbleView::kHelpBubbleElementIdForTesting,
          [](HelpBubbleView* view) { return view->GetWidget()->IsActive(); },
          false),
      // Close the bubble and clean up.
      CloseHelpBubble());
}

// This is a regression test to ensure that help bubbles prevent other bubbles
// they are anchored to from closing on loss of focus. Failing to do this
// results in situations where a user can abort a user education journey by
// entering accessible keyboard navigation commands to try to read the help
// bubble, or by trying to interact with the help bubble with the mouse to e.g.
// close it.
IN_PROC_BROWSER_TEST_F(HelpBubbleViewInteractiveUiTest,
                       BubblePreventsCloseOnLossOfFocus) {
  browser()->tab_strip_model()->AddToNewGroup({0});

  HelpBubbleParams params;
  params.body_text = u"foo";

  RunTestSequence(
      SetOnIncompatibleAction(
          OnIncompatibleAction::kSkipTest,
          "Programmatic window activation doesn't work on all platforms."),
      // Trigger the tab group editor.
      AfterShow(kTabGroupHeaderElementId,
                [](ui::TrackedElement* element) {
                  // Show the tab group editor bubble.
                  auto* const view = AsView(element);
                  view->ShowContextMenu(
                      view->GetLocalBounds().CenterPoint(),
                      ui::MenuSourceType::MENU_SOURCE_KEYBOARD);
                }),
      WaitForShow(kTabGroupEditorBubbleId),
      // Display a help bubble attached to the tab group editor.
      ShowHelpBubble(kTabGroupEditorBubbleId, std::move(params)),
      // Activate the help bubble. This should not cause the editor to close.
      ActivateSurface(HelpBubbleView::kHelpBubbleElementIdForTesting),
      // Re-Activate the dialog.
      ActivateSurface(kTabGroupEditorBubbleId),
      // Close the help bubble.
      CloseHelpBubble(),
      // Now that the help bubble is gone, locate the editor again and transfer
      // activation to its primary window widget (the browser window) - this
      // should close the editor as it is no longer pinned by the help bubble.
      ActivateSurface(kToolbarAppMenuButtonElementId),
      // Verify that the editor bubble closes now that it has lost focus.
      WaitForHide(kTabGroupEditorBubbleId));
}

IN_PROC_BROWSER_TEST_F(HelpBubbleViewInteractiveUiTest,
                       ElementIdentifierFindsButton) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, default_button_clicked);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, non_default_button_clicked);
  constexpr char16_t kButton1Text[] = u"button 1";
  constexpr char16_t kButton2Text[] = u"button 2";

  user_education::HelpBubbleParams params = GetBubbleParams();

  user_education::HelpBubbleButtonParams button1;
  button1.text = kButton1Text;
  button1.is_default = true;
  button1.callback = default_button_clicked.Get();
  params.buttons.emplace_back(std::move(button1));

  user_education::HelpBubbleButtonParams button2;
  button2.text = kButton2Text;
  button2.is_default = false;
  button2.callback = non_default_button_clicked.Get();
  params.buttons.emplace_back(std::move(button2));

  EXPECT_CALL(default_button_clicked, Run).Times(1);

  RunTestSequence(
      // Show a help bubble and verify the button text.
      ShowHelpBubble(kToolbarAppMenuButtonElementId, std::move(params)),
      CheckViewProperty(HelpBubbleView::kDefaultButtonIdForTesting,
                        &views::LabelButton::GetText, kButton1Text),
      CheckViewProperty(HelpBubbleView::kFirstNonDefaultButtonIdForTesting,
                        &views::LabelButton::GetText, kButton2Text),
      // Press the default button; the bubble should close.
      PressButton(HelpBubbleView::kDefaultButtonIdForTesting),
      WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting));
}

// This is a combined test for both help bubbles anchored to menus and menu
// annotation.
//
// This test does work on Linux, however, because of the way events are routed
// on Wayland specifically (and on Linux in general) the test itself isn't
// reliable on Linux. It has been manually tested, and based on the way the
// annotation event routing works, if it did not work (a) it would not work on
// any platform, and (b) it would not be possible to close a menu by clicking
// away from it and into e.g. the omnibox.
#if !BUILDFLAG(IS_LINUX)
IN_PROC_BROWSER_TEST_F(HelpBubbleViewInteractiveUiTest, AnnotateMenu) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, default_button_clicked);
  constexpr char16_t kButton1Text[] = u"button 1";

  user_education::HelpBubbleParams params = GetBubbleParams();

  params.arrow = user_education::HelpBubbleArrow::kRightCenter;

  user_education::HelpBubbleButtonParams button1;
  button1.text = kButton1Text;
  button1.is_default = true;
  button1.callback = default_button_clicked.Get();
  params.buttons.emplace_back(std::move(button1));

  EXPECT_CALL(default_button_clicked, Run).Times(1);

  RunTestSequence(
      // Show the application menu and attach a bubble to a menu item.
      PressButton(kToolbarAppMenuButtonElementId),
      ShowHelpBubble(AppMenuModel::kDownloadsMenuItem, std::move(params)),

      // Hover the default button and verify that the inkdrop is highlighted.
      MoveMouseTo(HelpBubbleView::kDefaultButtonIdForTesting),

      // TODO(dfried): figure out if we can determine if an inkdrop is in a
      // hovered state; currently that information can't be accessed.

      // Click the default button and verify that the help bubble closes but the
      // menu does not.
      ClickMouse(), WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting),
      EnsurePresent(AppMenuModel::kDownloadsMenuItem));
}

// Verifies that we can safely show and then close two help bubbles attached to
// the same menu. This may happen transiently during tutorials.
IN_PROC_BROWSER_TEST_F(HelpBubbleViewInteractiveUiTest, TwoMenuHelpBubbles) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, button_clicked);
  constexpr char16_t kButtonText[] = u"button";

  // First bubble has no buttons.
  auto params1 = GetBubbleParams();
  params1.arrow = user_education::HelpBubbleArrow::kRightCenter;

  // Second bubble has a default button.
  auto params2 = GetBubbleParams();
  params2.arrow = user_education::HelpBubbleArrow::kRightCenter;

  user_education::HelpBubbleButtonParams button;
  button.text = kButtonText;
  button.is_default = true;
  button.callback = button_clicked.Get();
  params2.buttons.emplace_back(std::move(button));

  EXPECT_CALL(button_clicked, Run).Times(1);

  RunTestSequence(
      // Show the application menu and attach a bubble to two different menu
      // items.
      PressButton(kToolbarAppMenuButtonElementId),
      ShowHelpBubble(AppMenuModel::kDownloadsMenuItem, std::move(params1)),
      ShowHelpBubble(AppMenuModel::kMoreToolsMenuItem, std::move(params2)),

      // Use the mouse to click the default button on the second bubble and wait
      // for the bubble to disappear.
      //
      // The default button should be targetable because it is at the bottom of
      // the lower of the two help bubbles.
      MoveMouseTo(HelpBubbleView::kDefaultButtonIdForTesting), ClickMouse(),
      WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting)
          .SetTransitionOnlyOnEvent(true),
      FlushEvents(),

      // Close the remaining help bubble.
      CloseHelpBubble());
}

#endif
