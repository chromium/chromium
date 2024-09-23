// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/content_setting_image_model.h"

#include <optional>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
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
#include "components/content_settings/core/browser/cookie_settings.h"
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
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/cookies/cookie_options.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/geolocation/buildflags.h"
#include "testing/gmock/include/gmock/gmock-actions.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
#include "chrome/browser/permissions/system/mock_platform_handle.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)

using content_settings::PageSpecificContentSettings;
using testing::_;
using testing::Return;

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
        Decision(simulated_reason_for_quiet_ui_, std::nullopt));
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
  // Some dependencies of this test execute code on the UI thread, while other
  // subsystems that happen to be indirectly triggered expect the IO thread to
  // exist. Passing REAL_IO_THREAD will make sure both threads are available.
  ContentSettingImageModelTest()
      : BrowserWithTestWindowTest(
            content::BrowserTaskEnvironment::REAL_IO_THREAD),
        request_(permissions::RequestType::kNotifications,
                 permissions::PermissionRequestGestureType::GESTURE) {
    scoped_feature_list_.InitWithFeatures(
        {features::kQuietNotificationPrompts,
#if BUILDFLAG(IS_MAC)
         features::kAppShimNotificationAttribution,
#endif
         // Enable all sensors just to avoid hardcoding the expected messages
         // to the motion sensor-specific ones.
         features::kGenericSensorExtraClasses},
        {permissions::features::kBlockRepeatedNotificationPermissionPrompts});
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
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));
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
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));
  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::PROTOCOL_HANDLERS);
  content_setting_image_model->Update(web_contents());
  EXPECT_FALSE(content_setting_image_model->is_visible());

  PageSpecificContentSettingsDelegate::FromWebContents(web_contents())
      ->set_pending_protocol_handler(
          custom_handlers::ProtocolHandler::CreateProtocolHandler(
              "mailto", GURL("https://www.google.com/")));
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
}

TEST_F(ContentSettingImageModelTest, CookieAccessed) {
  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));
  auto* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile());
  content_settings->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                             CONTENT_SETTING_BLOCK);
  content_settings->SetContentSettingDefaultScope(
      web_contents()->GetLastCommittedURL(), GURL(),
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);
  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::COOKIES);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_TRUE(content_setting_image_model->get_tooltip().empty());

  GURL origin("http://google.com");
  std::unique_ptr<net::CanonicalCookie> cookie(
      net::CanonicalCookie::CreateForTesting(origin, "A=B", base::Time::Now()));
  ASSERT_TRUE(cookie);
  PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame())
      ->OnCookiesAccessed({content::CookieAccessDetails::Type::kChange,
                           origin,
                           origin,
                           {{*cookie}},
                           /* blocked_by_policy = */ false});
  UpdateModelAndVerifyStates(content_setting_image_model.get(),
                             /* is_visible = */ true,
                             /* tooltip_empty = */ false);
}

TEST_F(ContentSettingImageModelTest, ThirdPartyCookieAccessed) {
  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));
  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                 CONTENT_SETTING_ALLOW);
  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::COOKIES);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_TRUE(content_setting_image_model->get_tooltip().empty());

  GURL top_level_url("https://google.com");
  GURL third_party_url("https://example.com");
  std::unique_ptr<net::CanonicalCookie> cookie(
      net::CanonicalCookie::CreateForTesting(
          third_party_url, "A=B;SameSite=None;Secure", base::Time::Now()));
  ASSERT_TRUE(cookie);

  // A blocked third-party cookie access, should not cause the indicator to be
  // shown regardless of the CookieControlsMode.
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kOff));
  PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame())
      ->OnCookiesAccessed({content::CookieAccessDetails::Type::kChange,
                           third_party_url,
                           top_level_url,
                           {{*cookie}},
                           /* blocked_by_policy = */ true,
                           /* is_ad_tagged = */ false,
                           net::CookieSettingOverrides(),
                           net::SiteForCookies::FromUrl(top_level_url)});
  UpdateModelAndVerifyStates(content_setting_image_model.get(),
                             /* is_visible = */ false,
                             /* tooltip_empty = */ true);

  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));
  PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame())
      ->OnCookiesAccessed({content::CookieAccessDetails::Type::kChange,
                           third_party_url,
                           top_level_url,
                           {{*cookie}},
                           /* blocked_by_policy = */ true,
                           /* is_ad_tagged = */ false,
                           net::CookieSettingOverrides(),
                           net::SiteForCookies::FromUrl(top_level_url)});
  UpdateModelAndVerifyStates(content_setting_image_model.get(),
                             /* is_visible = */ false,
                             /* tooltip_empty = */ true);
}

TEST_F(ContentSettingImageModelTest, SensorAccessed) {
  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));
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

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
// Test the correct ContentSettingImageModel for various permutations of site
// and system level Geolocation permissions
TEST_F(ContentSettingImageModelTest, GeolocationAccessPermissionsChanged) {
#if BUILDFLAG(IS_WIN)
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kWinSystemLocationPermission}, {});
#endif  // BUILDFLAG(IS_WIN)
  system_permission_settings::MockPlatformHandle mock_platform_handle;
  system_permission_settings::SetInstanceForTesting(&mock_platform_handle);
  EXPECT_CALL(mock_platform_handle, IsAllowed(ContentSettingsType::GEOLOCATION))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_platform_handle, CanPrompt(ContentSettingsType::GEOLOCATION))
      .WillRepeatedly(Return(false));

  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));
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

  EXPECT_CALL(mock_platform_handle, IsAllowed(ContentSettingsType::GEOLOCATION))
      .WillRepeatedly(Return(true));

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

  EXPECT_CALL(mock_platform_handle, IsAllowed(ContentSettingsType::GEOLOCATION))
      .WillRepeatedly(Return(false));

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

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// This test verifies the UI behavior when OS-level geolocation permission is
// undetermined. This state is only applicable on macOS and Windows.
TEST_F(ContentSettingImageModelTest, GeolocationAccessPermissionsUndetermined) {
#if BUILDFLAG(IS_WIN)
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kWinSystemLocationPermission}, {});
#endif  // BUILDFLAG(IS_WIN)
  system_permission_settings::MockPlatformHandle mock_platform_handle;
  system_permission_settings::SetInstanceForTesting(&mock_platform_handle);
  EXPECT_CALL(mock_platform_handle, IsAllowed(ContentSettingsType::GEOLOCATION))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_platform_handle, CanPrompt(ContentSettingsType::GEOLOCATION))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_platform_handle,
              Request(ContentSettingsType::GEOLOCATION, _))
      .Times(1);

  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));
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
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)

// Regression test for https://crbug.com/955408
// See also: ContentSettingBubbleModelTest.SensorAccessPermissionsChanged
TEST_F(ContentSettingImageModelTest, SensorAccessPermissionsChanged) {
  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));
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
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));
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
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::NOTIFICATIONS);

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

#if BUILDFLAG(IS_MAC)
TEST_F(ContentSettingImageModelTest, NotificationsIconSystemPermission) {
  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::NOTIFICATIONS);

  const webapps::AppId app_id = web_app::test::InstallDummyWebApp(
      profile(), "Web App Title", GURL("http://www.google.com"));
  AppShimRegistry::Get()->OnAppInstalledForProfile(app_id,
                                                   profile()->GetPath());

  web_app::WebAppTabHelper::CreateForWebContents(web_contents());
  web_app::WebAppTabHelper::FromWebContents(web_contents())->SetAppId(app_id);

  // Installed app, but it hasn't interacted with notifications yet.
  content_setting_image_model->Update(web_contents());
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_FALSE(content_setting_image_model->should_auto_open_bubble());
  EXPECT_FALSE(content_setting_image_model->blocked_on_system_level());

  // Same, but the system level permission has previously been denied.
  AppShimRegistry::Get()->SaveNotificationPermissionStatusForApp(
      app_id, mac_notifications::mojom::PermissionStatus::kDenied);
  content_setting_image_model->Update(web_contents());
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_FALSE(content_setting_image_model->should_auto_open_bubble());
  EXPECT_FALSE(content_setting_image_model->blocked_on_system_level());

  // If notification permission is allowed at the chrome level, the indicator
  // should show.
  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS,
                                 CONTENT_SETTING_ALLOW);
  content_settings->OnContentAllowed(ContentSettingsType::NOTIFICATIONS);
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_TRUE(content_setting_image_model->is_blocked());
  EXPECT_FALSE(content_setting_image_model->should_auto_open_bubble());
  EXPECT_TRUE(content_setting_image_model->blocked_on_system_level());

  // Granting system permission should remove the indicator.
  AppShimRegistry::Get()->SaveNotificationPermissionStatusForApp(
      app_id, mac_notifications::mojom::PermissionStatus::kGranted);
  content_setting_image_model->Update(web_contents());
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_FALSE(content_setting_image_model->should_auto_open_bubble());
  EXPECT_FALSE(content_setting_image_model->blocked_on_system_level());
}

TEST_F(ContentSettingImageModelTest,
       NotificationsIconSystemPermission_PermissionRequested) {
  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

  PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::NOTIFICATIONS);

  const webapps::AppId app_id = web_app::test::InstallDummyWebApp(
      browser()->profile(), "Web App Title", GURL("http://www.google.com"));
  AppShimRegistry::Get()->OnAppInstalledForProfile(
      app_id, browser()->profile()->GetPath());

  web_app::WebAppTabHelper::CreateForWebContents(web_contents());
  web_app::WebAppTabHelper::FromWebContents(web_contents())->SetAppId(app_id);

  // If the app requests notification permission while the system permission was
  // denied, the notification should show and the bubble should auto open.
  AppShimRegistry::Get()->SaveNotificationPermissionStatusForApp(
      app_id, mac_notifications::mojom::PermissionStatus::kDenied);
  content_settings->SetNotificationsWasDeniedBecauseOfSystemPermission();
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_TRUE(content_setting_image_model->is_blocked());
  EXPECT_TRUE(content_setting_image_model->should_auto_open_bubble());
  EXPECT_TRUE(content_setting_image_model->blocked_on_system_level());
}
#endif

#if !BUILDFLAG(IS_ANDROID)
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
  EXPECT_EQ(content_setting_image_model->icon(),
            &vector_icons::kStorageAccessIcon);

  // Add a blocked permission.
  content_settings->OnTwoSitePermissionChanged(
      ContentSettingsType::STORAGE_ACCESS,
      net::SchemefulSite(GURL("https://foo.com")), CONTENT_SETTING_BLOCK);
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_EQ(content_setting_image_model->icon(),
            &vector_icons::kStorageAccessOffIcon);

  // Change permission to be allowed. E.g. through PageInfo.
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(
      GURL("https://foo.com"), web_contents()->GetURL(),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW);
  content_setting_image_model->Update(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_EQ(content_setting_image_model->icon(),
            &vector_icons::kStorageAccessIcon);

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
