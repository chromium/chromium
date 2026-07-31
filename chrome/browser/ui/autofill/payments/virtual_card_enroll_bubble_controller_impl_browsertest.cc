// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl_test_api.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/payments/save_payment_method_and_virtual_card_enroll_confirmation_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_enroll_bubble_views.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace autofill {
namespace {

VirtualCardEnrollmentFields CreateVirtualCardEnrollmentFields(
    gfx::ImageSkia* card_art_image) {
  VirtualCardEnrollmentFields virtual_card_enrollment_fields;
  virtual_card_enrollment_fields.credit_card = test::GetFullServerCard();
  virtual_card_enrollment_fields.card_art_image = card_art_image;
  virtual_card_enrollment_fields.google_legal_message = {
      TestLegalMessageLine("google_test_legal_message")};
  virtual_card_enrollment_fields.issuer_legal_message = {
      TestLegalMessageLine("issuer_test_legal_message")};
  virtual_card_enrollment_fields.virtual_card_enrollment_source =
      VirtualCardEnrollmentSource::kUpstream;

  return virtual_card_enrollment_fields;
}

class VirtualCardEnrollBubbleControllerImplBubbleViewTest
    : public InProcessBrowserTest {
 public:
  VirtualCardEnrollBubbleControllerImplBubbleViewTest() = default;
  VirtualCardEnrollBubbleControllerImplBubbleViewTest(
      const VirtualCardEnrollBubbleControllerImplBubbleViewTest&) = delete;
  VirtualCardEnrollBubbleControllerImplBubbleViewTest& operator=(
      const VirtualCardEnrollBubbleControllerImplBubbleViewTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Ensure that the browser window is active.
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    VirtualCardEnrollBubbleControllerImpl::CreateForWebContents(web_contents);
    virtual_card_enrollment_fields_ =
        CreateVirtualCardEnrollmentFields(&card_art_image_);
  }

  void ShowBubble() {
    controller()->SetupAndShowBubble(
        virtual_card_enrollment_fields(),
        /*accept_virtual_card_callback=*/base::DoNothing(),
        /*decline_virtual_card_callback=*/base::DoNothing());
  }

  AutofillBubbleBase* GetBubbleViews() {
    return controller()->GetVirtualCardBubbleView();
  }

  const VirtualCardEnrollmentFields& virtual_card_enrollment_fields() const {
    return virtual_card_enrollment_fields_;
  }

 protected:
  VirtualCardEnrollBubbleControllerImpl* controller() {
    return VirtualCardEnrollBubbleControllerImpl::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }
  gfx::ImageSkia card_art_image_ =
      gfx::test::CreateImage(100, 50).AsImageSkia();
  VirtualCardEnrollmentFields virtual_card_enrollment_fields_;
};

// Ensures that bubble acceptance and loading shown metrics are recorded after
// bubble is shown and accepted .
IN_PROC_BROWSER_TEST_F(VirtualCardEnrollBubbleControllerImplBubbleViewTest,
                       ShowBubble) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  auto* bubble_views = GetBubbleViews();
  ASSERT_NE(bubble_views, nullptr);

  controller()->OnAcceptButton(/*did_switch_to_loading_state=*/false);

  // Metric should not be recorded from the accept button.
  histogram_tester.ExpectTotalCount(
      "Autofill.VirtualCardEnrollBubble.Result.Upstream.FirstShow", 0);

  // Close the bubble simulating accept button click.
  auto* views_impl = static_cast<VirtualCardEnrollBubbleViews*>(bubble_views);
  views::test::WidgetDestroyedWaiter waiter(views_impl->GetWidget());
  views_impl->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);
  waiter.Wait();

  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.Result.Upstream.FirstShow",
      VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_ACCEPTED,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.LoadingShown", false, 1);
}

// Ensures that bubble acceptance, loading shown, and loading result metrics are
// recorded when the bubble gets closed from the loading state.
IN_PROC_BROWSER_TEST_F(VirtualCardEnrollBubbleControllerImplBubbleViewTest,
                       ShowBubbleInLoadingState) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  auto* bubble_views = GetBubbleViews();
  ASSERT_NE(bubble_views, nullptr);

  controller()->OnAcceptButton(/*did_switch_to_loading_state=*/true);

  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.LoadingShown", true, 1);

  // Metric should be recorded from the accept button.
  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.Result.Upstream.FirstShow",
      VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_ACCEPTED,
      1);

  // Close the bubble simulating close button click.
  auto* views_impl = static_cast<VirtualCardEnrollBubbleViews*>(bubble_views);
  views::test::WidgetDestroyedWaiter waiter(views_impl->GetWidget());
  views_impl->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
  waiter.Wait();

  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.LoadingResult",
      VirtualCardEnrollmentBubbleResult::VIRTUAL_CARD_ENROLLMENT_BUBBLE_CLOSED,
      1);
}

// Tests virtual card enrollment flow with loading and confirmation.
IN_PROC_BROWSER_TEST_F(VirtualCardEnrollBubbleControllerImplBubbleViewTest,
                       ShowBubbleInLoadingAndConfirmationState) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  auto* bubble_views = GetBubbleViews();
  ASSERT_NE(bubble_views, nullptr);
  EXPECT_TRUE(controller()->IsIconVisible());
  EXPECT_EQ(test_api(*controller()).GetEnrollmentStatus(),
            VirtualCardEnrollBubbleControllerImpl::EnrollmentStatus::kNone);

  controller()->OnAcceptButton(/*did_switch_to_loading_state=*/true);
  EXPECT_EQ(test_api(*controller()).GetEnrollmentStatus(),
            VirtualCardEnrollBubbleControllerImpl::EnrollmentStatus::
                kPaymentsServerRequestInFlight);

  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.Result.Upstream.FirstShow",
      VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_ACCEPTED,
      1);

  controller()->ShowConfirmationBubbleView(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess);
  EXPECT_EQ(
      test_api(*controller()).GetEnrollmentStatus(),
      VirtualCardEnrollBubbleControllerImpl::EnrollmentStatus::kCompleted);

  auto* confirmation_bubble_views = GetBubbleViews();
  ASSERT_NE(confirmation_bubble_views, nullptr);
  EXPECT_TRUE(controller()->IsIconVisible());

  // Close the confirmation bubble simulating close button click.
  auto* views_impl = static_cast<
      SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews*>(
      confirmation_bubble_views);
  views::test::WidgetDestroyedWaiter waiter(views_impl->GetWidget());
  views_impl->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
  waiter.Wait();

  // Expect the metric for virtual card enroll bubble to not change after
  // showing the confirmation bubble.
  histogram_tester.ExpectTotalCount(
      "Autofill.VirtualCardEnrollBubble.Result.Upstream.FirstShow", 1);
}

// Test that on getting client-side timeout, virtual card bubble is closed in
// loading state and confirmation dialog is not shown.
IN_PROC_BROWSER_TEST_F(
    VirtualCardEnrollBubbleControllerImplBubbleViewTest,
    CloseBubbleInLoadingState_NoConfirmationBubble_ClientSideTimeout) {
  ShowBubble();
  auto* bubble_views = GetBubbleViews();
  ASSERT_NE(bubble_views, nullptr);
  EXPECT_TRUE(controller()->IsIconVisible());
  controller()->OnAcceptButton(/*did_switch_to_loading_state=*/true);

  auto* views_impl = static_cast<VirtualCardEnrollBubbleViews*>(bubble_views);
  views::test::WidgetDestroyedWaiter waiter(views_impl->GetWidget());

  controller()->ShowConfirmationBubbleView(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kClientSideTimeout);

  waiter.Wait();

  EXPECT_EQ(GetBubbleViews(), nullptr);
  EXPECT_FALSE(controller()->IsIconVisible());
}

}  // namespace
}  // namespace autofill
