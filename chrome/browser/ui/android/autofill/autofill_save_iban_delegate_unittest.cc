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

}  // namespace

using SaveIbanOfferUserDecision = AutofillClient::SaveIbanOfferUserDecision;
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

  AutofillClient::SaveIbanPromptCallback MakeLocalCallback();

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

AutofillClient::SaveIbanPromptCallback
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

// Tests that IBAN is saved if device lock UI is shown and device lock is set.
TEST_F(AutofillSaveIbanDelegateTest, DeviceLockUiShown_DeviceLockSet) {
  // Simulate user clicking save IBAN button and not having previously set up a
  // device lock.
  test_bridge_->SetShouldShowDeviceLockUi(true);

  base::MockOnceClosure mock_finish_gathering_consent_callback;
  delegate_->OnUiAccepted(mock_finish_gathering_consent_callback.Get(),
                          kUserProvidedNickname);

  // Verify that device lock UI is shown but IBAN is not saved yet.
  EXPECT_EQ(true, test_bridge_->device_lock_ui_was_shown());
  EXPECT_FALSE(local_offer_decision_.has_value());

  // Verify that IBAN is saved after user sets a device lock.
  test_bridge_->SimulateDeviceLockComplete(true);
  EXPECT_THAT(local_offer_decision_,
              testing::Optional(EqualToLocalCallbackArgs(
                  SaveIbanOfferUserDecision::kAccepted,
                  /*user_provided_nickname=*/kUserProvidedNickname)));
}

// Tests that IBAN is not saved if device lock UI is shown but device lock is
// not set.
TEST_F(AutofillSaveIbanDelegateTest, DeviceLockUiShown_DeviceLockNotSet) {
  // Simulate user clicking save IBAN button and not having previously set up a
  // device lock.
  test_bridge_->SetShouldShowDeviceLockUi(true);

  base::MockOnceClosure mock_finish_gathering_consent_callback;
  delegate_->OnUiAccepted(mock_finish_gathering_consent_callback.Get(), u"");

  // Verify that device lock UI is shown but IBAN is not saved yet.
  EXPECT_EQ(true, test_bridge_->device_lock_ui_was_shown());
  EXPECT_FALSE(local_offer_decision_.has_value());

  // Verify that IBAN not saved because user didn't set a device lock.
  test_bridge_->SimulateDeviceLockComplete(false);
  EXPECT_THAT(local_offer_decision_, testing::Optional(EqualToLocalCallbackArgs(
                                         SaveIbanOfferUserDecision::kDeclined,
                                         /*user_provided_nickname=*/u"")));
}

// Tests that IBAN is not saved if device lock UI is not shown because
// WindowAndroid is null in the case of a non-empty nickname.
TEST_F(AutofillSaveIbanDelegateTest,
       DeviceLockUiNotShown_WindowAndroidIsNullNonEmptyNickname) {
  // Set WindowAndroid to null.
  window_.reset(nullptr);

  // Simulate user clicking save IBAN button and getting prompted to set a
  // device lock.
  test_bridge_->SetShouldShowDeviceLockUi(true);

  base::MockOnceClosure mock_finish_gathering_consent_callback;
  delegate_->OnUiAccepted(mock_finish_gathering_consent_callback.Get(),
                          kUserProvidedNickname);

  // Verify that device lock UI is not shown and IBAN is not saved.
  EXPECT_EQ(false, test_bridge_->device_lock_ui_was_shown());
  EXPECT_THAT(local_offer_decision_, testing::Optional(EqualToLocalCallbackArgs(
                                         SaveIbanOfferUserDecision::kIgnored,
                                         /*user_provided_nickname=*/u"")));
}

// Tests that IBAN is not saved if device lock UI is not shown because
// WindowAndroid is null in the case of an empty nickname.
TEST_F(AutofillSaveIbanDelegateTest,
       DeviceLockUiNotShown_WindowAndroidIsNullEmptyNickname) {
  // Set WindowAndroid to null.
  window_.reset(nullptr);

  // Simulate user clicking save IBAN button and getting prompted to set a
  // device lock.
  test_bridge_->SetShouldShowDeviceLockUi(true);

  base::MockOnceClosure mock_finish_gathering_consent_callback;
  delegate_->OnUiAccepted(mock_finish_gathering_consent_callback.Get(), u"");

  // Verify that device lock UI is not shown and IBAN is not saved.
  EXPECT_EQ(false, test_bridge_->device_lock_ui_was_shown());
  EXPECT_THAT(local_offer_decision_, testing::Optional(EqualToLocalCallbackArgs(
                                         SaveIbanOfferUserDecision::kIgnored,
                                         /*user_provided_nickname=*/u"")));
}

// Tests that IBAN is saved right away if device lock is already set.
TEST_F(AutofillSaveIbanDelegateTest,
       DeviceLockUiNotShown_DeviceLockAlreadySet) {
  // Simulate user clicking save IBAN button and having previously set up a
  // device lock.
  test_bridge_->SetShouldShowDeviceLockUi(false);

  base::MockOnceClosure mock_finish_gathering_consent_callback;
  delegate_->OnUiAccepted(mock_finish_gathering_consent_callback.Get(),
                          kUserProvidedNickname);

  // Verify that device lock UI is not shown and IBAN is saved.
  EXPECT_EQ(false, test_bridge_->device_lock_ui_was_shown());
  EXPECT_THAT(local_offer_decision_,
              testing::Optional(EqualToLocalCallbackArgs(
                  SaveIbanOfferUserDecision::kAccepted,
                  /*user_provided_nickname=*/kUserProvidedNickname)));
}

}  // namespace autofill
