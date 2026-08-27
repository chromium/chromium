// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/email_verification_bottom_sheet_bridge.h"

#include <memory>
#include <string>

#include "base/test/mock_callback.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"

namespace autofill {

class TestEmailVerificationBottomSheetBridge
    : public EmailVerificationBottomSheetBridge {
 public:
  TestEmailVerificationBottomSheetBridge()
      : EmailVerificationBottomSheetBridge(
            base::android::ScopedJavaGlobalRef<jobject>()) {}
};

class EmailVerificationBottomSheetBridgeTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    bridge_ = std::make_unique<TestEmailVerificationBottomSheetBridge>();
  }

  std::unique_ptr<TestEmailVerificationBottomSheetBridge> bridge_;
};

// Tests that accepting the bottom sheet forwards kAllowed to the callback.
TEST_F(EmailVerificationBottomSheetBridgeTest, OnUiDecisionAllowed) {
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      callback;
  EXPECT_CALL(
      callback,
      Run(AutofillClient::EmailVerificationPermissionUiStatus::kAllowed));

  bridge_->RequestShowContent(u"google.com", u"user@example.com",
                              callback.Get());
  bridge_->OnUiDecision(
      /*env=*/nullptr,
      static_cast<int>(
          AutofillClient::EmailVerificationPermissionUiStatus::kAllowed));
}

// Tests that declining the bottom sheet forwards kDeclined to the callback.
TEST_F(EmailVerificationBottomSheetBridgeTest, OnUiDecisionDeclined) {
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      callback;
  EXPECT_CALL(
      callback,
      Run(AutofillClient::EmailVerificationPermissionUiStatus::kDeclined));

  bridge_->RequestShowContent(u"google.com", u"user@example.com",
                              callback.Get());
  bridge_->OnUiDecision(
      /*env=*/nullptr,
      static_cast<int>(
          AutofillClient::EmailVerificationPermissionUiStatus::kDeclined));
}

// Tests that user dismissal (e.g. back press or swipe) forwards kUserAborted.
TEST_F(EmailVerificationBottomSheetBridgeTest, OnUiDecisionUserAborted) {
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      callback;
  EXPECT_CALL(
      callback,
      Run(AutofillClient::EmailVerificationPermissionUiStatus::kUserAborted));

  bridge_->RequestShowContent(u"google.com", u"user@example.com",
                              callback.Get());
  bridge_->OnUiDecision(
      /*env=*/nullptr,
      static_cast<int>(
          AutofillClient::EmailVerificationPermissionUiStatus::kUserAborted));
}

// Tests that tab destruction or tab switching forwards kTabGone to the
// callback.
TEST_F(EmailVerificationBottomSheetBridgeTest, OnUiDecisionTabGone) {
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      callback;
  EXPECT_CALL(
      callback,
      Run(AutofillClient::EmailVerificationPermissionUiStatus::kTabGone));

  bridge_->RequestShowContent(u"google.com", u"user@example.com",
                              callback.Get());
  bridge_->OnUiDecision(
      /*env=*/nullptr,
      static_cast<int>(
          AutofillClient::EmailVerificationPermissionUiStatus::kTabGone));
}

// Tests that fallback/unknown UI decision status forwards kOther to the
// callback.
TEST_F(EmailVerificationBottomSheetBridgeTest, OnUiDecisionOther) {
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      callback;
  EXPECT_CALL(callback,
              Run(AutofillClient::EmailVerificationPermissionUiStatus::kOther));

  bridge_->RequestShowContent(u"google.com", u"user@example.com",
                              callback.Get());
  bridge_->OnUiDecision(
      /*env=*/nullptr,
      static_cast<int>(
          AutofillClient::EmailVerificationPermissionUiStatus::kOther));
}

// Tests that programmatically hiding the sheet resolves the callback with
// kUserAborted.
TEST_F(EmailVerificationBottomSheetBridgeTest, Hide) {
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      callback;
  EXPECT_CALL(
      callback,
      Run(AutofillClient::EmailVerificationPermissionUiStatus::kUserAborted));

  bridge_->RequestShowContent(u"google.com", u"user@example.com",
                              callback.Get());
  bridge_->Hide();
}

// Tests that destroying the bridge while showing resolves callback with
// kViewDestroyedDirectly.
TEST_F(EmailVerificationBottomSheetBridgeTest,
       DestroyWhileShowingInvokesCallback) {
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      callback;
  EXPECT_CALL(callback,
              Run(AutofillClient::EmailVerificationPermissionUiStatus::
                      kViewDestroyedDirectly));

  bridge_->RequestShowContent(u"google.com", u"user@example.com",
                              callback.Get());
  bridge_.reset();
}

// Tests that requesting a new prompt while showing cancels previous callback
// with kOverlappingPrompt.
TEST_F(EmailVerificationBottomSheetBridgeTest,
       ReentrantRequestShowContentResolvesPreviousCallback) {
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      callback1;
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      callback2;

  EXPECT_CALL(callback1,
              Run(AutofillClient::EmailVerificationPermissionUiStatus::
                      kOverlappingPrompt));
  EXPECT_CALL(
      callback2,
      Run(AutofillClient::EmailVerificationPermissionUiStatus::kAllowed));

  bridge_->RequestShowContent(u"google.com", u"user1@example.com",
                              callback1.Get());
  bridge_->RequestShowContent(u"google.com", u"user2@example.com",
                              callback2.Get());
  bridge_->OnUiDecision(
      /*env=*/nullptr,
      static_cast<int>(
          AutofillClient::EmailVerificationPermissionUiStatus::kAllowed));
}

// Tests that instantiating the bridge with a WindowAndroid and TabModel
// succeeds.
TEST_F(EmailVerificationBottomSheetBridgeTest, ConstructorWithWindow) {
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window->get()->AddChild(web_contents()->GetNativeView());
  auto tab_model = std::make_unique<TestTabModel>(profile());
  auto bridge = std::make_unique<EmailVerificationBottomSheetBridge>(
      window->get(), tab_model.get());
  EXPECT_NE(bridge, nullptr);
}

}  // namespace autofill
