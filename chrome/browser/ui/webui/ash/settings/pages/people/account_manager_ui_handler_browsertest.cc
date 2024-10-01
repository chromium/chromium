// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/people/account_manager_ui_handler.h"

#include <memory>
#include <optional>
#include <ostream>

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/account_manager/account_apps_availability.h"
#include "chrome/browser/ash/account_manager/account_apps_availability_factory.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::Contains;
using testing::Not;

using ::account_manager::AccountManager;

constexpr char kSecondaryAccount1Email[] = "secondary1@example.com";
constexpr char kSecondaryAccount2Email[] = "secondary2@example.com";
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

DeviceAccountInfo GetGaiaDeviceAccountInfo() {
  return {signin::GetTestGaiaIdForEmail("primary@example.com") /*id*/,
          "primary@example.com" /*email*/,
          "primary" /*fullName*/,
          "" /*organization*/,
          user_manager::UserType::kRegular /*user_type*/,
          account_manager::AccountType::kGaia /*account_type*/,
          "device-account-token" /*token*/};
}

DeviceAccountInfo GetChildDeviceAccountInfo() {
  return {supervised_user::kChildAccountSUID /*id*/,
          "child@example.com" /*email*/,
          "child" /*fullName*/,
          "Family Link" /*organization*/,
          user_manager::UserType::kChild /*user_type*/,
          account_manager::AccountType::kGaia /*account_type*/,
          "device-account-token" /*token*/};
}

std::optional<account_manager::Account> GetAccountByKey(
    std::vector<account_manager::Account> accounts,
    account_manager::AccountKey key) {
  for (const account_manager::Account& account : accounts) {
    if (account.key == key) {
      return account;
    }
  }
  return std::nullopt;
}

std::string ValueOrEmpty(const std::string* str) {
  return str ? *str : std::string();
}

MATCHER_P(AccountEmailEqual, other, "") {
  return arg.raw_email == other;
}

}  // namespace

namespace ash::settings {

class TestingAccountManagerUIHandler : public AccountManagerUIHandler {
 public:
  TestingAccountManagerUIHandler(
      AccountManager* account_manager,
      account_manager::AccountManagerFacade* account_manager_facade,
      signin::IdentityManager* identity_manager,
      AccountAppsAvailability* apps_availability,
      content::WebUI* web_ui)
      : AccountManagerUIHandler(account_manager,
                                account_manager_facade,
                                identity_manager,
                                apps_availability) {
    set_web_ui(web_ui);
  }

  TestingAccountManagerUIHandler(const TestingAccountManagerUIHandler&) =
      delete;
  TestingAccountManagerUIHandler& operator=(
      const TestingAccountManagerUIHandler&) = delete;
};

class AccountManagerUIHandlerTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<DeviceAccountInfo> {
 public:
  AccountManagerUIHandlerTest() {
    feature_list_.InitWithFeatures(
        {}, {ash::features::kSecondaryAccountAllowedInArcPolicy});
  }
  AccountManagerUIHandlerTest(const AccountManagerUIHandlerTest&) = delete;
  AccountManagerUIHandlerTest& operator=(const AccountManagerUIHandlerTest&) =
      delete;

  void SetUpOnMainThread() override {
    // Split the setup so it can be called from the inherited classes.
    SetUpEnvironment();

    auto* account_manager_facade =
        ::GetAccountManagerFacade(profile_->GetPath().value());
    account_apps_availability_ =
        AccountAppsAvailabilityFactory::GetForProfile(profile());

    handler_ = std::make_unique<TestingAccountManagerUIHandler>(
        account_manager_, account_manager_facade, identity_manager_,
        account_apps_availability_, &web_ui_);
    handler_->SetProfileForTesting(profile_.get());
    handler_->RegisterMessages();
    handler_->AllowJavascriptForTesting();
    base::RunLoop().RunUntilIdle();
  }

  void TearDownOnMainThread() override {
    account_apps_availability_ = nullptr;
    handler_.reset();
    GetFakeUserManager()->RemoveUserFromList(primary_account_id_);
    profile_.reset();
    base::RunLoop().RunUntilIdle();
    user_manager_enabler_.reset();
  }

  // Sets up profile and user manager. Should be called only once on test setup.
  void SetUpEnvironment() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestProfile"));
    profile_builder.SetProfileName(GetDeviceAccountInfo().email);
    if (GetDeviceAccountInfo().user_type == user_manager::UserType::kChild) {
      profile_builder.SetIsSupervisedProfile();
    }
    profile_ = profile_builder.Build();

    auto user_manager = std::make_unique<FakeChromeUserManager>();
    const user_manager::User* user;
    if (GetDeviceAccountInfo().user_type == user_manager::UserType::kChild) {
      user = user_manager->AddChildUser(AccountId::FromUserEmailGaiaId(
          GetDeviceAccountInfo().email, GetDeviceAccountInfo().id));
    } else {
      user = user_manager->AddUserWithAffiliationAndTypeAndProfile(
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

    auto* factory =
        g_browser_process->platform_part()->GetAccountManagerFactory();
    account_manager_ = factory->GetAccountManager(profile_->GetPath().value());

    account_manager_->UpsertAccount(
        ::account_manager::AccountKey{GetDeviceAccountInfo().id,
                                      GetDeviceAccountInfo().account_type},
        GetDeviceAccountInfo().email, GetDeviceAccountInfo().token);

    identity_manager_ = IdentityManagerFactory::GetForProfile(profile_.get());
    signin::WaitForRefreshTokensLoaded(identity_manager_);
  }

  FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  void UpsertAccount(std::string email) {
    account_manager_->UpsertAccount(
        ::account_manager::AccountKey{signin::GetTestGaiaIdForEmail(email),
                                      account_manager::AccountType::kGaia},
        email, AccountManager::kInvalidToken);
  }

  std::vector<::account_manager::Account> GetAccountsFromAccountManager()
      const {
    base::test::TestFuture<const std::vector<::account_manager::Account>&>
        future;
    account_manager_->GetAccounts(future.GetCallback());
    return future.Get();
  }

  bool HasDummyGaiaToken(const ::account_manager::AccountKey& account_key) {
    base::test::TestFuture<bool> future;
    account_manager_->HasDummyGaiaToken(account_key, future.GetCallback());
    return future.Get();
  }

  DeviceAccountInfo GetDeviceAccountInfo() const { return GetParam(); }

  TestingProfile* profile() { return profile_.get(); }
  content::TestWebUI* web_ui() { return &web_ui_; }
  signin::IdentityManager* identity_manager() { return identity_manager_; }
  AccountManager* account_manager() { return account_manager_; }

 private:
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<AccountManager, DanglingUntriaged> account_manager_ = nullptr;
  raw_ptr<signin::IdentityManager, DanglingUntriaged> identity_manager_ =
      nullptr;
  content::TestWebUI web_ui_;
  AccountId primary_account_id_;
  std::unique_ptr<TestingAccountManagerUIHandler> handler_;
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<AccountAppsAvailability> account_apps_availability_;
};

IN_PROC_BROWSER_TEST_P(AccountManagerUIHandlerTest,
                       OnGetAccountsNoSecondaryAccounts) {
  const std::vector<::account_manager::Account> account_manager_accounts =
      GetAccountsFromAccountManager();
  // Only Primary account.
  ASSERT_EQ(1UL, account_manager_accounts.size());

  // Call "getAccounts".
  base::Value::List args;
  args.Append(kHandleFunctionName);
  web_ui()->HandleReceivedMessage(kGetAccountsMessage, args);

  const content::TestWebUI::CallData& call_data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ(kHandleFunctionName, call_data.arg1()->GetString());
  ASSERT_TRUE(call_data.arg2()->GetBool());

  // Get results from JS callback.
  const base::Value::List& result = call_data.arg3()->GetList();
  ASSERT_EQ(account_manager_accounts.size(), result.size());

  // Check first (device) account.
  const base::Value::Dict& device_account = result[0].GetDict();
  EXPECT_TRUE(device_account.FindBool("isDeviceAccount").value());
  EXPECT_TRUE(device_account.FindBool("isSignedIn").value());
  EXPECT_FALSE(device_account.FindBool("unmigrated").value());
  EXPECT_EQ(static_cast<int>(GetDeviceAccountInfo().account_type),
            device_account.FindInt("accountType"));
  EXPECT_EQ(GetDeviceAccountInfo().email,
            ValueOrEmpty(device_account.FindString("email")));
  EXPECT_EQ(GetDeviceAccountInfo().id,
            ValueOrEmpty(device_account.FindString("id")));
  if (GetDeviceAccountInfo().user_type == user_manager::UserType::kChild) {
    std::string organization = GetDeviceAccountInfo().organization;
    base::ReplaceSubstringsAfterOffset(&organization, 0, " ", "&nbsp;");
    EXPECT_EQ(organization,
              ValueOrEmpty(device_account.FindString("organization")));
  } else {
    EXPECT_EQ(GetDeviceAccountInfo().organization,
              ValueOrEmpty(device_account.FindString("organization")));
  }
}

IN_PROC_BROWSER_TEST_P(AccountManagerUIHandlerTest,
                       OnGetAccountsWithSecondaryAccounts) {
  UpsertAccount(kSecondaryAccount1Email);
  UpsertAccount(kSecondaryAccount2Email);
  const std::vector<::account_manager::Account> account_manager_accounts =
      GetAccountsFromAccountManager();
  ASSERT_EQ(3UL, account_manager_accounts.size());

  // Wait for accounts to propagate to IdentityManager.
  base::RunLoop().RunUntilIdle();

  // Call "getAccounts".
  base::Value::List args;
  args.Append(kHandleFunctionName);
  web_ui()->HandleReceivedMessage(kGetAccountsMessage, args);

  const content::TestWebUI::CallData& call_data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ(kHandleFunctionName, call_data.arg1()->GetString());
  ASSERT_TRUE(call_data.arg2()->GetBool());

  // Get results from JS callback.
  const base::Value::List& result = call_data.arg3()->GetList();
  ASSERT_EQ(account_manager_accounts.size(), result.size());

  // Check first (device) account.
  const base::Value::Dict& device_account = result[0].GetDict();
  EXPECT_TRUE(device_account.FindBool("isDeviceAccount").value());
  EXPECT_TRUE(device_account.FindBool("isSignedIn").value());
  EXPECT_FALSE(device_account.FindBool("unmigrated").value());
  EXPECT_EQ(static_cast<int>(GetDeviceAccountInfo().account_type),
            device_account.FindInt("accountType"));
  EXPECT_EQ(GetDeviceAccountInfo().email,
            ValueOrEmpty(device_account.FindString("email")));
  EXPECT_EQ(GetDeviceAccountInfo().id,
            ValueOrEmpty(device_account.FindString("id")));
  if (GetDeviceAccountInfo().user_type == user_manager::UserType::kChild) {
    std::string organization = GetDeviceAccountInfo().organization;
    base::ReplaceSubstringsAfterOffset(&organization, 0, " ", "&nbsp;");
    EXPECT_EQ(organization,
              ValueOrEmpty(device_account.FindString("organization")));
  } else {
    EXPECT_EQ(GetDeviceAccountInfo().organization,
              ValueOrEmpty(device_account.FindString("organization")));
  }

  // Check secondary accounts.
  for (const base::Value& account_value : result) {
    const base::Value::Dict& account = account_value.GetDict();
    if (ValueOrEmpty(account.FindString("id")) == GetDeviceAccountInfo().id) {
      continue;
    }
    EXPECT_FALSE(account.FindBool("isDeviceAccount").value());

    ::account_manager::Account expected_account =
        GetAccountByKey(account_manager_accounts,
                        {ValueOrEmpty(account.FindString("id")),
                         account_manager::AccountType::kGaia})
            .value();
    if (GetDeviceAccountInfo().user_type == user_manager::UserType::kChild) {
      EXPECT_FALSE(account.FindBool("unmigrated").value());
    } else {
      EXPECT_EQ(HasDummyGaiaToken(expected_account.key),
                account.FindBool("unmigrated").value());
    }
    EXPECT_EQ(static_cast<int>(expected_account.key.account_type()),
              account.FindInt("accountType"));
    EXPECT_EQ(expected_account.raw_email,
              ValueOrEmpty(account.FindString("email")));

    AccountInfo expected_account_info =
        identity_manager()->FindExtendedAccountInfoByGaiaId(
            expected_account.key.id());
    EXPECT_FALSE(expected_account_info.IsEmpty());
    EXPECT_EQ(expected_account_info.full_name,
              ValueOrEmpty(account.FindString("fullName")));
    EXPECT_EQ(
        !identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
            expected_account_info.account_id),
        account.FindBool("isSignedIn").value());
  }
}

INSTANTIATE_TEST_SUITE_P(AccountManagerUIHandlerTestSuite,
                         AccountManagerUIHandlerTest,
                         ::testing::Values(GetGaiaDeviceAccountInfo(),
                                           GetChildDeviceAccountInfo()));

class AccountManagerUIHandlerTestWithManagedArcAccountRestriction
    : public AccountManagerUIHandlerTest {
 public:
  AccountManagerUIHandlerTestWithManagedArcAccountRestriction() {
    std::vector<base::test::FeatureRef> enabled_features;
    enabled_features.push_back(
        ash::features::kSecondaryAccountAllowedInArcPolicy);
    feature_list_.InitWithFeatures(enabled_features, {});
  }

  void SetUpOnMainThread() override {
    SetUpEnvironment();

    auto* account_manager_facade =
        ::GetAccountManagerFacade(profile()->GetPath().value());

    account_apps_availability_ =
        AccountAppsAvailabilityFactory::GetForProfile(profile());

    handler_ = std::make_unique<TestingAccountManagerUIHandler>(
        account_manager(), account_manager_facade, identity_manager(),
        account_apps_availability_, web_ui());
    handler_->SetProfileForTesting(profile());
    handler_->RegisterMessages();
    handler_->AllowJavascriptForTesting();
    base::RunLoop().RunUntilIdle();

    ASSERT_TRUE(
        ash::AccountAppsAvailability::IsArcManagedAccountRestrictionEnabled());
  }

  void TearDownOnMainThread() override {
    account_apps_availability_ = nullptr;
    handler_.reset();
    AccountManagerUIHandlerTest::TearDownOnMainThread();
  }

  base::flat_set<::account_manager::Account> GetAccountsAvailableInArc() const {
    base::test::TestFuture<const base::flat_set<::account_manager::Account>&>
        future;
    account_apps_availability_->GetAccountsAvailableInArc(future.GetCallback());
    return future.Get();
  }

  std::optional<::account_manager::Account> FindAccountByEmail(
      const std::vector<::account_manager::Account>& accounts,
      const std::string& email) {
    for (const auto& account : accounts) {
      if (account.raw_email == email) {
        return account;
      }
    }
    return std::nullopt;
  }

  std::optional<const base::Value> FindAccountDictByEmail(
      const base::Value::List& accounts,
      const std::string& email) {
    for (const base::Value& account : accounts) {
      if (ValueOrEmpty(account.GetDict().FindString("email")) == email) {
        return account.Clone();
      }
    }
    return std::nullopt;
  }

  AccountAppsAvailability* account_apps_availability() {
    return account_apps_availability_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<AccountAppsAvailability> account_apps_availability_;
  std::unique_ptr<TestingAccountManagerUIHandler> handler_;
};

IN_PROC_BROWSER_TEST_P(
    AccountManagerUIHandlerTestWithManagedArcAccountRestriction,
    CheckIsAvailableInArcValue) {
  UpsertAccount(kSecondaryAccount1Email);
  UpsertAccount(kSecondaryAccount2Email);
  const std::vector<::account_manager::Account> account_manager_accounts =
      GetAccountsFromAccountManager();
  ASSERT_EQ(3UL, account_manager_accounts.size());

  // Wait for accounts to propagate to IdentityManager.
  base::RunLoop().RunUntilIdle();

  std::optional<::account_manager::Account> account_1 =
      FindAccountByEmail(account_manager_accounts, kSecondaryAccount1Email);
  ASSERT_TRUE(account_1.has_value());
  std::optional<::account_manager::Account> account_2 =
      FindAccountByEmail(account_manager_accounts, kSecondaryAccount2Email);
  ASSERT_TRUE(account_2.has_value());

  account_apps_availability()->SetIsAccountAvailableInArc(account_1.value(),
                                                          true);
  account_apps_availability()->SetIsAccountAvailableInArc(account_2.value(),
                                                          false);

  // Call "getAccounts".
  base::Value::List args;
  args.Append(kHandleFunctionName);
  web_ui()->HandleReceivedMessage(kGetAccountsMessage, args);

  // Wait for the async calls to finish.
  base::RunLoop().RunUntilIdle();

  const content::TestWebUI::CallData& call_data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ(kHandleFunctionName, call_data.arg1()->GetString());
  ASSERT_TRUE(call_data.arg2()->GetBool());

  // Get results from JS callback.
  const base::Value::List& result = call_data.arg3()->GetList();
  ASSERT_EQ(account_manager_accounts.size(), result.size());

  // The value for the device account should be always `true`.
  const base::Value::Dict& device_account = result[0].GetDict();
  EXPECT_TRUE(device_account.FindBool("isAvailableInArc").value());

  // Check secondary accounts.
  std::optional<const base::Value> secondary_1_dict =
      FindAccountDictByEmail(result, kSecondaryAccount1Email);
  ASSERT_TRUE(secondary_1_dict.has_value());
  std::optional<const base::Value> secondary_2_dict =
      FindAccountDictByEmail(result, kSecondaryAccount2Email);
  ASSERT_TRUE(secondary_2_dict.has_value());

  std::optional<bool> is_available_1 =
      secondary_1_dict.value().GetDict().FindBool("isAvailableInArc");
  ASSERT_TRUE(is_available_1.has_value());
  std::optional<bool> is_available_2 =
      secondary_2_dict.value().GetDict().FindBool("isAvailableInArc");
  ASSERT_TRUE(is_available_2.has_value());

  // The values should match `SetIsAccountAvailableInArc` calls.
  EXPECT_TRUE(is_available_1.value());
  EXPECT_FALSE(is_available_2.value());
}

INSTANTIATE_TEST_SUITE_P(
    AccountManagerUIHandlerTestWithManagedArcAccountRestrictionSuite,
    AccountManagerUIHandlerTestWithManagedArcAccountRestriction,
    ::testing::Values(GetGaiaDeviceAccountInfo(), GetChildDeviceAccountInfo()));

}  // namespace ash::settings
