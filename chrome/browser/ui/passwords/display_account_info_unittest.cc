// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/task_environment.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync/test/test_sync_user_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kTestEmail[] = "john.doe@gmail.com";
const char kTestFullName[] = "John Doe";
}  // namespace

class DisplayAccountInfoTest : public testing::Test {
 public:
  DisplayAccountInfoTest() {
    // `identity_test_environment_` starts signed-out by default, but
    // `sync_service_` starts signed-in, make them consistent.
    sync_service_.SetSignedOut();
  }

  void SignIn() {
    AccountInfo account_info =
        identity_test_environment_.MakePrimaryAccountAvailable(
            kTestEmail, signin::ConsentLevel::kSignin);
    account_info.full_name = kTestFullName;
    identity_test_environment_.UpdateAccountInfoForAccount(account_info);
    sync_service_.SetSignedIn(signin::ConsentLevel::kSignin, account_info);
  }

  // Must only be called when there is a signed-in account.
  void SetCanDisplaySignedInAccountEmail(bool can_display) {
    CoreAccountInfo core_account_info =
        identity_manager()->GetPrimaryAccountInfo(
            signin::ConsentLevel::kSignin);
    ASSERT_FALSE(core_account_info.IsEmpty())
        << "Trying to update capability of signed-in account when there's none";
    AccountInfo account_info =
        identity_manager()->FindExtendedAccountInfo(core_account_info);
    AccountCapabilitiesTestMutator(&account_info.capabilities)
        .set_can_have_email_address_displayed(can_display);
    identity_test_environment_.UpdateAccountInfoForAccount(account_info);
  }

  syncer::SyncService* sync_service() { return &sync_service_; }

  signin::IdentityManager* identity_manager() {
    return identity_test_environment_.identity_manager();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  syncer::TestSyncService sync_service_;
  signin::IdentityTestEnvironment identity_test_environment_;
};

TEST_F(DisplayAccountInfoTest, SignedOut) {
  EXPECT_EQ(
      GetAccountInfoForPasswordMessages(sync_service(), identity_manager()),
      std::nullopt);
  EXPECT_EQ(GetDisplayableAccountName(sync_service(), identity_manager()),
            std::string());
}

TEST_F(DisplayAccountInfoTest, SignedInWithPasswordsEnabled) {
  SignIn();

  std::optional<AccountInfo> account_info =
      GetAccountInfoForPasswordMessages(sync_service(), identity_manager());
  ASSERT_TRUE(account_info.has_value());
  EXPECT_EQ(account_info->email, kTestEmail);
  EXPECT_EQ(GetDisplayableAccountName(sync_service(), identity_manager()),
            kTestEmail);
}

TEST_F(DisplayAccountInfoTest, SignedInWithPasswordsDisabled) {
  SignIn();
  sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, false);

  EXPECT_EQ(
      GetAccountInfoForPasswordMessages(sync_service(), identity_manager()),
      std::nullopt);
  EXPECT_EQ(GetDisplayableAccountName(sync_service(), identity_manager()),
            std::string());
}

TEST_F(DisplayAccountInfoTest, NonDisplayableEmail) {
  SignIn();
  SetCanDisplaySignedInAccountEmail(false);

  EXPECT_EQ(GetDisplayableAccountName(sync_service(), identity_manager()),
            kTestFullName);
}
