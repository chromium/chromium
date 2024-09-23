// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/media_coordinator.h"

#include "chrome/browser/media/prefs/capture_device_ranking.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/media_preview/media_preview_metrics.h"
#include "chrome/browser/ui/views/media_preview/media_view.h"
#include "components/media_effects/test/fake_audio_service.h"
#include "components/media_effects/test/fake_video_capture_service.h"
#include "components/media_effects/test/scoped_media_device_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kCameraId1[] = "camera_id_1";
constexpr char kCameraName1[] = "camera_name_1";
constexpr char kCameraId2[] = "camera_id_2";
constexpr char kCameraName2[] = "camera_name_2";

constexpr char kMicId1[] = "mic_id_1";
constexpr char kMicName1[] = "mic_name_1";
constexpr char kGroupId1[] = "group_id_1";
constexpr char kMicId2[] = "mic_id_2";
constexpr char kMicName2[] = "mic_name_2";
constexpr char kGroupId2[] = "group_id_2";

media_preview_metrics::Context GetMetricsContext(
    MediaCoordinator::ViewType type) {
  return {media_preview_metrics::UiLocation::kPermissionPrompt,
          media_coordinator::GetPreviewTypeFromMediaCoordinatorViewType(type)};
}

MATCHER(VideoCaptureDeviceInfoEq, "") {
  return std::get<0>(arg).descriptor.device_id ==
         std::get<1>(arg).descriptor.device_id;
}

}  // namespace

using testing::ElementsAre;
using testing::Pointwise;

class MediaCoordinatorTest : public TestWithBrowserView {
 protected:
  void SetUp() override {
    TestWithBrowserView::SetUp();
    media_device_info_.emplace();
  }

  void InitializeCoordinator(MediaCoordinator::ViewType view_type) {
    coordinator_.emplace(
        view_type, parent_view_, /*is_subsection=*/false,
        MediaCoordinator::EligibleDevices{}, *profile()->GetPrefs(),
        /*allow_device_selection=*/true, GetMetricsContext(view_type));
  }

  void TearDown() override {
    coordinator_.reset();
    TestWithBrowserView::TearDown();
  }

  void AddCameras() {
    ASSERT_TRUE(
        video_service_.AddFakeCameraBlocking({kCameraName1, kCameraId1}));
    ASSERT_TRUE(
        video_service_.AddFakeCameraBlocking({kCameraName2, kCameraId2}));
  }

  void AddMics() {
    ASSERT_TRUE(audio_service_.AddFakeInputDeviceBlocking(
        {kMicName1, kMicId1, kGroupId1}));
    ASSERT_TRUE(audio_service_.AddFakeInputDeviceBlocking(
        {kMicName2, kMicId2, kGroupId2}));
  }

  media_effects::ScopedFakeAudioService audio_service_;
  media_effects::ScopedFakeVideoCaptureService video_service_;
  std::optional<media_effects::ScopedMediaDeviceInfo> media_device_info_;

  MediaView parent_view_;
  std::optional<MediaCoordinator> coordinator_;
};

TEST_F(MediaCoordinatorTest, CamerasRankingUpdate) {
  InitializeCoordinator(MediaCoordinator::ViewType::kCameraOnly);
  AddCameras();

  const media::VideoCaptureDeviceInfo kCamera1{{kCameraName1, kCameraId1}};
  const media::VideoCaptureDeviceInfo kCamera2{{kCameraName2, kCameraId2}};

  // Preference ranking defaults to noop.
  std::vector camera_infos{kCamera2, kCamera1};
  media_prefs::PreferenceRankVideoDeviceInfos(*profile()->GetPrefs(),
                                              camera_infos);
  EXPECT_THAT(camera_infos,
              Pointwise(VideoCaptureDeviceInfoEq(), {kCamera2, kCamera1}));

  // Ranking is updated to make the currently selected device most preferred.
  coordinator_->UpdateDevicePreferenceRanking();
  media_prefs::PreferenceRankVideoDeviceInfos(*profile()->GetPrefs(),
                                              camera_infos);
  EXPECT_THAT(camera_infos,
              Pointwise(VideoCaptureDeviceInfoEq(), {kCamera1, kCamera2}));
}

TEST_F(MediaCoordinatorTest, MicsRankingUpdate) {
  InitializeCoordinator(MediaCoordinator::ViewType::kMicOnly);
  AddMics();

  const media::AudioDeviceDescription kMic1{kMicName1, kMicId1, kGroupId1};
  const media::AudioDeviceDescription kMic2{kMicName2, kMicId2, kGroupId2};

  // Preference ranking defaults to noop.
  std::vector mic_infos{kMic2, kMic1};
  media_prefs::PreferenceRankAudioDeviceInfos(*profile()->GetPrefs(),
                                              mic_infos);
  EXPECT_THAT(mic_infos, ElementsAre(kMic2, kMic1));

  // Ranking is updated to make the currently selected device most preferred.
  coordinator_->UpdateDevicePreferenceRanking();
  media_prefs::PreferenceRankAudioDeviceInfos(*profile()->GetPrefs(),
                                              mic_infos);
  EXPECT_THAT(mic_infos, ElementsAre(kMic1, kMic2));
}

TEST_F(MediaCoordinatorTest, CamerasAndMicsRankingUpdate) {
  InitializeCoordinator(MediaCoordinator::ViewType::kBoth);
  AddCameras();
  AddMics();

  const media::VideoCaptureDeviceInfo kCamera1{{kCameraName1, kCameraId1}};
  const media::VideoCaptureDeviceInfo kCamera2{{kCameraName2, kCameraId2}};
  const media::AudioDeviceDescription kMic1{kMicName1, kMicId1, kGroupId1};
  const media::AudioDeviceDescription kMic2{kMicName2, kMicId2, kGroupId2};

  // Preference ranking defaults to noop.
  std::vector camera_infos{kCamera2, kCamera1};
  std::vector mic_infos{kMic2, kMic1};
  media_prefs::PreferenceRankVideoDeviceInfos(*profile()->GetPrefs(),
                                              camera_infos);
  EXPECT_THAT(camera_infos,
              Pointwise(VideoCaptureDeviceInfoEq(), {kCamera2, kCamera1}));
  media_prefs::PreferenceRankAudioDeviceInfos(*profile()->GetPrefs(),
                                              mic_infos);
  EXPECT_THAT(mic_infos, ElementsAre(kMic2, kMic1));

  // Ranking is updated to make the currently selected device most preferred.
  coordinator_->UpdateDevicePreferenceRanking();
  media_prefs::PreferenceRankVideoDeviceInfos(*profile()->GetPrefs(),
                                              camera_infos);
  EXPECT_THAT(camera_infos,
              Pointwise(VideoCaptureDeviceInfoEq(), {kCamera1, kCamera2}));
  media_prefs::PreferenceRankAudioDeviceInfos(*profile()->GetPrefs(),
                                              mic_infos);
  EXPECT_THAT(mic_infos, ElementsAre(kMic1, kMic2));
}
