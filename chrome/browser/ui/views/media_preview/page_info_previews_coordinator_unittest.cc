// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/page_info_previews_coordinator.h"

#include <optional>

#include "base/system/system_monitor.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/media_preview/media_view.h"
#include "components/media_effects/test/fake_audio_service.h"
#include "components/media_effects/test/fake_video_capture_service.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

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

std::string ViewTypeToDurationHistogramName(
    MediaCoordinator::ViewType view_type) {
  switch (view_type) {
    case MediaCoordinator::ViewType::kCameraOnly:
      return "MediaPreviews.UI.PageInfo.Camera.Duration";
    case MediaCoordinator::ViewType::kMicOnly:
      return "MediaPreviews.UI.PageInfo.Mic.Duration";
    default:
      return "UnmappedViewType";
  }
}

}  // namespace

class PageInfoPreviewsCoordinatorTest : public TestWithBrowserView {
 protected:
  PageInfoPreviewsCoordinatorTest()
      : TestWithBrowserView(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void InitializeCoordinator(ContentSettingsType content_settings_type) {
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    web_contents_tester_ = content::WebContentsTester::For(web_contents_.get());
    web_contents_tester_->SetMediaCaptureRawDeviceIdsOpened(
        GetStreamTypeFromSettingsType(content_settings_type), {});

    coordinator_.emplace(web_contents_.get(), content_settings_type,
                         &parent_view_);
  }

  void TearDown() override {
    coordinator_.reset();
    web_contents_tester_ = nullptr;
    web_contents_.reset();
    TestWithBrowserView::TearDown();
  }

  void ExpectDurationHistogramUpdate(MediaCoordinator::ViewType view_type,
                                     int expected_bucket_min_value) {
    auto duration_histogram_name = ViewTypeToDurationHistogramName(view_type);
    histogram_tester_.ExpectUniqueSample(duration_histogram_name,
                                         expected_bucket_min_value,
                                         /*expected_bucket_count=*/1);
  }

  void AdvanceClock(base::TimeDelta delta) {
    task_environment()->AdvanceClock(delta);
  }

  base::SystemMonitor system_monitor_;
  media_effects::ScopedFakeAudioService audio_service_;
  media_effects::ScopedFakeVideoCaptureService video_service_;

  std::unique_ptr<content::WebContents> web_contents_;
  raw_ptr<content::WebContentsTester> web_contents_tester_;

  base::HistogramTester histogram_tester_;
  MediaView parent_view_;
  std::optional<PageInfoPreviewsCoordinator> coordinator_;
};

TEST_F(PageInfoPreviewsCoordinatorTest, MediaStreamCamera) {
  InitializeCoordinator(ContentSettingsType::MEDIASTREAM_CAMERA);
  auto view_type = coordinator_->GetViewTypeForTesting();
  ASSERT_EQ(view_type, MediaCoordinator::ViewType::kCameraOnly);

  AdvanceClock(base::Seconds(5));
  coordinator_.reset();
  ExpectDurationHistogramUpdate(view_type, /*expected_bucket_min_value=*/4);
}

TEST_F(PageInfoPreviewsCoordinatorTest, MediaStreamMic) {
  InitializeCoordinator(ContentSettingsType::MEDIASTREAM_MIC);
  auto view_type = coordinator_->GetViewTypeForTesting();
  ASSERT_EQ(view_type, MediaCoordinator::ViewType::kMicOnly);

  AdvanceClock(base::Seconds(10));
  coordinator_.reset();
  ExpectDurationHistogramUpdate(view_type, /*expected_bucket_min_value=*/8);
}

using PageInfoPreviewsCoordinatorDeathTest = PageInfoPreviewsCoordinatorTest;

TEST_F(PageInfoPreviewsCoordinatorDeathTest, NeitherCameraNorMic) {
  EXPECT_CHECK_DEATH_WITH(InitializeCoordinator(ContentSettingsType::COOKIES),
                          "");
}
