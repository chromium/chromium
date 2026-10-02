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

// Tests that accepting the bottom sheet runs the on_accepted callback.
TEST_F(EmailVerificationBottomSheetBridgeTest, OnUiAccepted) {
  base::MockCallback<base::OnceClosure> on_accepted;
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      on_dismissed;
  EXPECT_CALL(on_accepted, Run);
  EXPECT_CALL(on_dismissed, Run).Times(0);

  bridge_->RequestShowContent(u"google.com", u"user@example.com",
                              on_accepted.Get(), on_dismissed.Get());
  bridge_->OnUiAccepted(/*env=*/nullptr);

  // Accepting the prompt does not dismiss it.
  testing::Mock::VerifyAndClearExpectations(&on_dismissed);

  // When destroyed while still loading, on_dismissed is resolved with
  // kViewDestroyedDirectly.
  EXPECT_CALL(on_dismissed,
              Run(AutofillClient::EmailVerificationPermissionUiStatus::
                      kViewDestroyedDirectly));
  bridge_.reset();
}

// Tests that declining the bottom sheet forwards kDeclined to on_dismissed.
TEST_F(EmailVerificationBottomSheetBridgeTest, OnUiDismissedDeclined) {
  base::MockCallback<base::OnceClosure> on_accepted;
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      on_dismissed;
  EXPECT_CALL(on_accepted, Run).Times(0);
  EXPECT_CALL(
      on_dismissed,
      Run(AutofillClient::EmailVerificationPermissionUiStatus::kDeclined));

  bridge_->RequestShowContent(u"google.com", u"user@example.com",
                              on_accepted.Get(), on_dismissed.Get());
  bridge_->OnUiDismissed(
      /*env=*/nullptr,
      static_cast<int>(
          AutofillClient::EmailVerificationPermissionUiStatus::kDeclined));
}

// Tests that user dismissal (e.g. back press or swipe) forwards kUserAborted.
TEST_F(EmailVerificationBottomSheetBridgeTest, OnUiDismissedUserAborted) {
  base::MockCallback<base::OnceClosure> on_accepted;
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      on_dismissed;
  EXPECT_CALL(on_accepted, Run).Times(0);
  EXPECT_CALL(
      on_dismissed,
      Run(AutofillClient::EmailVerificationPermissionUiStatus::kUserAborted));

  bridge_->RequestShowContent(u"google.com", u"user@example.com",
                              on_accepted.Get(), on_dismissed.Get());
  bridge_->OnUiDismissed(
      /*env=*/nullptr,
      static_cast<int>(
          AutofillClient::EmailVerificationPermissionUiStatus::kUserAborted));
}

// Tests that tab destruction or tab switching forwards kTabGone.
TEST_F(EmailVerificationBottomSheetBridgeTest, OnUiDismissedTabGone) {
  base::MockCallback<base::OnceClosure> on_accepted;
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      on_dismissed;
  EXPECT_CALL(on_accepted, Run).Times(0);
  EXPECT_CALL(
      on_dismissed,
      Run(AutofillClient::EmailVerificationPermissionUiStatus::kTabGone));

  bridge_->RequestShowContent(u"google.com", u"user@example.com",
                              on_accepted.Get(), on_dismissed.Get());
  bridge_->OnUiDismissed(
      /*env=*/nullptr,
      static_cast<int>(
          AutofillClient::EmailVerificationPermissionUiStatus::kTabGone));
}

// Tests that fallback/unknown dismissal status forwards kOther.
TEST_F(EmailVerificationBottomSheetBridgeTest, OnUiDismissedOther) {
  base::MockCallback<base::OnceClosure> on_accepted;
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      on_dismissed;
  EXPECT_CALL(on_accepted, Run).Times(0);
  EXPECT_CALL(on_dismissed,
              Run(AutofillClient::EmailVerificationPermissionUiStatus::kOther));

  bridge_->RequestShowContent(u"google.com", u"user@example.com",
                              on_accepted.Get(), on_dismissed.Get());
  bridge_->OnUiDismissed(
      /*env=*/nullptr,
      static_cast<int>(
          AutofillClient::EmailVerificationPermissionUiStatus::kOther));
}

// Tests that programmatically hiding the sheet resolves on_dismissed with
// kOther if not yet accepted.
TEST_F(EmailVerificationBottomSheetBridgeTest, HideBeforeAccepted) {
  base::MockCallback<base::OnceClosure> on_accepted;
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      on_dismissed;
  EXPECT_CALL(on_accepted, Run).Times(0);
  EXPECT_CALL(on_dismissed,
              Run(AutofillClient::EmailVerificationPermissionUiStatus::kOther));

  bridge_->RequestShowContent(u"google.com", u"user@example.com",
                              on_accepted.Get(), on_dismissed.Get());
  bridge_->Hide();
}

// Tests that programmatically hiding the sheet resolves on_dismissed with
// kAllowed if already accepted (e.g. interaction complete).
TEST_F(EmailVerificationBottomSheetBridgeTest, HideAfterAccepted) {
  base::MockCallback<base::OnceClosure> on_accepted;
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      on_dismissed;
  EXPECT_CALL(on_accepted, Run);
  EXPECT_CALL(
      on_dismissed,
      Run(AutofillClient::EmailVerificationPermissionUiStatus::kAllowed));

  bridge_->RequestShowContent(u"google.com", u"user@example.com",
                              on_accepted.Get(), on_dismissed.Get());
  bridge_->OnUiAccepted(/*env=*/nullptr);
  bridge_->Hide();
}

// Tests that destroying the bridge while showing resolves callback with
// kViewDestroyedDirectly.
TEST_F(EmailVerificationBottomSheetBridgeTest,
       DestroyWhileShowingInvokesCallback) {
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      on_dismissed;
  EXPECT_CALL(on_dismissed,
              Run(AutofillClient::EmailVerificationPermissionUiStatus::
                      kViewDestroyedDirectly));

  bridge_->RequestShowContent(u"google.com", u"user@example.com",
                              base::DoNothing(), on_dismissed.Get());
  bridge_.reset();
}

// Tests that requesting a new prompt while already showing rejects the new
// request with kOverlappingPrompt and leaves the existing callbacks active.
TEST_F(EmailVerificationBottomSheetBridgeTest,
       ReentrantRequestShowContentRejectsNewCallback) {
  base::MockCallback<base::OnceClosure> on_accepted1;
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      on_dismissed1;
  base::MockCallback<base::OnceClosure> on_accepted2;
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      on_dismissed2;

  EXPECT_CALL(on_accepted1, Run);
  EXPECT_CALL(on_dismissed1, Run).Times(0);
  EXPECT_CALL(on_dismissed2,
              Run(AutofillClient::EmailVerificationPermissionUiStatus::
                      kOverlappingPrompt));

  bridge_->RequestShowContent(u"google.com", u"user1@example.com",
                              on_accepted1.Get(), on_dismissed1.Get());
  bridge_->RequestShowContent(u"google.com", u"user2@example.com",
                              on_accepted2.Get(), on_dismissed2.Get());
  bridge_->OnUiAccepted(/*env=*/nullptr);

  testing::Mock::VerifyAndClearExpectations(&on_dismissed1);
  EXPECT_CALL(on_dismissed1,
              Run(AutofillClient::EmailVerificationPermissionUiStatus::
                      kViewDestroyedDirectly));
  bridge_.reset();
}

// Tests that requesting a new prompt while the sheet is in the loading state
// (after OnUiAccepted) rejects the new request with kOverlappingPrompt.
TEST_F(EmailVerificationBottomSheetBridgeTest,
       RequestShowContentWhileLoadingRejectsNewCallback) {
  base::MockCallback<base::OnceClosure> on_accepted1;
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      on_dismissed1;
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      on_dismissed2;

  EXPECT_CALL(on_accepted1, Run);
  EXPECT_CALL(on_dismissed1, Run).Times(0);
  EXPECT_CALL(on_dismissed2,
              Run(AutofillClient::EmailVerificationPermissionUiStatus::
                      kOverlappingPrompt));

  bridge_->RequestShowContent(u"google.com", u"user1@example.com",
                              on_accepted1.Get(), on_dismissed1.Get());
  bridge_->OnUiAccepted(/*env=*/nullptr);

  // Requesting again while loading must reject callback2 with
  // kOverlappingPrompt.
  bridge_->RequestShowContent(u"google.com", u"user2@example.com",
                              base::DoNothing(), on_dismissed2.Get());

  testing::Mock::VerifyAndClearExpectations(&on_dismissed1);
  EXPECT_CALL(on_dismissed1,
              Run(AutofillClient::EmailVerificationPermissionUiStatus::
                      kViewDestroyedDirectly));
  bridge_.reset();
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

// Tests that OnUiDismissed invokes the dismissal callback passed to
// RequestShowContent when the sheet was accepted and remained open in loading
// state.
TEST_F(EmailVerificationBottomSheetBridgeTest, OnUiDismissedAfterAccepted) {
  base::MockCallback<base::OnceClosure> on_accepted;
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      on_dismissed;
  EXPECT_CALL(on_accepted, Run);
  EXPECT_CALL(
      on_dismissed,
      Run(AutofillClient::EmailVerificationPermissionUiStatus::kUserAborted));

  bridge_->RequestShowContent(u"google.com", u"user@example.com",
                              on_accepted.Get(), on_dismissed.Get());
  bridge_->OnUiAccepted(/*env=*/nullptr);
  bridge_->OnUiDismissed(
      /*env=*/nullptr,
      static_cast<int>(
          AutofillClient::EmailVerificationPermissionUiStatus::kUserAborted));
}

}  // namespace autofill
