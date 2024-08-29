// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"

#include "base/check_op.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl_test_api.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "content/public/browser/web_contents_user_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace autofill {
namespace {

VirtualCardEnrollmentFields CreateVirtualCardEnrollmentFields() {
  VirtualCardEnrollmentFields virtual_card_enrollment_fields;
  virtual_card_enrollment_fields.credit_card = test::GetFullServerCard();
  gfx::ImageSkia card_art_image = gfx::test::CreateImage(100, 50).AsImageSkia();
  virtual_card_enrollment_fields.card_art_image = &card_art_image;
  virtual_card_enrollment_fields.google_legal_message = {
      TestLegalMessageLine("google_test_legal_message")};
  virtual_card_enrollment_fields.issuer_legal_message = {
      TestLegalMessageLine("issuer_test_legal_message")};
  virtual_card_enrollment_fields.virtual_card_enrollment_source =
      VirtualCardEnrollmentSource::kUpstream;

  return virtual_card_enrollment_fields;
}

#if BUILDFLAG(IS_ANDROID)
class ControllerTestSupport {
 public:
  explicit ControllerTestSupport(content::WebContents* web_contents)
      : card_art_image_(gfx::test::CreateImage(100, 50).AsImageSkia()),
        controller_(static_cast<VirtualCardEnrollBubbleControllerImpl*>(
            VirtualCardEnrollBubbleControllerImpl::GetOrCreate(web_contents))) {
    virtual_card_enrollment_fields_ = CreateVirtualCardEnrollmentFields();
  }

  ~ControllerTestSupport() = default;

  VirtualCardEnrollBubbleControllerImpl* controller() const {
    return controller_;
  }

  const VirtualCardEnrollmentFields& virtual_card_enrollment_fields() const {
    return virtual_card_enrollment_fields_;
  }

 private:
  gfx::ImageSkia card_art_image_;
  raw_ptr<VirtualCardEnrollBubbleControllerImpl> controller_;
  VirtualCardEnrollmentFields virtual_card_enrollment_fields_;
};

class MockAutofillVCNEnrollBottomSheetBridge
    : public AutofillVCNEnrollBottomSheetBridge {
 public:
  MockAutofillVCNEnrollBottomSheetBridge()
      : AutofillSaveCardBottomSheetBridge() {}

  MOCK_METHOD(void, Hide, (), (override));
};

class VirtualCardEnrollBubbleControllerImplBottomSheetTest
    : public ChromeRenderViewHostTestHarness {
 public:
  VirtualCardEnrollBubbleControllerImplBottomSheetTest() = default;
};

TEST_F(VirtualCardEnrollBubbleControllerImplBottomSheetTest,
       ShowBubbleSetsUiModel) {
  ControllerTestSupport test_support(web_contents());

  test_support.controller()->ShowBubble(
      test_support.virtual_card_enrollment_fields(),
      /*accept_virtual_card_callback=*/base::DoNothing(),
      /*decline_virtual_card_callback=*/base::DoNothing());

  auto expected_model = VirtualCardEnrollUiModel::Create(
      test_support.virtual_card_enrollment_fields());
  EXPECT_EQ(*test_support.controller()->GetUiModel(), *expected_model);
}

TEST_F(VirtualCardEnrollBubbleControllerImplBottomSheetTest, ShowBubble) {
  ControllerTestSupport test_support(web_contents());

  test_support.controller()->ShowBubble(
      test_support.virtual_card_enrollment_fields(),
      /*accept_virtual_card_callback=*/base::DoNothing(),
      /*decline_virtual_card_callback=*/base::DoNothing());

  EXPECT_TRUE(test_api(test_support.controller()).DidShowBottomSheet());
}

TEST_F(VirtualCardEnrollBubbleControllerImplBottomSheetTest,
       ShowConfirmationBubbleView) {
  ControllerTestSupport test_support(web_contents());
  std::unique_ptr<MockAutofillVCNEnrollBottomSheetBridge> mock =
      std::make_unique<MockAutofillVCNEnrollBottomSheetBridge>();
  MockAutofillVCNEnrollBottomSheetBridge* bridge = mock.get();
  test_api(test_support.controller())
      .SetSetAutofillVCNEnrollBottomSheetBridge(mock);

  EXPECT_CALL(*bridge, Hide());

  test_support.controller()->ShowConfirmationBubbleView(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess);
}
#endif  // BUILDFLAG(IS_ANDROID)

class TestVirtualCardEnrollBubbleControllerImpl
    : public VirtualCardEnrollBubbleControllerImpl {
 public:
  static void CreateForTesting(content::WebContents* web_contents) {
    web_contents->SetUserData(
        UserDataKey(),
        std::make_unique<TestVirtualCardEnrollBubbleControllerImpl>(
            web_contents));
  }

  explicit TestVirtualCardEnrollBubbleControllerImpl(
      content::WebContents* web_contents)
      : VirtualCardEnrollBubbleControllerImpl(web_contents) {}

 private:
  bool IsWebContentsActive() override { return true; }
};

class VirtualCardEnrollBubbleControllerImplBubbleViewTest
    : public BrowserWithTestWindowTest {
 public:
  VirtualCardEnrollBubbleControllerImplBubbleViewTest() = default;
  VirtualCardEnrollBubbleControllerImplBubbleViewTest(
      VirtualCardEnrollBubbleControllerImplBubbleViewTest&) = delete;
  VirtualCardEnrollBubbleControllerImplBubbleViewTest& operator=(
      VirtualCardEnrollBubbleControllerImplBubbleViewTest&) = delete;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("about:blank"));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    TestVirtualCardEnrollBubbleControllerImpl::CreateForTesting(web_contents);
    virtual_card_enrollment_fields_ = CreateVirtualCardEnrollmentFields();
  }

  void ShowBubble() {
    controller()->ShowBubble(
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
  TestVirtualCardEnrollBubbleControllerImpl* controller() {
    return static_cast<TestVirtualCardEnrollBubbleControllerImpl*>(
        TestVirtualCardEnrollBubbleControllerImpl::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents()));
  }
  gfx::ImageSkia card_art_image_;
  base::test::ScopedFeatureList features_{
      features::kAutofillEnableVcnEnrollLoadingAndConfirmation};
  VirtualCardEnrollmentFields virtual_card_enrollment_fields_;
};

// Ensures that bubble acceptance and loading shown metrics are recorded after
// bubble is shown and accepted .
TEST_F(VirtualCardEnrollBubbleControllerImplBubbleViewTest, ShowBubble) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  EXPECT_NE(GetBubbleViews(), nullptr);
  controller()->OnAcceptButton(/*did_switch_to_loading_state=*/false);

  // Metric should not be recorded from the accept button.
  histogram_tester.ExpectTotalCount(
      "Autofill.VirtualCardEnrollBubble.Result.Upstream.FirstShow", 0);

  controller()->OnBubbleClosed(PaymentsBubbleClosedReason::kAccepted);
  controller()->HideIconAndBubble();
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
TEST_F(VirtualCardEnrollBubbleControllerImplBubbleViewTest,
       ShowBubbleInLoadingState) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  EXPECT_NE(GetBubbleViews(), nullptr);
  controller()->OnAcceptButton(/*did_switch_to_loading_state=*/true);

  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.LoadingShown", true, 1);

  // Metric should be recorded from the accept button.
  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.Result.Upstream.FirstShow",
      VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_ACCEPTED,
      1);

  controller()->OnBubbleClosed(PaymentsBubbleClosedReason::kClosed);
  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.LoadingResult",
      VirtualCardEnrollmentBubbleResult::VIRTUAL_CARD_ENROLLMENT_BUBBLE_CLOSED,
      1);
}

#if !BUILDFLAG(IS_ANDROID)
// Tests virtual card enrollment flow with loading and confirmation.
TEST_F(VirtualCardEnrollBubbleControllerImplBubbleViewTest,
       ShowBubbleInLoadingAndConfirmationState) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  EXPECT_NE(GetBubbleViews(), nullptr);
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
  EXPECT_NE(GetBubbleViews(), nullptr);
  EXPECT_TRUE(controller()->IsIconVisible());

  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.ConfirmationShown.CardEnrolled", true,
      1);

  controller()->OnBubbleClosed(PaymentsBubbleClosedReason::kClosed);
  // Expect the metric for virtual card enroll bubble to not change after
  // showing the confirmation bubble.
  histogram_tester.ExpectTotalCount(
      "Autofill.VirtualCardEnrollBubble.Result.Upstream.FirstShow", 1);

  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.ConfirmationResult.CardEnrolled",
      VirtualCardEnrollmentBubbleResult::VIRTUAL_CARD_ENROLLMENT_BUBBLE_CLOSED,
      1);
}

// Test that on getting client-side timeout, virtual card bubble is closed in
// loading state and confirmation dialog is not shown.
TEST_F(VirtualCardEnrollBubbleControllerImplBubbleViewTest,
       CloseBubbleInLoadingState_NoConfirmationBubble_ClientSideTimeout) {
  ShowBubble();
  EXPECT_NE(GetBubbleViews(), nullptr);
  EXPECT_TRUE(controller()->IsIconVisible());
  controller()->OnAcceptButton(/*did_switch_to_loading_state=*/true);
  controller()->ShowConfirmationBubbleView(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kClientSideTimeout);
  EXPECT_EQ(GetBubbleViews(), nullptr);
  EXPECT_FALSE(controller()->IsIconVisible());
}

// Tests that the correct confirmation result metric is logged when the
// confirmation bubble is closed after the card is not enrolled.
TEST_F(VirtualCardEnrollBubbleControllerImplBubbleViewTest,
       Metric_CloseConfirmationBubble_CardNotEnrolled) {
  base::HistogramTester histogram_tester;

  ShowBubble();
  controller()->OnAcceptButton(/*did_switch_to_loading_state=*/true);
  controller()->ShowConfirmationBubbleView(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure);
  controller()->OnBubbleClosed(PaymentsBubbleClosedReason::kClosed);

  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.ConfirmationResult.CardNotEnrolled",
      VirtualCardEnrollmentBubbleResult::VIRTUAL_CARD_ENROLLMENT_BUBBLE_CLOSED,
      1);
}
#endif  // !BUILDFLAG(IS_ANDROID)
}  // namespace
}  // namespace autofill
