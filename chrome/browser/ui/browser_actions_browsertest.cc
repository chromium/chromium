// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_actions.h"

#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/actions_util.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/address_bubbles_controller.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
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
  auto* const root_action_item =
      BrowserActions::From(browser())->root_action_item();
  const GURL addresses_url = GURL("chrome://settings/addresses");

  ASSERT_NE(GetActiveWebContents()->GetURL(), addresses_url);
  action_manager.FindAction(kActionShowAddressesBubbleOrPage, root_action_item)
      ->InvokeAction();
  EXPECT_EQ(GetActiveWebContents()->GetURL(), addresses_url);

  autofill::AddressBubblesController::CreateForWebContents(
      GetActiveWebContents());
  auto* bubble_controller = autofill::AddressBubblesController::FromWebContents(
      GetActiveWebContents());
  ASSERT_EQ(bubble_controller->GetBubbleView(), nullptr);
  action_manager.FindAction(kActionShowAddressesBubbleOrPage, root_action_item)
      ->InvokeAction();
  EXPECT_EQ(bubble_controller->GetBubbleView(), nullptr);

  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                     GURL(url::kAboutBlankURL)));
  ASSERT_NE(GetActiveWebContents()->GetURL(), addresses_url);
  action_manager.FindAction(kActionShowAddressesBubbleOrPage, root_action_item)
      ->InvokeAction();
  EXPECT_EQ(GetActiveWebContents()->GetURL(), addresses_url);
}

IN_PROC_BROWSER_TEST_F(BrowserActionsBrowserTest, ShowPaymentsBubbleOrPage) {
  CHECK(ui_test_utils::BringBrowserWindowToFront(browser()));
  auto& action_manager = actions::ActionManager::GetForTesting();
  auto* const root_action_item =
      BrowserActions::From(browser())->root_action_item();
  const GURL payments_url = GURL("chrome://settings/payments");

  ASSERT_NE(GetActiveWebContents()->GetURL(), payments_url);
  action_manager.FindAction(kActionShowPaymentsBubbleOrPage, root_action_item)
      ->InvokeAction();
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
  action_manager.FindAction(kActionShowPaymentsBubbleOrPage, root_action_item)
      ->InvokeAction();
  EXPECT_EQ(bubble_controller->GetPaymentBubbleView(), nullptr);
}

IN_PROC_BROWSER_TEST_F(BrowserActionsBrowserTest,
                       NewIncognitoWindowEnabledState) {
  auto& action_manager = actions::ActionManager::GetForTesting();
  auto* const root_action_item =
      BrowserActions::From(browser())->root_action_item();
  EXPECT_TRUE(
      action_manager.FindAction(kActionNewIncognitoWindow, root_action_item)
          ->GetEnabled());

  // Set Incognito to DISABLED and verify the action item is updated.
  IncognitoModePrefs::SetAvailability(
      browser()->GetProfile()->GetPrefs(),
      policy::IncognitoModeAvailability::kDisabled);
  EXPECT_FALSE(
      action_manager.FindAction(kActionNewIncognitoWindow, root_action_item)
          ->GetEnabled());
}

IN_PROC_BROWSER_TEST_F(BrowserActionsBrowserTest, DidCreateBrowserActions) {
  BrowserActions* browser_actions = BrowserActions::From(browser());
  auto& action_manager = actions::ActionManager::GetForTesting();

  std::vector<actions::ActionId> browser_action_ids = {
      kActionNewIncognitoWindow, kActionPrint,
      kActionClearBrowsingData,  kActionTaskManager,
      kActionDevTools,           kActionSendTabToSelf,
      kActionQrCodeGenerator,    kActionShowAddressesBubbleOrPage,
      kActionFederation,         kActionCycleToNextTab,
      kActionCycleToPrevTab,     kActionShowReadingModeSidePanel,
      kActionBookmarksSubmenu,   kActionOpenInChrome,
      kActionWebAppMenuAppInfo,  kActionUninstallWebApp};

  ASSERT_NE(browser_actions->root_action_item(), nullptr);

  for (actions::ActionId action_id : browser_action_ids) {
    EXPECT_NE(action_manager.FindAction(action_id,
                                        browser_actions->root_action_item()),
              nullptr);
  }
}

IN_PROC_BROWSER_TEST_F(BrowserActionsBrowserTest, CycleTabs) {
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(2, tab_strip->count());
  EXPECT_TRUE(tab_strip->IsTabSelected(1));

  auto& action_manager = actions::ActionManager::GetForTesting();
  auto* const root_action_item =
      BrowserActions::From(browser())->root_action_item();
  action_manager.FindAction(kActionCycleToNextTab, root_action_item)
      ->InvokeAction();
  EXPECT_TRUE(tab_strip->IsTabSelected(0));

  action_manager.FindAction(kActionCycleToPrevTab, root_action_item)
      ->InvokeAction();
  EXPECT_TRUE(tab_strip->IsTabSelected(1));
}

IN_PROC_BROWSER_TEST_F(BrowserActionsBrowserTest,
                       CheckBrowserActionsEnabledState) {
  BrowserActions* browser_actions = BrowserActions::From(browser());
  auto& action_manager = actions::ActionManager::GetForTesting();
  auto* const root_action_item = browser_actions->root_action_item();

  ASSERT_NE(root_action_item, nullptr);

  EXPECT_EQ(
      action_manager.FindAction(kActionNewIncognitoWindow, root_action_item)
          ->GetEnabled(),
      true);
  EXPECT_EQ(
      action_manager.FindAction(kActionClearBrowsingData, root_action_item)
          ->GetEnabled(),
      true);
  EXPECT_EQ(action_manager.FindAction(kActionTaskManager, root_action_item)
                ->GetEnabled(),
            true);
  EXPECT_EQ(action_manager.FindAction(kActionDevTools, root_action_item)
                ->GetEnabled(),
            true);
  EXPECT_EQ(
      action_manager.FindAction(kActionPrint, root_action_item)->GetEnabled(),
      CanPrint(browser()));
  EXPECT_EQ(action_manager.FindAction(kActionSendTabToSelf, root_action_item)
                ->GetEnabled(),
            CanSendTabToSelf(browser()));
  EXPECT_EQ(action_manager.FindAction(kActionQrCodeGenerator, root_action_item)
                ->GetEnabled(),
            false);
  EXPECT_EQ(action_manager
                .FindAction(kActionShowAddressesBubbleOrPage, root_action_item)
                ->GetEnabled(),
            true);
}

IN_PROC_BROWSER_TEST_F(BrowserActionsBrowserTest, GetCleanText) {
  // \u2026 is the unicode hex value for a horizontal ellipsis.
  const std::u16string expected = u"Print";
  std::u16string input = u"&Print\u2026";
  std::u16string output = GetCleanText(input);
  EXPECT_EQ(output, expected);

  std::u16string input_middle_amp = u"Pri&nt\u2026";
  std::u16string output_middle_amp = GetCleanText(input_middle_amp);
  EXPECT_EQ(output_middle_amp, expected);

  std::u16string input_ellipsis_text = u"&Print...";
  std::u16string output_ellipsis_text = GetCleanText(input_ellipsis_text);
  EXPECT_EQ(output_ellipsis_text, expected);
}

IN_PROC_BROWSER_TEST_F(BrowserActionsBrowserTest, GetCleanTextEllipsesOnly) {
  // The ellipsis is removed, but the mnemonic marker is kept. \u2026 is the
  // unicode hex value for a horizontal ellipsis.
  EXPECT_EQ(GetCleanText(u"&Print\u2026", CleanTextOptions::kEllipses),
            u"&Print");
  EXPECT_EQ(GetCleanText(u"Pri&nt...", CleanTextOptions::kEllipses), u"Pri&nt");
  // "&&" is a literal "&", and is kept as is.
  EXPECT_EQ(GetCleanText(u"Q&&A", CleanTextOptions::kEllipses), u"Q&&A");
}

IN_PROC_BROWSER_TEST_F(BrowserActionsBrowserTest, GetCleanTextMnemonicOnly) {
  // The mnemonic marker is removed, but the ellipsis is kept.
  EXPECT_EQ(GetCleanText(u"&Print\u2026", CleanTextOptions::kMnemonic),
            u"Print\u2026");
  EXPECT_EQ(GetCleanText(u"Pri&nt...", CleanTextOptions::kMnemonic),
            u"Print...");
}

// Menu actions keep the mnemonic marker in their text for menus, while
// GetText() stays clean for other surfaces, such as the toolbar and tooltips.
IN_PROC_BROWSER_TEST_F(BrowserActionsBrowserTest, MenuActionsKeepMnemonicText) {
  auto& action_manager = actions::ActionManager::GetForTesting();
  auto* const root_action_item =
      BrowserActions::From(browser())->root_action_item();

  // Built with ChromeMenuAction().
  actions::ActionItem* incognito =
      action_manager.FindAction(kActionNewIncognitoWindow, root_action_item);
  ASSERT_TRUE(incognito);
  const std::u16string incognito_title =
      l10n_util::GetStringUTF16(IDS_NEW_INCOGNITO_WINDOW);
  EXPECT_EQ(incognito->GetTextWithMnemonic(), incognito_title);
  EXPECT_EQ(incognito->GetText(), GetCleanText(incognito_title));

  // Built with ActionItem::Builder() directly.
  actions::ActionItem* new_tab =
      action_manager.FindAction(kActionNewTab, root_action_item);
  ASSERT_TRUE(new_tab);
  const std::u16string new_tab_title = l10n_util::GetStringUTF16(IDS_NEW_TAB);
  EXPECT_EQ(new_tab->GetTextWithMnemonic(), new_tab_title);
  EXPECT_EQ(new_tab->GetText(), GetCleanText(new_tab_title));

  actions::ActionItem* show_history =
      action_manager.FindAction(kActionShowHistory, root_action_item);
  ASSERT_TRUE(show_history);
  const std::u16string history_title =
      l10n_util::GetStringUTF16(IDS_HISTORY_MENU);
  EXPECT_EQ(show_history->GetTextWithMnemonic(),
            GetCleanText(history_title, CleanTextOptions::kEllipses));
  EXPECT_EQ(show_history->GetText(), GetCleanText(history_title));

  actions::ActionItem* manage_extensions =
      action_manager.FindAction(kActionManageExtensions, root_action_item);
  ASSERT_TRUE(manage_extensions);
  const std::u16string manage_extensions_title =
      l10n_util::GetStringUTF16(IDS_MANAGE_EXTENSIONS);
  EXPECT_EQ(manage_extensions->GetTextWithMnemonic(),
            GetCleanText(manage_extensions_title, CleanTextOptions::kEllipses));
  EXPECT_EQ(manage_extensions->GetText(),
            GetCleanText(manage_extensions_title));
}

}  // namespace chrome
