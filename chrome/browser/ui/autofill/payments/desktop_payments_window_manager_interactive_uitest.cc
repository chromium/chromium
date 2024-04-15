// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/autofill/payments/desktop_payments_window_manager.h"
#include "chrome/browser/ui/autofill/payments/desktop_payments_window_manager_test_api.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/autofill/payments/payments_window_user_consent_dialog_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_window_manager.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test_payments_network_interface.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill {

class TestContentAutofillClientForWindowManagerTest
    : public TestContentAutofillClient {
 public:
  explicit TestContentAutofillClientForWindowManagerTest(
      content::WebContents* web_contents)
      : TestContentAutofillClient(web_contents) {
    GetPaymentsAutofillClient()->set_test_payments_network_interface(
        std::make_unique<payments::TestPaymentsNetworkInterface>(
            nullptr, nullptr, nullptr));
    GetPaymentsAutofillClient()->set_payments_window_manager(
        std::make_unique<payments::DesktopPaymentsWindowManager>(this));
  }

  ~TestContentAutofillClientForWindowManagerTest() override = default;
};

namespace payments {

constexpr std::string_view kVcn3dsTestUrl = "https://site.example/";
constexpr std::string_view kTestContextToken = "Test context token";

class DesktopPaymentsWindowManagerInteractiveUiTest : public UiBrowserTest {
 public:
  DesktopPaymentsWindowManagerInteractiveUiTest() = default;

  void ShowUi(const std::string& name) override {
    if (name.find("Vcn3ds") != std::string::npos) {
      client()->set_last_committed_primary_main_frame_url(GURL(kVcn3dsTestUrl));

      PaymentsWindowManager::Vcn3dsContext context;
      card_ = test::GetVirtualCard();
      context.card = card_;
      context.context_token = kTestContextToken;
      context.challenge_option.url_to_open = GURL(kVcn3dsTestUrl);
      context.completion_callback = authentication_complete_callback_.Get();
      context.user_consent_already_given =
          name.find("ConsentAlreadyGiven") != std::string::npos;
      ON_CALL(authentication_complete_callback_, Run)
          .WillByDefault(
              [this](PaymentsWindowManager::Vcn3dsAuthenticationResponse
                         authentication_response) {
                set_authentication_response(std::move(authentication_response));
              });
      window_manager().InitVcn3dsAuthentication(std::move(context));
    } else {
      NOTREACHED();
    }
  }

  bool VerifyUi() override {
    // There should be two browsers present, the original browser and the
    // pop-up's browser.
    if (BrowserList::GetInstance()->size() != 2U) {
      return false;
    }

    auto* source_web_contents = GetOriginalPageWebContents();

    // The pop-up must be created from `source_web_contents`, so it will always
    // be the second browser in the BrowserList.
    auto* popup_web_contents = GetPopupWebContents();

    // This ensures that there is no scripting relationship between the pop-up
    // and the original tab.
    if (source_web_contents->GetSiteInstance()->IsRelatedSiteInstance(
            popup_web_contents->GetSiteInstance())) {
      return false;
    }

    std::string test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();
    if (test_name.find("Vcn3ds") != std::string::npos) {
      if (popup_web_contents->GetVisibleURL() != GURL(kVcn3dsTestUrl)) {
        return false;
      }
    } else {
      NOTREACHED();
    }

    return true;
  }

  void WaitForUserDismissal() override {}

 protected:
  content::WebContents* GetOriginalPageWebContents() {
    // The original page is always created first, so it is the first browser in
    // the browser list.
    return BrowserList::GetInstance()
        ->get(0)
        ->tab_strip_model()
        ->GetActiveWebContents();
  }

  content::WebContents* GetPopupWebContents() {
    // The pop-up must be created from `source_web_contents`, so it is the
    // second browser in the BrowserList.
    return BrowserList::GetInstance()
        ->get(1)
        ->tab_strip_model()
        ->GetActiveWebContents();
  }

  void ClosePopup() {
    GetPopupWebContents()->Close();
    base::RunLoop().RunUntilIdle();
  }

  TestContentAutofillClientForWindowManagerTest* client() {
    return test_autofill_client_injector_[GetOriginalPageWebContents()];
  }

  DesktopPaymentsWindowManager& window_manager() {
    return *static_cast<DesktopPaymentsWindowManager*>(
        client()->GetPaymentsAutofillClient()->GetPaymentsWindowManager());
  }

  void set_authentication_response(
      PaymentsWindowManager::Vcn3dsAuthenticationResponse
          authentication_response) {
    authentication_response_ = std::move(authentication_response);
  }
  std::optional<PaymentsWindowManager::Vcn3dsAuthenticationResponse>
  authentication_response() {
    return authentication_response_;
  }

  CreditCard card_;

 private:
  TestAutofillClientInjector<TestContentAutofillClientForWindowManagerTest>
      test_autofill_client_injector_;

  base::MockCallback<
      PaymentsWindowManager::OnVcn3dsAuthenticationCompleteCallback>
      authentication_complete_callback_;
  std::optional<PaymentsWindowManager::Vcn3dsAuthenticationResponse>
      authentication_response_;
};

// Test that the VCN 3DS pop-up is shown correctly, and on close an
// UnmaskCardRequest is triggered with the proper fields set if the right query
// params are present.
IN_PROC_BROWSER_TEST_F(DesktopPaymentsWindowManagerInteractiveUiTest,
                       InvokeUi_Vcn3ds_QueryParamsPresent) {
  ShowUi("Vcn3ds_ConsentAlreadyGiven");
  EXPECT_TRUE(VerifyUi());

  // Navigate to a page where there are shouldProceed and token query params.
  GetPopupWebContents()->OpenURL(
      content::OpenURLParams(
          GURL("https://site.example/?shouldProceed=true&token=sometesttoken"),
          content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
          ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
          /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});

  ClosePopup();
  EXPECT_FALSE(test_api(window_manager()).NoOngoingFlow());

  // Check that the flow was successful and an UnmaskCardRequest was triggered
  // with the correct fields set, and the progress dialog was shown.
  auto* autofill_client = client();
  EXPECT_TRUE(autofill_client->GetPaymentsAutofillClient()
                  ->autofill_progress_dialog_shown());
  const std::optional<payments::PaymentsNetworkInterface::UnmaskRequestDetails>&
      unmask_request = static_cast<payments::TestPaymentsNetworkInterface*>(
                           autofill_client->GetPaymentsAutofillClient()
                               ->GetPaymentsNetworkInterface())
                           ->unmask_request();
  ASSERT_TRUE(unmask_request.has_value());
  EXPECT_EQ(unmask_request->card, card_);
  EXPECT_EQ(unmask_request->redirect_completion_proof.value(), "sometesttoken");
  EXPECT_EQ(unmask_request->last_committed_primary_main_frame_origin,
            client()->GetLastCommittedPrimaryMainFrameOrigin().GetURL());

  // Simulate a response for the UnmaskCardRequest and ensure the callback is
  // run with the correct information.
  PaymentsNetworkInterface::UnmaskResponseDetails response_details;
  response_details.with_real_pan("1111222233334444");
  response_details.with_dcvv("123");
  response_details.expiration_month = "01";
  response_details.expiration_year = "2030";
  test_api(window_manager())
      .OnVcn3dsAuthenticationResponseReceived(
          AutofillClient::PaymentsRpcResult::kSuccess, response_details);

  EXPECT_EQ(unmask_request->context_token, kTestContextToken);
  ASSERT_TRUE(unmask_request->selected_challenge_option.has_value());
  EXPECT_EQ(unmask_request->selected_challenge_option->url_to_open,
            kVcn3dsTestUrl);
  std::optional<PaymentsWindowManager::Vcn3dsAuthenticationResponse> response =
      authentication_response();
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->card.has_value());
  EXPECT_EQ(response->card->number(),
            base::UTF8ToUTF16(response_details.real_pan));
  EXPECT_EQ(response->card->cvc(), base::UTF8ToUTF16(response_details.dcvv));
  int expiration_month;
  int expiration_year;
  base::StringToInt(response_details.expiration_month, &expiration_month);
  base::StringToInt(response_details.expiration_year, &expiration_year);
  EXPECT_EQ(response->card->expiration_month(), expiration_month);
  EXPECT_EQ(response->card->expiration_year(), expiration_year);
  EXPECT_EQ(response->card->record_type(),
            CreditCard::RecordType::kVirtualCard);
  EXPECT_FALSE(
      client()->GetPaymentsAutofillClient()->autofill_error_dialog_shown());
  EXPECT_TRUE(test_api(window_manager()).NoOngoingFlow());
}

// Test that the VCN 3DS pop-up is shown correctly, and on close an
// UnmaskCardRequest is not triggered if the query params indicate the
// authentication failed.
IN_PROC_BROWSER_TEST_F(DesktopPaymentsWindowManagerInteractiveUiTest,
                       InvokeUi_Vcn3ds_QueryParams_AuthenticationFailed) {
  ShowUi("Vcn3ds_ConsentAlreadyGiven");
  EXPECT_TRUE(VerifyUi());

  // Navigate to a page where there is an shouldProceed query param that denotes
  // the authentication failed.
  GetPopupWebContents()->OpenURL(
      content::OpenURLParams(GURL("https://site.example/?shouldProceed=false"),
                             content::Referrer(),
                             WindowOpenDisposition::CURRENT_TAB,
                             ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
                             /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});

  ClosePopup();
  EXPECT_TRUE(test_api(window_manager()).NoOngoingFlow());

  // Check that the flow was ended and no UnmaskCardRequest was triggered.
  const std::optional<payments::PaymentsNetworkInterface::UnmaskRequestDetails>&
      unmask_request = static_cast<payments::TestPaymentsNetworkInterface*>(
                           client()
                               ->GetPaymentsAutofillClient()
                               ->GetPaymentsNetworkInterface())
                           ->unmask_request();
  ASSERT_FALSE(unmask_request.has_value());
  std::optional<PaymentsWindowManager::Vcn3dsAuthenticationResponse> response =
      authentication_response();
  ASSERT_TRUE(response.has_value());
  EXPECT_FALSE(response->card.has_value());
  EXPECT_TRUE(
      client()->GetPaymentsAutofillClient()->autofill_error_dialog_shown());
}

// Test that the VCN 3DS pop-up is shown correctly, and on close an
// UnmaskCardRequest is not triggered if there are no query params present.
IN_PROC_BROWSER_TEST_F(DesktopPaymentsWindowManagerInteractiveUiTest,
                       InvokeUi_Vcn3ds_NoQueryParamsAndPopupClosed) {
  ShowUi("Vcn3ds_ConsentAlreadyGiven");
  EXPECT_TRUE(VerifyUi());

  ClosePopup();
  EXPECT_TRUE(test_api(window_manager()).NoOngoingFlow());

  // Check that the flow was ended and no UnmaskCardRequest was triggered.
  const std::optional<payments::PaymentsNetworkInterface::UnmaskRequestDetails>&
      unmask_request = static_cast<payments::TestPaymentsNetworkInterface*>(
                           client()
                               ->GetPaymentsAutofillClient()
                               ->GetPaymentsNetworkInterface())
                           ->unmask_request();
  ASSERT_FALSE(unmask_request.has_value());
  std::optional<PaymentsWindowManager::Vcn3dsAuthenticationResponse> response =
      authentication_response();
  ASSERT_TRUE(response.has_value());
  EXPECT_FALSE(response->card.has_value());
  EXPECT_FALSE(
      client()->GetPaymentsAutofillClient()->autofill_error_dialog_shown());
}

// Test that the VCN 3DS pop-up is shown correctly, and on close an
// UnmaskCardRequest is not triggered if the query params are invalid.
IN_PROC_BROWSER_TEST_F(DesktopPaymentsWindowManagerInteractiveUiTest,
                       InvokeUi_Vcn3ds_InvalidQueryParams) {
  ShowUi("Vcn3ds_ConsentAlreadyGiven");
  EXPECT_TRUE(VerifyUi());

  // Navigate to a page where there is an unrecognized query param.
  GetPopupWebContents()->OpenURL(
      content::OpenURLParams(
          GURL("https://site.example/?unrecognizedQueryParam=true"),
          content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
          ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
          /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});

  ClosePopup();
  EXPECT_TRUE(test_api(window_manager()).NoOngoingFlow());

  // Check that the flow was ended and no UnmaskCardRequest was triggered.
  const std::optional<payments::PaymentsNetworkInterface::UnmaskRequestDetails>&
      unmask_request = static_cast<payments::TestPaymentsNetworkInterface*>(
                           client()
                               ->GetPaymentsAutofillClient()
                               ->GetPaymentsNetworkInterface())
                           ->unmask_request();
  ASSERT_FALSE(unmask_request.has_value());
  std::optional<PaymentsWindowManager::Vcn3dsAuthenticationResponse> response =
      authentication_response();
  ASSERT_TRUE(response.has_value());
  EXPECT_FALSE(response->card.has_value());
  EXPECT_FALSE(
      client()->GetPaymentsAutofillClient()->autofill_error_dialog_shown());
}

// Test that the VCN 3DS pop-up is shown correctly, and when the user cancels
// the progress dialog, the state of the PaymentsWindowManager in relation to
// the ongoing UnmaskCardRequest is reset.
IN_PROC_BROWSER_TEST_F(DesktopPaymentsWindowManagerInteractiveUiTest,
                       InvokeUi_Vcn3ds_ProgressDialogCancelled) {
  ShowUi("Vcn3ds_ConsentAlreadyGiven");
  EXPECT_TRUE(VerifyUi());

  // Navigate to a page where there are shouldProceed and token query params.
  GetPopupWebContents()->OpenURL(
      content::OpenURLParams(
          GURL("https://site.example/?shouldProceed=true&token=sometesttoken"),
          content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
          ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
          /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});

  ClosePopup();
  EXPECT_FALSE(test_api(window_manager()).NoOngoingFlow());

  EXPECT_TRUE(
      client()->GetPaymentsAutofillClient()->autofill_progress_dialog_shown());

  // Check that the state of the PaymentsWindowManager is reset correctly
  // if the user cancels the progress dialog.
  EXPECT_TRUE(test_api(window_manager()).GetVcn3dsContext().has_value());
  test_api(window_manager()).OnVcn3dsAuthenticationProgressDialogCancelled();
  EXPECT_FALSE(test_api(window_manager()).GetVcn3dsContext().has_value());
  std::optional<PaymentsWindowManager::Vcn3dsAuthenticationResponse> response =
      authentication_response();
  ASSERT_TRUE(response.has_value());
  EXPECT_FALSE(response->card.has_value());
  EXPECT_TRUE(test_api(window_manager()).NoOngoingFlow());
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
// Tests that if a VCN 3DS flow is ongoing, and the original tab is set active,
// the payments window manager popup's web contents are re-activated.
IN_PROC_BROWSER_TEST_F(DesktopPaymentsWindowManagerInteractiveUiTest,
                       InvokeUi_Vcn3ds_OriginalTabSetLastActive) {
  ShowUi("Vcn3ds_ConsentAlreadyGiven");
  EXPECT_TRUE(VerifyUi());

  // Activate the original browser and check that the browser containing the
  // pop-up's web contents becomes the last active browser.
  ui_test_utils::BrowserActivationWaiter waiter(
      BrowserList::GetInstance()->get(1));
  BrowserList::GetInstance()->get(0)->window()->Activate();
  waiter.WaitForActivation();
  EXPECT_TRUE(BrowserList::GetInstance()
                  ->GetLastActive()
                  ->tab_strip_model()
                  ->GetActiveWebContents() == GetPopupWebContents());
}
#endif  // #if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)

// Integration test using Kombucha to ensure that the consent dialog creates a
// new pop-up when the ok button is clicked, and cancels the flow when the
// cancel button is clicked.
class PaymentsWindowUserConsentDialogIntegrationTest
    : public InteractiveBrowserTest {
 public:
  PaymentsWindowUserConsentDialogIntegrationTest() = default;
  PaymentsWindowUserConsentDialogIntegrationTest(
      const PaymentsWindowUserConsentDialogIntegrationTest&) = delete;
  PaymentsWindowUserConsentDialogIntegrationTest& operator=(
      const PaymentsWindowUserConsentDialogIntegrationTest&) = delete;
  ~PaymentsWindowUserConsentDialogIntegrationTest() override = default;

 protected:
  InteractiveBrowserTestApi::MultiStep TriggerDialogAndWaitForShow(
      ElementSpecifier element_specifier) {
    return Steps(TriggerDialog(),
                 // TODO(crbug.com/332603033): Tab-modal dialogs on MacOS run in
                 // a different context.
                 InAnyContext(WaitForShow(element_specifier)));
  }

  DesktopPaymentsWindowManager& GetWindowManager() {
    // The original page is always created first, so it is the first browser in
    // the browser list.
    auto* original_page_web_contents = BrowserList::GetInstance()
                                           ->get(0)
                                           ->tab_strip_model()
                                           ->GetActiveWebContents();
    return *static_cast<DesktopPaymentsWindowManager*>(
        test_autofill_client_injector_[original_page_web_contents]
            ->GetPaymentsAutofillClient()
            ->GetPaymentsWindowManager());
  }

  void set_authentication_response(
      PaymentsWindowManager::Vcn3dsAuthenticationResponse
          authentication_response) {
    authentication_response_ = std::move(authentication_response);
  }
  std::optional<PaymentsWindowManager::Vcn3dsAuthenticationResponse>
  authentication_response() {
    return authentication_response_;
  }

 private:
  InteractiveTestApi::StepBuilder TriggerDialog() {
    return Do([this]() {
      PaymentsWindowManager::Vcn3dsContext context;
      context.card = test::GetVirtualCard();
      context.context_token = kTestContextToken;
      context.challenge_option.url_to_open = GURL(kVcn3dsTestUrl);
      context.completion_callback = authentication_complete_callback_.Get();
      context.user_consent_already_given = false;
      ON_CALL(authentication_complete_callback_, Run)
          .WillByDefault(
              [this](PaymentsWindowManager::Vcn3dsAuthenticationResponse
                         authentication_response) {
                set_authentication_response(std::move(authentication_response));
              });
      GetWindowManager().InitVcn3dsAuthentication(std::move(context));
    });
  }

  TestAutofillClientInjector<TestContentAutofillClientForWindowManagerTest>
      test_autofill_client_injector_;

  base::MockCallback<
      PaymentsWindowManager::OnVcn3dsAuthenticationCompleteCallback>
      authentication_complete_callback_;
  std::optional<PaymentsWindowManager::Vcn3dsAuthenticationResponse>
      authentication_response_;
};

// Ensures the UI can be shown, and verifies that accepting the dialog runs the
// accept callback and creates the pop-up.
IN_PROC_BROWSER_TEST_F(PaymentsWindowUserConsentDialogIntegrationTest,
                       DialogAccepted) {
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1U);

  RunTestSequence(
      TriggerDialogAndWaitForShow(views::DialogClientView::kOkButtonElementId),
      // TriggerDialogAndWaitForShow() changes the context, so the same context
      // must be used.
      InSameContext(Steps(
          PressButton(views::DialogClientView::kOkButtonElementId),
          AfterHide(PaymentsWindowUserConsentDialogView::kTopViewId, []() {
            EXPECT_EQ(BrowserList::GetInstance()->size(), 2U);
          }))));
}

// Ensures the UI can be shown, and verifies that cancelling the dialog runs the
// cancel callback and resets the state.
IN_PROC_BROWSER_TEST_F(PaymentsWindowUserConsentDialogIntegrationTest,
                       DialogCancelled) {
  RunTestSequence(
      TriggerDialogAndWaitForShow(
          views::DialogClientView::kCancelButtonElementId),
      // TriggerDialogAndWaitForShow() changes the context, so the same context
      // must be used.
      InSameContext(Steps(
          PressButton(views::DialogClientView::kCancelButtonElementId),
          AfterHide(PaymentsWindowUserConsentDialogView::kTopViewId, [this]() {
            EXPECT_FALSE(
                test_api(GetWindowManager()).GetVcn3dsContext().has_value());
            EXPECT_TRUE(test_api(GetWindowManager()).NoOngoingFlow());
            std::optional<PaymentsWindowManager::Vcn3dsAuthenticationResponse>
                response = authentication_response();
            ASSERT_TRUE(response.has_value());
            EXPECT_FALSE(response->card.has_value());
          }))));
}

}  // namespace payments

}  // namespace autofill
