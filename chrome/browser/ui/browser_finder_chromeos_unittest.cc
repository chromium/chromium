// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_finder.h"

#include "ash/public/cpp/multi_user_window_manager.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/multi_user/multi_profile_support.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window_aura.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "ui/base/ui_base_features.h"

namespace test {

namespace {

const char kTestAccount1[] = "user1@test.com";
const char kTestAccount2[] = "user2@test.com";

}  // namespace

class BrowserFinderChromeOSTest : public BrowserWithTestWindowTest {
 protected:
  BrowserFinderChromeOSTest()
      : fake_user_manager_(new chromeos::FakeChromeUserManager),
        user_manager_enabler_(base::WrapUnique(fake_user_manager_)) {}

  TestingProfile* CreateMultiUserProfile(const AccountId& account_id) {
    TestingProfile* profile =
        profile_manager()->CreateTestingProfile(account_id.GetUserEmail());
    const user_manager::User* user = fake_user_manager_->AddUser(account_id);
    chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        const_cast<user_manager::User*>(user), profile);
    chromeos::ProfileHelper::Get()->SetProfileToUserMappingForTesting(
        const_cast<user_manager::User*>(user));
    // Force creation of MultiProfileSupport.
    GetMultiUserWindowManager();
    MultiProfileSupport::GetInstanceForTest()->AddUser(profile);
    return profile;
  }

  ash::MultiUserWindowManager* GetMultiUserWindowManager() {
    if (!MultiUserWindowManagerHelper::GetInstance())
      MultiUserWindowManagerHelper::CreateInstanceForTest(test_account_id1_);
    return MultiUserWindowManagerHelper::GetWindowManager();
  }

  AccountId test_account_id1_ = EmptyAccountId();
  AccountId test_account_id2_ = EmptyAccountId();

 private:
  void SetUp() override {
    test_account_id1_ = AccountId::FromUserEmail(kTestAccount1);
    test_account_id2_ = AccountId::FromUserEmail(kTestAccount2);
    BrowserWithTestWindowTest::SetUp();
    second_profile_ = CreateMultiUserProfile(test_account_id2_);
  }

  void TearDown() override {
    MultiUserWindowManagerHelper::DeleteInstance();
    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile* CreateProfile() override {
    return CreateMultiUserProfile(test_account_id1_);
  }

  TestingProfile* second_profile_;

  // |fake_user_manager_| is owned by |user_manager_enabler_|
  chromeos::FakeChromeUserManager* fake_user_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFinderChromeOSTest);
};

TEST_F(BrowserFinderChromeOSTest, IncognitoBrowserMatchTest) {
  // GetBrowserCount() use kMatchAll to find all browser windows for profile().
  EXPECT_EQ(1u, chrome::GetBrowserCount(profile()));
  EXPECT_TRUE(chrome::FindAnyBrowser(profile(), true));
  EXPECT_TRUE(chrome::FindAnyBrowser(profile(), false));
  set_browser(nullptr);

  // Create an incognito browser.
  Browser::CreateParams params(profile()->GetOffTheRecordProfile(), true);
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
  GetMultiUserWindowManager()->SetWindowOwner(
      browser->window()->GetNativeWindow(), test_account_id1_);
  EXPECT_EQ(1u, chrome::GetBrowserCount(profile()));
  EXPECT_TRUE(chrome::FindAnyBrowser(profile(), true));
  EXPECT_TRUE(chrome::FindAnyBrowser(profile(), false));

  // Move the browser window to another user's desktop. Then no window should
  // be available for the current profile.
  GetMultiUserWindowManager()->ShowWindowForUser(
      browser->window()->GetNativeWindow(), test_account_id2_);
  // ShowWindowForUser() notifies chrome async. FlushBindings() to ensure all
  // the changes happen.
  EXPECT_EQ(0u, chrome::GetBrowserCount(profile()));
  EXPECT_FALSE(chrome::FindAnyBrowser(profile(), true));
  EXPECT_FALSE(chrome::FindAnyBrowser(profile(), false));
}

}  // namespace test
