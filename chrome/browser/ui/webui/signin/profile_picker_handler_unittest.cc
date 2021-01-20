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

  TestingProfileManager* profile_manager() { return &profile_manager_; }
  content::TestWebUI* web_ui() { return &web_ui_; }
  ProfilePickerHandler* handler() { return &handler_; }

  void VerifyProfileListWasPushed(
      std::vector<ProfileAttributesEntry*> ordered_profile_entries) {
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

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  content::TestWebUI web_ui_;
  ProfilePickerHandler handler_;
};

TEST_F(ProfilePickerHandlerTest, MarkProfileAsOmitted) {
  base::ListValue empty_args;
  handler()->HandleMainViewInitialize(&empty_args);
  VerifyProfileListWasPushed({});
  web_ui()->ClearTrackedCalls();

  const char kProfileName[] = "Profile";
  auto* profile = profile_manager()->CreateTestingProfile(kProfileName);
  ProfileAttributesEntry* entry = nullptr;
  ASSERT_TRUE(profile_manager()
                  ->profile_attributes_storage()
                  ->GetProfileAttributesWithPath(profile->GetPath(), &entry));
  VerifyProfileListWasPushed({entry});
  web_ui()->ClearTrackedCalls();

  entry->SetIsEphemeral(true);
  entry->SetIsOmitted(true);
  VerifyProfileListWasPushed({});
  web_ui()->ClearTrackedCalls();

  entry->SetIsOmitted(false);
  entry->SetIsEphemeral(false);
  VerifyProfileListWasPushed({entry});
  web_ui()->ClearTrackedCalls();
}

TEST_F(ProfilePickerHandlerTest, OmittedProfileOnInit) {
  const char kProfileName[] = "Profile";
  auto* profile = profile_manager()->CreateTestingProfile(kProfileName);
  ProfileAttributesEntry* entry = nullptr;
  ASSERT_TRUE(profile_manager()
                  ->profile_attributes_storage()
                  ->GetProfileAttributesWithPath(profile->GetPath(), &entry));
  entry->SetIsEphemeral(true);
  entry->SetIsOmitted(true);

  base::ListValue empty_args;
  handler()->HandleMainViewInitialize(&empty_args);
  VerifyProfileListWasPushed({});
  web_ui()->ClearTrackedCalls();

  entry->SetIsOmitted(false);
  entry->SetIsEphemeral(false);
  VerifyProfileListWasPushed({entry});
  web_ui()->ClearTrackedCalls();
}
