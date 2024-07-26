// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/camera_coordinator.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/media/prefs/capture_device_ranking.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_effects/test/fake_audio_service.h"
#include "components/media_effects/test/fake_video_capture_service.h"
#include "components/media_effects/test/scoped_media_device_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

using testing::_;
using testing::Pointwise;

namespace {

constexpr char kNumDevicesHistogram[] =
    "MediaPreviews.UI.DeviceSelection.Permissions.Camera.NumDevices";
constexpr char kDeviceId[] = "device_id";
constexpr char kDeviceName[] = "device_name";
constexpr char kDeviceId2[] = "device_id_2";
constexpr char kDeviceName2[] = "device_name_2";

media_preview_metrics::Context GetMetricsContext() {
  // Camera coordinator is expected to narrow preview type to kCamera.
  // This is verified in ExpectHistogramTotalDevices() below.
  return {media_preview_metrics::UiLocation::kPermissionPrompt,
          media_preview_metrics::PreviewType::kCameraAndMic};
}

MATCHER_P(HasItems, items, "") {
  if (arg.GetItemCount() != items.size()) {
    *result_listener << "item count is " << arg.GetItemCount();
    return false;
  }

  for (size_t i = 0; i < items.size(); ++i) {
    if (base::UTF8ToUTF16(items[i]) != arg.GetItemAt(i)) {
      *result_listener << "item at index " << i << " is " << arg.GetItemAt(i);
      return false;
    }
  }

  return true;
}

MATCHER(VideoCaptureDeviceInfoEq, "") {
  return std::get<0>(arg).descriptor.device_id ==
         std::get<1>(arg).descriptor.device_id;
}

}  // namespace

class CameraCoordinatorTest : public TestWithBrowserView {
 protected:
  void SetUp() override {
    TestWithBrowserView::SetUp();
    fake_video_capture_service_.SetOnGetVideoSourceCallback(
        on_get_video_source_future_.GetRepeatingCallback());
    histogram_tester_.emplace();

    parent_view_.emplace();
    InitializeCoordinator(/*eligible_camera_ids=*/{});
  }

  void TearDown() override {
    coordinator_.reset();
    parent_view_.reset();
    TestWithBrowserView::TearDown();
  }

  void InitializeCoordinator(std::vector<std::string> eligible_camera_ids) {
    CHECK(profile()->GetPrefs());

    if (!media_device_info_) {
      media_device_info_.emplace();
    }

    coordinator_.emplace(*parent_view_,
                         /*needs_borders=*/true, eligible_camera_ids,
                         *profile()->GetPrefs(),
                         /*allow_device_selection=*/true, GetMetricsContext());
  }

  const ui::SimpleComboboxModel& GetComboboxModel() const {
    return coordinator_->GetComboboxModelForTest();
  }

  bool AddFakeCamera(const media::VideoCaptureDeviceDescriptor& descriptor) {
    return fake_video_capture_service_.AddFakeCameraBlocking(descriptor);
  }

  bool RemoveFakeCamera(const std::string& device_id) {
    return fake_video_capture_service_.RemoveFakeCameraBlocking(device_id);
  }

  void ExpectHistogramTotalDevices(size_t expected_bucket_min_value) {
    histogram_tester_->ExpectUniqueSample(kNumDevicesHistogram,
                                          expected_bucket_min_value,
                                          /*expected_bucket_count=*/1);
    histogram_tester_.emplace();
  }

  media_effects::ScopedFakeAudioService fake_audio_capture_service_;
  media_effects::ScopedFakeVideoCaptureService fake_video_capture_service_;
  std::optional<media_effects::ScopedMediaDeviceInfo> media_device_info_;

  std::optional<base::HistogramTester> histogram_tester_;
  std::optional<views::View> parent_view_;
  std::optional<CameraCoordinator> coordinator_;

  base::test::TestFuture<
      const std::string&,
      mojo::PendingReceiver<video_capture::mojom::VideoSource>>
      on_get_video_source_future_;
};

TEST_F(CameraCoordinatorTest, RelevantVideoCaptureDeviceInfoExtraction) {
  EXPECT_EQ(GetComboboxModel().GetItemCount(), 0u);

  // Add first camera, and connect to it.
  // camera connection is done automatically to the device at combobox's default
  // index (i.e. 0).
  ASSERT_TRUE(AddFakeCamera({kDeviceName, kDeviceId}));
  EXPECT_EQ(std::get<0>(on_get_video_source_future_.Take()), kDeviceId);
  EXPECT_THAT(GetComboboxModel(), HasItems(std::vector{kDeviceName}));

  // Add second camera and connection to the first is not affected.
  ASSERT_TRUE(AddFakeCamera({kDeviceName2, kDeviceId2}));
  EXPECT_FALSE(on_get_video_source_future_.IsReady());
  EXPECT_THAT(GetComboboxModel(),
              HasItems(std::vector{kDeviceName, kDeviceName2}));

  // Remove first camera, and connect to the second one.
  ASSERT_TRUE(RemoveFakeCamera(kDeviceId));
  EXPECT_EQ(std::get<0>(on_get_video_source_future_.Take()), kDeviceId2);
  EXPECT_THAT(GetComboboxModel(), HasItems(std::vector{kDeviceName2}));

  // Re-add first camera and connect to it.
  ASSERT_TRUE(AddFakeCamera({kDeviceName, kDeviceId}));
  EXPECT_EQ(std::get<0>(on_get_video_source_future_.Take()), kDeviceId);
  EXPECT_THAT(GetComboboxModel(),
              HasItems(std::vector{kDeviceName, kDeviceName2}));

  // Remove second camera, and connection to the first is not affected.
  ASSERT_TRUE(RemoveFakeCamera(kDeviceId2));
  EXPECT_FALSE(on_get_video_source_future_.IsReady());
  EXPECT_THAT(GetComboboxModel(), HasItems(std::vector{kDeviceName}));

  // Remove first camera.
  ASSERT_TRUE(RemoveFakeCamera(kDeviceId));
  EXPECT_EQ(GetComboboxModel().GetItemCount(), 0u);

  coordinator_.reset();
  ExpectHistogramTotalDevices(/*expected_bucket_min_value=*/0);
}

TEST_F(CameraCoordinatorTest,
       RelevantVideoCaptureDeviceInfoExtraction_ConstrainedToEligibleDevices) {
  coordinator_.reset();
  // Nothing is recorded if device list is not initialized yet.
  histogram_tester_->ExpectTotalCount(kNumDevicesHistogram,
                                      /*expected_count=*/0);

  InitializeCoordinator({kDeviceId2});
  EXPECT_EQ(GetComboboxModel().GetItemCount(), 0u);

  // Add first camera. It won't be added to the combobox because it's not in the
  // eligible list.
  ASSERT_TRUE(AddFakeCamera({kDeviceName, kDeviceId}));
  EXPECT_FALSE(on_get_video_source_future_.IsReady());
  EXPECT_EQ(GetComboboxModel().GetItemCount(), 0u);

  // Add second camera and connect to it since it's in the eligible list.
  ASSERT_TRUE(AddFakeCamera({kDeviceName2, kDeviceId2}));
  EXPECT_EQ(std::get<0>(on_get_video_source_future_.Take()), kDeviceId2);
  EXPECT_THAT(GetComboboxModel(), HasItems(std::vector{kDeviceName2}));

  // Remove first camera, nothing changes since it wasn't in the combobox.
  ASSERT_TRUE(RemoveFakeCamera(kDeviceId));
  EXPECT_FALSE(on_get_video_source_future_.IsReady());
  EXPECT_THAT(GetComboboxModel(), HasItems(std::vector{kDeviceName2}));

  // Remove second camera.
  ASSERT_TRUE(RemoveFakeCamera(kDeviceId2));
  EXPECT_EQ(GetComboboxModel().GetItemCount(), 0u);

  coordinator_.reset();
  ExpectHistogramTotalDevices(/*expected_bucket_min_value=*/0);
}

TEST_F(CameraCoordinatorTest, ConnectToDifferentDevice) {
  EXPECT_EQ(GetComboboxModel().GetItemCount(), 0u);

  // Add first camera, and connect to it.
  ASSERT_TRUE(AddFakeCamera({kDeviceName, kDeviceId}));
  EXPECT_EQ(std::get<0>(on_get_video_source_future_.Take()), kDeviceId);

  // Add second camera and connection to the first is not affected.
  ASSERT_TRUE(AddFakeCamera({kDeviceName2, kDeviceId2}));
  EXPECT_FALSE(on_get_video_source_future_.IsReady());

  //  Connect to the second camera.
  coordinator_->OnVideoSourceChanged(/*selected_index=*/1);
  EXPECT_EQ(std::get<0>(on_get_video_source_future_.Take()), kDeviceId2);

  coordinator_.reset();
  ExpectHistogramTotalDevices(/*expected_bucket_min_value=*/2);
}

TEST_F(CameraCoordinatorTest, TryConnectToSameDevice) {
  EXPECT_EQ(GetComboboxModel().GetItemCount(), 0u);

  // Add camera, and connect to it.
  ASSERT_TRUE(AddFakeCamera({kDeviceName, kDeviceId}));
  EXPECT_EQ(std::get<0>(on_get_video_source_future_.Take()), kDeviceId);

  //  Try connect to the camera.
  // Nothing is expected because we are already connected.
  coordinator_->OnVideoSourceChanged(/*selected_index=*/0);
  EXPECT_FALSE(on_get_video_source_future_.IsReady());

  // Remove camera.
  ASSERT_TRUE(RemoveFakeCamera(kDeviceId));
  EXPECT_EQ(GetComboboxModel().GetItemCount(), 0u);

  // Add camera, and connect to it again.
  ASSERT_TRUE(AddFakeCamera({kDeviceName, kDeviceId}));
  EXPECT_EQ(std::get<0>(on_get_video_source_future_.Take()), kDeviceId);

  coordinator_.reset();
  ExpectHistogramTotalDevices(/*expected_bucket_min_value=*/1);
}

TEST_F(CameraCoordinatorTest, UpdateDevicePreferenceRanking) {
  EXPECT_EQ(GetComboboxModel().GetItemCount(), 0u);
  const media::VideoCaptureDeviceInfo kDevice1{{kDeviceName, kDeviceId}};
  const media::VideoCaptureDeviceInfo kDevice2{{kDeviceName2, kDeviceId2}};

  ASSERT_TRUE(AddFakeCamera(kDevice1.descriptor));
  EXPECT_EQ(std::get<0>(on_get_video_source_future_.Take()), kDeviceId);
  ASSERT_TRUE(AddFakeCamera(kDevice2.descriptor));
  on_get_video_source_future_.Clear();

  // Select device 2.
  coordinator_->OnVideoSourceChanged(/*selected_index=*/1);
  EXPECT_EQ(std::get<0>(on_get_video_source_future_.Take()), kDeviceId2);

  // Preference ranking defaults to noop.
  std::vector device_infos{kDevice1, kDevice2};
  media_prefs::PreferenceRankVideoDeviceInfos(*profile()->GetPrefs(),
                                              device_infos);
  EXPECT_THAT(device_infos,
              Pointwise(VideoCaptureDeviceInfoEq(), {kDevice1, kDevice2}));

  // Ranking is updated to make the currently selected device most preferred.
  coordinator_->UpdateDevicePreferenceRanking();
  media_prefs::PreferenceRankVideoDeviceInfos(*profile()->GetPrefs(),
                                              device_infos);
  EXPECT_THAT(device_infos,
              Pointwise(VideoCaptureDeviceInfoEq(), {kDevice2, kDevice1}));

  coordinator_.reset();
  ExpectHistogramTotalDevices(/*expected_bucket_min_value=*/2);
}

TEST_F(CameraCoordinatorTest, ConnectDevicesBeforeCoordinatorInitialize) {
  coordinator_.reset();

  ASSERT_TRUE(AddFakeCamera({kDeviceName, kDeviceId}));
  ASSERT_TRUE(AddFakeCamera({kDeviceName2, kDeviceId2}));

  InitializeCoordinator({});
  EXPECT_EQ(std::get<0>(on_get_video_source_future_.Take()), kDeviceId);
  EXPECT_THAT(GetComboboxModel(),
              HasItems(std::vector{kDeviceName, kDeviceName2}));
}
