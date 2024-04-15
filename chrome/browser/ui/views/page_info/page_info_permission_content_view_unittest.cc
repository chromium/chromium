// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_permission_content_view.h"

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_FUCHSIA)

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/media_effects/test/fake_audio_service.h"
#include "components/media_effects/test/fake_video_capture_service.h"
#include "components/media_effects/test/scoped_media_device_info.h"
#include "components/permissions/permission_recovery_success_rate_tracker.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"

namespace {

constexpr char kCameraId[] = "camera_id";
constexpr char kCameraName[] = "camera_name";
constexpr char kCameraId2[] = "camera_id_2";
constexpr char kCameraName2[] = "camera_name_2";

constexpr char kMicId[] = "mic_id";
constexpr char kMicName[] = "mic_name";
constexpr char kGroupId[] = "group_id";
constexpr char kMicId2[] = "mic_id_2";
constexpr char kMicName2[] = "mic_name_2";
constexpr char kGroupId2[] = "group_id_2";

blink::mojom::MediaStreamType GetStreamTypeFromSettingsType(
    ContentSettingsType type) {
  switch (type) {
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      return blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE;
    case ContentSettingsType::MEDIASTREAM_MIC:
      return blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE;
    default:
      return blink::mojom::MediaStreamType::NO_SERVICE;
  }
}

}  // namespace

// Takes care of all setup needed to initialize page info permission content
// view (e.g. web contents, PageInfo, ...), as well as media previews (e.g.
// audio service, video service, ...).
class PageInfoPermissionContentViewTestMediaPreview
    : public TestWithBrowserView {
 protected:
  PageInfoPermissionContentViewTestMediaPreview() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kCameraMicPreview);
  }

  void SetUp() override {
    TestWithBrowserView::SetUp();
    base::test::TestFuture<void> mic_infos, camera_infos;
    audio_service_.SetOnRepliedWithInputDeviceDescriptionsCallback(
        mic_infos.GetCallback());
    video_service_.SetOnRepliedWithSourceInfosCallback(
        camera_infos.GetCallback());

    media_device_info_.emplace();

    ASSERT_TRUE(mic_infos.WaitAndClear());
    ASSERT_TRUE(camera_infos.WaitAndClear());
  }

  void InitializePageInfo(ContentSettingsType type) {
    auto url = GURL("http://www.example.com");
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    web_contents_tester_ = content::WebContentsTester::For(web_contents_.get());
    web_contents_tester_->SetMediaCaptureRawDeviceIdsOpened(
        GetStreamTypeFromSettingsType(type), {});
    permissions::PermissionRecoverySuccessRateTracker::CreateForWebContents(
        web_contents_.get());

    presenter_ui_ = std::make_unique<PageInfoUI>();
    auto delegate =
        std::make_unique<ChromePageInfoDelegate>(web_contents_.get());
    presenter_ = std::make_unique<PageInfo>(std::move(delegate),
                                            web_contents_.get(), url);
    base::RunLoop run_loop;
    presenter_->InitializeUiState(presenter_ui_.get(), run_loop.QuitClosure());
    run_loop.Run();

    presenter_->OnSitePermissionChanged(type, CONTENT_SETTING_BLOCK,
                                        url::Origin::Create(url),
                                        /*is_one_time=*/false);

    ui_delegate_ =
        std::make_unique<ChromePageInfoUiDelegate>(web_contents_.get(), url);

    page_info_ = std::make_unique<PageInfoPermissionContentView>(
        presenter_.get(), ui_delegate_.get(), type, web_contents_.get());
  }

  void TearDown() override {
    page_info_.reset();
    ui_delegate_.reset();
    presenter_.reset();
    presenter_ui_.reset();
    web_contents_tester_ = nullptr;
    web_contents_.reset();
    TestWithBrowserView::TearDown();
  }

  std::u16string GetExpectedCameraLabelText(size_t devices) {
    return l10n_util::GetStringFUTF16(IDS_SITE_SETTINGS_TYPE_CAMERA_WITH_COUNT,
                                      base::NumberToString16(devices));
  }

  std::u16string GetExpectedMicLabelText(size_t devices) {
    return l10n_util::GetStringFUTF16(IDS_SITE_SETTINGS_TYPE_MIC_WITH_COUNT,
                                      base::NumberToString16(devices));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  media_effects::ScopedFakeAudioService audio_service_;
  media_effects::ScopedFakeVideoCaptureService video_service_;
  std::optional<media_effects::ScopedMediaDeviceInfo> media_device_info_;

  std::unique_ptr<content::WebContents> web_contents_;
  raw_ptr<content::WebContentsTester> web_contents_tester_;
  std::unique_ptr<PageInfoUI> presenter_ui_;
  std::unique_ptr<PageInfo> presenter_;
  std::unique_ptr<ChromePageInfoUiDelegate> ui_delegate_;
  std::unique_ptr<PageInfoPermissionContentView> page_info_;
};

// Verify the device counter as well as the tooltip for the title label for page
// info camera subpage.
TEST_F(PageInfoPermissionContentViewTestMediaPreview, MediaPreviewCamera) {
  InitializePageInfo(ContentSettingsType::MEDIASTREAM_CAMERA);
  ASSERT_TRUE(page_info_->GetPreviewsCoordinatorForTesting());

  auto title_label = page_info_->GetTitleForTesting();
  ASSERT_TRUE(title_label);

  // TODO(b/332604136): Remove `base::RunLoop().RunUntilIdle()` here and
  // below, once fully migrate to use cached media device infos.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(title_label->GetText(), GetExpectedCameraLabelText(0));
  EXPECT_EQ(title_label->GetTooltipText(), std::u16string());

  ASSERT_TRUE(video_service_.AddFakeCameraBlocking({kCameraName, kCameraId}));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(title_label->GetText(), GetExpectedCameraLabelText(1));
  EXPECT_EQ(title_label->GetTooltipText(),
            base::UTF8ToUTF16(std::string(kCameraName)));

  ASSERT_TRUE(video_service_.AddFakeCameraBlocking({kCameraName2, kCameraId2}));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(title_label->GetText(), GetExpectedCameraLabelText(2));
  EXPECT_EQ(title_label->GetTooltipText(),
            base::UTF8ToUTF16(kCameraName + std::string("\n") + kCameraName2));

  ASSERT_TRUE(video_service_.RemoveFakeCameraBlocking(kCameraId2));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(title_label->GetText(), GetExpectedCameraLabelText(1));
  EXPECT_EQ(title_label->GetTooltipText(),
            base::UTF8ToUTF16(std::string(kCameraName)));
}

// Verify the device counter as well as the tooltip for the title label for page
// info mic subpage.
TEST_F(PageInfoPermissionContentViewTestMediaPreview, MediaPreviewMic) {
  InitializePageInfo(ContentSettingsType::MEDIASTREAM_MIC);
  ASSERT_TRUE(page_info_->GetPreviewsCoordinatorForTesting());

  auto title_label = page_info_->GetTitleForTesting();
  ASSERT_TRUE(title_label);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(title_label->GetText(), GetExpectedMicLabelText(0));
  EXPECT_EQ(title_label->GetTooltipText(), std::u16string());

  ASSERT_TRUE(
      audio_service_.AddFakeInputDeviceBlocking({kMicName, kMicId, kGroupId}));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(title_label->GetText(), GetExpectedMicLabelText(1));
  EXPECT_EQ(title_label->GetTooltipText(),
            base::UTF8ToUTF16(std::string(kMicName)));

  ASSERT_TRUE(audio_service_.AddFakeInputDeviceBlocking(
      {kMicName2, kMicId2, kGroupId2}));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(title_label->GetText(), GetExpectedMicLabelText(2));
  EXPECT_EQ(title_label->GetTooltipText(),
            base::UTF8ToUTF16(kMicName + std::string("\n") + kMicName2));

  ASSERT_TRUE(audio_service_.RemoveFakeInputDeviceBlocking(kMicId));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(title_label->GetText(), GetExpectedMicLabelText(1));
  EXPECT_EQ(title_label->GetTooltipText(),
            base::UTF8ToUTF16(std::string(kMicName2)));
}

// Verify there is no preview created when there is no camera or mic permissions
// requested.
TEST_F(PageInfoPermissionContentViewTestMediaPreview,
       MediaPreviewNoCameraOrMic) {
  InitializePageInfo(ContentSettingsType::GEOLOCATION);
  ASSERT_FALSE(page_info_->GetPreviewsCoordinatorForTesting());
}

#endif
