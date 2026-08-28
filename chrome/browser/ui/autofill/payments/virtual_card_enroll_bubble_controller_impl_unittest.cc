// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"

#include "base/check_op.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl_test_api.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

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

class ControllerTestSupport {
 public:
  explicit ControllerTestSupport(content::WebContents* web_contents)
      : card_art_image_(gfx::test::CreateImage(100, 50).AsImageSkia()),
        controller_(static_cast<VirtualCardEnrollBubbleControllerImpl*>(
            VirtualCardEnrollBubbleControllerImpl::GetOrCreate(web_contents))) {
    virtual_card_enrollment_fields_ =
        CreateVirtualCardEnrollmentFields(&card_art_image_);
  }

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
  MockAutofillVCNEnrollBottomSheetBridge() = default;

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

  test_support.controller()->SetupAndShowBubble(
      test_support.virtual_card_enrollment_fields(),
      /*accept_virtual_card_callback=*/base::DoNothing(),
      /*decline_virtual_card_callback=*/base::DoNothing());

  VirtualCardEnrollUiModel expected_model(
      test_support.virtual_card_enrollment_fields());
  const VirtualCardEnrollUiModel& ui_model =
      test_support.controller()->GetUiModel();
  EXPECT_EQ(ui_model.window_title(), expected_model.window_title());
  EXPECT_EQ(ui_model.explanatory_message(),
            expected_model.explanatory_message());
  EXPECT_EQ(ui_model.accept_action_text(), expected_model.accept_action_text());
  EXPECT_EQ(ui_model.cancel_action_text(), expected_model.cancel_action_text());
  EXPECT_EQ(ui_model.learn_more_link_text(),
            expected_model.learn_more_link_text());
  EXPECT_EQ(ui_model.enrollment_fields(), expected_model.enrollment_fields());
}

TEST_F(VirtualCardEnrollBubbleControllerImplBottomSheetTest, ShowBubble) {
  ControllerTestSupport test_support(web_contents());

  test_support.controller()->SetupAndShowBubble(
      test_support.virtual_card_enrollment_fields(),
      /*accept_virtual_card_callback=*/base::DoNothing(),
      /*decline_virtual_card_callback=*/base::DoNothing());

  EXPECT_TRUE(test_api(*test_support.controller()).DidShowBottomSheet());
}

TEST_F(VirtualCardEnrollBubbleControllerImplBottomSheetTest,
       ShowConfirmationBubbleView) {
  ControllerTestSupport test_support(web_contents());
  std::unique_ptr<MockAutofillVCNEnrollBottomSheetBridge> mock =
      std::make_unique<MockAutofillVCNEnrollBottomSheetBridge>();
  MockAutofillVCNEnrollBottomSheetBridge* bridge = mock.get();
  test_api(*test_support.controller())
      .SetAutofillVCNEnrollBottomSheetBridge(std::move(mock));

  EXPECT_CALL(*bridge, Hide());

  test_support.controller()->ShowConfirmationBubbleView(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess);
}
}  // namespace
}  // namespace autofill
