// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_save_card_delegate_android.h"

#include "autofill_save_card_delegate_android.h"
#include "base/logging.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/browser_ui/device_lock/android/device_lock_bridge.h"
#include "components/browser_ui/device_lock/android/test_device_lock_bridge.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "ui/android/window_android.h"

namespace autofill {

using ::testing::ElementsAre;

class AutofillSaveCardDelegateAndroidTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    delegate_ = std::make_unique<AutofillSaveCardDelegateAndroid>(
        CreateSaveCardCallback(),
        payments::PaymentsAutofillClient::SaveCreditCardOptions(),
        web_contents());
    auto bridge = std::make_unique<TestDeviceLockBridge>();
    test_bridge_ = bridge.get();
    delegate_->SetDeviceLockBridgeForTesting(std::move(bridge));

    // Create a scoped window so that
    // WebContents::GetNativeView()->GetWindowAndroid() does not return null.
    window_ = ui::WindowAndroid::CreateForTesting();
    window_.get()->get()->AddChild(web_contents()->GetNativeView());
  }

  std::unique_ptr<AutofillSaveCardDelegateAndroid> delegate_;
  raw_ptr<TestDeviceLockBridge> test_bridge_;
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window_;
  std::vector<payments::PaymentsAutofillClient::SaveCardOfferUserDecision>
      save_card_decisions_;

 private:
  void SaveCardCallback(
      payments::PaymentsAutofillClient::SaveCardOfferUserDecision decision) {
    save_card_decisions_.push_back(decision);
  }

  payments::PaymentsAutofillClient::LocalSaveCardPromptCallback
  CreateSaveCardCallback() {
    return base::BindOnce(
        &AutofillSaveCardDelegateAndroidTest::SaveCardCallback,
        base::Unretained(
            this));  // Test function does not outlive test fixture.
  }
};

// Tests that card is saved if device lock requirements are met.
TEST_F(AutofillSaveCardDelegateAndroidTest, DeviceLockRequirementsMet) {
  EXPECT_TRUE(save_card_decisions_.empty());
  delegate_->OnUiAccepted();

  // Simulate finishing checking the device lock requirements and the device
  // lock requirements are met (example: device lock is required and user set a
  // device lock).
  test_bridge_->SimulateFinishedCheckingDeviceLockRequirements(
      /*are_device_lock_requirements_met=*/true);

  EXPECT_THAT(save_card_decisions_,
              ElementsAre(payments::PaymentsAutofillClient::
                              SaveCardOfferUserDecision::kAccepted));
}

// Tests that card is not saved if device lock requirements are not met.
TEST_F(AutofillSaveCardDelegateAndroidTest, DeviceLockRequirementsNotMet) {
  EXPECT_TRUE(save_card_decisions_.empty());
  delegate_->OnUiAccepted();

  // Simulate finishing checking the device lock requirements and the device
  // lock requirements are not met (example: device lock is required and user
  // didn't set a device lock).
  test_bridge_->SimulateFinishedCheckingDeviceLockRequirements(
      /*are_device_lock_requirements_met=*/false);
  EXPECT_THAT(save_card_decisions_,
              ElementsAre(payments::PaymentsAutofillClient::
                              SaveCardOfferUserDecision::kIgnored));
}

}  // namespace autofill
