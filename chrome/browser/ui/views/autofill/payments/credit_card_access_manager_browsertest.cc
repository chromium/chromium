// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/autofill/payments/payments_ui_constants.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/test_autofill_manager.h"
#include "content/public/test/browser_test.h"

namespace autofill {

class CreditCardAccessManagerBrowserTest : public InProcessBrowserTest {
 public:
  CreditCardAccessManagerBrowserTest() = default;
  ~CreditCardAccessManagerBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data/autofill");
    embedded_test_server()->StartAcceptingConnections();

    // Wait for Personal Data Manager to be fully loaded to prevent that
    // spurious notifications deceive the tests.
    WaitForPersonalDataManagerToBeLoaded(browser()->profile());
  }

  CreditCardAccessManager* GetCreditCardAccessManager() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ContentAutofillDriver* autofill_driver =
        ContentAutofillDriverFactory::FromWebContents(web_contents)
            ->DriverForFrame(web_contents->GetMainFrame());
    return autofill_driver->autofill_manager()->credit_card_access_manager();
  }

  CreditCard SaveServerCard(std::string card_number) {
    CreditCard server_card;
    test::SetCreditCardInfo(&server_card, "John Smith", card_number.c_str(),
                            "12", test::NextYear().c_str(), "1");
    server_card.set_guid("00000000-0000-0000-0000-" +
                         card_number.substr(0, 12));
    server_card.set_record_type(CreditCard::FULL_SERVER_CARD);
    server_card.set_server_id("full_id_" + card_number);
    AddTestServerCreditCard(browser()->profile(), server_card);
    return server_card;
  }
};

IN_PROC_BROWSER_TEST_F(CreditCardAccessManagerBrowserTest,
                       NavigateFromPage_UnmaskedCardCacheResets) {
  CreditCard card = SaveServerCard("1223122348859229");

  // CreditCardAccessManager is completely recreated on page navigation, so to
  // ensure we're not using stale pointers, always re-fetch it on use.
  EXPECT_TRUE(GetCreditCardAccessManager()->UnmaskedCardCacheIsEmpty());
  GetCreditCardAccessManager()->CacheUnmaskedCardInfo(card, u"123");
  EXPECT_FALSE(GetCreditCardAccessManager()->UnmaskedCardCacheIsEmpty());

  // Cache should reset upon navigation.
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/credit_card_upload_form_cc.html"));
  EXPECT_TRUE(GetCreditCardAccessManager()->UnmaskedCardCacheIsEmpty());
}

}  // namespace autofill
