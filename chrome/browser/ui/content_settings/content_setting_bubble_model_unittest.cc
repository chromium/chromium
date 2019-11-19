// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/test_protocol_handler_registry_delegate.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/blocked_content/popup_blocker.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/fake_owner.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "services/device/public/cpp/device_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebContentsTester;

class ContentSettingBubbleModelTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    TabSpecificContentSettings::CreateForWebContents(web_contents());
    InfoBarService::CreateForWebContents(web_contents());
  }

  void CheckGeolocationBubble(size_t expected_domains,
                              bool expect_clear_link,
                              bool expect_reload_hint) {
    std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        ContentSettingBubbleModel::CreateContentSettingBubbleModel(
            NULL, web_contents(), ContentSettingsType::GEOLOCATION));
    const ContentSettingBubbleModel::BubbleContent& bubble_content =
        content_setting_bubble_model->bubble_content();
    EXPECT_TRUE(bubble_content.title.empty());
    EXPECT_TRUE(bubble_content.radio_group.radio_items.empty());
    EXPECT_TRUE(bubble_content.list_items.empty());
    EXPECT_EQ(expected_domains, bubble_content.domain_lists.size());
    EXPECT_NE(expect_clear_link || expect_reload_hint,
              bubble_content.custom_link.empty());
    EXPECT_EQ(expect_clear_link, bubble_content.custom_link_enabled);
    EXPECT_FALSE(bubble_content.manage_text.empty());
  }

  std::string GetDefaultAudioDevice() {
    PrefService* prefs = profile()->GetPrefs();
    return prefs->GetString(prefs::kDefaultAudioCaptureDevice);
  }

  std::string GetDefaultVideoDevice() {
    PrefService* prefs = profile()->GetPrefs();
    return prefs->GetString(prefs::kDefaultVideoCaptureDevice);
  }
};

TEST_F(ContentSettingBubbleModelTest, ImageRadios) {
  WebContentsTester::For(web_contents())->
      NavigateAndCommit(GURL("https://www.example.com"));
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  content_settings->OnContentBlocked(ContentSettingsType::IMAGES);

  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          NULL, web_contents(), ContentSettingsType::IMAGES));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_FALSE(bubble_content.title.empty());
  EXPECT_EQ(2U, bubble_content.radio_group.radio_items.size());
  EXPECT_EQ(0, bubble_content.radio_group.default_item);
  EXPECT_TRUE(bubble_content.custom_link.empty());
  EXPECT_FALSE(bubble_content.manage_text.empty());
}

TEST_F(ContentSettingBubbleModelTest, Cookies) {
  WebContentsTester::For(web_contents())->
      NavigateAndCommit(GURL("https://www.example.com"));
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  content_settings->OnContentBlocked(ContentSettingsType::COOKIES);

  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          NULL, web_contents(), ContentSettingsType::COOKIES));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  base::string16 title = bubble_content.title;
  EXPECT_FALSE(title.empty());
  ASSERT_EQ(2U, bubble_content.radio_group.radio_items.size());
  EXPECT_FALSE(bubble_content.custom_link.empty());
  EXPECT_TRUE(bubble_content.custom_link_enabled);
  EXPECT_FALSE(bubble_content.manage_text.empty());

  content_settings->ClearNavigationRelatedContentSettings();
  content_settings->OnContentAllowed(ContentSettingsType::COOKIES);
  content_setting_bubble_model =
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          NULL, web_contents(), ContentSettingsType::COOKIES);
  const ContentSettingBubbleModel::BubbleContent& bubble_content_2 =
      content_setting_bubble_model->bubble_content();

  EXPECT_FALSE(bubble_content_2.title.empty());
  EXPECT_NE(title, bubble_content_2.title);
  ASSERT_EQ(2U, bubble_content_2.radio_group.radio_items.size());
  EXPECT_EQ(bubble_content_2.radio_group.radio_items[0],
            l10n_util::GetStringUTF16(IDS_ALLOWED_COOKIES_NO_ACTION));
  EXPECT_EQ(
      bubble_content_2.radio_group.radio_items[1],
      l10n_util::GetStringFUTF16(IDS_ALLOWED_COOKIES_BLOCK,
                                 url_formatter::FormatUrlForSecurityDisplay(
                                     web_contents()->GetURL())));
  EXPECT_FALSE(bubble_content_2.custom_link.empty());
  EXPECT_TRUE(bubble_content_2.custom_link_enabled);
  EXPECT_FALSE(bubble_content_2.manage_text.empty());
}

TEST_F(ContentSettingBubbleModelTest, MediastreamMicAndCamera) {
  // Required to break dependency on BrowserMainLoop.
  MediaCaptureDevicesDispatcher::GetInstance()->
      DisableDeviceEnumerationForTesting();

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  std::string request_host = "google.com";
  GURL security_origin("http://" + request_host);
  TabSpecificContentSettings::MicrophoneCameraState microphone_camera_state =
      TabSpecificContentSettings::MICROPHONE_ACCESSED |
      TabSpecificContentSettings::CAMERA_ACCESSED;
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               microphone_camera_state,
                                               GetDefaultAudioDevice(),
                                               GetDefaultVideoDevice(),
                                               std::string(),
                                               std::string());

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
  EXPECT_EQ(2U, bubble_content.media_menus.size());
}

TEST_F(ContentSettingBubbleModelTest, BlockedMediastreamMicAndCamera) {
  // Required to break dependency on BrowserMainLoop.
  MediaCaptureDevicesDispatcher::GetInstance()->
      DisableDeviceEnumerationForTesting();

  WebContentsTester::For(web_contents())->
      NavigateAndCommit(GURL("https://www.example.com"));
  GURL url = web_contents()->GetURL();

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  ContentSetting setting = CONTENT_SETTING_BLOCK;
  host_content_settings_map->SetContentSettingDefaultScope(
      url, GURL(), ContentSettingsType::MEDIASTREAM_MIC, std::string(),
      setting);
  host_content_settings_map->SetContentSettingDefaultScope(
      url, GURL(), ContentSettingsType::MEDIASTREAM_CAMERA, std::string(),
      setting);

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  TabSpecificContentSettings::MicrophoneCameraState microphone_camera_state =
      TabSpecificContentSettings::MICROPHONE_ACCESSED |
      TabSpecificContentSettings::MICROPHONE_BLOCKED |
      TabSpecificContentSettings::CAMERA_ACCESSED |
      TabSpecificContentSettings::CAMERA_BLOCKED;
  content_settings->OnMediaStreamPermissionSet(url,
                                               microphone_camera_state,
                                               GetDefaultAudioDevice(),
                                               GetDefaultVideoDevice(),
                                               std::string(),
                                               std::string());

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
                url, url, ContentSettingsType::MEDIASTREAM_MIC, std::string()));
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      host_content_settings_map->GetContentSetting(
          url, url, ContentSettingsType::MEDIASTREAM_CAMERA, std::string()));

  owner->SetSelectedRadioOptionAndCommit(0);

  // Test that the media setting were change correctly.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::MEDIASTREAM_MIC, std::string()));
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      host_content_settings_map->GetContentSetting(
          url, url, ContentSettingsType::MEDIASTREAM_CAMERA, std::string()));
}

// Tests whether a changed setting in the setting bubble is displayed again when
// the bubble is re-opened.
TEST_F(ContentSettingBubbleModelTest, MediastreamContentBubble) {
  ASSERT_TRUE(profile()->CreateHistoryService(
      /* delete_file= */ true,
      /* no_db= */ false));

  // Required to break dependency on BrowserMainLoop.
  MediaCaptureDevicesDispatcher::GetInstance()->
      DisableDeviceEnumerationForTesting();

  WebContentsTester::For(web_contents())->
      NavigateAndCommit(GURL("https://www.example.com"));
  GURL url = web_contents()->GetURL();

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  ContentSetting setting = CONTENT_SETTING_BLOCK;
  host_content_settings_map->SetContentSettingDefaultScope(
      url, GURL(), ContentSettingsType::MEDIASTREAM_MIC, std::string(),
      setting);

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  TabSpecificContentSettings::MicrophoneCameraState microphone_camera_state =
      TabSpecificContentSettings::MICROPHONE_ACCESSED |
      TabSpecificContentSettings::MICROPHONE_BLOCKED;
  content_settings->OnMediaStreamPermissionSet(url,
                                               microphone_camera_state,
                                               GetDefaultAudioDevice(),
                                               std::string(),
                                               std::string(),
                                               std::string());
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
                url, url, ContentSettingsType::MEDIASTREAM_MIC, std::string()));

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
                url, url, ContentSettingsType::MEDIASTREAM_MIC, std::string()));

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

// Tests whether the media menu settings are correctly persisted in the bubble.
TEST_F(ContentSettingBubbleModelTest, MediastreamContentBubbleMediaMenus) {
  // Required to break dependency on BrowserMainLoop.
  MediaCaptureDevicesDispatcher::GetInstance()->
      DisableDeviceEnumerationForTesting();

  WebContentsTester::For(web_contents())->
      NavigateAndCommit(GURL("https://www.example.com"));
  GURL url = web_contents()->GetURL();

  blink::MediaStreamDevices audio_devices;
  blink::MediaStreamDevice fake_audio_device1(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "fake_dev1",
      "Fake Audio Device 1");
  blink::MediaStreamDevice fake_audio_device2(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "fake_dev2",
      "Fake Audio Device 2");
  blink::MediaStreamDevice fake_audio_device3(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "fake_dev3",
      "Fake Audio Device 3");
  audio_devices.push_back(fake_audio_device1);
  audio_devices.push_back(fake_audio_device2);
  audio_devices.push_back(fake_audio_device3);
  MediaCaptureDevicesDispatcher::GetInstance()->SetTestAudioCaptureDevices(
      audio_devices);

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  TabSpecificContentSettings::MicrophoneCameraState microphone_camera_state =
      TabSpecificContentSettings::MICROPHONE_ACCESSED |
      TabSpecificContentSettings::MICROPHONE_BLOCKED;
  content_settings->OnMediaStreamPermissionSet(url,
                                               microphone_camera_state,
                                               GetDefaultAudioDevice(),
                                               std::string(),
                                               std::string(),
                                               std::string());
  {
    std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        new ContentSettingMediaStreamBubbleModel(nullptr, web_contents()));
    const ContentSettingBubbleModel::BubbleContent& bubble_content =
        content_setting_bubble_model->bubble_content();
    std::unique_ptr<FakeOwner> owner = FakeOwner::Create(
        *content_setting_bubble_model, bubble_content.radio_group.default_item);
    EXPECT_TRUE(bubble_content.custom_link.empty());

    EXPECT_EQ(1U, bubble_content.media_menus.size());
    EXPECT_EQ(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
              bubble_content.media_menus.begin()->first);
    EXPECT_FALSE(bubble_content.media_menus.begin()->second.disabled);
    // The first audio device should be selected by default.
    EXPECT_TRUE(fake_audio_device1.IsSameDevice(
        bubble_content.media_menus.begin()->second.selected_device));

    // Select a different (the second) device.
    content_setting_bubble_model->OnMediaMenuClicked(
        blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
        fake_audio_device2.id);
    content_setting_bubble_model->CommitChanges();
  }
  {
    std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        new ContentSettingMediaStreamBubbleModel(nullptr, web_contents()));
    const ContentSettingBubbleModel::BubbleContent& bubble_content =
        content_setting_bubble_model->bubble_content();
    std::unique_ptr<FakeOwner> owner = FakeOwner::Create(
        *content_setting_bubble_model, bubble_content.radio_group.default_item);
    EXPECT_EQ(1U, bubble_content.media_menus.size());
    EXPECT_EQ(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
              bubble_content.media_menus.begin()->first);
    EXPECT_FALSE(bubble_content.media_menus.begin()->second.disabled);
    // The second audio device should be selected.
    EXPECT_TRUE(fake_audio_device2.IsSameDevice(
        bubble_content.media_menus.begin()->second.selected_device));
    // The "settings changed" message should not be displayed when there is no
    // active capture.
    EXPECT_FALSE(bubble_content.custom_link_enabled);
    EXPECT_TRUE(bubble_content.custom_link.empty());
    content_setting_bubble_model->CommitChanges();
  }

  // Simulate that an audio stream is being captured.
  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()->
        GetMediaStreamCaptureIndicator();
  std::unique_ptr<content::MediaStreamUI> media_stream_ui =
      indicator->RegisterMediaStream(web_contents(), audio_devices);
  media_stream_ui->OnStarted(base::OnceClosure(),
                             content::MediaStreamUI::SourceCallback());
  microphone_camera_state &= ~TabSpecificContentSettings::MICROPHONE_BLOCKED;
  content_settings->OnMediaStreamPermissionSet(url,
                                               microphone_camera_state,
                                               GetDefaultAudioDevice(),
                                               std::string(),
                                               std::string(),
                                               std::string());

  {
    std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        new ContentSettingMediaStreamBubbleModel(nullptr, web_contents()));
    const ContentSettingBubbleModel::BubbleContent& bubble_content =
        content_setting_bubble_model->bubble_content();
    std::unique_ptr<FakeOwner> owner = FakeOwner::Create(
        *content_setting_bubble_model, bubble_content.radio_group.default_item);
    // Settings not changed yet, so the "settings changed" message should not be
    // shown.
    EXPECT_TRUE(bubble_content.custom_link.empty());

    EXPECT_EQ(1U, bubble_content.media_menus.size());
    EXPECT_EQ(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
              bubble_content.media_menus.begin()->first);
    EXPECT_FALSE(bubble_content.media_menus.begin()->second.disabled);
    EXPECT_TRUE(fake_audio_device2.IsSameDevice(
        bubble_content.media_menus.begin()->second.selected_device));

    // Select a different different device.
    content_setting_bubble_model->OnMediaMenuClicked(
        blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
        fake_audio_device3.id);
    content_setting_bubble_model->CommitChanges();
  }

  {
    std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        new ContentSettingMediaStreamBubbleModel(nullptr, web_contents()));
    const ContentSettingBubbleModel::BubbleContent& bubble_content =
        content_setting_bubble_model->bubble_content();
    std::unique_ptr<FakeOwner> owner = FakeOwner::Create(
        *content_setting_bubble_model, bubble_content.radio_group.default_item);
    // Test that the reload hint is displayed.
    EXPECT_FALSE(bubble_content.custom_link_enabled);
    EXPECT_EQ(
        bubble_content.custom_link,
        l10n_util::GetStringUTF16(IDS_MEDIASTREAM_SETTING_CHANGED_MESSAGE));
  }

  // Simulate that yet another audio stream capture request was initiated.
  microphone_camera_state |= TabSpecificContentSettings::MICROPHONE_BLOCKED;
  content_settings->OnMediaStreamPermissionSet(url,
                                               microphone_camera_state,
                                               GetDefaultAudioDevice(),
                                               std::string(),
                                               std::string(),
                                               std::string());

  {
    std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        new ContentSettingMediaStreamBubbleModel(nullptr, web_contents()));
    const ContentSettingBubbleModel::BubbleContent& bubble_content =
        content_setting_bubble_model->bubble_content();
    std::unique_ptr<FakeOwner> owner = FakeOwner::Create(
        *content_setting_bubble_model, bubble_content.radio_group.default_item);
    // Test that the reload hint is not displayed any more, because this is a
    // new permission request.
    EXPECT_FALSE(bubble_content.custom_link_enabled);
    EXPECT_TRUE(bubble_content.custom_link.empty());

    // Though the audio menu setting should have persisted.
    EXPECT_EQ(1U, bubble_content.media_menus.size());
    EXPECT_EQ(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
              bubble_content.media_menus.begin()->first);
    EXPECT_FALSE(bubble_content.media_menus.begin()->second.disabled);
    EXPECT_TRUE(fake_audio_device3.IsSameDevice(
        bubble_content.media_menus.begin()->second.selected_device));
  }
}

TEST_F(ContentSettingBubbleModelTest, MediastreamMic) {
  // Required to break dependency on BrowserMainLoop.
  MediaCaptureDevicesDispatcher::GetInstance()->
      DisableDeviceEnumerationForTesting();

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  std::string request_host = "google.com";
  GURL security_origin("http://" + request_host);
  TabSpecificContentSettings::MicrophoneCameraState microphone_camera_state =
      TabSpecificContentSettings::MICROPHONE_ACCESSED;
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               microphone_camera_state,
                                               GetDefaultAudioDevice(),
                                               std::string(),
                                               std::string(),
                                               std::string());

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
  EXPECT_EQ(1U, bubble_content.media_menus.size());
  EXPECT_EQ(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
            bubble_content.media_menus.begin()->first);

  // Change the microphone access.
  microphone_camera_state |= TabSpecificContentSettings::MICROPHONE_BLOCKED;
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               microphone_camera_state,
                                               GetDefaultAudioDevice(),
                                               std::string(),
                                               std::string(),
                                               std::string());
  content_setting_bubble_model.reset(
      new ContentSettingMediaStreamBubbleModel(nullptr, web_contents()));
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
  EXPECT_EQ(1U, new_bubble_content.media_menus.size());
  EXPECT_EQ(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
            new_bubble_content.media_menus.begin()->first);
}

TEST_F(ContentSettingBubbleModelTest, MediastreamCamera) {
  // Required to break dependency on BrowserMainLoop.
  MediaCaptureDevicesDispatcher::GetInstance()->
      DisableDeviceEnumerationForTesting();

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  std::string request_host = "google.com";
  GURL security_origin("http://" + request_host);
  TabSpecificContentSettings::MicrophoneCameraState microphone_camera_state =
      TabSpecificContentSettings::CAMERA_ACCESSED;
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               microphone_camera_state,
                                               std::string(),
                                               GetDefaultVideoDevice(),
                                               std::string(),
                                               std::string());

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
  EXPECT_EQ(1U, bubble_content.media_menus.size());
  EXPECT_EQ(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
            bubble_content.media_menus.begin()->first);

  // Change the camera access.
  microphone_camera_state |= TabSpecificContentSettings::CAMERA_BLOCKED;
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               microphone_camera_state,
                                               std::string(),
                                               GetDefaultVideoDevice(),
                                               std::string(),
                                               std::string());
  content_setting_bubble_model.reset(
      new ContentSettingMediaStreamBubbleModel(nullptr, web_contents()));
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
  EXPECT_EQ(1U, new_bubble_content.media_menus.size());
  EXPECT_EQ(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
            new_bubble_content.media_menus.begin()->first);
}

TEST_F(ContentSettingBubbleModelTest, AccumulateMediastreamMicAndCamera) {
  // Required to break dependency on BrowserMainLoop.
  MediaCaptureDevicesDispatcher::GetInstance()->
      DisableDeviceEnumerationForTesting();

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  std::string request_host = "google.com";
  GURL security_origin("http://" + request_host);

  // Firstly, add microphone access.
  TabSpecificContentSettings::MicrophoneCameraState microphone_camera_state =
      TabSpecificContentSettings::MICROPHONE_ACCESSED;
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               microphone_camera_state,
                                               GetDefaultAudioDevice(),
                                               std::string(),
                                               std::string(),
                                               std::string());

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
  EXPECT_EQ(1U, bubble_content.media_menus.size());
  EXPECT_EQ(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
            bubble_content.media_menus.begin()->first);

  // Then add camera access.
  microphone_camera_state |= TabSpecificContentSettings::CAMERA_ACCESSED;
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               microphone_camera_state,
                                               GetDefaultAudioDevice(),
                                               GetDefaultVideoDevice(),
                                               std::string(),
                                               std::string());

  content_setting_bubble_model.reset(
      new ContentSettingMediaStreamBubbleModel(nullptr, web_contents()));
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
  EXPECT_EQ(2U, new_bubble_content.media_menus.size());
}

TEST_F(ContentSettingBubbleModelTest, Plugins) {
  WebContentsTester::For(web_contents())->
      NavigateAndCommit(GURL("https://www.example.com"));
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  const base::string16 plugin_name = base::ASCIIToUTF16("plugin_name");

  content_settings->OnContentBlocked(ContentSettingsType::PLUGINS);

  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          NULL, web_contents(), ContentSettingsType::PLUGINS));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_FALSE(bubble_content.title.empty());
  ASSERT_EQ(0U, bubble_content.list_items.size());
  EXPECT_EQ(0U, bubble_content.radio_group.radio_items.size());
  EXPECT_FALSE(bubble_content.custom_link_enabled);
  EXPECT_TRUE(bubble_content.custom_link.empty());
  EXPECT_FALSE(bubble_content.manage_text.empty());
  EXPECT_TRUE(bubble_content.show_learn_more);
}

TEST_F(ContentSettingBubbleModelTest, PepperBroker) {
  WebContentsTester::For(web_contents())->
      NavigateAndCommit(GURL("https://www.example.com"));
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  content_settings->OnContentBlocked(ContentSettingsType::PPAPI_BROKER);

  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          NULL, web_contents(), ContentSettingsType::PPAPI_BROKER));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();

  base::string16 title = bubble_content.title;
  EXPECT_FALSE(title.empty());
  ASSERT_EQ(2U, bubble_content.radio_group.radio_items.size());
  base::string16 radio1 = bubble_content.radio_group.radio_items[0];
  base::string16 radio2 = bubble_content.radio_group.radio_items[1];
  EXPECT_FALSE(bubble_content.custom_link_enabled);
  EXPECT_FALSE(bubble_content.manage_text.empty());

  content_settings
      ->ClearContentSettingsExceptForNavigationRelatedSettings();
  content_settings->OnContentAllowed(ContentSettingsType::PPAPI_BROKER);
  content_setting_bubble_model =
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          NULL, web_contents(), ContentSettingsType::PPAPI_BROKER);
  const ContentSettingBubbleModel::BubbleContent& bubble_content_2 =
      content_setting_bubble_model->bubble_content();

  EXPECT_FALSE(bubble_content_2.title.empty());
  EXPECT_NE(title, bubble_content_2.title);
  ASSERT_EQ(2U, bubble_content_2.radio_group.radio_items.size());
  EXPECT_NE(radio1, bubble_content_2.radio_group.radio_items[0]);
  EXPECT_NE(radio2, bubble_content_2.radio_group.radio_items[1]);
  EXPECT_FALSE(bubble_content_2.custom_link_enabled);
  EXPECT_FALSE(bubble_content_2.manage_text.empty());
}

TEST_F(ContentSettingBubbleModelTest, Geolocation) {
  const GURL page_url("http://toplevel.example/");
  const GURL frame1_url("http://host1.example/");
  const GURL frame2_url("http://host2.example:999/");

  NavigateAndCommit(page_url);
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());

  // One permitted frame, but not in the content map: requires reload.
  content_settings->OnGeolocationPermissionSet(frame1_url, true);
  CheckGeolocationBubble(1, false, true);

  // Add it to the content map, should now have a clear link.
  HostContentSettingsMap* setting_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  setting_map->SetContentSettingDefaultScope(
      frame1_url, page_url, ContentSettingsType::GEOLOCATION, std::string(),
      CONTENT_SETTING_ALLOW);
  CheckGeolocationBubble(1, true, false);

  // Change the default to allow: no message needed.
  setting_map->SetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                        CONTENT_SETTING_ALLOW);
  CheckGeolocationBubble(1, false, false);

  // Second frame denied, but not stored in the content map: requires reload.
  content_settings->OnGeolocationPermissionSet(frame2_url, false);
  CheckGeolocationBubble(2, false, true);

  // Change the default to block: offer a clear link for the persisted frame 1.
  setting_map->SetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                        CONTENT_SETTING_BLOCK);
  CheckGeolocationBubble(2, true, false);
}

TEST_F(ContentSettingBubbleModelTest, FileURL) {
  std::string file_url("file:///tmp/test.html");
  NavigateAndCommit(GURL(file_url));
  TabSpecificContentSettings::FromWebContents(web_contents())
      ->OnContentBlocked(ContentSettingsType::IMAGES);
  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          nullptr, web_contents(), ContentSettingsType::IMAGES));
  base::string16 title =
      content_setting_bubble_model->bubble_content().radio_group.radio_items[0];
  ASSERT_NE(base::string16::npos, title.find(base::UTF8ToUTF16(file_url)));
}

TEST_F(ContentSettingBubbleModelTest, RegisterProtocolHandler) {
  const GURL page_url("http://toplevel.example/");
  NavigateAndCommit(page_url);
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  content_settings->set_pending_protocol_handler(
      ProtocolHandler::CreateProtocolHandler(
          "mailto", GURL("http://www.toplevel.example/")));

  ContentSettingRPHBubbleModel content_setting_bubble_model(
      NULL, web_contents(), NULL);

  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model.bubble_content();
  EXPECT_FALSE(bubble_content.title.empty());
  EXPECT_FALSE(bubble_content.radio_group.radio_items.empty());
  EXPECT_TRUE(bubble_content.list_items.empty());
  EXPECT_TRUE(bubble_content.domain_lists.empty());
  EXPECT_TRUE(bubble_content.custom_link.empty());
  EXPECT_FALSE(bubble_content.custom_link_enabled);
  EXPECT_FALSE(bubble_content.manage_text.empty());
}

TEST_F(ContentSettingBubbleModelTest, RPHAllow) {
  ProtocolHandlerRegistry registry(
      profile(), std::make_unique<TestProtocolHandlerRegistryDelegate>());
  registry.InitProtocolSettings();

  const GURL page_url("http://toplevel.example/");
  NavigateAndCommit(page_url);
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  ProtocolHandler test_handler = ProtocolHandler::CreateProtocolHandler(
      "mailto", GURL("http://www.toplevel.example/"));
  content_settings->set_pending_protocol_handler(test_handler);

  ContentSettingRPHBubbleModel content_setting_bubble_model(
      NULL, web_contents(), &registry);
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
  ProtocolHandlerRegistry registry(
      profile(), std::make_unique<TestProtocolHandlerRegistryDelegate>());
  registry.InitProtocolSettings();

  const GURL page_url("http://toplevel.example/");
  NavigateAndCommit(page_url);
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  ProtocolHandler test_handler = ProtocolHandler::CreateProtocolHandler(
      "mailto", GURL("http://www.toplevel.example/"));
  content_settings->set_pending_protocol_handler(test_handler);

  ContentSettingRPHBubbleModel content_setting_bubble_model(
      NULL, web_contents(), &registry);
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
  EXPECT_EQ(0U, bubble_content.media_menus.size());
}

// Regression test for https://crbug.com/955408
// See also: ContentSettingImageModelTest.SensorAccessPermissionsChanged
TEST_F(ContentSettingBubbleModelTest, SensorAccessPermissionsChanged) {
  // Enable all sensors just to avoid hardcoding the expected messages to the
  // motion sensor-specific ones.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGenericSensorExtraClasses);

  WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.example.com"));
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  // Go from allow by default to block by default to allow by default.
  {
    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_ALLOW);
    content_settings->OnContentAllowed(ContentSettingsType::SENSORS);
    std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        ContentSettingBubbleModel::CreateContentSettingBubbleModel(
            NULL, web_contents(), ContentSettingsType::SENSORS));
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
                                       web_contents()->GetURL())));
    EXPECT_EQ(bubble_content.radio_group.default_item, 0);

    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_BLOCK);
    content_settings->OnContentBlocked(ContentSettingsType::SENSORS);
    content_setting_bubble_model =
        ContentSettingBubbleModel::CreateContentSettingBubbleModel(
            NULL, web_contents(), ContentSettingsType::SENSORS);
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
                                       web_contents()->GetURL())));
    EXPECT_EQ(bubble_content_2.radio_group.radio_items[1],
              l10n_util::GetStringUTF16(IDS_BLOCKED_SENSORS_NO_ACTION));
    EXPECT_EQ(bubble_content_2.radio_group.default_item, 1);

    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_ALLOW);
    content_settings->OnContentAllowed(ContentSettingsType::SENSORS);
    content_setting_bubble_model =
        ContentSettingBubbleModel::CreateContentSettingBubbleModel(
            NULL, web_contents(), ContentSettingsType::SENSORS);
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
                                       web_contents()->GetURL())));
    EXPECT_EQ(bubble_content_3.radio_group.default_item, 0);
  }

  content_settings->ClearContentSettingsExceptForNavigationRelatedSettings();

  // Go from block by default to allow by default to block by default.
  {
    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_BLOCK);
    content_settings->OnContentBlocked(ContentSettingsType::SENSORS);
    std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        ContentSettingBubbleModel::CreateContentSettingBubbleModel(
            NULL, web_contents(), ContentSettingsType::SENSORS));
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
                                       web_contents()->GetURL())));
    EXPECT_EQ(bubble_content.radio_group.radio_items[1],
              l10n_util::GetStringUTF16(IDS_BLOCKED_SENSORS_NO_ACTION));
    EXPECT_EQ(bubble_content.radio_group.default_item, 1);

    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_ALLOW);
    content_settings->OnContentAllowed(ContentSettingsType::SENSORS);
    content_setting_bubble_model =
        ContentSettingBubbleModel::CreateContentSettingBubbleModel(
            NULL, web_contents(), ContentSettingsType::SENSORS);
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
                                       web_contents()->GetURL())));
    EXPECT_EQ(bubble_content_2.radio_group.default_item, 0);

    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_BLOCK);
    content_settings->OnContentBlocked(ContentSettingsType::SENSORS);
    content_setting_bubble_model =
        ContentSettingBubbleModel::CreateContentSettingBubbleModel(
            NULL, web_contents(), ContentSettingsType::SENSORS);
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
                                       web_contents()->GetURL())));
    EXPECT_EQ(bubble_content_3.radio_group.radio_items[1],
              l10n_util::GetStringUTF16(IDS_BLOCKED_SENSORS_NO_ACTION));
    EXPECT_EQ(bubble_content_3.radio_group.default_item, 1);
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
    std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        ContentSettingBubbleModel::CreateContentSettingBubbleModel(
            NULL, web_contents(), ContentSettingsType::SENSORS));
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
                                       web_contents()->GetURL())));
    EXPECT_EQ(bubble_content.radio_group.default_item, 0);
  }

  content_settings->ClearContentSettingsExceptForNavigationRelatedSettings();
  // Clear site-specific exceptions.
  settings_map->ClearSettingsForOneType(ContentSettingsType::SENSORS);

  // Allow by default but block a specific site.
  {
    settings_map->SetDefaultContentSetting(ContentSettingsType::SENSORS,
                                           CONTENT_SETTING_ALLOW);
    settings_map->SetContentSettingDefaultScope(
        web_contents()->GetURL(), web_contents()->GetURL(),
        ContentSettingsType::SENSORS, std::string(), CONTENT_SETTING_BLOCK);
    content_settings->OnContentBlocked(ContentSettingsType::SENSORS);
    std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        ContentSettingBubbleModel::CreateContentSettingBubbleModel(
            NULL, web_contents(), ContentSettingsType::SENSORS));
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
                                       web_contents()->GetURL())));
    EXPECT_EQ(bubble_content.radio_group.radio_items[1],
              l10n_util::GetStringUTF16(IDS_BLOCKED_SENSORS_NO_ACTION));
    EXPECT_EQ(bubble_content.radio_group.default_item, 1);
  }
}

TEST_F(ContentSettingBubbleModelTest, PopupBubbleModelListItems) {
  const GURL url("https://www.example.test/");
  WebContentsTester::For(web_contents())->NavigateAndCommit(url);
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  content_settings->OnContentBlocked(ContentSettingsType::POPUPS);

  PopupBlockerTabHelper::CreateForWebContents(web_contents());
  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          nullptr, web_contents(), ContentSettingsType::POPUPS));
  const auto& list_items =
      content_setting_bubble_model->bubble_content().list_items;
  EXPECT_EQ(0U, list_items.size());

  BlockedWindowParams params(GURL("about:blank"), url::Origin(),
                             content::Referrer(), std::string(),
                             WindowOpenDisposition::NEW_POPUP,
                             blink::mojom::WindowFeatures(), false, true);
  constexpr size_t kItemCount = 3;
  for (size_t i = 1; i <= kItemCount; i++) {
    NavigateParams navigate_params =
        params.CreateNavigateParams(web_contents());
    EXPECT_TRUE(MaybeBlockPopup(web_contents(), &url, &navigate_params,
                                nullptr /*=open_url_params*/,
                                params.features()));
    EXPECT_EQ(i, list_items.size());
  }
}

TEST_F(ContentSettingBubbleModelTest, ValidUrl) {
  WebContentsTester::For(web_contents())->
      NavigateAndCommit(GURL("https://www.example.com"));

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  content_settings->OnContentBlocked(ContentSettingsType::COOKIES);

  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          NULL, web_contents(), ContentSettingsType::COOKIES));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();

  EXPECT_TRUE(bubble_content.radio_group.user_managed);
}

TEST_F(ContentSettingBubbleModelTest, InvalidUrl) {
  WebContentsTester::For(web_contents())->
      NavigateAndCommit(GURL("about:blank"));

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  content_settings->OnContentBlocked(ContentSettingsType::COOKIES);

  std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          NULL, web_contents(), ContentSettingsType::COOKIES));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();

  EXPECT_FALSE(bubble_content.radio_group.user_managed);
}
