// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/email_verifier/email_verification_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verification_controller_desktop.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verification_controller_test_api.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verification_popup_controller.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verification_popup_view.h"
#include "chrome/browser/ui/autofill/email_verifier/mock_email_verification_popup_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {
namespace {

using ::base::test::TestFuture;
using CompletionStatus = EmailVerificationController::CompletionStatus;
using EmailVerificationPermissionUiStatus =
    AutofillClient::EmailVerificationPermissionUiStatus;

constexpr gfx::RectF kDefaultBounds(0, 0, 10, 10);
constexpr char16_t kDefaultEmail[] = u"user@example.com";

url::Origin DefaultOrigin() {
  return url::Origin::Create(GURL("https://issuer.com"));
}

class EmailVerificationControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  EmailVerificationControllerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~EmailVerificationControllerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    controller_ =
        std::make_unique<EmailVerificationControllerDesktop>(web_contents());
  }

  void TearDown() override {
    controller_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  EmailVerificationController& controller() { return *controller_; }

  EmailVerificationPopupController& popup_controller() {
    return test_api(controller()).popup_controller();
  }

  void ResetController() { controller_.reset(); }

  void SetupMockViewFactory(
      std::unique_ptr<MockEmailVerificationPopupView>& mock_view_out) {
    popup_controller().set_view_factory_for_testing(base::BindRepeating(
        [](std::unique_ptr<MockEmailVerificationPopupView>* mock_view_ptr,
           base::WeakPtr<EmailVerificationPopupController> delegate,
           views::Widget* parent_widget, const url::Origin& issuer_origin,
           const std::u16string& email, base::OnceCallback<void(bool)> callback)
            -> base::WeakPtr<EmailVerificationPopupView> {
          *mock_view_ptr = std::make_unique<MockEmailVerificationPopupView>();
          (*mock_view_ptr)->set_decision_callback(std::move(callback));
          return (*mock_view_ptr)->GetWeakPtr();
        },
        base::Unretained(&mock_view_out)));
  }

 private:
  std::unique_ptr<EmailVerificationController> controller_;
};

// Tests that the controller initializes with no loading start time or active
// timers.
TEST_F(EmailVerificationControllerTest, InitialState) {
  EXPECT_FALSE(test_api(controller()).loading_start_time().has_value());
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
}

// Tests that calling ShowPopup shows the popup view and that confirming the
// prompt transitions to the loading state and records loading_start_time_.
TEST_F(EmailVerificationControllerTest, ConfirmPromptTransitionsToLoading) {
  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  SetupMockViewFactory(mock_view);

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().ShowPopup(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                         future.GetCallback());
  ASSERT_TRUE(mock_view);

  EXPECT_CALL(*mock_view, ShowLoadingState);
  EXPECT_CALL(*mock_view, Hide).Times(0);
  mock_view->RunDecisionCallback(true);

  EXPECT_EQ(future.Get(), EmailVerificationPermissionUiStatus::kAllowed);
  EXPECT_TRUE(test_api(controller()).loading_start_time().has_value());
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
}

// Tests that declining the first-run prompt resolves with kDeclined and
// does not record loading_start_time_.
TEST_F(EmailVerificationControllerTest,
       DeclinePromptDoesNotSetLoadingStartTime) {
  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  SetupMockViewFactory(mock_view);

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().ShowPopup(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                         future.GetCallback());
  ASSERT_TRUE(mock_view);

  EXPECT_CALL(*mock_view, Hide);
  mock_view->RunDecisionCallback(false);

  EXPECT_EQ(future.Get(), EmailVerificationPermissionUiStatus::kDeclined);
  EXPECT_FALSE(test_api(controller()).loading_start_time().has_value());
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
}

// Tests that HidePopup() enforces the minimum display duration (800ms) while
// the popup is in the loading state before dismissing the view.
TEST_F(EmailVerificationControllerTest,
       MinimumLoadingDurationEnforcedOnFastResponse) {
  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  SetupMockViewFactory(mock_view);

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().ShowPopup(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                         future.GetCallback());
  ASSERT_TRUE(mock_view);

  EXPECT_CALL(*mock_view, ShowLoadingState);
  EXPECT_CALL(*mock_view, Hide).Times(0);
  mock_view->RunDecisionCallback(true);

  // Fast response arrives before 800ms: HidePopup must defer dismissal.
  controller().HidePopup();
  EXPECT_TRUE(test_api(controller()).is_hide_popup_timer_running());

  // Halfway through (400ms): popup is still loading and timer is active.
  task_environment()->FastForwardBy(
      EmailVerificationController::kMinimumLoadingDuration / 2);
  EXPECT_TRUE(test_api(controller()).is_hide_popup_timer_running());

  testing::Mock::VerifyAndClearExpectations(mock_view.get());
  // Once remaining 400ms elapses: popup dismisses and state resets.
  EXPECT_CALL(*mock_view, Hide);
  task_environment()->FastForwardBy(
      EmailVerificationController::kMinimumLoadingDuration / 2);
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
  EXPECT_FALSE(test_api(controller()).loading_start_time().has_value());
}

// Tests that if HidePopup() is called after the minimum loading duration has
// already elapsed, the popup is dismissed immediately without starting a timer.
TEST_F(EmailVerificationControllerTest,
       HidePopupDismissesImmediatelyOnSlowResponse) {
  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  SetupMockViewFactory(mock_view);

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().ShowPopup(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                         future.GetCallback());
  ASSERT_TRUE(mock_view);

  EXPECT_CALL(*mock_view, ShowLoadingState);
  mock_view->RunDecisionCallback(true);

  // Response takes 900ms (>800ms).
  task_environment()->FastForwardBy(base::Milliseconds(900));

  // HidePopup should dismiss immediately.
  EXPECT_CALL(*mock_view, Hide);
  controller().HidePopup();
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
  EXPECT_FALSE(test_api(controller()).loading_start_time().has_value());
}

// Tests that calling HidePopup() when the popup is showing but not loading
// dismisses the popup immediately without starting a timer.
TEST_F(EmailVerificationControllerTest,
       HidePopupWhenNotLoadingDismissesImmediately) {
  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  SetupMockViewFactory(mock_view);

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().ShowPopup(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                         future.GetCallback());
  ASSERT_TRUE(mock_view);

  EXPECT_CALL(*mock_view, Hide);
  controller().HidePopup();
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
  EXPECT_FALSE(test_api(controller()).loading_start_time().has_value());
}

// Tests that calling HidePopup() before any popup is shown is a safe no-op.
TEST_F(EmailVerificationControllerTest, HidePopupWhenClosedIsNoOp) {
  controller().HidePopup();
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
  EXPECT_FALSE(test_api(controller()).loading_start_time().has_value());
}

// Tests that in the subsequent-run flow, calling HidePopup() followed
// immediately by HidePopup(CompletionStatus::kVerified) while the loading toast
// is active re-arms `hide_popup_timer_` to dismiss the loading toast and show
// the verified toast once the minimum loading duration has elapsed.
TEST_F(EmailVerificationControllerTest,
       SubsequentRunShowVerifiedToastDefersDuringHidePopupDelay) {
  controller().ShowLoadingToast(base::DoNothing());
  EXPECT_TRUE(test_api(controller()).loading_start_time().has_value());

  controller().HidePopup();
  EXPECT_TRUE(test_api(controller()).is_hide_popup_timer_running());
  EXPECT_TRUE(test_api(controller()).loading_start_time().has_value());

  controller().HidePopup(CompletionStatus::kVerified,
                         GURL("https://issuer.com"));
  EXPECT_TRUE(test_api(controller()).is_hide_popup_timer_running());

  task_environment()->FastForwardBy(
      EmailVerificationController::kMinimumLoadingDuration);
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
  EXPECT_FALSE(test_api(controller()).loading_start_time().has_value());
}

// Tests that calling ShowLoadingToast() initiates loading state and sets
// loading_start_time_.
TEST_F(EmailVerificationControllerTest, SubsequentRunLoadingToastStartsTimer) {
  controller().ShowLoadingToast(base::DoNothing());
  EXPECT_TRUE(test_api(controller()).loading_start_time().has_value());
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
}

// Tests that calling HidePopup() while a loading toast is active defers
// dismissal until the minimum loading duration has elapsed, then resets state.
TEST_F(EmailVerificationControllerTest,
       HidePopupWhileLoadingToastDismissesAfterMinimumDuration) {
  controller().ShowLoadingToast(base::DoNothing());
  EXPECT_TRUE(test_api(controller()).loading_start_time().has_value());

  controller().HidePopup();
  EXPECT_TRUE(test_api(controller()).loading_start_time().has_value());
  EXPECT_TRUE(test_api(controller()).is_hide_popup_timer_running());

  // Halfway through (400ms): timer is still running.
  task_environment()->FastForwardBy(
      EmailVerificationController::kMinimumLoadingDuration / 2);
  EXPECT_TRUE(test_api(controller()).loading_start_time().has_value());
  EXPECT_TRUE(test_api(controller()).is_hide_popup_timer_running());

  // Full duration (800ms) elapses: loading_start_time_ is cleared.
  task_environment()->FastForwardBy(
      EmailVerificationController::kMinimumLoadingDuration / 2);
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
  EXPECT_FALSE(test_api(controller()).loading_start_time().has_value());
}

// Tests that calling HidePopup() while a loading toast is active after the
// minimum loading duration has already elapsed clears `loading_start_time_`
// immediately without starting `hide_popup_timer_`.
TEST_F(EmailVerificationControllerTest,
       HidePopupWhileLoadingToastDismissesImmediatelyOnSlowResponse) {
  controller().ShowLoadingToast(base::DoNothing());
  EXPECT_TRUE(test_api(controller()).loading_start_time().has_value());

  // Response takes 900ms (>800ms).
  task_environment()->FastForwardBy(base::Milliseconds(900));
  EXPECT_TRUE(test_api(controller()).loading_start_time().has_value());

  controller().HidePopup();
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
  EXPECT_FALSE(test_api(controller()).loading_start_time().has_value());
}

// Tests that calling ShowPopup() while a loading toast is active cancels
// active timers, resets `loading_start_time_`, and shows the popup.
TEST_F(EmailVerificationControllerTest,
       ShowPopupWhileLoadingToastCancelsTimersAndTransitionsToShowing) {
  controller().ShowLoadingToast(base::DoNothing());
  controller().HidePopup();
  ASSERT_TRUE(test_api(controller()).is_hide_popup_timer_running());
  ASSERT_TRUE(test_api(controller()).loading_start_time().has_value());

  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  SetupMockViewFactory(mock_view);
  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().ShowPopup(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                         future.GetCallback());
  ASSERT_TRUE(mock_view);
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
  EXPECT_FALSE(test_api(controller()).loading_start_time().has_value());

  EXPECT_CALL(*mock_view, Hide);
  popup_controller().Dismiss();
}

// Tests that calling ShowLoadingToast() while a loading toast is already
// active cancels any pending hide timer and re-arms the loading timer.
TEST_F(EmailVerificationControllerTest,
       ShowLoadingToastWhileLoadingToastReArmsLoadingTimer) {
  controller().ShowLoadingToast(base::DoNothing());
  const std::optional<base::TimeTicks> first_start_time =
      test_api(controller()).loading_start_time();
  ASSERT_TRUE(first_start_time.has_value());

  // Advance 300ms and arm `hide_popup_timer_`.
  task_environment()->FastForwardBy(base::Milliseconds(300));
  controller().HidePopup();
  ASSERT_TRUE(test_api(controller()).is_hide_popup_timer_running());

  // Calling ShowLoadingToast() again cancels the pending hide timer and
  // records a fresh `loading_start_time_`.
  controller().ShowLoadingToast(base::DoNothing());
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
  ASSERT_TRUE(test_api(controller()).loading_start_time().has_value());
  EXPECT_GT(*test_api(controller()).loading_start_time(), *first_start_time);
}

// Tests that cancelling loading while a request is in flight invokes the
// cancellation callback and resets loading state.
TEST_F(EmailVerificationControllerTest,
       OnLoadingCancelledWhileInFlightInvokesCancelCallback) {
  TestFuture<void> cancel_future;
  controller().ShowLoadingToast(cancel_future.GetCallback());
  EXPECT_TRUE(test_api(controller()).loading_start_time().has_value());

  test_api(controller()).OnLoadingCancelled();
  EXPECT_TRUE(cancel_future.IsReady());
  EXPECT_FALSE(test_api(controller()).loading_start_time().has_value());
}

// Tests that cancelling loading after the response has arrived (while the toast
// is deferred) immediately flushes the deferred toast if the tab is visible.
TEST_F(EmailVerificationControllerTest,
       OnLoadingCancelledAfterResponseFlushesDeferredToast) {
  TestFuture<void> cancel_future;
  controller().ShowLoadingToast(cancel_future.GetCallback());

  // Response arrives before minimum duration; toast is deferred.
  controller().HidePopup(CompletionStatus::kVerified,
                         GURL("https://issuer.com"));
  EXPECT_TRUE(test_api(controller()).is_hide_popup_timer_running());

  // User dismisses/cancels the loading UI while toast is deferred.
  test_api(controller()).OnLoadingCancelled();

  // Cancel callback should NOT be invoked because the response already arrived.
  EXPECT_FALSE(cancel_future.IsReady());
  // Timer should have fired immediately and loading_start_time_ should be
  // cleared.
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
  EXPECT_FALSE(test_api(controller()).loading_start_time().has_value());
}

// Tests that calling OnLoadingCancelled() when no loading toast is active
// (e.g., while a first-run prompt is loading) is a no-op and does not
// corrupt the active prompt's state or timers.
TEST_F(EmailVerificationControllerTest,
       OnLoadingCancelledWhenNotLoadingToastIsNoOp) {
  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  SetupMockViewFactory(mock_view);

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().ShowPopup(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                         future.GetCallback());
  ASSERT_TRUE(mock_view);
  EXPECT_CALL(*mock_view, ShowLoadingState);
  mock_view->RunDecisionCallback(true);

  controller().HidePopup();
  EXPECT_TRUE(test_api(controller()).is_hide_popup_timer_running());
  EXPECT_TRUE(test_api(controller()).loading_start_time().has_value());

  // Spurious OnLoadingCancelled() must not disturb loading state or timer.
  test_api(controller()).OnLoadingCancelled();
  EXPECT_TRUE(test_api(controller()).is_hide_popup_timer_running());
  EXPECT_TRUE(test_api(controller()).loading_start_time().has_value());

  EXPECT_CALL(*mock_view, Hide);
  task_environment()->FastForwardBy(
      EmailVerificationController::kMinimumLoadingDuration);
}

// Tests that preempting an in-flight loading toast with ShowPopup() invokes the
// preempted request's cancel callback.
TEST_F(EmailVerificationControllerTest,
       ShowPopupWhileLoadingToastInFlightInvokesCancelCallback) {
  TestFuture<void> cancel_future;
  controller().ShowLoadingToast(cancel_future.GetCallback());

  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  SetupMockViewFactory(mock_view);
  TestFuture<EmailVerificationPermissionUiStatus> popup_future;
  controller().ShowPopup(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                         popup_future.GetCallback());

  EXPECT_TRUE(cancel_future.IsReady());

  EXPECT_CALL(*mock_view, Hide);
  controller().HidePopup();
}

// Tests that preempting an in-flight loading toast with a new
// ShowLoadingToast() invokes the first request's cancel callback without
// cancelling the new one.
TEST_F(EmailVerificationControllerTest,
       ShowLoadingToastWhileLoadingToastInFlightInvokesPreviousCancelCallback) {
  TestFuture<void> first_cancel_future;
  controller().ShowLoadingToast(first_cancel_future.GetCallback());

  TestFuture<void> second_cancel_future;
  controller().ShowLoadingToast(second_cancel_future.GetCallback());

  EXPECT_TRUE(first_cancel_future.IsReady());
  EXPECT_FALSE(second_cancel_future.IsReady());

  test_api(controller()).OnLoadingCancelled();
  EXPECT_TRUE(second_cancel_future.IsReady());
}

// Tests that calling ShowLoadingToast() while a first-run popup's hide timer
// is pending cancels `hide_popup_timer_` and immediately dismisses the popup.
TEST_F(EmailVerificationControllerTest, ShowLoadingToastHidesPendingPopup) {
  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  SetupMockViewFactory(mock_view);

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().ShowPopup(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                         future.GetCallback());
  ASSERT_TRUE(mock_view);
  EXPECT_CALL(*mock_view, ShowLoadingState);
  mock_view->RunDecisionCallback(true);

  // Start the anti-flicker hide timer for the first-run popup.
  controller().HidePopup();
  EXPECT_TRUE(test_api(controller()).is_hide_popup_timer_running());

  // Triggering a subsequent-run loading toast must cancel the pending hide
  // timer and immediately dismiss the first-run popup.
  EXPECT_CALL(*mock_view, Hide);
  controller().ShowLoadingToast(base::DoNothing());
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
  EXPECT_TRUE(test_api(controller()).loading_start_time().has_value());
}

// Tests that ShowLoadingToast() dismisses an open first-run permission prompt.
TEST_F(EmailVerificationControllerTest,
       ShowLoadingToastWhileShowingHidesPopup) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  SetupMockViewFactory(mock_view);

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().ShowPopup(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                         future.GetCallback());
  ASSERT_TRUE(mock_view);

  EXPECT_CALL(*mock_view, Hide);
  controller().ShowLoadingToast(base::DoNothing());

  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), EmailVerificationPermissionUiStatus::kOther);
  EXPECT_TRUE(test_api(controller()).loading_start_time().has_value());
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPermissionUiStatus::kOther, 1);

  // Subsequent user interaction on the WebContents must not log a duplicate
  // kUserAborted sample or reset loading_start_time_.
  blink::WebMouseEvent mouse_event;
  popup_controller().DidGetUserInteraction(mouse_event);
  histogram_tester.ExpectTotalCount("Blink.Evp.PermissionUi.Status", 1);
  EXPECT_TRUE(test_api(controller()).loading_start_time().has_value());
}

// Tests that calling HidePopup(CompletionStatus::kVerified) while loading is in
// progress defers displaying the toast until the minimum display duration
// (800ms) has elapsed.
TEST_F(EmailVerificationControllerTest, ShowVerifiedToastDefersWhenLoading) {
  controller().ShowLoadingToast(base::DoNothing());
  ASSERT_TRUE(test_api(controller()).loading_start_time().has_value());

  controller().HidePopup(CompletionStatus::kVerified,
                         GURL("https://issuer.com"));
  EXPECT_TRUE(test_api(controller()).is_hide_popup_timer_running());

  // Halfway through (400ms): timer is still running.
  task_environment()->FastForwardBy(
      EmailVerificationController::kMinimumLoadingDuration / 2);
  EXPECT_TRUE(test_api(controller()).is_hide_popup_timer_running());

  // Full duration (800ms) elapses: timer fires and completes.
  task_environment()->FastForwardBy(
      EmailVerificationController::kMinimumLoadingDuration / 2);
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
  EXPECT_FALSE(test_api(controller()).loading_start_time().has_value());
}

// Tests that calling HidePopup(CompletionStatus::kError) while loading is in
// progress defers displaying the error toast until the minimum display duration
// (800ms) has elapsed.
TEST_F(EmailVerificationControllerTest, ShowErrorToastDefersWhenLoading) {
  controller().ShowLoadingToast(base::DoNothing());
  ASSERT_TRUE(test_api(controller()).loading_start_time().has_value());

  controller().HidePopup(CompletionStatus::kError);
  EXPECT_TRUE(test_api(controller()).is_hide_popup_timer_running());

  // Halfway through (400ms): timer is still running.
  task_environment()->FastForwardBy(
      EmailVerificationController::kMinimumLoadingDuration / 2);
  EXPECT_TRUE(test_api(controller()).is_hide_popup_timer_running());

  // Full duration (800ms) elapses: timer fires and completes.
  task_environment()->FastForwardBy(
      EmailVerificationController::kMinimumLoadingDuration / 2);
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
  EXPECT_FALSE(test_api(controller()).loading_start_time().has_value());
}

// Tests that in the first-run flow, calling
// HidePopup(CompletionStatus::kVerified) while loading defers hiding the popup
// and showing the toast until the minimum loading duration has elapsed.
TEST_F(EmailVerificationControllerTest,
       FirstRunShowVerifiedToastDefersDuringHidePopupDelay) {
  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  SetupMockViewFactory(mock_view);

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().ShowPopup(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                         future.GetCallback());
  ASSERT_TRUE(mock_view);

  EXPECT_CALL(*mock_view, ShowLoadingState);
  mock_view->RunDecisionCallback(true);

  // HidePopup(CompletionStatus::kVerified) is called upon receiving the
  // verification response before 800ms has elapsed.
  controller().HidePopup(CompletionStatus::kVerified,
                         GURL("https://issuer.com"));
  EXPECT_TRUE(test_api(controller()).is_hide_popup_timer_running());

  // Advance past minimum loading duration: popup is hidden and toast is shown.
  EXPECT_CALL(*mock_view, Hide);
  task_environment()->FastForwardBy(
      EmailVerificationController::kMinimumLoadingDuration);
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
  EXPECT_FALSE(test_api(controller()).loading_start_time().has_value());
}

// Tests that calling HidePopup(CompletionStatus::kVerified) or
// HidePopup(CompletionStatus::kError) when not loading does not start a
// deferral timer.
TEST_F(EmailVerificationControllerTest,
       ShowToastFiresImmediatelyWhenNotLoading) {
  controller().HidePopup(CompletionStatus::kVerified,
                         GURL("https://issuer.com"));
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
  EXPECT_FALSE(test_api(controller()).loading_start_time().has_value());

  controller().HidePopup(CompletionStatus::kError);
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
  EXPECT_FALSE(test_api(controller()).loading_start_time().has_value());
}

// Tests that calling HidePopup(CompletionStatus::kVerified) after the minimum
// display duration has already elapsed does not defer.
TEST_F(EmailVerificationControllerTest,
       ShowToastFiresImmediatelyWhenDurationAlreadyElapsed) {
  controller().ShowLoadingToast(base::DoNothing());
  ASSERT_TRUE(test_api(controller()).loading_start_time().has_value());

  task_environment()->FastForwardBy(base::Milliseconds(900));

  controller().HidePopup(CompletionStatus::kVerified,
                         GURL("https://issuer.com"));
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
  EXPECT_FALSE(test_api(controller()).loading_start_time().has_value());
}

// Tests that if an error toast is requested while a verified toast is pending,
// the pending timer is superseded and only the error toast is scheduled.
TEST_F(EmailVerificationControllerTest, ToastTimersCancelEachOther) {
  controller().ShowLoadingToast(base::DoNothing());
  ASSERT_TRUE(test_api(controller()).loading_start_time().has_value());

  // Trigger verified toast first.
  controller().HidePopup(CompletionStatus::kVerified,
                         GURL("https://issuer.com"));
  EXPECT_TRUE(test_api(controller()).is_hide_popup_timer_running());

  // Trigger error toast; must supersede the pending verified toast.
  controller().HidePopup(CompletionStatus::kError);
  EXPECT_TRUE(test_api(controller()).is_hide_popup_timer_running());

  task_environment()->FastForwardBy(
      EmailVerificationController::kMinimumLoadingDuration);
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
  EXPECT_FALSE(test_api(controller()).loading_start_time().has_value());
}

// Tests that if a new popup is requested while a previous popup's hide timer
// is pending, the timer is canceled and the previous view is dismissed.
TEST_F(EmailVerificationControllerTest, NewPopupCancelsPendingHidePopupTimer) {
  std::unique_ptr<MockEmailVerificationPopupView> first_view;
  SetupMockViewFactory(first_view);

  TestFuture<EmailVerificationPermissionUiStatus> first_future;
  controller().ShowPopup(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                         first_future.GetCallback());
  ASSERT_TRUE(first_view);

  EXPECT_CALL(*first_view, ShowLoadingState);
  EXPECT_CALL(*first_view, Hide).Times(0);
  first_view->RunDecisionCallback(true);

  // Call HidePopup, starting the anti-flicker delay timer.
  controller().HidePopup();
  EXPECT_TRUE(test_api(controller()).is_hide_popup_timer_running());

  // Rapidly trigger a new popup before the 800ms timer fires.
  // The first view should be torn down when the new popup is created.
  testing::Mock::VerifyAndClearExpectations(first_view.get());
  EXPECT_CALL(*first_view, Hide);
  std::unique_ptr<MockEmailVerificationPopupView> new_mock_view;
  SetupMockViewFactory(new_mock_view);

  TestFuture<EmailVerificationPermissionUiStatus> second_future;
  controller().ShowPopup(gfx::RectF(20, 20, 10, 10), DefaultOrigin(),
                         kDefaultEmail, second_future.GetCallback());
  ASSERT_TRUE(new_mock_view);

  // ShowPopup must have cancelled the pending hide timer and reset state.
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
  EXPECT_FALSE(test_api(controller()).loading_start_time().has_value());

  // Fast forward past 800ms; the new popup view should NOT be dismissed by the
  // previous timer.
  EXPECT_CALL(*new_mock_view, Hide).Times(0);
  task_environment()->FastForwardBy(base::Milliseconds(900));

  // Explicitly dismiss at the end of test.
  testing::Mock::VerifyAndClearExpectations(new_mock_view.get());
  EXPECT_CALL(*new_mock_view, Hide);
  popup_controller().Dismiss();
}

// Tests that calling ShowPopup() while a previous popup is showing dismisses
// the previous popup (resolving its callback with kOther) and shows the new
// one.
TEST_F(EmailVerificationControllerTest,
       ShowPopupWhileShowingDismissesFirstPopupAndShowsSecond) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<MockEmailVerificationPopupView> first_view;
  SetupMockViewFactory(first_view);

  TestFuture<EmailVerificationPermissionUiStatus> first_future;
  controller().ShowPopup(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                         first_future.GetCallback());
  ASSERT_TRUE(first_view);

  std::unique_ptr<MockEmailVerificationPopupView> second_view;
  SetupMockViewFactory(second_view);
  EXPECT_CALL(*first_view, Hide);

  TestFuture<EmailVerificationPermissionUiStatus> second_future;
  controller().ShowPopup(gfx::RectF(20, 20, 10, 10), DefaultOrigin(),
                         u"second@example.com", second_future.GetCallback());

  ASSERT_TRUE(first_future.IsReady());
  EXPECT_EQ(first_future.Get(), EmailVerificationPermissionUiStatus::kOther);
  EXPECT_FALSE(second_future.IsReady());
  ASSERT_TRUE(second_view);
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPermissionUiStatus::kOther, 1);

  // Explicitly dismiss the replacement popup before teardown.
  EXPECT_CALL(*second_view, Hide);
  popup_controller().Dismiss();
}

// Tests that calling ShowPopup() while a first-run request is actively in
// flight rejects the overlapping request with kOverlappingPrompt.
TEST_F(EmailVerificationControllerTest,
       ShowPopupWhileFirstRunRequestInFlightRejectsRequest) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  SetupMockViewFactory(mock_view);

  TestFuture<EmailVerificationPermissionUiStatus> first_future;
  controller().ShowPopup(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                         first_future.GetCallback());
  ASSERT_TRUE(mock_view);

  EXPECT_CALL(*mock_view, ShowLoadingState);
  mock_view->RunDecisionCallback(true);
  EXPECT_EQ(first_future.Get(), EmailVerificationPermissionUiStatus::kAllowed);
  const std::optional<base::TimeTicks> initial_loading_start_time =
      test_api(controller()).loading_start_time();
  ASSERT_TRUE(initial_loading_start_time.has_value());
  const gfx::RectF initial_element_bounds = popup_controller().element_bounds();

  // Attempt an overlapping ShowPopup while the first request is still in
  // flight.
  EXPECT_CALL(*mock_view, Hide).Times(0);
  TestFuture<EmailVerificationPermissionUiStatus> second_future;
  controller().ShowPopup(gfx::RectF(20, 20, 10, 10), DefaultOrigin(),
                         u"second@example.com", second_future.GetCallback());

  EXPECT_EQ(second_future.Get(),
            EmailVerificationPermissionUiStatus::kOverlappingPrompt);
  EXPECT_EQ(test_api(controller()).loading_start_time(),
            initial_loading_start_time);
  EXPECT_EQ(popup_controller().element_bounds(), initial_element_bounds);
  histogram_tester.ExpectBucketCount(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPermissionUiStatus::kOverlappingPrompt, 1);

  testing::Mock::VerifyAndClearExpectations(mock_view.get());
  // Completing the first request still respects the original 800ms timer.
  controller().HidePopup();
  EXPECT_TRUE(test_api(controller()).is_hide_popup_timer_running());
  EXPECT_CALL(*mock_view, Hide);
  task_environment()->FastForwardBy(
      EmailVerificationController::kMinimumLoadingDuration);
}

// Tests that destroying EmailVerificationController while a popup is open
// resolves the caller's pending callback with kOther.
TEST_F(EmailVerificationControllerTest,
       ControllerDestructionWhileShowingResolvesCallback) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  SetupMockViewFactory(mock_view);

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().ShowPopup(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                         future.GetCallback());
  ASSERT_TRUE(mock_view);

  EXPECT_CALL(*mock_view, Hide);
  ResetController();
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), EmailVerificationPermissionUiStatus::kOther);
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPermissionUiStatus::kOther, 1);
}

// Tests that destroying EmailVerificationController while a hide timer is
// running cleans up cleanly without use-after-free or dangling timers.
TEST_F(EmailVerificationControllerTest,
       ControllerDestructionWhileLoadingCleansUpCleanly) {
  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  SetupMockViewFactory(mock_view);

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().ShowPopup(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                         future.GetCallback());
  ASSERT_TRUE(mock_view);

  EXPECT_CALL(*mock_view, ShowLoadingState);
  mock_view->RunDecisionCallback(true);

  controller().HidePopup();
  EXPECT_TRUE(test_api(controller()).is_hide_popup_timer_running());

  EXPECT_CALL(*mock_view, Hide);
  ResetController();
}

// A `WebContentsViewDelegate` that records calls to `WebContents::Focus()`. It
// is installed through `FocusRecordingContentBrowserClient` because the test
// `RenderWidgetHostView` does not track focus.
class FocusRecordingWebContentsViewDelegate
    : public content::WebContentsViewDelegate {
 public:
  explicit FocusRecordingWebContentsViewDelegate(int& focus_count)
      : focus_count_(focus_count) {}

  // `WebContentsView::Focus()` calls this on all desktop platforms (unlike
  // `Focus()`, which the Mac implementation does not forward to the delegate).
  void ResetStoredFocus() override { ++*focus_count_; }

 private:
  raw_ref<int> focus_count_;
};

class FocusRecordingContentBrowserClient
    : public content::ContentBrowserClient {
 public:
  std::unique_ptr<content::WebContentsViewDelegate> GetWebContentsViewDelegate(
      content::WebContents* web_contents) override {
    return std::make_unique<FocusRecordingWebContentsViewDelegate>(
        focus_count_);
  }

  int focus_count() const { return focus_count_; }

 private:
  int focus_count_ = 0;
};

class EmailVerificationControllerFocusTest
    : public EmailVerificationControllerTest {
 public:
  void SetUp() override {
    // The client must be installed before the test WebContents is created so
    // that its view picks up the recording delegate.
    scoped_client_setting_ =
        std::make_unique<content::ScopedContentBrowserClientSetting>(
            &browser_client_);
    EmailVerificationControllerTest::SetUp();
  }

  void TearDown() override {
    EmailVerificationControllerTest::TearDown();
    scoped_client_setting_.reset();
  }

  int focus_count() const { return browser_client_.focus_count(); }

 private:
  FocusRecordingContentBrowserClient browser_client_;
  std::unique_ptr<content::ScopedContentBrowserClientSetting>
      scoped_client_setting_;
};

// Tests that dismissing the popup after it entered the loading state returns
// focus to the WebContents, so that the form field regains focus instead of
// the completion toast.
TEST_F(EmailVerificationControllerFocusTest,
       HidePopupReturnsFocusToWebContentsWhenLoading) {
  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  SetupMockViewFactory(mock_view);

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().ShowPopup(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                         future.GetCallback());

  ASSERT_TRUE(mock_view);
  EXPECT_CALL(*mock_view, ShowLoadingState);
  mock_view->RunDecisionCallback(true);

  // Let the minimum loading duration elapse so that HidePopup() dismisses the
  // popup immediately.
  task_environment()->FastForwardBy(
      EmailVerificationController::kMinimumLoadingDuration);

  const int focus_count_before = focus_count();
  EXPECT_CALL(*mock_view, Hide);
  controller().HidePopup();
  EXPECT_EQ(focus_count(), focus_count_before + 1);
}

// Tests that calling HidePopup() while the popup is showing (before the user
// clicks Verify) dismisses the popup immediately without returning focus to
// the WebContents, since the non-loading popup never took activation.
TEST_F(EmailVerificationControllerFocusTest,
       HidePopupWhileShowingDismissesImmediatelyWithoutReturningFocus) {
  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  SetupMockViewFactory(mock_view);

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().ShowPopup(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                         future.GetCallback());
  ASSERT_TRUE(mock_view);

  const int focus_count_before = focus_count();
  EXPECT_CALL(*mock_view, Hide);
  controller().HidePopup();
  EXPECT_FALSE(test_api(controller()).is_hide_popup_timer_running());
  EXPECT_EQ(focus_count(), focus_count_before);
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), EmailVerificationPermissionUiStatus::kOther);
}

}  // namespace
}  // namespace autofill
