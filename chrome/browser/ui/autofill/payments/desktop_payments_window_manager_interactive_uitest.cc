// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>

#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/payments/desktop_payments_window_manager.h"
#include "chrome/browser/ui/autofill/payments/desktop_payments_window_manager_test_api.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/payments_window_manager.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

constexpr std::string_view kVcn3dsTestUrl = "https://site.example";
constexpr std::string_view kTestContextToken = "Test context token";

class DesktopPaymentsWindowManagerInteractiveUiTest : public UiBrowserTest {
 public:
  DesktopPaymentsWindowManagerInteractiveUiTest() = default;

  void ShowUi(const std::string& name) override {
    if (name == "Vcn3ds") {
      PaymentsWindowManager::Vcn3dsContext context;
      card_ = test::GetVirtualCard();
      context.card = card_;
      context.context_token = kTestContextToken;
      context.url = GURL(kVcn3dsTestUrl);
      window_manager().InitVcn3dsAuthentication(context);
    } else {
      NOTREACHED();
    }
  }

  bool VerifyUi() override {
    // There should be two browser windows present, the original browser and the
    // pop-up's browser.
    if (BrowserList::GetInstance()->size() != 2U) {
      return false;
    }

    auto* source_web_contents = BrowserList::GetInstance()
                                    ->get(0)
                                    ->tab_strip_model()
                                    ->GetActiveWebContents();

    // The pop-up must be created from `source_web_contents`, so it will always
    // be the second browser in the BrowserList.
    auto* popup_web_contents = BrowserList::GetInstance()
                                   ->get(1)
                                   ->tab_strip_model()
                                   ->GetActiveWebContents();

    // This ensures that there is no scripting relationship between the pop-up
    // and the original tab.
    if (source_web_contents->GetSiteInstance()->IsRelatedSiteInstance(
            popup_web_contents->GetSiteInstance())) {
      return false;
    }

    std::string test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();
    if (test_name == "InvokeUi_Vcn3ds") {
      if (popup_web_contents->GetURL() != GURL(kVcn3dsTestUrl)) {
        return false;
      }

      std::optional<PaymentsWindowManager::Vcn3dsContext> context =
          test_api(window_manager()).GetVcn3dsContext();
      if (!context.has_value() || context->card != card_ ||
          context->context_token != kTestContextToken ||
          context->url != GURL(kVcn3dsTestUrl)) {
        return false;
      }
    } else {
      NOTREACHED();
    }

    return true;
  }

  void WaitForUserDismissal() override {}

 private:
  DesktopPaymentsWindowManager& window_manager() {
    auto* client = ChromeAutofillClient::FromWebContentsForTesting(
        browser()->tab_strip_model()->GetActiveWebContents());
    return *static_cast<DesktopPaymentsWindowManager*>(
        client->GetPaymentsWindowManager());
  }

  CreditCard card_;
};

IN_PROC_BROWSER_TEST_F(DesktopPaymentsWindowManagerInteractiveUiTest,
                       InvokeUi_Vcn3ds) {
  ShowAndVerifyUi();
}

}  // namespace autofill::payments
