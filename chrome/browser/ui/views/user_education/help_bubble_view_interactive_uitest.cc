// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"

using user_education::HelpBubbleArrow;
using user_education::HelpBubbleParams;
using user_education::HelpBubbleView;

class HelpBubbleViewInteractiveTest : public InProcessBrowserTest {
 public:
  HelpBubbleViewInteractiveTest() = default;
  ~HelpBubbleViewInteractiveTest() override = default;

 protected:
  views::TrackedElementViews* GetAnchorElement() {
    return views::ElementTrackerViews::GetInstance()->GetElementForView(
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->app_menu_button());
  }

  HelpBubbleParams GetBubbleParams() {
    HelpBubbleParams params;
    params.body_text = u"To X, do Y";
    params.arrow = HelpBubbleArrow::kTopRight;
    return params;
  }
};

IN_PROC_BROWSER_TEST_F(HelpBubbleViewInteractiveTest,
                       WidgetNotActivatedByDefault) {
  auto params = GetBubbleParams();

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto* const focus_manager = browser_view->GetWidget()->GetFocusManager();
  EXPECT_TRUE(browser_view->GetWidget()->IsActive());

  browser_view->FocusToolbar();
  views::View* const initial_focused_view = focus_manager->GetFocusedView();
  EXPECT_NE(nullptr, initial_focused_view);

  auto* const bubble = new HelpBubbleView(
      GetHelpBubbleDelegate(), GetAnchorElement()->view(), std::move(params));
  views::test::WidgetVisibleWaiter(bubble->GetWidget()).Wait();

  EXPECT_TRUE(browser_view->GetWidget()->IsActive());
  EXPECT_FALSE(bubble->GetWidget()->IsActive());
  bubble->Close();
}

// This is a regression test to ensure that help bubbles prevent other bubbles
// they are anchored to from closing on loss of focus. Failing to do this
// results in situations where a user can abort a user education journey by
// entering accessible keyboard navigation commands to try to read the help
// bubble, or by trying to interact with the help bubble with the mouse to e.g.
// close it.
IN_PROC_BROWSER_TEST_F(HelpBubbleViewInteractiveTest,
                       BubblePreventsCloseOnLossOfFocus) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);

  HelpBubbleView* help_bubble_view = nullptr;

  browser()->tab_strip_model()->AddToNewGroup({0});
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetContext(browser()->window()->GetElementContext())
          .SetCompletedCallback(completed.Get())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetElementID(kTabGroupHeaderElementId)
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [](ui::InteractionSequence*,
                         ui::TrackedElement* element) {
                        // Show the tab group editor bubble.
                        auto* const view =
                            element->AsA<views::TrackedElementViews>()->view();
                        view->ShowContextMenu(
                            view->GetLocalBounds().CenterPoint(),
                            ui::MenuSourceType::MENU_SOURCE_KEYBOARD);
                      }))
                  .Build())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetElementID(kTabGroupEditorBubbleId)
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetMustRemainVisible(true)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        // Show a help bubble attached to the tab group editor
                        // bubble.
                        auto* const anchor_view =
                            element->AsA<views::TrackedElementViews>()->view();
                        HelpBubbleParams params;
                        params.body_text = u"foo";
                        help_bubble_view =
                            new HelpBubbleView(GetHelpBubbleDelegate(),
                                               anchor_view, std::move(params));
                      }))
                  .Build())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetElementID(HelpBubbleView::kHelpBubbleElementIdForTesting)
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetMustRemainVisible(true)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        // Activate the help bubble. This should not cause the
                        // editor to close.
                        auto* const widget =
                            element->AsA<views::TrackedElementViews>()
                                ->view()
                                ->GetWidget();
                        widget->Activate();
                        views::test::WidgetActivationWaiter(widget, true)
                            .Wait();
                      }))
                  .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kTabGroupEditorBubbleId)
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetMustBeVisibleAtStart(true)
                       .SetMustRemainVisible(true)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             // Activate the editor then close the help bubble.
                             auto* const widget =
                                 element->AsA<views::TrackedElementViews>()
                                     ->view()
                                     ->GetWidget();
                             widget->Activate();
                             views::test::WidgetActivationWaiter(widget, true)
                                 .Wait();
                             ASSERT_TRUE(widget->IsActive());
                             help_bubble_view->GetWidget()->Close();
                           }))
                       .Build())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  // Wait for the help bubble to close.
                  .SetElementID(HelpBubbleView::kHelpBubbleElementIdForTesting)
                  .SetType(ui::InteractionSequence::StepType::kHidden)
                  .Build())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetElementID(kTabGroupEditorBubbleId)
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetMustBeVisibleAtStart(true)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        // Now that the help bubble is gone, locate the editor
                        // again and transfer activation to its primary window
                        // widget (the browser window)
                        // - this should close the editor as it is no longer
                        // pinned by the help bubble.
                        auto* const widget =
                            element->AsA<views::TrackedElementViews>()
                                ->view()
                                ->GetWidget();
                        // Delay this in case we're chaining off of the previous
                        // hidden step; we need the help bubble to fully clean
                        // up (this wouldn't be an issue in an actual live
                        // browser because the activation would be due to user
                        // input and therefore have to be processed via the
                        // message pump instead of being allowed to execute
                        // inside the Widget's close logic).
                        base::ThreadTaskRunnerHandle::Get()->PostTask(
                            FROM_HERE,
                            base::BindOnce(
                                [](views::Widget* widget) {
                                  widget->GetPrimaryWindowWidget()->Activate();
                                },
                                base::Unretained(widget)));
                      }))
                  .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       // Verify that the editor bubble closes now that it has
                       // lost focus.
                       .SetElementID(kTabGroupEditorBubbleId)
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}
