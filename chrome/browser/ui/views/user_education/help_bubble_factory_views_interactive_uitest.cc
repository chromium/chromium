// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_forward.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/user_education/help_bubble_factory_registry.h"
#include "chrome/browser/ui/user_education/help_bubble_params.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/help_bubble_factory_views.h"
#include "chrome/browser/ui/views/user_education/help_bubble_view.h"
#include "chrome/browser/ui/views/user_education/user_education_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
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

  HelpBubbleFactoryRegistry* registry() {
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
  HelpBubbleParams params;
  params.body_text = u"Hello world!";
  HelpBubbleButtonParams button_params;
  button_params.text = u"Button";
  button_params.is_default = true;
  params.buttons.emplace_back(std::move(button_params));

  std::unique_ptr<HelpBubble> help_bubble =
      registry()->CreateHelpBubble(GetAnchorElement(), std::move(params));
  HelpBubbleView* const bubble_view =
      help_bubble->AsA<HelpBubbleViews>()->bubble_view();

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
