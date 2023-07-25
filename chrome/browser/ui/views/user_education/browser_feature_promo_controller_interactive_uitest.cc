// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"

#include <memory>

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/widget_focus_observer.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using BrowserFeaturePromoControllerUiTest = InteractiveBrowserTest;

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest, CanShowPromo) {
  auto widget = std::make_unique<views::Widget>();

  auto can_show_promo = [this](ui::TrackedElement* anchor) {
    return static_cast<BrowserFeaturePromoController*>(
               browser()->window()->GetFeaturePromoController())
        ->CanShowPromo(anchor);
  };

  RunTestSequence(
      // Verify that at first, we can show the promo on the browser.
      CheckElement(kAppMenuButtonElementId, can_show_promo, true),
      // Start observing widget focus, and create the widget.
      ObserveState(views::test::kCurrentWidgetFocus),
      // Create a second widget and give it focus. We can't guarantee that we
      // can deactivate unless there is a second window, because of how some
      // platforms handle focus.
      WithView(kBrowserViewElementId,
               [&widget](BrowserView* browser_view) {
                 views::Widget::InitParams params(
                     views::Widget::InitParams::TYPE_WINDOW);
                 params.context = browser_view->GetWidget()->GetNativeWindow();
                 params.ownership =
                     views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
                 params.bounds = gfx::Rect(0, 0, 200, 200);
                 widget->Init(std::move(params));

                 // Doing this dance will make sure the necessary message gets
                 // sent to the window on all platforms we care about.
                 widget->Show();
                 browser_view->GetWidget()->Deactivate();
                 widget->Activate();
               }),
      // Wait for widget activation to move to the new widget.
      WaitForState(views::test::kCurrentWidgetFocus,
                   [&widget]() { return widget->GetNativeView(); }),
      // Verify that we can no longer show the promo, since the browser is not
      // the active window.
      CheckElement(kAppMenuButtonElementId, can_show_promo, false));
}
