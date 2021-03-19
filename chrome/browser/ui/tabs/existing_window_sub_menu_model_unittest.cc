// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/existing_window_sub_menu_model.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "ui/gfx/text_elider.h"

namespace {

class ExistingWindowSubMenuModelTest : public BrowserWithTestWindowTest {
 public:
  ExistingWindowSubMenuModelTest() : BrowserWithTestWindowTest() {}

 protected:
  std::unique_ptr<Browser> CreateTestBrowser(bool incognito, bool popup);
  void AddTabWithTitle(Browser* browser, std::string title);

  static void CheckBrowserTitle(const std::u16string& title,
                                const std::string& expected_title,
                                const int expected_num_tabs);
};

std::unique_ptr<Browser> ExistingWindowSubMenuModelTest::CreateTestBrowser(
    bool incognito,
    bool popup) {
  TestBrowserWindow* window = new TestBrowserWindow;
  new TestBrowserWindowOwner(window);
  Profile* profile = incognito ? browser()->profile()->GetPrimaryOTRProfile()
                               : browser()->profile();
  Browser::Type type = popup ? Browser::TYPE_POPUP : Browser::TYPE_NORMAL;

  std::unique_ptr<Browser> browser =
      CreateBrowser(profile, type, false, window);
  BrowserList::SetLastActive(browser.get());
  return browser;
}

void ExistingWindowSubMenuModelTest::AddTabWithTitle(Browser* browser,
                                                     std::string title) {
  AddTab(browser, GURL("about:blank"));
  NavigateAndCommitActiveTabWithTitle(browser, GURL("about:blank"),
                                      base::ASCIIToUTF16(title));
}

// static
void ExistingWindowSubMenuModelTest::CheckBrowserTitle(
    const std::u16string& title,
    const std::string& expected_title,
    const int expected_num_tabs) {
  const std::u16string expected_title16 = base::ASCIIToUTF16(expected_title);

  // Check the suffix, which should always show if there are multiple tabs.
  if (expected_num_tabs > 1) {
    std::ostringstream oss;
    oss << " and " << expected_num_tabs - 1;
    oss << ((expected_num_tabs == 2) ? " other tab" : " other tabs");
    const std::u16string expected_suffix16 = base::ASCIIToUTF16(oss.str());

    // Not case sensitive, since MacOS uses title case.
    EXPECT_TRUE(base::EndsWith(title, expected_suffix16,
                               base::CompareCase::INSENSITIVE_ASCII));
  }

  // Either the title contains the whole tab title, or it was elided.
  if (!base::StartsWith(title, expected_title16,
                        base::CompareCase::SENSITIVE)) {
    // Check that the title before being elided matches the tab title.
    std::vector<std::u16string> tokens =
        SplitString(title, gfx::kEllipsisUTF16, base::KEEP_WHITESPACE,
                    base::SPLIT_WANT_NONEMPTY);
    EXPECT_TRUE(base::StartsWith(expected_title16, tokens[0],
                                 base::CompareCase::SENSITIVE));

    // Title should always have at least a few characters.
    EXPECT_GE(tokens[0].size(), 3ull);
  }
}

// Ensure that the move to existing window menu only appears when another window
// of the current profile exists.
TEST_F(ExistingWindowSubMenuModelTest, ShouldShowSubmenu) {
  // Shouldn't show menu for one window.
  ASSERT_FALSE(ExistingWindowSubMenuModel::ShouldShowSubmenu(profile()));

  // Add another browser, and make sure we do show the menu now.
  std::unique_ptr<Browser> browser_2(CreateTestBrowser(false, false));
  ASSERT_TRUE(ExistingWindowSubMenuModel::ShouldShowSubmenu(profile()));

  // Close the window, so the menu does not show anymore.
  BrowserList::RemoveBrowser(browser_2.get());
  ASSERT_FALSE(ExistingWindowSubMenuModel::ShouldShowSubmenu(profile()));
}

// Ensure we only show the menu on incognito when at least one other incognito
// window exists.
TEST_F(ExistingWindowSubMenuModelTest, ShouldShowSubmenuIncognito) {
  // Shouldn't show menu for one window.
  ASSERT_FALSE(ExistingWindowSubMenuModel::ShouldShowSubmenu(profile()));
  ASSERT_FALSE(ExistingWindowSubMenuModel::ShouldShowSubmenu(
      profile()->GetPrimaryOTRProfile()));

  // Create an incognito browser. We shouldn't show the menu, because we only
  // move tabs between windows of the same profile.
  std::unique_ptr<Browser> incognito_browser_1(CreateTestBrowser(true, false));
  ASSERT_FALSE(ExistingWindowSubMenuModel::ShouldShowSubmenu(profile()));
  ASSERT_FALSE(ExistingWindowSubMenuModel::ShouldShowSubmenu(
      profile()->GetPrimaryOTRProfile()));

  // Add another incognito browser, and make sure we do show the menu now.
  std::unique_ptr<Browser> incognito_browser_2(CreateTestBrowser(true, false));
  ASSERT_FALSE(ExistingWindowSubMenuModel::ShouldShowSubmenu(profile()));
  ASSERT_TRUE(ExistingWindowSubMenuModel::ShouldShowSubmenu(
      profile()->GetPrimaryOTRProfile()));
}

// Ensure we don't show the menu on a popup window.
TEST_F(ExistingWindowSubMenuModelTest, ShouldShowSubmenuPopup) {
  // Popup windows aren't counted when determining whether to show the menu.
  std::unique_ptr<Browser> browser_2(CreateTestBrowser(false, true));
  ASSERT_FALSE(ExistingWindowSubMenuModel::ShouldShowSubmenu(profile()));

  // Add another tabbed window, make sure the menu shows.
  std::unique_ptr<Browser> browser_3(CreateTestBrowser(false, false));
  ASSERT_TRUE(ExistingWindowSubMenuModel::ShouldShowSubmenu(profile()));
}

// Validate that windows appear in MRU order and with the expected labels.
TEST_F(ExistingWindowSubMenuModelTest, BuildSubmenuOrder) {
  // Add some browsers.
  BrowserList::SetLastActive(browser());
  std::unique_ptr<Browser> browser_2(CreateTestBrowser(false, false));
  std::unique_ptr<Browser> browser_3(CreateTestBrowser(false, false));
  std::unique_ptr<Browser> browser_4(CreateTestBrowser(false, false));

  // Add tabs.
  constexpr char kLongTabTitleExample[] =
      "Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Maecenas "
      "porttitor congue massa. Fusce posuere, magna sed pulvinar ultricies,"
      "purus lectus malesuada libero, sit amet commodo magna eros quis urna.";

  AddTabWithTitle(browser(), "Browser 1");
  AddTabWithTitle(browser_2.get(), kLongTabTitleExample);
  AddTabWithTitle(browser_3.get(), "Browser 3 Tab 1");
  AddTabWithTitle(browser_3.get(), "Browser 3 Tab 2");
  AddTabWithTitle(browser_4.get(), "Browser 4 Tab 1");
  AddTabWithTitle(browser_4.get(), "Browser 4 Tab 2");
  AddTabWithTitle(browser_4.get(), kLongTabTitleExample);

  // Create menu from browser 1.
  ExistingWindowSubMenuModel menu1(nullptr, browser()->tab_strip_model(), 0);
  ASSERT_EQ(5, menu1.GetItemCount());
  CheckBrowserTitle(menu1.GetLabelAt(2), kLongTabTitleExample, 3);
  CheckBrowserTitle(menu1.GetLabelAt(3), "Browser 3 Tab 2", 2);
  CheckBrowserTitle(menu1.GetLabelAt(4), kLongTabTitleExample, 1);

  // Create menu from browser 2.
  ExistingWindowSubMenuModel menu2(nullptr, browser_2->tab_strip_model(), 0);
  ASSERT_EQ(5, menu2.GetItemCount());
  CheckBrowserTitle(menu2.GetLabelAt(2), kLongTabTitleExample, 3);
  CheckBrowserTitle(menu2.GetLabelAt(3), "Browser 3 Tab 2", 2);
  CheckBrowserTitle(menu2.GetLabelAt(4), "Browser 1", 1);

  // Rearrange the MRU and re-test.
  BrowserList::SetLastActive(browser());
  BrowserList::SetLastActive(browser_2.get());

  ExistingWindowSubMenuModel menu3(nullptr, browser_3->tab_strip_model(), 0);
  ASSERT_EQ(5, menu3.GetItemCount());
  CheckBrowserTitle(menu3.GetLabelAt(2), kLongTabTitleExample, 1);
  CheckBrowserTitle(menu3.GetLabelAt(3), "Browser 1", 1);
  CheckBrowserTitle(menu3.GetLabelAt(4), kLongTabTitleExample, 3);

  // Clean up.
  chrome::CloseTab(browser_2.get());
  chrome::CloseTab(browser_3.get());
  chrome::CloseTab(browser_3.get());
  chrome::CloseTab(browser_4.get());
  chrome::CloseTab(browser_4.get());
  chrome::CloseTab(browser_4.get());
}

// Ensure that normal browsers and incognito browsers have their own lists.
TEST_F(ExistingWindowSubMenuModelTest, BuildSubmenuIncognito) {
  // Add some browsers.
  BrowserList::SetLastActive(browser());
  std::unique_ptr<Browser> browser_2(CreateTestBrowser(false, false));
  std::unique_ptr<Browser> browser_3(CreateTestBrowser(false, false));
  std::unique_ptr<Browser> incognito_browser_1(CreateTestBrowser(true, false));
  std::unique_ptr<Browser> incognito_browser_2(CreateTestBrowser(true, false));

  AddTabWithTitle(browser(), "Browser 1");
  AddTabWithTitle(browser_2.get(), "Browser 2");
  AddTabWithTitle(browser_3.get(), "Browser 3");
  AddTabWithTitle(incognito_browser_1.get(), "Incognito Browser 1");
  AddTabWithTitle(incognito_browser_2.get(), "Incognito Browser 2");

  const std::u16string kBrowser2ExpectedTitle = u"Browser 2";
  const std::u16string kBrowser3ExpectedTitle = u"Browser 3";
  const std::u16string kIncognitoBrowser2ExpectedTitle = u"Incognito Browser 2";

  // Test that a non-incognito browser only shows non-incognito windows.
  ExistingWindowSubMenuModel menu(nullptr, browser()->tab_strip_model(), 0);
  ASSERT_EQ(4, menu.GetItemCount());
  ASSERT_EQ(kBrowser3ExpectedTitle, menu.GetLabelAt(2));
  ASSERT_EQ(kBrowser2ExpectedTitle, menu.GetLabelAt(3));

  // Test that a incognito browser only shows incognito windows.
  ExistingWindowSubMenuModel menu_incognito(
      nullptr, incognito_browser_1->tab_strip_model(), 0);
  ASSERT_EQ(3, menu_incognito.GetItemCount());
  ASSERT_EQ(kIncognitoBrowser2ExpectedTitle, menu_incognito.GetLabelAt(2));

  // Clean up.
  chrome::CloseTab(browser_2.get());
  chrome::CloseTab(browser_3.get());
  chrome::CloseTab(incognito_browser_1.get());
  chrome::CloseTab(incognito_browser_2.get());
}

// Ensure that popups don't appear in the list of existing windows.
TEST_F(ExistingWindowSubMenuModelTest, BuildSubmenuPopups) {
  // Add some browsers.
  BrowserList::SetLastActive(browser());
  std::unique_ptr<Browser> browser_2(CreateTestBrowser(false, false));
  std::unique_ptr<Browser> browser_3(CreateTestBrowser(false, false));
  std::unique_ptr<Browser> popup_browser_1(CreateTestBrowser(false, true));
  std::unique_ptr<Browser> popup_browser_2(CreateTestBrowser(false, true));

  AddTabWithTitle(browser(), "Browser 1");
  AddTabWithTitle(browser_2.get(), "Browser 2");
  AddTabWithTitle(browser_3.get(), "Browser 3");

  const std::u16string kBrowser2ExpectedTitle = u"Browser 2";
  const std::u16string kBrowser3ExpectedTitle = u"Browser 3";

  // Test that popups do not show.
  ExistingWindowSubMenuModel menu(nullptr, browser()->tab_strip_model(), 0);
  ASSERT_EQ(4, menu.GetItemCount());
  ASSERT_EQ(kBrowser3ExpectedTitle, menu.GetLabelAt(2));
  ASSERT_EQ(kBrowser2ExpectedTitle, menu.GetLabelAt(3));

  // Clean up.
  chrome::CloseTab(browser_2.get());
  chrome::CloseTab(browser_3.get());
  chrome::CloseTab(popup_browser_1.get());
  chrome::CloseTab(popup_browser_2.get());
}

}  // namespace
