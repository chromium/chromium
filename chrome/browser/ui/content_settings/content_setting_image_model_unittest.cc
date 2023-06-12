// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/content_setting_image_model.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
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
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_browser_process_platform_part.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_ui_selector.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/cookies/cookie_options.h"
#include "services/device/public/cpp/device_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"

#if BUILDFLAG(IS_MAC)
#include "services/device/public/cpp/geolocation/geolocation_manager.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"
#include "services/device/public/cpp/test/fake_geolocation_manager.h"
#endif

using content_settings::PageSpecificContentSettings;

namespace {

class TestQuietNotificationPermissionUiSelector
    : public permissions::PermissionUiSelector {
 public:
  explicit TestQuietNotificationPermissionUiSelector(
      QuietUiReason simulated_reason_for_quiet_ui)
      : simulated_reason_for_quiet_ui_(simulated_reason_for_quiet_ui) {}

  TestQuietNotificationPermissionUiSelector(
      const TestQuietNotificationPermissionUiSelector&) = delete;
  TestQuietNotificationPermissionUiSelector& operator=(
      const TestQuietNotificationPermissionUiSelector&) = delete;

  ~TestQuietNotificationPermissionUiSelector() override = default;

 protected:
  // permissions::PermissionUiSelector:
  void SelectUiToUse(permissions::PermissionRequest* request,
                     DecisionMadeCallback callback) override {
    std::move(callback).Run(
        Decision(simulated_reason_for_quiet_ui_, absl::nullopt));
  }

  bool IsPermissionRequestSupported(
      permissions::RequestType request_type) override {
    return request_type == permissions::RequestType::kNotifications;
  }

 private:
  QuietUiReason simulated_reason_for_quiet_ui_;
};

class ContentSettingImageModelTest : public BrowserWithTestWindowTest {
 public:
  ContentSettingImageModelTest()
      : request_(permissions::RequestType::kNotifications,
                 permissions::PermissionRequestGestureType::GESTURE) {
    scoped_feature_list_.InitWithFeatures(
        {features::kQuietNotificationPrompts,
         // Enable all sensors just to avoid hardcoding the expected messages
         // to the motion sensor-specific ones.
         features::kGenericSensorExtraClasses},
        {permissions::features::kBlockRepeatedNotificationPermissionPrompts,
         permissions::features::kPermissionQuietChip});
  }

  ContentSettingImageModelTest(const ContentSettingImageModelTest&) = delete;
  ContentSettingImageModelTest& operator=(const ContentSettingImageModelTest&) =
      delete;

  ~ContentSettingImageModelTest() override = default;

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("http://www.google.com"));
    controller_ = &web_contents()->GetController();
    NavigateAndCommit(web_contents(), GURL("http://www.google.com"));
    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
    manager_ =
        permissions::PermissionRequestManager::FromWebContents(web_contents());
  }

  void WaitForBubbleToBeShown() {
    manager_->DocumentOnLoadCompletedInPrimaryMainFrame();
    base::RunLoop().RunUntilIdle();
  }

  void UpdateModelAndVerifyStates(ContentSettingImageModel* model,
                                  bool is_visible,
                                  bool tooltip_empty) {
    model->Update(web_contents());
    EXPECT_EQ(model->is_visible(), is_visible);
    EXPECT_EQ(model->get_tooltip().empty(), tooltip_empty);
    EXPECT_EQ(!model->GetIcon(gfx::kPlaceholderColor).IsEmpty(), is_visible);
  }

  void UpdateModelAndVerifyStates(ContentSettingImageModel* model,
                                  bool is_visible,
                                  bool tooltip_empty,
                                  int tooltip_id,
                                  int explanatory_string_id) {
    UpdateModelAndVerifyStates(model, is_visible, tooltip_empty);
    EXPECT_EQ(model->get_tooltip(), l10n_util::GetStringUTF16(tooltip_id));
    EXPECT_EQ(model->explanatory_string_id(), explanatory_string_id);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  permissions::MockPermissionRequest request_;
  raw_ptr<permissions::PermissionRequestManager, DanglingUntriaged> manager_ =
      nullptr;
  raw_ptr<content::NavigationController, DanglingUntriaged> controller_ =
      nullptr;
};

TEST_F(ContentSettingImageModelTest, Update) {
  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
          web_contents()));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::IMAGES);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_TRUE(content_setting_image_model->get_tooltip().empty());

  content_settings->OnContentBlocked(ContentSettingsType::IMAGES);
  UpdateModelAndVerifyStates(content_setting_image_model.get(),
                             /* is_visible = */ true,
                             /* tooltip_empty = */ false);
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
      ->set_pending_protocol_handler(
          custom_handlers::ProtocolHandler::CreateProtocolHandler(
              "mailto", GURL("https://www.google.com/")));
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
      origin, "A=B", base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */));
  ASSERT_TRUE(cookie);
  PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame())
      ->OnCookiesAccessed({content::CookieAccessDetails::Type::kChange,
                           origin,
                           origin,
                           {*cookie},
                           false});
  UpdateModelAndVerifyStates(content_setting_image_model.get(),
                             /* is_visible = */ true,
                             /* tooltip_empty = */ false);
}

TEST_F(ContentSettingImageModelTest, SensorAccessed) {
  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
          web_contents()));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());

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
  UpdateModelAndVerifyStates(content_setting_image_model.get(),
                             /* is_visible = */ false,
                             /* tooltip_empty = */ true);

  NavigateAndCommit(web_contents(), GURL("http://www.google.com"));
  content_settings = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());

  // Allowing by default but blocking (e.g. due to a permissions policy) causes
  // the indicator to be shown.
  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                 CONTENT_SETTING_ALLOW);
  content_settings->OnContentBlocked(ContentSettingsType::SENSORS);
  UpdateModelAndVerifyStates(
      content_setting_image_model.get(), /* is_visible = */ true,
      /* tooltip_empty = */ false, IDS_SENSORS_BLOCKED_TOOLTIP,
      /* explanatory_string_id = */ 0);

  NavigateAndCommit(web_contents(), GURL("http://www.google.com"));
  content_settings = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());

  // Blocking by default but allowing (e.g. via a site-specific exception)
  // causes the indicator to be shown.
  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                 CONTENT_SETTING_BLOCK);
  content_settings->OnContentAllowed(ContentSettingsType::SENSORS);
  UpdateModelAndVerifyStates(
      content_setting_image_model.get(), /* is_visible = */ true,
      /* tooltip_empty = */ false, IDS_SENSORS_ALLOWED_TOOLTIP,
      /* explanatory_string_id = */ 0);

  NavigateAndCommit(web_contents(), GURL("http://www.google.com"));
  content_settings = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());

  // Blocking access by default also causes the indicator to be shown so users
  // can set an exception.
  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                 CONTENT_SETTING_BLOCK);
  content_settings->OnContentBlocked(ContentSettingsType::SENSORS);
  UpdateModelAndVerifyStates(
      content_setting_image_model.get(), /* is_visible = */ true,
      /* tooltip_empty = */ false, IDS_SENSORS_BLOCKED_TOOLTIP,
      /* explanatory_string_id = */ 0);
}

#if BUILDFLAG(IS_MAC)
// Test the correct ContentSettingImageModel for various permutations of site
// and system level Geolocation permissions
TEST_F(ContentSettingImageModelTest, GeolocationAccessPermissionsChanged) {
  auto test_geolocation_manager =
      std::make_unique<device::FakeGeolocationManager>();
  device::FakeGeolocationManager* geolocation_manager =
      test_geolocation_manager.get();
  device::GeolocationManager::SetInstance(std::move(test_geolocation_manager));

  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
          web_contents()));
  GURL requesting_origin = GURL("https://www.example.com");
  NavigateAndCommit(web_contents(), requesting_origin);
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::GEOLOCATION);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_TRUE(content_setting_image_model->get_tooltip().empty());

  geolocation_manager->SetSystemPermission(
      device::LocationSystemPermissionStatus::kAllowed);

  settings_map->SetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                         CONTENT_SETTING_ALLOW);
  content_settings->OnContentAllowed(ContentSettingsType::GEOLOCATION);
  UpdateModelAndVerifyStates(
      content_setting_image_model.get(), /* is_visible = */ true,
      /* tooltip_empty = */ false, IDS_ALLOWED_GEOLOCATION_MESSAGE,
      /* explanatory_string_id = */ 0);

  settings_map->SetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                         CONTENT_SETTING_BLOCK);
  content_settings->OnContentBlocked(ContentSettingsType::GEOLOCATION);
  UpdateModelAndVerifyStates(
      content_setting_image_model.get(), /* is_visible = */ true,
      /* tooltip_empty = */ false, IDS_BLOCKED_GEOLOCATION_MESSAGE,
      /* explanatory_string_id = */ 0);

  geolocation_manager->SetSystemPermission(
      device::LocationSystemPermissionStatus::kDenied);
  UpdateModelAndVerifyStates(
      content_setting_image_model.get(), /* is_visible = */ true,
      /* tooltip_empty = */ false, IDS_BLOCKED_GEOLOCATION_MESSAGE,
      /* explanatory_string_id = */ 0);

  content_settings->OnContentAllowed(ContentSettingsType::GEOLOCATION);
  UpdateModelAndVerifyStates(
      content_setting_image_model.get(), /* is_visible = */ true,
      /* tooltip_empty = */ false, IDS_BLOCKED_GEOLOCATION_MESSAGE,
      IDS_GEOLOCATION_TURNED_OFF);
}

TEST_F(ContentSettingImageModelTest, GeolocationAccessPermissionsUndetermined) {
  auto test_geolocation_manager =
      std::make_unique<device::FakeGeolocationManager>();
  test_geolocation_manager->SetSystemPermission(
      device::LocationSystemPermissionStatus::kNotDetermined);
  device::GeolocationManager::SetInstance(std::move(test_geolocation_manager));

  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
          web_contents()));
  GURL requesting_origin = GURL("https://www.example.com");
  NavigateAndCommit(web_contents(), requesting_origin);
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
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
  UpdateModelAndVerifyStates(
      content_setting_image_model.get(), /* is_visible = */ true,
      /* tooltip_empty = */ false, IDS_BLOCKED_GEOLOCATION_MESSAGE, 0);

  // When site permission is blocked it should not make any difference what the
  // OS level permission is.
  settings_map->SetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                         CONTENT_SETTING_BLOCK);
  content_settings->OnContentBlocked(ContentSettingsType::GEOLOCATION);
  UpdateModelAndVerifyStates(
      content_setting_image_model.get(), /* is_visible = */ true,
      /* tooltip_empty = */ false, IDS_BLOCKED_GEOLOCATION_MESSAGE, 0);
}

TEST_F(ContentSettingImageModelTest, GeolocationAccessDeniedExperiment) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kLocationPermissionsExperiment}, {});
  auto test_geolocation_manager =
      std::make_unique<device::FakeGeolocationManager>();
  device::FakeGeolocationManager* geolocation_manager =
      test_geolocation_manager.get();
  device::GeolocationManager::SetInstance(std::move(test_geolocation_manager));

  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
          web_contents()));
  GURL requesting_origin = GURL("https://www.example.com");
  NavigateAndCommit(web_contents(), requesting_origin);
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());

  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::GEOLOCATION);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_TRUE(content_setting_image_model->get_tooltip().empty());

  geolocation_manager->SetSystemPermission(
      device::LocationSystemPermissionStatus::kDenied);
  content_settings->OnContentAllowed(ContentSettingsType::GEOLOCATION);

  auto* local_state = g_browser_process->local_state();

  // Verify the button is shown without a label the first three time permission
  // is denied by system preferences while allowed for chrome preferences/
  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(local_state->GetInteger(
                  prefs::kMacRestoreLocationPermissionsExperimentCount),
              i);
    UpdateModelAndVerifyStates(
        content_setting_image_model.get(), /* is_visible = */ true,
        /* tooltip_empty = */ false, IDS_BLOCKED_GEOLOCATION_MESSAGE, 0);
  }
  // Verify the button is shown with a label the fourth to eighth time
  // permission is denied by system preferences while allowed for chrome
  // preferences/
  for (int i = 3; i < 8; i++) {
    EXPECT_EQ(local_state->GetInteger(
                  prefs::kMacRestoreLocationPermissionsExperimentCount),
              i);
    UpdateModelAndVerifyStates(
        content_setting_image_model.get(), /* is_visible = */ true,
        /* tooltip_empty = */ false, IDS_BLOCKED_GEOLOCATION_MESSAGE,
        IDS_GEOLOCATION_TURNED_OFF);
  }
  // Verify we return to normal behavior after the eighth time permission is
  // denied by system preferences while allowed for chrome preferences/
  for (int i = 8; i < 10; i++) {
    EXPECT_EQ(local_state->GetInteger(
                  prefs::kMacRestoreLocationPermissionsExperimentCount),
              8);
    UpdateModelAndVerifyStates(
        content_setting_image_model.get(), /* is_visible = */ true,
        /* tooltip_empty = */ false, IDS_BLOCKED_GEOLOCATION_MESSAGE,
        IDS_GEOLOCATION_TURNED_OFF);
  }
}
#endif

// Regression test for https://crbug.com/955408
// See also: ContentSettingBubbleModelTest.SensorAccessPermissionsChanged
TEST_F(ContentSettingImageModelTest, SensorAccessPermissionsChanged) {
  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
          web_contents()));
  NavigateAndCommit(web_contents(), GURL("https://www.example.com"));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
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

    UpdateModelAndVerifyStates(content_setting_image_model.get(),
                               /* is_visible = */ false,
                               /* tooltip_empty = */ true);

    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_BLOCK);
    content_settings->OnContentBlocked(ContentSettingsType::SENSORS);
    UpdateModelAndVerifyStates(
        content_setting_image_model.get(), /* is_visible = */ true,
        /* tooltip_empty = */ false, IDS_SENSORS_BLOCKED_TOOLTIP, 0);

    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_ALLOW);
    content_settings->OnContentAllowed(ContentSettingsType::SENSORS);
    content_setting_image_model->Update(web_contents());
    // The icon and toolip remain set to the values above, but it is not a
    // problem since the image model is not visible.
    EXPECT_FALSE(content_setting_image_model->is_visible());
  }

  NavigateAndCommit(web_contents(), GURL("https://www.example.com"));
  content_settings = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());

  // Go from block by default to allow by default to block by default.
  {
    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_BLOCK);
    content_settings->OnContentBlocked(ContentSettingsType::SENSORS);
    UpdateModelAndVerifyStates(
        content_setting_image_model.get(), /* is_visible = */ true,
        /* tooltip_empty = */ false, IDS_SENSORS_BLOCKED_TOOLTIP, 0);
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
    UpdateModelAndVerifyStates(
        content_setting_image_model.get(), /* is_visible = */ true,
        /* tooltip_empty = */ false, IDS_SENSORS_BLOCKED_TOOLTIP, 0);
  }

  NavigateAndCommit(web_contents(), GURL("https://www.example.com"));
  content_settings = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());

  // Block by default but allow a specific site.
  {
    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_BLOCK);
    settings_map->SetContentSettingDefaultScope(
        web_contents()->GetLastCommittedURL(),
        web_contents()->GetLastCommittedURL(), ContentSettingsType::SENSORS,
        CONTENT_SETTING_ALLOW);
    content_settings->OnContentAllowed(ContentSettingsType::SENSORS);

    UpdateModelAndVerifyStates(
        content_setting_image_model.get(), /* is_visible = */ true,
        /* tooltip_empty = */ false, IDS_SENSORS_ALLOWED_TOOLTIP, 0);
  }

  NavigateAndCommit(web_contents(), GURL("https://www.example.com"));
  content_settings = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  // Clear site-specific exceptions.
  settings_map->ClearSettingsForOneType(ContentSettingsType::SENSORS);

  // Allow by default but allow a specific site.
  {
    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_ALLOW);
    settings_map->SetContentSettingDefaultScope(
        web_contents()->GetLastCommittedURL(),
        web_contents()->GetLastCommittedURL(), ContentSettingsType::SENSORS,
        CONTENT_SETTING_BLOCK);
    content_settings->OnContentBlocked(ContentSettingsType::SENSORS);

    UpdateModelAndVerifyStates(
        content_setting_image_model.get(), /* is_visible = */ true,
        /* tooltip_empty = */ false, IDS_SENSORS_BLOCKED_TOOLTIP, 0);
  }
}

// Regression test for http://crbug.com/161854.
TEST_F(ContentSettingImageModelTest, NULLPageSpecificContentSettings) {
  PageSpecificContentSettings::DeleteForWebContentsForTest(web_contents());
  EXPECT_EQ(nullptr, PageSpecificContentSettings::GetForFrame(
                         web_contents()->GetPrimaryMainFrame()));
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
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::ADS);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_TRUE(content_setting_image_model->get_tooltip().empty());

  content_settings->OnContentBlocked(ContentSettingsType::ADS);
  UpdateModelAndVerifyStates(content_setting_image_model.get(),
                             /* is_visible = */ true,
                             /* tooltip_empty = */ false);
}

TEST_F(ContentSettingImageModelTest, NotificationsIconVisibility) {
  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
          web_contents()));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
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

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ContentSettingImageModelTest, NotificationsPrompt) {
  auto* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(prefs::kEnableQuietNotificationPermissionUi,
                                  true);

  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::NOTIFICATIONS_QUIET_PROMPT);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_);
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
  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::NOTIFICATIONS_QUIET_PROMPT);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  manager_->set_permission_ui_selector_for_testing(
      std::make_unique<TestQuietNotificationPermissionUiSelector>(
          permissions::PermissionUiSelector::QuietUiReason::
              kTriggeredByCrowdDeny));
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_EQ(0, content_setting_image_model->explanatory_string_id());
  manager_->Accept();
}

TEST_F(ContentSettingImageModelTest, NotificationsPromptAbusive) {
  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::NOTIFICATIONS_QUIET_PROMPT);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  manager_->set_permission_ui_selector_for_testing(
      std::make_unique<TestQuietNotificationPermissionUiSelector>(
          permissions::PermissionUiSelector::QuietUiReason::
              kTriggeredDueToAbusiveRequests));
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_EQ(0, content_setting_image_model->explanatory_string_id());
  manager_->Accept();
}

TEST_F(ContentSettingImageModelTest, NotificationsContentAbusive) {
  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::NOTIFICATIONS_QUIET_PROMPT);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  manager_->set_permission_ui_selector_for_testing(
      std::make_unique<TestQuietNotificationPermissionUiSelector>(
          permissions::PermissionUiSelector::QuietUiReason::
              kTriggeredDueToAbusiveContent));
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_EQ(0, content_setting_image_model->explanatory_string_id());
  manager_->Accept();
}

TEST_F(ContentSettingImageModelTest, StorageAccess) {
  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::STORAGE_ACCESS);
  EXPECT_FALSE(content_setting_image_model->is_visible());

  auto* content_settings = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());

  // Add an allowed permission.
  content_settings->OnTwoSitePermissionChanged(
      ContentSettingsType::STORAGE_ACCESS,
      net::SchemefulSite(GURL("https://example.com")), CONTENT_SETTING_ALLOW);
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_EQ(content_setting_image_model->get_icon_badge(), &gfx::kNoneIcon);

  // Add a blocked permission.
  content_settings->OnTwoSitePermissionChanged(
      ContentSettingsType::STORAGE_ACCESS,
      net::SchemefulSite(GURL("https://foo.com")), CONTENT_SETTING_BLOCK);
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_EQ(content_setting_image_model->get_icon_badge(),
            &vector_icons::kBlockedBadgeIcon);

  // Change permission to be allowed. E.g. through PageInfo.
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(
      GURL("https://foo.com"), web_contents()->GetURL(),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW);
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_EQ(content_setting_image_model->get_icon_badge(), &gfx::kNoneIcon);

  // Reset permissions.
  map->SetContentSettingDefaultScope(
      GURL("https://foo.com"), web_contents()->GetURL(),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ASK);
  map->SetContentSettingDefaultScope(
      GURL("https://example.com"), web_contents()->GetURL(),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ASK);
  content_setting_image_model->Update(web_contents());
  EXPECT_FALSE(content_setting_image_model->is_visible());
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
