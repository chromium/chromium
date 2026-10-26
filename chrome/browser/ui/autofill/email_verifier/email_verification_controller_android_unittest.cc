// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/email_verifier/email_verification_controller_android.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/android/autofill/email_verification_bottom_sheet_bridge.h"
#include "chrome/browser/ui/autofill/autofill_message_model.h"
#include "chrome/browser/ui/autofill/autofill_message_model_test_api.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verification_controller_test_api.h"
#include "chrome/browser/ui/autofill/mock_autofill_message_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/messages/android/message_enums.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {
namespace {

using CompletionStatus = EmailVerificationController::CompletionStatus;

class MockEmailVerificationBottomSheetBridge
    : public EmailVerificationBottomSheetBridge {
 public:
  explicit MockEmailVerificationBottomSheetBridge(
      content::WebContents* web_contents)
      : EmailVerificationBottomSheetBridge(web_contents) {
    ON_CALL(*this, HideJavaBottomSheet).WillByDefault([this]() {
      OnUiDismissed(
          /*env=*/nullptr,
          static_cast<int>(
              AutofillClient::EmailVerificationPermissionUiStatus::kOther));
    });
  }
  ~MockEmailVerificationBottomSheetBridge() override = default;

  MOCK_METHOD(void,
              ShowJavaBottomSheet,
              (const std::u16string& title, const std::u16string& description),
              (override));
  MOCK_METHOD(void, HideJavaBottomSheet, (), (override));
};

class EmailVerificationControllerAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  EmailVerificationControllerAndroidTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    window_ = ui::WindowAndroid::CreateForTesting();
    // Attach the native view to `window_` so
    // `EmailVerificationBottomSheetBridge::RequestShowContent()` sees a
    // top-level native window.
    window_->get()->AddChild(web_contents()->GetNativeView());
    evp_controller_ =
        std::make_unique<EmailVerificationControllerAndroid>(web_contents());
    test_api(*evp_controller_)
        .set_message_controller(&mock_message_controller_);
    test_api(*evp_controller_)
        .set_open_settings_callback(open_settings_callback_.Get());
    auto mock_bridge = std::make_unique<MockEmailVerificationBottomSheetBridge>(
        web_contents());
    bridge_ = mock_bridge.get();
    test_api(*evp_controller_).set_bottom_sheet_bridge(std::move(mock_bridge));
  }

  void TearDown() override {
    bridge_ = nullptr;
    test_api(*evp_controller_).set_message_controller(nullptr);
    evp_controller_.reset();
    window_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  void ShowPopup(base::OnceCallback<
                     void(AutofillClient::EmailVerificationPermissionUiStatus)>
                     ui_callback = base::DoNothing()) {
    EXPECT_CALL(*bridge_, ShowJavaBottomSheet);
    evp_controller_->ShowPopup(gfx::RectF(0, 0, 10, 10),
                               url::Origin::Create(GURL("https://google.com")),
                               u"user@example.com", std::move(ui_callback));
  }

  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window_;
  MockAutofillMessageController mock_message_controller_;
  base::MockCallback<base::RepeatingClosure> open_settings_callback_;
  std::unique_ptr<EmailVerificationControllerAndroid> evp_controller_;
  raw_ptr<MockEmailVerificationBottomSheetBridge> bridge_ = nullptr;
};

TEST_F(EmailVerificationControllerAndroidTest,
       BottomSheetMinimumLoadingDurationEnforced) {
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      ui_callback;
  ShowPopup(ui_callback.Get());

  EXPECT_CALL(
      ui_callback,
      Run(AutofillClient::EmailVerificationPermissionUiStatus::kAllowed));
  bridge_->OnUiAccepted(/*env=*/nullptr);
  EXPECT_TRUE(test_api(*evp_controller_).loading_start_time().has_value());

  // Calling HidePopup at 200ms (< 800ms) defers hiding.
  task_environment()->FastForwardBy(base::Milliseconds(200));
  EXPECT_CALL(*bridge_, HideJavaBottomSheet).Times(0);
  evp_controller_->HidePopup();
  EXPECT_TRUE(test_api(*evp_controller_).is_hide_popup_timer_running());

  // After the remaining 600ms, the bottom sheet is hidden.
  EXPECT_CALL(*bridge_, HideJavaBottomSheet);
  task_environment()->FastForwardBy(base::Milliseconds(600));
  EXPECT_FALSE(test_api(*evp_controller_).is_hide_popup_timer_running());
  EXPECT_FALSE(test_api(*evp_controller_).loading_start_time().has_value());
}

TEST_F(EmailVerificationControllerAndroidTest,
       HidePopupWhileShowingDismissesImmediately) {
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      ui_callback;
  ShowPopup(ui_callback.Get());

  // Calling HidePopup before the user accepts the prompt immediately hides the
  // bottom sheet without starting hide_popup_timer_.
  EXPECT_CALL(*bridge_, HideJavaBottomSheet);
  EXPECT_CALL(ui_callback,
              Run(AutofillClient::EmailVerificationPermissionUiStatus::kOther));
  evp_controller_->HidePopup();
  EXPECT_FALSE(test_api(*evp_controller_).is_hide_popup_timer_running());
  EXPECT_FALSE(test_api(*evp_controller_).loading_start_time().has_value());
}

TEST_F(EmailVerificationControllerAndroidTest,
       ShowLoadingToastWhileShowingDismissesPrompt) {
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      ui_callback;
  ShowPopup(ui_callback.Get());

  // Calling ShowLoadingToast while the first-run prompt is still awaiting a
  // decision dismisses the open bottom sheet.
  EXPECT_CALL(*bridge_, HideJavaBottomSheet);
  EXPECT_CALL(ui_callback,
              Run(AutofillClient::EmailVerificationPermissionUiStatus::kOther));
  evp_controller_->ShowLoadingToast(base::DoNothing());
}

TEST_F(EmailVerificationControllerAndroidTest,
       DeclinedPromptDoesNotStartLoadingTimer) {
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      ui_callback;
  ShowPopup(ui_callback.Get());

  EXPECT_CALL(
      ui_callback,
      Run(AutofillClient::EmailVerificationPermissionUiStatus::kDeclined));
  bridge_->OnUiDismissed(
      /*env=*/nullptr,
      static_cast<int>(
          AutofillClient::EmailVerificationPermissionUiStatus::kDeclined));
  EXPECT_FALSE(test_api(*evp_controller_).loading_start_time().has_value());

  // Subsequent HidePopup is a no-op since the prompt is already closed.
  EXPECT_CALL(*bridge_, HideJavaBottomSheet).Times(0);
  evp_controller_->HidePopup();
  EXPECT_FALSE(test_api(*evp_controller_).is_hide_popup_timer_running());
}

TEST_F(EmailVerificationControllerAndroidTest,
       BottomSheetDismissedEarlyClearsLoadingTimer) {
  ShowPopup();

  bridge_->OnUiAccepted(/*env=*/nullptr);
  EXPECT_TRUE(test_api(*evp_controller_).loading_start_time().has_value());

  // User swipes away the bottom sheet at 100ms while loading.
  task_environment()->FastForwardBy(base::Milliseconds(100));
  bridge_->OnUiDismissed(
      /*env=*/nullptr,
      static_cast<int>(
          AutofillClient::EmailVerificationPermissionUiStatus::kUserAborted));
  EXPECT_FALSE(test_api(*evp_controller_).loading_start_time().has_value());

  // When the response arrives at 200ms, neither HidePopup() nor
  // HidePopup(CompletionStatus::kVerified) waits for the remaining 600ms, and
  // HideJavaBottomSheet() is not called again since the bottom sheet was
  // already dismissed.
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_CALL(*bridge_, HideJavaBottomSheet).Times(0);
  evp_controller_->HidePopup();
  EXPECT_FALSE(test_api(*evp_controller_).is_hide_popup_timer_running());

  EXPECT_CALL(mock_message_controller_, Show);
  evp_controller_->HidePopup(CompletionStatus::kVerified,
                             GURL("https://google.com"));
  EXPECT_FALSE(test_api(*evp_controller_).is_hide_popup_timer_running());
}

TEST_F(EmailVerificationControllerAndroidTest,
       BottomSheetDismissedAfterResponseFiresPendingToast) {
  ShowPopup();

  bridge_->OnUiAccepted(/*env=*/nullptr);

  // Response arrives at 200ms (< 800ms), scheduling hide_popup_timer_ for the
  // remaining 600ms.
  task_environment()->FastForwardBy(base::Milliseconds(200));
  evp_controller_->HidePopup(CompletionStatus::kVerified,
                             GURL("https://google.com"));
  EXPECT_TRUE(test_api(*evp_controller_).is_hide_popup_timer_running());

  // User swipes away the bottom sheet at 300ms. The pending verified message
  // fires immediately instead of waiting another 500ms.
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_CALL(mock_message_controller_, Show);
  bridge_->OnUiDismissed(
      /*env=*/nullptr,
      static_cast<int>(
          AutofillClient::EmailVerificationPermissionUiStatus::kUserAborted));
  EXPECT_FALSE(test_api(*evp_controller_).is_hide_popup_timer_running());
}

TEST_F(EmailVerificationControllerAndroidTest,
       BottomSheetClosedUnexpectedlyClearsLoadingTimer) {
  ShowPopup();

  bridge_->OnUiAccepted(/*env=*/nullptr);
  EXPECT_TRUE(test_api(*evp_controller_).loading_start_time().has_value());

  // Bottom sheet closed unexpectedly (e.g. tab backgrounded/destroyed) while
  // loading.
  task_environment()->FastForwardBy(base::Milliseconds(100));
  bridge_->OnUiDismissed(
      /*env=*/nullptr,
      static_cast<int>(
          AutofillClient::EmailVerificationPermissionUiStatus::kOther));
  EXPECT_FALSE(test_api(*evp_controller_).loading_start_time().has_value());

  // When response arrives, hiding does not wait and does not call Hide() again.
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_CALL(*bridge_, HideJavaBottomSheet).Times(0);
  evp_controller_->HidePopup();
  EXPECT_FALSE(test_api(*evp_controller_).is_hide_popup_timer_running());
}

TEST_F(EmailVerificationControllerAndroidTest,
       ShowPopupFlushesPendingHidePopupTimer) {
  ShowPopup();

  bridge_->OnUiAccepted(/*env=*/nullptr);

  // First flow completes at 200ms (< 800ms), starting hide_popup_timer_.
  task_environment()->FastForwardBy(base::Milliseconds(200));
  evp_controller_->HidePopup();
  EXPECT_TRUE(test_api(*evp_controller_).is_hide_popup_timer_running());

  // A new ShowPopup call while hide_popup_timer_ is pending immediately hides
  // the completed sheet and shows the new one.
  testing::InSequence s;
  EXPECT_CALL(*bridge_, HideJavaBottomSheet);
  EXPECT_CALL(*bridge_, ShowJavaBottomSheet);
  evp_controller_->ShowPopup(gfx::RectF(0, 0, 10, 10),
                             url::Origin::Create(GURL("https://google.com")),
                             u"other@example.com", base::DoNothing());
  EXPECT_FALSE(test_api(*evp_controller_).is_hide_popup_timer_running());
}

TEST_F(EmailVerificationControllerAndroidTest,
       OverlappingShowPopupWhileInFlightPreservesLoadingTimer) {
  ShowPopup();

  bridge_->OnUiAccepted(/*env=*/nullptr);
  EXPECT_TRUE(test_api(*evp_controller_).loading_start_time().has_value());

  // While the first request is still in-flight at 100ms, an overlapping
  // ShowPopup call is rejected with kOverlappingPrompt without hiding the
  // active sheet.
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_CALL(*bridge_, HideJavaBottomSheet).Times(0);
  EXPECT_CALL(*bridge_, ShowJavaBottomSheet).Times(0);
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      second_callback;
  EXPECT_CALL(second_callback,
              Run(AutofillClient::EmailVerificationPermissionUiStatus::
                      kOverlappingPrompt));
  evp_controller_->ShowPopup(gfx::RectF(0, 0, 10, 10),
                             url::Origin::Create(GURL("https://google.com")),
                             u"other@example.com", second_callback.Get());

  // The active first sheet's loading timer must still be preserved.
  EXPECT_TRUE(test_api(*evp_controller_).loading_start_time().has_value());
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_CALL(*bridge_, HideJavaBottomSheet).Times(0);
  evp_controller_->HidePopup();
  EXPECT_TRUE(test_api(*evp_controller_).is_hide_popup_timer_running());
}

TEST_F(EmailVerificationControllerAndroidTest,
       VerifiedToastActionInvokesOpenSettingsCallback) {
  std::unique_ptr<AutofillMessageModel> shown_model;
  EXPECT_CALL(mock_message_controller_, Show)
      .WillOnce([&shown_model](std::unique_ptr<AutofillMessageModel> model) {
        shown_model = std::move(model);
      });

  evp_controller_->HidePopup(CompletionStatus::kVerified,
                             GURL("https://google.com"));
  ASSERT_TRUE(shown_model);

  EXPECT_CALL(open_settings_callback_, Run);
  shown_model->OnActionClicked();
}

TEST_F(EmailVerificationControllerAndroidTest,
       OverlappingShowPopupWhileShowingReplacesSheet) {
  base::MockCallback<base::OnceCallback<void(
      AutofillClient::EmailVerificationPermissionUiStatus)>>
      first_ui_callback;
  ShowPopup(first_ui_callback.Get());

  // A second ShowPopup call while the first prompt is showing dismisses the
  // first sheet (calling HideJavaBottomSheet on bridge) and requests the new
  // sheet.
  testing::InSequence s;
  EXPECT_CALL(*bridge_, HideJavaBottomSheet);
  EXPECT_CALL(first_ui_callback,
              Run(AutofillClient::EmailVerificationPermissionUiStatus::kOther));
  EXPECT_CALL(*bridge_, ShowJavaBottomSheet);
  evp_controller_->ShowPopup(gfx::RectF(0, 0, 10, 10),
                             url::Origin::Create(GURL("https://google.com")),
                             u"other@example.com", base::DoNothing());
}

TEST_F(EmailVerificationControllerAndroidTest,
       ShowVerifiedToastDelaysDisplayForMinimumLoadingDuration) {
  ShowPopup();

  bridge_->OnUiAccepted(/*env=*/nullptr);

  // Verification response arrives at 200ms (< 800ms).
  task_environment()->FastForwardBy(base::Milliseconds(200));
  evp_controller_->HidePopup(CompletionStatus::kVerified,
                             GURL("https://google.com"));
  EXPECT_TRUE(test_api(*evp_controller_).is_hide_popup_timer_running());

  // Message should NOT be shown before the 800ms minimum loading duration.
  EXPECT_CALL(mock_message_controller_, Show).Times(0);
  task_environment()->FastForwardBy(base::Milliseconds(599));

  // Exactly at 800ms, the verified message is displayed and the sheet hidden.
  EXPECT_CALL(*bridge_, HideJavaBottomSheet);
  EXPECT_CALL(mock_message_controller_, Show);
  task_environment()->FastForwardBy(base::Milliseconds(1));
  EXPECT_FALSE(test_api(*evp_controller_).is_hide_popup_timer_running());
}

TEST_F(EmailVerificationControllerAndroidTest,
       ShowVerifiedToastDoesNotShowWhenWebContentsHidden) {
  web_contents()->WasHidden();
  EXPECT_CALL(mock_message_controller_, Show).Times(0);
  evp_controller_->HidePopup(CompletionStatus::kVerified,
                             GURL("https://google.com"));
}

TEST_F(EmailVerificationControllerAndroidTest,
       ShowErrorToastEnqueuesErrorMessage) {
  std::unique_ptr<AutofillMessageModel> shown_model;
  EXPECT_CALL(mock_message_controller_, Show)
      .WillOnce([&shown_model](std::unique_ptr<AutofillMessageModel> model) {
        shown_model = std::move(model);
      });

  evp_controller_->HidePopup(CompletionStatus::kError);
  ASSERT_TRUE(shown_model);
  EXPECT_EQ(shown_model->GetType(),
            AutofillMessageModel::Type::kEmailVerificationError);
  EXPECT_EQ(test_api(*shown_model).GetMessage().GetTitle(),
            l10n_util::GetStringUTF16(IDS_EMAIL_VERIFICATION_ERROR));
}

TEST_F(EmailVerificationControllerAndroidTest,
       ShowErrorToastDoesNotShowWhenWebContentsHidden) {
  web_contents()->WasHidden();
  EXPECT_CALL(mock_message_controller_, Show).Times(0);
  evp_controller_->HidePopup(CompletionStatus::kError);
}

TEST_F(EmailVerificationControllerAndroidTest,
       ShowLoadingToastEnqueuesLoadingMessage) {
  std::unique_ptr<AutofillMessageModel> shown_model;
  EXPECT_CALL(mock_message_controller_, Show)
      .WillOnce([&shown_model](std::unique_ptr<AutofillMessageModel> model) {
        shown_model = std::move(model);
      });

  evp_controller_->ShowLoadingToast(base::DoNothing());
  ASSERT_TRUE(shown_model);
  EXPECT_EQ(shown_model->GetType(),
            AutofillMessageModel::Type::kEmailVerificationLoading);
  EXPECT_EQ(test_api(*shown_model).GetMessage().GetTitle(),
            l10n_util::GetStringUTF16(IDS_EMAIL_VERIFICATION_LOADING));
  EXPECT_TRUE(
      test_api(*shown_model).GetMessage().GetPrimaryButtonText().empty());
  EXPECT_EQ(test_api(*shown_model).GetMessage().GetPrimaryWidgetAppearance(),
            messages::PrimaryWidgetAppearance::PROGRESS_SPINNER);
}

TEST_F(EmailVerificationControllerAndroidTest,
       ShowLoadingToastDoesNotShowWhenWebContentsHidden) {
  web_contents()->WasHidden();
  EXPECT_CALL(mock_message_controller_, Show).Times(0);
  evp_controller_->ShowLoadingToast(base::DoNothing());
}

TEST_F(EmailVerificationControllerAndroidTest,
       GestureDismissWhileInFlightInvokesCancelCallback) {
  std::unique_ptr<AutofillMessageModel> loading_model;
  EXPECT_CALL(mock_message_controller_, Dismiss).Times(0);
  EXPECT_CALL(mock_message_controller_, Show)
      .WillOnce([&loading_model](std::unique_ptr<AutofillMessageModel> model) {
        loading_model = std::move(model);
      });

  base::MockCallback<base::OnceClosure> cancel_callback;
  evp_controller_->ShowLoadingToast(cancel_callback.Get());
  ASSERT_TRUE(loading_model);
  testing::Mock::VerifyAndClearExpectations(&mock_message_controller_);

  // User swipes away the loading banner (GESTURE) at 400ms while verification
  // is still in flight.
  task_environment()->FastForwardBy(base::Milliseconds(400));
  EXPECT_CALL(cancel_callback, Run).Times(1);
  loading_model->OnDismissed(messages::DismissReason::GESTURE);
  EXPECT_FALSE(test_api(*evp_controller_).loading_start_time().has_value());
}

TEST_F(EmailVerificationControllerAndroidTest,
       FeatureDismissWhileInFlightDoesNotInvokeCancelCallback) {
  std::unique_ptr<AutofillMessageModel> loading_model;
  EXPECT_CALL(mock_message_controller_, Dismiss).Times(0);
  EXPECT_CALL(mock_message_controller_, Show)
      .WillOnce([&loading_model](std::unique_ptr<AutofillMessageModel> model) {
        loading_model = std::move(model);
      });

  base::MockCallback<base::OnceClosure> cancel_callback;
  evp_controller_->ShowLoadingToast(cancel_callback.Get());
  ASSERT_TRUE(loading_model);
  testing::Mock::VerifyAndClearExpectations(&mock_message_controller_);

  // A programmatic dismissal (e.g. replacement by a subsequent EVP UI state)
  // must not be treated as a user cancellation.
  EXPECT_CALL(cancel_callback, Run).Times(0);
  loading_model->OnDismissed(messages::DismissReason::DISMISSED_BY_FEATURE);
  EXPECT_TRUE(test_api(*evp_controller_).loading_start_time().has_value());
}

TEST_F(EmailVerificationControllerAndroidTest,
       ShowPopupDismissesActiveLoadingMessage) {
  std::unique_ptr<AutofillMessageModel> loading_model;
  EXPECT_CALL(mock_message_controller_, Dismiss).Times(0);
  EXPECT_CALL(mock_message_controller_, Show)
      .WillOnce([&loading_model](std::unique_ptr<AutofillMessageModel> model) {
        loading_model = std::move(model);
      });

  base::MockCallback<base::OnceClosure> cancel_callback;
  evp_controller_->ShowLoadingToast(cancel_callback.Get());
  ASSERT_TRUE(loading_model);
  testing::Mock::VerifyAndClearExpectations(&mock_message_controller_);

  // Calling ShowPopup while the loading message banner is active preempts the
  // in-flight verification (invoking cancel_callback once) and dismisses the
  // message banner with DISMISSED_BY_FEATURE without invoking cancel_callback
  // a second time.
  EXPECT_CALL(cancel_callback, Run).Times(1);
  EXPECT_CALL(mock_message_controller_,
              Dismiss(messages::DismissReason::DISMISSED_BY_FEATURE))
      .WillOnce([&loading_model](messages::DismissReason reason) {
        loading_model->OnDismissed(reason);
      });
  ShowPopup();
}

TEST_F(EmailVerificationControllerAndroidTest,
       HidePopupWhileLoadingToastDismissesLoadingMessageAfterMinimumDuration) {
  EXPECT_CALL(mock_message_controller_, Show);
  evp_controller_->ShowLoadingToast(base::DoNothing());
  testing::Mock::VerifyAndClearExpectations(&mock_message_controller_);

  // Calling HidePopup at 200ms (< 800ms) defers dismissing the loading message.
  task_environment()->FastForwardBy(base::Milliseconds(200));
  EXPECT_CALL(mock_message_controller_, Dismiss).Times(0);
  evp_controller_->HidePopup();
  EXPECT_TRUE(test_api(*evp_controller_).is_hide_popup_timer_running());

  // At 800ms, the loading message is dismissed.
  EXPECT_CALL(mock_message_controller_,
              Dismiss(messages::DismissReason::DISMISSED_BY_FEATURE));
  task_environment()->FastForwardBy(base::Milliseconds(600));
  EXPECT_FALSE(test_api(*evp_controller_).is_hide_popup_timer_running());
}

TEST_F(EmailVerificationControllerAndroidTest,
       ShowLoadingToastWhileLoadingToastDismissesPreviousLoadingMessage) {
  EXPECT_CALL(mock_message_controller_, Show);
  evp_controller_->ShowLoadingToast(base::DoNothing());
  testing::Mock::VerifyAndClearExpectations(&mock_message_controller_);

  testing::InSequence s;
  EXPECT_CALL(mock_message_controller_,
              Dismiss(messages::DismissReason::DISMISSED_BY_FEATURE));
  EXPECT_CALL(mock_message_controller_, Show);
  evp_controller_->ShowLoadingToast(base::DoNothing());
}

TEST_F(EmailVerificationControllerAndroidTest,
       ShowLoadingToastThenVerifiedToastDelaysAndDismissesLoadingMessage) {
  EXPECT_CALL(mock_message_controller_, Show);
  evp_controller_->ShowLoadingToast(base::DoNothing());
  testing::Mock::VerifyAndClearExpectations(&mock_message_controller_);

  // Verification response arrives at 200ms (< 800ms).
  task_environment()->FastForwardBy(base::Milliseconds(200));
  evp_controller_->HidePopup(CompletionStatus::kVerified,
                             GURL("https://google.com"));
  EXPECT_TRUE(test_api(*evp_controller_).is_hide_popup_timer_running());

  // Neither dismissal of the loading message nor display of the verified
  // message should happen before 800ms.
  EXPECT_CALL(mock_message_controller_, Dismiss).Times(0);
  EXPECT_CALL(mock_message_controller_, Show).Times(0);
  task_environment()->FastForwardBy(base::Milliseconds(599));

  // At 800ms, the loading message is dismissed and the verified message is
  // shown.
  std::unique_ptr<AutofillMessageModel> shown_model;
  {
    testing::InSequence s;
    EXPECT_CALL(mock_message_controller_,
                Dismiss(messages::DismissReason::DISMISSED_BY_FEATURE));
    EXPECT_CALL(mock_message_controller_, Show)
        .WillOnce([&shown_model](std::unique_ptr<AutofillMessageModel> model) {
          shown_model = std::move(model);
        });
  }
  task_environment()->FastForwardBy(base::Milliseconds(1));
  ASSERT_TRUE(shown_model);
  EXPECT_EQ(shown_model->GetType(), AutofillMessageModel::Type::kEmailVerified);
}

TEST_F(EmailVerificationControllerAndroidTest,
       ShowLoadingToastThenErrorToastDelaysAndDismissesLoadingMessage) {
  EXPECT_CALL(mock_message_controller_, Show);
  evp_controller_->ShowLoadingToast(base::DoNothing());
  testing::Mock::VerifyAndClearExpectations(&mock_message_controller_);

  // Error response arrives at 200ms (< 800ms).
  task_environment()->FastForwardBy(base::Milliseconds(200));
  evp_controller_->HidePopup(CompletionStatus::kError);
  EXPECT_TRUE(test_api(*evp_controller_).is_hide_popup_timer_running());

  // Neither dismissal of the loading message nor display of the error message
  // should happen before 800ms.
  EXPECT_CALL(mock_message_controller_, Dismiss).Times(0);
  EXPECT_CALL(mock_message_controller_, Show).Times(0);
  task_environment()->FastForwardBy(base::Milliseconds(599));

  // At 800ms, the loading message is dismissed and the error message is shown.
  std::unique_ptr<AutofillMessageModel> shown_model;
  {
    testing::InSequence s;
    EXPECT_CALL(mock_message_controller_,
                Dismiss(messages::DismissReason::DISMISSED_BY_FEATURE));
    EXPECT_CALL(mock_message_controller_, Show)
        .WillOnce([&shown_model](std::unique_ptr<AutofillMessageModel> model) {
          shown_model = std::move(model);
        });
  }
  task_environment()->FastForwardBy(base::Milliseconds(1));
  ASSERT_TRUE(shown_model);
  EXPECT_EQ(shown_model->GetType(),
            AutofillMessageModel::Type::kEmailVerificationError);
}

TEST_F(EmailVerificationControllerAndroidTest,
       ShowLoadingToastThenHiddenWebContentsStillDismissesLoadingMessage) {
  EXPECT_CALL(mock_message_controller_, Show);
  evp_controller_->ShowLoadingToast(base::DoNothing());
  testing::Mock::VerifyAndClearExpectations(&mock_message_controller_);

  // The WebContents becomes hidden while verification is in flight.
  web_contents()->WasHidden();
  task_environment()->FastForwardBy(base::Milliseconds(800));

  // When verification finishes, the loading message is still dismissed even
  // though the completion message is suppressed because WebContents is hidden.
  EXPECT_CALL(mock_message_controller_,
              Dismiss(messages::DismissReason::DISMISSED_BY_FEATURE));
  EXPECT_CALL(mock_message_controller_, Show).Times(0);
  evp_controller_->HidePopup(CompletionStatus::kVerified,
                             GURL("https://google.com"));
}

TEST_F(
    EmailVerificationControllerAndroidTest,
    ShowLoadingToastThenHiddenWebContentsStillDismissesLoadingMessageOnError) {
  EXPECT_CALL(mock_message_controller_, Show);
  evp_controller_->ShowLoadingToast(base::DoNothing());
  testing::Mock::VerifyAndClearExpectations(&mock_message_controller_);

  // The WebContents becomes hidden while verification is in flight.
  web_contents()->WasHidden();
  task_environment()->FastForwardBy(base::Milliseconds(800));

  // When verification fails, the loading message is still dismissed even
  // though the error message is suppressed because WebContents is hidden.
  EXPECT_CALL(mock_message_controller_,
              Dismiss(messages::DismissReason::DISMISSED_BY_FEATURE));
  EXPECT_CALL(mock_message_controller_, Show).Times(0);
  evp_controller_->HidePopup(CompletionStatus::kError);
}

}  // namespace
}  // namespace autofill
