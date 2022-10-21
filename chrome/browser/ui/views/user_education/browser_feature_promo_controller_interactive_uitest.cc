// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"

#include <memory>

#include "base/test/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/widget_focus_waiter.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

class BrowserFeaturePromoControllerUiTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    browser_view_ = BrowserView::GetBrowserViewForBrowser(browser());
    controller_ = browser_view_->GetFeaturePromoController();
  }

  ui::TrackedElement* GetAnchorElement() {
    auto* const result =
        views::ElementTrackerViews::GetInstance()->GetElementForView(
            browser_view_->toolbar()->app_menu_button());
    CHECK(result);
    return result;
  }

 protected:
  raw_ptr<BrowserView, DanglingUntriaged> browser_view_ = nullptr;
  raw_ptr<BrowserFeaturePromoController, DanglingUntriaged> controller_ =
      nullptr;
};

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest, CanShowPromo) {
  EXPECT_TRUE(controller_->CanShowPromo(GetAnchorElement()));

  // Create a second widget and give it focus. We can't guarantee that we can
  // deactivate unless there is a second window, because of how some platforms
  // handle focus.
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.context = browser_view_->GetWidget()->GetNativeWindow();
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 200, 200);
  widget->Init(std::move(params));
  WidgetFocusWaiter waiter(widget.get());

  waiter.WaitAfter(base::BindLambdaForTesting([&]() {
    // Doing this dance will make sure the necessary message gets sent to the
    // window on all platforms we care about.
    widget->Show();
    browser_view_->GetWidget()->Deactivate();
    widget->Activate();
  }));

  // If the browser isn't active, then CanShowPromo should return false.
  EXPECT_FALSE(controller_->CanShowPromo(GetAnchorElement()));
}
