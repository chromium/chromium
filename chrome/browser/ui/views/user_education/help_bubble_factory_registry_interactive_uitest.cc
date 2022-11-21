// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"

class HelpBubbleFactoryRegistryInteractiveUitest : public InProcessBrowserTest {
 public:
  HelpBubbleFactoryRegistryInteractiveUitest() = default;
  ~HelpBubbleFactoryRegistryInteractiveUitest() override = default;

 protected:
  user_education::HelpBubbleParams GetBubbleParams() {
    user_education::HelpBubbleParams params;
    params.body_text = u"To X, do Y";
    params.arrow = user_education::HelpBubbleArrow::kRightTop;
    return params;
  }

  BrowserView* GetBrowserView() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  ui::ElementContext GetContext() {
    return browser()->window()->GetElementContext();
  }

  user_education::HelpBubbleFactoryRegistry* GetRegistry() {
    return GetBrowserView()
        ->GetFeaturePromoController()
        ->bubble_factory_registry();
  }
};

IN_PROC_BROWSER_TEST_F(HelpBubbleFactoryRegistryInteractiveUitest,
                       AnchorToViewsMenuItem) {
  const ui::ElementContext context = GetContext();

  // Demonstrate that we can find the app button without having to pick through
  // the BrowserView hierarcny.
  auto* const app_menu_button =
      ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
          kAppMenuButtonElementId, context);
  ASSERT_NE(nullptr, app_menu_button);
  InteractionTestUtilBrowser test_util;
  test_util.PressButton(app_menu_button);

  // Verify that the history menu item is visible.
  ui::TrackedElement* const element =
      ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
          AppMenuModel::kHistoryMenuItem, context);
  auto bubble = GetRegistry()->CreateHelpBubble(element, GetBubbleParams());
  EXPECT_TRUE(bubble);
  EXPECT_TRUE(bubble->is_open());
  EXPECT_TRUE(GetBrowserView()->GetWidget()->IsActive());
  bubble->Close();
}

IN_PROC_BROWSER_TEST_F(HelpBubbleFactoryRegistryInteractiveUitest,
                       AnchorToContextMenuItem) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  Tab* const tab = GetBrowserView()->tabstrip()->tab_at(0);

  // Because context menus run synchronously on Mac we will use an
  // InteractionSequence in order to show the context menu, respond immediately
  // when the menu is shown, and then tear down the menu (by closing the
  // browser) when we're done.
  //
  // Interaction sequences let us respond synchronously to events as well as
  // queue up sequences of actions in response to UI changes in a way that
  // would be far more complicated trying to use RunLoops, tasks, and events.

  auto open_context_menu = base::BindLambdaForTesting([&]() {
    tab->ShowContextMenu(tab->bounds().CenterPoint(),
                         ui::MenuSourceType::MENU_SOURCE_MOUSE);
  });

  auto set_up = base::BindLambdaForTesting(
      [&](ui::InteractionSequence*, ui::TrackedElement*) {
#if BUILDFLAG(IS_MAC)
        // Have to defer opening because this call is blocking on Mac;
        // subsequent steps will be called from within the run loop of the
        // context menu.
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, std::move(open_context_menu));
#else
        // Conversely, on other platforms, this is already an async call and
        // weirdness sometimes results from doing this from a callback, so
        // we'll call it directly instead.
        open_context_menu.Run();
#endif
      });

  auto show_bubble_on_menu_item = base::BindLambdaForTesting(
      [&](ui::InteractionSequence*, ui::TrackedElement* element) {
        auto bubble =
            GetRegistry()->CreateHelpBubble(element, GetBubbleParams());
        EXPECT_TRUE(bubble);
        EXPECT_TRUE(bubble->is_open());
        bubble->Close();
      });

  auto tear_down = base::BindLambdaForTesting([&](ui::TrackedElement*) {
    // For platforms where context menus run synchronously, if we don't close
    // the menu, we will get stuck in the inner message pump and can never
    // finish the test.
    static_cast<BrowserTabStripController*>(
        GetBrowserView()->tabstrip()->controller())
        ->CloseContextMenuForTesting();
  });

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .AddStep(ui::InteractionSequence::WithInitialElement(
              views::ElementTrackerViews::GetInstance()->GetElementForView(
                  tab, /* assign_temporary_id =*/true),
              set_up))
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(TabMenuModel::kAddToNewGroupItemIdentifier)
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetStartCallback(show_bubble_on_menu_item)
                       .SetEndCallback(tear_down)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}
