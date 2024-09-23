// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/fake_owner.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/permissions/permission_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"

using content_settings::PageSpecificContentSettings;

class ContentSettingBubbleModelMediaStreamTest : public InProcessBrowserTest {
 public:
  void ManageMediaStreamSettings(
      PageSpecificContentSettings::MicrophoneCameraState state) {
    content::WebContents* original_tab = OpenTab();
    std::unique_ptr<ContentSettingBubbleModel> bubble = ShowBubble(state);

    // Click the manage button, which opens in a new tab or window. Wait until
    // it loads.
    bubble->OnManageButtonClicked();
    ASSERT_NE(GetActiveTab(), original_tab);
    content::TestNavigationObserver observer(GetActiveTab());
    observer.Wait();
  }

  std::unique_ptr<ContentSettingBubbleModel> ShowBubble(
      PageSpecificContentSettings::MicrophoneCameraState state) {
    content::WebContents* web_contents = GetActiveTab();

    // Create a bubble with the given camera and microphone access state.
    PageSpecificContentSettings::GetForFrame(
        web_contents->GetPrimaryMainFrame())
        ->OnMediaStreamPermissionSet(web_contents->GetLastCommittedURL(),
                                     state);
    return std::make_unique<ContentSettingMediaStreamBubbleModel>(
        browser()->content_setting_bubble_model_delegate(), web_contents);
  }

  content::WebContents* GetActiveTab() {
    // First, we need to find the active browser window. It should be at
    // the same desktop as the browser in which we invoked the bubble.
    Browser* active_browser = chrome::FindLastActive();
    return active_browser->tab_strip_model()->GetActiveWebContents();
  }

  content::WebContents* OpenTab() {
    // Open a tab for which we will invoke the media bubble.
    GURL url(
        https_server_->GetURL("/content_setting_bubble/mixed_script.html"));
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    return GetActiveTab();
  }

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeatures(
        {}, {content_settings::features::kLeftHandSideActivityIndicators});

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_->Start());
  }

  void BlockMediaStreamPermissionWithSupervision(
      const std::vector<ContentSettingsType>& types) {
    auto provider = std::make_unique<content_settings::MockProvider>();
    for (ContentSettingsType type : types) {
      provider->SetWebsiteSetting(
          ContentSettingsPattern::Wildcard(),
          ContentSettingsPattern::Wildcard(), type,
          base::Value(ContentSetting::CONTENT_SETTING_BLOCK),
          /*constraints=*/{},
          content_settings::PartitionKey::GetDefaultForTesting());
    }
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());
    content_settings::TestUtils::OverrideProvider(
        map, std::move(provider),
        content_settings::ProviderType::kSupervisedProvider);
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that clicking on the manage button in the media bubble opens the
// correct section of the settings UI. This test sometimes leaks memory,
// detected by linux_chromium_asan_rel_ng. See http://crbug/668693 for more
// info.
IN_PROC_BROWSER_TEST_F(ContentSettingBubbleModelMediaStreamTest,
                       DISABLED_ManageLink) {
  // For each of the three options, we click the manage button and check if the
  // active tab loads the correct internal url.

  // The microphone bubble links to microphone exceptions.
  ManageMediaStreamSettings({PageSpecificContentSettings::kMicrophoneAccessed});
  EXPECT_EQ(GURL("chrome://settings/contentExceptions#media-stream-mic"),
            GetActiveTab()->GetLastCommittedURL());

  // The bubble for both media devices links to the the first section of the
  // default media content settings, which is the microphone section.
  ManageMediaStreamSettings({PageSpecificContentSettings::kMicrophoneAccessed,
                             PageSpecificContentSettings::kCameraAccessed});
  EXPECT_EQ(GURL("chrome://settings/content#media-stream-mic"),
            GetActiveTab()->GetLastCommittedURL());

  // The camera bubble links to camera exceptions.
  ManageMediaStreamSettings({PageSpecificContentSettings::kCameraAccessed});
  EXPECT_EQ(GURL("chrome://settings/contentExceptions#media-stream-camera"),
            GetActiveTab()->GetLastCommittedURL());
}

// Tests that media bubble content matches the expect layout when the
// mic is not blocked on NTP
IN_PROC_BROWSER_TEST_F(ContentSettingBubbleModelMediaStreamTest,
                       BubbleContentForNonBlockedMic) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
          browser(), GURL(chrome::kChromeUINewTabURL), 1);
  content::WebContents::FromRenderFrameHost(main_rfh)->Focus();

  ASSERT_TRUE(main_rfh);
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            web_contents->GetLastCommittedURL());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabPageURL),
            main_rfh->GetLastCommittedOrigin().GetURL());

  // Create a bubble with the given camera and microphone access state.
  PageSpecificContentSettings::GetForFrame(web_contents->GetPrimaryMainFrame())
      ->OnMediaStreamPermissionSet(
          permissions::PermissionUtil::GetCanonicalOrigin(
              ContentSettingsType::MEDIASTREAM_MIC,
              web_contents->GetPrimaryMainFrame()
                  ->GetLastCommittedOrigin()
                  .GetURL(),
              web_contents->GetLastCommittedURL()),
          {PageSpecificContentSettings::kMicrophoneAccessed});
  std::unique_ptr<ContentSettingBubbleModel> mic_bubble =
      std::make_unique<ContentSettingMediaStreamBubbleModel>(
          browser()->content_setting_bubble_model_delegate(), web_contents);

  EXPECT_TRUE(mic_bubble->bubble_content().is_user_modifiable);
}

// Tests that media bubble content matches the expect layout when the
// microphone is blocked by a custodian.
IN_PROC_BROWSER_TEST_F(ContentSettingBubbleModelMediaStreamTest,
                       BubbleContentForSupervisedBlockedMic) {
  BlockMediaStreamPermissionWithSupervision(
      {ContentSettingsType::MEDIASTREAM_MIC});

  OpenTab();

  std::unique_ptr<ContentSettingBubbleModel> mic_and_camera_bubble =
      ShowBubble({PageSpecificContentSettings::kMicrophoneAccessed,
                  PageSpecificContentSettings::kCameraAccessed});
  EXPECT_TRUE(mic_and_camera_bubble->bubble_content().is_user_modifiable);

  std::unique_ptr<ContentSettingBubbleModel> mic_bubble =
      ShowBubble({PageSpecificContentSettings::kMicrophoneAccessed});
  EXPECT_FALSE(mic_bubble->bubble_content().is_user_modifiable);

  std::unique_ptr<ContentSettingBubbleModel> camera_bubble =
      ShowBubble({PageSpecificContentSettings::kCameraAccessed});
  EXPECT_TRUE(camera_bubble->bubble_content().is_user_modifiable);
}

// Tests that media bubble content matches the expect layout when the
// camera is blocked by a custodian.
IN_PROC_BROWSER_TEST_F(ContentSettingBubbleModelMediaStreamTest,
                       BubbleContentForSupervisedBlockedCamera) {
  BlockMediaStreamPermissionWithSupervision(
      {ContentSettingsType::MEDIASTREAM_CAMERA});

  OpenTab();

  std::unique_ptr<ContentSettingBubbleModel> mic_and_camera_bubble =
      ShowBubble({PageSpecificContentSettings::kMicrophoneAccessed,
                  PageSpecificContentSettings::kCameraAccessed});
  EXPECT_TRUE(mic_and_camera_bubble->bubble_content().is_user_modifiable);

  std::unique_ptr<ContentSettingBubbleModel> mic_bubble =
      ShowBubble({PageSpecificContentSettings::kMicrophoneAccessed});
  EXPECT_TRUE(mic_bubble->bubble_content().is_user_modifiable);

  std::unique_ptr<ContentSettingBubbleModel> camera_bubble =
      ShowBubble({PageSpecificContentSettings::kCameraAccessed});
  EXPECT_FALSE(camera_bubble->bubble_content().is_user_modifiable);
}

// Tests that media bubble content matches the expect layout when the
// mic and camera are blocked by a custodian.
IN_PROC_BROWSER_TEST_F(ContentSettingBubbleModelMediaStreamTest,
                       BubbleContentForSupervisedBlockedMicAndCamera) {
  BlockMediaStreamPermissionWithSupervision(
      {ContentSettingsType::MEDIASTREAM_CAMERA,
       ContentSettingsType::MEDIASTREAM_MIC});

  OpenTab();

  std::unique_ptr<ContentSettingBubbleModel> mic_and_camera_bubble =
      ShowBubble({PageSpecificContentSettings::kMicrophoneAccessed,
                  PageSpecificContentSettings::kCameraAccessed});
  EXPECT_FALSE(mic_and_camera_bubble->bubble_content().is_user_modifiable);

  std::unique_ptr<ContentSettingBubbleModel> mic_bubble =
      ShowBubble({PageSpecificContentSettings::kMicrophoneAccessed});
  EXPECT_FALSE(mic_bubble->bubble_content().is_user_modifiable);

  std::unique_ptr<ContentSettingBubbleModel> camera_bubble =
      ShowBubble({PageSpecificContentSettings::kCameraAccessed});
  EXPECT_FALSE(camera_bubble->bubble_content().is_user_modifiable);
}

// Tests that media bubble content includes camera PTZ when the permission has
// been granted to the website.
IN_PROC_BROWSER_TEST_F(ContentSettingBubbleModelMediaStreamTest,
                       BubbleContentIncludesCameraPanTiltZoom) {
  content::WebContents* web_contents = OpenTab();
  GURL url = web_contents->GetLastCommittedURL();

  // Do not grant camera PTZ permission to current tab.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(url, GURL(),
                                      ContentSettingsType::CAMERA_PAN_TILT_ZOOM,
                                      CONTENT_SETTING_ASK);

  // The mic & camera bubble content does not include camera PTZ.
  std::unique_ptr<ContentSettingBubbleModel> mic_and_camera_bubble =
      ShowBubble({PageSpecificContentSettings::kMicrophoneAccessed,
                  PageSpecificContentSettings::kCameraAccessed});
  EXPECT_EQ(mic_and_camera_bubble->bubble_content().radio_group.radio_items[0],
            l10n_util::GetStringFUTF16(
                IDS_ALLOWED_MEDIASTREAM_MIC_AND_CAMERA_NO_ACTION,
                url_formatter::FormatUrlForSecurityDisplay(url)));

  // The camera bubble content does not include camera PTZ.
  std::unique_ptr<ContentSettingBubbleModel> camera_bubble =
      ShowBubble({PageSpecificContentSettings::kCameraAccessed});
  EXPECT_EQ(camera_bubble->bubble_content().radio_group.radio_items[0],
            l10n_util::GetStringFUTF16(
                IDS_ALLOWED_MEDIASTREAM_CAMERA_NO_ACTION,
                url_formatter::FormatUrlForSecurityDisplay(url)));

  // Grant camera PTZ permission to current tab.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(url, GURL(),
                                      ContentSettingsType::CAMERA_PAN_TILT_ZOOM,
                                      CONTENT_SETTING_ALLOW);

  // The mic & camera bubble content includes camera PTZ.
  std::unique_ptr<ContentSettingBubbleModel> mic_and_camera_ptz_bubble =
      ShowBubble({PageSpecificContentSettings::kMicrophoneAccessed,
                  PageSpecificContentSettings::kCameraAccessed});
  EXPECT_EQ(
      mic_and_camera_ptz_bubble->bubble_content().radio_group.radio_items[0],
      l10n_util::GetStringFUTF16(
          IDS_ALLOWED_MEDIASTREAM_MIC_AND_CAMERA_PAN_TILT_ZOOM_NO_ACTION,
          url_formatter::FormatUrlForSecurityDisplay(url)));

  // The camera bubble content includes camera PTZ.
  std::unique_ptr<ContentSettingBubbleModel> camera_ptz_bubble =
      ShowBubble({PageSpecificContentSettings::kCameraAccessed});
  EXPECT_EQ(camera_ptz_bubble->bubble_content().radio_group.radio_items[0],
            l10n_util::GetStringFUTF16(
                IDS_ALLOWED_CAMERA_PAN_TILT_ZOOM_NO_ACTION,
                url_formatter::FormatUrlForSecurityDisplay(url)));
}

class ContentSettingBubbleModelPopupTest : public InProcessBrowserTest {
 protected:
  static constexpr int kDisallowButtonIndex = 1;

  void SetUpInProcessBrowserTestFixture() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_->Start());
  }
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

// Tests that each popup action is counted in the right bucket.
IN_PROC_BROWSER_TEST_F(ContentSettingBubbleModelPopupTest,
                       PopupsActionsCount){
  GURL url(https_server_->GetURL("/popup_blocker/popup-many.html"));
  base::HistogramTester histograms;
  histograms.ExpectTotalCount("ContentSettings.Popups", 0);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  histograms.ExpectBucketCount(
        "ContentSettings.Popups",
        content_settings::POPUPS_ACTION_DISPLAYED_BLOCKED_ICON_IN_OMNIBOX, 1);

  // Creates the ContentSettingPopupBubbleModel in order to emulate clicks.
  std::unique_ptr<ContentSettingBubbleModel> model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          browser()->content_setting_bubble_model_delegate(),
          browser()->tab_strip_model()->GetActiveWebContents(),
          ContentSettingsType::POPUPS));
  std::unique_ptr<FakeOwner> owner =
      FakeOwner::Create(*model, kDisallowButtonIndex);

  histograms.ExpectBucketCount(
        "ContentSettings.Popups",
        content_settings::POPUPS_ACTION_DISPLAYED_BUBBLE, 1);

  ui::MouseEvent click_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);

  model->OnListItemClicked(0, click_event);
  histograms.ExpectBucketCount(
        "ContentSettings.Popups",
        content_settings::POPUPS_ACTION_CLICKED_LIST_ITEM_CLICKED, 1);

  model->OnManageButtonClicked();
  histograms.ExpectBucketCount(
        "ContentSettings.Popups",
        content_settings::POPUPS_ACTION_CLICKED_MANAGE_POPUPS_BLOCKING, 1);

  owner->SetSelectedRadioOptionAndCommit(model->kAllowButtonIndex);
  histograms.ExpectBucketCount(
        "ContentSettings.Popups",
        content_settings::POPUPS_ACTION_SELECTED_ALWAYS_ALLOW_POPUPS_FROM, 1);

  histograms.ExpectTotalCount("ContentSettings.Popups", 5);
}
