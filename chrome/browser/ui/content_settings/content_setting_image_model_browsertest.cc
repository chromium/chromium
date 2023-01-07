// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/content_setting_image_model.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"

using content::WebContents;
using ImageType = ContentSettingImageModel::ImageType;

typedef InProcessBrowserTest ContentSettingImageModelBrowserTest;

// Tests that every model creates a valid bubble.
IN_PROC_BROWSER_TEST_F(ContentSettingImageModelBrowserTest, CreateBubbleModel) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content_settings::PageSpecificContentSettings* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents->GetPrimaryMainFrame());
  content_settings->BlockAllContentForTesting();

  // Automatic downloads are handled by DownloadRequestLimiter.
  DownloadRequestLimiter::TabDownloadState* tab_download_state =
      g_browser_process->download_request_limiter()->GetDownloadState(
          web_contents, true);
  tab_download_state->set_download_seen();
  tab_download_state->SetDownloadStatusAndNotify(
      url::Origin::Create(web_contents->GetVisibleURL()),
      DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED);

  // Test that image models tied to a single content setting create bubbles tied
  // to the same setting.
  static constexpr ContentSettingImageModel::ImageType
      content_settings_to_test[] = {
          ImageType::COOKIES,
          ImageType::IMAGES,
          ImageType::JAVASCRIPT,
          ImageType::POPUPS,
          ImageType::MIXEDSCRIPT,
          ImageType::PROTOCOL_HANDLERS,
          ImageType::MIDI_SYSEX,
      };

  for (auto type : content_settings_to_test) {
    auto model = ContentSettingImageModel::CreateForContentType(type);
    std::unique_ptr<ContentSettingBubbleModel> bubble(
        model->CreateBubbleModel(nullptr, web_contents));

    // All of the above content settings should create a
    // ContentSettingSimpleBubbleModel that is tied to a particular setting,
    // and thus it should be an instance of ContentSettingSimpleBubbleModel.
    ContentSettingSimpleBubbleModel* simple_bubble =
        bubble->AsSimpleBubbleModel();
    ASSERT_TRUE(simple_bubble);
    EXPECT_EQ(static_cast<ContentSettingSimpleImageModel*>(model.get())
                  ->content_type(),
              simple_bubble->content_type());
    EXPECT_EQ(type, model->image_type());
  }

  // For other models, we can only test that they create a valid bubble, and
  // that all the image types are unique.
  std::set<ImageType> image_types;
  std::vector<std::unique_ptr<ContentSettingImageModel>> models =
      ContentSettingImageModel::GenerateContentSettingImageModels();
  for (auto& model : models) {
    EXPECT_TRUE(model->CreateBubbleModel(nullptr, web_contents));
    EXPECT_TRUE(image_types.insert(model->image_type()).second);
  }
}

// Tests that we correctly remember for which WebContents the animation has run,
// and thus we should not run it again.
IN_PROC_BROWSER_TEST_F(ContentSettingImageModelBrowserTest,
                       ShouldRunAnimation) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  auto model =
      ContentSettingImageModel::CreateForContentType(ImageType::IMAGES);

  EXPECT_TRUE(model->ShouldRunAnimation(web_contents));
  model->SetAnimationHasRun(web_contents);
  EXPECT_FALSE(model->ShouldRunAnimation(web_contents));

  // The animation has run for the current WebContents, but not for any other.
  Profile* profile = browser()->profile();
  WebContents::CreateParams create_params(profile);
  std::unique_ptr<WebContents> other_web_contents =
      WebContents::Create(create_params);
  content::WebContents* raw_other_web_contents = other_web_contents.get();
  browser()->tab_strip_model()->AppendWebContents(std::move(other_web_contents),
                                                  true);
  EXPECT_TRUE(model->ShouldRunAnimation(raw_other_web_contents));
}

// Tests that we go to the correct link when learn more is clicked in Ads
// bubble.
IN_PROC_BROWSER_TEST_F(ContentSettingImageModelBrowserTest,
                       AdsLearnMoreLinkClicked) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  auto model = ContentSettingImageModel::CreateForContentType(ImageType::ADS);
  std::unique_ptr<ContentSettingBubbleModel> bubble(model->CreateBubbleModel(
      browser()->content_setting_bubble_model_delegate(), web_contents));

  content::TestNavigationObserver observer(nullptr);
  observer.StartWatchingNewWebContents();
  bubble->OnLearnMoreClicked();
  observer.Wait();

  std::string link_value(subresource_filter::kLearnMoreLink);
  EXPECT_EQ(link_value, observer.last_navigation_url().spec());
}
