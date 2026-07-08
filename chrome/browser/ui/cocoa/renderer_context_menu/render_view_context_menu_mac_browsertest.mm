// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/renderer_context_menu/render_view_context_menu_mac.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

bool MenuHasItemWithCommand(const ui::MenuModel& menu, int command) {
  for (size_t i = 0; i < menu.GetItemCount(); ++i) {
    if (menu.GetTypeAt(i) == ui::MenuModel::TYPE_SUBMENU) {
      ui::MenuModel* submenu = menu.GetSubmenuModelAt(i);
      if (MenuHasItemWithCommand(*submenu, command)) {
        return true;
      }
    }
    if (menu.GetCommandIdAt(i) == command) {
      return true;
    }
  }
  return false;
}

class RenderViewContextMenuMacBrowserTest : public InProcessBrowserTest {
 public:
  RenderViewContextMenuMacBrowserTest() = default;

  SpellcheckService* GetSpellcheckService() {
    return SpellcheckServiceFactory::GetForContext(browser()->GetProfile());
  }

  std::unique_ptr<RenderViewContextMenuMac> CreateMenu(
      std::u16string selection_text,
      std::u16string misspelled_word = std::u16string()) {
    content::ContextMenuParams params;
    params.is_editable = true;
    params.selection_text = selection_text;
    params.misspelled_word = misspelled_word;

    auto menu = std::make_unique<RenderViewContextMenuMac>(
        *browser()
             ->tab_strip_model()
             ->GetActiveWebContents()
             ->GetPrimaryMainFrame(),
        params, /*is_paste_enabled=*/false,
        /*is_paste_and_match_style_enabled=*/false);
    menu->InitToolkitMenu();
    return menu;
  }
};

// Tests that the "Remove from Dictionary" command appears when a word that has
// been added to the dictionary by the user is detected, and that selecting it
// removes the word from the user's dictionary.
IN_PROC_BROWSER_TEST_F(RenderViewContextMenuMacBrowserTest,
                       ExecuteRemoveFromDictionary) {
  SpellcheckService* spellcheck_service = GetSpellcheckService();
  ASSERT_TRUE(spellcheck_service);

  std::u16string word = u"customword";
  spellcheck_service->GetCustomDictionary()->AddWord(base::UTF16ToUTF8(word));
  ASSERT_TRUE(spellcheck_service->GetCustomDictionary()->HasWord(
      base::UTF16ToUTF8(word)));

  auto menu = CreateMenu(word);
  EXPECT_TRUE(MenuHasItemWithCommand(menu->menu_model(),
                                     IDC_SPELLCHECK_REMOVE_FROM_DICTIONARY));

  menu->ExecuteCommand(IDC_SPELLCHECK_REMOVE_FROM_DICTIONARY, 0);
  EXPECT_FALSE(spellcheck_service->GetCustomDictionary()->HasWord(
      base::UTF16ToUTF8(word)));
}

// Verifies that the "Remove from dictionary" option is not shown when a word
// is identified as a misspelling.
IN_PROC_BROWSER_TEST_F(RenderViewContextMenuMacBrowserTest,
                       RemoveFromDictionaryNotShownForMisspelledWord) {
  SpellcheckService* spellcheck_service = GetSpellcheckService();
  ASSERT_TRUE(spellcheck_service);

  std::u16string word = u"customword";
  spellcheck_service->GetCustomDictionary()->AddWord(base::UTF16ToUTF8(word));
  ASSERT_TRUE(spellcheck_service->GetCustomDictionary()->HasWord(
      base::UTF16ToUTF8(word)));

  auto menu = CreateMenu(word, word);
  EXPECT_FALSE(MenuHasItemWithCommand(menu->menu_model(),
                                      IDC_SPELLCHECK_REMOVE_FROM_DICTIONARY));
}

// Verifies that a selection of a single word with leading/trailing
// whitespace is correctly identified as a user-added word.
IN_PROC_BROWSER_TEST_F(RenderViewContextMenuMacBrowserTest,
                       RemoveFromDictionaryHandlesWhitespaceSelection) {
  SpellcheckService* spellcheck_service = GetSpellcheckService();
  ASSERT_TRUE(spellcheck_service);

  std::u16string word = u"customword";
  spellcheck_service->GetCustomDictionary()->AddWord(base::UTF16ToUTF8(word));
  ASSERT_TRUE(spellcheck_service->GetCustomDictionary()->HasWord(
      base::UTF16ToUTF8(word)));

  auto menu = CreateMenu(u"   customword  \t ");
  EXPECT_TRUE(MenuHasItemWithCommand(menu->menu_model(),
                                     IDC_SPELLCHECK_REMOVE_FROM_DICTIONARY));
}

// Verifies that a word in the custom dictionary but not in the system
// dictionary is correctly identified and can be removed.
IN_PROC_BROWSER_TEST_F(RenderViewContextMenuMacBrowserTest,
                       RemoveFromDictionary_CustomDictOnly) {
  SpellcheckService* spellcheck_service = GetSpellcheckService();
  ASSERT_TRUE(spellcheck_service);

  std::u16string word = u"customwordonly";

  spellcheck_service->GetCustomDictionary()->AddWord(base::UTF16ToUTF8(word));
  ASSERT_TRUE(spellcheck_service->GetCustomDictionary()->HasWord(
      base::UTF16ToUTF8(word)));

  spellcheck_platform::RemoveWord(spellcheck_service->platform_spell_checker(),
                                  word);
  ASSERT_FALSE(spellcheck_platform::IsUserAddedWord(
      spellcheck_service->platform_spell_checker(), word));

  auto menu = CreateMenu(word);
  EXPECT_TRUE(MenuHasItemWithCommand(menu->menu_model(),
                                     IDC_SPELLCHECK_REMOVE_FROM_DICTIONARY));

  menu->ExecuteCommand(IDC_SPELLCHECK_REMOVE_FROM_DICTIONARY, 0);
  EXPECT_FALSE(spellcheck_service->GetCustomDictionary()->HasWord(
      base::UTF16ToUTF8(word)));
  EXPECT_FALSE(spellcheck_platform::IsUserAddedWord(
      spellcheck_service->platform_spell_checker(), word));
}

// Verifies that a word in the system dictionary but not in the custom
// dictionary is correctly identified and can be removed.
IN_PROC_BROWSER_TEST_F(RenderViewContextMenuMacBrowserTest,
                       RemoveFromDictionary_SystemDictOnly) {
  SpellcheckService* spellcheck_service = GetSpellcheckService();
  ASSERT_TRUE(spellcheck_service);

  std::u16string word = u"systemwordonly";
  spellcheck_service->GetCustomDictionary()->RemoveWord(
      base::UTF16ToUTF8(word));
  ASSERT_FALSE(spellcheck_service->GetCustomDictionary()->HasWord(
      base::UTF16ToUTF8(word)));

  spellcheck_platform::AddWord(spellcheck_service->platform_spell_checker(),
                               word);
  ASSERT_TRUE(spellcheck_platform::IsUserAddedWord(
      spellcheck_service->platform_spell_checker(), word));

  auto menu = CreateMenu(word);
  EXPECT_TRUE(MenuHasItemWithCommand(menu->menu_model(),
                                     IDC_SPELLCHECK_REMOVE_FROM_DICTIONARY));

  menu->ExecuteCommand(IDC_SPELLCHECK_REMOVE_FROM_DICTIONARY, 0);
  EXPECT_FALSE(spellcheck_platform::IsUserAddedWord(
      spellcheck_service->platform_spell_checker(), word));
  EXPECT_FALSE(spellcheck_service->GetCustomDictionary()->HasWord(
      base::UTF16ToUTF8(word)));
}

}  // namespace
