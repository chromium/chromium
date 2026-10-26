// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/email_verifier/email_verification_popup_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verification_popup_view.h"
#include "chrome/browser/ui/autofill/email_verifier/mock_email_verification_popup_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace views {
class Widget;
}

namespace autofill {
namespace {

using ::base::test::TestFuture;
using EmailVerificationPermissionUiStatus =
    AutofillClient::EmailVerificationPermissionUiStatus;

constexpr gfx::RectF kDefaultBounds(0, 0, 10, 10);
constexpr char16_t kDefaultEmail[] = u"user@example.com";

url::Origin DefaultOrigin() {
  return url::Origin::Create(GURL("https://issuer.com"));
}

class EmailVerificationPopupControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  EmailVerificationPopupControllerTest() = default;
  ~EmailVerificationPopupControllerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    controller_ =
        std::make_unique<EmailVerificationPopupController>(web_contents());
  }

  void TearDown() override {
    controller_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  EmailVerificationPopupController& controller() { return *controller_; }
  void ResetController() { controller_.reset(); }

  std::unique_ptr<MockEmailVerificationPopupView> SetupMockViewFactory() {
    auto mock_view = std::make_unique<MockEmailVerificationPopupView>();
    controller().set_view_factory_for_testing(base::BindRepeating(
        [](base::WeakPtr<MockEmailVerificationPopupView> view,
           base::WeakPtr<EmailVerificationPopupController> delegate,
           views::Widget* parent_widget, const url::Origin& issuer_origin,
           const std::u16string& email, base::OnceCallback<void(bool)> callback)
            -> base::WeakPtr<EmailVerificationPopupView> {
          view->set_decision_callback(std::move(callback));
          return view->GetWeakPtr();
        },
        mock_view->GetWeakPtr()));
    return mock_view;
  }

 private:
  std::unique_ptr<EmailVerificationPopupController> controller_;
};

// Tests that the controller initializes in a non-loading state and provides
// correct AutofillPopupViewDelegate default properties.
TEST_F(EmailVerificationPopupControllerTest, InitialStateAndDelegateGetters) {
  EXPECT_FALSE(controller().is_loading());
  EXPECT_EQ(controller().anchor_type(), PopupAnchorType::kField);
  EXPECT_EQ(controller().GetElementTextDirection(),
            base::i18n::TextDirection::UNKNOWN_DIRECTION);
  EXPECT_EQ(controller().GetWebContents(), web_contents());
  EXPECT_EQ(controller().container_view(),
            web_contents()->GetContentNativeView());
  EXPECT_TRUE(controller().element_bounds().IsEmpty());
}

// Tests that calling Show invokes the view factory, retains the element bounds,
// and leaves the callback pending until the user interacts.
TEST_F(EmailVerificationPopupControllerTest, ShowInvokesViewFactory) {
  auto mock_view = SetupMockViewFactory();

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().Show(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                    future.GetCallback());

  EXPECT_EQ(controller().element_bounds(), kDefaultBounds);
  EXPECT_FALSE(controller().is_loading());
  EXPECT_FALSE(future.IsReady());

  // Dismiss to clean up cleanly before test tear down.
  EXPECT_CALL(*mock_view, Hide);
  controller().Dismiss();
}

// Tests that calling Show with a `nullptr` WebContents resolves callback with
// kOther and logs Blink.Evp.PermissionUi.Status.
TEST_F(EmailVerificationPopupControllerTest,
       ShowWithNullWebContentsResolvesCallbackWithOther) {
  base::HistogramTester histogram_tester;
  auto controller = std::make_unique<EmailVerificationPopupController>(
      /*web_contents=*/nullptr);

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller->Show(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                   future.GetCallback());

  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), EmailVerificationPermissionUiStatus::kOther);
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPermissionUiStatus::kOther, 1);
}

// Tests that if view creation fails, Show resolves callback with kOther and
// logs Blink.Evp.PermissionUi.Status.
TEST_F(EmailVerificationPopupControllerTest,
       ShowEarlyReturnWhenViewFactoryFailsResolvesCallback) {
  base::HistogramTester histogram_tester;
  controller().set_view_factory_for_testing(base::BindRepeating(
      [](base::WeakPtr<EmailVerificationPopupController>, views::Widget*,
         const url::Origin&, const std::u16string&,
         base::OnceCallback<void(bool)>)
          -> base::WeakPtr<EmailVerificationPopupView> {
        return base::WeakPtr<EmailVerificationPopupView>();
      }));

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().Show(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                    future.GetCallback());

  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), EmailVerificationPermissionUiStatus::kOther);
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPermissionUiStatus::kOther, 1);
}

// Tests that confirming the prompt (clicking Verify) transitions the popup view
// to loading, sets is_loading to true, and resolves the callback with kAllowed.
TEST_F(EmailVerificationPopupControllerTest,
       ConfirmPromptTransitionsToLoadingAndResolvesAllowed) {
  base::HistogramTester histogram_tester;
  auto mock_view = SetupMockViewFactory();

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().Show(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                    future.GetCallback());

  EXPECT_CALL(*mock_view, ShowLoadingState);
  EXPECT_CALL(*mock_view, Hide).Times(0);
  mock_view->RunDecisionCallback(true);

  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), EmailVerificationPermissionUiStatus::kAllowed);
  EXPECT_TRUE(controller().is_loading());
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPermissionUiStatus::kAllowed, 1);

  // Clean up the active loading view.
  testing::Mock::VerifyAndClearExpectations(mock_view.get());
  EXPECT_CALL(*mock_view, Hide);
  controller().Dismiss();
  EXPECT_FALSE(controller().is_loading());
}

// Tests that canceling the prompt (clicking Don't allow) hides the view,
// resets is_loading to false, and resolves callback with kDeclined.
TEST_F(EmailVerificationPopupControllerTest,
       CancelPromptHidesViewAndResolvesDeclined) {
  base::HistogramTester histogram_tester;
  auto mock_view = SetupMockViewFactory();

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().Show(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                    future.GetCallback());

  EXPECT_CALL(*mock_view, Hide);
  mock_view->RunDecisionCallback(false);

  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), EmailVerificationPermissionUiStatus::kDeclined);
  EXPECT_FALSE(controller().is_loading());
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPermissionUiStatus::kDeclined, 1);
}

// Tests that user interaction on the WebContents (clicking outside) while the
// prompt is showing hides the view and resolves callback with kUserAborted.
TEST_F(EmailVerificationPopupControllerTest,
       UserInteractionDismissesWhenNotLoading) {
  base::HistogramTester histogram_tester;
  auto mock_view = SetupMockViewFactory();

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().Show(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                    future.GetCallback());

  EXPECT_CALL(*mock_view, Hide);
  blink::WebMouseEvent mouse_event;
  controller().DidGetUserInteraction(mouse_event);

  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), EmailVerificationPermissionUiStatus::kUserAborted);
  EXPECT_FALSE(controller().is_loading());
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPermissionUiStatus::kUserAborted, 1);
}

// Tests that user interaction on the WebContents while the prompt is loading
// is ignored so that the spinner remains visible.
TEST_F(EmailVerificationPopupControllerTest,
       UserInteractionIgnoredWhenLoading) {
  auto mock_view = SetupMockViewFactory();

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().Show(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                    future.GetCallback());

  EXPECT_CALL(*mock_view, ShowLoadingState);
  mock_view->RunDecisionCallback(true);
  EXPECT_TRUE(controller().is_loading());

  EXPECT_CALL(*mock_view, Hide).Times(0);
  blink::WebMouseEvent mouse_event;
  controller().DidGetUserInteraction(mouse_event);
  EXPECT_TRUE(controller().is_loading());

  testing::Mock::VerifyAndClearExpectations(mock_view.get());
  EXPECT_CALL(*mock_view, Hide);
  controller().Dismiss();
}

// Tests that user interaction when no view is open is a safe no-op.
TEST_F(EmailVerificationPopupControllerTest, UserInteractionIgnoredWhenNoView) {
  base::HistogramTester histogram_tester;
  blink::WebMouseEvent mouse_event;
  controller().DidGetUserInteraction(mouse_event);
  EXPECT_FALSE(controller().is_loading());
  histogram_tester.ExpectTotalCount("Blink.Evp.PermissionUi.Status", 0);
}

// Tests mapping of SuggestionHidingReason to
// EmailVerificationPermissionUiStatus when the popup is not loading.
TEST_F(EmailVerificationPopupControllerTest, HideReasonsMappingWhenNotLoading) {
  const struct {
    SuggestionHidingReason reason;
    EmailVerificationPermissionUiStatus expected_status;
  } kTestCases[] = {
      {SuggestionHidingReason::kUserAborted,
       EmailVerificationPermissionUiStatus::kUserAborted},
      {SuggestionHidingReason::kFocusChanged,
       EmailVerificationPermissionUiStatus::kUserAborted},
      {SuggestionHidingReason::kEndEditing,
       EmailVerificationPermissionUiStatus::kUserAborted},
      {SuggestionHidingReason::kTabGone,
       EmailVerificationPermissionUiStatus::kTabGone},
      {SuggestionHidingReason::kWidgetChanged,
       EmailVerificationPermissionUiStatus::kWidgetChanged},
      {SuggestionHidingReason::kOverlappingWithAnotherPrompt,
       EmailVerificationPermissionUiStatus::kOverlappingPrompt},
      {SuggestionHidingReason::kOverlappingWithPictureInPictureWindow,
       EmailVerificationPermissionUiStatus::kOverlappingPrompt},
      {SuggestionHidingReason::kContextMenuOpened,
       EmailVerificationPermissionUiStatus::kOverlappingPrompt},
      {SuggestionHidingReason::kContentAreaMoved,
       EmailVerificationPermissionUiStatus::kOther},
      {SuggestionHidingReason::kRendererEvent,
       EmailVerificationPermissionUiStatus::kOther},
      {SuggestionHidingReason::kInsufficientSpace,
       EmailVerificationPermissionUiStatus::kOther},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Reason: " << static_cast<int>(test_case.reason));
    base::HistogramTester histogram_tester;
    auto mock_view = SetupMockViewFactory();

    TestFuture<EmailVerificationPermissionUiStatus> future;
    controller().Show(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                      future.GetCallback());

    EXPECT_CALL(*mock_view, Hide);
    controller().Hide(test_case.reason);

    ASSERT_TRUE(future.IsReady());
    EXPECT_EQ(future.Get(), test_case.expected_status);
    histogram_tester.ExpectUniqueSample("Blink.Evp.PermissionUi.Status",
                                        test_case.expected_status, 1);
  }
}

// Tests that transient hide reasons are suppressed while in the loading state.
TEST_F(EmailVerificationPopupControllerTest,
       LoadingStateSuppressesTransientHideReasons) {
  const SuggestionHidingReason kTransientReasons[] = {
      SuggestionHidingReason::kUserAborted,
      SuggestionHidingReason::kFocusChanged,
      SuggestionHidingReason::kEndEditing,
      SuggestionHidingReason::kWidgetChanged,
      SuggestionHidingReason::kContentAreaMoved,
      SuggestionHidingReason::kElementOutsideOfContentArea,
      SuggestionHidingReason::kRendererEvent,
      SuggestionHidingReason::kInsufficientSpace,
      SuggestionHidingReason::kOverlappingWithAnotherPrompt,
      SuggestionHidingReason::kOverlappingWithPictureInPictureWindow,
      SuggestionHidingReason::kContextMenuOpened,
  };

  auto mock_view = SetupMockViewFactory();

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().Show(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                    future.GetCallback());

  EXPECT_CALL(*mock_view, ShowLoadingState);
  EXPECT_CALL(*mock_view, Hide).Times(0);
  mock_view->RunDecisionCallback(true);
  EXPECT_TRUE(controller().is_loading());

  for (SuggestionHidingReason reason : kTransientReasons) {
    controller().Hide(reason);
    EXPECT_TRUE(controller().is_loading());
  }

  // Explicit dismissal closes the view.
  testing::Mock::VerifyAndClearExpectations(mock_view.get());
  EXPECT_CALL(*mock_view, Hide);
  controller().Dismiss();
  EXPECT_FALSE(controller().is_loading());
}

// Tests that fatal hide reasons (e.g. kTabGone, kAttachInterstitialPage,
// kViewDestroyed) are NOT suppressed even while in the loading state.
TEST_F(EmailVerificationPopupControllerTest,
       LoadingStateDoesNotSuppressFatalHideReasons) {
  const SuggestionHidingReason kFatalReasons[] = {
      SuggestionHidingReason::kTabGone,
      SuggestionHidingReason::kAttachInterstitialPage,
      SuggestionHidingReason::kViewDestroyed,
  };

  for (SuggestionHidingReason reason : kFatalReasons) {
    SCOPED_TRACE(testing::Message()
                 << "Fatal Reason: " << static_cast<int>(reason));
    auto mock_view = SetupMockViewFactory();

    TestFuture<EmailVerificationPermissionUiStatus> future;
    controller().Show(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                      future.GetCallback());

    EXPECT_CALL(*mock_view, ShowLoadingState);
    mock_view->RunDecisionCallback(true);
    EXPECT_EQ(future.Get(), EmailVerificationPermissionUiStatus::kAllowed);
    EXPECT_TRUE(controller().is_loading());

    EXPECT_CALL(*mock_view, Hide);
    controller().Hide(reason);
    EXPECT_FALSE(controller().is_loading());
  }
}

// Tests that calling Dismiss while a popup is showing resolves callback with
// kOther and hides the view.
TEST_F(EmailVerificationPopupControllerTest, DismissWhileShowingResolvesOther) {
  base::HistogramTester histogram_tester;
  auto mock_view = SetupMockViewFactory();

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().Show(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                    future.GetCallback());

  EXPECT_CALL(*mock_view, Hide);
  controller().Dismiss();

  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), EmailVerificationPermissionUiStatus::kOther);
  EXPECT_FALSE(controller().is_loading());
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPermissionUiStatus::kOther, 1);
}

// Tests that calling Dismiss while a popup is loading hides the view and resets
// is_loading to false.
TEST_F(EmailVerificationPopupControllerTest, DismissWhileLoadingResetsLoading) {
  auto mock_view = SetupMockViewFactory();

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().Show(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                    future.GetCallback());

  EXPECT_CALL(*mock_view, ShowLoadingState);
  mock_view->RunDecisionCallback(true);
  EXPECT_EQ(future.Get(), EmailVerificationPermissionUiStatus::kAllowed);
  EXPECT_TRUE(controller().is_loading());

  EXPECT_CALL(*mock_view, Hide);
  controller().Dismiss();
  EXPECT_FALSE(controller().is_loading());
}

// Tests that calling Dismiss when no popup is open is a safe no-op.
TEST_F(EmailVerificationPopupControllerTest, DismissWhenClosedIsNoOp) {
  controller().Dismiss();
  EXPECT_FALSE(controller().is_loading());
}

// Tests that calling Show while a previous popup is open resolves the first
// popup's callback with kOther, hides the first view, and displays the new
// popup.
TEST_F(EmailVerificationPopupControllerTest,
       OverlappingShowWhileShowingReplacesPopup) {
  base::HistogramTester histogram_tester;
  auto first_view = SetupMockViewFactory();

  TestFuture<EmailVerificationPermissionUiStatus> first_future;
  controller().Show(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                    first_future.GetCallback());

  auto second_view = SetupMockViewFactory();
  EXPECT_CALL(*first_view, Hide);

  TestFuture<EmailVerificationPermissionUiStatus> second_future;
  controller().Show(gfx::RectF(20, 20, 10, 10), DefaultOrigin(),
                    u"second@example.com", second_future.GetCallback());
  EXPECT_EQ(controller().element_bounds(), gfx::RectF(20, 20, 10, 10));

  ASSERT_TRUE(first_future.IsReady());
  EXPECT_EQ(first_future.Get(), EmailVerificationPermissionUiStatus::kOther);
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPermissionUiStatus::kOther, 1);
  EXPECT_FALSE(second_future.IsReady());

  EXPECT_CALL(*second_view, Hide);
  controller().Dismiss();
  EXPECT_TRUE(second_future.IsReady());
  EXPECT_EQ(second_future.Get(), EmailVerificationPermissionUiStatus::kOther);
}

// Tests that calling Show while a popup is actively in the loading state
// rejects the overlapping request with kOverlappingPrompt and preserves the
// active loading popup.
TEST_F(EmailVerificationPopupControllerTest,
       OverlappingShowWhileLoadingRejected) {
  base::HistogramTester histogram_tester;
  auto mock_view = SetupMockViewFactory();

  TestFuture<EmailVerificationPermissionUiStatus> first_future;
  controller().Show(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                    first_future.GetCallback());

  EXPECT_CALL(*mock_view, ShowLoadingState);
  mock_view->RunDecisionCallback(true);
  EXPECT_EQ(first_future.Get(), EmailVerificationPermissionUiStatus::kAllowed);
  EXPECT_TRUE(controller().is_loading());

  // Attempt overlapping Show while still loading.
  EXPECT_CALL(*mock_view, Hide).Times(0);
  TestFuture<EmailVerificationPermissionUiStatus> second_future;
  controller().Show(gfx::RectF(20, 20, 10, 10), DefaultOrigin(),
                    u"second@example.com", second_future.GetCallback());

  EXPECT_EQ(second_future.Get(),
            EmailVerificationPermissionUiStatus::kOverlappingPrompt);
  EXPECT_TRUE(controller().is_loading());
  EXPECT_EQ(controller().element_bounds(), kDefaultBounds);
  histogram_tester.ExpectBucketCount(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPermissionUiStatus::kOverlappingPrompt, 1);

  // Clean up first popup.
  testing::Mock::VerifyAndClearExpectations(mock_view.get());
  EXPECT_CALL(*mock_view, Hide);
  controller().Dismiss();
}

// Tests that if ViewDestroyed is called directly, the callback is resolved with
// kViewDestroyedDirectly.
TEST_F(EmailVerificationPopupControllerTest,
       ViewDestroyedDirectlyResolvesCallback) {
  base::HistogramTester histogram_tester;
  auto mock_view = SetupMockViewFactory();

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().Show(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                    future.GetCallback());

  controller().ViewDestroyed();
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(),
            EmailVerificationPermissionUiStatus::kViewDestroyedDirectly);
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPermissionUiStatus::kViewDestroyedDirectly, 1);
}

// Tests that calling ViewDestroyed while a popup is loading resets is_loading
// to false and ensures subsequent Dismiss calls are safe no-ops.
TEST_F(EmailVerificationPopupControllerTest,
       ViewDestroyedWhileLoadingResetsLoading) {
  auto mock_view = SetupMockViewFactory();

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().Show(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                    future.GetCallback());

  EXPECT_CALL(*mock_view, ShowLoadingState);
  mock_view->RunDecisionCallback(true);
  EXPECT_EQ(future.Get(), EmailVerificationPermissionUiStatus::kAllowed);
  EXPECT_TRUE(controller().is_loading());

  // Since the native view is destroyed directly, Hide() is not called on the
  // view.
  EXPECT_CALL(*mock_view, Hide).Times(0);
  controller().ViewDestroyed();
  EXPECT_FALSE(controller().is_loading());

  // Subsequent dismissal is a safe no-op.
  controller().Dismiss();
  EXPECT_FALSE(controller().is_loading());
}

// Tests that destroying the controller while a popup is showing hides the view
// and resolves callback with kOther.
TEST_F(EmailVerificationPopupControllerTest,
       ControllerDestructionWhileShowingResolvesCallback) {
  base::HistogramTester histogram_tester;
  auto mock_view = SetupMockViewFactory();

  TestFuture<EmailVerificationPermissionUiStatus> future;
  controller().Show(kDefaultBounds, DefaultOrigin(), kDefaultEmail,
                    future.GetCallback());

  EXPECT_CALL(*mock_view, Hide);
  ResetController();

  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), EmailVerificationPermissionUiStatus::kOther);
  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPermissionUiStatus::kOther, 1);
}

}  // namespace
}  // namespace autofill
