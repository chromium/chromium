// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockVirtualCardEnrollBubbleController
    : public VirtualCardEnrollBubbleControllerImpl {
 public:
  explicit MockVirtualCardEnrollBubbleController(
      content::WebContents* web_contents)
      : VirtualCardEnrollBubbleControllerImpl(web_contents) {}
  ~MockVirtualCardEnrollBubbleController() override = default;

#if !BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(void, ShowConfirmationBubbleView, (bool), (override));
#endif
  MOCK_METHOD(bool, IsIconVisible, (), (const override));
};

class ChromePaymentsAutofillClientTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromePaymentsAutofillClientTest() {
    feature_list_.InitAndEnableFeature(
        features::kAutofillEnableVcnEnrollLoadingAndConfirmation);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    ChromeAutofillClient::CreateForWebContents(web_contents());
    auto mock_virtual_card_bubble_controller =
        std::make_unique<MockVirtualCardEnrollBubbleController>(web_contents());
    web_contents()->SetUserData(
        mock_virtual_card_bubble_controller->UserDataKey(),
        std::move(mock_virtual_card_bubble_controller));
  }

  payments::ChromePaymentsAutofillClient* chrome_payments_client() {
    return static_cast<payments::ChromePaymentsAutofillClient*>(
        client()->GetPaymentsAutofillClient());
  }

  MockVirtualCardEnrollBubbleController& virtual_card_bubble_controller() {
    return static_cast<MockVirtualCardEnrollBubbleController&>(
        *VirtualCardEnrollBubbleController::GetOrCreate(web_contents()));
  }

  ChromeAutofillClient* client() {
    return ChromeAutofillClient::FromWebContentsForTesting(web_contents());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

#if !BUILDFLAG(IS_ANDROID)
// Verify that confirmation bubble view is shown after virtual card enrollment
// is completed.
TEST_F(ChromePaymentsAutofillClientTest,
       VirtualCardEnrollCompleted_ShowsConfirmation) {
  ON_CALL(virtual_card_bubble_controller(), IsIconVisible())
      .WillByDefault(testing::Return(true));
  EXPECT_CALL(virtual_card_bubble_controller(),
              ShowConfirmationBubbleView(true));
  chrome_payments_client()->VirtualCardEnrollCompleted(true);
}
#endif

// Test that there is always an PaymentsWindowManager present if attempted
// to be retrieved.
TEST_F(ChromePaymentsAutofillClientTest, GetPaymentsWindowManager) {
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    EXPECT_EQ(chrome_payments_client()->GetPaymentsWindowManager(), nullptr);
  } else {
    EXPECT_NE(chrome_payments_client()->GetPaymentsWindowManager(), nullptr);
  }
}

}  // namespace autofill
