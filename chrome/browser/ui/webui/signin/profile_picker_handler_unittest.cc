// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/profile_picker_handler.h"
#include "base/strings/utf_string_conversions.h"
#include "base/util/values/values_util.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

void VerifyProfileEntry(const base::Value& value,
                        ProfileAttributesEntry* entry) {
  EXPECT_EQ(*value.FindKey("profilePath"),
            util::FilePathToValue(entry->GetPath()));
  EXPECT_EQ(*value.FindStringKey("localProfileName"),
            base::UTF16ToUTF8(entry->GetLocalProfileName()));
  EXPECT_EQ(value.FindBoolKey("isSyncing"),
            entry->GetSigninState() ==
                SigninState::kSignedInWithConsentedPrimaryAccount);
  EXPECT_EQ(value.FindBoolKey("needsSignin"), entry->IsSigninRequired());
  EXPECT_EQ(*value.FindStringKey("gaiaName"),
            base::UTF16ToUTF8(entry->GetGAIANameToDisplay()));
  EXPECT_EQ(*value.FindStringKey("userName"),
            base::UTF16ToUTF8(entry->GetUserName()));
  EXPECT_EQ(value.FindBoolKey("isManaged"),
            AccountInfo::IsManaged(entry->GetHostedDomain()));
}

class ProfilePickerHandlerTest : public testing::Test {
 public:
  ProfilePickerHandlerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    handler_.set_web_ui(&web_ui_);
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
      VerifyProfileEntry(data.arg2()->GetList()[i], ordered_profile_entries[i]);
    }
  }

  void VerifyProfileWasRemoved(const base::FilePath& profile_path) {
    ASSERT_TRUE(!web_ui()->call_data().empty());
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    ASSERT_EQ("cr.webUIListenerCallback", data.function_name());
    ASSERT_EQ("profile-removed", data.arg1()->GetString());
    ASSERT_EQ(*data.arg2(), util::FilePathToValue(profile_path));
  }

  void InitializeMainViewAndVerifyProfileList(
      const std::vector<ProfileAttributesEntry*>& ordered_profile_entries) {
    base::ListValue empty_args;
    handler()->HandleMainViewInitialize(&empty_args);
    VerifyProfileListWasPushed(ordered_profile_entries);
  }

  // Creates a new testing profile and returns its `ProfileAttributesEntry`.
  ProfileAttributesEntry* CreateTestingProfile(
      const std::string& profile_name) {
    auto* profile = profile_manager()->CreateTestingProfile(profile_name);
    ProfileAttributesEntry* entry =
        profile_manager()
            ->profile_attributes_storage()
            ->GetProfileAttributesWithPath(profile->GetPath());
    CHECK(entry);
    return entry;
  }

  TestingProfileManager* profile_manager() { return &profile_manager_; }
  content::TestWebUI* web_ui() { return &web_ui_; }
  ProfilePickerHandler* handler() { return &handler_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  content::TestWebUI web_ui_;
  ProfilePickerHandler handler_;
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
  web_ui()->ClearTrackedCalls();
}

TEST_F(ProfilePickerHandlerTest, AddProfileToEmptyList) {
  InitializeMainViewAndVerifyProfileList({});
  web_ui()->ClearTrackedCalls();

  ProfileAttributesEntry* profile = CreateTestingProfile("Profile");
  VerifyProfileListWasPushed({profile});
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
  web_ui()->ClearTrackedCalls();

  // Verify that the next profile push is correct.
  ProfileAttributesEntry* profile_e = CreateTestingProfile("E");
  VerifyProfileListWasPushed({profile_a, profile_c, profile_d, profile_e});
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
  VerifyProfileListWasPushed({profile_a, profile_c, profile_d});
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
