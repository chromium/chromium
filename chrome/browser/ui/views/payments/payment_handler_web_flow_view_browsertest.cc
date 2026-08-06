// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/payments/payment_handler_web_flow_view_controller.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_test_api.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/payments/content/payment_request_state.h"
#include "components/payments/core/features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

using PaymentHandlerWebFlowViewTest = PaymentRequestBrowserTestBase;

class TestClient : public ChromeContentBrowserClient {
 public:
  void CreateThrottlesForNavigation(
      content::NavigationThrottleRegistry& registry) override {
    ChromeContentBrowserClient::CreateThrottlesForNavigation(registry);
    content::NavigationHandle& handle = registry.GetNavigationHandle();
    if (handle.GetURL().DomainIs(url_to_intercept)) {
      saw_navigation_ = true;
      initiator_origin_ = handle.GetInitiatorOrigin();
    }
  }

  std::string url_to_intercept;
  bool saw_navigation_ = false;
  std::optional<url::Origin> initiator_origin_;
};

// Test that the content view itself is not in a ScrollView, as the web view
// should be a static size that is itself scrollable.
IN_PROC_BROWSER_TEST_F(PaymentHandlerWebFlowViewTest,
                       ContentViewNotScrollable) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  // Trigger PaymentRequest, and wait until the PaymentHandler has loaded a
  // web-contents that has set a title.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ASSERT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("launchWithoutWaitForResponse($1)", method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  // We always push the initial browser sheet to the stack, even if it isn't
  // shown. Since it also defines a CONTENT_VIEW, we have to explicitly test the
  // front PaymentHandler view here.
  views::View* top_view = test_api(dialog_view()).view_stack()->top();

  views::View* sheet_view = GetChildByDialogViewID(
      top_view, DialogViewID::PAYMENT_APP_OPENED_WINDOW_SHEET);
  // The content view should be within the sheet view.
  EXPECT_NE(nullptr,
            GetChildByDialogViewID(sheet_view, DialogViewID::CONTENT_VIEW));

  // There should be no scroll view.
  EXPECT_EQ(nullptr, GetChildByDialogViewID(
                         top_view, DialogViewID::PAYMENT_SHEET_SCROLL_VIEW));
}

class PaymentHandlerWebFlowViewUseInitiatorInUrlLoadEnabledTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentHandlerWebFlowViewUseInitiatorInUrlLoadEnabledTest() {
    feature_list_.InitAndEnableFeature(
        payments::features::kPaymentHandlerDialogUseInitiatorInUrlLoad);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    PaymentHandlerWebFlowViewUseInitiatorInUrlLoadEnabledTest,
    InitiatorOriginSet) {
  const std::string kPaymentAppHost = "a.com";
  TestClient test_client;
  test_client.url_to_intercept = kPaymentAppHost;
  content::ScopedContentBrowserClientSetting scoped_setting(&test_client);

  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp(kPaymentAppHost, "/payment_handler_sw.js", &method_name);

  // Trigger PaymentRequest.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET});

  ASSERT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("launchWithoutWaitForResponse($1)", method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  EXPECT_TRUE(test_client.saw_navigation_);
  ASSERT_TRUE(test_client.initiator_origin_.has_value());
  EXPECT_EQ(url::Origin::Create(GetActiveWebContents()->GetLastCommittedURL()),
            test_client.initiator_origin_.value());
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerWebFlowViewTest, UserInteractionRecorded) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  // Trigger PaymentRequest.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ASSERT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("launchWithoutWaitForResponse($1)", method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  // Check that user interaction has not been recorded yet.
  auto payment_requests = GetPaymentRequests();
  ASSERT_EQ(1u, payment_requests.size());
  PaymentRequestState* request_state = payment_requests[0]->state().get();
  ASSERT_NE(nullptr, request_state);
  EXPECT_FALSE(request_state->user_interaction_in_web_payment_app());

  // Get the payment handler web contents.
  views::View* top_view = test_api(dialog_view()).view_stack()->top();
  auto* sheet_controller =
      test_api(dialog_view()).controller_map()->at(top_view).get();
  auto* web_flow_controller =
      static_cast<PaymentHandlerWebFlowViewController*>(sheet_controller);
  content::WebContents* payment_handler_contents =
      web_flow_controller->web_contents();
  ASSERT_NE(nullptr, payment_handler_contents);

  // Simulate click on the page. Note that input events to a page may not work
  // right after a page load because of paint holding.
  content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(
      payment_handler_contents);
  content::SimulateMouseClick(payment_handler_contents, /*modifiers=*/0,
                              blink::WebMouseEvent::Button::kLeft);

  EXPECT_TRUE(request_state->user_interaction_in_web_payment_app());
}

// Action to perform after the payment handler calls window.close(). The API
// is expected to ignore window.close() and rely on below action to close the
// payment dialog.
enum class PostWindowCloseAction {
  kCloseTab,
  kCompletePayment,
  kRejectPayment,
};

struct WindowCloseTestParams {
  PostWindowCloseAction post_window_close_action;
  std::string test_name;
};

// Test suite that verifies the window.close() from JS will be ignored and
// PaymentHandler UI can handle different post-window-close actions correctly.
class PaymentHandlerWindowCloseTest
    : public PaymentRequestBrowserTestBase,
      public testing::WithParamInterface<WindowCloseTestParams> {
 protected:
  PaymentHandlerWindowCloseTest() { SetBypassUserInteractionForTesting(); }
};

IN_PROC_BROWSER_TEST_P(PaymentHandlerWindowCloseTest, WindowCloseIsIgnored) {
  const WindowCloseTestParams& params = GetParam();
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  // Trigger PaymentRequest, and wait until the PaymentHandler has loaded.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ASSERT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("launchWithoutWaitForResponse($1)", method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  // Get the controller of the top view on the stack.
  views::View* top_view = test_api(dialog_view()).view_stack()->top();
  auto* controller_map = test_api(dialog_view()).controller_map();
  auto it = controller_map->find(top_view);
  ASSERT_NE(it, controller_map->end());
  auto* controller =
      static_cast<PaymentHandlerWebFlowViewController*>(it->second.get());
  content::WebContents* payment_handler_web_contents =
      controller->web_contents();
  ASSERT_NE(nullptr, payment_handler_web_contents);

  // Wait until the PaymentHandler page finishes loading. There is a race
  // condition between the web page finish loading (i.e. the Service Worker
  // dispatching the confirmation message to the PaymentRequest dialog) and
  // below window.close() javascript execution. If window.close() is executed
  // before the Service Worker has dispatched the confirmation message, the
  // remaining html body script parsing and loading will be canceled, and the
  // PaymentRequest response promise will never be resolved.
  ASSERT_TRUE(content::WaitForLoadStop(payment_handler_web_contents));

  // Verify that window.close() is ignored.
  autofill::EventWaiter<PaymentRequestBrowserTestBase::DialogEvent> waiter(
      {PaymentRequestBrowserTestBase::DialogEvent::DIALOG_CLOSED},
      base::Seconds(2));
  ASSERT_TRUE(content::ExecJs(payment_handler_web_contents, "window.close();"));
  ASSERT_FALSE(waiter.Wait());

  // Verify that the payment handler sheet is still the top view (it did not
  // close).
  views::View* top_view_after_window_close =
      test_api(dialog_view()).view_stack()->top();
  EXPECT_EQ(top_view, top_view_after_window_close);

  switch (params.post_window_close_action) {
    case PostWindowCloseAction::kCloseTab: {
      // Verify that tab close can still close the payment dialog properly.
      ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
      chrome::CloseTab(browser());
      ASSERT_TRUE(WaitForObservedEvent());
      EXPECT_TRUE(GetPaymentRequests().empty());
      break;
    }
    case PostWindowCloseAction::kCompletePayment: {
      // Complete the payment request successfully on the merchant side.
      ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
      ASSERT_EQ("success", content::EvalJs(GetActiveWebContents(),
                                           "completeResponse('success')"));
      ASSERT_TRUE(WaitForObservedEvent());
      break;
    }
    case PostWindowCloseAction::kRejectPayment: {
      // Make reject payment request promise by clicking the cancel-button.
      ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_HIDDEN,
                                   DialogEvent::ERROR_MESSAGE_SHOWN});
      ASSERT_TRUE(content::ExecJs(GetActiveWebContents(),
                                  "completeResponse('fail')",
                                  content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
      ASSERT_TRUE(WaitForObservedEvent());
      break;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PaymentHandlerWindowCloseTest,
    // Verifies that window.close() is ignored using different closure methods:
    // - Tab Close: Verifies window.close() was ignored without causing resource
    //   leaks or state corruption, and that closing the tab still cleanly
    //   destroys the dialog.
    // - Complete / Reject Payment: If window.close() had taken effect, the
    //   dialog and underlying PaymentRequest would already be destroyed,
    //   causing these actions to fail. Completing/rejecting successfully proves
    //   the dialog stayed open.
    testing::Values(
        WindowCloseTestParams{
            .post_window_close_action = PostWindowCloseAction::kCloseTab,
            .test_name = "TabCloseCanCloseDialog"},
        WindowCloseTestParams{
            .post_window_close_action = PostWindowCloseAction::kCompletePayment,
            .test_name = "PaymentRequestCanBeCompleted"},
        WindowCloseTestParams{
            .post_window_close_action = PostWindowCloseAction::kRejectPayment,
            .test_name = "PaymentRequestCanBeRejected"}),
    [](const testing::TestParamInfo<WindowCloseTestParams>& info) {
      return info.param.test_name;
    });

class PaymentHandlerWebFlowViewMandatoryUiEnabledTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentHandlerWebFlowViewMandatoryUiEnabledTest() {
    feature_list_.InitAndEnableFeature(
        payments::features::kPaymentRequestMandatoryPaymentAppUi);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PaymentHandlerWebFlowViewMandatoryUiEnabledTest,
                       PaymentResponseDeferredUntilUserInteraction) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  // Trigger PaymentRequest.
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN,
       DialogEvent::PROCESSING_SPINNER_HIDDEN, DialogEvent::DIALOG_OPENED,
       DialogEvent::LOADING_VIEW_SHOWN,
       DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
       // Note: LOADING_VIEW_HIDDEN comes after PAYMENT_HANDLER_WINDOW_OPENED
       // because the loading view is hidden asynchronously.
       DialogEvent::LOADING_VIEW_HIDDEN,
       DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ASSERT_EQ("success",
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace("launchAndComplete($1)", method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  // Check that user interaction has not been recorded yet.
  auto payment_requests = GetPaymentRequests();
  ASSERT_EQ(1u, payment_requests.size());
  PaymentRequestState* request_state = payment_requests[0]->state().get();
  ASSERT_NE(nullptr, request_state);
  EXPECT_FALSE(request_state->user_interaction_in_web_payment_app());

  // Get the payment handler web contents.
  views::View* top_view = test_api(dialog_view()).view_stack()->top();
  auto* sheet_controller =
      test_api(dialog_view()).controller_map()->at(top_view).get();
  auto* web_flow_controller =
      static_cast<PaymentHandlerWebFlowViewController*>(sheet_controller);
  content::WebContents* payment_handler_contents =
      web_flow_controller->web_contents();

  // Wait for the payment to be processed.
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});

  // Simulate click on the page.
  content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(
      payment_handler_contents);
  content::SimulateMouseClick(payment_handler_contents, /*modifiers=*/0,
                              blink::WebMouseEvent::Button::kLeft);

  EXPECT_TRUE(request_state->user_interaction_in_web_payment_app());
  ASSERT_TRUE(WaitForObservedEvent());
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerWebFlowViewMandatoryUiEnabledTest,
                       DialogClosedOnErrorAfterUserInteraction) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com",
                    "/payment_handler_sw_error_after_user_interaction.js",
                    &method_name);

  // Trigger PaymentRequest.
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN,
       DialogEvent::PROCESSING_SPINNER_HIDDEN, DialogEvent::DIALOG_OPENED,
       DialogEvent::LOADING_VIEW_SHOWN,
       DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
       // Note: LOADING_VIEW_HIDDEN comes after PAYMENT_HANDLER_WINDOW_OPENED
       // because the loading view is hidden asynchronously.
       DialogEvent::LOADING_VIEW_HIDDEN,
       DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ASSERT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace("launchAndWaitUntilAppReady($1)",
                                               method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  // Get the payment handler web contents.
  views::View* top_view = test_api(dialog_view()).view_stack()->top();
  auto* sheet_controller =
      test_api(dialog_view()).controller_map()->at(top_view).get();
  auto* web_flow_controller =
      static_cast<PaymentHandlerWebFlowViewController*>(sheet_controller);
  content::WebContents* payment_handler_contents =
      web_flow_controller->web_contents();

  // Expect the dialog to close.
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

  // Reject the promise with a user interaction.
  content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(
      payment_handler_contents);
  content::SimulateMouseClickOrTapElementWithId(payment_handler_contents,
                                                "reject-button");

  ASSERT_TRUE(WaitForObservedEvent());
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerWebFlowViewMandatoryUiEnabledTest,
                       ErrorMessageShownOnErrorWithoutUserInteraction) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com",
                    "/payment_handler_sw_error_without_user_interaction.js",
                    &method_name);

  // Trigger PaymentRequest. We expect the error message sheet to be shown
  // because the app rejects the payment immediately before any user interaction
  // occurs.
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN,
       DialogEvent::PROCESSING_SPINNER_HIDDEN, DialogEvent::DIALOG_OPENED,
       DialogEvent::LOADING_VIEW_SHOWN,
       DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
       // Note: LOADING_VIEW_HIDDEN comes after PAYMENT_HANDLER_WINDOW_OPENED
       // because the loading view is hidden asynchronously.
       DialogEvent::LOADING_VIEW_HIDDEN, DialogEvent::PAYMENT_HANDLER_TITLE_SET,
       DialogEvent::PROCESSING_SPINNER_HIDDEN,
       DialogEvent::ERROR_MESSAGE_SHOWN});
  ASSERT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("launchWithoutWaitForResponse($1)", method_name)));
  ASSERT_TRUE(WaitForObservedEvent());
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerWebFlowViewMandatoryUiEnabledTest,
                       LoadingViewShownAndHiddenEvents) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  // Trigger PaymentRequest and verify that the loading view shown and hidden
  // events are observed when mandatory UI is enabled.
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN,
       DialogEvent::PROCESSING_SPINNER_HIDDEN, DialogEvent::DIALOG_OPENED,
       DialogEvent::LOADING_VIEW_SHOWN,
       DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
       // Note: LOADING_VIEW_HIDDEN comes after PAYMENT_HANDLER_WINDOW_OPENED
       // because the loading view is hidden asynchronously.
       DialogEvent::LOADING_VIEW_HIDDEN,
       DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ASSERT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("launchWithoutWaitForResponse($1)", method_name)));
  ASSERT_TRUE(WaitForObservedEvent());
}

class PaymentHandlerWebFlowViewMandatoryUiDisabledTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentHandlerWebFlowViewMandatoryUiDisabledTest() {
    feature_list_.InitAndDisableFeature(
        payments::features::kPaymentRequestMandatoryPaymentAppUi);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PaymentHandlerWebFlowViewMandatoryUiDisabledTest,
                       PaymentResponseCompletesImmediately) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  // Trigger PaymentRequest. We expect it to complete and close the dialog
  // automatically because the mandatory UI feature is disabled.
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN,
       DialogEvent::PROCESSING_SPINNER_HIDDEN, DialogEvent::DIALOG_OPENED,
       DialogEvent::PROCESSING_SPINNER_SHOWN,
       DialogEvent::PROCESSING_SPINNER_HIDDEN,
       DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
       DialogEvent::PAYMENT_HANDLER_TITLE_SET,
       DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ASSERT_EQ("success",
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace("launchAndComplete($1)", method_name)));
  ASSERT_TRUE(WaitForObservedEvent());
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerWebFlowViewMandatoryUiDisabledTest,
                       ErrorMessageShownOnErrorAfterUserInteraction) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com",
                    "/payment_handler_sw_error_after_user_interaction.js",
                    &method_name);

  // Trigger PaymentRequest.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ASSERT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace("launchAndWaitUntilAppReady($1)",
                                               method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  // Get the payment handler web contents.
  views::View* top_view = test_api(dialog_view()).view_stack()->top();
  auto* sheet_controller =
      test_api(dialog_view()).controller_map()->at(top_view).get();
  auto* web_flow_controller =
      static_cast<PaymentHandlerWebFlowViewController*>(sheet_controller);
  content::WebContents* payment_handler_contents =
      web_flow_controller->web_contents();

  // Expect error message screen is shown.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::ERROR_MESSAGE_SHOWN});

  // Reject the promise with a user interaction.
  content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(
      payment_handler_contents);
  content::SimulateMouseClickOrTapElementWithId(payment_handler_contents,
                                                "reject-button");

  ASSERT_TRUE(WaitForObservedEvent());
}

class PaymentHandlerWebFlowViewCameraTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentHandlerWebFlowViewCameraTest() {
    feature_list_.InitAndEnableFeature(features::kPaymentHandlerCameraAccess);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PaymentRequestBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PaymentHandlerWebFlowViewTest,
                       CameraAccessDisabledByDefault) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ASSERT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("launchWithoutWaitForResponse($1)", method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  views::View* top_view = test_api(dialog_view()).view_stack()->top();
  auto* sheet_controller =
      test_api(dialog_view()).controller_map()->at(top_view).get();
  auto* web_flow_controller =
      static_cast<PaymentHandlerWebFlowViewController*>(sheet_controller);
  content::WebContents* payment_handler_contents =
      web_flow_controller->web_contents();
  EXPECT_EQ(nullptr, OneTimePermissionsTrackerHelper::FromWebContents(
                         payment_handler_contents));

  std::string result = content::EvalJs(payment_handler_contents, R"(
    navigator.mediaDevices.getUserMedia({video: true})
      .then(() => 'allowed')
      .catch(err => err.name);
  )")
                           .ExtractString();
  EXPECT_EQ("NotSupportedError", result);
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerWebFlowViewCameraTest,
                       CameraAccessPreGrantedSuccess) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ASSERT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("launchWithoutWaitForResponse($1)", method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  views::View* top_view = test_api(dialog_view()).view_stack()->top();
  auto* sheet_controller =
      test_api(dialog_view()).controller_map()->at(top_view).get();
  auto* web_flow_controller =
      static_cast<PaymentHandlerWebFlowViewController*>(sheet_controller);
  content::WebContents* payment_handler_contents =
      web_flow_controller->web_contents();

  // Ensure that the Payment Handler window has installed a
  // OneTimePermissionsTrackerHelper on its webcontents, to support "Allow this
  // time" permissions from a nested pop-up window to persist through this
  // session.
  EXPECT_NE(nullptr, OneTimePermissionsTrackerHelper::FromWebContents(
                         payment_handler_contents));

  GURL payment_app_url = payment_handler_contents->GetLastCommittedURL();
  HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile())
      ->SetContentSettingDefaultScope(payment_app_url, payment_app_url,
                                      ContentSettingsType::MEDIASTREAM_CAMERA,
                                      CONTENT_SETTING_ALLOW);

  std::string result = content::EvalJs(payment_handler_contents, R"(
    navigator.mediaDevices.getUserMedia({video: true})
      .then(stream =>
          stream.getVideoTracks().length > 0 ? 'success' : 'no-tracks')
      .catch(err => err.name);
  )")
                           .ExtractString();
  EXPECT_EQ("success", result);
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerWebFlowViewCameraTest, AudioAccessDenied) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ASSERT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("launchWithoutWaitForResponse($1)", method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  views::View* top_view = test_api(dialog_view()).view_stack()->top();
  auto* sheet_controller =
      test_api(dialog_view()).controller_map()->at(top_view).get();
  auto* web_flow_controller =
      static_cast<PaymentHandlerWebFlowViewController*>(sheet_controller);
  content::WebContents* payment_handler_contents =
      web_flow_controller->web_contents();

  GURL payment_app_url = payment_handler_contents->GetLastCommittedURL();
  HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile())
      ->SetContentSettingDefaultScope(payment_app_url, payment_app_url,
                                      ContentSettingsType::MEDIASTREAM_MIC,
                                      CONTENT_SETTING_ALLOW);

  std::string result = content::EvalJs(payment_handler_contents, R"(
    navigator.mediaDevices.getUserMedia({audio: true})
      .then(() => 'allowed')
      .catch(err => err.name);
  )")
                           .ExtractString();
  EXPECT_EQ("NotSupportedError", result);
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerWebFlowViewCameraTest,
                       AudioAndVideoAccessDenied) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ASSERT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("launchWithoutWaitForResponse($1)", method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  views::View* top_view = test_api(dialog_view()).view_stack()->top();
  auto* sheet_controller =
      test_api(dialog_view()).controller_map()->at(top_view).get();
  auto* web_flow_controller =
      static_cast<PaymentHandlerWebFlowViewController*>(sheet_controller);
  content::WebContents* payment_handler_contents =
      web_flow_controller->web_contents();

  GURL payment_app_url = payment_handler_contents->GetLastCommittedURL();
  HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile())
      ->SetContentSettingDefaultScope(payment_app_url, payment_app_url,
                                      ContentSettingsType::MEDIASTREAM_MIC,
                                      CONTENT_SETTING_ALLOW);
  HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile())
      ->SetContentSettingDefaultScope(payment_app_url, payment_app_url,
                                      ContentSettingsType::MEDIASTREAM_CAMERA,
                                      CONTENT_SETTING_ALLOW);

  std::string result = content::EvalJs(payment_handler_contents, R"(
    navigator.mediaDevices.getUserMedia({audio: true, video: true})
      .then(() => 'allowed')
      .catch(err => err.name);
  )")
                           .ExtractString();
  EXPECT_EQ("NotSupportedError", result);
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerWebFlowViewCameraTest,
                       CameraAccessBlocked) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ASSERT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("launchWithoutWaitForResponse($1)", method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  views::View* top_view = test_api(dialog_view()).view_stack()->top();
  auto* sheet_controller =
      test_api(dialog_view()).controller_map()->at(top_view).get();
  auto* web_flow_controller =
      static_cast<PaymentHandlerWebFlowViewController*>(sheet_controller);
  content::WebContents* payment_handler_contents =
      web_flow_controller->web_contents();

  GURL payment_app_url = payment_handler_contents->GetLastCommittedURL();
  HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile())
      ->SetContentSettingDefaultScope(payment_app_url, payment_app_url,
                                      ContentSettingsType::MEDIASTREAM_CAMERA,
                                      CONTENT_SETTING_BLOCK);

  std::string result = content::EvalJs(payment_handler_contents, R"(
    navigator.mediaDevices.getUserMedia({video: true})
      .then(() => 'allowed')
      .catch(err => err.name);
  )")
                           .ExtractString();
  EXPECT_NE("success", result);
}

}  // namespace payments
