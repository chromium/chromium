// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_manage_profile_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace settings {

namespace {

class TestManageProfileHandler : public ManageProfileHandler {
 public:
  explicit TestManageProfileHandler(Profile* profile)
      : ManageProfileHandler(profile) {}

  using ManageProfileHandler::set_web_ui;
  using ManageProfileHandler::AllowJavascript;
};

}  // namespace

class ManageProfileHandlerTest : public testing::Test {
 public:
  ManageProfileHandlerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        profile_(nullptr) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("Profile 1");

    handler_ = std::make_unique<TestManageProfileHandler>(profile_);
    handler_->set_web_ui(&web_ui_);
    handler()->AllowJavascript();
    web_ui()->ClearTrackedCalls();
  }

  void VerifyIconListWithNoneSelected(const base::Value* value) {
    VerifyIconList(value, 0 /* ignored */, true);
  }

  void VerifyIconListWithSingleSelection(const base::Value* value,
                                         size_t selected_index) {
    VerifyIconList(value, selected_index, false);
  }

  content::TestWebUI* web_ui() { return &web_ui_; }
  Profile* profile() const { return profile_; }
  TestManageProfileHandler* handler() const { return handler_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  content::TestWebUI web_ui_;

  Profile* profile_;
  std::unique_ptr<TestManageProfileHandler> handler_;

  void VerifyIconList(const base::Value* value,
                      size_t selected_index,
                      bool all_not_selected) {
    const base::ListValue* icons = nullptr;
    ASSERT_TRUE(value->GetAsList(&icons));

    // Expect a non-empty list of dictionaries containing non-empty strings for
    // profile avatar icon urls and labels.
    EXPECT_FALSE(icons->empty());
    for (size_t i = 0; i < icons->GetSize(); ++i) {
      const base::DictionaryValue* icon = nullptr;
      EXPECT_TRUE(icons->GetDictionary(i, &icon));
      std::string icon_url;
      size_t icon_index;
      EXPECT_TRUE(icon->GetString("url", &icon_url));
      EXPECT_FALSE(icon_url.empty());
      EXPECT_TRUE(profiles::IsDefaultAvatarIconUrl(icon_url, &icon_index));
      std::string icon_label;
      EXPECT_TRUE(icon->GetString("label", &icon_label));
      EXPECT_FALSE(icon_label.empty());
      bool icon_selected;
      bool has_icon_selected = icon->GetBoolean("selected", &icon_selected);
      if (all_not_selected) {
        EXPECT_FALSE(has_icon_selected);
      } else if (selected_index == icon_index) {
        EXPECT_TRUE(has_icon_selected);
        EXPECT_TRUE(icon_selected);
      }
    }
  }
};

TEST_F(ManageProfileHandlerTest, HandleSetProfileIconToGaiaAvatar) {
  handler()->HandleSetProfileIconToGaiaAvatar(nullptr);

  PrefService* pref_service = profile()->GetPrefs();
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kProfileUsingDefaultAvatar));
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kProfileUsingGAIAAvatar));
}

TEST_F(ManageProfileHandlerTest, HandleSetProfileIconToDefaultAvatar) {
  base::ListValue list_args;
  list_args.AppendString("chrome://theme/IDR_PROFILE_AVATAR_15");
  handler()->HandleSetProfileIconToDefaultAvatar(&list_args);

  PrefService* pref_service = profile()->GetPrefs();
  EXPECT_EQ(15, pref_service->GetInteger(prefs::kProfileAvatarIndex));
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kProfileUsingDefaultAvatar));
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kProfileUsingGAIAAvatar));
}

TEST_F(ManageProfileHandlerTest, HandleSetProfileName) {
  base::ListValue list_args;
  list_args.AppendString("New Profile Name");
  handler()->HandleSetProfileName(&list_args);

  PrefService* pref_service = profile()->GetPrefs();
  EXPECT_EQ("New Profile Name", pref_service->GetString(prefs::kProfileName));
}

TEST_F(ManageProfileHandlerTest, HandleGetAvailableIcons) {
  PrefService* pref_service = profile()->GetPrefs();
  pref_service->SetInteger(prefs::kProfileAvatarIndex, 27);

  base::ListValue list_args_1;
  list_args_1.AppendString("get-icons-callback-id");
  handler()->HandleGetAvailableIcons(&list_args_1);

  EXPECT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& data_1 = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data_1.function_name());

  std::string callback_id_1;
  ASSERT_TRUE(data_1.arg1()->GetAsString(&callback_id_1));
  EXPECT_EQ("get-icons-callback-id", callback_id_1);

  VerifyIconListWithSingleSelection(data_1.arg3(), 27);
}

TEST_F(ManageProfileHandlerTest, HandleGetAvailableIconsOldIconSelected) {
  PrefService* pref_service = profile()->GetPrefs();
  pref_service->SetInteger(prefs::kProfileAvatarIndex, 7);

  base::ListValue list_args;
  list_args.AppendString("get-icons-callback-id");
  handler()->HandleGetAvailableIcons(&list_args);

  EXPECT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data.function_name());

  std::string callback_id;
  ASSERT_TRUE(data.arg1()->GetAsString(&callback_id));
  EXPECT_EQ("get-icons-callback-id", callback_id);

  VerifyIconListWithNoneSelected(data.arg3());
}

TEST_F(ManageProfileHandlerTest, HandleGetAvailableIconsGaiaAvatarSelected) {
  PrefService* pref_service = profile()->GetPrefs();
  pref_service->SetInteger(prefs::kProfileAvatarIndex, 27);
  pref_service->SetBoolean(prefs::kProfileUsingGAIAAvatar, true);

  base::ListValue list_args;
  list_args.AppendString("get-icons-callback-id");
  handler()->HandleGetAvailableIcons(&list_args);

  EXPECT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data.function_name());

  std::string callback_id;
  ASSERT_TRUE(data.arg1()->GetAsString(&callback_id));
  EXPECT_EQ("get-icons-callback-id", callback_id);

  VerifyIconListWithNoneSelected(data.arg3());
}

TEST_F(ManageProfileHandlerTest, ProfileAvatarChangedWebUIEvent) {
  PrefService* pref_service = profile()->GetPrefs();
  pref_service->SetInteger(prefs::kProfileAvatarIndex, 12);

  handler()->OnProfileAvatarChanged(base::FilePath());

  EXPECT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data.function_name());

  std::string event_id;
  ASSERT_TRUE(data.arg1()->GetAsString(&event_id));
  EXPECT_EQ("available-icons-changed", event_id);
  VerifyIconListWithSingleSelection(data.arg2(), 12);
}

}  // namespace settings
