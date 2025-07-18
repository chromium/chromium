// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/android_payments_window_manager.h"

#include <memory>
#include <string_view>

#include "base/test/mock_callback.h"
#include "chrome/browser/ui/autofill/payments/android_payments_window_manager_test_api.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_window_manager.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {

class TestContentAutofillClientForWindowManagerTest
    : public TestContentAutofillClient {
 public:
  explicit TestContentAutofillClientForWindowManagerTest(
      content::WebContents* web_contents)
      : TestContentAutofillClient(web_contents) {
    GetPaymentsAutofillClient()->set_payments_window_manager(
        std::make_unique<payments::AndroidPaymentsWindowManager>(this));
  }

  ~TestContentAutofillClientForWindowManagerTest() override = default;
};

namespace payments {

constexpr std::string_view kBnplInitialUrl = "https://www.bnplinitialurl.com/";
constexpr std::string_view kBnplSuccessUrlPrefix =
    "https://www.bnplsuccess.com/";
constexpr std::string_view kBnplFailureUrlPrefix =
    "https://www.bnplfailure.com/";

class AndroidPaymentsWindowManagerTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  TestContentAutofillClientForWindowManagerTest* client() {
    return test_autofill_client_injector_[web_contents()];
  }

  AndroidPaymentsWindowManager& window_manager() {
    return *static_cast<AndroidPaymentsWindowManager*>(
        client()->GetPaymentsAutofillClient()->GetPaymentsWindowManager());
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

  base::MockCallback<PaymentsWindowManager::OnBnplPopupClosedCallback>
      bnpl_tab_closed_callback_;
  base::HistogramTester histogram_tester_;

 private:
  TestAutofillClientInjector<TestContentAutofillClientForWindowManagerTest>
      test_autofill_client_injector_;
};

// Test that calling InitBnplFlow() correctly sets the internal state of the
// window manager.
TEST_F(AndroidPaymentsWindowManagerTest, InitBnplFlow) {
  // The flow should not be ongoing initially.
  EXPECT_TRUE(test_api(window_manager()).NoOngoingFlow());

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

// Test that when the web contents are destroyed during a BNPL flow after a
// successful navigation, the correct result is propagated and metrics are
// logged.
TEST_F(AndroidPaymentsWindowManagerTest, WebContentsDestroyed_Bnpl_Success) {
  InitBnplFlowForTest();

  // Simulate navigation to the success URL.
  const GURL success_url =
      GURL(std::string(kBnplSuccessUrlPrefix) + "?status=success");
  window_manager().OnDidFinishNavigationForBnpl(success_url);
  EXPECT_EQ(test_api(window_manager()).GetMostRecentUrlNavigation(),
            success_url);

  EXPECT_CALL(
      bnpl_tab_closed_callback_,
      Run(PaymentsWindowManager::BnplFlowResult::kSuccess, success_url));

  // Simulate destruction of the tab.
  window_manager().WebContentsDestroyed();

  // Verify metrics and state reset.
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Bnpl.PopupWindowResult.Affirm",
      PaymentsWindowManager::BnplFlowResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Bnpl.PopupWindowLatency.Affirm.Success", 1);
  EXPECT_TRUE(test_api(window_manager()).NoOngoingFlow());
}

// Test that when the web contents are destroyed during a BNPL flow after a
// failed navigation, the correct result is propagated and metrics are logged.
TEST_F(AndroidPaymentsWindowManagerTest, WebContentsDestroyed_Bnpl_Failure) {
  InitBnplFlowForTest();

  // Simulate navigation to the failure URL.
  const GURL failure_url =
      GURL(std::string(kBnplFailureUrlPrefix) + "?status=failure");
  window_manager().OnDidFinishNavigationForBnpl(failure_url);
  EXPECT_EQ(test_api(window_manager()).GetMostRecentUrlNavigation(),
            failure_url);

  EXPECT_CALL(
      bnpl_tab_closed_callback_,
      Run(PaymentsWindowManager::BnplFlowResult::kFailure, failure_url));

  // Simulate destruction of the tab.
  window_manager().WebContentsDestroyed();

  // Verify metrics and state reset.
  histogram_tester_.ExpectUniqueSample(
      "Autofill.Bnpl.PopupWindowResult.Affirm",
      PaymentsWindowManager::BnplFlowResult::kFailure, 1);
  histogram_tester_.ExpectTotalCount(
      "Autofill.Bnpl.PopupWindowLatency.Affirm.Failure", 1);
  EXPECT_TRUE(test_api(window_manager()).NoOngoingFlow());
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

}  // namespace payments

}  // namespace autofill
