// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"

#include <stddef.h>

#include <memory>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/permissions/system/mock_platform_handle.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/blocked_content/blocked_window_params.h"
#include "chrome/browser/ui/blocked_content/chrome_popup_navigation_delegate.h"
#include "chrome/browser/ui/content_settings/fake_owner.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_browser_process_platform_part.h"
#include "chrome/test/base/testing_profile.h"
#include "components/blocked_content/popup_blocker.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/custom_handlers/test_protocol_handler_registry_delegate.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_recovery_success_rate_tracker.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/schemeful_site.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/geolocation/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/test/gmock_expected_support.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#endif  // !BUILDFLAG(IS_ANDROID)

using content::WebContentsTester;
using content_settings::PageSpecificContentSettings;
using custom_handlers::ProtocolHandler;
using testing::Pair;
using testing::Return;
using testing::UnorderedElementsAre;
using ContentSettingBubbleAction =
    ContentSettingBubbleModel::ContentSettingBubbleAction;

class ContentSettingBubbleModelTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    PageSpecificContentSettings::CreateForWebContents(
        web_contents(),
        std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));
    infobars::ContentInfoBarManager::CreateForWebContents(web_contents());

    permissions::PermissionRecoverySuccessRateTracker::CreateForWebContents(
        web_contents());
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory()}};
  }
};

TEST_F(ContentSettingBubbleModelTest, ImageRadios) {
  WebContentsTester::For(web_contents())->
      NavigateAndCommit(GURL("https://www.example.com"));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  content_settings->OnContentBlocked(ContentSettingsType::IMAGES);

  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          nullptr, web_contents(), ContentSettingsType::IMAGES));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_FALSE(bubble_content.title.empty());
  EXPECT_EQ(2U, bubble_content.radio_group.radio_items.size());
  EXPECT_EQ(0, bubble_content.radio_group.default_item);
  EXPECT_TRUE(bubble_content.custom_link.empty());
  EXPECT_FALSE(bubble_content.manage_text.empty());
}

void VerifyBubbleContent(
    ContentSetting site_setting,
    const ContentSettingBubbleModel::BubbleContent& bubble_content) {
  EXPECT_FALSE(bubble_content.title.empty());
  ASSERT_EQ(bubble_content.radio_group.radio_items.size(), 2u);
  if (site_setting == CONTENT_SETTING_BLOCK) {
    EXPECT_EQ(
        bubble_content.radio_group.radio_items[0],
        l10n_util::GetStringUTF16(IDS_BLOCKED_ON_DEVICE_SITE_DATA_UNBLOCK));
    EXPECT_EQ(
        bubble_content.radio_group.radio_items[1],
        l10n_util::GetStringUTF16(IDS_BLOCKED_ON_DEVICE_SITE_DATA_NO_ACTION));
    EXPECT_EQ(bubble_content.title,
              l10n_util::GetStringUTF16(IDS_BLOCKED_ON_DEVICE_SITE_DATA_TITLE));
  } else {
    EXPECT_EQ(
        bubble_content.radio_group.radio_items[0],
        l10n_util::GetStringUTF16(IDS_ALLOWED_ON_DEVICE_SITE_DATA_NO_ACTION));
    EXPECT_EQ(bubble_content.radio_group.radio_items[1],
              l10n_util::GetStringUTF16(IDS_ALLOWED_ON_DEVICE_SITE_DATA_BLOCK));
    EXPECT_EQ(
        bubble_content.title,
        l10n_util::GetStringUTF16(IDS_ACCESSED_ON_DEVICE_SITE_DATA_TITLE));
  }
  EXPECT_TRUE(bubble_content.custom_link.empty());
  EXPECT_FALSE(bubble_content.custom_link_enabled);
  EXPECT_FALSE(bubble_content.manage_text.empty());
}

TEST_F(ContentSettingBubbleModelTest,
       CookiesContentSettingReflectedWhenCookiesBlocked) {
  const std::string url = "https://www.example.com";
  auto cookie_settings = CookieSettingsFactory::GetForProfile(profile());
  cookie_settings->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  WebContentsTester::For(web_contents())->NavigateAndCommit(GURL(url));
  auto* content_settings = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());

  content_settings->OnContentBlocked(ContentSettingsType::COOKIES);
  VerifyBubbleContent(
      CONTENT_SETTING_BLOCK,
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          nullptr, web_contents(), ContentSettingsType::COOKIES)
          ->bubble_content());
}

class CookiesContentSettingBubbleModelTest
    : public ContentSettingBubbleModelTest,
      public testing::WithParamInterface<
          std::tuple<ContentSetting, ContentSetting>> {};

TEST_P(CookiesContentSettingBubbleModelTest,
       BubbleShowsSiteSettingWhenDifferentFromDefaultSetting) {
  const std::string url = "https://www.example.com";
  ContentSetting site_setting = std::get<1>(GetParam());
  auto cookie_settings = CookieSettingsFactory::GetForProfile(profile());
  cookie_settings->SetDefaultCookieSetting(std::get<0>(GetParam()));
  cookie_settings->SetCookieSetting(GURL(url), site_setting);
  WebContentsTester::For(web_contents())->NavigateAndCommit(GURL(url));
  auto* content_settings = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());

  // Even if cookies are blocked on the 1P site, it's still possible for
  // OnContentAllowed() to be called due to 3PC being allowed by the default
  // content setting. This should NOT change how we render the 1PC bubble.
  content_settings->OnContentAllowed(ContentSettingsType::COOKIES);
  VerifyBubbleContent(
      site_setting, ContentSettingBubbleModel::CreateContentSettingBubbleModel(
                        nullptr, web_contents(), ContentSettingsType::COOKIES)
                        ->bubble_content());

  // Even if cookies are allowed on the 1P site, it's still possible for
  // OnContentBlocked() to be called due to 3PC being blocked by the default
  // content setting. This should NOT change how we render the 1PC bubble.
  content_settings->OnContentBlocked(ContentSettingsType::COOKIES);
  VerifyBubbleContent(
      site_setting, ContentSettingBubbleModel::CreateContentSettingBubbleModel(
                        nullptr, web_contents(), ContentSettingsType::COOKIES)
                        ->bubble_content());
}

INSTANTIATE_TEST_SUITE_P(
    CookiesContentSettingBubbleModelTests,
    CookiesContentSettingBubbleModelTest,
    testing::Values(
        std::make_tuple(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK),
        std::make_tuple(CONTENT_SETTING_BLOCK, CONTENT_SETTING_ALLOW)));

TEST_F(ContentSettingBubbleModelTest, MediastreamMicAndCamera) {
  WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com"));
  // Required to break dependency on BrowserMainLoop.
  MediaCaptureDevicesDispatcher::GetInstance()->
      DisableDeviceEnumerationForTesting();

  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  std::string request_host = "google.com";
  GURL security_origin("http://" + request_host);
  PageSpecificContentSettings::MicrophoneCameraState microphone_camera_state{
      PageSpecificContentSettings::kMicrophoneAccessed,
      PageSpecificContentSettings::kCameraAccessed,
  };
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               microphone_camera_state);

  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      new ContentSettingMediaStreamBubbleModel(nullptr, web_contents()));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_EQ(bubble_content.title,
            l10n_util::GetStringUTF16(IDS_MICROPHONE_CAMERA_ALLOWED_TITLE));
  EXPECT_EQ(bubble_content.message,
            l10n_util::GetStringUTF16(IDS_MICROPHONE_CAMERA_ALLOWED));
  EXPECT_EQ(2U, bubble_content.radio_group.radio_items.size());
  EXPECT_EQ(bubble_content.radio_group.radio_items[0],
            l10n_util::GetStringFUTF16(
                IDS_ALLOWED_MEDIASTREAM_MIC_AND_CAMERA_NO_ACTION,
                url_formatter::FormatUrlForSecurityDisplay(security_origin)));
  EXPECT_EQ(
      bubble_content.radio_group.radio_items[1],
      l10n_util::GetStringUTF16(IDS_ALLOWED_MEDIASTREAM_MIC_AND_CAMERA_BLOCK));
  EXPECT_EQ(0, bubble_content.radio_group.default_item);
  EXPECT_TRUE(bubble_content.custom_link.empty());
  EXPECT_FALSE(bubble_content.custom_link_enabled);
  EXPECT_FALSE(bubble_content.manage_text.empty());
}

TEST_F(ContentSettingBubbleModelTest, BlockedMediastreamMicAndCamera) {
  // Required to break dependency on BrowserMainLoop.
  MediaCaptureDevicesDispatcher::GetInstance()->
      DisableDeviceEnumerationForTesting();

  WebContentsTester::For(web_contents())->
      NavigateAndCommit(GURL("https://www.example.com"));
  GURL url = web_contents()->GetLastCommittedURL();

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  ContentSetting setting = CONTENT_SETTING_BLOCK;
  host_content_settings_map->SetContentSettingDefaultScope(
      url, GURL(), ContentSettingsType::MEDIASTREAM_MIC, setting);
  host_content_settings_map->SetContentSettingDefaultScope(
      url, GURL(), ContentSettingsType::MEDIASTREAM_CAMERA, setting);

  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  PageSpecificContentSettings::MicrophoneCameraState microphone_camera_state{
      PageSpecificContentSettings::kMicrophoneAccessed,
      PageSpecificContentSettings::kCameraAccessed,
      PageSpecificContentSettings::kMicrophoneBlocked,
      PageSpecificContentSettings::kCameraBlocked,
  };
  content_settings->OnMediaStreamPermissionSet(url, microphone_camera_state);

  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      new ContentSettingMediaStreamBubbleModel(nullptr, web_contents()));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  // Test if the correct radio item is selected for the blocked mediastream
  // setting.
  EXPECT_EQ(1, bubble_content.radio_group.default_item);

  std::unique_ptr<FakeOwner> owner =
      FakeOwner::Create(*content_setting_bubble_model, 1);
  content_setting_bubble_model->CommitChanges();

  // Test that the media settings where not changed.
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::MEDIASTREAM_CAMERA));

  owner->SetSelectedRadioOptionAndCommit(0);

  // Test that the media setting were change correctly.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::MEDIASTREAM_CAMERA));
}

// Tests whether a changed setting in the setting bubble is displayed again when
// the bubble is re-opened.
TEST_F(ContentSettingBubbleModelTest, MediastreamContentBubble) {
  // Required to break dependency on BrowserMainLoop.
  MediaCaptureDevicesDispatcher::GetInstance()->
      DisableDeviceEnumerationForTesting();

  WebContentsTester::For(web_contents())->
      NavigateAndCommit(GURL("https://www.example.com"));
  GURL url = web_contents()->GetLastCommittedURL();

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  ContentSetting setting = CONTENT_SETTING_BLOCK;
  host_content_settings_map->SetContentSettingDefaultScope(
      url, GURL(), ContentSettingsType::MEDIASTREAM_MIC, setting);

  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  PageSpecificContentSettings::MicrophoneCameraState microphone_camera_state{
      PageSpecificContentSettings::kMicrophoneAccessed,
      PageSpecificContentSettings::kMicrophoneBlocked,
  };
  content_settings->OnMediaStreamPermissionSet(url, microphone_camera_state);
  {
    std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        new ContentSettingMediaStreamBubbleModel(nullptr, web_contents()));
    const ContentSettingBubbleModel::BubbleContent& bubble_content =
        content_setting_bubble_model->bubble_content();
    // Test if the correct radio item is selected for the blocked mediastream
    // setting.
    EXPECT_EQ(1, bubble_content.radio_group.default_item);

    std::unique_ptr<FakeOwner> owner =
        FakeOwner::Create(*content_setting_bubble_model, 1);
    // Change the radio setting.
    owner->SetSelectedRadioOptionAndCommit(0);
  }
  // Test that the setting was changed.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::MEDIASTREAM_MIC));

  {
    std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        new ContentSettingMediaStreamBubbleModel(nullptr, web_contents()));
    const ContentSettingBubbleModel::BubbleContent& bubble_content =
        content_setting_bubble_model->bubble_content();
    // Test that the reload hint is displayed.
    EXPECT_FALSE(bubble_content.custom_link_enabled);
    EXPECT_EQ(
        bubble_content.custom_link,
        l10n_util::GetStringUTF16(IDS_MEDIASTREAM_SETTING_CHANGED_MESSAGE));

    EXPECT_EQ(0, bubble_content.radio_group.default_item);

    std::unique_ptr<FakeOwner> owner =
        FakeOwner::Create(*content_setting_bubble_model, 0);
    // Restore the radio setting (to block).
    owner->SetSelectedRadioOptionAndCommit(1);
  }
  // Test that the media settings were changed again.
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::MEDIASTREAM_MIC));

  {
    std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        new ContentSettingMediaStreamBubbleModel(nullptr, web_contents()));
    const ContentSettingBubbleModel::BubbleContent& bubble_content =
        content_setting_bubble_model->bubble_content();
    // Test that the reload hint is not displayed any more.
    EXPECT_FALSE(bubble_content.custom_link_enabled);
    EXPECT_TRUE(bubble_content.custom_link.empty());

    EXPECT_EQ(1, bubble_content.radio_group.default_item);
  }
}

TEST_F(ContentSettingBubbleModelTest, MediastreamMic) {
  // Keep `kLeftHandSideActivityIndicators` disabled to test camera/mic content
  // setting bubble.
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      content_settings::features::kLeftHandSideActivityIndicators);

  WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com"));
  // Required to break dependency on BrowserMainLoop.
  MediaCaptureDevicesDispatcher::GetInstance()->
      DisableDeviceEnumerationForTesting();

  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  std::string request_host = "google.com";
  GURL security_origin("http://" + request_host);
  PageSpecificContentSettings::MicrophoneCameraState microphone_camera_state{
      PageSpecificContentSettings::kMicrophoneAccessed};
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               microphone_camera_state);

  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      new ContentSettingMediaStreamBubbleModel(nullptr, web_contents()));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_EQ(bubble_content.title,
            l10n_util::GetStringUTF16(IDS_MICROPHONE_ACCESSED_TITLE));
  EXPECT_EQ(bubble_content.message,
            l10n_util::GetStringUTF16(IDS_MICROPHONE_ACCESSED));
  EXPECT_EQ(2U, bubble_content.radio_group.radio_items.size());
  EXPECT_EQ(bubble_content.radio_group.radio_items[0],
            l10n_util::GetStringFUTF16(
                IDS_ALLOWED_MEDIASTREAM_MIC_NO_ACTION,
                url_formatter::FormatUrlForSecurityDisplay(security_origin)));
  EXPECT_EQ(bubble_content.radio_group.radio_items[1],
            l10n_util::GetStringUTF16(IDS_ALLOWED_MEDIASTREAM_MIC_BLOCK));
  EXPECT_EQ(0, bubble_content.radio_group.default_item);
  EXPECT_TRUE(bubble_content.custom_link.empty());
  EXPECT_FALSE(bubble_content.custom_link_enabled);
  EXPECT_FALSE(bubble_content.manage_text.empty());

  // Change the microphone access.
  microphone_camera_state.Put(PageSpecificContentSettings::kMicrophoneBlocked);
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               microphone_camera_state);
  content_setting_bubble_model =
      std::make_unique<ContentSettingMediaStreamBubbleModel>(nullptr,
                                                             web_contents());
  const ContentSettingBubbleModel::BubbleContent& new_bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_EQ(new_bubble_content.title,
            l10n_util::GetStringUTF16(IDS_MICROPHONE_BLOCKED_TITLE));
  EXPECT_EQ(new_bubble_content.message,
            l10n_util::GetStringUTF16(IDS_MICROPHONE_BLOCKED));
  EXPECT_EQ(2U, new_bubble_content.radio_group.radio_items.size());
  EXPECT_EQ(new_bubble_content.radio_group.radio_items[0],
            l10n_util::GetStringFUTF16(
                IDS_BLOCKED_MEDIASTREAM_MIC_ASK,
                url_formatter::FormatUrlForSecurityDisplay(security_origin)));
  EXPECT_EQ(new_bubble_content.radio_group.radio_items[1],
            l10n_util::GetStringUTF16(IDS_BLOCKED_MEDIASTREAM_MIC_NO_ACTION));
  EXPECT_EQ(1, new_bubble_content.radio_group.default_item);
  EXPECT_TRUE(new_bubble_content.custom_link.empty());
  EXPECT_FALSE(new_bubble_content.custom_link_enabled);
  EXPECT_FALSE(new_bubble_content.manage_text.empty());
}

TEST_F(ContentSettingBubbleModelTest, MediastreamCamera) {
  // Keep `kLeftHandSideActivityIndicators` disabled to test camera/mic content
  // setting bubble.
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      content_settings::features::kLeftHandSideActivityIndicators);

  WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com"));
  // Required to break dependency on BrowserMainLoop.
  MediaCaptureDevicesDispatcher::GetInstance()->
      DisableDeviceEnumerationForTesting();

  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  std::string request_host = "google.com";
  GURL security_origin("http://" + request_host);
  PageSpecificContentSettings::MicrophoneCameraState microphone_camera_state{
      PageSpecificContentSettings::kCameraAccessed};
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               microphone_camera_state);

  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      new ContentSettingMediaStreamBubbleModel(nullptr, web_contents()));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_EQ(bubble_content.title,
            l10n_util::GetStringUTF16(IDS_CAMERA_ACCESSED_TITLE));
  EXPECT_EQ(bubble_content.message,
            l10n_util::GetStringUTF16(IDS_CAMERA_ACCESSED));
  EXPECT_EQ(2U, bubble_content.radio_group.radio_items.size());
  EXPECT_EQ(bubble_content.radio_group.radio_items[0],
            l10n_util::GetStringFUTF16(
                IDS_ALLOWED_MEDIASTREAM_CAMERA_NO_ACTION,
                url_formatter::FormatUrlForSecurityDisplay(security_origin)));
  EXPECT_EQ(bubble_content.radio_group.radio_items[1],
            l10n_util::GetStringUTF16(IDS_ALLOWED_MEDIASTREAM_CAMERA_BLOCK));
  EXPECT_EQ(0, bubble_content.radio_group.default_item);
  EXPECT_TRUE(bubble_content.custom_link.empty());
  EXPECT_FALSE(bubble_content.custom_link_enabled);
  EXPECT_FALSE(bubble_content.manage_text.empty());

  // Change the camera access.
  microphone_camera_state.Put(PageSpecificContentSettings::kCameraBlocked);
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               microphone_camera_state);
  content_setting_bubble_model =
      std::make_unique<ContentSettingMediaStreamBubbleModel>(nullptr,
                                                             web_contents());
  const ContentSettingBubbleModel::BubbleContent& new_bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_EQ(new_bubble_content.title,
            l10n_util::GetStringUTF16(IDS_CAMERA_BLOCKED_TITLE));
  EXPECT_EQ(new_bubble_content.message,
            l10n_util::GetStringUTF16(IDS_CAMERA_BLOCKED));
  EXPECT_EQ(2U, new_bubble_content.radio_group.radio_items.size());
  EXPECT_EQ(new_bubble_content.radio_group.radio_items[0],
            l10n_util::GetStringFUTF16(
                IDS_BLOCKED_MEDIASTREAM_CAMERA_ASK,
                url_formatter::FormatUrlForSecurityDisplay(security_origin)));
  EXPECT_EQ(
      new_bubble_content.radio_group.radio_items[1],
      l10n_util::GetStringUTF16(IDS_BLOCKED_MEDIASTREAM_CAMERA_NO_ACTION));
  EXPECT_EQ(1, new_bubble_content.radio_group.default_item);
  EXPECT_TRUE(new_bubble_content.custom_link.empty());
  EXPECT_FALSE(new_bubble_content.custom_link_enabled);
  EXPECT_FALSE(new_bubble_content.manage_text.empty());
}

TEST_F(ContentSettingBubbleModelTest, AccumulateMediastreamMicAndCamera) {
  // Keep `kLeftHandSideActivityIndicators` disabled to test camera/mic content
  // setting bubble.
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      content_settings::features::kLeftHandSideActivityIndicators);

  WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com"));
  // Required to break dependency on BrowserMainLoop.
  MediaCaptureDevicesDispatcher::GetInstance()->
      DisableDeviceEnumerationForTesting();

  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  std::string request_host = "google.com";
  GURL security_origin("http://" + request_host);

  // Firstly, add microphone access.
  PageSpecificContentSettings::MicrophoneCameraState microphone_camera_state{
      PageSpecificContentSettings::kMicrophoneAccessed};
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               microphone_camera_state);

  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      new ContentSettingMediaStreamBubbleModel(nullptr, web_contents()));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_EQ(bubble_content.title,
            l10n_util::GetStringUTF16(IDS_MICROPHONE_ACCESSED_TITLE));
  EXPECT_EQ(bubble_content.message,
            l10n_util::GetStringUTF16(IDS_MICROPHONE_ACCESSED));
  EXPECT_EQ(2U, bubble_content.radio_group.radio_items.size());
  EXPECT_EQ(bubble_content.radio_group.radio_items[0],
            l10n_util::GetStringFUTF16(
                IDS_ALLOWED_MEDIASTREAM_MIC_NO_ACTION,
                url_formatter::FormatUrlForSecurityDisplay(security_origin)));
  EXPECT_EQ(bubble_content.radio_group.radio_items[1],
            l10n_util::GetStringUTF16(IDS_ALLOWED_MEDIASTREAM_MIC_BLOCK));
  EXPECT_EQ(0, bubble_content.radio_group.default_item);

  // Then add camera access.
  microphone_camera_state.Put(PageSpecificContentSettings::kCameraAccessed);
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               microphone_camera_state);

  content_setting_bubble_model =
      std::make_unique<ContentSettingMediaStreamBubbleModel>(nullptr,
                                                             web_contents());
  const ContentSettingBubbleModel::BubbleContent& new_bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_EQ(new_bubble_content.title,
            l10n_util::GetStringUTF16(IDS_MICROPHONE_CAMERA_ALLOWED_TITLE));
  EXPECT_EQ(new_bubble_content.message,
            l10n_util::GetStringUTF16(IDS_MICROPHONE_CAMERA_ALLOWED));
  EXPECT_EQ(2U, new_bubble_content.radio_group.radio_items.size());
  EXPECT_EQ(new_bubble_content.radio_group.radio_items[0],
            l10n_util::GetStringFUTF16(
                IDS_ALLOWED_MEDIASTREAM_MIC_AND_CAMERA_NO_ACTION,
                url_formatter::FormatUrlForSecurityDisplay(security_origin)));
  EXPECT_EQ(
      new_bubble_content.radio_group.radio_items[1],
      l10n_util::GetStringUTF16(IDS_ALLOWED_MEDIASTREAM_MIC_AND_CAMERA_BLOCK));
  EXPECT_EQ(0, new_bubble_content.radio_group.default_item);
}

// Enable geolocation bubble tests to be run with OS-level permission
// integration enabled or disabled on platforms where support is toggleable.
class ContentSettingGeolocationBubbleModelTest
    : public ContentSettingBubbleModelTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    ContentSettingBubbleModelTest::SetUp();
#if BUILDFLAG(IS_WIN)
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          {features::kWinSystemLocationPermission}, {});
    }
#endif  // BUILDFLAG(IS_WIN)
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(ContentSettingGeolocationBubbleModelTest, Geolocation) {
  system_permission_settings::MockPlatformHandle mock_platform_handle;
  system_permission_settings::SetInstanceForTesting(&mock_platform_handle);

  // This parameter is meaningful only on Windows, where geolocation permissions
  // are controlled by the 'features::kWinSystemLocationPermission' feature.
  // If the feature is disabled (GetParam() returns false), the location system
  // permission is expected to be always allowed.
  const bool is_os_level_geolocation_permission_support_enabled = GetParam();
  if (is_os_level_geolocation_permission_support_enabled) {
    EXPECT_CALL(mock_platform_handle,
                IsAllowed(ContentSettingsType::GEOLOCATION))
        .WillRepeatedly(Return(false));
  } else {
    EXPECT_CALL(mock_platform_handle,
                IsAllowed(ContentSettingsType::GEOLOCATION))
        .WillRepeatedly(Return(true));
  }

  WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com"));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  // Set geolocation to allow.
  settings_map->SetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                         CONTENT_SETTING_ALLOW);
  content_settings->OnContentAllowed(ContentSettingsType::GEOLOCATION);

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  // System-level geolocation permission is blocked.
  if (is_os_level_geolocation_permission_support_enabled) {
    auto content_setting_bubble_model =
        std::make_unique<ContentSettingGeolocationBubbleModel>(nullptr,
                                                               web_contents());
    std::unique_ptr<FakeOwner> owner =
        FakeOwner::Create(*content_setting_bubble_model, 0);
    const auto& bubble_content = content_setting_bubble_model->bubble_content();

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
    EXPECT_EQ(bubble_content.title,
              u"Location is turned off in system settings");
#endif
    EXPECT_TRUE(bubble_content.message.empty());
    EXPECT_EQ(bubble_content.radio_group.radio_items.size(), 0U);

    // This should be a no-op.
    content_setting_bubble_model->CommitChanges();
  }

  // System-level geolocation permission is blocked, but allowed while the
  // bubble is visible. The displayed message should not change.
  if (is_os_level_geolocation_permission_support_enabled) {
    auto content_setting_bubble_model =
        std::make_unique<ContentSettingGeolocationBubbleModel>(nullptr,
                                                               web_contents());
    std::unique_ptr<FakeOwner> owner =
        FakeOwner::Create(*content_setting_bubble_model, 0);
    const auto& bubble_content = content_setting_bubble_model->bubble_content();

    EXPECT_CALL(mock_platform_handle,
                IsAllowed(ContentSettingsType::GEOLOCATION))
        .WillRepeatedly(Return(true));

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
    EXPECT_EQ(bubble_content.title,
              u"Location is turned off in system settings");
#endif
    EXPECT_TRUE(bubble_content.message.empty());
    EXPECT_EQ(bubble_content.radio_group.radio_items.size(), 0U);

    // This should be a no-op.
    content_setting_bubble_model->CommitChanges();
  }
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)

  // Go from allow by default to block by default to allow by default.
  {
    std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        std::make_unique<ContentSettingGeolocationBubbleModel>(nullptr,
                                                               web_contents()));
    const auto& bubble_content = content_setting_bubble_model->bubble_content();

    EXPECT_EQ(bubble_content.title,
              l10n_util::GetStringUTF16(IDS_ALLOWED_GEOLOCATION_TITLE));
    EXPECT_EQ(bubble_content.message,
              l10n_util::GetStringUTF16(IDS_ALLOWED_GEOLOCATION_MESSAGE));
    ASSERT_EQ(bubble_content.radio_group.radio_items.size(), 2U);
    EXPECT_EQ(bubble_content.radio_group.radio_items[0],
              l10n_util::GetStringUTF16(IDS_ALLOWED_GEOLOCATION_NO_ACTION));
    EXPECT_EQ(
        bubble_content.radio_group.radio_items[1],
        l10n_util::GetStringFUTF16(IDS_ALLOWED_GEOLOCATION_BLOCK,
                                   url_formatter::FormatUrlForSecurityDisplay(
                                       web_contents()->GetLastCommittedURL())));
    EXPECT_EQ(bubble_content.radio_group.default_item, 0);

    settings_map->SetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                           CONTENT_SETTING_BLOCK);
    content_settings->OnContentBlocked(ContentSettingsType::GEOLOCATION);
    content_setting_bubble_model =
        std::make_unique<ContentSettingGeolocationBubbleModel>(nullptr,
                                                               web_contents());
    const auto& bubble_content_2 =
        content_setting_bubble_model->bubble_content();

    EXPECT_EQ(bubble_content_2.title,
              l10n_util::GetStringUTF16(IDS_BLOCKED_GEOLOCATION_TITLE));
    EXPECT_EQ(bubble_content_2.message,
              l10n_util::GetStringUTF16(IDS_BLOCKED_GEOLOCATION_MESSAGE));
    EXPECT_EQ(bubble_content_2.radio_group.radio_items.size(), 2U);
    EXPECT_EQ(
        bubble_content_2.radio_group.radio_items[0],
        l10n_util::GetStringFUTF16(IDS_BLOCKED_GEOLOCATION_UNBLOCK,
                                   url_formatter::FormatUrlForSecurityDisplay(
                                       web_contents()->GetLastCommittedURL())));
    EXPECT_EQ(bubble_content_2.radio_group.radio_items[1],
              l10n_util::GetStringUTF16(IDS_BLOCKED_GEOLOCATION_NO_ACTION));
    EXPECT_EQ(bubble_content_2.radio_group.default_item, 1);

    settings_map->SetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                           CONTENT_SETTING_ALLOW);
    content_settings->OnContentAllowed(ContentSettingsType::GEOLOCATION);
    content_setting_bubble_model =
        std::make_unique<ContentSettingGeolocationBubbleModel>(nullptr,
                                                               web_contents());
    const auto& bubble_content_3 =
        content_setting_bubble_model->bubble_content();
    EXPECT_EQ(bubble_content_3.title,
              l10n_util::GetStringUTF16(IDS_ALLOWED_GEOLOCATION_TITLE));
    EXPECT_EQ(bubble_content_3.message,
              l10n_util::GetStringUTF16(IDS_ALLOWED_GEOLOCATION_MESSAGE));
    ASSERT_EQ(bubble_content_3.radio_group.radio_items.size(), 2U);
    EXPECT_EQ(bubble_content_3.radio_group.radio_items[0],
              l10n_util::GetStringUTF16(IDS_ALLOWED_GEOLOCATION_NO_ACTION));
    EXPECT_EQ(
        bubble_content_3.radio_group.radio_items[1],
        l10n_util::GetStringFUTF16(IDS_ALLOWED_GEOLOCATION_BLOCK,
                                   url_formatter::FormatUrlForSecurityDisplay(
                                       web_contents()->GetLastCommittedURL())));
    EXPECT_EQ(bubble_content_3.radio_group.default_item, 0);
  }

  WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com"));
  content_settings = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());

  // Go from block by default to allow by default to block by default.
  {
    settings_map->SetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                           CONTENT_SETTING_BLOCK);
    content_settings->OnContentBlocked(ContentSettingsType::GEOLOCATION);
    std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        std::make_unique<ContentSettingGeolocationBubbleModel>(nullptr,
                                                               web_contents()));

    const auto& bubble_content = content_setting_bubble_model->bubble_content();

    EXPECT_EQ(bubble_content.title,
              l10n_util::GetStringUTF16(IDS_BLOCKED_GEOLOCATION_TITLE));
    EXPECT_EQ(bubble_content.message,
              l10n_util::GetStringUTF16(IDS_BLOCKED_GEOLOCATION_MESSAGE));
    ASSERT_EQ(bubble_content.radio_group.radio_items.size(), 2U);
    EXPECT_EQ(
        bubble_content.radio_group.radio_items[0],
        l10n_util::GetStringFUTF16(IDS_BLOCKED_GEOLOCATION_UNBLOCK,
                                   url_formatter::FormatUrlForSecurityDisplay(
                                       web_contents()->GetLastCommittedURL())));
    EXPECT_EQ(bubble_content.radio_group.radio_items[1],
              l10n_util::GetStringUTF16(IDS_BLOCKED_GEOLOCATION_NO_ACTION));
    EXPECT_EQ(bubble_content.radio_group.default_item, 1);

    settings_map->SetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                           CONTENT_SETTING_ALLOW);
    content_settings->OnContentAllowed(ContentSettingsType::GEOLOCATION);
    content_setting_bubble_model =
        std::make_unique<ContentSettingGeolocationBubbleModel>(nullptr,
                                                               web_contents());
    const auto& bubble_content_2 =
        content_setting_bubble_model->bubble_content();

    EXPECT_EQ(bubble_content_2.title,
              l10n_util::GetStringUTF16(IDS_ALLOWED_GEOLOCATION_TITLE));
    EXPECT_EQ(bubble_content_2.message,
              l10n_util::GetStringUTF16(IDS_ALLOWED_GEOLOCATION_MESSAGE));
    EXPECT_EQ(bubble_content_2.radio_group.radio_items.size(), 2U);
    EXPECT_EQ(bubble_content_2.radio_group.radio_items[0],
              l10n_util::GetStringUTF16(IDS_ALLOWED_GEOLOCATION_NO_ACTION));
    EXPECT_EQ(
        bubble_content_2.radio_group.radio_items[1],
        l10n_util::GetStringFUTF16(IDS_ALLOWED_GEOLOCATION_BLOCK,
                                   url_formatter::FormatUrlForSecurityDisplay(
                                       web_contents()->GetLastCommittedURL())));
    EXPECT_EQ(bubble_content_2.radio_group.default_item, 0);

    settings_map->SetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                           CONTENT_SETTING_BLOCK);
    content_settings->OnContentBlocked(ContentSettingsType::GEOLOCATION);
    content_setting_bubble_model =
        std::make_unique<ContentSettingGeolocationBubbleModel>(nullptr,
                                                               web_contents());
    const auto& bubble_content_3 =
        content_setting_bubble_model->bubble_content();

    EXPECT_EQ(bubble_content_3.title,
              l10n_util::GetStringUTF16(IDS_BLOCKED_GEOLOCATION_TITLE));
    EXPECT_EQ(bubble_content_3.message,
              l10n_util::GetStringUTF16(IDS_BLOCKED_GEOLOCATION_MESSAGE));
    ASSERT_EQ(bubble_content_3.radio_group.radio_items.size(), 2U);
    EXPECT_EQ(
        bubble_content_3.radio_group.radio_items[0],
        l10n_util::GetStringFUTF16(IDS_BLOCKED_GEOLOCATION_UNBLOCK,
                                   url_formatter::FormatUrlForSecurityDisplay(
                                       web_contents()->GetLastCommittedURL())));
    EXPECT_EQ(bubble_content_3.radio_group.radio_items[1],
              l10n_util::GetStringUTF16(IDS_BLOCKED_GEOLOCATION_NO_ACTION));
    EXPECT_EQ(bubble_content_3.radio_group.default_item, 1);
  }

  WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com"));
  content_settings = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  // Clear site-specific exceptions.
  settings_map->ClearSettingsForOneType(ContentSettingsType::GEOLOCATION);

  // Allow by default but block a specific site.
  {
    settings_map->SetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                           CONTENT_SETTING_ALLOW);
    settings_map->SetContentSettingDefaultScope(
        web_contents()->GetLastCommittedURL(),
        web_contents()->GetLastCommittedURL(), ContentSettingsType::GEOLOCATION,
        CONTENT_SETTING_BLOCK);
    content_settings->OnContentBlocked(ContentSettingsType::GEOLOCATION);
    std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        std::make_unique<ContentSettingGeolocationBubbleModel>(nullptr,
                                                               web_contents()));
    const auto& bubble_content = content_setting_bubble_model->bubble_content();

    EXPECT_EQ(bubble_content.title,
              l10n_util::GetStringUTF16(IDS_BLOCKED_GEOLOCATION_TITLE));
    EXPECT_EQ(bubble_content.message,
              l10n_util::GetStringUTF16(IDS_BLOCKED_GEOLOCATION_MESSAGE));
    ASSERT_EQ(bubble_content.radio_group.radio_items.size(), 2U);
    EXPECT_EQ(
        bubble_content.radio_group.radio_items[0],
        l10n_util::GetStringFUTF16(IDS_BLOCKED_GEOLOCATION_UNBLOCK,
                                   url_formatter::FormatUrlForSecurityDisplay(
                                       web_contents()->GetLastCommittedURL())));
    EXPECT_EQ(bubble_content.radio_group.radio_items[1],
              l10n_util::GetStringUTF16(IDS_BLOCKED_GEOLOCATION_NO_ACTION));
    EXPECT_EQ(bubble_content.radio_group.default_item, 1);
  }
  // Ensure the selecting and committing of a radio button successfully commits
  // and becomes the default selection next time a bubble is created.
  {
    auto content_setting_bubble_model =
        std::make_unique<ContentSettingGeolocationBubbleModel>(nullptr,
                                                               web_contents());
    std::unique_ptr<FakeOwner> owner =
        FakeOwner::Create(*content_setting_bubble_model, 0);
    const auto& bubble_content = content_setting_bubble_model->bubble_content();
    ASSERT_EQ(bubble_content.radio_group.radio_items.size(), 2U);
    EXPECT_EQ(bubble_content.radio_group.default_item, 1);

    owner->SetSelectedRadioOptionAndCommit(0);
  }
  {
    auto content_setting_bubble_model =
        std::make_unique<ContentSettingGeolocationBubbleModel>(nullptr,
                                                               web_contents());
    std::unique_ptr<FakeOwner> owner =
        FakeOwner::Create(*content_setting_bubble_model, 0);
    const auto& bubble_content = content_setting_bubble_model->bubble_content();
    ASSERT_EQ(bubble_content.radio_group.radio_items.size(), 2U);
    EXPECT_EQ(bubble_content.radio_group.default_item, 0);
    owner->SetSelectedRadioOptionAndCommit(1);
  }
  {
    auto content_setting_bubble_model =
        std::make_unique<ContentSettingGeolocationBubbleModel>(nullptr,
                                                               web_contents());
    std::unique_ptr<FakeOwner> owner =
        FakeOwner::Create(*content_setting_bubble_model, 0);
    const auto& bubble_content = content_setting_bubble_model->bubble_content();
    ASSERT_EQ(bubble_content.radio_group.radio_items.size(), 2U);
    EXPECT_EQ(bubble_content.radio_group.default_item, 1);
  }
}

INSTANTIATE_TEST_SUITE_P(ContentSettingGeolocationBubbleModelTests,
                         ContentSettingGeolocationBubbleModelTest,
#if BUILDFLAG(IS_WIN)
                         testing::Values(false, true)
#else
                         testing::Values(true)
#endif
);

TEST_F(ContentSettingBubbleModelTest, FileURL) {
  std::string file_url("file:///tmp/test.html");
  NavigateAndCommit(GURL(file_url));
  PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame())
      ->OnContentBlocked(ContentSettingsType::IMAGES);
  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          nullptr, web_contents(), ContentSettingsType::IMAGES));
  std::u16string title =
      content_setting_bubble_model->bubble_content().radio_group.radio_items[0];
  ASSERT_NE(std::u16string::npos, title.find(base::UTF8ToUTF16(file_url)));
}

#if !BUILDFLAG(IS_ANDROID)
class ContentSettingBubbleModelIsolatedWebAppTest
    : public ContentSettingBubbleModelTest {
 public:
  void SetUp() override {
    ContentSettingBubbleModelTest::SetUp();
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

 private:
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(ContentSettingBubbleModelIsolatedWebAppTest, IsolatedWebAppUrl) {
  const std::string app_name("Test IWA Name");
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> iwa =
      web_app::IsolatedWebAppBuilder(
          web_app::ManifestBuilder().SetName(app_name))
          .BuildBundle();
  iwa->TrustSigningKey();
  iwa->FakeInstallPageState(profile());
  ASSERT_OK_AND_ASSIGN(web_app::IsolatedWebAppUrlInfo url_info,
                       iwa->Install(profile()));
  web_app::SimulateIsolatedWebAppNavigation(web_contents(),
                                            url_info.origin().GetURL());

  PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame())
      ->OnContentBlocked(ContentSettingsType::IMAGES);

  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          nullptr, web_contents(), ContentSettingsType::IMAGES));
  std::u16string title =
      content_setting_bubble_model->bubble_content().radio_group.radio_items[0];
  ASSERT_NE(std::u16string::npos, title.find(base::UTF8ToUTF16(app_name)));
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(ContentSettingBubbleModelTest, RegisterProtocolHandler) {
  const GURL page_url("https://toplevel.example/");
  NavigateAndCommit(page_url);
  PageSpecificContentSettingsDelegate::FromWebContents(web_contents())
      ->set_pending_protocol_handler(ProtocolHandler::CreateProtocolHandler(
          "mailto", GURL("https://www.toplevel.example/")));

  ContentSettingRPHBubbleModel content_setting_bubble_model(
      nullptr, web_contents(), nullptr);

  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model.bubble_content();
  EXPECT_FALSE(bubble_content.title.empty());
  EXPECT_FALSE(bubble_content.radio_group.radio_items.empty());
  EXPECT_TRUE(bubble_content.list_items.empty());
  EXPECT_TRUE(bubble_content.site_list.empty());
  EXPECT_TRUE(bubble_content.custom_link.empty());
  EXPECT_FALSE(bubble_content.custom_link_enabled);
  EXPECT_FALSE(bubble_content.manage_text.empty());
}

TEST_F(ContentSettingBubbleModelTest, RPHAllow) {
  custom_handlers::ProtocolHandlerRegistry registry(
      profile()->GetPrefs(),
      std::make_unique<custom_handlers::TestProtocolHandlerRegistryDelegate>());
  registry.InitProtocolSettings();

  const GURL page_url("https://toplevel.example/");
  NavigateAndCommit(page_url);
  auto* content_settings =
      PageSpecificContentSettingsDelegate::FromWebContents(web_contents());
  ProtocolHandler test_handler = ProtocolHandler::CreateProtocolHandler(
      "mailto", GURL("https://www.toplevel.example/"));
  content_settings->set_pending_protocol_handler(test_handler);

  ContentSettingRPHBubbleModel content_setting_bubble_model(
      nullptr, web_contents(), &registry);
  std::unique_ptr<FakeOwner> owner =
      FakeOwner::Create(content_setting_bubble_model, 0);

  {
    ProtocolHandler handler = registry.GetHandlerFor("mailto");
    EXPECT_TRUE(handler.IsEmpty());
    EXPECT_EQ(CONTENT_SETTING_DEFAULT,
              content_settings->pending_protocol_handler_setting());
  }

  // "0" is the "Allow" radio button.
  owner->SetSelectedRadioOptionAndCommit(0);
  {
    ProtocolHandler handler = registry.GetHandlerFor("mailto");
    ASSERT_FALSE(handler.IsEmpty());
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              content_settings->pending_protocol_handler_setting());
  }

  // "1" is the "Deny" radio button.
  owner->SetSelectedRadioOptionAndCommit(1);
  {
    ProtocolHandler handler = registry.GetHandlerFor("mailto");
    EXPECT_TRUE(handler.IsEmpty());
    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              content_settings->pending_protocol_handler_setting());
  }

  // "2" is the "Ignore button.
  owner->SetSelectedRadioOptionAndCommit(2);
  {
    ProtocolHandler handler = registry.GetHandlerFor("mailto");
    EXPECT_TRUE(handler.IsEmpty());
    EXPECT_EQ(CONTENT_SETTING_DEFAULT,
              content_settings->pending_protocol_handler_setting());
    EXPECT_TRUE(registry.IsIgnored(test_handler));
  }

  // "0" is the "Allow" radio button.
  owner->SetSelectedRadioOptionAndCommit(0);
  {
    ProtocolHandler handler = registry.GetHandlerFor("mailto");
    ASSERT_FALSE(handler.IsEmpty());
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              content_settings->pending_protocol_handler_setting());
    EXPECT_FALSE(registry.IsIgnored(test_handler));
  }

  registry.Shutdown();
}

TEST_F(ContentSettingBubbleModelTest, RPHDefaultDone) {
  custom_handlers::ProtocolHandlerRegistry registry(
      profile()->GetPrefs(),
      std::make_unique<custom_handlers::TestProtocolHandlerRegistryDelegate>());
  registry.InitProtocolSettings();

  const GURL page_url("https://toplevel.example/");
  NavigateAndCommit(page_url);
  auto* content_settings =
      PageSpecificContentSettingsDelegate::FromWebContents(web_contents());
  ProtocolHandler test_handler = ProtocolHandler::CreateProtocolHandler(
      "mailto", GURL("https://www.toplevel.example/"));
  content_settings->set_pending_protocol_handler(test_handler);

  ContentSettingRPHBubbleModel content_setting_bubble_model(
      nullptr, web_contents(), &registry);
  std::unique_ptr<FakeOwner> owner = FakeOwner::Create(
      content_setting_bubble_model,
      content_setting_bubble_model.bubble_content().radio_group.default_item);

  // If nothing is selected, the default action "Ignore" should be performed.
  content_setting_bubble_model.CommitChanges();
  {
    ProtocolHandler handler = registry.GetHandlerFor("mailto");
    EXPECT_TRUE(handler.IsEmpty());
    EXPECT_EQ(CONTENT_SETTING_DEFAULT,
              content_settings->pending_protocol_handler_setting());
    EXPECT_TRUE(registry.IsIgnored(test_handler));
  }

  registry.Shutdown();
}

TEST_F(ContentSettingBubbleModelTest, SubresourceFilter) {
  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      new ContentSettingSubresourceFilterBubbleModel(nullptr, web_contents()));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_EQ(bubble_content.title,
            l10n_util::GetStringUTF16(IDS_BLOCKED_ADS_PROMPT_TITLE));
  EXPECT_EQ(bubble_content.message,
            l10n_util::GetStringUTF16(IDS_BLOCKED_ADS_PROMPT_EXPLANATION));
  EXPECT_EQ(0U, bubble_content.radio_group.radio_items.size());
  EXPECT_EQ(0, bubble_content.radio_group.default_item);
  EXPECT_TRUE(bubble_content.show_learn_more);
  EXPECT_TRUE(bubble_content.custom_link.empty());
  EXPECT_FALSE(bubble_content.custom_link_enabled);
  EXPECT_EQ(bubble_content.manage_text,
            l10n_util::GetStringUTF16(IDS_ALWAYS_ALLOW_ADS));
}

class GenericSensorContentSettingBubbleModelTest
    : public ContentSettingBubbleModelTest {
 public:
  GenericSensorContentSettingBubbleModelTest() {
    // Enable all sensors just to avoid hardcoding the expected messages to the
    // motion sensor-specific ones.
    scoped_feature_list_.InitAndEnableFeature(
        features::kGenericSensorExtraClasses);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Regression test for https://crbug.com/955408
// See also: ContentSettingImageModelTest.SensorAccessPermissionsChanged
TEST_F(GenericSensorContentSettingBubbleModelTest,
       SensorAccessPermissionsChanged) {
  WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com"));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  // Go from allow by default to block by default to allow by default.
  {
    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_ALLOW);
    content_settings->OnContentAllowed(ContentSettingsType::SENSORS);
    std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        ContentSettingBubbleModel::CreateContentSettingBubbleModel(
            nullptr, web_contents(), ContentSettingsType::SENSORS));
    const auto& bubble_content = content_setting_bubble_model->bubble_content();

    EXPECT_EQ(bubble_content.title,
              l10n_util::GetStringUTF16(IDS_ALLOWED_SENSORS_TITLE));
    EXPECT_EQ(bubble_content.message,
              l10n_util::GetStringUTF16(IDS_ALLOWED_SENSORS_MESSAGE));
    ASSERT_EQ(bubble_content.radio_group.radio_items.size(), 2U);
    EXPECT_EQ(bubble_content.radio_group.radio_items[0],
              l10n_util::GetStringUTF16(IDS_ALLOWED_SENSORS_NO_ACTION));
    EXPECT_EQ(
        bubble_content.radio_group.radio_items[1],
        l10n_util::GetStringFUTF16(IDS_ALLOWED_SENSORS_BLOCK,
                                   url_formatter::FormatUrlForSecurityDisplay(
                                       web_contents()->GetLastCommittedURL())));
    EXPECT_EQ(bubble_content.radio_group.default_item, 0);

    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_BLOCK);
    content_settings->OnContentBlocked(ContentSettingsType::SENSORS);
    content_setting_bubble_model =
        ContentSettingBubbleModel::CreateContentSettingBubbleModel(
            nullptr, web_contents(), ContentSettingsType::SENSORS);
    const auto& bubble_content_2 =
        content_setting_bubble_model->bubble_content();

    EXPECT_EQ(bubble_content_2.title,
              l10n_util::GetStringUTF16(IDS_BLOCKED_SENSORS_TITLE));
    EXPECT_EQ(bubble_content_2.message,
              l10n_util::GetStringUTF16(IDS_BLOCKED_SENSORS_MESSAGE));
    EXPECT_EQ(bubble_content_2.radio_group.radio_items.size(), 2U);
    EXPECT_EQ(
        bubble_content_2.radio_group.radio_items[0],
        l10n_util::GetStringFUTF16(IDS_BLOCKED_SENSORS_UNBLOCK,
                                   url_formatter::FormatUrlForSecurityDisplay(
                                       web_contents()->GetLastCommittedURL())));
    EXPECT_EQ(bubble_content_2.radio_group.radio_items[1],
              l10n_util::GetStringUTF16(IDS_BLOCKED_SENSORS_NO_ACTION));
    EXPECT_EQ(bubble_content_2.radio_group.default_item, 1);

    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_ALLOW);
    content_settings->OnContentAllowed(ContentSettingsType::SENSORS);
    content_setting_bubble_model =
        ContentSettingBubbleModel::CreateContentSettingBubbleModel(
            nullptr, web_contents(), ContentSettingsType::SENSORS);
    const auto& bubble_content_3 =
        content_setting_bubble_model->bubble_content();

    EXPECT_EQ(bubble_content_3.title,
              l10n_util::GetStringUTF16(IDS_ALLOWED_SENSORS_TITLE));
    EXPECT_EQ(bubble_content_3.message,
              l10n_util::GetStringUTF16(IDS_ALLOWED_SENSORS_MESSAGE));
    ASSERT_EQ(bubble_content_3.radio_group.radio_items.size(), 2U);
    EXPECT_EQ(bubble_content_3.radio_group.radio_items[0],
              l10n_util::GetStringUTF16(IDS_ALLOWED_SENSORS_NO_ACTION));
    EXPECT_EQ(
        bubble_content_3.radio_group.radio_items[1],
        l10n_util::GetStringFUTF16(IDS_ALLOWED_SENSORS_BLOCK,
                                   url_formatter::FormatUrlForSecurityDisplay(
                                       web_contents()->GetLastCommittedURL())));
    EXPECT_EQ(bubble_content_3.radio_group.default_item, 0);
  }

  WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com"));
  content_settings = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());

  // Go from block by default to allow by default to block by default.
  {
    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_BLOCK);
    content_settings->OnContentBlocked(ContentSettingsType::SENSORS);
    std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        ContentSettingBubbleModel::CreateContentSettingBubbleModel(
            nullptr, web_contents(), ContentSettingsType::SENSORS));
    const auto& bubble_content = content_setting_bubble_model->bubble_content();

    EXPECT_EQ(bubble_content.title,
              l10n_util::GetStringUTF16(IDS_BLOCKED_SENSORS_TITLE));
    EXPECT_EQ(bubble_content.message,
              l10n_util::GetStringUTF16(IDS_BLOCKED_SENSORS_MESSAGE));
    ASSERT_EQ(bubble_content.radio_group.radio_items.size(), 2U);
    EXPECT_EQ(
        bubble_content.radio_group.radio_items[0],
        l10n_util::GetStringFUTF16(IDS_BLOCKED_SENSORS_UNBLOCK,
                                   url_formatter::FormatUrlForSecurityDisplay(
                                       web_contents()->GetLastCommittedURL())));
    EXPECT_EQ(bubble_content.radio_group.radio_items[1],
              l10n_util::GetStringUTF16(IDS_BLOCKED_SENSORS_NO_ACTION));
    EXPECT_EQ(bubble_content.radio_group.default_item, 1);

    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_ALLOW);
    content_settings->OnContentAllowed(ContentSettingsType::SENSORS);
    content_setting_bubble_model =
        ContentSettingBubbleModel::CreateContentSettingBubbleModel(
            nullptr, web_contents(), ContentSettingsType::SENSORS);
    const auto& bubble_content_2 =
        content_setting_bubble_model->bubble_content();

    EXPECT_EQ(bubble_content_2.title,
              l10n_util::GetStringUTF16(IDS_ALLOWED_SENSORS_TITLE));
    EXPECT_EQ(bubble_content_2.message,
              l10n_util::GetStringUTF16(IDS_ALLOWED_SENSORS_MESSAGE));
    EXPECT_EQ(bubble_content_2.radio_group.radio_items.size(), 2U);
    EXPECT_EQ(bubble_content_2.radio_group.radio_items[0],
              l10n_util::GetStringUTF16(IDS_ALLOWED_SENSORS_NO_ACTION));
    EXPECT_EQ(
        bubble_content_2.radio_group.radio_items[1],
        l10n_util::GetStringFUTF16(IDS_ALLOWED_SENSORS_BLOCK,
                                   url_formatter::FormatUrlForSecurityDisplay(
                                       web_contents()->GetLastCommittedURL())));
    EXPECT_EQ(bubble_content_2.radio_group.default_item, 0);

    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_BLOCK);
    content_settings->OnContentBlocked(ContentSettingsType::SENSORS);
    content_setting_bubble_model =
        ContentSettingBubbleModel::CreateContentSettingBubbleModel(
            nullptr, web_contents(), ContentSettingsType::SENSORS);
    const auto& bubble_content_3 =
        content_setting_bubble_model->bubble_content();

    EXPECT_EQ(bubble_content_3.title,
              l10n_util::GetStringUTF16(IDS_BLOCKED_SENSORS_TITLE));
    EXPECT_EQ(bubble_content_3.message,
              l10n_util::GetStringUTF16(IDS_BLOCKED_SENSORS_MESSAGE));
    ASSERT_EQ(bubble_content_3.radio_group.radio_items.size(), 2U);
    EXPECT_EQ(
        bubble_content_3.radio_group.radio_items[0],
        l10n_util::GetStringFUTF16(IDS_BLOCKED_SENSORS_UNBLOCK,
                                   url_formatter::FormatUrlForSecurityDisplay(
                                       web_contents()->GetLastCommittedURL())));
    EXPECT_EQ(bubble_content_3.radio_group.radio_items[1],
              l10n_util::GetStringUTF16(IDS_BLOCKED_SENSORS_NO_ACTION));
    EXPECT_EQ(bubble_content_3.radio_group.default_item, 1);
  }

  WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com"));
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
    std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        ContentSettingBubbleModel::CreateContentSettingBubbleModel(
            nullptr, web_contents(), ContentSettingsType::SENSORS));
    const auto& bubble_content = content_setting_bubble_model->bubble_content();

    EXPECT_EQ(bubble_content.title,
              l10n_util::GetStringUTF16(IDS_ALLOWED_SENSORS_TITLE));
    EXPECT_EQ(bubble_content.message,
              l10n_util::GetStringUTF16(IDS_ALLOWED_SENSORS_MESSAGE));
    ASSERT_EQ(bubble_content.radio_group.radio_items.size(), 2U);
    EXPECT_EQ(bubble_content.radio_group.radio_items[0],
              l10n_util::GetStringUTF16(IDS_ALLOWED_SENSORS_NO_ACTION));
    EXPECT_EQ(
        bubble_content.radio_group.radio_items[1],
        l10n_util::GetStringFUTF16(IDS_ALLOWED_SENSORS_BLOCK,
                                   url_formatter::FormatUrlForSecurityDisplay(
                                       web_contents()->GetLastCommittedURL())));
    EXPECT_EQ(bubble_content.radio_group.default_item, 0);
  }

  WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com"));
  content_settings = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  // Clear site-specific exceptions.
  settings_map->ClearSettingsForOneType(ContentSettingsType::SENSORS);

  // Allow by default but block a specific site.
  {
    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_ALLOW);
    settings_map->SetContentSettingDefaultScope(
        web_contents()->GetLastCommittedURL(),
        web_contents()->GetLastCommittedURL(), ContentSettingsType::SENSORS,
        CONTENT_SETTING_BLOCK);
    content_settings->OnContentBlocked(ContentSettingsType::SENSORS);
    std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        ContentSettingBubbleModel::CreateContentSettingBubbleModel(
            nullptr, web_contents(), ContentSettingsType::SENSORS));
    const auto& bubble_content = content_setting_bubble_model->bubble_content();

    EXPECT_EQ(bubble_content.title,
              l10n_util::GetStringUTF16(IDS_BLOCKED_SENSORS_TITLE));
    EXPECT_EQ(bubble_content.message,
              l10n_util::GetStringUTF16(IDS_BLOCKED_SENSORS_MESSAGE));
    ASSERT_EQ(bubble_content.radio_group.radio_items.size(), 2U);
    EXPECT_EQ(
        bubble_content.radio_group.radio_items[0],
        l10n_util::GetStringFUTF16(IDS_BLOCKED_SENSORS_UNBLOCK,
                                   url_formatter::FormatUrlForSecurityDisplay(
                                       web_contents()->GetLastCommittedURL())));
    EXPECT_EQ(bubble_content.radio_group.radio_items[1],
              l10n_util::GetStringUTF16(IDS_BLOCKED_SENSORS_NO_ACTION));
    EXPECT_EQ(bubble_content.radio_group.default_item, 1);
  }
}

TEST_F(ContentSettingBubbleModelTest, PopupBubbleModelListItems) {
  const GURL url("https://www.example.test/");
  WebContentsTester::For(web_contents())->NavigateAndCommit(url);
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  content_settings->OnContentBlocked(ContentSettingsType::POPUPS);

  blocked_content::PopupBlockerTabHelper::CreateForWebContents(web_contents());
  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          nullptr, web_contents(), ContentSettingsType::POPUPS));
  const auto& list_items =
      content_setting_bubble_model->bubble_content().list_items;
  EXPECT_EQ(0U, list_items.size());

  BlockedWindowParams params(GURL("about:blank"), url::Origin(), nullptr,
                             content::Referrer(), std::string(),
                             WindowOpenDisposition::NEW_POPUP,
                             blink::mojom::WindowFeatures(), false, true);
  constexpr size_t kItemCount = 3;
  for (size_t i = 1; i <= kItemCount; i++) {
    NavigateParams navigate_params =
        params.CreateNavigateParams(process(), web_contents());
    EXPECT_FALSE(blocked_content::MaybeBlockPopup(
        web_contents(), &url,
        std::make_unique<ChromePopupNavigationDelegate>(
            std::move(navigate_params)),
        nullptr /*=open_url_params*/, params.features(),
        HostContentSettingsMapFactory::GetForProfile(profile())));
    EXPECT_EQ(i, list_items.size());
  }
}

TEST_F(ContentSettingBubbleModelTest, ValidUrl) {
  WebContentsTester::For(web_contents())->
      NavigateAndCommit(GURL("https://www.example.com"));

  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  content_settings->OnContentBlocked(ContentSettingsType::COOKIES);

  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          nullptr, web_contents(), ContentSettingsType::COOKIES));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();

  EXPECT_TRUE(bubble_content.is_user_modifiable);
}

TEST_F(ContentSettingBubbleModelTest, InvalidUrl) {
  WebContentsTester::For(web_contents())->
      NavigateAndCommit(GURL("about:blank"));

  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  content_settings->OnContentBlocked(ContentSettingsType::COOKIES);

  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          nullptr, web_contents(), ContentSettingsType::COOKIES));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();

  EXPECT_FALSE(bubble_content.is_user_modifiable);
}

TEST_F(ContentSettingBubbleModelTest, StorageAccess) {
  base::HistogramTester t;
  WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com"));
  auto* content_settings = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());

  net::SchemefulSite site(GURL("https://example.com"));
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(site.GetURL(), web_contents()->GetURL(),
                                     ContentSettingsType::STORAGE_ACCESS,
                                     CONTENT_SETTING_BLOCK);

  content_settings->OnTwoSitePermissionChanged(
      ContentSettingsType::STORAGE_ACCESS, site, CONTENT_SETTING_BLOCK);

  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          nullptr, web_contents(), ContentSettingsType::STORAGE_ACCESS));
  t.ExpectUniqueSample("ContentSettings.Bubble.StorageAccess.Action",
                       ContentSettingBubbleAction::kOpened, 1);
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();

  EXPECT_FALSE(bubble_content.title.empty());
  EXPECT_EQ(0U, bubble_content.radio_group.radio_items.size());
  EXPECT_THAT(bubble_content.site_list,
              UnorderedElementsAre(Pair(site, false)));

  content_setting_bubble_model->OnSiteRowClicked(site, true);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(site.GetURL(), web_contents()->GetURL(),
                                   ContentSettingsType::STORAGE_ACCESS));
  t.ExpectTotalCount("ContentSettings.Bubble.StorageAccess.Action", 2);
  t.ExpectBucketCount("ContentSettings.Bubble.StorageAccess.Action",
                      ContentSettingBubbleAction::kPermissionAllowed, 1);
  content_setting_bubble_model->CommitChanges();
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(site.GetURL(), web_contents()->GetURL(),
                                   ContentSettingsType::STORAGE_ACCESS));
}
