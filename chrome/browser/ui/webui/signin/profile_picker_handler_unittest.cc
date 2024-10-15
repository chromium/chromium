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

namespace {

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

 private:
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
