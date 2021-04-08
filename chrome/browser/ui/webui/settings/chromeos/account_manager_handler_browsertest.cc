// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/account_manager_handler.h"

#include <memory>
#include <ostream>

#include "ash/components/account_manager/account_manager.h"
#include "ash/components/account_manager/account_manager_factory.h"
#include "base/test/bind.h"
#include "chrome/browser/account_manager_facade_factory.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::ash::AccountManager;

constexpr char kGetAccountsMessage[] = "getAccounts";
constexpr char kHandleFunctionName[] = "handleFunctionName";

struct DeviceAccountInfo {
  std::string id;
  std::string email;
  std::string fullName;
  std::string organization;

  user_manager::UserType user_type;
  account_manager::AccountType account_type;
  std::string token;

  friend std::ostream& operator<<(std::ostream& stream,
                                  const DeviceAccountInfo& device_account_info);
};

std::ostream& operator<<(std::ostream& stream,
                         const DeviceAccountInfo& device_account_info) {
  return stream << "{email: " << device_account_info.email
                << ", user_type: " << device_account_info.user_type << "}";
}

DeviceAccountInfo GetActiveDirectoryDeviceAccountInfo() {
  return {"fake-ad-id" /*id*/,
          "primary@example.com" /*email*/,
          "primary" /*fullName*/,
          "example.com" /*organization*/,
          user_manager::USER_TYPE_ACTIVE_DIRECTORY /*user_type*/,
          account_manager::AccountType::kActiveDirectory /*account_type*/,
          AccountManager::kActiveDirectoryDummyToken /*token*/};
}

DeviceAccountInfo GetGaiaDeviceAccountInfo() {
  return {signin::GetTestGaiaIdForEmail("primary@example.com") /*id*/,
          "primary@example.com" /*email*/,
          "primary" /*fullName*/,
          "" /*organization*/,
          user_manager::USER_TYPE_REGULAR /*user_type*/,
          account_manager::AccountType::kGaia /*account_type*/,
          "device-account-token" /*token*/};
}

DeviceAccountInfo GetChildDeviceAccountInfo() {
  return {supervised_users::kChildAccountSUID /*id*/,
          "child@example.com" /*email*/,
          "child" /*fullName*/,
          "Family Link" /*organization*/,
          user_manager::USER_TYPE_CHILD /*user_type*/,
          account_manager::AccountType::kGaia /*account_type*/,
          "device-account-token" /*token*/};
}

account_manager::Account GetAccountByKey(
    std::vector<account_manager::Account> accounts,
    account_manager::AccountKey key) {
  for (const account_manager::Account& account : accounts) {
    if (account.key == key) {
      return account;
    }
  }
  return account_manager::Account();
}

std::string ValueOrEmpty(const std::string* str) {
  return str ? *str : std::string();
}

}  // namespace

namespace chromeos {
namespace settings {

class TestingAccountManagerUIHandler : public AccountManagerUIHandler {
 public:
  TestingAccountManagerUIHandler(
      AccountManager* account_manager,
      account_manager::AccountManagerFacade* account_manager_facade,
      signin::IdentityManager* identity_manager,
      content::WebUI* web_ui)
      : AccountManagerUIHandler(account_manager,
                                account_manager_facade,
                                identity_manager) {
    set_web_ui(web_ui);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingAccountManagerUIHandler);
};

class AccountManagerUIHandlerTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<DeviceAccountInfo> {
 public:
  AccountManagerUIHandlerTest() = default;
  AccountManagerUIHandlerTest(const AccountManagerUIHandlerTest&) = delete;
  AccountManagerUIHandlerTest& operator=(const AccountManagerUIHandlerTest&) =
      delete;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestProfile"));
    profile_builder.SetProfileName(GetDeviceAccountInfo().email);
    if (GetDeviceAccountInfo().user_type ==
        user_manager::UserType::USER_TYPE_CHILD) {
      profile_builder.SetSupervisedUserId(GetDeviceAccountInfo().id);
    }
    profile_ = profile_builder.Build();

    auto user_manager = std::make_unique<FakeChromeUserManager>();
    const user_manager::User* user;
    if (GetDeviceAccountInfo().user_type ==
        user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY) {
      user = GetFakeUserManager()->AddUserWithAffiliationAndTypeAndProfile(
          AccountId::AdFromUserEmailObjGuid(GetDeviceAccountInfo().email,
                                            GetDeviceAccountInfo().id),
          true, user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY,
          profile_.get());
    } else if (GetDeviceAccountInfo().user_type ==
               user_manager::UserType::USER_TYPE_CHILD) {
      user = GetFakeUserManager()->AddChildUser(AccountId::FromUserEmailGaiaId(
          GetDeviceAccountInfo().email, GetDeviceAccountInfo().id));
    } else {
      user = GetFakeUserManager()->AddUserWithAffiliationAndTypeAndProfile(
          AccountId::FromUserEmailGaiaId(GetDeviceAccountInfo().email,
                                         GetDeviceAccountInfo().id),
          true, GetDeviceAccountInfo().user_type, profile_.get());
    }
    primary_account_id_ = user->GetAccountId();
    user_manager->LoginUser(primary_account_id_);
    ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                            profile_.get());
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    identity_manager_ = IdentityManagerFactory::GetForProfile(profile_.get());

    auto* factory =
        g_browser_process->platform_part()->GetAccountManagerFactory();
    account_manager_ = factory->GetAccountManager(profile_->GetPath().value());

    account_manager_->UpsertAccount(
        ::account_manager::AccountKey{GetDeviceAccountInfo().id,
                                      GetDeviceAccountInfo().account_type},
        GetDeviceAccountInfo().email, GetDeviceAccountInfo().token);

    auto* account_manager_facade =
        ::GetAccountManagerFacade(profile_->GetPath().value());

    handler_ = std::make_unique<TestingAccountManagerUIHandler>(
        account_manager_, account_manager_facade, identity_manager_, &web_ui_);
    handler_->SetProfileForTesting(profile_.get());
    handler_->RegisterMessages();
    handler_->AllowJavascriptForTesting();
    base::RunLoop().RunUntilIdle();
  }

  void TearDownOnMainThread() override {
    handler_.reset();
    ProfileHelper::Get()->RemoveUserFromListForTesting(primary_account_id_);
    profile_.reset();
    base::RunLoop().RunUntilIdle();
    user_manager_enabler_.reset();
  }

  void UpsertAccount(std::string email) {
    account_manager_->UpsertAccount(
        ::account_manager::AccountKey{signin::GetTestGaiaIdForEmail(email),
                                      account_manager::AccountType::kGaia},
        email, AccountManager::kInvalidToken);
  }

  std::vector<::account_manager::Account> GetAccountsFromAccountManager()
      const {
    std::vector<::account_manager::Account> accounts;

    base::RunLoop run_loop;
    account_manager_->GetAccounts(base::BindLambdaForTesting(
        [&accounts, &run_loop](
            const std::vector<::account_manager::Account>& stored_accounts) {
          accounts = stored_accounts;
          run_loop.Quit();
        }));
    run_loop.Run();

    return accounts;
  }

  bool HasDummyGaiaToken(const ::account_manager::AccountKey& account_key) {
    bool has_dummy_token_result;

    base::RunLoop run_loop;
    account_manager_->HasDummyGaiaToken(
        account_key,
        base::BindLambdaForTesting(
            [&has_dummy_token_result, &run_loop](bool has_dummy_token) {
              has_dummy_token_result = has_dummy_token;
              run_loop.Quit();
            }));
    run_loop.Run();

    return has_dummy_token_result;
  }

  DeviceAccountInfo GetDeviceAccountInfo() const { return GetParam(); }

  content::TestWebUI* web_ui() { return &web_ui_; }
  signin::IdentityManager* identity_manager() { return identity_manager_; }
  AccountManager* account_manager() { return account_manager_; }

 private:
  FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;
  AccountManager* account_manager_ = nullptr;
  signin::IdentityManager* identity_manager_ = nullptr;
  content::TestWebUI web_ui_;
  AccountId primary_account_id_;
  std::unique_ptr<TestingAccountManagerUIHandler> handler_;
};

IN_PROC_BROWSER_TEST_P(AccountManagerUIHandlerTest,
                       OnGetAccountsNoSecondaryAccounts) {
  const std::vector<::account_manager::Account> account_manager_accounts =
      GetAccountsFromAccountManager();
  // Only Primary account.
  ASSERT_EQ(1UL, account_manager_accounts.size());

  // Call "getAccounts".
  base::ListValue args;
  args.AppendString(kHandleFunctionName);
  web_ui()->HandleReceivedMessage(kGetAccountsMessage, &args);

  const content::TestWebUI::CallData& call_data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ(kHandleFunctionName, call_data.arg1()->GetString());
  ASSERT_TRUE(call_data.arg2()->GetBool());

  // Get results from JS callback.
  const base::span<const base::Value> result = call_data.arg3()->GetList();
  ASSERT_EQ(account_manager_accounts.size(), result.size());

  // Check first (device) account.
  const base::Value& device_account = result[0];
  EXPECT_TRUE(device_account.FindBoolKey("isDeviceAccount").value());
  EXPECT_TRUE(device_account.FindBoolKey("isSignedIn").value());
  EXPECT_FALSE(device_account.FindBoolKey("unmigrated").value());
  EXPECT_EQ(static_cast<int>(GetDeviceAccountInfo().account_type),
            device_account.FindIntKey("accountType"));
  EXPECT_EQ(GetDeviceAccountInfo().email,
            ValueOrEmpty(device_account.FindStringKey("email")));
  EXPECT_EQ(GetDeviceAccountInfo().id,
            ValueOrEmpty(device_account.FindStringKey("id")));
  if (GetDeviceAccountInfo().user_type ==
      user_manager::UserType::USER_TYPE_CHILD) {
    std::string organization = GetDeviceAccountInfo().organization;
    base::ReplaceSubstringsAfterOffset(&organization, 0, " ", "&nbsp;");
    EXPECT_EQ(organization,
              ValueOrEmpty(device_account.FindStringKey("organization")));
  } else {
    EXPECT_EQ(GetDeviceAccountInfo().organization,
              ValueOrEmpty(device_account.FindStringKey("organization")));
  }
}

IN_PROC_BROWSER_TEST_P(AccountManagerUIHandlerTest,
                       OnGetAccountsWithSecondaryAccounts) {
  UpsertAccount("secondary1@example.com");
  UpsertAccount("secondary2@example.com");
  const std::vector<::account_manager::Account> account_manager_accounts =
      GetAccountsFromAccountManager();
  ASSERT_EQ(3UL, account_manager_accounts.size());

  // Wait for accounts to propagate to IdentityManager.
  base::RunLoop().RunUntilIdle();

  // Call "getAccounts".
  base::ListValue args;
  args.AppendString(kHandleFunctionName);
  web_ui()->HandleReceivedMessage(kGetAccountsMessage, &args);

  const content::TestWebUI::CallData& call_data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ(kHandleFunctionName, call_data.arg1()->GetString());
  ASSERT_TRUE(call_data.arg2()->GetBool());

  // Get results from JS callback.
  const base::span<const base::Value> result = call_data.arg3()->GetList();
  ASSERT_EQ(account_manager_accounts.size(), result.size());

  // Check first (device) account.
  const base::Value& device_account = result[0];
  EXPECT_TRUE(device_account.FindBoolKey("isDeviceAccount").value());
  EXPECT_TRUE(device_account.FindBoolKey("isSignedIn").value());
  EXPECT_FALSE(device_account.FindBoolKey("unmigrated").value());
  EXPECT_EQ(static_cast<int>(GetDeviceAccountInfo().account_type),
            device_account.FindIntKey("accountType"));
  EXPECT_EQ(GetDeviceAccountInfo().email,
            ValueOrEmpty(device_account.FindStringKey("email")));
  EXPECT_EQ(GetDeviceAccountInfo().id,
            ValueOrEmpty(device_account.FindStringKey("id")));
  if (GetDeviceAccountInfo().user_type ==
      user_manager::UserType::USER_TYPE_CHILD) {
    std::string organization = GetDeviceAccountInfo().organization;
    base::ReplaceSubstringsAfterOffset(&organization, 0, " ", "&nbsp;");
    EXPECT_EQ(organization,
              ValueOrEmpty(device_account.FindStringKey("organization")));
  } else {
    EXPECT_EQ(GetDeviceAccountInfo().organization,
              ValueOrEmpty(device_account.FindStringKey("organization")));
  }

  // Check secondary accounts.
  for (const base::Value& account : result) {
    if (ValueOrEmpty(account.FindStringKey("id")) == GetDeviceAccountInfo().id)
      continue;
    EXPECT_FALSE(account.FindBoolKey("isDeviceAccount").value());

    ::account_manager::Account expected_account = GetAccountByKey(
        account_manager_accounts, {ValueOrEmpty(account.FindStringKey("id")),
                                   account_manager::AccountType::kGaia});
    if (GetDeviceAccountInfo().user_type ==
        user_manager::UserType::USER_TYPE_CHILD) {
      EXPECT_FALSE(account.FindBoolKey("unmigrated").value());
    } else {
      EXPECT_EQ(HasDummyGaiaToken(expected_account.key),
                account.FindBoolKey("unmigrated").value());
    }
    EXPECT_EQ(static_cast<int>(expected_account.key.account_type),
              account.FindIntKey("accountType"));
    EXPECT_EQ(expected_account.raw_email,
              ValueOrEmpty(account.FindStringKey("email")));

    base::Optional<AccountInfo> expected_account_info =
        identity_manager()
            ->FindExtendedAccountInfoForAccountWithRefreshTokenByGaiaId(
                expected_account.key.id);
    EXPECT_TRUE(expected_account_info.has_value());
    EXPECT_EQ(expected_account_info->full_name,
              ValueOrEmpty(account.FindStringKey("fullName")));
    EXPECT_EQ(
        !identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
            expected_account_info->account_id),
        account.FindBoolKey("isSignedIn").value());
  }
}

INSTANTIATE_TEST_SUITE_P(
    AccountManagerUIHandlerTestSuite,
    AccountManagerUIHandlerTest,
    ::testing::Values(GetActiveDirectoryDeviceAccountInfo(),
                      GetGaiaDeviceAccountInfo(),
                      GetChildDeviceAccountInfo()));

}  // namespace settings
}  // namespace chromeos
