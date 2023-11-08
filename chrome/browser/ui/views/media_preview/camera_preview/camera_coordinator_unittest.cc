// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/camera_coordinator.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/video_capture_service.h"
#include "services/video_capture/public/cpp/mock_video_capture_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

namespace {

std::vector<media::VideoCaptureDeviceInfo> MakeDeviceInfosList() {
  std::vector<media::VideoCaptureDeviceInfo> device_infos;

  constexpr char kDeviceId[] = "device_id";
  constexpr char kDeviceName[] = "device_name";
  constexpr char kDeviceId2[] = "device_id_2";
  constexpr char kDeviceName2[] = "device_name_2";

  // Just using some arbitrary values to push back into the vector.
  media::VideoCaptureDeviceInfo first_device;
  first_device.descriptor = {kDeviceName, kDeviceId};
  first_device.supported_formats = {
      // NV12 QQVGA at 30fps, 15fps
      {{160, 120}, 30.0, media::PIXEL_FORMAT_NV12},
      {{160, 120}, 15.0, media::PIXEL_FORMAT_NV12},
      // NV12 VGA
      {{640, 480}, 30.0, media::PIXEL_FORMAT_NV12},
      // UYVY VGA
      {{640, 480}, 30.0, media::PIXEL_FORMAT_UYVY},
      // MJPEG 4K
      {{3840, 2160}, 30.0, media::PIXEL_FORMAT_MJPEG},
      // Odd resolution
      {{844, 400}, 30.0, media::PIXEL_FORMAT_NV12},
      // HD at unknown pixel format
      {{1280, 720}, 30.0, media::PIXEL_FORMAT_UNKNOWN}};
  device_infos.push_back(first_device);

  media::VideoCaptureDeviceInfo second_device;
  second_device.descriptor = {kDeviceName2, kDeviceId2};
  second_device.supported_formats = {
      // UYVY VGA to test that we get 2 UYVY and 2 VGA in metrics.
      {{640, 480}, 30.0, media::PIXEL_FORMAT_UYVY}};
  device_infos.push_back(second_device);

  return device_infos;
}

}  // namespace

class CameraCoordinatorTest : public TestWithBrowserView {
 protected:
  void SetUp() override {
    TestWithBrowserView::SetUp();
    content::OverrideVideoCaptureServiceForTesting(
        &mock_video_capture_service_);

    parent_view_ = std::make_unique<views::View>();
    coordinator_ = std::make_unique<CameraCoordinator>(*parent_view_,
                                                       /*needs_borders=*/true);
  }

  void TearDown() override {
    coordinator_.reset();
    parent_view_.reset();
    content::OverrideVideoCaptureServiceForTesting(nullptr);
    TestWithBrowserView::TearDown();
  }

  void OnVideoSourceInfosReceived(
      const std::vector<media::VideoCaptureDeviceInfo>& device_infos) {
    coordinator_->OnVideoSourceInfosReceived(device_infos);
  }

  void VerifyComboboxModelItemCount(size_t size) const {
    ASSERT_NO_FATAL_FAILURE(
        ASSERT_EQ(coordinator_->combobox_model_.GetItemCount(), size));
  }

  void VerifyComboboxModelItemAt(int index,
                                 const std::u16string& device_id) const {
    EXPECT_EQ(coordinator_->combobox_model_.GetItemAt(index), device_id);
  }

  video_capture::MockVideoCaptureService mock_video_capture_service_;

  std::unique_ptr<views::View> parent_view_;
  std::unique_ptr<CameraCoordinator> coordinator_;
};

TEST_F(CameraCoordinatorTest, RelevantVideoCaptureDeviceInfoExtraction) {
  const int kIndex = 1;  // any random value
  std::vector<media::VideoCaptureDeviceInfo> device_infos =
      MakeDeviceInfosList();

  // Our combobox model size will always be >= 1. If no cameras are connected, a
  // message is shown to the user to connect a camera.
  // Verify that there is precisely one item in the combobox model.
  VerifyComboboxModelItemCount(/*size=*/1);
  VerifyComboboxModelItemAt(
      /*index=*/0,
      l10n_util::GetStringUTF16(IDS_MEDIA_PREVIEW_NO_CAMERAS_FOUND_COMBOBOX));

  OnVideoSourceInfosReceived(device_infos);

  VerifyComboboxModelItemCount(/*size=*/device_infos.size());
  VerifyComboboxModelItemAt(
      kIndex,
      base::UTF8ToUTF16(device_infos.at(kIndex).descriptor.GetNameAndModel()));
}
