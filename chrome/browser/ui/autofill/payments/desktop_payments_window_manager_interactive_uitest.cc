// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>

#include "chrome/browser/ui/autofill/payments/desktop_payments_window_manager.h"
#include "chrome/browser/ui/autofill/payments/desktop_payments_window_manager_test_api.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_window_manager.h"
#include "components/autofill/core/browser/payments/test_payments_network_interface.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class TestContentAutofillClientForWindowManagerTest
    : public TestContentAutofillClient {
 public:
  explicit TestContentAutofillClientForWindowManagerTest(
      content::WebContents* web_contents)
      : TestContentAutofillClient(web_contents) {
    set_test_payments_network_interface(
        std::make_unique<payments::TestPaymentsNetworkInterface>(
            nullptr, nullptr, nullptr));
    set_payments_window_manager(
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

      std::optional<PaymentsWindowManager::Vcn3dsContext> context =
          test_api(window_manager()).GetVcn3dsContext();
      if (!context.has_value() || context->card != card_ ||
          context->context_token != kTestContextToken ||
          context->challenge_option.url_to_open != GURL(kVcn3dsTestUrl)) {
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

  TestContentAutofillClientForWindowManagerTest* client() {
    return test_autofill_client_injector_[GetOriginalPageWebContents()];
  }

  DesktopPaymentsWindowManager& window_manager() {
    return *static_cast<DesktopPaymentsWindowManager*>(
        client()->GetPaymentsWindowManager());
  }

  CreditCard card_;

 private:
  TestAutofillClientInjector<TestContentAutofillClientForWindowManagerTest>
      test_autofill_client_injector_;
};

// Test that the VCN 3DS pop-up is shown correctly, and on close an
// UnmaskCardRequest is triggered with the proper fields set if the right query
// params are present.
IN_PROC_BROWSER_TEST_F(DesktopPaymentsWindowManagerInteractiveUiTest,
                       InvokeUi_Vcn3ds_QueryParamsPresent) {
  ShowUi("Vcn3ds");
  VerifyUi();
  auto* popup_web_contents = GetPopupWebContents();

  // Navigate to a page where there are isComplete and token query params.
  popup_web_contents->OpenURL(content::OpenURLParams(
      GURL("https://site.example/?isComplete=true&token=sometesttoken"),
      content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
      /*is_renderer_initiated=*/false));

  // Close the pop-up to mock the Payments Server closing the pop-up on
  // redirect.
  popup_web_contents->Close();

  base::RunLoop().RunUntilIdle();

  // Check that the flow was successful and an UnmaskCardRequest was triggered
  // with the correct fields set, and the progress dialog was shown.
  auto* autofill_client = client();
  EXPECT_TRUE(autofill_client->autofill_progress_dialog_shown());
  const std::optional<payments::PaymentsNetworkInterface::UnmaskRequestDetails>&
      unmask_request = static_cast<payments::TestPaymentsNetworkInterface*>(
                           autofill_client->GetPaymentsNetworkInterface())
                           ->unmask_request();
  ASSERT_TRUE(unmask_request.has_value());
  EXPECT_EQ(unmask_request->card, card_);
  EXPECT_EQ(unmask_request->redirect_completion_proof.value(), "sometesttoken");
  EXPECT_EQ(unmask_request->last_committed_primary_main_frame_origin,
            client()->GetLastCommittedPrimaryMainFrameOrigin().GetURL());
  EXPECT_EQ(unmask_request->context_token,
            test_api(window_manager()).GetVcn3dsContext()->context_token);
  ASSERT_TRUE(unmask_request->selected_challenge_option.has_value());
  EXPECT_EQ(unmask_request->selected_challenge_option->url_to_open,
            kVcn3dsTestUrl);
  EXPECT_EQ(unmask_request->selected_challenge_option->id,
            test_api(window_manager()).GetVcn3dsContext()->challenge_option.id);
}

// Test that the VCN 3DS pop-up is shown correctly, and on close an
// UnmaskCardRequest is not triggered if the query params indicate the
// authentication failed.
IN_PROC_BROWSER_TEST_F(DesktopPaymentsWindowManagerInteractiveUiTest,
                       InvokeUi_Vcn3ds_QueryParams_AuthenticationFailed) {
  ShowUi("Vcn3ds");
  VerifyUi();

  auto* popup_web_contents = GetPopupWebContents();

  // Navigate to a page where there is an isComplete query param that denotes
  // the authentication failed.
  popup_web_contents->OpenURL(content::OpenURLParams(
      GURL("https://site.example/?isComplete=false"), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
      /*is_renderer_initiated=*/false));

  // Close the pop-up to mock the Payments Server closing the pop-up on
  // redirect.
  GetPopupWebContents()->Close();

  base::RunLoop().RunUntilIdle();

  // Check that the flow was ended and no UnmaskCardRequest was triggered.
  const std::optional<payments::PaymentsNetworkInterface::UnmaskRequestDetails>&
      unmask_request = static_cast<payments::TestPaymentsNetworkInterface*>(
                           client()->GetPaymentsNetworkInterface())
                           ->unmask_request();
  ASSERT_FALSE(unmask_request.has_value());
}

// Test that the VCN 3DS pop-up is shown correctly, and on close an
// UnmaskCardRequest is not triggered if there are no query params present.
IN_PROC_BROWSER_TEST_F(DesktopPaymentsWindowManagerInteractiveUiTest,
                       InvokeUi_Vcn3ds_NoQueryParamsAndPopupClosed) {
  ShowUi("Vcn3ds");
  VerifyUi();

  // Close the pop-up to mock the Payments Server closing the pop-up on
  // redirect.
  GetPopupWebContents()->Close();

  base::RunLoop().RunUntilIdle();

  // Check that the flow was ended and no UnmaskCardRequest was triggered.
  const std::optional<payments::PaymentsNetworkInterface::UnmaskRequestDetails>&
      unmask_request = static_cast<payments::TestPaymentsNetworkInterface*>(
                           client()->GetPaymentsNetworkInterface())
                           ->unmask_request();
  ASSERT_FALSE(unmask_request.has_value());
}

// Test that the VCN 3DS pop-up is shown correctly, and on close an
// UnmaskCardRequest is not triggered if the query params are invalid.
IN_PROC_BROWSER_TEST_F(DesktopPaymentsWindowManagerInteractiveUiTest,
                       InvokeUi_Vcn3ds_InvalidQueryParams) {
  ShowUi("Vcn3ds");
  VerifyUi();

  auto* popup_web_contents = GetPopupWebContents();

  // Navigate to a page where there is an isComplete query param but not token
  // query param.
  popup_web_contents->OpenURL(content::OpenURLParams(
      GURL("https://site.example/?isComplete=true"), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
      /*is_renderer_initiated=*/false));

  // Close the pop-up to mock the Payments Server closing the pop-up on
  // redirect.
  GetPopupWebContents()->Close();

  base::RunLoop().RunUntilIdle();

  // Check that the flow was ended and no UnmaskCardRequest was triggered.
  const std::optional<payments::PaymentsNetworkInterface::UnmaskRequestDetails>&
      unmask_request = static_cast<payments::TestPaymentsNetworkInterface*>(
                           client()->GetPaymentsNetworkInterface())
                           ->unmask_request();
  ASSERT_FALSE(unmask_request.has_value());
}

}  // namespace payments

}  // namespace autofill
