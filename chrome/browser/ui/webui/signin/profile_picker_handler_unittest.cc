// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/profile_picker_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/test/base/profile_waiter.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/account_manager/account_profile_mapper.h"
#include "chrome/browser/lacros/account_manager/get_account_information_helper.h"
#include "chrome/browser/lacros/identity_manager_lacros.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_upsertion_result.h"
#include "components/account_manager_core/mock_account_manager_facade.h"
#include "ui/gfx/image/image_unittest_util.h"

const char kTestCallbackId[] = "test-callback-id";
#endif

namespace {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class MockIdentityManagerLacros : public IdentityManagerLacros {
 public:
  MockIdentityManagerLacros() = default;
  ~MockIdentityManagerLacros() override = default;

  MOCK_METHOD(
      void,
      GetAccountEmail,
      (const std::string& gaia_id,
       crosapi::mojom::IdentityManager::GetAccountEmailCallback callback));

  MOCK_METHOD(
      void,
      HasAccountWithPersistentError,
      (const std::string& gaia_id,
       crosapi::mojom::IdentityManager::HasAccountWithPersistentErrorCallback
           callback));
};
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

void VerifyProfileEntry(const base::Value::Dict& dict,
                        ProfileAttributesEntry* entry) {
  EXPECT_EQ(*dict.Find("profilePath"), base::FilePathToValue(entry->GetPath()));
  EXPECT_EQ(*dict.FindString("localProfileName"),
            base::UTF16ToUTF8(entry->GetLocalProfileName()));
  EXPECT_EQ(*dict.FindBool("isSyncing"),
            entry->GetSigninState() ==
                SigninState::kSignedInWithConsentedPrimaryAccount);
  EXPECT_EQ(*dict.FindBool("needsSignin"), entry->IsSigninRequired());
  EXPECT_EQ(*dict.FindString("gaiaName"),
            base::UTF16ToUTF8(entry->GetGAIANameToDisplay()));
  EXPECT_EQ(*dict.FindString("userName"),
            base::UTF16ToUTF8(entry->GetUserName()));
  EXPECT_EQ(dict.FindString("avatarBadge")->empty(),
            !AccountInfo::IsManaged(entry->GetHostedDomain()));
}

}  // namespace

class ProfilePickerHandlerTest : public testing::Test {
 public:
  ProfilePickerHandlerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    scoped_feature_list_.InitAndEnableFeature(
        supervised_user::kHideGuestModeForSupervisedUsers);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  }

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    handler_ = std::make_unique<ProfilePickerHandler>();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Configure a mock account manager facade.
    ON_CALL(mock_account_manager_facade_, GetAccounts(testing::_))
        .WillByDefault(
            [this](
                base::OnceCallback<void(
                    const std::vector<account_manager::Account>&)> callback) {
              DCHECK(!facade_get_accounts_callback_);
              facade_get_accounts_callback_ = std::move(callback);
            });
    ON_CALL(mock_account_manager_facade_, GetPersistentErrorForAccount)
        .WillByDefault(
            [](const account_manager::AccountKey&,
               base::OnceCallback<void(const GoogleServiceAuthError&)>
                   callback) {
              std::move(callback).Run(GoogleServiceAuthError::AuthErrorNone());
            });
    profile_manager()->SetAccountProfileMapper(
        std::make_unique<AccountProfileMapper>(
            &mock_account_manager_facade_,
            profile_manager()->profile_attributes_storage(),
            profile_manager()->local_state()->Get()));

    handler_->identity_manager_lacros_ =
        std::make_unique<MockIdentityManagerLacros>();

#endif
    web_ui_profile_ = GetWebUIProfile();
    web_ui_.set_web_contents(
        web_contents_factory_.CreateWebContents(web_ui_profile_));
    handler_->set_web_ui(&web_ui_);
    handler_->RegisterMessages();
  }

  virtual Profile* GetWebUIProfile() {
    return profile_manager()->CreateSystemProfile();
  }

  void VerifyProfileListWasPushed(
      const std::vector<ProfileAttributesEntry*>& ordered_profile_entries) {
    ASSERT_TRUE(!web_ui()->call_data().empty());
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    ASSERT_EQ("cr.webUIListenerCallback", data.function_name());
    ASSERT_EQ("profiles-list-changed", data.arg1()->GetString());
    size_t size = data.arg2()->GetList().size();
    ASSERT_EQ(size, ordered_profile_entries.size());
    for (size_t i = 0; i < size; ++i) {
      ASSERT_TRUE(data.arg2()->GetList()[i].is_dict());
      VerifyProfileEntry(data.arg2()->GetList()[i].GetDict(),
                         ordered_profile_entries[i]);
    }
  }

  void VerifyIfGuestModeUpdateWasCalled(bool expected_guest_mode) {
    std::optional<bool> expected_guest_mode_update_value;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    // Feature needs to be enabled in order to invoke update guest mode.
    if (base::FeatureList::IsEnabled(
            supervised_user::kHideGuestModeForSupervisedUsers)) {
      expected_guest_mode_update_value = expected_guest_mode;
    }
#endif  //  BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

    auto it = base::ranges::find_if(web_ui()->call_data(), [](auto& data_ptr) {
      return data_ptr->function_name() == "cr.webUIListenerCallback" &&
             data_ptr->arg1()->GetString() == "guest-mode-availability-updated";
    });

    std::optional<bool> guest_mode_update_value;
    if (it != web_ui()->call_data().end()) {
      CHECK(it->get()->arg2()->is_bool());
      guest_mode_update_value = it->get()->arg2()->GetBool();
    }

    EXPECT_EQ(guest_mode_update_value, expected_guest_mode_update_value);
  }

  void VerifyProfileWasRemoved(const base::FilePath& profile_path) {
    ASSERT_TRUE(!web_ui()->call_data().empty());
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    ASSERT_EQ("cr.webUIListenerCallback", data.function_name());
    ASSERT_EQ("profile-removed", data.arg1()->GetString());
    ASSERT_EQ(*data.arg2(), base::FilePathToValue(profile_path));
  }

  void InitializeMainViewAndVerifyProfileList(
      const std::vector<ProfileAttributesEntry*>& ordered_profile_entries) {
    base::Value::List empty_args;
    web_ui()->HandleReceivedMessage("mainViewInitialize", empty_args);
    VerifyProfileListWasPushed(ordered_profile_entries);
  }

  // Creates a new testing profile, sets its supervision status
  // and returns its `ProfileAttributesEntry`.
  ProfileAttributesEntry* CreateTestingProfile(
      const std::string& profile_name,
      const bool is_supervised = false) {
    auto* profile = profile_manager()->CreateTestingProfile(
        profile_name, std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
        base::UTF8ToUTF16(profile_name), /*avatar_id=*/0,
        /*testing_factories=*/{},
        /*is_supervised_profile=*/is_supervised);
    ProfileAttributesEntry* entry =
        profile_manager()
            ->profile_attributes_storage()
            ->GetProfileAttributesWithPath(profile->GetPath());
    CHECK(entry);
    return entry;
  }

  TestingProfileManager* profile_manager() { return &profile_manager_; }
  content::TestWebUI* web_ui() { return &web_ui_; }
  ProfilePickerHandler* handler() { return handler_.get(); }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  account_manager::MockAccountManagerFacade* mock_account_manager_facade() {
    return &mock_account_manager_facade_;
  }

  void CompleteFacadeGetAccounts(
      const std::vector<account_manager::Account>& accounts) {
    std::move(facade_get_accounts_callback_).Run(accounts);
  }

  MockIdentityManagerLacros* mock_identity_manager_lacros() {
    return static_cast<MockIdentityManagerLacros*>(
        handler_->identity_manager_lacros_.get());
  }

  // Set the account check to return in error and the proper email based on the
  // given gaia id through callbacks.
  void SetAccountInErrorRequestResponse(const std::string& gaia_id,
                                        const std::string& email,
                                        bool account_in_error_response) {
    ON_CALL(*mock_identity_manager_lacros(),
            GetAccountEmail(testing::_, testing::_))
        .WillByDefault(testing::Invoke(
            [email](const std::string& gaia_id,
                    crosapi::mojom::IdentityManager::GetAccountEmailCallback
                        callback) { std::move(callback).Run(email); }));

    ON_CALL(*mock_identity_manager_lacros(),
            HasAccountWithPersistentError(gaia_id, testing::_))
        .WillByDefault(testing::Invoke(
            [account_in_error_response](
                const std::string& gaia_id,
                crosapi::mojom::IdentityManager::
                    HasAccountWithPersistentErrorCallback callback) {
              std::move(callback).Run(account_in_error_response);
            }));
  }
#endif

 private:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  testing::NiceMock<account_manager::MockAccountManagerFacade>
      mock_account_manager_facade_;

  // Callback to configure the accounts in the facade.
  base::OnceCallback<void(const std::vector<account_manager::Account>&)>
      facade_get_accounts_callback_;
#endif

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<Profile> web_ui_profile_ = nullptr;
  content::TestWebUI web_ui_;
  std::unique_ptr<ProfilePickerHandler> handler_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ProfilePickerHandlerTest, OrderedAlphabeticallyOnInit) {
  ProfileAttributesEntry* profile_a = CreateTestingProfile("A");
  ProfileAttributesEntry* profile_d = CreateTestingProfile("D");
  ProfileAttributesEntry* profile_c = CreateTestingProfile("C");
  ProfileAttributesEntry* profile_b = CreateTestingProfile("B");

  InitializeMainViewAndVerifyProfileList(
      {profile_a, profile_b, profile_c, profile_d});
  web_ui()->ClearTrackedCalls();
}

TEST_F(ProfilePickerHandlerTest, AddProfile) {
  ProfileAttributesEntry* profile_a = CreateTestingProfile("A");
  ProfileAttributesEntry* profile_c = CreateTestingProfile("C");
  ProfileAttributesEntry* profile_d = CreateTestingProfile("D");

  InitializeMainViewAndVerifyProfileList({profile_a, profile_c, profile_d});
  web_ui()->ClearTrackedCalls();

  // A new profile should be added to the end of the list.
  ProfileAttributesEntry* profile_b = CreateTestingProfile("B");
  VerifyProfileListWasPushed({profile_a, profile_c, profile_d, profile_b});
  VerifyIfGuestModeUpdateWasCalled(/*expected_guest_mode=*/true);
  web_ui()->ClearTrackedCalls();
}

TEST_F(ProfilePickerHandlerTest, AddProfileToEmptyList) {
  InitializeMainViewAndVerifyProfileList({});
  web_ui()->ClearTrackedCalls();

  ProfileAttributesEntry* profile = CreateTestingProfile("Profile");
  VerifyProfileListWasPushed({profile});
  VerifyIfGuestModeUpdateWasCalled(/*expected_guest_mode=*/true);
  web_ui()->ClearTrackedCalls();
}

TEST_F(ProfilePickerHandlerTest, RenameProfile) {
  ProfileAttributesEntry* profile_a = CreateTestingProfile("A");
  ProfileAttributesEntry* profile_b = CreateTestingProfile("B");
  ProfileAttributesEntry* profile_c = CreateTestingProfile("C");
  ProfileAttributesEntry* profile_d = CreateTestingProfile("D");

  InitializeMainViewAndVerifyProfileList(
      {profile_a, profile_b, profile_c, profile_d});
  web_ui()->ClearTrackedCalls();

  // The profile list doesn't get re-ordered after a rename.
  profile_b->SetLocalProfileName(u"X", false);
  VerifyProfileListWasPushed({profile_a, profile_b, profile_c, profile_d});
  web_ui()->ClearTrackedCalls();
}

TEST_F(ProfilePickerHandlerTest, RemoveProfile) {
  ProfileAttributesEntry* profile_a = CreateTestingProfile("A");
  ProfileAttributesEntry* profile_b = CreateTestingProfile("B");
  ProfileAttributesEntry* profile_c = CreateTestingProfile("C");
  ProfileAttributesEntry* profile_d = CreateTestingProfile("D");

  InitializeMainViewAndVerifyProfileList(
      {profile_a, profile_b, profile_c, profile_d});
  web_ui()->ClearTrackedCalls();

  base::FilePath b_path = profile_b->GetPath();
  profile_manager()->DeleteTestingProfile("B");
  VerifyProfileWasRemoved(b_path);
  VerifyIfGuestModeUpdateWasCalled(/*expected_guest_mode=*/true);
  web_ui()->ClearTrackedCalls();

  // Verify that the next profile push is correct.
  ProfileAttributesEntry* profile_e = CreateTestingProfile("E");
  VerifyProfileListWasPushed({profile_a, profile_c, profile_d, profile_e});
  VerifyIfGuestModeUpdateWasCalled(/*expected_guest_mode=*/true);
  web_ui()->ClearTrackedCalls();
}

TEST_F(ProfilePickerHandlerTest, RemoveOmittedProfile) {
  ProfileAttributesEntry* profile_a = CreateTestingProfile("A");
  ProfileAttributesEntry* profile_d = CreateTestingProfile("D");
  ProfileAttributesEntry* profile_c = CreateTestingProfile("C");
  ProfileAttributesEntry* profile_b = CreateTestingProfile("B");
  profile_b->SetIsEphemeral(true);
  profile_b->SetIsOmitted(true);

  InitializeMainViewAndVerifyProfileList({profile_a, profile_c, profile_d});
  web_ui()->ClearTrackedCalls();

  profile_manager()->DeleteTestingProfile("B");
  // No callbacks should be called.
  ASSERT_TRUE(web_ui()->call_data().empty());
  web_ui()->ClearTrackedCalls();

  // Verify that the next profile push is correct.
  ProfileAttributesEntry* profile_e = CreateTestingProfile("E");
  VerifyProfileListWasPushed({profile_a, profile_c, profile_d, profile_e});
  web_ui()->ClearTrackedCalls();
}

TEST_F(ProfilePickerHandlerTest, MarkProfileAsOmitted) {
  ProfileAttributesEntry* profile_a = CreateTestingProfile("A");
  ProfileAttributesEntry* profile_b = CreateTestingProfile("B");
  ProfileAttributesEntry* profile_c = CreateTestingProfile("C");
  ProfileAttributesEntry* profile_d = CreateTestingProfile("D");

  InitializeMainViewAndVerifyProfileList(
      {profile_a, profile_b, profile_c, profile_d});
  web_ui()->ClearTrackedCalls();

  profile_b->SetIsEphemeral(true);
  profile_b->SetIsOmitted(true);
  VerifyProfileWasRemoved(profile_b->GetPath());
  VerifyIfGuestModeUpdateWasCalled(/*expected_guest_mode=*/true);
  web_ui()->ClearTrackedCalls();

  // Omitted profile is appended to the end of the profile list.
  profile_b->SetIsOmitted(false);
  VerifyProfileListWasPushed({profile_a, profile_c, profile_d, profile_b});
  web_ui()->ClearTrackedCalls();
}

TEST_F(ProfilePickerHandlerTest, OmittedProfileOnInit) {
  ProfileAttributesEntry* profile_a = CreateTestingProfile("A");
  ProfileAttributesEntry* profile_b = CreateTestingProfile("B");
  ProfileAttributesEntry* profile_c = CreateTestingProfile("C");
  ProfileAttributesEntry* profile_d = CreateTestingProfile("D");
  profile_b->SetIsEphemeral(true);
  profile_b->SetIsOmitted(true);

  InitializeMainViewAndVerifyProfileList({profile_a, profile_c, profile_d});
  web_ui()->ClearTrackedCalls();

  profile_b->SetIsOmitted(false);
  VerifyProfileListWasPushed({profile_a, profile_c, profile_d, profile_b});
  web_ui()->ClearTrackedCalls();
}

// Tests the behavior of the profile picker handler in presence of supervised
// profiles.
class SupervisedProfilePickerHandlerTest
    : public ProfilePickerHandlerTest,
      public testing::WithParamInterface<
          /*HideGuestModeForSupervisedUsers=*/bool> {
 public:
  SupervisedProfilePickerHandlerTest() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    scoped_feature_list_.InitWithFeatureState(
        supervised_user::kHideGuestModeForSupervisedUsers,
        HideGuestModeForSupervisedUsersEnabled());
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  }

  bool HideGuestModeForSupervisedUsersEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(SupervisedProfilePickerHandlerTest,
       AddSupervisedProfileDisablesGuestMode) {
  ProfileAttributesEntry* profile_a = CreateTestingProfile("A");
  InitializeMainViewAndVerifyProfileList({profile_a});
  web_ui()->ClearTrackedCalls();

  // Adding a new supervised profile should disable the guest mode.
  CreateTestingProfile("B", /*is_supervised=*/true);
  VerifyIfGuestModeUpdateWasCalled(/*expected_guest_mode=*/false);
  web_ui()->ClearTrackedCalls();
}

TEST_P(SupervisedProfilePickerHandlerTest,
       RemoveLastSupervisedProfileEnablesGuestMode) {
  ProfileAttributesEntry* profile_a = CreateTestingProfile("A");
  ProfileAttributesEntry* profile_b =
      CreateTestingProfile("B", /*is_supervised=*/true);
  ProfileAttributesEntry* profile_c =
      CreateTestingProfile("C", /*is_supervised=*/true);

  InitializeMainViewAndVerifyProfileList({profile_a, profile_b, profile_c});
  web_ui()->ClearTrackedCalls();

  base::FilePath b_path = profile_b->GetPath();
  profile_manager()->DeleteTestingProfile("B");
  VerifyProfileWasRemoved(b_path);
  // Guest mode is still set to disabled, as there are more supervised profiles.
  VerifyIfGuestModeUpdateWasCalled(/*expected_guest_mode=*/false);
  web_ui()->ClearTrackedCalls();

  base::FilePath c_path = profile_c->GetPath();
  profile_manager()->DeleteTestingProfile("C");
  VerifyProfileWasRemoved(c_path);
  // Guest mode should be re-enabled after last supervised profile deletion.
  VerifyIfGuestModeUpdateWasCalled(/*expected_guest_mode=*/true);
  web_ui()->ClearTrackedCalls();
}

TEST_P(SupervisedProfilePickerHandlerTest,
       SettingSupervisedProfileRemovesGuestMode) {
  ProfileAttributesEntry* profile_a = CreateTestingProfile("A");
  ProfileAttributesEntry* profile_b = CreateTestingProfile("B");
  Profile* profile_b_ptr =
      profile_manager()->profile_manager()->GetProfileByPath(
          profile_b->GetPath());
  CHECK(profile_b_ptr);

  InitializeMainViewAndVerifyProfileList({profile_a, profile_b});
  web_ui()->ClearTrackedCalls();

  // Make Profile B supervised.
  profile_b_ptr->AsTestingProfile()->SetIsSupervisedProfile();
  profile_b->SetSupervisedUserId(
      profile_b_ptr->GetPrefs()->GetString(prefs::kSupervisedUserId));

  // Guest mode should be set to disabled.
  VerifyIfGuestModeUpdateWasCalled(/*expected_guest_mode=*/false);
  web_ui()->ClearTrackedCalls();
}

INSTANTIATE_TEST_SUITE_P(All,
                         SupervisedProfilePickerHandlerTest,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
                         testing::Bool(),
#else
      testing::Values(false),
#endif
                         [](const auto& info) {
                           return info.param ? "WithHideGuestModeEnabled"
                                             : "WithHideGuestModeDisabled";
                         });

#if BUILDFLAG(IS_CHROMEOS_LACROS)

// Tests that accounts available as primary are returned.
TEST_F(ProfilePickerHandlerTest, HandleGetAvailableAccounts_Empty) {
  CompleteFacadeGetAccounts({});

  // Send message to the handler.
  base::Value::List empty_args;
  web_ui()->HandleReceivedMessage("getAvailableAccounts", empty_args);

  // Check that the handler replied.
  ASSERT_TRUE(!web_ui()->call_data().empty());
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data.function_name());
  EXPECT_EQ("available-accounts-changed", data.arg1()->GetString());
  EXPECT_TRUE(data.arg2()->GetList().empty());
}

TEST_F(ProfilePickerHandlerTest, HandleGetAvailableAccounts_Available) {
  // AccountProfileMapper only allows available accounts if there are
  // multiple profiles.
  CreateTestingProfile("Primary");
  ProfileAttributesEntry* secondary = CreateTestingProfile("Secondary");

  // Add an available account into the facade
  const std::string kGaiaId1 = "some_gaia_id1";
  const std::string kGaiaId2 = "some_gaia_id2";
  const std::string kEmail2 = "example2@gmail.com";
  CompleteFacadeGetAccounts(
      {account_manager::Account{
           account_manager::AccountKey{kGaiaId1,
                                       account_manager::AccountType::kGaia},
           "example1@gmail.com"},
       account_manager::Account{
           account_manager::AccountKey{kGaiaId2,
                                       account_manager::AccountType::kGaia},
           kEmail2}});

  // ****** No accounts syncing in any profile: return all.
  // Send message to the handler.
  base::Value::List empty_args;
  web_ui()->HandleReceivedMessage("getAvailableAccounts", empty_args);

  // Check that the handler replied.
  ASSERT_TRUE(!web_ui()->call_data().empty());
  const content::TestWebUI::CallData& data1 = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data1.function_name());
  EXPECT_EQ("available-accounts-changed", data1.arg1()->GetString());
  EXPECT_EQ(data1.arg2()->GetList().size(), 2u);

  // ****** Account 1 syncing in Secondary profile: return account 1 and 2
  // regardless of syncing status.
  secondary->SetAuthInfo(kGaiaId1, u"example1@gmail.com",
                         /*is_consented_primary_account=*/true);
  // Send message to the handler.
  web_ui()->HandleReceivedMessage("getAvailableAccounts", empty_args);

  // Check that the handler replied.
  ASSERT_TRUE(!web_ui()->call_data().empty());
  const content::TestWebUI::CallData& data2 = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data2.function_name());
  EXPECT_EQ("available-accounts-changed", data2.arg1()->GetString());
  EXPECT_EQ(data2.arg2()->GetList().size(), 2u);
  // Arbitrary order of results; using a set to perform the search without
  // order.
  base::flat_set<std::string> gaia_id_results;
  const std::string* gaia_id1 =
      data2.arg2()->GetList()[0].GetDict().FindString("gaiaId");
  EXPECT_NE(gaia_id1, nullptr);
  gaia_id_results.insert(*gaia_id1);
  const std::string* gaia_id2 =
      data2.arg2()->GetList()[1].GetDict().FindString("gaiaId");
  EXPECT_NE(gaia_id2, nullptr);
  gaia_id_results.insert(*gaia_id2);
  EXPECT_TRUE(gaia_id_results.contains(kGaiaId1));
  EXPECT_TRUE(gaia_id_results.contains(kGaiaId2));
  // TODO(https://crbug/1226050): Test all other fields.
}

TEST_F(ProfilePickerHandlerTest, ProfilePickerObservesAvailableAccounts) {
  // AccountProfileMapper only allows available accounts if there are
  // multiple profiles.
  CreateTestingProfile("Primary");
  CreateTestingProfile("Secondary");

  // Add some available accounts into the facade.
  const std::string kGaiaId1 = "some_gaia_id1";
  const std::string kGaiaId2 = "some_gaia_id2";
  account_manager::Account account1{
      account_manager::AccountKey{kGaiaId1,
                                  account_manager::AccountType::kGaia},
      "example1@gmail.com"};
  account_manager::Account account2{
      account_manager::AccountKey{kGaiaId2,
                                  account_manager::AccountType::kGaia},
      "example2@gmail.com"};
  CompleteFacadeGetAccounts({account1, account2});

  // Send message to the handler.
  base::Value::List empty_args;
  web_ui()->HandleReceivedMessage("getAvailableAccounts", empty_args);

  // Check that the handler replied.
  ASSERT_TRUE(!web_ui()->call_data().empty());
  LOG(INFO) << web_ui()->call_data().size();
  const content::TestWebUI::CallData& data1 = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data1.function_name());
  EXPECT_EQ("available-accounts-changed", data1.arg1()->GetString());
  EXPECT_EQ(data1.arg2()->GetList().size(), 2u);

  // Add another account.
  const std::string kGaiaId = "some_gaia_id3";
  account_manager::Account new_account{
      account_manager::AccountKey{kGaiaId, account_manager::AccountType::kGaia},
      "example3@gmail.com"};
  profile_manager()
      ->profile_manager()
      ->GetAccountProfileMapper()
      ->OnAccountUpserted(new_account);
  CompleteFacadeGetAccounts({account1, account2, new_account});

  // Check that the profile picker handler picked up the new account, and
  // forwarded it to the Web UI.
  ASSERT_TRUE(!web_ui()->call_data().empty());
  LOG(INFO) << web_ui()->call_data().size();
  const content::TestWebUI::CallData& data2 = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data2.function_name());
  EXPECT_EQ("available-accounts-changed", data2.arg1()->GetString());
  EXPECT_EQ(data2.arg2()->GetList().size(), 3u);
}

TEST_F(ProfilePickerHandlerTest, CreateProfileExistingAccount) {
  const std::string kGaiaId = "some_gaia_id";
  const std::string kEmail = "example@gmail.com";
  SetAccountInErrorRequestResponse(kGaiaId, kEmail,
                                   /*account_in_error_response=*/false);

  // Lacros always expects a default profile.
  CreateTestingProfile("Default");

  // Add account to the facade.
  CompleteFacadeGetAccounts({account_manager::Account{
      account_manager::AccountKey{kGaiaId, account_manager::AccountType::kGaia},
      kEmail}});

  // OS account addition flow should not trigger.
  EXPECT_CALL(*mock_account_manager_facade(),
              ShowAddAccountDialog(testing::_, testing::_))
      .Times(0);

  // Expecting the account in error check call.
  EXPECT_CALL(*mock_identity_manager_lacros(),
              HasAccountWithPersistentError(kGaiaId, testing::_))
      .Times(1);
  // Since the account is not in error there is no call to getting the email.
  EXPECT_CALL(*mock_identity_manager_lacros(),
              GetAccountEmail(kGaiaId, testing::_))
      .Times(0);

  // Request profile creation with the existing account.
  ProfileWaiter profile_waiter;
  base::Value::List args;
  args.Append(/*color=*/base::Value());
  args.Append(/*gaiaId=*/kGaiaId);
  web_ui()->HandleReceivedMessage("selectExistingAccountLacros", args);

  // Check profile creation.
  Profile* new_profile = profile_waiter.WaitForProfileAdded();
  ASSERT_TRUE(new_profile);
  ProfileAttributesEntry* entry =
      profile_manager()
          ->profile_attributes_storage()
          ->GetProfileAttributesWithPath(new_profile->GetPath());
  ASSERT_TRUE(entry);
  EXPECT_EQ(entry->GetGaiaIds(), base::flat_set<std::string>{kGaiaId});

  // Set the primary account (simulate the `SigninManager`).
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(new_profile);
  std::vector<CoreAccountInfo> accounts =
      identity_manager->GetAccountsWithRefreshTokens();
  ASSERT_EQ(1u, accounts.size());
  identity_manager->GetPrimaryAccountMutator()->SetPrimaryAccount(
      accounts[0].account_id, signin::ConsentLevel::kSignin);

  // Check that the handler replied.
  ASSERT_TRUE(!web_ui()->call_data().empty());
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data.function_name());
  EXPECT_EQ("load-signin-finished", data.arg1()->GetString());
  bool success = data.arg2()->GetBool();
  EXPECT_TRUE(success);
}

TEST_F(ProfilePickerHandlerTest, CreateProfileExistingAccountInError) {
  const std::string kGaiaId = "some_gaia_id";
  const std::string kEmail = "example@gmail.com";
  SetAccountInErrorRequestResponse(kGaiaId, kEmail,
                                   /*account_in_error_response=*/true);

  // Add account to the facade.
  CompleteFacadeGetAccounts({account_manager::Account{
      account_manager::AccountKey{kGaiaId, account_manager::AccountType::kGaia},
      kEmail}});

  // Since account is in error, AccountManagerFacade will try to display the
  // Reauth Account dialog after getting the email. Testing version directly
  // redirects to the callback simulating the reauth dialog closed.
  EXPECT_CALL(*mock_identity_manager_lacros(),
              HasAccountWithPersistentError(kGaiaId, testing::_))
      .Times(1);

  EXPECT_CALL(*mock_identity_manager_lacros(),
              GetAccountEmail(kGaiaId, testing::_))
      .Times(1);

  base::Value::List args;
  args.Append(/*color=*/base::Value());
  args.Append(/*gaiaId=*/kGaiaId);
  web_ui()->HandleReceivedMessage("selectExistingAccountLacros", args);

  // Check that the handler replied.
  ASSERT_TRUE(!web_ui()->call_data().empty());
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data.function_name());
  EXPECT_EQ("reauth-dialog-closed", data.arg1()->GetString());
}

TEST_F(ProfilePickerHandlerTest, CreateProfileNewAccount) {
  // Lacros always expects a default profile.
  CreateTestingProfile("Default");
  CompleteFacadeGetAccounts({});

  // Mock the OS account addition.
  const std::string kGaiaId = "some_gaia_id";
  account_manager::Account account{
      account_manager::AccountKey{kGaiaId, account_manager::AccountType::kGaia},
      "example@gmail.com"};
  EXPECT_CALL(
      *mock_account_manager_facade(),
      ShowAddAccountDialog(account_manager::AccountManagerFacade::
                               AccountAdditionSource::kChromeProfileCreation,
                           testing::_))
      .WillOnce(
          [account, this](
              account_manager::AccountManagerFacade::AccountAdditionSource,
              base::OnceCallback<void(
                  const account_manager::AccountUpsertionResult&)> callback) {
            std::move(callback).Run(
                account_manager::AccountUpsertionResult::FromAccount(account));
            // Notify the mapper that an account has been added.
            profile_manager()
                ->profile_manager()
                ->GetAccountProfileMapper()
                ->OnAccountUpserted(account);
            CompleteFacadeGetAccounts({account});
          });

  // Request profile creation.
  ProfileWaiter profile_waiter;
  base::Value::List args;
  args.Append(/*color=*/base::Value());
  web_ui()->HandleReceivedMessage("selectNewAccount", args);

  // Check profile creation.
  Profile* new_profile = profile_waiter.WaitForProfileAdded();
  ASSERT_TRUE(new_profile);
  ProfileAttributesEntry* entry =
      profile_manager()
          ->profile_attributes_storage()
          ->GetProfileAttributesWithPath(new_profile->GetPath());
  ASSERT_TRUE(entry);
  EXPECT_EQ(entry->GetGaiaIds(), base::flat_set<std::string>{kGaiaId});

  // Set the primary account (simulate the `SigninManager`).
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(new_profile);
  std::vector<CoreAccountInfo> accounts =
      identity_manager->GetAccountsWithRefreshTokens();
  ASSERT_EQ(1u, accounts.size());
  identity_manager->GetPrimaryAccountMutator()->SetPrimaryAccount(
      accounts[0].account_id, signin::ConsentLevel::kSignin);

  // Check that the handler replied.
  ASSERT_TRUE(!web_ui()->call_data().empty());
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data.function_name());
  EXPECT_EQ("load-signin-finished", data.arg1()->GetString());
  bool success = data.arg2()->GetBool();
  EXPECT_TRUE(success);
}

class ProfilePickerHandlerInUserProfileTest : public ProfilePickerHandlerTest {
 public:
  ProfilePickerHandlerInUserProfileTest() = default;

  void SetUp() override {
    ProfilePickerHandlerTest::SetUp();
    // AccountProfileMapper only allows available accounts if there are
    // multiple profiles (another profile, named "Secondary", is created below).
    CreateTestingProfile("Primary");
  }

  Profile* GetWebUIProfile() override {
    if (!secondary_profile_)
      secondary_profile_ = profile_manager()->CreateTestingProfile("Secondary");
    return secondary_profile_;
  }

 protected:
  const base::Value& GetThemeInfoReply() {
    EXPECT_TRUE(!web_ui()->call_data().empty());
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    EXPECT_EQ(kTestCallbackId, data.arg1()->GetString());
    EXPECT_TRUE(data.arg2()->GetBool()) << "Callback should get resolved.";
    EXPECT_TRUE(data.arg3()->is_dict());
    return *data.arg3();
  }

  raw_ptr<Profile> secondary_profile_ = nullptr;
};

// Tests that accounts available as secondary are returned.
TEST_F(ProfilePickerHandlerInUserProfileTest,
       HandleGetAvailableAccounts_Empty) {
  CompleteFacadeGetAccounts({});

  // Send message to the handler.
  base::Value::List empty_args;
  web_ui()->HandleReceivedMessage("getAvailableAccounts", empty_args);

  // Check that the handler replied.
  ASSERT_TRUE(!web_ui()->call_data().empty());
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data.function_name());
  EXPECT_EQ("available-accounts-changed", data.arg1()->GetString());
  EXPECT_TRUE(data.arg2()->GetList().empty());
}

TEST_F(ProfilePickerHandlerInUserProfileTest,
       HandleGetAvailableAccounts_Available) {
  // Add an available account into the facade
  const std::string kGaiaId1 = "some_gaia_id1";
  const std::string kGaiaId2 = "some_gaia_id2";
  const std::string kEmail2 = "example2@gmail.com";
  CompleteFacadeGetAccounts(
      {account_manager::Account{
           account_manager::AccountKey{kGaiaId1,
                                       account_manager::AccountType::kGaia},
           "example1@gmail.com"},
       account_manager::Account{
           account_manager::AccountKey{kGaiaId2,
                                       account_manager::AccountType::kGaia},
           kEmail2}});

  // ****** No accounts assigned to "Secondary": return all.
  // Send message to the handler.
  base::Value::List empty_args;
  web_ui()->HandleReceivedMessage("getAvailableAccounts", empty_args);

  // Check that the handler replied.
  ASSERT_TRUE(!web_ui()->call_data().empty());
  const content::TestWebUI::CallData& data1 = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data1.function_name());
  EXPECT_EQ("available-accounts-changed", data1.arg1()->GetString());
  EXPECT_EQ(data1.arg2()->GetList().size(), 2u);

  // ****** Account 1 is assigned to "Secondary": return account 2.
  ProfileAttributesEntry* profile_entry =
      profile_manager()
          ->profile_attributes_storage()
          ->GetProfileAttributesWithPath(GetWebUIProfile()->GetPath());
  profile_entry->SetGaiaIds({kGaiaId1});
  // Send message to the handler.
  web_ui()->HandleReceivedMessage("getAvailableAccounts", empty_args);

  // Check that the handler replied.
  ASSERT_TRUE(!web_ui()->call_data().empty());
  const content::TestWebUI::CallData& data2 = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data2.function_name());
  EXPECT_EQ("available-accounts-changed", data2.arg1()->GetString());
  EXPECT_EQ(data2.arg2()->GetList().size(), 1u);
  const std::string* gaia_id =
      data2.arg2()->GetList()[0].GetDict().FindString("gaiaId");
  EXPECT_NE(gaia_id, nullptr);
  EXPECT_EQ(*gaia_id, kGaiaId2);
}

TEST_F(ProfilePickerHandlerInUserProfileTest,
       HandleExtendedAccountInformation) {
  std::string kGaiaId1 = "some_gaia_id1";
  std::string kEmail1 = "example1@gmail.com";
  std::string kFullName1 = "Example Name1";
  GetAccountInformationHelper::GetAccountInformationResult account1;
  account1.gaia = kGaiaId1;
  account1.email = kEmail1;
  account1.full_name = kFullName1;
  account1.account_image = gfx::test::CreateImage(100, 100);

  // Explicitly call the function so we can pass the resulting account info.
  handler()->AllowJavascript();
  handler()->SendAvailableAccounts({account1});

  // Check that the handler replied.
  ASSERT_TRUE(!web_ui()->call_data().empty());
  const content::TestWebUI::CallData& data1 = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data1.function_name());
  EXPECT_EQ("available-accounts-changed", data1.arg1()->GetString());
  EXPECT_EQ(data1.arg2()->GetList().size(), 1u);
  const std::string* gaia_id =
      data1.arg2()->GetList()[0].GetDict().FindString("gaiaId");
  EXPECT_NE(gaia_id, nullptr);
  EXPECT_EQ(*gaia_id, kGaiaId1);
  const std::string* email =
      data1.arg2()->GetList()[0].GetDict().FindString("email");
  EXPECT_NE(email, nullptr);
  EXPECT_EQ(*email, kEmail1);
  const std::string* full_name =
      data1.arg2()->GetList()[0].GetDict().FindString("name");
  EXPECT_NE(full_name, nullptr);
  EXPECT_EQ(*full_name, kFullName1);
  const std::string* account_image_url =
      data1.arg2()->GetList()[0].GetDict().FindString("accountImageUrl");
  EXPECT_NE(account_image_url, nullptr);
}

TEST_F(ProfilePickerHandlerInUserProfileTest,
       HandleGetNewProfileSuggestedThemeInfo_Default) {
  // Send message to the handler.
  base::Value::List args;
  args.Append(kTestCallbackId);
  web_ui()->HandleReceivedMessage("getNewProfileSuggestedThemeInfo", args);

  // Check that the handler replied correctly.
  const base::Value::Dict& theme_info = GetThemeInfoReply().GetDict();
  EXPECT_EQ(-1, *theme_info.FindInt("colorId"));  // -1: default color
  EXPECT_EQ(std::nullopt, theme_info.FindInt("color"));
}

TEST_F(ProfilePickerHandlerInUserProfileTest,
       HandleGetNewProfileSuggestedThemeInfo_Color) {
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(GetWebUIProfile());
  theme_service->BuildAutogeneratedThemeFromColor(SK_ColorRED);

  // Send message to the handler.
  base::Value::List args;
  args.Append(kTestCallbackId);
  web_ui()->HandleReceivedMessage("getNewProfileSuggestedThemeInfo", args);

  // Check that the handler replied correctly.
  const base::Value::Dict& theme_info = GetThemeInfoReply().GetDict();
  EXPECT_EQ(0, *theme_info.FindInt("colorId"));  // 0: manually picked color.
  EXPECT_EQ(SK_ColorRED, static_cast<SkColor>(*theme_info.FindInt("color")));
}

TEST_F(ProfilePickerHandlerInUserProfileTest, NoAvailableAccount) {
  // Lacros always expects a default profile.
  CreateTestingProfile("Default");
  CompleteFacadeGetAccounts({});
  const std::string kGaiaId = "some_gaia_id";

  // Set a callback for account selection.
  testing::StrictMock<base::MockOnceCallback<void(const std::string&)>>
      callback;
  ProfilePicker::Show(ProfilePicker::Params::ForLacrosSelectAvailableAccount(
      base::FilePath(), callback.Get()));
  EXPECT_CALL(callback, Run(kGaiaId));

  // Mock the OS account addition.
  account_manager::Account account{
      account_manager::AccountKey{kGaiaId, account_manager::AccountType::kGaia},
      "example@gmail.com"};
  EXPECT_CALL(*mock_account_manager_facade(),
              ShowAddAccountDialog(account_manager::AccountManagerFacade::
                                       AccountAdditionSource::kOgbAddAccount,
                                   testing::_))
      .WillOnce(
          [account, this](
              account_manager::AccountManagerFacade::AccountAdditionSource,
              base::OnceCallback<void(
                  const account_manager::AccountUpsertionResult&)> callback) {
            std::move(callback).Run(
                account_manager::AccountUpsertionResult::FromAccount(account));
            // Notify the mapper that an account has been added.
            profile_manager()
                ->profile_manager()
                ->GetAccountProfileMapper()
                ->OnAccountUpserted(account);
            CompleteFacadeGetAccounts({account});
          });

  // Request account addition.
  base::Value::List args;
  args.Append(/*color=*/base::Value());
  web_ui()->HandleReceivedMessage("selectNewAccount", args);
}

TEST_F(ProfilePickerHandlerInUserProfileTest,
       ExistingProfileExistingAccountInError) {
  const std::string kGaiaId = "some_gaia_id";
  const std::string kEmail = "example@gmail.com";
  SetAccountInErrorRequestResponse(kGaiaId, kEmail,
                                   /*account_in_error_response=*/true);

  // Add account to the facade.
  CompleteFacadeGetAccounts({account_manager::Account{
      account_manager::AccountKey{kGaiaId, account_manager::AccountType::kGaia},
      kEmail}});

  // Since account is in error, AccountManagerFacade will try to display the
  // Reauth Account dialog after getting the email. Testing version directly
  // redirects to the callback simulating the reauth dialog closed.
  EXPECT_CALL(*mock_identity_manager_lacros(),
              HasAccountWithPersistentError(kGaiaId, testing::_))
      .Times(1);
  EXPECT_CALL(*mock_identity_manager_lacros(),
              GetAccountEmail(kGaiaId, testing::_))
      .Times(1);

  base::Value::List args;
  args.Append(/*color=*/base::Value());
  args.Append(/*gaiaId=*/kGaiaId);
  web_ui()->HandleReceivedMessage("selectExistingAccountLacros", args);

  // Check that the handler replied.
  ASSERT_TRUE(!web_ui()->call_data().empty());
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data.function_name());
  EXPECT_EQ("reauth-dialog-closed", data.arg1()->GetString());
}

#endif  //  BUILDFLAG(IS_CHROMEOS_LACROS)

TEST_F(ProfilePickerHandlerTest, UpdateProfileOrder) {
  auto entries_to_names =
      [](const std::vector<ProfileAttributesEntry*> entries) {
        std::vector<std::string> names;
        for (auto* entry : entries) {
          names.emplace_back(base::UTF16ToUTF8(entry->GetLocalProfileName()));
        }
        return names;
      };

  std::vector<std::string> profile_names = {"A", "B", "C", "D"};
  for (auto name : profile_names) {
    CreateTestingProfile(name);
  }

  ProfileAttributesStorage* storage =
      profile_manager()->profile_attributes_storage();
  std::vector<ProfileAttributesEntry*> display_entries =
      storage->GetAllProfilesAttributesSortedForDisplay();
  ASSERT_EQ(entries_to_names(display_entries), profile_names);

  // Perform first changes.
  {
    base::Value::List args;
    args.Append(0);  // `from_index`
    args.Append(2);  // `to_index`
    web_ui()->HandleReceivedMessage("updateProfileOrder", args);

    std::vector<std::string> expected_profile_order_names{"B", "C", "A", "D"};
    EXPECT_EQ(
        entries_to_names(storage->GetAllProfilesAttributesSortedForDisplay()),
        expected_profile_order_names);
  }

  // Perform second changes.
  {
    base::Value::List args;
    args.Append(1);  // `from_index`
    args.Append(3);  // `to_index`
    web_ui()->HandleReceivedMessage("updateProfileOrder", args);

    std::vector<std::string> expected_profile_order_names{"B", "A", "D", "C"};
    EXPECT_EQ(
        entries_to_names(storage->GetAllProfilesAttributesSortedForDisplay()),
        expected_profile_order_names);
  }
}
