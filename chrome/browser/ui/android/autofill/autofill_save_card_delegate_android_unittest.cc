// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_save_card_delegate_android.h"

#include "autofill_save_card_delegate_android.h"
#include "base/logging.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/browser_ui/device_lock/android/device_lock_bridge.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "ui/android/window_android.h"

namespace {

class TestDeviceLockBridge : public DeviceLockBridge {
 public:
  TestDeviceLockBridge() = default;
  TestDeviceLockBridge(const TestDeviceLockBridge&) = delete;
  TestDeviceLockBridge& operator=(const TestDeviceLockBridge&) = delete;

  bool ShouldShowDeviceLockUi() override { return should_show_device_lock_ui_; }

  bool RequiresDeviceLock() override { return should_show_device_lock_ui_; }

  void LaunchDeviceLockUiBeforeRunningCallback(
      ui::WindowAndroid* window_android,
      DeviceLockConfirmedCallback callback) override {
    callback_ = std::move(callback);
    device_lock_ui_shown_count_++;
  }

  void SimulateDeviceLockComplete(bool is_device_lock_set) {
    std::move(callback_).Run(is_device_lock_set);
  }

  void SetShouldShowDeviceLockUi(bool should_show_device_lock_ui) {
    should_show_device_lock_ui_ = should_show_device_lock_ui;
  }

  int device_lock_ui_shown_count() { return device_lock_ui_shown_count_; }

 private:
  bool should_show_device_lock_ui_ = false;
  int device_lock_ui_shown_count_ = 0;
  DeviceLockConfirmedCallback callback_;
};

}  // namespace

namespace autofill {

class AutofillSaveCardDelegateAndroidTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    delegate_ = std::make_unique<AutofillSaveCardDelegateAndroid>(
        CreateSaveCardCallback(), AutofillClient::SaveCreditCardOptions(),
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
  std::vector<AutofillClient::SaveCardOfferUserDecision> save_card_decisions_;

 private:
  void SaveCardCallback(AutofillClient::SaveCardOfferUserDecision decision) {
    save_card_decisions_.push_back(decision);
  }

  AutofillClient::LocalSaveCardPromptCallback CreateSaveCardCallback() {
    return base::BindOnce(
        &AutofillSaveCardDelegateAndroidTest::SaveCardCallback,
        base::Unretained(
            this));  // Test function does not outlive test fixture.
  }
};

// Tests that card is saved if device lock UI is shown and device lock is set.
TEST_F(AutofillSaveCardDelegateAndroidTest, DeviceLockUiShown_DeviceLockSet) {
  // Simulate user clicking save card button and not having previously set up a
  // device lock.
  test_bridge_->SetShouldShowDeviceLockUi(true);
  delegate_->OnUiAccepted();

  // Verify that device lock UI is shown but card is not saved yet.
  EXPECT_EQ(1, test_bridge_->device_lock_ui_shown_count());
  EXPECT_TRUE(save_card_decisions_.empty());

  // Verify that card is saved after user sets a device lock.
  test_bridge_->SimulateDeviceLockComplete(true);
  EXPECT_THAT(save_card_decisions_,
              testing::ElementsAre(
                  AutofillClient::SaveCardOfferUserDecision::kAccepted));
}

// Tests that card is not saved if device lock UI is shown but device lock is
// not set.
TEST_F(AutofillSaveCardDelegateAndroidTest,
       DeviceLockUiShown_DeviceLockNotSet) {
  // Simulate user clicking save card button and not having previously set up a
  // device lock.
  test_bridge_->SetShouldShowDeviceLockUi(true);
  delegate_->OnUiAccepted();

  // Verify that device lock UI is shown but card is not saved yet.
  EXPECT_EQ(1, test_bridge_->device_lock_ui_shown_count());
  EXPECT_TRUE(save_card_decisions_.empty());

  // Verify that card not saved because user didn't set a device lock.
  test_bridge_->SimulateDeviceLockComplete(false);
  EXPECT_THAT(save_card_decisions_,
              testing::ElementsAre(
                  AutofillClient::SaveCardOfferUserDecision::kIgnored));
}

// Tests that card is not saved if device lock UI is not shown because
// WindowAndroid is null.
TEST_F(AutofillSaveCardDelegateAndroidTest,
       DeviceLockUiNotShown_WindowAndroidIsNull) {
  // Set WindowAndroid to null.
  window_.reset(nullptr);

  // Simulate user clicking save card button and getting prompted to set a
  // device lock.
  test_bridge_->SetShouldShowDeviceLockUi(true);
  delegate_->OnUiAccepted();

  // Verify that device lock UI is not shown and card is not saved.
  EXPECT_EQ(0, test_bridge_->device_lock_ui_shown_count());
  EXPECT_THAT(save_card_decisions_,
              testing::ElementsAre(
                  AutofillClient::SaveCardOfferUserDecision::kIgnored));
}

// Tests that card is saved right away if device lock is already set.
TEST_F(AutofillSaveCardDelegateAndroidTest,
       DeviceLockUiNotShown_DeviceLockAlreadySet) {
  // Simulate user clicking save card button and having previously set up a
  // device lock.
  test_bridge_->SetShouldShowDeviceLockUi(false);
  delegate_->OnUiAccepted();

  // Verify that device lock UI is not shown and card is saved.
  EXPECT_EQ(0, test_bridge_->device_lock_ui_shown_count());
  EXPECT_THAT(save_card_decisions_,
              testing::ElementsAre(
                  AutofillClient::SaveCardOfferUserDecision::kAccepted));
}

}  // namespace autofill
