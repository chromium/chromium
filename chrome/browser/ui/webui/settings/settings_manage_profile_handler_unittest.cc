// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_manage_profile_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image_unittest_util.h"

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
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("Profile 1");
    entry_ = profile_manager_.profile_attributes_storage()
                 ->GetProfileAttributesWithPath(profile_->GetPath());
    ASSERT_NE(entry_, nullptr);
    entry_->SetAvatarIconIndex(profiles::GetPlaceholderAvatarIndex());

    handler_ = std::make_unique<TestManageProfileHandler>(profile_);
    handler_->set_web_ui(&web_ui_);
    handler()->AllowJavascript();
    web_ui()->ClearTrackedCalls();
  }

  void SetSignedInProfile() {
    gfx::Image gaia_image(gfx::test::CreateImage(256, 256));
    entry()->SetAuthInfo("gaia_id", u"user@gmail.com", false);
    entry()->SetGAIAPicture("GAIA_IMAGE_URL_WITH_SIZE", gaia_image);
    EXPECT_TRUE(entry()->IsUsingDefaultAvatar());
    EXPECT_TRUE(entry()->IsUsingGAIAPicture());
  }

  void VerifyIconListWithOnlyCustomAvatars(const base::Value* value,
                                           size_t selected_index) {
    VerifyIconList(value, selected_index, false, false);
  }

  void VerifyIconList(const base::Value* icons,
                      size_t selected_index,
                      bool gaia_included,
                      bool gaia_selected) {
    ASSERT_TRUE(icons->is_list());

    // Expect a non-empty list of dictionaries containing non-empty strings for
    // profile avatar icon urls and labels.
    EXPECT_FALSE(icons->GetList().empty());
    if (gaia_included) {
      VerifyGaiaAvatar(icons, gaia_selected);
    } else {
      // Local profile
      VerifyDefaultGenericAvatar(icons, selected_index);
    }
    bool selected_found =
        gaia_selected ||
        (selected_index == profiles::GetPlaceholderAvatarIndex());

    for (size_t i = 1; i < icons->GetList().size(); ++i) {
      const base::Value& icon = icons->GetList()[i];
      EXPECT_TRUE(icon.is_dict());

      const base::Value::Dict& icon_dict = icon.GetDict();
      const std::string* icon_url = icon_dict.FindString("url");
      EXPECT_TRUE(icon_url);
      EXPECT_FALSE(icon_url->empty());

      int icon_index_int = *icon_dict.FindInt("index");
      EXPECT_TRUE(profiles::IsDefaultAvatarIconIndex(icon_index_int));
      size_t icon_index = static_cast<size_t>(icon_index_int);
      EXPECT_NE(icon_index, profiles::GetPlaceholderAvatarIndex());
      EXPECT_NE(icon_index_int, 0);
      size_t url_icon_index;
      EXPECT_TRUE(profiles::IsDefaultAvatarIconUrl(*icon_url, &url_icon_index));
      EXPECT_EQ(icon_index, url_icon_index);
      EXPECT_TRUE(!icon_dict.FindString("label")->empty());
      std::optional<bool> current_selected = icon_dict.FindBool("selected");
      if (selected_index == icon_index) {
        EXPECT_FALSE(selected_found);
        EXPECT_TRUE(current_selected.value_or(false));
        selected_found = true;
      } else {
        EXPECT_FALSE(current_selected.value_or(false));
      }
    }

    EXPECT_TRUE(selected_index == 0 || selected_found);
  }

  content::TestWebUI* web_ui() { return &web_ui_; }
  Profile* profile() const { return profile_; }
  ProfileAttributesEntry* entry() const { return entry_; }
  TestManageProfileHandler* handler() const { return handler_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<ProfileAttributesEntry> entry_ = nullptr;
  content::TestWebUI web_ui_;

  raw_ptr<Profile> profile_ = nullptr;
  std::unique_ptr<TestManageProfileHandler> handler_;

  void VerifyGaiaAvatar(const base::Value* icons, bool gaia_selected) {
    const base::Value& icon = icons->GetList()[0];
    EXPECT_TRUE(icon.is_dict());
    const base::Value::Dict& icon_dict = icon.GetDict();
    EXPECT_EQ(*icon_dict.FindInt("index"), 0);

    const gfx::Image* avatar_icon = entry()->GetGAIAPicture();
    ASSERT_TRUE(avatar_icon);
    EXPECT_EQ(*icon_dict.FindString("url"),
              webui::GetBitmapDataUrl(
                  profiles::GetAvatarIconForWebUI(*avatar_icon).AsBitmap()));
    EXPECT_TRUE(!icon_dict.FindString("label")->empty());
    EXPECT_EQ(*icon_dict.FindBool("selected"), gaia_selected);
  }

  void VerifyDefaultGenericAvatar(const base::Value* icons,
                                  size_t selected_index) {
    const base::Value& icon = icons->GetList()[0];
    EXPECT_TRUE(icon.is_dict());
    const base::Value::Dict& icon_dict = icon.GetDict();
    EXPECT_TRUE(!icon_dict.FindString("label")->empty());
    int icon_index_int = icon_dict.FindInt("index").value_or(0);
    EXPECT_TRUE(icon_index_int != 0);
    size_t icon_index = static_cast<size_t>(icon_index_int);
    EXPECT_EQ(icon_index, profiles::GetPlaceholderAvatarIndex());
    EXPECT_EQ(*icon_dict.FindBool("selected"), selected_index == icon_index);
  }
};

TEST_F(ManageProfileHandlerTest, HandleSetProfileIconToGaiaAvatar) {
  handler()->HandleSetProfileIconToGaiaAvatar(base::Value::List());

  PrefService* pref_service = profile()->GetPrefs();
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kProfileUsingDefaultAvatar));
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kProfileUsingGAIAAvatar));
}

TEST_F(ManageProfileHandlerTest, HandleSetProfileIconToDefaultCustomAvatar) {
  base::Value::List list_args;
  list_args.Append(15);
  handler()->HandleSetProfileIconToDefaultAvatar(list_args);

  PrefService* pref_service = profile()->GetPrefs();
  EXPECT_EQ(15, pref_service->GetInteger(prefs::kProfileAvatarIndex));
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kProfileUsingDefaultAvatar));
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kProfileUsingGAIAAvatar));
}

TEST_F(ManageProfileHandlerTest, HandleSetProfileIconToDefaultGenericAvatar) {
  int generic_avatar_index = profiles::GetPlaceholderAvatarIndex();
  base::Value::List list_args;
  list_args.Append(generic_avatar_index);
  handler()->HandleSetProfileIconToDefaultAvatar(list_args);

  PrefService* pref_service = profile()->GetPrefs();
  EXPECT_EQ(generic_avatar_index,
            pref_service->GetInteger(prefs::kProfileAvatarIndex));
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kProfileUsingDefaultAvatar));
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kProfileUsingGAIAAvatar));
}

TEST_F(ManageProfileHandlerTest, HandleSetProfileName) {
  base::Value::List list_args;
  list_args.Append("New Profile Name");
  handler()->HandleSetProfileName(list_args);

  PrefService* pref_service = profile()->GetPrefs();
  EXPECT_EQ("New Profile Name", pref_service->GetString(prefs::kProfileName));
}

TEST_F(ManageProfileHandlerTest, HandleGetAvailableIcons) {
  // Set avatar icon will trigger avatar icons updated event.
  entry()->SetIsUsingDefaultAvatar(false);
  entry()->SetAvatarIconIndex(27);
  EXPECT_EQ(1U, web_ui()->call_data().size());
  web_ui()->ClearTrackedCalls();

  base::Value::List list_args_1;
  list_args_1.Append("get-icons-callback-id");
  handler()->HandleGetAvailableIcons(list_args_1);

  EXPECT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& data_1 = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data_1.function_name());
  EXPECT_EQ("get-icons-callback-id", data_1.arg1()->GetString());

  VerifyIconListWithOnlyCustomAvatars(data_1.arg3(), 27);
}

TEST_F(ManageProfileHandlerTest, HandleGetAvailableIconsOldIconSelected) {
  // Set avatar icon will trigger avatar icons updated event.
  entry()->SetAvatarIconIndex(7);
  EXPECT_EQ(1U, web_ui()->call_data().size());
  web_ui()->ClearTrackedCalls();

  base::Value::List list_args;
  list_args.Append("get-icons-callback-id");
  handler()->HandleGetAvailableIcons(list_args);

  EXPECT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data.function_name());
  EXPECT_EQ("get-icons-callback-id", data.arg1()->GetString());

  VerifyIconListWithOnlyCustomAvatars(data.arg3(), 0);
}

TEST_F(ManageProfileHandlerTest, GetAvailableIconsSignedInProfile) {
  SetSignedInProfile();
  EXPECT_TRUE(entry()->IsUsingDefaultAvatar());
  EXPECT_TRUE(entry()->IsUsingGAIAPicture());
  web_ui()->ClearTrackedCalls();

  base::Value::List list_args;
  list_args.Append("get-icons-callback-id");
  handler()->HandleGetAvailableIcons(list_args);

  EXPECT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data.function_name());

  EXPECT_EQ("get-icons-callback-id", data.arg1()->GetString());
  VerifyIconList(data.arg3(), /*selected_index=*/0,
                 /*gaia_included=*/true,
                 /*gaia_selected=*/true);

  web_ui()->ClearTrackedCalls();

  // Set custom avatar
  entry()->SetIsUsingDefaultAvatar(false);
  entry()->SetAvatarIconIndex(30);

  const content::TestWebUI::CallData& data_1 = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data_1.function_name());

  EXPECT_EQ("available-icons-changed", data_1.arg1()->GetString());
  VerifyIconList(data_1.arg2(), /*selected_index=*/30,
                 /*gaia_included=*/true,
                 /*gaia_selected=*/false);

  // Sign out.
  entry()->SetAuthInfo("", std::u16string(), false);
  entry()->SetGAIAPicture(std::string(), gfx::Image());

  const content::TestWebUI::CallData& data_2 = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data_2.function_name());

  EXPECT_EQ("available-icons-changed", data_2.arg1()->GetString());
  VerifyIconList(data_2.arg2(), /*selected_index=*/30,
                 /*gaia_included=*/false,
                 /*gaia_selected=*/false);
}

TEST_F(ManageProfileHandlerTest, GetAvailableIconsLocalProfile) {
  EXPECT_FALSE(entry()->IsUsingGAIAPicture());
  EXPECT_EQ(entry()->GetAvatarIconIndex(),
            profiles::GetPlaceholderAvatarIndex());

  base::Value::List list_args;
  list_args.Append("get-icons-callback-id");
  handler()->HandleGetAvailableIcons(list_args);

  EXPECT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data.function_name());

  std::string callback_id = data.arg1()->GetString();
  EXPECT_EQ("get-icons-callback-id", callback_id);
  VerifyIconList(data.arg3(), /*selected_index=*/entry()->GetAvatarIconIndex(),
                 /*gaia_included=*/false,
                 /*gaia_selected=*/false);

  // Sign in.
  SetSignedInProfile();
  EXPECT_TRUE(entry()->IsUsingGAIAPicture());
  const content::TestWebUI::CallData& data_1 = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data_1.function_name());

  EXPECT_EQ("available-icons-changed", data_1.arg1()->GetString());
  VerifyIconList(data_1.arg2(), /*selected_index=*/0,
                 /*gaia_included=*/true,
                 /*gaia_selected=*/true);
}

TEST_F(ManageProfileHandlerTest, ProfileAvatarChangedWebUIEvent) {
  entry()->SetIsUsingDefaultAvatar(false);
  entry()->SetAvatarIconIndex(27);

  EXPECT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data.function_name());

  EXPECT_EQ("available-icons-changed", data.arg1()->GetString());
  VerifyIconListWithOnlyCustomAvatars(data.arg2(), 27);
}

TEST_F(ManageProfileHandlerTest, ProfileThemeColorsChangedWebUIEvent) {
  ProfileThemeColors colors = {SK_ColorTRANSPARENT, SK_ColorBLACK,
                               SK_ColorWHITE};
  entry()->SetProfileThemeColors(colors);

  // The expected number of calls are two, since the profile avatar has changed
  // along with the color, as the current profile avatar is the generic colored
  // avatar.
  EXPECT_EQ(2U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data.function_name());
  EXPECT_EQ("available-icons-changed", data.arg1()->GetString());
  VerifyIconList(data.arg2(), /*selected_index=*/entry()->GetAvatarIconIndex(),
                 /*gaia_included=*/false,
                 /*gaia_selected=*/false);

  // Set custom avatar.
  entry()->SetIsUsingDefaultAvatar(false);
  entry()->SetAvatarIconIndex(37);
  web_ui()->ClearTrackedCalls();

  entry()->SetProfileThemeColors(std::nullopt);
  EXPECT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& data_1 = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data_1.function_name());
  EXPECT_EQ("available-icons-changed", data_1.arg1()->GetString());
  VerifyIconList(data_1.arg2(),
                 /*selected_index=*/entry()->GetAvatarIconIndex(),
                 /*gaia_included=*/false,
                 /*gaia_selected=*/false);
}

}  // namespace settings
