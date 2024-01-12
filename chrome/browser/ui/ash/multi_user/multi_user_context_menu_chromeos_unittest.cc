// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/test/ash_test_helper.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_profile_support.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_context_menu.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "ui/aura/window.h"
#include "ui/base/models/menu_model.h"

namespace ash {

// A test class for preparing the MultiUserContextMenu.
class MultiUserContextMenuChromeOSTest : public ChromeAshTestBase {
 public:
  MultiUserContextMenuChromeOSTest()
      : fake_user_manager_(new FakeChromeUserManager),
        user_manager_enabler_(base::WrapUnique(fake_user_manager_.get())) {}

  MultiUserContextMenuChromeOSTest(const MultiUserContextMenuChromeOSTest&) =
      delete;
  MultiUserContextMenuChromeOSTest& operator=(
      const MultiUserContextMenuChromeOSTest&) = delete;

  void SetUp() override;
  void TearDown() override;

 protected:
  // Set up the test environment for this many windows.
  void SetUpForThisManyWindows(int windows);

  // Ensures there are |n| logged-in users.
  void SetLoggedInUsers(size_t n) {
    DCHECK_LE(fake_user_manager_->GetLoggedInUsers().size(), n);
    while (fake_user_manager_->GetLoggedInUsers().size() < n) {
      AccountId account_id = AccountId::FromUserEmail(
          base::StringPrintf("generated-user-%" PRIuS "@consumer.example.com",
                             fake_user_manager_->GetLoggedInUsers().size()));
      fake_user_manager_->AddUser(account_id);
      fake_user_manager_->LoginUser(account_id);
    }
  }

  aura::Window* window() { return window_; }

 private:
  // A window which can be used for testing.
  raw_ptr<aura::Window, DanglingUntriaged> window_;

  // Owned by |user_manager_enabler_|.
  raw_ptr<FakeChromeUserManager, DanglingUntriaged> fake_user_manager_ =
      nullptr;
  user_manager::ScopedUserManager user_manager_enabler_;
};

void MultiUserContextMenuChromeOSTest::SetUp() {
  ChromeAshTestBase::SetUp();

  window_ = CreateTestWindowInShellWithId(0);
  window_->Show();

  MultiUserWindowManagerHelper::CreateInstanceForTest(
      AccountId::FromUserEmail("a"));
}

void MultiUserContextMenuChromeOSTest::TearDown() {
  delete window_;

  MultiUserWindowManagerHelper::DeleteInstance();
  ChromeAshTestBase::TearDown();
}

// Check that an unowned window will never create a menu.
TEST_F(MultiUserContextMenuChromeOSTest, UnownedWindow) {
  EXPECT_EQ(nullptr, CreateMultiUserContextMenu(window()).get());

  // Add more users.
  SetLoggedInUsers(2);
  EXPECT_EQ(nullptr, CreateMultiUserContextMenu(window()).get());
}

// Check that an owned window will never create a menu.
TEST_F(MultiUserContextMenuChromeOSTest, OwnedWindow) {
  // Make the window owned and check that there is no menu (since only a single
  // user exists).
  MultiUserWindowManagerHelper::GetWindowManager()->SetWindowOwner(
      window(), AccountId::FromUserEmail("a"));
  EXPECT_EQ(nullptr, CreateMultiUserContextMenu(window()).get());

  // After adding another user a menu should get created.
  {
    SetLoggedInUsers(2);
    std::unique_ptr<ui::MenuModel> menu = CreateMultiUserContextMenu(window());
    ASSERT_TRUE(menu.get());
    EXPECT_EQ(1u, menu.get()->GetItemCount());
  }
  {
    SetLoggedInUsers(3);
    std::unique_ptr<ui::MenuModel> menu = CreateMultiUserContextMenu(window());
    ASSERT_TRUE(menu.get());
    EXPECT_EQ(2u, menu.get()->GetItemCount());
  }
}

}  // namespace ash
