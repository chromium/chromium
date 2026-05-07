// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/people/people_section.h"

#include <memory>
#include <sstream>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "chrome/browser/ash/login/users/profile_user_manager_controller.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "components/account_id/account_id.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/test/test_user_session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {

namespace mojom {

using ::chromeos::settings::mojom::Setting;

}  // namespace mojom

namespace {

constexpr char kPrimaryEmail[] = "primary@gmail.com";
constexpr char kPrimaryGaiaId[] = "primary_gaia_id";
constexpr char kSecondaryEmail[] = "secondary@gmail.com";
constexpr char kSecondaryGaiaId[] = "secondary_gaia_id";

AccountId PrimaryAccountId() {
  return AccountId::FromUserEmailGaiaId(kPrimaryEmail, GaiaId(kPrimaryGaiaId));
}

std::string GetRemoveAccountSearchResultId() {
  std::stringstream ss;
  ss << mojom::Setting::kRemoveAccount << ","
     << IDS_OS_SETTINGS_TAG_PEOPLE_ACCOUNTS_REMOVE;
  return ss.str();
}

}  // namespace

class PeopleSectionTest : public testing::Test {
 public:
  PeopleSectionTest()
      : local_search_service_proxy_(
            std::make_unique<local_search_service::LocalSearchServiceProxy>(
                /*for_testing=*/true)),
        search_tag_registry_(local_search_service_proxy_.get()) {}

  PeopleSectionTest(const PeopleSectionTest&) = delete;
  PeopleSectionTest& operator=(const PeopleSectionTest&) = delete;
  ~PeopleSectionTest() override = default;

 protected:
  void SetUp() override {
    UserDataAuthClient::InitializeFake();
    test_user_session_manager_ = std::make_unique<test::TestUserSessionManager>(
        TestingBrowserProcess::GetGlobal()->local_state());
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_user_manager_controller_ =
        std::make_unique<ProfileUserManagerController>(
            profile_manager_->profile_manager(),
            user_manager::UserManager::Get());

    const AccountId primary_account_id = PrimaryAccountId();
    user_ = test_user_session_manager_->AddRegularUser(primary_account_id);
    ASSERT_TRUE(user_);
    test_user_session_manager_->LogIn(primary_account_id);
    profile_ = profile_manager_->CreateTestingProfile(kPrimaryEmail);
    ASSERT_TRUE(profile_);

    account_manager_ = AccountManagerFactory::Get()->GetAccountManager(
        profile_->GetPath().value());
    ASSERT_TRUE(account_manager_);

    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return account_manager_->IsInitialized(); }));
    profile_->GetPrefs()->SetBoolean(
        ::account_manager::prefs::kSecondaryGoogleAccountSigninAllowed, true);

    AddAccount(kPrimaryGaiaId, kPrimaryEmail);
  }

  void TearDown() override {
    people_section_.reset();
    profile_ = nullptr;
    account_manager_ = nullptr;
    user_ = nullptr;

    profile_manager_.reset();
    profile_user_manager_controller_.reset();
    test_user_session_manager_.reset();

    UserDataAuthClient::Shutdown();
  }

  void AddAccount(const std::string& gaia_id, const std::string& email) {
    account_manager_->UpsertAccount(
        account_manager::AccountKey::FromGaiaId(GaiaId(gaia_id)), email,
        account_manager::AccountManager::kInvalidToken);
  }

  void RemoveAccount(const std::string& gaia_id) {
    account_manager_->RemoveAccount(
        account_manager::AccountKey::FromGaiaId(GaiaId(gaia_id)));
  }

  void CreatePeopleSection() {
    people_section_ = std::make_unique<PeopleSection>(
        profile_.get(), &search_tag_registry_,
        /*identity_manager=*/nullptr, profile_->GetPrefs());
  }

  bool HasRemoveAccountSearchTag() const {
    return search_tag_registry_.GetTagMetadata(
               GetRemoveAccountSearchResultId()) != nullptr;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<test::TestUserSessionManager> test_user_session_manager_;
  std::unique_ptr<ProfileUserManagerController>
      profile_user_manager_controller_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_;
  SearchTagRegistry search_tag_registry_;
  raw_ptr<user_manager::User> user_ = nullptr;
  raw_ptr<TestingProfile> profile_ = nullptr;
  raw_ptr<account_manager::AccountManager> account_manager_ = nullptr;
  std::unique_ptr<PeopleSection> people_section_;
};

TEST_F(PeopleSectionTest, InitialSecondaryAccountAddsRemoveAccountSearchTag) {
  // Existing secondary accounts should be reflected when PeopleSection performs
  // its initial direct AccountManager read during construction.
  AddAccount(kSecondaryGaiaId, kSecondaryEmail);

  CreatePeopleSection();

  EXPECT_TRUE(HasRemoveAccountSearchTag());
}

TEST_F(PeopleSectionTest,
       RemovingLastSecondaryAccountRemovesRemoveAccountSearchTag) {
  // Direct AccountManager removal notifications should make PeopleSection drop
  // the dynamic remove-account search tag when no secondary accounts remain.
  AddAccount(kSecondaryGaiaId, kSecondaryEmail);
  CreatePeopleSection();
  ASSERT_TRUE(HasRemoveAccountSearchTag());

  RemoveAccount(kSecondaryGaiaId);

  EXPECT_FALSE(HasRemoveAccountSearchTag());
}

TEST_F(PeopleSectionTest,
       AddingSecondaryAccountAfterConstructionAddsRemoveAccountSearchTag) {
  // Direct AccountManager token-upsert notifications should make PeopleSection
  // add the dynamic remove-account search tag after construction.
  CreatePeopleSection();
  ASSERT_FALSE(HasRemoveAccountSearchTag());

  AddAccount(kSecondaryGaiaId, kSecondaryEmail);

  EXPECT_TRUE(HasRemoveAccountSearchTag());
}

}  // namespace ash::settings
