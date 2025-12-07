// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_actions.h"

#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/address_bubbles_controller.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/user_prefs/user_prefs.h"
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

IN_PROC_BROWSER_TEST_F(BrowserActionsBrowserTest,
                       NewIncognitoWindowEnabledState) {
  auto& action_manager = actions::ActionManager::GetForTesting();
  EXPECT_TRUE(
      action_manager.FindAction(kActionNewIncognitoWindow)->GetEnabled());

  // Set Incognito to DISABLED and verify the action item is updated.
  IncognitoModePrefs::SetAvailability(
      browser()->profile()->GetPrefs(),
      policy::IncognitoModeAvailability::kDisabled);
  EXPECT_FALSE(
      action_manager.FindAction(kActionNewIncognitoWindow)->GetEnabled());
}

IN_PROC_BROWSER_TEST_F(BrowserActionsBrowserTest, DidCreateBrowserActions) {
  BrowserActions* browser_actions = browser()->browser_actions();
  auto& action_manager = actions::ActionManager::GetForTesting();

  std::vector<actions::ActionId> browser_action_ids = {
      kActionNewIncognitoWindow, kActionPrint,
      kActionClearBrowsingData,  kActionTaskManager,
      kActionDevTools,           kActionSendTabToSelf,
      kActionQrCodeGenerator,    kActionShowAddressesBubbleOrPage};

  ASSERT_NE(browser_actions->root_action_item(), nullptr);

  for (actions::ActionId action_id : browser_action_ids) {
    EXPECT_NE(action_manager.FindAction(action_id), nullptr);
  }
}

IN_PROC_BROWSER_TEST_F(BrowserActionsBrowserTest,
                       CheckBrowserActionsEnabledState) {
  BrowserActions* browser_actions = browser()->browser_actions();
  auto& action_manager = actions::ActionManager::GetForTesting();

  ASSERT_NE(browser_actions->root_action_item(), nullptr);

  EXPECT_EQ(action_manager.FindAction(kActionNewIncognitoWindow)->GetEnabled(),
            true);
  EXPECT_EQ(action_manager.FindAction(kActionClearBrowsingData)->GetEnabled(),
            true);
  EXPECT_EQ(action_manager.FindAction(kActionTaskManager)->GetEnabled(), true);
  EXPECT_EQ(action_manager.FindAction(kActionDevTools)->GetEnabled(), true);
  EXPECT_EQ(action_manager.FindAction(kActionPrint)->GetEnabled(),
            CanPrint(browser()));
  EXPECT_EQ(action_manager.FindAction(kActionSendTabToSelf)->GetEnabled(),
            CanSendTabToSelf(browser()));
  EXPECT_EQ(action_manager.FindAction(kActionQrCodeGenerator)->GetEnabled(),
            false);
  EXPECT_EQ(
      action_manager.FindAction(kActionShowAddressesBubbleOrPage)->GetEnabled(),
      true);
}

IN_PROC_BROWSER_TEST_F(BrowserActionsBrowserTest, GetCleanTitleAndTooltipText) {
  // \u2026 is the unicode hex value for a horizontal ellipsis.
  const std::u16string expected = u"Print";
  std::u16string input = u"&Print\u2026";
  std::u16string output = BrowserActions::GetCleanTitleAndTooltipText(input);
  EXPECT_EQ(output, expected);

  std::u16string input_middle_amp = u"Pri&nt\u2026";
  std::u16string output_middle_amp =
      BrowserActions::GetCleanTitleAndTooltipText(input_middle_amp);
  EXPECT_EQ(output_middle_amp, expected);

  std::u16string input_ellipsis_text = u"&Print...";
  std::u16string output_ellipsis_text =
      BrowserActions::GetCleanTitleAndTooltipText(input_ellipsis_text);
  EXPECT_EQ(output_ellipsis_text, expected);
}

}  // namespace chrome
