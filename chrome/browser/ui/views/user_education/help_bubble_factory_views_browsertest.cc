// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/views/help_bubble_factory_views.h"

#include "base/functional/callback_forward.h"
#include "base/i18n/rtl.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"

class HelpBubbleFactoryViewsBrowsertest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    user_education::HelpBubbleParams params;
    params.arrow = user_education::HelpBubbleArrow::kTopRight;
    params.body_text = u"Hello world, I am a tutorial";
    params.progress = std::make_pair(3, 5);
    params.timeout = base::TimeDelta();

    help_bubble_ =
        registry()->CreateHelpBubble(GetAnchorElement(), std::move(params));
  }

 protected:
  ui::ElementContext context() {
    return browser()->window()->GetElementContext();
  }

  user_education::HelpBubbleFactoryRegistry* registry() {
    return &UserEducationServiceFactory::GetForBrowserContext(
                browser()->profile())
                ->help_bubble_factory_registry();
  }

  views::TrackedElementViews* GetAnchorElement() {
    return views::ElementTrackerViews::GetInstance()->GetElementForView(
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->app_menu_button());
  }

  std::unique_ptr<user_education::HelpBubble> help_bubble_;
};

IN_PROC_BROWSER_TEST_F(HelpBubbleFactoryViewsBrowsertest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(HelpBubbleFactoryViewsBrowsertest, ShowAndClose) {
  user_education::HelpBubbleParams params;
  params.body_text = u"Hello world!";
  help_bubble_ =
      registry()->CreateHelpBubble(GetAnchorElement(), std::move(params));
  ASSERT_TRUE(help_bubble_);
  ASSERT_TRUE(help_bubble_->IsA<user_education::HelpBubbleViews>());
  EXPECT_TRUE(help_bubble_->is_open());
  EXPECT_TRUE(help_bubble_->Close());
  EXPECT_FALSE(help_bubble_->is_open());
}

IN_PROC_BROWSER_TEST_F(HelpBubbleFactoryViewsBrowsertest, GetContext) {
  user_education::HelpBubbleParams params;
  params.body_text = u"Hello world!";
  help_bubble_ =
      registry()->CreateHelpBubble(GetAnchorElement(), std::move(params));
  EXPECT_EQ(context(), help_bubble_->GetContext());
}

// Note: if this test is flaky (especially the final EXPECT_TRUE) it may be
// that the browser window completely fills the test display and expanding it
// does not work. Please look at the error message reported and make
// adjustments to the logic as necessary (specifically how we adjust the
// browser size to force the help bubble to move).
IN_PROC_BROWSER_TEST_F(HelpBubbleFactoryViewsBrowsertest, GetAndUpdateBounds) {
  user_education::HelpBubbleParams params;
  params.body_text = u"Hello world!";
  help_bubble_ =
      registry()->CreateHelpBubble(GetAnchorElement(), std::move(params));
  const gfx::Rect initial_bounds = help_bubble_->GetBoundsInScreen();
  EXPECT_FALSE(initial_bounds.IsEmpty());
  views::Widget* const browser_widget =
      BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
  gfx::Rect widget_bounds = browser_widget->GetWindowBoundsInScreen();
  widget_bounds.set_width(widget_bounds.width() + 10);
  browser_widget->SetSize(widget_bounds.size());
  browser_widget->LayoutRootViewIfNecessary();
  help_bubble_->OnAnchorBoundsChanged();
  const gfx::Rect new_bounds = help_bubble_->GetBoundsInScreen();

  // This might fail if the display used for the test is too small. See notes
  // above.
  EXPECT_TRUE(!new_bounds.IsEmpty() && initial_bounds != new_bounds)
      << "Expanding the browser window did not result in the help bubble "
         "repositioning. See following details to diagnose:"
      << "\n  Direction (should be LTR): "
      << (base::i18n::IsRTL() ? "RTL" : "LTR")
      << "\n  Display dimensions (should hold browser window comfortably): "
      << display::Screen::GetScreen()
             ->GetDisplayMatching(initial_bounds)
             .bounds()
             .ToString()
      << "\n  Target browser bounds: " << widget_bounds.ToString()
      << "\n  Initial help bubble bounds: " << initial_bounds.ToString()
      << "\n  Final help bubble bounds: " << new_bounds.ToString();
}
