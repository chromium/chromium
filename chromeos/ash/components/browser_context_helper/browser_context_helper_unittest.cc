// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/components/browser_context_helper/fake_browser_context_helper_delegate.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

class BrowserContextHelperTest : public testing::Test {
 public:
  BrowserContextHelperTest() = default;
  ~BrowserContextHelperTest() override = default;

 private:
  // Sets up fake UI thread, required by TestBrowserContext.
  content::BrowserTaskEnvironment env_;
};

// Parameterized for UseAnnotatedAccountId.
class BrowserContextHelperAccountIdTest
    : public BrowserContextHelperTest,
      public ::testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(ash::features::kUseAnnotatedAccountId);
    } else {
      feature_list_.InitAndDisableFeature(
          ash::features::kUseAnnotatedAccountId);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

TEST_F(BrowserContextHelperTest, GetUserIdHashFromBrowserContext) {
  // If nullptr is passed, returns an error.
  EXPECT_EQ("", BrowserContextHelper::GetUserIdHashFromBrowserContext(nullptr));

  constexpr struct {
    const char* expect;
    const char* path;
  } kTestData[] = {
      // Regular case. Use relative path, as temporary directory is created
      // there.
      {"abcde123456", "home/chronos/u-abcde123456"},

      // Special case for legacy path.
      {"user", "home/chronos/user"},

      // Special case for testing profile.
      {"test-user", "home/chronos/test-user"},

      // Error case. Data directory must start with "u-".
      {"", "abcde123456"},
  };
  for (const auto& test_case : kTestData) {
    content::TestBrowserContext context(base::FilePath(test_case.path));
    EXPECT_EQ(test_case.expect,
              BrowserContextHelper::GetUserIdHashFromBrowserContext(&context));
  }
}

TEST_P(BrowserContextHelperAccountIdTest, GetBrowserContextByAccountId) {
  // Set up BrowserContextHelper instance.
  auto delegate = std::make_unique<FakeBrowserContextHelperDelegate>();
  auto* delegate_ptr = delegate.get();
  BrowserContextHelper helper(std::move(delegate));

  // Set up UserManager.
  user_manager::ScopedUserManager scoped_user_manager(
      std::make_unique<user_manager::FakeUserManager>());
  auto* fake_user_manager = static_cast<user_manager::FakeUserManager*>(
      user_manager::UserManager::Get());

  // Set up a User and its BrowserContext instance.
  const AccountId account_id = AccountId::FromUserEmail("test@test");
  const std::string username_hash =
      user_manager::FakeUserManager::GetFakeUsernameHash(account_id);
  fake_user_manager->AddUser(account_id);
  fake_user_manager->UserLoggedIn(account_id, username_hash,
                                  /*browser_restart=*/false,
                                  /*is_child=*/false);
  content::BrowserContext* browser_context = delegate_ptr->CreateBrowserContext(
      delegate_ptr->GetUserDataDir()->Append("u-" + username_hash),
      /*is_off_the_record=*/false);
  AnnotatedAccountId::Set(browser_context, account_id,
                          /*for_test=*/false);
  fake_user_manager->OnUserProfileCreated(account_id, /*prefs=*/nullptr);

  // BrowserContext instance corresponding to the account_id should be returned.
  EXPECT_EQ(browser_context, helper.GetBrowserContextByAccountId(account_id));

  // It returns nullptr, if User instance corresponding to account_id is not
  // found.
  EXPECT_FALSE(helper.GetBrowserContextByAccountId(
      AccountId::FromUserEmail("notfound@test")));

  fake_user_manager->OnUserProfileWillBeDestroyed(account_id);
}

TEST_P(BrowserContextHelperAccountIdTest, GetBrowserContextByUser) {
  // Set up BrowserContextHelper instance.
  auto delegate = std::make_unique<FakeBrowserContextHelperDelegate>();
  auto* delegate_ptr = delegate.get();
  BrowserContextHelper helper(std::move(delegate));

  // Set up UserManager.
  user_manager::ScopedUserManager scoped_user_manager(
      std::make_unique<user_manager::FakeUserManager>());
  auto* fake_user_manager = static_cast<user_manager::FakeUserManager*>(
      user_manager::UserManager::Get());

  // Set up a User and its BrowserContext instance.
  const AccountId account_id = AccountId::FromUserEmail("test@test");
  const std::string username_hash =
      user_manager::FakeUserManager::GetFakeUsernameHash(account_id);
  const user_manager::User* user = fake_user_manager->AddUser(account_id);
  fake_user_manager->UserLoggedIn(account_id, username_hash,
                                  /*browser_restart=*/false,
                                  /*is_child=*/false);
  content::BrowserContext* browser_context = delegate_ptr->CreateBrowserContext(
      delegate_ptr->GetUserDataDir()->Append("u-" + username_hash),
      /*is_off_the_record=*/false);
  AnnotatedAccountId::Set(browser_context, account_id,
                          /*for_test=*/false);

  // Before User is marked that its Profile is created, GetBrowserContextByUser
  // should return nullptr.
  EXPECT_FALSE(helper.GetBrowserContextByUser(user));

  // Mark User as if Profile is created.
  fake_user_manager->OnUserProfileCreated(account_id, /*prefs=*/nullptr);

  // Then the appropriate BrowserContext instance should be returned.
  EXPECT_EQ(browser_context, helper.GetBrowserContextByUser(user));

  fake_user_manager->OnUserProfileWillBeDestroyed(account_id);
}

TEST_P(BrowserContextHelperAccountIdTest, GetBrowserContextByUser_Guest) {
  // Set up BrowserContextHelper instance.
  auto delegate = std::make_unique<FakeBrowserContextHelperDelegate>();
  auto* delegate_ptr = delegate.get();
  BrowserContextHelper helper(std::move(delegate));

  // Set up UserManager.
  user_manager::ScopedUserManager scoped_user_manager(
      std::make_unique<user_manager::FakeUserManager>());
  auto* fake_user_manager = static_cast<user_manager::FakeUserManager*>(
      user_manager::UserManager::Get());

  // Set up a User and its BrowserContext instance.
  const AccountId account_id = AccountId::FromUserEmail("guest@guest");
  const std::string username_hash =
      user_manager::FakeUserManager::GetFakeUsernameHash(account_id);
  const user_manager::User* user = fake_user_manager->AddGuestUser(account_id);
  fake_user_manager->UserLoggedIn(account_id, username_hash,
                                  /*browser_restart=*/false,
                                  /*is_child=*/false);

  auto* browser_context = delegate_ptr->CreateBrowserContext(
      delegate_ptr->GetUserDataDir()->Append("u-" + username_hash),
      /*is_off_the_record=*/false);
  AnnotatedAccountId::Set(browser_context, account_id,
                          /*for_test=*/false);
  content::BrowserContext* otr_browser_context =
      delegate_ptr->CreateBrowserContext(
          delegate_ptr->GetUserDataDir()->Append("u-" + username_hash),
          /*is_off_the_record=*/true);
  fake_user_manager->OnUserProfileCreated(account_id, /*prefs=*/nullptr);

  // Off the record instance should be returned.
  EXPECT_EQ(otr_browser_context, helper.GetBrowserContextByUser(user));

  fake_user_manager->OnUserProfileWillBeDestroyed(account_id);
}

TEST_P(BrowserContextHelperAccountIdTest, GetUserByBrowserContext) {
  // Set up BrowserContextHelper instance.
  auto delegate = std::make_unique<FakeBrowserContextHelperDelegate>();
  auto* delegate_ptr = delegate.get();
  BrowserContextHelper helper(std::move(delegate));

  // Set up UserManager.
  user_manager::ScopedUserManager scoped_user_manager(
      std::make_unique<user_manager::FakeUserManager>());
  auto* fake_user_manager = static_cast<user_manager::FakeUserManager*>(
      user_manager::UserManager::Get());

  const AccountId account_id = AccountId::FromUserEmail("test@test");
  const std::string username_hash =
      user_manager::FakeUserManager::GetFakeUsernameHash(account_id);
  const user_manager::User* user = fake_user_manager->AddUser(account_id);
  fake_user_manager->UserLoggedIn(account_id, username_hash,
                                  /*browser_restart=*/false,
                                  /*is_child=*/false);
  content::BrowserContext* browser_context = delegate_ptr->CreateBrowserContext(
      delegate_ptr->GetUserDataDir()->Append("u-" + username_hash),
      /*is_off_the_record=*/false);
  AnnotatedAccountId::Set(browser_context, account_id,
                          /*for_test=*/false);
  fake_user_manager->OnUserProfileCreated(account_id, /*prefs=*/nullptr);

  EXPECT_EQ(user, helper.GetUserByBrowserContext(browser_context));

  // Special browser_context.
  content::BrowserContext* signin_browser_context =
      delegate_ptr->CreateBrowserContext(
          delegate_ptr->GetUserDataDir()->Append(kSigninBrowserContextBaseName),
          /*is_off_the_record=*/false);
  EXPECT_FALSE(helper.GetUserByBrowserContext(signin_browser_context));

  // Returns nullptr for unknown browser context.
  content::BrowserContext* unknown_browser_context =
      delegate_ptr->CreateBrowserContext(
          delegate_ptr->GetUserDataDir()->Append("unknown@user"),
          /*is_off_the_record=*/false);
  AnnotatedAccountId::Set(unknown_browser_context,
                          AccountId::FromUserEmail("unknown@test"),
                          /*for_test=*/false);
  EXPECT_FALSE(helper.GetUserByBrowserContext(unknown_browser_context));

  fake_user_manager->OnUserProfileWillBeDestroyed(account_id);
}

INSTANTIATE_TEST_SUITE_P(All,
                         BrowserContextHelperAccountIdTest,
                         ::testing::Bool());

TEST_F(BrowserContextHelperTest, GetUserBrowserContextDirName) {
  constexpr struct {
    const char* expect;
    const char* user_id_hash;
  } kTestData[] = {
      // Regular case.
      {"u-abcde123456", "abcde123456"},

      // Special case for the legacy path.
      {"user", "user"},

      // Special case for testing.
      {"test-user", "test-user"},
  };
  for (const auto& test_case : kTestData) {
    EXPECT_EQ(test_case.expect,
              BrowserContextHelper::GetUserBrowserContextDirName(
                  test_case.user_id_hash));
  }
}

TEST_F(BrowserContextHelperTest, GetBrowserContextPathByUserIdHash) {
  auto delegate = std::make_unique<FakeBrowserContextHelperDelegate>();
  auto* delegate_ptr = delegate.get();
  BrowserContextHelper helper(std::move(delegate));

  // u- prefix is expected. See GetUserBrowserContextDirName for details.
  EXPECT_EQ(delegate_ptr->GetUserDataDir()->Append("u-0123456789"),
            helper.GetBrowserContextPathByUserIdHash("0123456789"));
  // Special use name case.
  EXPECT_EQ(delegate_ptr->GetUserDataDir()->Append("user"),
            helper.GetBrowserContextPathByUserIdHash("user"));
  EXPECT_EQ(delegate_ptr->GetUserDataDir()->Append("test-user"),
            helper.GetBrowserContextPathByUserIdHash("test-user"));
}

TEST_F(BrowserContextHelperTest, GetSigninBrowserContext) {
  auto delegate = std::make_unique<FakeBrowserContextHelperDelegate>();
  auto* delegate_ptr = delegate.get();
  BrowserContextHelper helper(std::move(delegate));

  // If not yet loaded, GetSigninBrowserContext() should return nullptr.
  EXPECT_FALSE(helper.GetSigninBrowserContext());

  // Load the signin browser context.
  delegate_ptr->CreateBrowserContext(
      delegate_ptr->GetUserDataDir()->Append(kSigninBrowserContextBaseName),
      /*is_off_the_record=*/false);

  // Then it should start returning the instance.
  auto* signin_browser_context = helper.GetSigninBrowserContext();
  ASSERT_TRUE(signin_browser_context);
  EXPECT_TRUE(IsSigninBrowserContext(signin_browser_context));
  EXPECT_TRUE(signin_browser_context->IsOffTheRecord());
}

TEST_F(BrowserContextHelperTest, DeprecatedGetOrCreateSigninBrowserContext) {
  BrowserContextHelper helper(
      std::make_unique<FakeBrowserContextHelperDelegate>());

  // DeprecatedGetOrCreateSigninBrowserContext() should create the instance,
  // if it is not yet.
  auto* signin_browser_context =
      helper.DeprecatedGetOrCreateSigninBrowserContext();
  ASSERT_TRUE(signin_browser_context);
  // Other than that, it should work in the same way with
  // GetSigninBrowserContext().
  EXPECT_EQ(helper.GetSigninBrowserContext(), signin_browser_context);
}

TEST_F(BrowserContextHelperTest, GetLockScreenBrowserContext) {
  auto delegate = std::make_unique<FakeBrowserContextHelperDelegate>();
  auto* delegate_ptr = delegate.get();
  BrowserContextHelper helper(std::move(delegate));

  // If not yet loaded, GetLockScreenBrowserContext() should return nullptr.
  EXPECT_FALSE(helper.GetLockScreenBrowserContext());

  // Load the lock screen browser context.
  delegate_ptr->CreateBrowserContext(
      delegate_ptr->GetUserDataDir()->Append(kLockScreenBrowserContextBaseName),
      /*is_off_the_record=*/false);

  // Then it should start returning the instance.
  auto* lock_screen_browser_context = helper.GetLockScreenBrowserContext();
  ASSERT_TRUE(lock_screen_browser_context);
  EXPECT_TRUE(IsLockScreenBrowserContext(lock_screen_browser_context));
  EXPECT_TRUE(lock_screen_browser_context->IsOffTheRecord());
}

}  // namespace ash
