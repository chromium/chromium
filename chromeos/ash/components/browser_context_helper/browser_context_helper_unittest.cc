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
#include "components/account_id/account_id_literal.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr AccountId::Literal kTestAccountId =
    AccountId::Literal::FromUserEmailGaiaId("test@test",
                                            GaiaId::Literal("fakegaia"));

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
  BrowserContextHelperAccountIdTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(ash::features::kUseAnnotatedAccountId);
    } else {
      feature_list_.InitAndDisableFeature(
          ash::features::kUseAnnotatedAccountId);
    }
  }

  ~BrowserContextHelperAccountIdTest() override = default;

  void SetUp() override {
    BrowserContextHelperTest::SetUp();
    ash::test::UserSessionTestEnvironment::RegisterLocalStatePrefs(
        local_state_.registry());
    user_session_test_environment_ =
        std::make_unique<ash::test::UserSessionTestEnvironment>(&local_state_);
  }

  void TearDown() override {
    user_session_test_environment_.reset();
    BrowserContextHelperTest::TearDown();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
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

  // Set up a User and its BrowserContext instance.
  const user_manager::User* user =
      user_session_test_environment_->AddRegularUser(kTestAccountId);
  ASSERT_TRUE(user);
  user_session_test_environment_->LogIn(kTestAccountId);
  const std::string& username_hash = user->username_hash();
  content::BrowserContext* browser_context = delegate_ptr->CreateBrowserContext(
      delegate_ptr->GetUserDataDir()->Append("u-" + username_hash),
      /*is_off_the_record=*/false);
  AnnotatedAccountId::Set(browser_context, kTestAccountId,
                          /*for_test=*/false);
  user_manager::UserManager::Get()->OnUserProfileCreated(kTestAccountId,
                                                         /*prefs=*/nullptr);

  // BrowserContext instance corresponding to the account_id should be returned.
  EXPECT_EQ(browser_context,
            helper.GetBrowserContextByAccountId(kTestAccountId));

  // It returns nullptr, if User instance corresponding to account_id is not
  // found.
  EXPECT_FALSE(helper.GetBrowserContextByAccountId(
      AccountId::FromUserEmail("notfound@test")));

  user_manager::UserManager::Get()->OnUserProfileWillBeDestroyed(
      kTestAccountId);
}

TEST_P(BrowserContextHelperAccountIdTest, GetBrowserContextByUser) {
  // Set up BrowserContextHelper instance.
  auto delegate = std::make_unique<FakeBrowserContextHelperDelegate>();
  auto* delegate_ptr = delegate.get();
  BrowserContextHelper helper(std::move(delegate));

  // Set up a User and its BrowserContext instance.
  const user_manager::User* user =
      user_session_test_environment_->AddRegularUser(kTestAccountId);
  ASSERT_TRUE(user);
  user_session_test_environment_->LogIn(kTestAccountId);
  const std::string& username_hash = user->username_hash();
  content::BrowserContext* browser_context = delegate_ptr->CreateBrowserContext(
      delegate_ptr->GetUserDataDir()->Append("u-" + username_hash),
      /*is_off_the_record=*/false);
  AnnotatedAccountId::Set(browser_context, kTestAccountId,
                          /*for_test=*/false);

  // Before User is marked that its Profile is created, GetBrowserContextByUser
  // should return nullptr.
  EXPECT_FALSE(helper.GetBrowserContextByUser(user));

  // Mark User as if Profile is created.
  user_manager::UserManager::Get()->OnUserProfileCreated(kTestAccountId,
                                                         /*prefs=*/nullptr);

  // Then the appropriate BrowserContext instance should be returned.
  EXPECT_EQ(browser_context, helper.GetBrowserContextByUser(user));

  user_manager::UserManager::Get()->OnUserProfileWillBeDestroyed(
      kTestAccountId);
}

TEST_P(BrowserContextHelperAccountIdTest, GetBrowserContextByUser_Guest) {
  // Set up BrowserContextHelper instance.
  auto delegate = std::make_unique<FakeBrowserContextHelperDelegate>();
  auto* delegate_ptr = delegate.get();
  BrowserContextHelper helper(std::move(delegate));

  // Set up a User and its BrowserContext instance.
  const user_manager::User* user =
      user_session_test_environment_->AddGuestUser();
  ASSERT_TRUE(user);
  const AccountId& account_id = user->GetAccountId();
  user_session_test_environment_->LogIn(account_id);
  const std::string& username_hash = user->username_hash();

  auto* browser_context = delegate_ptr->CreateBrowserContext(
      delegate_ptr->GetUserDataDir()->Append("u-" + username_hash),
      /*is_off_the_record=*/false);
  AnnotatedAccountId::Set(browser_context, account_id,
                          /*for_test=*/false);
  content::BrowserContext* otr_browser_context =
      delegate_ptr->CreateBrowserContext(
          delegate_ptr->GetUserDataDir()->Append("u-" + username_hash),
          /*is_off_the_record=*/true);
  user_manager::UserManager::Get()->OnUserProfileCreated(account_id,
                                                         /*prefs=*/nullptr);

  // Off the record instance should be returned.
  EXPECT_EQ(otr_browser_context, helper.GetBrowserContextByUser(user));

  user_manager::UserManager::Get()->OnUserProfileWillBeDestroyed(account_id);
}

TEST_P(BrowserContextHelperAccountIdTest, GetUserByBrowserContext) {
  // Set up BrowserContextHelper instance.
  auto delegate = std::make_unique<FakeBrowserContextHelperDelegate>();
  auto* delegate_ptr = delegate.get();
  BrowserContextHelper helper(std::move(delegate));

  const user_manager::User* user =
      user_session_test_environment_->AddRegularUser(kTestAccountId);
  ASSERT_TRUE(user);
  user_session_test_environment_->LogIn(kTestAccountId);
  const std::string& username_hash = user->username_hash();
  content::BrowserContext* browser_context = delegate_ptr->CreateBrowserContext(
      delegate_ptr->GetUserDataDir()->Append("u-" + username_hash),
      /*is_off_the_record=*/false);
  AnnotatedAccountId::Set(browser_context, kTestAccountId,
                          /*for_test=*/false);
  user_manager::UserManager::Get()->OnUserProfileCreated(kTestAccountId,
                                                         /*prefs=*/nullptr);

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

  user_manager::UserManager::Get()->OnUserProfileWillBeDestroyed(
      kTestAccountId);
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
