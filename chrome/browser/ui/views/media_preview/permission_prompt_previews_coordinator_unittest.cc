// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/permission_prompt_previews_coordinator.h"

#include <optional>

#include "base/system/system_monitor.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "components/media_effects/test/fake_audio_service.h"
#include "components/media_effects/test/fake_video_capture_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kCameraId[] = "camera_id";
constexpr char kMicId[] = "mic_id";

std::string ViewTypeToDurationHistogramName(
    MediaCoordinator::ViewType view_type) {
  switch (view_type) {
    case MediaCoordinator::ViewType::kCameraOnly:
      return "MediaPreviews.UI.Permissions.Camera.Duration";
    case MediaCoordinator::ViewType::kMicOnly:
      return "MediaPreviews.UI.Permissions.Mic.Duration";
    case MediaCoordinator::ViewType::kBoth:
      return "MediaPreviews.UI.Permissions.CameraAndMic.Duration";
  }
}

}  // namespace

class PermissionPromptPreviewsCoordinatorTest : public TestWithBrowserView {
 protected:
  PermissionPromptPreviewsCoordinatorTest()
      : TestWithBrowserView(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void InitializeCoordinator(
      std::vector<std::string> requested_audio_capture_device_ids,
      std::vector<std::string> requested_video_capture_device_ids) {
    coordinator_.emplace(browser(), &parent_view_, /*index=*/0,
                         requested_audio_capture_device_ids,
                         requested_video_capture_device_ids);
  }

  void TearDown() override {
    coordinator_.reset();
    TestWithBrowserView::TearDown();
  }

  void ExpectDurationHistogramUpdate(int expected_bucket_min_value,
                                     MediaCoordinator::ViewType view_type) {
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

  base::HistogramTester histogram_tester_;
  views::View parent_view_;
  std::optional<PermissionPromptPreviewsCoordinator> coordinator_;
};

TEST_F(PermissionPromptPreviewsCoordinatorTest, EligibleCamerasandMics) {
  InitializeCoordinator({kMicId}, {kCameraId});
  auto view_type = coordinator_->GetViewTypeForTesting();
  ASSERT_EQ(view_type, MediaCoordinator::ViewType::kBoth);

  AdvanceClock(base::Seconds(2.5));
  coordinator_.reset();
  ExpectDurationHistogramUpdate(/*expected_bucket_min_value=*/2, view_type);
}

TEST_F(PermissionPromptPreviewsCoordinatorTest, EligibleCameras) {
  InitializeCoordinator({}, {kCameraId});
  auto view_type = coordinator_->GetViewTypeForTesting();
  ASSERT_EQ(view_type, MediaCoordinator::ViewType::kCameraOnly);

  AdvanceClock(base::Seconds(5));
  coordinator_.reset();
  ExpectDurationHistogramUpdate(/*expected_bucket_min_value=*/4, view_type);
}

TEST_F(PermissionPromptPreviewsCoordinatorTest, EligibleMics) {
  InitializeCoordinator({kMicId}, {});
  auto view_type = coordinator_->GetViewTypeForTesting();
  ASSERT_EQ(view_type, MediaCoordinator::ViewType::kMicOnly);

  AdvanceClock(base::Seconds(10));
  coordinator_.reset();
  ExpectDurationHistogramUpdate(/*expected_bucket_min_value=*/8, view_type);
}

using PermissionPromptPreviewsCoordinatorDeathTest =
    PermissionPromptPreviewsCoordinatorTest;

TEST_F(PermissionPromptPreviewsCoordinatorDeathTest, NoEligibleDevices) {
  EXPECT_CHECK_DEATH_WITH(InitializeCoordinator({}, {}), "");
}
