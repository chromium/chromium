// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"

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
        WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
        // Prevent direct chaining off the show event.
        FlushEvents());
  }

  // Closes the current help bubble and waits for it to hide.
  auto CloseHelpBubble() {
    return Steps(
        WithView(HelpBubbleView::kHelpBubbleElementIdForTesting,
                 [](HelpBubbleView* bubble) { bubble->GetWidget()->Close(); }),
        WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting),
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
      ShowHelpBubble(kAppMenuButtonElementId),
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
      ActivateSurface(kAppMenuButtonElementId),
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
      ShowHelpBubble(kAppMenuButtonElementId, std::move(params)),
      CheckViewProperty(HelpBubbleView::kDefaultButtonIdForTesting,
                        &views::LabelButton::GetText, kButton1Text),
      CheckViewProperty(HelpBubbleView::kFirstNonDefaultButtonIdForTesting,
                        &views::LabelButton::GetText, kButton2Text),
      // Press the default button; the bubble should close.
      PressButton(HelpBubbleView::kDefaultButtonIdForTesting),
      WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting));
}
