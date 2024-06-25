// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/autofill/payments/payments_ui_constants.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager_test_api.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "content/public/test/browser_test.h"

namespace autofill {

class CreditCardAccessManagerBrowserTest : public InProcessBrowserTest {
 public:
  CreditCardAccessManagerBrowserTest() = default;
  ~CreditCardAccessManagerBrowserTest() override = default;

 protected:
  class TestAutofillManager : public BrowserAutofillManager {
   public:
    explicit TestAutofillManager(ContentAutofillDriver* driver)
        : BrowserAutofillManager(driver, "en-US") {}

    testing::AssertionResult WaitForFormsSeen(int min_num_awaited_calls) {
      return forms_seen_waiter_.Wait(min_num_awaited_calls);
    }

   private:
    TestAutofillManagerWaiter forms_seen_waiter_{
        *this,
        {AutofillManagerEvent::kFormsSeen}};
  };

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data/autofill");
    embedded_test_server()->StartAcceptingConnections();

    // Wait for Personal Data Manager to be fully loaded to prevent that
    // spurious notifications deceive the tests.
    WaitForPersonalDataManagerToBeLoaded(browser()->profile());
  }

  TestAutofillManager* GetAutofillManager() {
    return autofill_manager_injector_[web_contents()];
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void NavigateToAndWaitForForm(const GURL& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    ASSERT_TRUE(GetAutofillManager()->WaitForFormsSeen(1));
  }

  CreditCardAccessManager& GetCreditCardAccessManager() {
    ContentAutofillDriver* autofill_driver =
        ContentAutofillDriver::GetForRenderFrameHost(
            web_contents()->GetPrimaryMainFrame());
    return static_cast<BrowserAutofillManager&>(
               autofill_driver->GetAutofillManager())
        .GetCreditCardAccessManager();
  }

 private:
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  TestAutofillManagerInjector<TestAutofillManager> autofill_manager_injector_;
};

IN_PROC_BROWSER_TEST_F(CreditCardAccessManagerBrowserTest,
                       NavigateFromPage_UnmaskedCardCacheResets) {
  // CreditCardAccessManager is completely recreated on page navigation, so to
  // ensure we're not using stale pointers, always re-fetch it on use.
  EXPECT_TRUE(
      test_api(GetCreditCardAccessManager()).UnmaskedCardCacheIsEmpty());
  CreditCard card = test::GetFullServerCard();
  GetCreditCardAccessManager().CacheUnmaskedCardInfo(card, u"123");
  EXPECT_FALSE(
      test_api(GetCreditCardAccessManager()).UnmaskedCardCacheIsEmpty());

  // Cache should reset upon navigation.
  NavigateToAndWaitForForm(
      embedded_test_server()->GetURL("/credit_card_upload_form_cc.html"));
  EXPECT_TRUE(
      test_api(GetCreditCardAccessManager()).UnmaskedCardCacheIsEmpty());
}

}  // namespace autofill
