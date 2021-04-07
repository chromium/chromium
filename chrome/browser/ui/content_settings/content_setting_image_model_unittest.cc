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
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_browser_process_platform_part.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/permissions/features.h"
#include "components/permissions/notification_permission_ui_selector.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/cookies/cookie_options.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_mac.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"
#include "services/device/public/cpp/test/fake_geolocation_system_permission.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"

using content_settings::PageSpecificContentSettings;
using device::LocationSystemPermissionStatus;

namespace {

class TestQuietNotificationPermissionUiSelector
    : public permissions::NotificationPermissionUiSelector {
 public:
  explicit TestQuietNotificationPermissionUiSelector(
      QuietUiReason simulated_reason_for_quiet_ui)
      : simulated_reason_for_quiet_ui_(simulated_reason_for_quiet_ui) {}
  ~TestQuietNotificationPermissionUiSelector() override = default;

 protected:
  // permissions::NotificationPermissionUiSelector:
  void SelectUiToUse(permissions::PermissionRequest* request,
                     DecisionMadeCallback callback) override {
    std::move(callback).Run(
        Decision(simulated_reason_for_quiet_ui_, base::nullopt));
  }

 private:
  QuietUiReason simulated_reason_for_quiet_ui_;

  DISALLOW_COPY_AND_ASSIGN(TestQuietNotificationPermissionUiSelector);
};

class ContentSettingImageModelTest : public BrowserWithTestWindowTest {
 public:
  ContentSettingImageModelTest()
      : request_("test1",
                 permissions::RequestType::kNotifications,
                 permissions::PermissionRequestGestureType::GESTURE) {}
  ~ContentSettingImageModelTest() override {}

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("http://www.google.com"));
    controller_ = &web_contents()->GetController();
    NavigateAndCommit(controller_, GURL("http://www.google.com"));
    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
    manager_ =
        permissions::PermissionRequestManager::FromWebContents(web_contents());
  }

  void WaitForBubbleToBeShown() {
    manager_->DocumentOnLoadCompletedInMainFrame(
        web_contents()->GetMainFrame());
    base::RunLoop().RunUntilIdle();
  }

 protected:
  permissions::MockPermissionRequest request_;
  permissions::PermissionRequestManager* manager_ = nullptr;
  content::NavigationController* controller_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingImageModelTest);
};

bool HasIcon(const ContentSettingImageModel& model) {
  return !model.GetIcon(gfx::kPlaceholderColor).IsEmpty();
}

TEST_F(ContentSettingImageModelTest, Update) {
  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
          web_contents()));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());
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
  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
          web_contents()));
  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::PROTOCOL_HANDLERS);
  content_setting_image_model->Update(web_contents());
  EXPECT_FALSE(content_setting_image_model->is_visible());

  chrome::PageSpecificContentSettingsDelegate::FromWebContents(web_contents())
      ->set_pending_protocol_handler(ProtocolHandler::CreateProtocolHandler(
          "mailto", GURL("http://www.google.com/")));
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
}

TEST_F(ContentSettingImageModelTest, CookieAccessed) {
  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
          web_contents()));
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
  PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame())
      ->OnCookiesAccessed({content::CookieAccessDetails::Type::kChange,
                           origin,
                           origin,
                           {*cookie},
                           false});
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

  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
          web_contents()));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());

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

  NavigateAndCommit(controller_, GURL("http://www.google.com"));
  content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());

  // Allowing by default but blocking (e.g. due to a permissions policy) causes
  // the indicator to be shown.
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

  NavigateAndCommit(controller_, GURL("http://www.google.com"));
  content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());

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

  NavigateAndCommit(controller_, GURL("http://www.google.com"));
  content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());

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

#if defined(OS_MAC)
// Test the correct ContentSettingImageModel for various permutations of site
// and system level Geolocation permissions
TEST_F(ContentSettingImageModelTest, GeolocationAccessPermissionsChanged) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kMacCoreLocationImplementation);
  auto test_location_permission_manager =
      std::make_unique<FakeSystemGeolocationPermissionsManager>();
  FakeSystemGeolocationPermissionsManager* location_permission_manager =
      test_location_permission_manager.get();
  TestingBrowserProcess::GetGlobal()
      ->GetTestPlatformPart()
      ->SetLocationPermissionManager(
          std::move(test_location_permission_manager));

  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
          web_contents()));
  GURL requesting_origin = GURL("https://www.example.com");
  NavigateAndCommit(controller_, requesting_origin);
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::GEOLOCATION);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_TRUE(content_setting_image_model->get_tooltip().empty());

  location_permission_manager->set_status(
      LocationSystemPermissionStatus::kAllowed);

  settings_map->SetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                         CONTENT_SETTING_ALLOW);
  content_settings->OnContentAllowed(ContentSettingsType::GEOLOCATION);
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
  EXPECT_EQ(content_setting_image_model->get_tooltip(),
            l10n_util::GetStringUTF16(IDS_ALLOWED_GEOLOCATION_MESSAGE));
  EXPECT_EQ(content_setting_image_model->explanatory_string_id(), 0);

  settings_map->SetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                         CONTENT_SETTING_BLOCK);
  content_settings->OnContentBlocked(ContentSettingsType::GEOLOCATION);
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_TRUE(HasIcon(*content_setting_image_model));
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
  EXPECT_EQ(content_setting_image_model->get_tooltip(),
            l10n_util::GetStringUTF16(IDS_BLOCKED_GEOLOCATION_MESSAGE));
  EXPECT_EQ(content_setting_image_model->explanatory_string_id(), 0);

  location_permission_manager->set_status(
      LocationSystemPermissionStatus::kDenied);
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
  EXPECT_EQ(content_setting_image_model->get_tooltip(),
            l10n_util::GetStringUTF16(IDS_BLOCKED_GEOLOCATION_MESSAGE));
  EXPECT_EQ(content_setting_image_model->explanatory_string_id(), 0);

  content_settings->OnContentAllowed(ContentSettingsType::GEOLOCATION);
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
  EXPECT_EQ(content_setting_image_model->get_tooltip(),
            l10n_util::GetStringUTF16(IDS_BLOCKED_GEOLOCATION_MESSAGE));
  EXPECT_EQ(content_setting_image_model->explanatory_string_id(),
            IDS_GEOLOCATION_TURNED_OFF);
}

TEST_F(ContentSettingImageModelTest, GeolocationAccessPermissionsUndetermined) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kMacCoreLocationImplementation);
  auto test_location_permission_manager =
      std::make_unique<FakeSystemGeolocationPermissionsManager>();
  test_location_permission_manager->set_status(
      LocationSystemPermissionStatus::kNotDetermined);
  TestingBrowserProcess::GetGlobal()
      ->GetTestPlatformPart()
      ->SetLocationPermissionManager(
          std::move(test_location_permission_manager));

  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
          web_contents()));
  GURL requesting_origin = GURL("https://www.example.com");
  NavigateAndCommit(controller_, requesting_origin);
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::GEOLOCATION);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_TRUE(content_setting_image_model->get_tooltip().empty());

  // When OS level permission is not determined the UI should show as if it is
  // blocked. However, the explanatory string is not displayed since we aren't
  // completely sure yet.
  settings_map->SetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                         CONTENT_SETTING_ALLOW);
  content_settings->OnContentAllowed(ContentSettingsType::GEOLOCATION);
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
  EXPECT_EQ(content_setting_image_model->get_tooltip(),
            l10n_util::GetStringUTF16(IDS_BLOCKED_GEOLOCATION_MESSAGE));
  EXPECT_EQ(content_setting_image_model->explanatory_string_id(), 0);

  // When site permission is blocked it should not make any difference what the
  // OS level permission is.
  settings_map->SetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                         CONTENT_SETTING_BLOCK);
  content_settings->OnContentBlocked(ContentSettingsType::GEOLOCATION);
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_TRUE(HasIcon(*content_setting_image_model));
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
  EXPECT_EQ(content_setting_image_model->get_tooltip(),
            l10n_util::GetStringUTF16(IDS_BLOCKED_GEOLOCATION_MESSAGE));
  EXPECT_EQ(content_setting_image_model->explanatory_string_id(), 0);
}
#endif

// Regression test for https://crbug.com/955408
// See also: ContentSettingBubbleModelTest.SensorAccessPermissionsChanged
TEST_F(ContentSettingImageModelTest, SensorAccessPermissionsChanged) {
  // Enable all sensors just to avoid hardcoding the expected messages to the
  // motion sensor-specific ones.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGenericSensorExtraClasses);

  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
          web_contents()));
  NavigateAndCommit(controller_, GURL("https://www.example.com"));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());
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

  NavigateAndCommit(controller_, GURL("https://www.example.com"));
  content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());

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

  NavigateAndCommit(controller_, GURL("https://www.example.com"));
  content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());

  // Block by default but allow a specific site.
  {
    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_BLOCK);
    settings_map->SetContentSettingDefaultScope(
        web_contents()->GetURL(), web_contents()->GetURL(),
        ContentSettingsType::SENSORS, CONTENT_SETTING_ALLOW);
    content_settings->OnContentAllowed(ContentSettingsType::SENSORS);
    content_setting_image_model->Update(web_contents());

    EXPECT_TRUE(content_setting_image_model->is_visible());
    EXPECT_TRUE(HasIcon(*content_setting_image_model));
    EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
    EXPECT_EQ(content_setting_image_model->get_tooltip(),
              l10n_util::GetStringUTF16(IDS_SENSORS_ALLOWED_TOOLTIP));
  }

  NavigateAndCommit(controller_, GURL("https://www.example.com"));
  content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());
  // Clear site-specific exceptions.
  settings_map->ClearSettingsForOneType(ContentSettingsType::SENSORS);

  // Allow by default but allow a specific site.
  {
    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_ALLOW);
    settings_map->SetContentSettingDefaultScope(
        web_contents()->GetURL(), web_contents()->GetURL(),
        ContentSettingsType::SENSORS, CONTENT_SETTING_BLOCK);
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
TEST_F(ContentSettingImageModelTest, NULLPageSpecificContentSettings) {
  PageSpecificContentSettings::DeleteForWebContentsForTest(web_contents());
  EXPECT_EQ(nullptr, PageSpecificContentSettings::GetForFrame(
                         web_contents()->GetMainFrame()));
  // Should not crash.
  ContentSettingImageModel::CreateForContentType(
      ContentSettingImageModel::ImageType::IMAGES)
      ->Update(web_contents());
}

TEST_F(ContentSettingImageModelTest, SubresourceFilter) {
  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
          web_contents()));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());
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
  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
          web_contents()));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(web_contents()->GetMainFrame());
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

#if !defined(OS_ANDROID)
TEST_F(ContentSettingImageModelTest, NotificationsPrompt) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kQuietNotificationPrompts},
      {permissions::features::kBlockRepeatedNotificationPermissionPrompts});

  auto* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(prefs::kEnableQuietNotificationPermissionUi,
                                  true);

  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::NOTIFICATIONS_QUIET_PROMPT);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_NE(0, content_setting_image_model->explanatory_string_id());
  manager_->Accept();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  content_setting_image_model->Update(web_contents());
  EXPECT_FALSE(content_setting_image_model->is_visible());
}

TEST_F(ContentSettingImageModelTest, NotificationsPromptCrowdDeny) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kQuietNotificationPrompts);

  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::NOTIFICATIONS_QUIET_PROMPT);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  manager_->set_notification_permission_ui_selector_for_testing(
      std::make_unique<TestQuietNotificationPermissionUiSelector>(
          permissions::NotificationPermissionUiSelector::QuietUiReason::
              kTriggeredByCrowdDeny));
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_EQ(0, content_setting_image_model->explanatory_string_id());
  manager_->Accept();
}

TEST_F(ContentSettingImageModelTest, NotificationsPromptAbusive) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kQuietNotificationPrompts);

  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::NOTIFICATIONS_QUIET_PROMPT);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  manager_->set_notification_permission_ui_selector_for_testing(
      std::make_unique<TestQuietNotificationPermissionUiSelector>(
          permissions::NotificationPermissionUiSelector::QuietUiReason::
              kTriggeredDueToAbusiveRequests));
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_EQ(0, content_setting_image_model->explanatory_string_id());
  manager_->Accept();
}

TEST_F(ContentSettingImageModelTest, NotificationsContentAbusive) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kQuietNotificationPrompts);

  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::NOTIFICATIONS_QUIET_PROMPT);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  manager_->set_notification_permission_ui_selector_for_testing(
      std::make_unique<TestQuietNotificationPermissionUiSelector>(
          permissions::NotificationPermissionUiSelector::QuietUiReason::
              kTriggeredDueToAbusiveContent));
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_EQ(0, content_setting_image_model->explanatory_string_id());
  manager_->Accept();
}
#endif  // !defined(OS_ANDROID)

}  // namespace
