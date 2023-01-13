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

class HelpBubbleViewInteractiveTest : public InteractiveBrowserTest {
 public:
  HelpBubbleViewInteractiveTest() = default;
  ~HelpBubbleViewInteractiveTest() override = default;

 protected:
  HelpBubbleParams GetBubbleParams() {
    HelpBubbleParams params;
    params.body_text = u"To X, do Y";
    params.arrow = HelpBubbleArrow::kTopRight;
    return params;
  }

  // Activates the widget containing the given UI element.
  auto ActivateWidgetContaining(ElementSpecifier spec) {
    return AfterShow(spec, base::BindOnce([](ui::TrackedElement* element) {
                       auto* const widget = AsView(element)->GetWidget();
                       widget->Activate();
                       views::test::WidgetActivationWaiter(widget, true).Wait();
                       ASSERT_TRUE(widget->IsActive());
                     }));
  }
};

IN_PROC_BROWSER_TEST_F(HelpBubbleViewInteractiveTest,
                       WidgetNotActivatedByDefault) {
  RunTestSequence(
      CheckViewProperty(kBrowserViewElementId, &BrowserView::IsActive, true),
      WithView(kBrowserViewElementId,
               base::BindOnce([](BrowserView* view) { view->FocusToolbar(); })),
      CheckView(kBrowserViewElementId, base::BindOnce([](BrowserView* view) {
                  return view->GetFocusManager()->GetFocusedView();
                }),
                testing::Ne(nullptr)),
      WithView(kAppMenuButtonElementId,
               base::BindLambdaForTesting([this](views::View* button) {
                 new HelpBubbleView(GetHelpBubbleDelegate(), {button},
                                    GetBubbleParams());
               })),
      WaitForShow(HelpBubbleView::kHelpBubbleElementIdForTesting),
      FlushEvents(),
      CheckView(HelpBubbleView::kHelpBubbleElementIdForTesting,
                base::BindOnce([](HelpBubbleView* bubble) {
                  return bubble->GetWidget()->IsVisible();
                })),
      CheckViewProperty(kBrowserViewElementId, &BrowserView::IsActive, true),
      CheckView(HelpBubbleView::kHelpBubbleElementIdForTesting,
                base::BindOnce([](HelpBubbleView* view) {
                  return view->GetWidget()->IsActive();
                }),
                false),
      WithView(HelpBubbleView::kHelpBubbleElementIdForTesting,
               base::BindOnce([](HelpBubbleView* bubble) {
                 bubble->GetWidget()->Close();
               })),
      WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting));
}

// This is a regression test to ensure that help bubbles prevent other bubbles
// they are anchored to from closing on loss of focus. Failing to do this
// results in situations where a user can abort a user education journey by
// entering accessible keyboard navigation commands to try to read the help
// bubble, or by trying to interact with the help bubble with the mouse to e.g.
// close it.
IN_PROC_BROWSER_TEST_F(HelpBubbleViewInteractiveTest,
                       BubblePreventsCloseOnLossOfFocus) {
  browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      AfterShow(kTabGroupHeaderElementId,
                base::BindLambdaForTesting([](ui::TrackedElement* element) {
                  // Show the tab group editor bubble.
                  auto* const view = AsView(element);
                  view->ShowContextMenu(
                      view->GetLocalBounds().CenterPoint(),
                      ui::MenuSourceType::MENU_SOURCE_KEYBOARD);
                })),
      AfterShow(kTabGroupEditorBubbleId,
                base::BindOnce([](ui::TrackedElement* element) {
                  // Show a help bubble attached to the tab group editor
                  // bubble.
                  HelpBubbleParams params;
                  params.body_text = u"foo";
                  new HelpBubbleView(GetHelpBubbleDelegate(), {AsView(element)},
                                     std::move(params));
                })),
      // Activate the help bubble. This should not cause the editor to close.
      ActivateWidgetContaining(HelpBubbleView::kHelpBubbleElementIdForTesting),
      // Re-Activate the dialog.
      ActivateWidgetContaining(kTabGroupEditorBubbleId),
      // Close the help bubble.
      WithView(HelpBubbleView::kHelpBubbleElementIdForTesting,
               base::BindOnce([](HelpBubbleView* bubble) {
                 bubble->GetWidget()->Close();
               })),
      WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting),
      // Delay this to prevent chaining off of the previous hidden step; we need
      // the help bubble to fully clean up (this wouldn't be an issue in an
      // actual live browser because the activation would be due to user input
      // and therefore have to be processed via the message pump instead of
      // being allowed to execute inside the Widget's close logic).
      FlushEvents(),
      // Now that the help bubble is gone, locate the editor again and transfer
      // activation to its primary window widget (the browser window) - this
      // should close the editor as it is no longer pinned by the help bubble.
      ActivateWidgetContaining(kAppMenuButtonElementId),
      // Verify that the editor bubble closes now that it has lost focus.
      WaitForHide(kTabGroupEditorBubbleId));
}

IN_PROC_BROWSER_TEST_F(HelpBubbleViewInteractiveTest,
                       ElementIdentifierFindsButton) {
  constexpr char16_t kButton1Text[] = u"button 1";
  constexpr char16_t kButton2Text[] = u"button 2";

  user_education::HelpBubbleParams params = GetBubbleParams();

  user_education::HelpBubbleButtonParams button1;
  button1.text = kButton1Text;
  button1.is_default = true;
  params.buttons.emplace_back(std::move(button1));

  user_education::HelpBubbleButtonParams button2;
  button2.text = kButton2Text;
  button2.is_default = false;
  params.buttons.emplace_back(std::move(button2));

  RunTestSequence(
      WithView(kAppMenuButtonElementId,
               base::BindLambdaForTesting([&](views::View* button) {
                 new HelpBubbleView(GetHelpBubbleDelegate(), {button},
                                    std::move(params));
               })),
      CheckViewProperty(HelpBubbleView::kDefaultButtonIdForTesting,
                        &views::LabelButton::GetText, kButton1Text),
      CheckViewProperty(HelpBubbleView::kFirstNonDefaultButtonIdForTesting,
                        &views::LabelButton::GetText, kButton2Text),
      PressButton(HelpBubbleView::kDefaultButtonIdForTesting),
      WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting));
}
