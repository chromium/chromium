// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_finder.h"

#include <optional>

#include "ash/multi_user/multi_user_window_manager.h"
#include "ash/shell.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/browser_delegate/browser_controller_impl.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_browser_adaptor.h"
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

    // Create secondary user/profile.
    LogIn(kTestAccountId2.GetUserEmail(), kTestAccountId2.GetGaiaId());
    second_profile_ =
        CreateProfile(std::string(kTestAccountId2.GetUserEmail()));

    browser_controller_.emplace();
  }

  void TearDown() override {
    browser_controller_.reset();
    second_profile_ = nullptr;
    multi_user_window_manager_browser_adaptor_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  // BrowserWithTestWindow:
  void OnAshTestHelperCreated() override {
    multi_user_window_manager_browser_adaptor_ =
        std::make_unique<ash::MultiUserWindowManagerBrowserAdaptor>(
            ash::Shell::Get()->multi_user_window_manager());
  }

  std::optional<std::string> GetDefaultProfileName() override {
    return std::string(kTestAccountId1.GetUserEmail());
  }

  TestingProfile* CreateProfile(const std::string& profile_name) override {
    auto* profile = BrowserWithTestWindowTest::CreateProfile(profile_name);
    auto* user = user_manager()->FindUserAndModify(
        AccountId::FromUserEmail(profile_name));
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user, profile);

    multi_user_window_manager_browser_adaptor_->AddUser(user->GetAccountId());
    return profile;
  }

  std::unique_ptr<ash::MultiUserWindowManagerBrowserAdaptor>
      multi_user_window_manager_browser_adaptor_;
  raw_ptr<TestingProfile> second_profile_;
  std::optional<ash::BrowserControllerImpl> browser_controller_;
};

TEST_F(BrowserFinderChromeOSTest, IncognitoBrowserMatchTest) {
  // GetBrowserCount() use kMatchAll to find all browser windows for profile().
  EXPECT_EQ(1u, chrome::GetBrowserCount(profile()));
  EXPECT_TRUE(chrome::FindAnyBrowser(profile(), true));
  EXPECT_TRUE(chrome::FindAnyBrowser(profile(), false));
  release_browser();

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
  release_browser();

  Browser::CreateParams params(profile()->GetOriginalProfile(), true);
  std::unique_ptr<Browser> browser(
      chrome::CreateBrowserWithViewsTestWindowForParams(params));
  ash::Shell::Get()->multi_user_window_manager()->SetWindowOwner(
      browser->window()->GetNativeWindow(), kTestAccountId1);
  EXPECT_EQ(1u, chrome::GetBrowserCount(profile()));
  EXPECT_TRUE(chrome::FindAnyBrowser(profile(), true));
  EXPECT_TRUE(chrome::FindAnyBrowser(profile(), false));

  // Move the browser window to another user's desktop. Then no window should
  // be available for the current profile.
  ash::Shell::Get()->multi_user_window_manager()->ShowWindowForUser(
      browser->window()->GetNativeWindow(), kTestAccountId2);
  // ShowWindowForUser() notifies chrome async. FlushBindings() to ensure all
  // the changes happen.
  EXPECT_EQ(0u, chrome::GetBrowserCount(profile()));
  EXPECT_FALSE(chrome::FindAnyBrowser(profile(), true));
  EXPECT_FALSE(chrome::FindAnyBrowser(profile(), false));
}

}  // namespace test
