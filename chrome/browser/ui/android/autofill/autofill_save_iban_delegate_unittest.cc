// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_save_iban_delegate.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/test/mock_callback.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/browser_ui/device_lock/android/test_device_lock_bridge.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"

namespace autofill {
namespace {

const std::u16string kUserProvidedNickname = u"My doctor's IBAN";

using SaveIbanOfferUserDecision =
    payments::PaymentsAutofillClient::SaveIbanOfferUserDecision;
using LocalCallbackArgs = std::pair<SaveIbanOfferUserDecision, std::u16string>;

class AutofillSaveIbanDelegateTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    delegate_ = std::make_unique<AutofillSaveIbanDelegate>(MakeLocalCallback(),
                                                           web_contents());
    std::unique_ptr<TestDeviceLockBridge> bridge =
        std::make_unique<TestDeviceLockBridge>();
    test_bridge_ = bridge.get();
    delegate_->SetDeviceLockBridgeForTesting(std::move(bridge));

    // Create a scoped window so that
    // WebContents::GetNativeView()->GetWindowAndroid() does not return null.
    window_ = ui::WindowAndroid::CreateForTesting();
    window_.get()->get()->AddChild(web_contents()->GetNativeView());
  }

  void LocalCallback(SaveIbanOfferUserDecision decision,
                     std::u16string_view nickname);

  payments::PaymentsAutofillClient::SaveIbanPromptCallback MakeLocalCallback();

  std::optional<LocalCallbackArgs> local_offer_decision_;

  std::unique_ptr<AutofillSaveIbanDelegate> delegate_;
  raw_ptr<TestDeviceLockBridge> test_bridge_;
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window_;
};

void AutofillSaveIbanDelegateTest::LocalCallback(
    SaveIbanOfferUserDecision decision,
    std::u16string_view nickname) {
  local_offer_decision_.emplace(decision, nickname);
}

payments::PaymentsAutofillClient::SaveIbanPromptCallback
AutofillSaveIbanDelegateTest::MakeLocalCallback() {
  return base::BindOnce(
      &AutofillSaveIbanDelegateTest::LocalCallback,
      base::Unretained(this));  // Test function does not outlive test fixture.
}

// Matches the SaveIbanPromptCallback arguments to a LocalCallbackArgs.
testing::Matcher<LocalCallbackArgs> EqualToLocalCallbackArgs(
    SaveIbanOfferUserDecision decision,
    std::u16string user_provided_nickname) {
  return testing::AllOf(
      testing::Field(&LocalCallbackArgs::first, decision),
      testing::Field(&LocalCallbackArgs::second, user_provided_nickname));
}

TEST_F(AutofillSaveIbanDelegateTest,
       OnUiAcceptedWithCallbackArgumentRunsCallback) {
  auto delegate = AutofillSaveIbanDelegate(MakeLocalCallback(), web_contents());

  base::MockOnceClosure mock_finish_gathering_consent_callback;
  EXPECT_CALL(mock_finish_gathering_consent_callback, Run).Times(1);
  delegate.OnUiAccepted(mock_finish_gathering_consent_callback.Get());
}

TEST_F(AutofillSaveIbanDelegateTest, OnUiAcceptedRunsLocalCallback) {
  auto delegate = AutofillSaveIbanDelegate(MakeLocalCallback(), web_contents());

  base::MockOnceClosure mock_finish_gathering_consent_callback;
  delegate.OnUiAccepted(mock_finish_gathering_consent_callback.Get(),
                        kUserProvidedNickname);

  EXPECT_THAT(local_offer_decision_,
              testing::Optional(EqualToLocalCallbackArgs(
                  SaveIbanOfferUserDecision::kAccepted,
                  /*user_provided_nickname=*/kUserProvidedNickname)));
}

TEST_F(AutofillSaveIbanDelegateTest, OnUiCanceledRunsLocalCallback) {
  auto delegate = AutofillSaveIbanDelegate(MakeLocalCallback(), web_contents());

  delegate.OnUiCanceled();

  EXPECT_THAT(local_offer_decision_, testing::Optional(EqualToLocalCallbackArgs(
                                         SaveIbanOfferUserDecision::kDeclined,
                                         /*user_provided_nickname=*/u"")));
}

TEST_F(AutofillSaveIbanDelegateTest, OnUiIgnoredRunsLocalCallback) {
  auto delegate = AutofillSaveIbanDelegate(MakeLocalCallback(), web_contents());

  delegate.OnUiIgnored();

  EXPECT_THAT(local_offer_decision_, testing::Optional(EqualToLocalCallbackArgs(
                                         SaveIbanOfferUserDecision::kIgnored,
                                         /*user_provided_nickname=*/u"")));
}

// Tests that IBAN is saved if device lock requirements are met (example: device
// lock is set).
TEST_F(AutofillSaveIbanDelegateTest, DeviceLockRequirementsMet) {
  base::MockOnceClosure mock_finish_gathering_consent_callback;
  delegate_->OnUiAccepted(mock_finish_gathering_consent_callback.Get(),
                          kUserProvidedNickname);

  // Verify that device lock requirements check was started but that IBAN is not
  // saved yet.
  EXPECT_TRUE(test_bridge_->did_start_checking_device_lock_requirements());
  EXPECT_FALSE(local_offer_decision_.has_value());

  test_bridge_->SimulateFinishedCheckingDeviceLockRequirements(
      /*are_device_lock_requirements_met=*/true);
  EXPECT_THAT(local_offer_decision_,
              testing::Optional(EqualToLocalCallbackArgs(
                  SaveIbanOfferUserDecision::kAccepted,
                  /*user_provided_nickname=*/kUserProvidedNickname)));
}

// Tests that IBAN is not saved if device lock UI is shown but device lock is
// not set.
TEST_F(AutofillSaveIbanDelegateTest, DeviceLockRequirementsNotMet) {
  base::MockOnceClosure mock_finish_gathering_consent_callback;
  delegate_->OnUiAccepted(mock_finish_gathering_consent_callback.Get(), u"");

  // Verify that device lock requirements check was started but that IBAN is not
  // saved yet.
  EXPECT_TRUE(test_bridge_->did_start_checking_device_lock_requirements());
  EXPECT_FALSE(local_offer_decision_.has_value());

  test_bridge_->SimulateFinishedCheckingDeviceLockRequirements(
      /*are_device_lock_requirements_met=*/false);
  EXPECT_THAT(local_offer_decision_, testing::Optional(EqualToLocalCallbackArgs(
                                         SaveIbanOfferUserDecision::kDeclined,
                                         /*user_provided_nickname=*/u"")));
}

}  // namespace
}  // namespace autofill
