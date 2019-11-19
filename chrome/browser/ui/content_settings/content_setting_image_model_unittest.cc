// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/content_setting_image_model.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/permissions/mock_permission_request.h"
#include "chrome/browser/permissions/permission_features.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/permission_bubble/mock_permission_prompt_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/cookies/cookie_options.h"
#include "services/device/public/cpp/device_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"

namespace {

class ContentSettingImageModelTest : public BrowserWithTestWindowTest {
 public:
  ContentSettingImageModelTest()
      : request_("test1",
                 PermissionRequestType::PERMISSION_NOTIFICATIONS,
                 PermissionRequestGestureType::GESTURE) {}
  ~ContentSettingImageModelTest() override {}

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("http://www.google.com"));
    controller_ = &web_contents()->GetController();
    NavigateAndCommit(controller_, GURL("http://www.google.com"));
    PermissionRequestManager::CreateForWebContents(web_contents());
    manager_ = PermissionRequestManager::FromWebContents(web_contents());
  }

  void WaitForBubbleToBeShown() {
    manager_->DocumentOnLoadCompletedInMainFrame();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  MockPermissionRequest request_;
  PermissionRequestManager* manager_ = nullptr;
  content::NavigationController* controller_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingImageModelTest);
};

bool HasIcon(const ContentSettingImageModel& model) {
  return !model.GetIcon(gfx::kPlaceholderColor).IsEmpty();
}

TEST_F(ContentSettingImageModelTest, Update) {
  TabSpecificContentSettings::CreateForWebContents(web_contents());
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::IMAGES);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_TRUE(content_setting_image_model->get_tooltip().empty());

  content_settings->OnContentBlocked(ContentSettingsType::IMAGES);
  content_setting_image_model->Update(web_contents());

  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_TRUE(HasIcon(*content_setting_image_model));
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
}

TEST_F(ContentSettingImageModelTest, RPHUpdate) {
  TabSpecificContentSettings::CreateForWebContents(web_contents());
  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::PROTOCOL_HANDLERS);
  content_setting_image_model->Update(web_contents());
  EXPECT_FALSE(content_setting_image_model->is_visible());

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  content_settings->set_pending_protocol_handler(
      ProtocolHandler::CreateProtocolHandler(
          "mailto", GURL("http://www.google.com/")));
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
}

TEST_F(ContentSettingImageModelTest, CookieAccessed) {
  TabSpecificContentSettings::CreateForWebContents(web_contents());
  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                 CONTENT_SETTING_BLOCK);
  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::COOKIES);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_TRUE(content_setting_image_model->get_tooltip().empty());

  GURL origin("http://google.com");
  std::unique_ptr<net::CanonicalCookie> cookie(net::CanonicalCookie::Create(
      origin, "A=B", base::Time::Now(), base::nullopt /* server_time */));
  ASSERT_TRUE(cookie);
  web_contents()->OnCookieChange(origin, origin, *cookie, false);
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_TRUE(HasIcon(*content_setting_image_model));
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
}

TEST_F(ContentSettingImageModelTest, SensorAccessed) {
  // Enable all sensors just to avoid hardcoding the expected messages to the
  // motion sensor-specific ones.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGenericSensorExtraClasses);

  TabSpecificContentSettings::CreateForWebContents(web_contents());
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());

  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::SENSORS);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_TRUE(content_setting_image_model->get_tooltip().empty());

  // Allowing by default means sensor access will not cause the indicator to be
  // shown.
  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                 CONTENT_SETTING_ALLOW);
  content_settings->OnContentAllowed(ContentSettingsType::SENSORS);
  content_setting_image_model->Update(web_contents());
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_TRUE(content_setting_image_model->get_tooltip().empty());

  content_settings->ClearContentSettingsExceptForNavigationRelatedSettings();

  // Allowing by default but blocking (e.g. due to a feature policy) causes the
  // indicator to be shown.
  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                 CONTENT_SETTING_ALLOW);
  content_settings->OnContentBlocked(ContentSettingsType::SENSORS);
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_TRUE(HasIcon(*content_setting_image_model));
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
  EXPECT_EQ(content_setting_image_model->get_tooltip(),
            l10n_util::GetStringUTF16(IDS_SENSORS_BLOCKED_TOOLTIP));

  content_settings->ClearContentSettingsExceptForNavigationRelatedSettings();

  // Blocking by default but allowing (e.g. via a site-specific exception)
  // causes the indicator to be shown.
  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                 CONTENT_SETTING_BLOCK);
  content_settings->OnContentAllowed(ContentSettingsType::SENSORS);
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_TRUE(HasIcon(*content_setting_image_model));
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
  EXPECT_EQ(content_setting_image_model->get_tooltip(),
            l10n_util::GetStringUTF16(IDS_SENSORS_ALLOWED_TOOLTIP));

  content_settings->ClearContentSettingsExceptForNavigationRelatedSettings();

  // Blocking access by default also causes the indicator to be shown so users
  // can set an exception.
  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                 CONTENT_SETTING_BLOCK);
  content_settings->OnContentBlocked(ContentSettingsType::SENSORS);
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_TRUE(HasIcon(*content_setting_image_model));
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
  EXPECT_EQ(content_setting_image_model->get_tooltip(),
            l10n_util::GetStringUTF16(IDS_SENSORS_BLOCKED_TOOLTIP));
}

// Regression test for https://crbug.com/955408
// See also: ContentSettingBubbleModelTest.SensorAccessPermissionsChanged
TEST_F(ContentSettingImageModelTest, SensorAccessPermissionsChanged) {
  // Enable all sensors just to avoid hardcoding the expected messages to the
  // motion sensor-specific ones.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGenericSensorExtraClasses);

  TabSpecificContentSettings::CreateForWebContents(web_contents());
  NavigateAndCommit(controller_, GURL("https://www.example.com"));
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::SENSORS);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_TRUE(content_setting_image_model->get_tooltip().empty());

  // Go from allow by default to block by default to allow by default.
  {
    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_ALLOW);
    content_settings->OnContentAllowed(ContentSettingsType::SENSORS);
    content_setting_image_model->Update(web_contents());
    EXPECT_FALSE(content_setting_image_model->is_visible());
    EXPECT_TRUE(content_setting_image_model->get_tooltip().empty());

    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_BLOCK);
    content_settings->OnContentBlocked(ContentSettingsType::SENSORS);
    content_setting_image_model->Update(web_contents());
    EXPECT_TRUE(content_setting_image_model->is_visible());
    EXPECT_TRUE(HasIcon(*content_setting_image_model));
    EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
    EXPECT_EQ(content_setting_image_model->get_tooltip(),
              l10n_util::GetStringUTF16(IDS_SENSORS_BLOCKED_TOOLTIP));

    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_ALLOW);
    content_settings->OnContentAllowed(ContentSettingsType::SENSORS);
    content_setting_image_model->Update(web_contents());
    // The icon and toolip remain set to the values above, but it is not a
    // problem since the image model is not visible.
    EXPECT_FALSE(content_setting_image_model->is_visible());
  }

  content_settings->ClearContentSettingsExceptForNavigationRelatedSettings();

  // Go from block by default to allow by default to block by default.
  {
    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_BLOCK);
    content_settings->OnContentBlocked(ContentSettingsType::SENSORS);
    content_setting_image_model->Update(web_contents());
    EXPECT_TRUE(content_setting_image_model->is_visible());
    EXPECT_TRUE(HasIcon(*content_setting_image_model));
    EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
    EXPECT_EQ(content_setting_image_model->get_tooltip(),
              l10n_util::GetStringUTF16(IDS_SENSORS_BLOCKED_TOOLTIP));
    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_ALLOW);

    content_settings->OnContentAllowed(ContentSettingsType::SENSORS);
    content_setting_image_model->Update(web_contents());
    // The icon and toolip remain set to the values above, but it is not a
    // problem since the image model is not visible.
    EXPECT_FALSE(content_setting_image_model->is_visible());

    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_BLOCK);
    content_settings->OnContentBlocked(ContentSettingsType::SENSORS);
    content_setting_image_model->Update(web_contents());
    EXPECT_TRUE(content_setting_image_model->is_visible());
    EXPECT_TRUE(HasIcon(*content_setting_image_model));
    EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
    EXPECT_EQ(content_setting_image_model->get_tooltip(),
              l10n_util::GetStringUTF16(IDS_SENSORS_BLOCKED_TOOLTIP));
  }

  content_settings->ClearContentSettingsExceptForNavigationRelatedSettings();

  // Block by default but allow a specific site.
  {
    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_BLOCK);
    settings_map->SetContentSettingDefaultScope(
        web_contents()->GetURL(), web_contents()->GetURL(),
        ContentSettingsType::SENSORS, std::string(), CONTENT_SETTING_ALLOW);
    content_settings->OnContentAllowed(ContentSettingsType::SENSORS);
    content_setting_image_model->Update(web_contents());

    EXPECT_TRUE(content_setting_image_model->is_visible());
    EXPECT_TRUE(HasIcon(*content_setting_image_model));
    EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
    EXPECT_EQ(content_setting_image_model->get_tooltip(),
              l10n_util::GetStringUTF16(IDS_SENSORS_ALLOWED_TOOLTIP));
  }

  content_settings->ClearContentSettingsExceptForNavigationRelatedSettings();
  // Clear site-specific exceptions.
  settings_map->ClearSettingsForOneType(ContentSettingsType::SENSORS);

  // Allow by default but allow a specific site.
  {
    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_ALLOW);
    settings_map->SetContentSettingDefaultScope(
        web_contents()->GetURL(), web_contents()->GetURL(),
        ContentSettingsType::SENSORS, std::string(), CONTENT_SETTING_BLOCK);
    content_settings->OnContentBlocked(ContentSettingsType::SENSORS);
    content_setting_image_model->Update(web_contents());

    EXPECT_TRUE(content_setting_image_model->is_visible());
    EXPECT_TRUE(HasIcon(*content_setting_image_model));
    EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
    EXPECT_EQ(content_setting_image_model->get_tooltip(),
              l10n_util::GetStringUTF16(IDS_SENSORS_BLOCKED_TOOLTIP));
  }
}

// Regression test for http://crbug.com/161854.
TEST_F(ContentSettingImageModelTest, NULLTabSpecificContentSettings) {
  web_contents()->RemoveUserData(TabSpecificContentSettings::UserDataKey());
  EXPECT_EQ(nullptr,
            TabSpecificContentSettings::FromWebContents(web_contents()));
  // Should not crash.
  ContentSettingImageModel::CreateForContentType(
      ContentSettingImageModel::ImageType::IMAGES)
      ->Update(web_contents());
}

TEST_F(ContentSettingImageModelTest, SubresourceFilter) {
  TabSpecificContentSettings::CreateForWebContents(web_contents());
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::ADS);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_TRUE(content_setting_image_model->get_tooltip().empty());

  content_settings->OnContentBlocked(ContentSettingsType::ADS);
  content_setting_image_model->Update(web_contents());

  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_TRUE(HasIcon(*content_setting_image_model));
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
}

TEST_F(ContentSettingImageModelTest, NotificationsIconVisibility) {
  TabSpecificContentSettings::CreateForWebContents(web_contents());
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::NOTIFICATIONS_QUIET_PROMPT);

  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS,
                                 CONTENT_SETTING_ALLOW);
  content_settings->OnContentAllowed(ContentSettingsType::NOTIFICATIONS);
  content_setting_image_model->Update(web_contents());
  EXPECT_FALSE(content_setting_image_model->is_visible());
  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS,
                                 CONTENT_SETTING_BLOCK);
  content_settings->OnContentBlocked(ContentSettingsType::NOTIFICATIONS);
  content_setting_image_model->Update(web_contents());
  EXPECT_FALSE(content_setting_image_model->is_visible());
}

TEST_F(ContentSettingImageModelTest, NotificationsPrompt) {
#if !defined(OS_ANDROID)
  std::map<std::string, std::string> parameters;
  parameters[kQuietNotificationPromptsUIFlavorParameterName] =
      kQuietNotificationPromptsAnimatedIcon;
  parameters[kQuietNotificationPromptsActivationParameterName] =
      kQuietNotificationPromptsActivationAlways;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kQuietNotificationPrompts, parameters}},
      {features::kBlockRepeatedNotificationPermissionPrompts});

  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::NOTIFICATIONS_QUIET_PROMPT);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  manager_->AddRequest(&request_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldShowQuietPermissionPrompt());
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  manager_->Accept();
  EXPECT_FALSE(manager_->ShouldShowQuietPermissionPrompt());
  content_setting_image_model->Update(web_contents());
  EXPECT_FALSE(content_setting_image_model->is_visible());
#endif  // !defined(OS_ANDROID)
}

}  // namespace
