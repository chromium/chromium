// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/content_setting_image_model.h"

#import <AVFoundation/AVFoundation.h>

#include "base/mac/mac_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/content_settings/media_authorization_wrapper_test.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

namespace gfx {
struct VectorIcon;
}

namespace {

bool HasIcon(const ContentSettingImageModel& model) {
  return !model.GetIcon(gfx::kPlaceholderColor).IsEmpty();
}

void ExpectImageModelState(const ContentSettingImageModel& model,
                           bool is_visible,
                           bool has_icon,
                           const base::string16& tooltip,
                           int explanatory_string_id,
                           const gfx::VectorIcon* icon_badge) {
  EXPECT_EQ(model.is_visible(), is_visible);
  EXPECT_EQ(HasIcon(model), has_icon);
  EXPECT_EQ(model.get_tooltip(), tooltip);
  EXPECT_EQ(model.explanatory_string_id(), explanatory_string_id);
  EXPECT_EQ(model.get_icon_badge(), icon_badge);
}

class ContentSettingMediaImageModelTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    TabSpecificContentSettings::CreateForWebContents(web_contents());
    InfoBarService::CreateForWebContents(web_contents());
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

TEST_F(ContentSettingMediaImageModelTest, MediaUpdate) {
  if (!base::mac::IsAtLeastOS10_14())
    return;

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kMacSystemMediaPermissionsInfoUi);

  TabSpecificContentSettings::CreateForWebContents(web_contents());
  auto* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  const GURL kTestOrigin("https://www.example.com");
  auto content_setting_image_model =
      ContentSettingImageModel::CreateForContentType(
          ContentSettingImageModel::ImageType::MEDIASTREAM);
  MediaAuthorizationWrapperTest auth_wrapper;
  system_media_permissions::SetMediaAuthorizationWrapperForTesting(
      &auth_wrapper);

  // Camera allowed per site: Test for system level permissions.
  {
    content_settings->OnMediaStreamPermissionSet(
        kTestOrigin, TabSpecificContentSettings::CAMERA_ACCESSED, std::string(),
        GetDefaultVideoDevice(), std::string(), std::string());
    auth_wrapper.SetMockMediaPermissionStatus(kAllowed);
    content_setting_image_model->Update(web_contents());
    ExpectImageModelState(
        *content_setting_image_model, true /*is_visible*/, true /*has_icon*/,
        l10n_util::GetStringUTF16(IDS_CAMERA_ACCESSED), 0, &gfx::kNoneIcon);
    auth_wrapper.SetMockMediaPermissionStatus(kDenied);
    content_setting_image_model->Update(web_contents());
    ExpectImageModelState(*content_setting_image_model, true /*is_visible*/,
                          true /*has_icon*/,
                          l10n_util::GetStringUTF16(IDS_CAMERA_BLOCKED),
                          IDS_CAMERA_TURNED_OFF, &kBlockedBadgeIcon);
    auth_wrapper.SetMockMediaPermissionStatus(kNotDetermined);
    content_setting_image_model->Update(web_contents());
    EXPECT_FALSE(content_setting_image_model->is_visible());
  }

  // Microphone allowed per site: Test for system level permissions.
  {
    content_settings->OnMediaStreamPermissionSet(
        kTestOrigin, TabSpecificContentSettings::MICROPHONE_ACCESSED,
        std::string(), GetDefaultVideoDevice(), std::string(), std::string());
    auth_wrapper.SetMockMediaPermissionStatus(kAllowed);
    content_setting_image_model->Update(web_contents());
    ExpectImageModelState(
        *content_setting_image_model, true /*is_visible*/, true /*has_icon*/,
        l10n_util::GetStringUTF16(IDS_MICROPHONE_ACCESSED), 0, &gfx::kNoneIcon);
    auth_wrapper.SetMockMediaPermissionStatus(kDenied);
    content_setting_image_model->Update(web_contents());
    ExpectImageModelState(*content_setting_image_model, true /*is_visible*/,
                          true /*has_icon*/,
                          l10n_util::GetStringUTF16(IDS_MICROPHONE_BLOCKED),
                          IDS_MIC_TURNED_OFF, &kBlockedBadgeIcon);
    auth_wrapper.SetMockMediaPermissionStatus(kNotDetermined);
    content_setting_image_model->Update(web_contents());
    EXPECT_FALSE(content_setting_image_model->is_visible());
  }

  // Microphone & camera allowed per site: Test for system level permissions.
  {
    content_settings->OnMediaStreamPermissionSet(
        kTestOrigin,
        (TabSpecificContentSettings::MICROPHONE_ACCESSED |
         TabSpecificContentSettings::CAMERA_ACCESSED),
        std::string(), GetDefaultVideoDevice(), std::string(), std::string());
    auth_wrapper.SetMockMediaPermissionStatus(kAllowed);
    auth_wrapper.SetMockMediaPermissionStatus(kAllowed);
    content_setting_image_model->Update(web_contents());
    ExpectImageModelState(
        *content_setting_image_model, true /*is_visible*/, true /*has_icon*/,
        l10n_util::GetStringUTF16(IDS_MICROPHONE_CAMERA_ALLOWED), 0,
        &gfx::kNoneIcon);
    auth_wrapper.SetMockMediaPermissionStatus(kDenied);
    auth_wrapper.SetMockMediaPermissionStatus(kDenied);
    content_setting_image_model->Update(web_contents());
    ExpectImageModelState(
        *content_setting_image_model, true /*is_visible*/, true /*has_icon*/,
        l10n_util::GetStringUTF16(IDS_MICROPHONE_CAMERA_BLOCKED),
        IDS_CAMERA_TURNED_OFF, &kBlockedBadgeIcon);
    auth_wrapper.SetMockMediaPermissionStatus(kNotDetermined);
    auth_wrapper.SetMockMediaPermissionStatus(kNotDetermined);
    content_setting_image_model->Update(web_contents());
    EXPECT_EQ(content_setting_image_model->is_visible(), false);
  }

  // Test that system permissions being allowed do not affect the image view,
  // when the per site permission is denied.
  for (const auto system_state : {kAllowed, kDenied}) {
    SCOPED_TRACE(system_state);
    auth_wrapper.SetMockMediaPermissionStatus(system_state);
    auth_wrapper.SetMockMediaPermissionStatus(system_state);

    // Camera blocked per site.
    {
      content_settings->OnMediaStreamPermissionSet(
          kTestOrigin,
          TabSpecificContentSettings::CAMERA_ACCESSED |
              TabSpecificContentSettings::CAMERA_BLOCKED,
          GetDefaultAudioDevice(), GetDefaultVideoDevice(), std::string(),
          std::string());
      content_setting_image_model->Update(web_contents());
      ExpectImageModelState(
          *content_setting_image_model, true /*is_visible*/, true /*has_icon*/,
          l10n_util::GetStringUTF16(IDS_CAMERA_BLOCKED), 0, &kBlockedBadgeIcon);
    }

    // Microphone blocked per site.
    {
      content_settings->OnMediaStreamPermissionSet(
          kTestOrigin,
          TabSpecificContentSettings::MICROPHONE_ACCESSED |
              TabSpecificContentSettings::MICROPHONE_BLOCKED,
          GetDefaultAudioDevice(), GetDefaultVideoDevice(), std::string(),
          std::string());
      content_setting_image_model->Update(web_contents());
      ExpectImageModelState(*content_setting_image_model, true /*is_visible*/,
                            true /*has_icon*/,
                            l10n_util::GetStringUTF16(IDS_MICROPHONE_BLOCKED),
                            0, &kBlockedBadgeIcon);
    }

    // Microphone & camera blocked per site
    {
      content_settings->OnMediaStreamPermissionSet(
          kTestOrigin,
          TabSpecificContentSettings::CAMERA_ACCESSED |
              TabSpecificContentSettings::CAMERA_BLOCKED |
              TabSpecificContentSettings::MICROPHONE_ACCESSED |
              TabSpecificContentSettings::MICROPHONE_BLOCKED,
          GetDefaultAudioDevice(), GetDefaultVideoDevice(), std::string(),
          std::string());
      content_setting_image_model->Update(web_contents());
      ExpectImageModelState(
          *content_setting_image_model, true /*is_visible*/, true /*has_icon*/,
          l10n_util::GetStringUTF16(IDS_MICROPHONE_CAMERA_BLOCKED), 0,
          &kBlockedBadgeIcon);
    }
  }
}

}  // namespace
