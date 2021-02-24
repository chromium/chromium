// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/entity_match.h"

#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/commander/command_source.h"
#include "chrome/browser/ui/commander/entity_match.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"

namespace commander {

class CommanderEntityMatchTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    BrowserList::GetInstance()->SetLastActive(browser());
  }

  // Creates and returns a browser with `title` as its user title.
  // If `profile` is provided, it is used, otherwise uses the profile of this
  // test's browser.
  std::unique_ptr<Browser> CreateAndActivateBrowser(
      const std::string& title,
      Profile* browser_profile = nullptr) {
    Browser::CreateParams params(browser_profile ? browser_profile : profile(),
                                 true);
    auto browser = CreateBrowserWithTestWindowForParams(params);
    browser->SetWindowUserTitle(title);
    BrowserList::GetInstance()->SetLastActive(browser.get());
    return browser;
  }
};

TEST_F(CommanderEntityMatchTest, WindowExcludesCurrentBrowser) {
  std::string title("Title");
  browser()->SetWindowUserTitle(title);
  auto other_browser = CreateAndActivateBrowser(title);

  auto matches =
      WindowsMatchingInput(browser(), base::UTF8ToUTF16(title), false);
  EXPECT_EQ(matches.size(), 1u);
}

TEST_F(CommanderEntityMatchTest, WindowIncludesAllProfilesIfUnrestricted) {
  std::string title("Title");

  auto same_profile_browser = CreateAndActivateBrowser(title);
  TestingProfile* other_profile =
      profile_manager()->CreateTestingProfile("other");
  auto other_profile_browser = CreateAndActivateBrowser(title, other_profile);

  auto matches =
      WindowsMatchingInput(browser(), base::UTF8ToUTF16(title), false);
  EXPECT_EQ(matches.size(), 2u);
}

TEST_F(CommanderEntityMatchTest, WindowOmitsNonmatchingProfilesIfRestricted) {
  std::string title("Title");
  auto same_profile_browser = CreateAndActivateBrowser(title);

  TestingProfile* other_profile =
      profile_manager()->CreateTestingProfile("other");
  auto other_profile_browser = CreateAndActivateBrowser(title, other_profile);

  auto matches =
      WindowsMatchingInput(browser(), base::UTF8ToUTF16(title), true);
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches.at(0).browser, same_profile_browser.get());
}

TEST_F(CommanderEntityMatchTest, WindowOnlyIncludesMatches) {
  auto browser_with_match = CreateAndActivateBrowser("Orange juice");
  auto browser_without_match = CreateAndActivateBrowser("Aqua regia");

  auto matches =
      WindowsMatchingInput(browser(), base::ASCIIToUTF16("orange"), true);
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches.at(0).browser, browser_with_match.get());
}

TEST_F(CommanderEntityMatchTest, WindowRanksMatches) {
  auto browser_best_match = CreateAndActivateBrowser("Orange juice");
  auto browser_good_match =
      CreateAndActivateBrowser("Oracular Nouns Gesture Electrically");

  auto matches =
      WindowsMatchingInput(browser(), base::ASCIIToUTF16("orange"), true);
  ASSERT_EQ(matches.size(), 2u);
  base::ranges::sort(matches, std::greater<>(), &WindowMatch::score);
  EXPECT_EQ(matches.at(0).browser, browser_best_match.get());
}

TEST_F(CommanderEntityMatchTest, WindowMRUOrderWithNoInput) {
  auto browser1 = CreateAndActivateBrowser("Beep");
  auto browser2 = CreateAndActivateBrowser("Boop");

  // Browser 2 was activated last, so we expect it to be the top match.
  auto matches = WindowsMatchingInput(browser(), base::ASCIIToUTF16(""), true);
  ASSERT_EQ(matches.size(), 2u);
  base::ranges::sort(matches, std::greater<>(), &WindowMatch::score);
  EXPECT_EQ(matches.at(0).browser, browser2.get());

  BrowserList::GetInstance()->SetLastActive(browser1.get());
  // Activating browser 1 should have brought it to the top.
  matches = WindowsMatchingInput(browser(), base::ASCIIToUTF16(""), true);
  ASSERT_EQ(matches.size(), 2u);
  base::ranges::sort(matches, std::greater<>(), &WindowMatch::score);
  EXPECT_EQ(matches.at(0).browser, browser1.get());
}

}  // namespace commander
