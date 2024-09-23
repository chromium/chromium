// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_vcn_enroll_bottom_sheet_bridge.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl_test_api.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/autofill_virtual_card_enrollment_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace autofill {
namespace {

class AutofillVCNEnrollBottomSheetBridgeTest
    : public ChromeRenderViewHostTestHarness {
 public:
  AutofillVCNEnrollBottomSheetBridgeTest()
      : card_art_image_(gfx::test::CreateImage(100, 50).AsImageSkia()) {}
  ~AutofillVCNEnrollBottomSheetBridgeTest() override = default;

  VirtualCardEnrollBubbleControllerImpl* BuildController() {
    VirtualCardEnrollBubbleControllerImpl* controller =
        static_cast<VirtualCardEnrollBubbleControllerImpl*>(
            VirtualCardEnrollBubbleControllerImpl::GetOrCreate(web_contents()));
    VirtualCardEnrollmentFields virtual_card_enrollment_fields;
    virtual_card_enrollment_fields.credit_card = test::GetFullServerCard();
    virtual_card_enrollment_fields.card_art_image = &card_art_image_;
    virtual_card_enrollment_fields.google_legal_message = {
        TestLegalMessageLine("google_test_legal_message")};
    virtual_card_enrollment_fields.issuer_legal_message = {
        TestLegalMessageLine("issuer_test_legal_message")};
    virtual_card_enrollment_fields.virtual_card_enrollment_source =
        VirtualCardEnrollmentSource::kUpstream;
    test_api(*controller)
        .SetUiModel(std::make_unique<VirtualCardEnrollUiModel>(
            virtual_card_enrollment_fields));
    return controller;
  }

 private:
  gfx::ImageSkia card_art_image_;
};

TEST_F(AutofillVCNEnrollBottomSheetBridgeTest,
       RequestShowContentWithoutWebContents) {
  AutofillVCNEnrollBottomSheetBridge bridge;

  bool did_show =
      bridge.RequestShowContent(/*web_contents=*/nullptr, /*delegate=*/nullptr);

  EXPECT_FALSE(did_show);
}

TEST_F(AutofillVCNEnrollBottomSheetBridgeTest,
       RequestShowContentWithWebContents) {
  auto delegate =
      std::make_unique<AutofillVirtualCardEnrollmentInfoBarDelegateMobile>(
          BuildController());
  AutofillVCNEnrollBottomSheetBridge bridge;

  bool did_show =
      bridge.RequestShowContent(web_contents(), std::move(delegate));

  EXPECT_FALSE(did_show);
}

class MockDelegate : public AutofillVirtualCardEnrollmentInfoBarDelegateMobile {
 public:
  explicit MockDelegate(VirtualCardEnrollBubbleControllerImpl* controller)
      : AutofillVirtualCardEnrollmentInfoBarDelegateMobile(controller) {}
  ~MockDelegate() override = default;
  MOCK_METHOD(void, InfoBarDismissed, (), (override));
  MOCK_METHOD(bool, Accept, (), (override));
  MOCK_METHOD(bool, Cancel, (), (override));
};

TEST_F(AutofillVCNEnrollBottomSheetBridgeTest, DismissCallback) {
  auto delegate = std::make_unique<MockDelegate>(BuildController());
  MockDelegate& delegate_reference = *delegate;
  AutofillVCNEnrollBottomSheetBridge bridge;
  bridge.RequestShowContent(web_contents(), std::move(delegate));

  EXPECT_CALL(delegate_reference, InfoBarDismissed());

  bridge.OnDismiss(/*env=*/nullptr);
}

TEST_F(AutofillVCNEnrollBottomSheetBridgeTest, AcceptCallback) {
  auto delegate = std::make_unique<MockDelegate>(BuildController());
  MockDelegate& delegate_reference = *delegate;
  AutofillVCNEnrollBottomSheetBridge bridge;
  bridge.RequestShowContent(web_contents(), std::move(delegate));

  EXPECT_CALL(delegate_reference, Accept());

  bridge.OnAccept(/*env=*/nullptr);
}

TEST_F(AutofillVCNEnrollBottomSheetBridgeTest, CancelCallback) {
  auto delegate = std::make_unique<MockDelegate>(BuildController());
  MockDelegate& delegate_reference = *delegate;
  AutofillVCNEnrollBottomSheetBridge bridge;
  bridge.RequestShowContent(web_contents(), std::move(delegate));

  EXPECT_CALL(delegate_reference, Cancel());

  bridge.OnCancel(/*env=*/nullptr);
}

TEST_F(AutofillVCNEnrollBottomSheetBridgeTest, RecordLinkClickMetric) {
  base::HistogramTester histogram_tester;
  auto delegate = std::make_unique<MockDelegate>(BuildController());
  AutofillVCNEnrollBottomSheetBridge bridge;
  bridge.RequestShowContent(web_contents(), std::move(delegate));

  bridge.RecordLinkClickMetric(
      /*env=*/nullptr,
      static_cast<int>(VirtualCardEnrollmentLinkType::
                           VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK));

  histogram_tester.ExpectTotalCount(
      "Autofill.VirtualCardEnroll.LinkClicked.Upstream.LearnMoreLink", 1);
}

}  // namespace
}  // namespace autofill
