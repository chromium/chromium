// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_renderer_host.h"
#include "net/cookies/cookie_options.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_palette.h"

namespace {

bool HasIcon(const ContentSettingImageModel& model) {
  return !model.GetIcon(gfx::kPlaceholderColor).IsEmpty();
}

// Forward all NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED to the specified
// ContentSettingImageModel.
class NotificationForwarder : public content::NotificationObserver {
 public:
  explicit NotificationForwarder(ContentSettingImageModel* model)
      : model_(model) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
                   content::NotificationService::AllSources());
  }
  ~NotificationForwarder() override {}

  void clear() {
    registrar_.RemoveAll();
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    if (type == chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED) {
      model_->UpdateFromWebContents(
          content::Source<content::WebContents>(source).ptr());
    }
  }

 private:
  content::NotificationRegistrar registrar_;
  ContentSettingImageModel* model_;

  DISALLOW_COPY_AND_ASSIGN(NotificationForwarder);
};

class ContentSettingImageModelTest : public ChromeRenderViewHostTestHarness {
};

TEST_F(ContentSettingImageModelTest, UpdateFromWebContents) {
  TabSpecificContentSettings::CreateForWebContents(web_contents());
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::IMAGES);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_TRUE(content_setting_image_model->get_tooltip().empty());

  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES);
  content_setting_image_model->UpdateFromWebContents(web_contents());

  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_TRUE(HasIcon(*content_setting_image_model));
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
}

TEST_F(ContentSettingImageModelTest, RPHUpdateFromWebContents) {
  TabSpecificContentSettings::CreateForWebContents(web_contents());
  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::PROTOCOL_HANDLERS);
  content_setting_image_model->UpdateFromWebContents(web_contents());
  EXPECT_FALSE(content_setting_image_model->is_visible());

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  content_settings->set_pending_protocol_handler(
      ProtocolHandler::CreateProtocolHandler(
          "mailto", GURL("http://www.google.com/")));
  content_setting_image_model->UpdateFromWebContents(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
}

TEST_F(ContentSettingImageModelTest, CookieAccessed) {
  TabSpecificContentSettings::CreateForWebContents(web_contents());
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES,
                                 CONTENT_SETTING_BLOCK);
  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::COOKIES);
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_TRUE(content_setting_image_model->get_tooltip().empty());

  net::CookieOptions options;
  GURL origin("http://google.com");
  std::unique_ptr<net::CanonicalCookie> cookie(
      net::CanonicalCookie::Create(origin, "A=B", base::Time::Now(), options));
  ASSERT_TRUE(cookie);
  content_settings->OnCookieChange(origin, origin, *cookie, false);
  content_setting_image_model->UpdateFromWebContents(web_contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_TRUE(HasIcon(*content_setting_image_model));
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
}

// Regression test for http://crbug.com/161854.
TEST_F(ContentSettingImageModelTest, NULLTabSpecificContentSettings) {
  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::IMAGES);
  NotificationForwarder forwarder(content_setting_image_model.get());
  // Should not crash.
  TabSpecificContentSettings::CreateForWebContents(web_contents());
  forwarder.clear();
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

  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_ADS);
  content_setting_image_model->UpdateFromWebContents(web_contents());

  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_TRUE(HasIcon(*content_setting_image_model));
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
}

}  // namespace
