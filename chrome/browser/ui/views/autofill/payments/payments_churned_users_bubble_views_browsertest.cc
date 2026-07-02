// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/payments/payments_churned_users_bubble_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/autofill/payments/payments_churned_users_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_test_support.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/test/widget_test.h"

namespace autofill {

class PaymentsChurnedUsersBubbleViewsBrowserTest
    : public ::InProcessBrowserTest {
 public:
  PaymentsChurnedUsersBubbleViewsBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kAutofillEnableResurrectingPaymentsUsers);
  }
  ~PaymentsChurnedUsersBubbleViewsBrowserTest() override = default;

  void ShowBubble() {
    EXPECT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://new-tab-page")));
    autofill::ChromeAutofillClient* autofill_client =
        autofill::ChromeAutofillClient::FromWebContentsForTesting(
            browser()->tab_strip_model()->GetActiveWebContents());
    ASSERT_TRUE(autofill_client);
    autofill_client->GetPaymentsAutofillClient()->ShowPaymentsChurnedUsersUI();
  }

  bool IsIconVisible() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    auto* provider = browser_view->toolbar_button_provider();
    IconLabelBubbleView* icon = page_actions::GetIconLabelBubbleViewForTesting(
        provider->GetPageActionViewInterface(
            kActionShowPaymentsChurnedUsersBubble),
        kActionShowPaymentsChurnedUsersBubble);
    return icon && icon->GetVisible();
  }

  bool IsBubbleShowing() {
    PaymentsChurnedUsersBubbleController* controller =
        PaymentsChurnedUsersBubbleController::From(
            *browser()->tab_strip_model()->GetActiveTab());
    return controller && controller->IsShowingBubble();
  }

  void ClickOnIcon() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    auto* provider = browser_view->toolbar_button_provider();
    views::View* icon = page_actions::GetIconLabelBubbleViewForTesting(
        provider->GetPageActionViewInterface(
            kActionShowPaymentsChurnedUsersBubble),
        kActionShowPaymentsChurnedUsersBubble);
    EXPECT_TRUE(icon);
    if (!icon) {
      return;
    }

    ui::MouseEvent pressed(ui::EventType::kMousePressed, gfx::Point(),
                           gfx::Point(), base::TimeTicks::Now(),
                           ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    icon->OnMousePressed(pressed);
    ui::MouseEvent released_event =
        ui::MouseEvent(ui::EventType::kMouseReleased, gfx::Point(),
                       gfx::Point(), base::TimeTicks::Now(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    icon->OnMouseReleased(released_event);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PaymentsChurnedUsersBubbleViewsBrowserTest, ShowBubble) {
  ShowBubble();
  EXPECT_TRUE(IsIconVisible());
  EXPECT_TRUE(IsBubbleShowing());
}

// TODO(crbug.com/529904307): Disabled due to flakey test.
IN_PROC_BROWSER_TEST_F(PaymentsChurnedUsersBubbleViewsBrowserTest,
                       DISABLED_ReshowBubbleOnIconClick) {
  ShowBubble();

  PaymentsChurnedUsersBubbleController* controller =
      PaymentsChurnedUsersBubbleController::From(
          *browser()->tab_strip_model()->GetActiveTab());
  if (controller) {
    controller->HideBubble(false);
    ASSERT_TRUE(base::test::RunUntil([&]() { return !IsBubbleShowing(); }));
  }

  EXPECT_FALSE(IsBubbleShowing());

  ClickOnIcon();

  EXPECT_TRUE(IsBubbleShowing());
}

}  // namespace autofill
