// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/personalization/personalization_section.h"

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ui/webui/ash/settings/os_settings_identifier.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui_data_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {

// Test for the device settings page.
class PersonalizationSectionTest : public testing::Test {
 public:
  PersonalizationSectionTest()
      : local_search_service_proxy_(
            std::make_unique<
                ash::local_search_service::LocalSearchServiceProxy>(
                /*for_testing=*/true)),
        search_tag_registry_(local_search_service_proxy_.get()) {}
  ~PersonalizationSectionTest() override = default;

 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        ash::features::kOsSettingsRevampWayfinding);

    ASSERT_TRUE(test_profile_manager_.SetUp());
    user_manager_ = std::make_unique<FakeChromeUserManager>();
    user_manager_->Initialize();
    task_environment_.RunUntilIdle();

    profile_ =
        test_profile_manager_.CreateTestingProfile("test-user@example.com");
    section_ = std::make_unique<PersonalizationSection>(
        profile_, &search_tag_registry_, &pref_service_);
  }

  void TearDown() override {
    section_.reset();
    profile_ = nullptr;
    user_manager_->Shutdown();
    user_manager_->Destroy();
    user_manager_.reset();
    test_profile_manager_.DeleteAllTestingProfiles();
  }

  void LoginUser() {
    const AccountId account_id(
        AccountId::FromUserEmail(profile_->GetProfileUserName()));
    user_manager_->AddUser(account_id);
    user_manager_->LoginUser(account_id);
    user_manager_->SwitchActiveUser(account_id);
    task_environment_.RunUntilIdle();
  }

  void LoginGuestUser() {
    user_manager::User* guest_user = user_manager_->AddGuestUser();
    const AccountId account_id = guest_user->GetAccountId();
    test_profile_manager_.CreateTestingProfile(account_id.GetUserEmail());
    user_manager_->LoginUser(account_id);
    user_manager_->SwitchActiveUser(account_id);
    task_environment_.RunUntilIdle();
  }

  std::unique_ptr<PersonalizationSection> section_;

 private:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ash::local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_;
  ash::settings::SearchTagRegistry search_tag_registry_;
  TestingPrefServiceSimple pref_service_;
  raw_ptr<Profile> profile_;
  std::unique_ptr<FakeChromeUserManager> user_manager_;
  TestingProfileManager test_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
};

// Verify menu item description string in load time data.
TEST_F(PersonalizationSectionTest, MenuItemDescriptionString) {
  LoginUser();
  std::unique_ptr<content::TestWebUIDataSource> html_source =
      content::TestWebUIDataSource::Create(chrome::kChromeUIOSSettingsHost);
  section_->AddLoadTimeData(html_source->GetWebUIDataSource());

  EXPECT_EQ(std::string("Dark theme, screen saver"),
            *html_source->GetLocalizedStrings()->FindString(
                "personalizationMenuItemDescription"));
}

// Verify menu item description string in guest mode does not include "screen
// saver", in load time data.
TEST_F(PersonalizationSectionTest, MenuItemDescriptionStringGuestMode) {
  LoginGuestUser();
  std::unique_ptr<content::TestWebUIDataSource> html_source =
      content::TestWebUIDataSource::Create(chrome::kChromeUIOSSettingsHost);
  section_->AddLoadTimeData(html_source->GetWebUIDataSource());

  EXPECT_EQ(std::string("Dark theme"),
            *html_source->GetLocalizedStrings()->FindString(
                "personalizationMenuItemDescription"));
}

// Verify row description string in load time data.
TEST_F(PersonalizationSectionTest, RowDescriptionString) {
  LoginUser();
  std::unique_ptr<content::TestWebUIDataSource> html_source =
      content::TestWebUIDataSource::Create(chrome::kChromeUIOSSettingsHost);
  section_->AddLoadTimeData(html_source->GetWebUIDataSource());

  EXPECT_EQ(
      std::string("Personalize wallpaper, screen saver, dark theme, and more"),
      *html_source->GetLocalizedStrings()->FindString(
          "personalizationHubSubtitle"));
}

// Verify row description string in guest mode does not include "screen saver",
// in load time data.
TEST_F(PersonalizationSectionTest, RowDescriptionStringGuestMode) {
  LoginGuestUser();
  std::unique_ptr<content::TestWebUIDataSource> html_source =
      content::TestWebUIDataSource::Create(chrome::kChromeUIOSSettingsHost);
  section_->AddLoadTimeData(html_source->GetWebUIDataSource());

  EXPECT_EQ(std::string("Personalize wallpaper, dark theme, and more"),
            *html_source->GetLocalizedStrings()->FindString(
                "personalizationHubSubtitle"));
}

}  // namespace ash::settings
