// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_context_menu.h"

#include <memory>

#include "ash/multi_user/multi_user_window_manager.h"
#include "ash/shell.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/login/users/scoped_account_id_annotator.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_browser_adaptor.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#include "ui/aura/window.h"
#include "ui/base/models/menu_model.h"

namespace ash {
namespace {

constexpr auto kAccountId1 =
    AccountId::Literal::FromUserEmailGaiaId("test1@test",
                                            GaiaId::Literal("test1"));
constexpr auto kAccountId2 =
    AccountId::Literal::FromUserEmailGaiaId("test2@test",
                                            GaiaId::Literal("test2"));
constexpr auto kAccountId3 =
    AccountId::Literal::FromUserEmailGaiaId("test3@test",
                                            GaiaId::Literal("test3"));

}  // namespace

// A test class for preparing the MultiUserContextMenu.
class MultiUserContextMenuChromeOSTest : public ChromeAshTestBase {
 public:
  MultiUserContextMenuChromeOSTest() = default;
  MultiUserContextMenuChromeOSTest(const MultiUserContextMenuChromeOSTest&) =
      delete;
  MultiUserContextMenuChromeOSTest& operator=(
      const MultiUserContextMenuChromeOSTest&) = delete;

  void SetUp() override {
    user_manager_.Reset(std::make_unique<user_manager::UserManagerImpl>(
        std::make_unique<user_manager::FakeUserManagerDelegate>(),
        TestingBrowserProcess::GetGlobal()->GetTestingLocalState(),
        /*cros_settings=*/nullptr));

    user_manager::TestHelper test_helper(user_manager_.Get());
    ASSERT_TRUE(test_helper.AddRegularUser(kAccountId1));
    ASSERT_TRUE(test_helper.AddRegularUser(kAccountId2));
    ASSERT_TRUE(test_helper.AddRegularUser(kAccountId3));

    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());

    ChromeAshTestBase::SetUp();

    ash::ProfileHelper::Get();  // Instantiate
    LogIn(kAccountId1);

    window_.reset(CreateTestWindowInShell({.window_id = 0}));
    window_->Show();

    multi_user_window_manager_browser_adaptor_ =
        std::make_unique<MultiUserWindowManagerBrowserAdaptor>(
            ash::Shell::Get()->multi_user_window_manager());
    multi_user_window_manager_browser_adaptor_->AddUser(kAccountId1);
  }

  void TearDown() override {
    window_.reset();
    multi_user_window_manager_browser_adaptor_.reset();
    ChromeAshTestBase::TearDown();
    for (Profile* profile :
         testing_profile_manager_->profile_manager()->GetLoadedProfiles()) {
      const auto* account_id = ash::AnnotatedAccountId::Get(profile);
      if (account_id) {
        user_manager_->OnUserProfileWillBeDestroyed(*account_id);
      }
    }
    testing_profile_manager_.reset();
    user_manager_.Reset();
  }

 protected:
  void LogIn(const AccountId& account_id) {
    user_manager_->UserLoggedIn(
        account_id, user_manager::TestHelper::GetFakeUsernameHash(account_id));
    ash::ScopedAccountIdAnnotator annotator(
        testing_profile_manager_->profile_manager(), account_id);
    TestingProfile* profile = testing_profile_manager_->CreateTestingProfile(
        account_id.GetUserEmail());
    user_manager_->OnUserProfileCreated(account_id, profile->GetPrefs());
  }

  aura::Window* window() { return window_.get(); }

 private:
  user_manager::ScopedUserManager user_manager_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  std::unique_ptr<ash::MultiUserWindowManagerBrowserAdaptor>
      multi_user_window_manager_browser_adaptor_;

  // A window which can be used for testing.
  std::unique_ptr<aura::Window> window_;
};

// Check that an unowned window will never create a menu.
TEST_F(MultiUserContextMenuChromeOSTest, UnownedWindow) {
  EXPECT_EQ(nullptr, CreateMultiUserContextMenu(window()).get());

  // Add more users.
  LogIn(kAccountId2);
  EXPECT_EQ(nullptr, CreateMultiUserContextMenu(window()).get());
}

// Check that an owned window will never create a menu.
TEST_F(MultiUserContextMenuChromeOSTest, OwnedWindow) {
  // Make the window owned and check that there is no menu (since only a single
  // user exists).
  ash::Shell::Get()->multi_user_window_manager()->SetWindowOwner(window(),
                                                                 kAccountId1);
  EXPECT_EQ(nullptr, CreateMultiUserContextMenu(window()).get());

  // After adding another user a menu should get created.
  LogIn(kAccountId2);
  {
    std::unique_ptr<ui::MenuModel> menu = CreateMultiUserContextMenu(window());
    ASSERT_TRUE(menu);
    EXPECT_EQ(1u, menu->GetItemCount());
  }

  LogIn(kAccountId3);
  {
    std::unique_ptr<ui::MenuModel> menu = CreateMultiUserContextMenu(window());
    ASSERT_TRUE(menu);
    EXPECT_EQ(2u, menu->GetItemCount());
  }
}

}  // namespace ash
