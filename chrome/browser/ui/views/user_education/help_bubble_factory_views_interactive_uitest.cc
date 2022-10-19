// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_forward.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/widget_focus_waiter.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"

class HelpBubbleFactoryViewsUiTest : public InProcessBrowserTest {
 public:
 protected:
  ui::ElementContext context() {
    return browser()->window()->GetElementContext();
  }

  user_education::HelpBubbleFactoryRegistry* registry() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->GetFeaturePromoController()
        ->bubble_factory_registry();
  }

  views::TrackedElementViews* GetAnchorElement() {
    return views::ElementTrackerViews::GetInstance()->GetElementForView(
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->app_menu_button());
  }
};

// Moved to interactive uitests due to crbug.com/1290983 (widget activation is
// not reliable when running alongside other tests).
IN_PROC_BROWSER_TEST_F(HelpBubbleFactoryViewsUiTest,
                       ToggleFocusForAccessibility) {
  user_education::HelpBubbleParams params;
  params.body_text = u"Hello world!";
  user_education::HelpBubbleButtonParams button_params;
  button_params.text = u"Button";
  button_params.is_default = true;
  params.buttons.emplace_back(std::move(button_params));

  std::unique_ptr<user_education::HelpBubble> help_bubble =
      registry()->CreateHelpBubble(GetAnchorElement(), std::move(params));
  auto* const bubble_view =
      help_bubble->AsA<user_education::HelpBubbleViews>()->bubble_view();

  // Toggle focus to the help widget and then wait for it to be focused.
  {
    WidgetFocusWaiter waiter(bubble_view->GetWidget());
    waiter.WaitAfter(base::BindLambdaForTesting(
        [&]() { help_bubble->ToggleFocusForAccessibility(); }));
    EXPECT_TRUE(bubble_view->GetDefaultButtonForTesting()->HasFocus());
  }

  // Toggle focus to the anchor view and wait for it to become focused.
#if BUILDFLAG(IS_MAC)
  // Widget activation is a little wonky on Mac, so we'll just ensure that the
  // anchor element is correctly focused.
  help_bubble->ToggleFocusForAccessibility();
#else
  // On non-Mac platforms, we can safely wait for widget focus to return to the
  // browser window.
  {
    WidgetFocusWaiter waiter(GetAnchorElement()->view()->GetWidget());
    waiter.WaitAfter(base::BindLambdaForTesting(
        [&]() { help_bubble->ToggleFocusForAccessibility(); }));
  }
#endif
  EXPECT_TRUE(GetAnchorElement()->view()->HasFocus());
}

// Sending accelerators to the main window is flaky at best and doesn't work at
// all at worst on Mac, so we can't test it directly here.
// See http://crbug.com/824551 and ConstrainedWindowViewTest.FocusTest for
// examples of the tests simply not working on Mac.
// #if BUILDFLAG(IS_MAC)
// #define MAYBE_ToggleFocusViaAccelerator DISABLED_ToggleFocusViaAccelerator
// #else
// #define MAYBE_ToggleFocusViaAccelerator ToggleFocusViaAccelerator
// #endif

IN_PROC_BROWSER_TEST_F(HelpBubbleFactoryViewsUiTest,
                       ToggleFocusViaAccelerator) {
  user_education::HelpBubbleParams params;
  params.body_text = u"Hello world!";
  auto help_bubble_ptr =
      registry()->CreateHelpBubble(GetAnchorElement(), std::move(params));
  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto* const bubble_view =
      help_bubble_ptr->AsA<user_education::HelpBubbleViews>()->bubble_view();

#if BUILDFLAG(IS_MAC)

  // Focus the help bubble.
  views::test::WidgetActivationWaiter bubble_waiter(bubble_view->GetWidget(),
                                                    true);
  ASSERT_TRUE(chrome::ExecuteCommand(browser(), IDC_FOCUS_NEXT_PANE));
  bubble_waiter.Wait();

  // Focus the browser.
  views::test::WidgetActivationWaiter browser_waiter(browser_view->GetWidget(),
                                                     true);
  ASSERT_TRUE(chrome::ExecuteCommand(browser(), IDC_FOCUS_NEXT_PANE));
  browser_waiter.Wait();

#else  // !BUILDFLAG(IS_MAC)

  // Get the appropriate accelerator.
  ui::Accelerator accel;
  ASSERT_TRUE(browser_view->GetAccelerator(IDC_FOCUS_NEXT_PANE, &accel));

  // Focus the help bubble.
  views::test::WidgetActivationWaiter bubble_waiter(bubble_view->GetWidget(),
                                                    true);
  ASSERT_TRUE(browser_view->GetFocusManager()->ProcessAccelerator(accel));
  bubble_waiter.Wait();

  // Focus the browser.
  views::test::WidgetActivationWaiter browser_waiter(browser_view->GetWidget(),
                                                     true);
  ASSERT_TRUE(bubble_view->GetFocusManager()->ProcessAccelerator(accel));
  browser_waiter.Wait();

#endif  // BUILDFLAG(IS_MAC)
}
