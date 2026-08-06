// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/payments/payments_churned_users_bubble_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/payments_churned_users_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_test_support.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/test/widget_test.h"

namespace autofill {

inline constexpr int kSecurityTreatmentId = 1;
inline constexpr int kConvenienceTreatmentId = 2;

class PaymentsChurnedUsersBubbleViewsBrowserTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<int> {
 public:
  PaymentsChurnedUsersBubbleViewsBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kAutofillEnableResurrectingPaymentsUsers,
        {{"autofill_enable_resurrecting_payments_churned_users_treatment",
          base::NumberToString(GetParam())}});
  }
  ~PaymentsChurnedUsersBubbleViewsBrowserTest() override = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override { ShowBubble(); }

  void ShowBubble(base::OnceClosure accept_callback = base::DoNothing(),
                  base::OnceClosure cancel_callback = base::DoNothing(),
                  base::OnceClosure closed_callback = base::DoNothing()) {
    EXPECT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://new-tab-page")));
    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(browser()->GetProfile()),
        "user@example.com", signin::ConsentLevel::kSignin);
    autofill::ChromeAutofillClient* autofill_client =
        autofill::ChromeAutofillClient::FromWebContentsForTesting(
            browser()->tab_strip_model()->GetActiveWebContents());
    ASSERT_TRUE(autofill_client);
    autofill_client->GetPaymentsAutofillClient()->ShowPaymentsChurnedUsersUI(
        std::move(accept_callback), std::move(cancel_callback),
        std::move(closed_callback));
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

  PaymentsChurnedUsersBubbleView* GetBubbleView() {
    PaymentsChurnedUsersBubbleController* controller =
        PaymentsChurnedUsersBubbleController::From(
            *browser()->tab_strip_model()->GetActiveTab());
    if (!controller) {
      return nullptr;
    }

    return static_cast<PaymentsChurnedUsersBubbleView*>(
        controller->GetBubbleViewForTesting());
  }

  AutofillBubbleBase* GetAutofillBubbleView() {
    PaymentsChurnedUsersBubbleController* controller =
        PaymentsChurnedUsersBubbleController::From(
            *browser()->tab_strip_model()->GetActiveTab());
    if (!controller) {
      return nullptr;
    }

    return controller->GetBubbleViewForTesting();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         PaymentsChurnedUsersBubbleViewsBrowserTest,
                         testing::Values(kSecurityTreatmentId,
                                         kConvenienceTreatmentId));

IN_PROC_BROWSER_TEST_P(PaymentsChurnedUsersBubbleViewsBrowserTest, InvokeUi) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PaymentsChurnedUsersBubbleViewsBrowserTest, ShowBubble) {
  ShowBubble();
  EXPECT_TRUE(IsIconVisible());
  EXPECT_TRUE(IsBubbleShowing());
}

IN_PROC_BROWSER_TEST_P(PaymentsChurnedUsersBubbleViewsBrowserTest,
                       AcceptCallbackTriggered) {
  base::test::TestFuture<void> accept_future;
  ShowBubble(accept_future.GetCallback(), base::DoNothing(), base::DoNothing());

  PaymentsChurnedUsersBubbleView* bubble_view = GetBubbleView();
  ASSERT_TRUE(bubble_view);
  bubble_view->AcceptDialog();

  EXPECT_TRUE(accept_future.Wait());
  EXPECT_TRUE(base::test::RunUntil([&]() { return !IsIconVisible(); }));
}

IN_PROC_BROWSER_TEST_P(PaymentsChurnedUsersBubbleViewsBrowserTest,
                       CancelCallbackTriggered) {
  base::test::TestFuture<void> cancel_future;
  ShowBubble(base::DoNothing(), cancel_future.GetCallback(), base::DoNothing());

  PaymentsChurnedUsersBubbleView* bubble_view = GetBubbleView();
  ASSERT_TRUE(bubble_view);
  bubble_view->CancelDialog();

  EXPECT_TRUE(cancel_future.Wait());
  EXPECT_FALSE(IsIconVisible());
}

IN_PROC_BROWSER_TEST_P(PaymentsChurnedUsersBubbleViewsBrowserTest,
                       ClosedCallbackTriggered) {
  base::test::TestFuture<void> closed_future;
  ShowBubble(base::DoNothing(), base::DoNothing(), closed_future.GetCallback());

  PaymentsChurnedUsersBubbleView* bubble_view = GetBubbleView();
  ASSERT_TRUE(bubble_view);
  bubble_view->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kLostFocus);

  EXPECT_TRUE(closed_future.Wait());
  EXPECT_TRUE(IsIconVisible());
}

IN_PROC_BROWSER_TEST_P(PaymentsChurnedUsersBubbleViewsBrowserTest,
                       CloseButtonCallbackTriggered) {
  base::test::TestFuture<void> closed_future;
  ShowBubble(base::DoNothing(), base::DoNothing(), closed_future.GetCallback());

  PaymentsChurnedUsersBubbleView* bubble_view = GetBubbleView();
  ASSERT_TRUE(bubble_view);
  bubble_view->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);

  EXPECT_TRUE(closed_future.Wait());
  EXPECT_FALSE(IsIconVisible());
}

// TODO(crbug.com/529904307): Disabled due to flakey test.
IN_PROC_BROWSER_TEST_P(PaymentsChurnedUsersBubbleViewsBrowserTest,
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

IN_PROC_BROWSER_TEST_P(PaymentsChurnedUsersBubbleViewsBrowserTest,
                       AcceptanceShowsConfirmationBubble) {
  ShowBubble();

  PaymentsChurnedUsersBubbleView* bubble_view = GetBubbleView();
  ASSERT_TRUE(bubble_view);

  // Accept the dialog.
  bubble_view->AcceptDialog();

  // The confirmation bubble will be shown after a short delay.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    AutofillBubbleBase* current_bubble = GetAutofillBubbleView();
    if (!current_bubble || current_bubble == bubble_view) {
      return false;
    }

    // Check that it's the confirmation bubble by verifying its view ID.
    auto* location_bar_bubble =
        static_cast<AutofillLocationBarBubble*>(current_bubble);
    return location_bar_bubble->GetID() ==
           DialogViewId::
               SAVE_PAYMENT_METHOD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_BUBBLE_VIEWS;
  }));
}

}  // namespace autofill
