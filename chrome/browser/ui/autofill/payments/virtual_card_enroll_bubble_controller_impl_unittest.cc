// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl_test_api.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace autofill {
namespace {

class ControllerTestSupport {
 public:
  explicit ControllerTestSupport(content::WebContents* web_contents)
      : card_art_image_(gfx::test::CreateImage().AsImageSkia()),
        controller_(static_cast<VirtualCardEnrollBubbleControllerImpl*>(
            VirtualCardEnrollBubbleControllerImpl::GetOrCreate(web_contents))) {
    virtual_card_enrollment_fields_.credit_card = test::GetFullServerCard();
    virtual_card_enrollment_fields_.card_art_image = &card_art_image_;
    virtual_card_enrollment_fields_.google_legal_message = {
        TestLegalMessageLine("google_test_legal_message")};
    virtual_card_enrollment_fields_.issuer_legal_message = {
        TestLegalMessageLine("issuer_test_legal_message")};
    virtual_card_enrollment_fields_.virtual_card_enrollment_source =
        VirtualCardEnrollmentSource::kUpstream;
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

class VirtualCardEnrollBubbleControllerImplBottomSheetTest
    : public ChromeRenderViewHostTestHarness {
 public:
  VirtualCardEnrollBubbleControllerImplBottomSheetTest() {
    features_.InitAndEnableFeature(
        features::kAutofillEnablePaymentsAndroidBottomSheet);
  }

 private:
  base::test::ScopedFeatureList features_;
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
  EXPECT_EQ(test_support.controller()->GetUiModel(), expected_model);
}

TEST_F(VirtualCardEnrollBubbleControllerImplBottomSheetTest, ShowBubble) {
  ControllerTestSupport test_support(web_contents());

  test_support.controller()->ShowBubble(
      test_support.virtual_card_enrollment_fields(),
      /*accept_virtual_card_callback=*/base::DoNothing(),
      /*decline_virtual_card_callback=*/base::DoNothing());

  EXPECT_TRUE(test_api(test_support.controller()).DidShowBottomSheet());
}

class VirtualCardEnrollBubbleControllerImplInfoBarTest
    : public ChromeRenderViewHostTestHarness {
 public:
  VirtualCardEnrollBubbleControllerImplInfoBarTest() {
    features_.InitAndDisableFeature(
        features::kAutofillEnablePaymentsAndroidBottomSheet);
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(VirtualCardEnrollBubbleControllerImplInfoBarTest, ShowBubble) {
  infobars::ContentInfoBarManager::CreateForWebContents(web_contents());
  ControllerTestSupport test_support(web_contents());

  test_support.controller()->ShowBubble(
      test_support.virtual_card_enrollment_fields(),
      /*accept_virtual_card_callback=*/base::DoNothing(),
      /*decline_virtual_card_callback=*/base::DoNothing());

  EXPECT_FALSE(test_api(test_support.controller()).DidShowBottomSheet());
}

}  // namespace
}  // namespace autofill
