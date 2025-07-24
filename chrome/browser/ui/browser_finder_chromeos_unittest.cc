// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_finder.h"

#include "ash/public/cpp/multi_user_window_manager.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/multi_user/multi_profile_support.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window_aura.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/user_manager/user.h"
#include "google_apis/gaia/gaia_id.h"
#include "ui/base/ui_base_features.h"

namespace test {

namespace {

constexpr auto kTestAccountId1 =
    AccountId::Literal::FromUserEmailGaiaId("user1@test.com",
                                            GaiaId::Literal("fakegaia"));
constexpr auto kTestAccountId2 =
    AccountId::Literal::FromUserEmailGaiaId("user2@test.com",
                                            GaiaId::Literal("fakegaia2"));

}  // namespace

class BrowserFinderChromeOSTest : public BrowserWithTestWindowTest {
 protected:
  BrowserFinderChromeOSTest() = default;
  BrowserFinderChromeOSTest(const BrowserFinderChromeOSTest&) = delete;
  BrowserFinderChromeOSTest& operator=(const BrowserFinderChromeOSTest&) =
      delete;

 private:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    ash::ProfileHelper::Get();  // Instantiate.

    // Here the primary user/profile is created.
    // *Then*, set up MultiUserWindowManagerHelper. This is to mirror the
    // current implementation of the helper, which is done as a part of the
    // shelf creation. The structure is going to be changed soon, though
    // (crbug.com/4251603989).
    ASSERT_FALSE(MultiUserWindowManagerHelper::GetInstance());
    MultiUserWindowManagerHelper::CreateInstanceForTest();
    MultiUserWindowManagerHelper::GetWindowManager()->SetPrimaryUser(
        kTestAccountId1);
    MultiUserWindowManagerHelper::GetInstance()->AddUser(kTestAccountId1);

    // Create secondary user/profile.
    LogIn(kTestAccountId2.GetUserEmail(), kTestAccountId2.GetGaiaId());
    second_profile_ =
        CreateProfile(std::string(kTestAccountId2.GetUserEmail()));
  }

  void TearDown() override {
    second_profile_ = nullptr;
    MultiUserWindowManagerHelper::DeleteInstance();
    BrowserWithTestWindowTest::TearDown();
  }

  // BrowserWithTestWindow:
  std::optional<std::string> GetDefaultProfileName() override {
    return std::string(kTestAccountId1.GetUserEmail());
  }

  TestingProfile* CreateProfile(const std::string& profile_name) override {
    auto* profile = BrowserWithTestWindowTest::CreateProfile(profile_name);
    auto* user = user_manager()->FindUserAndModify(
        AccountId::FromUserEmail(profile_name));
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user, profile);

    if (auto* helper = MultiUserWindowManagerHelper::GetInstance()) {
      // Second time or later. Explicitly call AddUser is needed to register
      // the user.
      helper->AddUser(user->GetAccountId());
    }
    return profile;
  }

  raw_ptr<TestingProfile> second_profile_;
};

TEST_F(BrowserFinderChromeOSTest, IncognitoBrowserMatchTest) {
  // GetBrowserCount() use kMatchAll to find all browser windows for profile().
  EXPECT_EQ(1u, chrome::GetBrowserCount(profile()));
  EXPECT_TRUE(chrome::FindAnyBrowser(profile(), true));
  EXPECT_TRUE(chrome::FindAnyBrowser(profile(), false));
  set_browser(nullptr);

  // Create an incognito browser.
  Browser::CreateParams params(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true), true);
  std::unique_ptr<Browser> incognito_browser(
      chrome::CreateBrowserWithViewsTestWindowForParams(params));
  // Incognito windows are excluded in GetBrowserCount() because kMatchAll
  // doesn't match original profile of the browser with the given profile.
  EXPECT_EQ(0u, chrome::GetBrowserCount(profile()));
  EXPECT_TRUE(chrome::FindAnyBrowser(profile(), true));
  EXPECT_FALSE(chrome::FindAnyBrowser(profile(), false));
}

TEST_F(BrowserFinderChromeOSTest, FindBrowserOwnedByAnotherProfile) {
  set_browser(nullptr);

  Browser::CreateParams params(profile()->GetOriginalProfile(), true);
  std::unique_ptr<Browser> browser(
      chrome::CreateBrowserWithViewsTestWindowForParams(params));
  MultiUserWindowManagerHelper::GetWindowManager()->SetWindowOwner(
      browser->window()->GetNativeWindow(), kTestAccountId1);
  EXPECT_EQ(1u, chrome::GetBrowserCount(profile()));
  EXPECT_TRUE(chrome::FindAnyBrowser(profile(), true));
  EXPECT_TRUE(chrome::FindAnyBrowser(profile(), false));

  // Move the browser window to another user's desktop. Then no window should
  // be available for the current profile.
  MultiUserWindowManagerHelper::GetWindowManager()->ShowWindowForUser(
      browser->window()->GetNativeWindow(), kTestAccountId2);
  // ShowWindowForUser() notifies chrome async. FlushBindings() to ensure all
  // the changes happen.
  EXPECT_EQ(0u, chrome::GetBrowserCount(profile()));
  EXPECT_FALSE(chrome::FindAnyBrowser(profile(), true));
  EXPECT_FALSE(chrome::FindAnyBrowser(profile(), false));
}

}  // namespace test
