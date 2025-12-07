// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/android_payments_window_manager.h"

#include <memory>
#include <string_view>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/touch_to_fill/autofill/android/mock_touch_to_fill_payment_method_controller.h"
#include "chrome/browser/ui/android/autofill/payments/payments_window_bridge.h"
#include "chrome/browser/ui/android/autofill/payments/payments_window_delegate.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/payments/android_payments_window_manager.h"
#include "chrome/browser/ui/autofill/payments/android_payments_window_manager_test_api.h"
#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/payments/payments_window_manager.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {
namespace payments {

class MockPaymentsWindowDelegate : public PaymentsWindowDelegate {
 public:
  MockPaymentsWindowDelegate() = default;

  MOCK_METHOD(void, OnDidFinishNavigationForBnpl, (const GURL&), (override));
  MOCK_METHOD(void,
              OnWebContentsObservationStarted,
              (content::WebContents&),
              (override));
  MOCK_METHOD(void, WebContentsDestroyed, (), (override));
};

class MockPaymentsWindowBridge : public PaymentsWindowBridge {
 public:
  explicit MockPaymentsWindowBridge(PaymentsWindowDelegate* delegate)
      : PaymentsWindowBridge(delegate) {}

  MOCK_METHOD(void,
              OpenEphemeralTab,
              (const GURL&, const std::u16string&, content::WebContents&),
              (override));
  MOCK_METHOD(void, CloseEphemeralTab, (), (override));
};

constexpr std::string_view kBnplInitialUrl = "https://www.bnplinitialurl.com/";
constexpr std::string_view kBnplSuccessUrlPrefix =
    "https://www.bnplsuccess.com/";
constexpr std::string_view kBnplFailureUrlPrefix =
    "https://www.bnplfailure.com/";
constexpr std::string_view kBnplUnknownUrl = "https://www.bnplunknown.com/";

class AndroidPaymentsWindowManagerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ChromeAutofillClient::CreateForWebContents(web_contents());
  }

 protected:
  AndroidPaymentsWindowManager& window_manager() {
    return *static_cast<AndroidPaymentsWindowManager*>(
        chrome_payments_client()->GetPaymentsWindowManager());
  }

  ChromePaymentsAutofillClient* chrome_payments_client() {
    return static_cast<ChromePaymentsAutofillClient*>(
        ChromeAutofillClient::FromWebContents(web_contents())
            ->GetPaymentsAutofillClient());
  }

  void InitBnplFlowForTest() {
    PaymentsWindowManager::BnplContext context;
    context.issuer_id = BnplIssuer::IssuerId::kBnplAffirm;
    context.success_url_prefix = GURL(kBnplSuccessUrlPrefix);
    context.failure_url_prefix = GURL(kBnplFailureUrlPrefix);
    context.initial_url = GURL(kBnplInitialUrl);
    context.completion_callback = bnpl_tab_closed_callback_.Get();
    window_manager().InitBnplFlow(std::move(context));
  }

  void SetUpMockPaymentsWindowBridge() {
    MockPaymentsWindowDelegate mock_delegate;
    std::unique_ptr<MockPaymentsWindowBridge> payments_window_bridge_ptr =
        std::make_unique<MockPaymentsWindowBridge>(&mock_delegate);
    test_api(window_manager())
        .SetPaymentsWindowBridge(std::move(payments_window_bridge_ptr));
  }

  MockTouchToFillPaymentMethodController*
  InjectMockTouchToFillPaymentMethodController() {
    std::unique_ptr<MockTouchToFillPaymentMethodController> mock =
        std::make_unique<MockTouchToFillPaymentMethodController>();
    MockTouchToFillPaymentMethodController* pointer = mock.get();
    chrome_payments_client()->SetTouchToFillPaymentMethodControllerForTesting(
        std::move(mock));
    return pointer;
  }

  base::MockCallback<PaymentsWindowManager::OnBnplPopupClosedCallback>
      bnpl_tab_closed_callback_;
  base::HistogramTester histogram_tester_;
};

// Test that calling InitBnplFlow() correctly sets the internal state of the
// window manager.
TEST_F(AndroidPaymentsWindowManagerTest, InitBnplFlow) {
  // The flow should not be ongoing initially.
  EXPECT_TRUE(test_api(window_manager()).NoOngoingFlow());
  SetUpMockPaymentsWindowBridge();
  EXPECT_CALL(static_cast<MockPaymentsWindowBridge&>(
                  test_api(window_manager()).GetPaymentsWindowBridge()),
              OpenEphemeralTab(
                  GURL(kBnplInitialUrl),
                  BnplIssuerIdToDisplayName(BnplIssuer::IssuerId::kBnplAffirm),
                  testing::A<content::WebContents&>()))
      .Times(1);

  InitBnplFlowForTest();

  // After initialization, a BNPL flow should be ongoing.
  EXPECT_FALSE(test_api(window_manager()).NoOngoingFlow());
  ASSERT_TRUE(test_api(window_manager()).GetBnplContext().has_value());
  EXPECT_EQ(test_api(window_manager()).GetBnplContext()->initial_url,
            GURL(kBnplInitialUrl));
  EXPECT_EQ(test_api(window_manager()).GetBnplContext()->success_url_prefix,
            GURL(kBnplSuccessUrlPrefix));
  EXPECT_EQ(test_api(window_manager()).GetBnplContext()->failure_url_prefix,
            GURL(kBnplFailureUrlPrefix));
  histogram_tester_.ExpectUniqueSample("Autofill.Bnpl.PopupWindowShown.Affirm",
                                       true, 1);
}

// Test that OnWebContentsObservationStarted disables payments autofill.
TEST_F(AndroidPaymentsWindowManagerTest,
       OnWebContentsObservationStarted_DisablesPaymentsAutofill) {
  EXPECT_TRUE(chrome_payments_client()->IsAutofillPaymentMethodsEnabled());

  window_manager().OnWebContentsObservationStarted(*web_contents());

  EXPECT_FALSE(chrome_payments_client()->IsAutofillPaymentMethodsEnabled());
}

// Test that destroying web contents after the flow state has already been reset
// does not cause a crash.
TEST_F(AndroidPaymentsWindowManagerTest, WebContentsDestroyed_NoOngoingFlow) {
  EXPECT_TRUE(test_api(window_manager()).NoOngoingFlow());

  window_manager().WebContentsDestroyed();
}

// Test that when the web contents are destroyed during a BNPL flow before the
// flow completes, the correct result is propagated and metrics are logged.
TEST_F(AndroidPaymentsWindowManagerTest, WebContentsDestroyed_Bnpl_UserClosed) {
  InitBnplFlowForTest();

  // Simulate navigation to some intermediate URL.
  const GURL intermediate_url("https://www.bnpl.com/intermediate-step");
  window_manager().OnDidFinishNavigationForBnpl(intermediate_url);
  EXPECT_EQ(test_api(window_manager()).GetMostRecentUrlNavigation(),
            intermediate_url);

  EXPECT_CALL(bnpl_tab_closed_callback_,
              Run(PaymentsWindowManager::BnplFlowResult::kUserClosed,
                  intermediate_url));

  // Simulate destruction of the tab.
  window_manager().WebContentsDestroyed();

  // Verify metrics and state reset.
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Bnpl.PopupWindowResult.Affirm",
      PaymentsWindowManager::BnplFlowResult::kUserClosed, 1);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Bnpl.PopupWindowLatency.Affirm.UserClosed", 1);
  EXPECT_TRUE(test_api(window_manager()).NoOngoingFlow());
}

// Test that if the tab is closed by the user before any navigation
// completes, it's handled as a user cancellation.
TEST_F(AndroidPaymentsWindowManagerTest,
       WebContentsDestroyed_Bnpl_UserClosed_NoNavigation) {
  InitBnplFlowForTest();

  // No navigation occurs. Before any navigation,
  // `most_recent_url_navigation` is empty. The completion callback will
  // receive an empty GURL.
  EXPECT_TRUE(
      test_api(window_manager()).GetMostRecentUrlNavigation().is_empty());

  // Expect the completion callback to be called with a user-closed result.
  // Since no navigation finished, the URL passed should be empty.
  EXPECT_CALL(bnpl_tab_closed_callback_,
              Run(PaymentsWindowManager::BnplFlowResult::kUserClosed, GURL()));

  // Simulate destruction of the tab.
  window_manager().WebContentsDestroyed();

  // Verify metrics and state reset.
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Bnpl.PopupWindowResult.Affirm",
      PaymentsWindowManager::BnplFlowResult::kUserClosed, 1);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Bnpl.PopupWindowLatency.Affirm.UserClosed", 1);
  EXPECT_TRUE(test_api(window_manager()).NoOngoingFlow());
}

// Test that if the tab is closed by the user during a BNPL flow before the
// flow completes, we clean up any leftover bottom sheet UI state in the
// TouchToFillPaymentMethodController.
TEST_F(AndroidPaymentsWindowManagerTest,
       WebContentsDestroyed_Bnpl_UserClosed_ClearsTouchToFillState) {
  MockTouchToFillPaymentMethodController* mock_controller =
      InjectMockTouchToFillPaymentMethodController();
  InitBnplFlowForTest();

  // Simulate navigation to some intermediate URL.
  const GURL intermediate_url("https://www.bnpl.com/intermediate-step");
  window_manager().OnDidFinishNavigationForBnpl(intermediate_url);
  EXPECT_EQ(test_api(window_manager()).GetMostRecentUrlNavigation(),
            intermediate_url);

  EXPECT_CALL(*mock_controller,
              OnDismissed(testing::IsNull(), /*dismissed_by_user=*/true,
                          /*should_reshow=*/true));

  // Simulate destruction of the tab.
  window_manager().WebContentsDestroyed();
}

// Test that if the tab is closed by the user before any navigation
// completes, we clean up any leftover bottom sheet UI state in the
// TouchToFillPaymentMethodController.
TEST_F(
    AndroidPaymentsWindowManagerTest,
    WebContentsDestroyed_Bnpl_UserClosed_NoNavigation_ClearsTouchToFillState) {
  MockTouchToFillPaymentMethodController* mock_controller =
      InjectMockTouchToFillPaymentMethodController();
  InitBnplFlowForTest();

  // No navigation occurs. Before any navigation,
  // `most_recent_url_navigation` is empty.
  EXPECT_TRUE(
      test_api(window_manager()).GetMostRecentUrlNavigation().is_empty());

  EXPECT_CALL(*mock_controller,
              OnDismissed(testing::IsNull(), /*dismissed_by_user=*/true,
                          /*should_reshow=*/true));

  // Simulate destruction of the tab.
  window_manager().WebContentsDestroyed();
}

TEST_F(AndroidPaymentsWindowManagerTest,
       OnDidFinishNavigationForBnpl_WhenSuccessUrl_ClosesTab) {
  SetUpMockPaymentsWindowBridge();
  InitBnplFlowForTest();
  EXPECT_CALL(static_cast<MockPaymentsWindowBridge&>(
                  test_api(window_manager()).GetPaymentsWindowBridge()),
              CloseEphemeralTab())
      .Times(1);

  window_manager().OnDidFinishNavigationForBnpl(
      GURL(std::string(kBnplSuccessUrlPrefix) + "?status=success"));
}

// Test that after navigating to a success URL, the completion callback is
// triggered, metrics are logged, and the flow state is reset.
TEST_F(
    AndroidPaymentsWindowManagerTest,
    OnDidFinishNavigationForBnpl_WhenSuccessUrl_TriggerCompletionCallbackAndLogMetrics) {
  InitBnplFlowForTest();
  const GURL success_url =
      GURL(std::string(kBnplSuccessUrlPrefix) + "?status=success");

  EXPECT_CALL(
      bnpl_tab_closed_callback_,
      Run(PaymentsWindowManager::BnplFlowResult::kSuccess, success_url));

  // Simulate navigation to the success URL.
  window_manager().OnDidFinishNavigationForBnpl(success_url);

  // Verify metrics and state reset.
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Bnpl.PopupWindowResult.Affirm",
      PaymentsWindowManager::BnplFlowResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Bnpl.PopupWindowLatency.Affirm.Success", 1);
  EXPECT_TRUE(test_api(window_manager()).NoOngoingFlow());
}

TEST_F(AndroidPaymentsWindowManagerTest,
       OnDidFinishNavigationForBnpl_WhenFailureUrl_ClosesTab) {
  SetUpMockPaymentsWindowBridge();
  InitBnplFlowForTest();
  EXPECT_CALL(static_cast<MockPaymentsWindowBridge&>(
                  test_api(window_manager()).GetPaymentsWindowBridge()),
              CloseEphemeralTab())
      .Times(1);

  window_manager().OnDidFinishNavigationForBnpl(
      GURL(std::string(kBnplFailureUrlPrefix) + "?status=failure"));
}

// Test that after navigating to a failure URL, the completion callback is
// triggered, metrics are logged, and the flow state is reset.
TEST_F(
    AndroidPaymentsWindowManagerTest,
    OnDidFinishNavigationForBnpl_WhenFailureUrl_TriggerCompletionCallbackAndLogMetrics) {
  InitBnplFlowForTest();
  const GURL failure_url =
      GURL(std::string(kBnplFailureUrlPrefix) + "?status=failure");

  EXPECT_CALL(
      bnpl_tab_closed_callback_,
      Run(PaymentsWindowManager::BnplFlowResult::kFailure, failure_url));

  // Simulate navigation to the failure URL.
  window_manager().OnDidFinishNavigationForBnpl(failure_url);

  // Verify metrics and state reset.
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Bnpl.PopupWindowResult.Affirm",
      PaymentsWindowManager::BnplFlowResult::kFailure, 1);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Bnpl.PopupWindowLatency.Affirm.Failure", 1);
  EXPECT_TRUE(test_api(window_manager()).NoOngoingFlow());
}

TEST_F(AndroidPaymentsWindowManagerTest,
       OnDidFinishNavigationForBnpl_WhenNotFinished_DoesNotCloseTab) {
  SetUpMockPaymentsWindowBridge();
  InitBnplFlowForTest();
  EXPECT_CALL(static_cast<MockPaymentsWindowBridge&>(
                  test_api(window_manager()).GetPaymentsWindowBridge()),
              CloseEphemeralTab())
      .Times(0);

  window_manager().OnDidFinishNavigationForBnpl(
      GURL(std::string(kBnplUnknownUrl)));
}

// Test that if `OnDidFinishNavigationForBnpl` is called after the flow has
// already finished (e.g., due to a late JS redirect happening after a success
// URL), the manager returns early. It should not crash, trigger callbacks
// twice, or log duplicate metrics.
TEST_F(
    AndroidPaymentsWindowManagerTest,
    OnDidFinishNavigationForBnpl_WhenFlowFinished_IgnoresSubsequentNavigation) {
  InitBnplFlowForTest();
  const GURL success_url =
      GURL(std::string(kBnplSuccessUrlPrefix) + "?status=success");

  EXPECT_CALL(bnpl_tab_closed_callback_,
              Run(PaymentsWindowManager::BnplFlowResult::kSuccess, success_url))
      .Times(1);

  // Simulate navigation to the success URL.
  window_manager().OnDidFinishNavigationForBnpl(success_url);

  // Verify metrics and state reset.
  EXPECT_TRUE(test_api(window_manager()).NoOngoingFlow());

  // Simulate the "race condition": An extra navigation event triggers
  // immediately after the first one (e.g., a late redirect).
  const GURL late_redirect_url("https://www.example.com/late-redirect");
  window_manager().OnDidFinishNavigationForBnpl(late_redirect_url);

  EXPECT_TRUE(test_api(window_manager()).NoOngoingFlow());
}

}  // namespace payments

}  // namespace autofill
