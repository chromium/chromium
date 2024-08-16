// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_actions.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/address_bubbles_controller.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/actions/actions.h"
#include "url/url_constants.h"

namespace chrome {

class BrowserActionsBrowserTest : public InProcessBrowserTest {
 public:
  BrowserActionsBrowserTest() = default;

 protected:
  raw_ptr<content::WebContents> GetActiveWebContents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  base::test::ScopedFeatureList feature_list_{features::kToolbarPinning};
};

IN_PROC_BROWSER_TEST_F(BrowserActionsBrowserTest, ShowAddressesBubbleOrPage) {
  auto& action_manager = actions::ActionManager::GetForTesting();
  const GURL addresses_url = GURL("chrome://settings/addresses");

  ASSERT_NE(GetActiveWebContents()->GetURL(), addresses_url);
  action_manager.FindAction(kActionShowAddressesBubbleOrPage)->InvokeAction();
  EXPECT_EQ(GetActiveWebContents()->GetURL(), addresses_url);

  autofill::AddressBubblesController::CreateForWebContents(
      GetActiveWebContents());
  auto* bubble_controller = autofill::AddressBubblesController::FromWebContents(
      GetActiveWebContents());
  ASSERT_EQ(bubble_controller->GetBubbleView(), nullptr);
  autofill::AddressBubblesController::SetUpAndShowAddNewAddressBubble(
      GetActiveWebContents(), base::DoNothing());
  ASSERT_NE(bubble_controller->GetBubbleView(), nullptr);
  action_manager.FindAction(kActionShowAddressesBubbleOrPage)->InvokeAction();
  EXPECT_EQ(bubble_controller->GetBubbleView(), nullptr);

  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  ASSERT_NE(GetActiveWebContents()->GetURL(), addresses_url);
  action_manager.FindAction(kActionShowAddressesBubbleOrPage)->InvokeAction();
  EXPECT_EQ(GetActiveWebContents()->GetURL(), addresses_url);
}

IN_PROC_BROWSER_TEST_F(BrowserActionsBrowserTest, ShowPaymentsBubbleOrPage) {
  CHECK(ui_test_utils::BringBrowserWindowToFront(browser()));
  auto& action_manager = actions::ActionManager::GetForTesting();
  const GURL payments_url = GURL("chrome://settings/payments");

  ASSERT_NE(GetActiveWebContents()->GetURL(), payments_url);
  action_manager.FindAction(kActionShowPaymentsBubbleOrPage)->InvokeAction();
  EXPECT_EQ(GetActiveWebContents()->GetURL(), payments_url);

  autofill::SaveCardBubbleControllerImpl::CreateForWebContents(
      GetActiveWebContents());
  autofill::SaveCardBubbleControllerImpl* bubble_controller =
      autofill::SaveCardBubbleControllerImpl::FromWebContents(
          GetActiveWebContents());
  CHECK(bubble_controller);

  ASSERT_EQ(bubble_controller->GetPaymentBubbleView(), nullptr);
  bubble_controller->OfferLocalSave(
      autofill::test::GetCreditCard(),
      autofill::payments::PaymentsAutofillClient::SaveCreditCardOptions()
          .with_show_prompt(true),
      base::DoNothing());
  ASSERT_NE(bubble_controller->GetPaymentBubbleView(), nullptr);
  action_manager.FindAction(kActionShowPaymentsBubbleOrPage)->InvokeAction();
  EXPECT_EQ(bubble_controller->GetPaymentBubbleView(), nullptr);
}

}  // namespace chrome
