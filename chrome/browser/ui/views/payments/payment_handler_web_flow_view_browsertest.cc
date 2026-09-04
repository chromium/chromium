// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/payments/payment_handler_web_flow_view_controller.h"
#include "chrome/browser/ui/views/payments/payment_handler_web_flow_view_test_api.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_test_api.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/page_info/page_info.h"
#include "components/payments/content/payment_request_state.h"
#include "components/payments/core/features.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_request.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"

namespace payments {

namespace {

class VideoCaptureWaiter : public MediaStreamCaptureIndicator::Observer {
 public:
  explicit VideoCaptureWaiter(content::WebContents* web_contents)
      : target_web_contents_(web_contents) {
    if (scoped_refptr<MediaStreamCaptureIndicator> indicator =
            MediaCaptureDevicesDispatcher::GetInstance()
                ->GetMediaStreamCaptureIndicator()) {
      observation_.Observe(indicator.get());
      is_capturing_ = indicator->IsCapturingVideo(web_contents);
    }
  }

  ~VideoCaptureWaiter() override = default;

  void WaitForCaptureState(bool capture_state) {
    if (is_capturing_ == capture_state) {
      return;
    }
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void OnIsCapturingVideoChanged(content::WebContents* web_contents,
                                 bool is_capturing_video) override {
    if (web_contents == target_web_contents_) {
      is_capturing_ = is_capturing_video;
      if (quit_closure_) {
        std::move(quit_closure_).Run();
      }
    }
  }

 private:
  raw_ptr<content::WebContents> target_web_contents_;
  bool is_capturing_ = false;
  base::OnceClosure quit_closure_;
  base::ScopedObservation<MediaStreamCaptureIndicator,
                          MediaStreamCaptureIndicator::Observer>
      observation_{this};
};

}  // namespace

class PaymentHandlerWebFlowViewTest : public PaymentRequestBrowserTestBase {
 public:
  PaymentHandlerWebFlowViewTest() = default;

 private:
  base::test::ScopedFeatureList feature_list_{
      features::kPaymentRequestMandatoryPaymentAppUi};
};

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

class PermissionPromptWaiter
    : public permissions::PermissionRequestManager::Observer {
 public:
  explicit PermissionPromptWaiter(
      permissions::PermissionRequestManager* manager) {
    observation_.Observe(manager);
  }

  void OnPromptAdded() override { run_loop_.Quit(); }

  void OnRequestsFinalized() override { finalize_run_loop_.Quit(); }

  void WaitUntilPromptAdded() { run_loop_.Run(); }
  void WaitUntilRequestsFinalized() { finalize_run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
  base::RunLoop finalize_run_loop_;
  base::ScopedObservation<permissions::PermissionRequestManager,
                          permissions::PermissionRequestManager::Observer>
      observation_{this};
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
                               DialogEvent::LOADING_VIEW_SHOWN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::LOADING_VIEW_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ASSERT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("launchWithoutWaitForResponse($1)", method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  // We always push the initial browser sheet to the stack, even if it isn't
  // shown. Since it also defines a CONTENT_VIEW, we have to explicitly test
  // the front PaymentHandler view here.
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
    feature_list_.InitWithFeatures(
        {payments::features::kPaymentHandlerDialogUseInitiatorInUrlLoad,
         payments::features::kPaymentRequestMandatoryPaymentAppUi},
        {});
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
                               DialogEvent::LOADING_VIEW_SHOWN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::LOADING_VIEW_HIDDEN,
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
                               DialogEvent::LOADING_VIEW_SHOWN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::LOADING_VIEW_HIDDEN,
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

 private:
  base::test::ScopedFeatureList feature_list_{
      features::kPaymentRequestMandatoryPaymentAppUi};
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
                               DialogEvent::LOADING_VIEW_SHOWN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::LOADING_VIEW_HIDDEN,
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
    //   causing these actions to fail. Completing/rejecting successfully
    //   proves the dialog stayed open.
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
  PaymentHandlerWebFlowViewMandatoryUiEnabledTest() = default;

 private:
  base::test::ScopedFeatureList feature_list_{
      payments::features::kPaymentRequestMandatoryPaymentAppUi};
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

  // Wait for the WebView to finish composition and load.
  content::WaitForCopyableViewInWebContents(payment_handler_contents);
  // Reject the promise with a user interaction.
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
  // because the app rejects the payment immediately before any user
  // interaction occurs.
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

  // Wait for the WebView to finish composition and load.
  content::WaitForCopyableViewInWebContents(payment_handler_contents);
  // Reject the promise with a user interaction.
  content::SimulateMouseClickOrTapElementWithId(payment_handler_contents,
                                                "reject-button");

}

class PaymentHandlerWebFlowViewCameraTest
    : public PaymentRequestBrowserTestBase,
      public testing::WithParamInterface<base::test::FeatureRef> {
 public:
  PaymentHandlerWebFlowViewCameraTest() {
    feature_list_.InitWithFeatures(
        {*GetParam(), features::kPaymentRequestMandatoryPaymentAppUi}, {});
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
                               DialogEvent::LOADING_VIEW_SHOWN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::LOADING_VIEW_HIDDEN,
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
  EXPECT_EQ(nullptr, permissions::PermissionRequestManager::FromWebContents(
                         payment_handler_contents));
  EXPECT_NE(nullptr, PaymentHandlerWebFlowViewController::FromWebContents(
                         payment_handler_contents));
  EXPECT_EQ(nullptr, web_flow_controller->GetPageInfoIconView());

  std::string result = content::EvalJs(payment_handler_contents, R"(
    navigator.mediaDevices.getUserMedia({video: true})
      .then(() => 'allowed')
      .catch(err => err.name);
  )")
                           .ExtractString();
  EXPECT_EQ("NotSupportedError", result);
}

IN_PROC_BROWSER_TEST_P(PaymentHandlerWebFlowViewCameraTest,
                       CameraAccessPreGrantedSuccess) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::LOADING_VIEW_SHOWN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::LOADING_VIEW_HIDDEN,
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
  // OneTimePermissionsTrackerHelper on its webcontents, to support "Allow
  // this time" permissions from a nested pop-up window to persist through
  // this session.
  EXPECT_NE(nullptr, OneTimePermissionsTrackerHelper::FromWebContents(
                         payment_handler_contents));

  // kPaymentHandlerCameraAccessUx flag also initializes
  // PermissionRequestManager for permission prompting and indicators.
  if (GetParam() == features::kPaymentHandlerCameraAccessUx) {
    EXPECT_NE(nullptr, permissions::PermissionRequestManager::FromWebContents(
                           payment_handler_contents));
  } else {
    EXPECT_EQ(nullptr, permissions::PermissionRequestManager::FromWebContents(
                           payment_handler_contents));
  }

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

IN_PROC_BROWSER_TEST_P(PaymentHandlerWebFlowViewCameraTest, AudioAccessDenied) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::LOADING_VIEW_SHOWN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::LOADING_VIEW_HIDDEN,
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

IN_PROC_BROWSER_TEST_P(PaymentHandlerWebFlowViewCameraTest,
                       AudioAndVideoAccessDenied) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::LOADING_VIEW_SHOWN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::LOADING_VIEW_HIDDEN,
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

IN_PROC_BROWSER_TEST_P(PaymentHandlerWebFlowViewCameraTest,
                       CameraAccessBlocked) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::LOADING_VIEW_SHOWN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::LOADING_VIEW_HIDDEN,
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
  EXPECT_EQ("NotAllowedError", result);
}

IN_PROC_BROWSER_TEST_P(
    PaymentHandlerWebFlowViewCameraTest,
    PermissionPromptBubble_AnchorsToAppIconInPaymentHandler) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::LOADING_VIEW_SHOWN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::LOADING_VIEW_HIDDEN,
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

  if (GetParam() == features::kPaymentHandlerCameraAccessUx) {
    views::View* page_info_icon = web_flow_controller->GetPageInfoIconView();
    ASSERT_NE(nullptr, page_info_icon);

    bubble_anchor_util::AnchorConfiguration config =
        bubble_anchor_util::GetPermissionPromptBubbleAnchorConfiguration(
            payment_handler_contents);
    EXPECT_EQ(page_info_icon, config.anchor.GetIfView());
    EXPECT_EQ(PaymentHandlerWebFlowViewController::kAppIconElementId,
              config.highlighted_element);
    EXPECT_EQ(views::BubbleBorder::TOP_LEFT, config.bubble_arrow);
  } else {
    EXPECT_EQ(nullptr, web_flow_controller->GetPageInfoIconView());
    EXPECT_NE(nullptr, top_view->GetViewByID(static_cast<int>(
                           DialogViewID::PAYMENT_APP_HEADER_ICON)));

    bubble_anchor_util::AnchorConfiguration config =
        bubble_anchor_util::GetPermissionPromptBubbleAnchorConfiguration(
            payment_handler_contents);
    EXPECT_TRUE(config.anchor.IsNull());
  }
}

IN_PROC_BROWSER_TEST_P(PaymentHandlerWebFlowViewCameraTest,
                       AppIconButton_OpensPaymentHandlerPageInfo) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::LOADING_VIEW_SHOWN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::LOADING_VIEW_HIDDEN,
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

  if (GetParam() == features::kPaymentHandlerCameraAccessUx) {
    views::View* page_info_icon = web_flow_controller->GetPageInfoIconView();
    ASSERT_NE(nullptr, page_info_icon);
    auto* location_icon_view =
        views::AsViewClass<LocationIconView>(page_info_icon);
    ASSERT_NE(nullptr, location_icon_view);

    views::test::ButtonTestApi(location_icon_view)
        .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                    gfx::Point(), base::TimeTicks(),
                                    ui::EF_LEFT_MOUSE_BUTTON,
                                    ui::EF_LEFT_MOUSE_BUTTON));

    views::BubbleDialogDelegateView* bubble =
        PageInfoBubbleViewBase::GetPageInfoBubbleForTesting();
    ASSERT_NE(nullptr, bubble);
    EXPECT_EQ(PageInfoBubbleViewBase::BUBBLE_PAGE_INFO,
              PageInfoBubbleViewBase::GetShownBubbleType());

    auto* page_info_bubble = static_cast<PageInfoBubbleView*>(bubble);
    // Verify navigating to security sub-page and cookies sub-page does not
    // crash.
    page_info_bubble->OpenSecurityPage();
    page_info_bubble->OpenCookiesPage();

    views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
    // Verify clicking the app icon button a second time closes the bubble.
    views::test::ButtonTestApi(location_icon_view)
        .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                    gfx::Point(), base::TimeTicks(),
                                    ui::EF_LEFT_MOUSE_BUTTON,
                                    ui::EF_LEFT_MOUSE_BUTTON));
    waiter.Wait();
    EXPECT_EQ(nullptr, PageInfoBubbleViewBase::GetPageInfoBubbleForTesting());
  } else {
    EXPECT_EQ(nullptr, web_flow_controller->GetPageInfoIconView());
  }
}

IN_PROC_BROWSER_TEST_P(PaymentHandlerWebFlowViewCameraTest,
                       PermissionPrompt_ShowsInPaymentHandlerWithoutCrash) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::LOADING_VIEW_SHOWN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::LOADING_VIEW_HIDDEN,
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

  if (GetParam() == features::kPaymentHandlerCameraAccessUx) {
    auto* permission_manager =
        permissions::PermissionRequestManager::FromWebContents(
            payment_handler_contents);
    ASSERT_NE(nullptr, permission_manager);

    PermissionPromptWaiter prompt_waiter(permission_manager);

    // Requesting permission inside Payment Handler must anchor to the app
    // icon and show the prompt view without crashing.
    permission_manager->AddRequest(
        payment_handler_contents->GetPrimaryMainFrame(),
        std::make_unique<permissions::MockPermissionRequest>(
            payment_handler_contents->GetLastCommittedURL(),
            permissions::RequestType::kCameraStream,
            permissions::PermissionRequestGestureType::GESTURE));
    prompt_waiter.WaitUntilPromptAdded();
    EXPECT_TRUE(permission_manager->IsRequestInProgress());
    permission_manager->FinalizeCurrentRequests();
  }
}

IN_PROC_BROWSER_TEST_P(
    PaymentHandlerWebFlowViewCameraTest,
    OpenURLFromTab_RejectsCurrentTabAndRoutesNewTabToParent) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::LOADING_VIEW_SHOWN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::LOADING_VIEW_HIDDEN,
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

  // Verify CURRENT_TAB returns nullptr to preserve internal dialog navigation
  // behavior and prevent accidental parent tab navigation.
  content::OpenURLParams current_tab_params(
      GURL("https://example.com"), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_LINK,
      /*is_renderer_initiated=*/false);
  EXPECT_EQ(nullptr, payment_handler_contents->GetDelegate()->OpenURLFromTab(
                         payment_handler_contents, current_tab_params,
                         base::NullCallback()));

  // Verify NEW_FOREGROUND_TAB (e.g. PageInfo "Learn more") routes to the
  // parent tab's WebContents.
  content::OpenURLParams new_tab_params(
      GURL("https://example.com"), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      /*is_renderer_initiated=*/false);
  content::WebContents* result =
      payment_handler_contents->GetDelegate()->OpenURLFromTab(
          payment_handler_contents, new_tab_params, base::NullCallback());
  EXPECT_NE(nullptr, result);
}

class PaymentHandlerWebFlowViewCameraUxTest
    : public PaymentRequestBrowserTestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PaymentRequestBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      {features::kPaymentHandlerCameraAccessUx,
       features::kPaymentRequestMandatoryPaymentAppUi}};
};

IN_PROC_BROWSER_TEST_F(PaymentHandlerWebFlowViewCameraUxTest,
                       CameraInUseIndicator_TogglesOnVideoCapture) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::LOADING_VIEW_SHOWN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::LOADING_VIEW_HIDDEN,
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
                                      CONTENT_SETTING_ALLOW);

  // Initial state: not capturing.
  EXPECT_NE(nullptr, web_flow_controller->GetPageInfoIconView());
  ASSERT_NE(nullptr, test_api(web_flow_controller).location_icon_view());
  EXPECT_TRUE(test_api(web_flow_controller).location_icon_view()->GetVisible());
  ASSERT_NE(nullptr, test_api(web_flow_controller).permission_dashboard_view());
  EXPECT_FALSE(
      test_api(web_flow_controller).permission_dashboard_view()->GetVisible());

  // Start video capture.
  VideoCaptureWaiter waiter(payment_handler_contents);
  ASSERT_EQ("success", content::EvalJs(payment_handler_contents, R"(
              navigator.mediaDevices.getUserMedia({video: true})
                .then(stream => {
                  window.activeStream = stream;
                  return 'success';
                })
                .catch(err => err.name);
            )"));
  waiter.WaitForCaptureState(true);

  // PermissionDashboardView should be visible, LocationIconView hidden.
  EXPECT_TRUE(
      test_api(web_flow_controller).permission_dashboard_view()->GetVisible());
  EXPECT_FALSE(
      test_api(web_flow_controller).location_icon_view()->GetVisible());
  auto* indicator_chip = test_api(web_flow_controller)
                             .permission_dashboard_view()
                             ->GetIndicatorChip();
  ASSERT_NE(nullptr, indicator_chip);
  EXPECT_TRUE(indicator_chip->GetVisible());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_CAMERA_IN_USE),
            indicator_chip->GetTooltipText());
  EXPECT_EQ(indicator_chip, web_flow_controller->GetPageInfoIconView());

  // Stop video capture.
  ASSERT_EQ("stopped", content::EvalJs(payment_handler_contents, R"(
              window.activeStream.getVideoTracks().forEach(t => t.stop());
              'stopped';
            )"));
  waiter.WaitForCaptureState(false);

  // PermissionDashboardView should be hidden, LocationIconView visible again.
  EXPECT_FALSE(
      test_api(web_flow_controller).permission_dashboard_view()->GetVisible());
  EXPECT_TRUE(test_api(web_flow_controller).location_icon_view()->GetVisible());
  EXPECT_EQ(test_api(web_flow_controller).location_icon_view(),
            web_flow_controller->GetPageInfoIconView());
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerWebFlowViewCameraUxTest,
                       CameraInUseIndicator_IgnoresForeignWebContents) {
  NavigateTo("/payment_handler.html");
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::LOADING_VIEW_SHOWN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::LOADING_VIEW_HIDDEN,
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

  // Initial state: not capturing.
  ASSERT_NE(nullptr, test_api(web_flow_controller).permission_dashboard_view());
  EXPECT_FALSE(
      test_api(web_flow_controller).permission_dashboard_view()->GetVisible());
  ASSERT_NE(nullptr, test_api(web_flow_controller).location_icon_view());
  EXPECT_TRUE(test_api(web_flow_controller).location_icon_view()->GetVisible());

  // Trigger video capture on parent tab WebContents.
  content::WebContents* parent_contents = GetActiveWebContents();
  GURL parent_url = parent_contents->GetLastCommittedURL();
  HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile())
      ->SetContentSettingDefaultScope(parent_url, parent_url,
                                      ContentSettingsType::MEDIASTREAM_CAMERA,
                                      CONTENT_SETTING_ALLOW);

  VideoCaptureWaiter parent_waiter(parent_contents);
  ASSERT_EQ("success", content::EvalJs(parent_contents, R"(
                         navigator.mediaDevices.getUserMedia({video: true})
                           .then(stream => {
                             window.activeStream = stream;
                             return 'success';
                           })
                           .catch(err => err.name);
                       )"));
  parent_waiter.WaitForCaptureState(true);

  // Verify Payment Handler indicator remains hidden (not affected by parent
  // tab).
  EXPECT_FALSE(
      test_api(web_flow_controller).permission_dashboard_view()->GetVisible());
  EXPECT_TRUE(test_api(web_flow_controller).location_icon_view()->GetVisible());

  // Clean up parent stream.
  ASSERT_EQ("stopped", content::EvalJs(parent_contents, R"(
              window.activeStream.getVideoTracks().forEach(t => t.stop());
              'stopped';
            )"));
  parent_waiter.WaitForCaptureState(false);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PaymentHandlerWebFlowViewCameraTest,
    testing::Values(
        base::test::FeatureRef(features::kPaymentHandlerCameraAccess),
        base::test::FeatureRef(features::kPaymentHandlerCameraAccessUx)));

}  // namespace payments
