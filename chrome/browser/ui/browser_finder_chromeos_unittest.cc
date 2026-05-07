// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "chrome/test/base/ui_test_utils.h"
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
    browser_controller_.emplace();
    BrowserWithTestWindowTest::SetUp();
    ash::ProfileHelper::Get();  // Instantiate.

    // Create secondary user/profile.
    LogIn(kTestAccountId2.GetUserEmail(), kTestAccountId2.GetGaiaId());
    second_profile_ =
        CreateProfile(std::string(kTestAccountId2.GetUserEmail()));
  }

  void TearDown() override {
    second_profile_ = nullptr;
    multi_user_window_manager_browser_adaptor_.reset();
    BrowserWithTestWindowTest::TearDown();
    browser_controller_.reset();
  }

  // BrowserWithTestWindow:
  void OnAshTestHelperCreated() override {
    multi_user_window_manager_browser_adaptor_ =
        std::make_unique<ash::MultiUserWindowManagerBrowserAdaptor>(
            ash::Shell::Get()->multi_user_window_manager(),
            &browser_controller_.value());
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
  EXPECT_TRUE(ui_test_utils::FindAnyBrowser(profile()));
  EXPECT_TRUE(ui_test_utils::FindAnyBrowser(profile(),
                                            /*match_original_profiles=*/false));
  release_browser();

  // Create an incognito browser.
  Browser::CreateParams params(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true), true);
  std::unique_ptr<Browser> incognito_browser(
      chrome::CreateBrowserWithViewsTestWindowForParams(params));

  // Exact profile match returns nothing (only the incognito browser exists,
  // and it belongs to the OTR profile).
  EXPECT_FALSE(ui_test_utils::FindAnyBrowser(
      profile(), /*match_original_profiles=*/false));

  // But searching by original profile finds the incognito browser.
  EXPECT_TRUE(ui_test_utils::FindAnyBrowser(profile()));
}

TEST_F(BrowserFinderChromeOSTest, FindBrowserOwnedByAnotherProfile) {
  release_browser();

  Browser::CreateParams params(profile()->GetOriginalProfile(), true);
  std::unique_ptr<Browser> browser(
      chrome::CreateBrowserWithViewsTestWindowForParams(params));
  ash::Shell::Get()->multi_user_window_manager()->SetWindowOwner(
      browser->window()->GetNativeWindow(), kTestAccountId1);
  // The browser is shown for the owning user, so FindAnyBrowser finds it
  // regardless of match_original_profiles setting.
  EXPECT_TRUE(ui_test_utils::FindAnyBrowser(profile(),
                                            /*match_original_profiles=*/false));
  EXPECT_TRUE(ui_test_utils::FindAnyBrowser(profile()));

  // Move the browser window to another user's desktop.
  ash::Shell::Get()->multi_user_window_manager()->ShowWindowForUser(
      browser->window()->GetNativeWindow(), kTestAccountId2);
  // ShowWindowForUser() notifies chrome async.
  // FindAnyBrowser filters by multi-user window visibility on ChromeOS.
  // A window shown on another user's desktop is excluded regardless of
  // match_original_profiles setting.
  EXPECT_FALSE(ui_test_utils::FindAnyBrowser(
      profile(), /*match_original_profiles=*/false));
  EXPECT_FALSE(ui_test_utils::FindAnyBrowser(profile()));
}

}  // namespace test
