// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/camera_mediator.h"

#include <stddef.h>

#include <memory>

#include "base/run_loop.h"
#include "base/system/system_monitor.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "content/public/browser/video_capture_service.h"
#include "services/video_capture/public/cpp/mock_video_capture_service.h"
#include "services/video_capture/public/cpp/mock_video_source_provider.h"
#include "services/video_capture/public/mojom/video_source.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace {

constexpr char kDeviceId[] = "device_id";
constexpr char kDeviceName[] = "device_name";
constexpr char kDeviceId2[] = "device_id_2";
constexpr char kDeviceName2[] = "device_name_2";

void VerifyEquality(const media::VideoCaptureDeviceInfo& a,
                    const media::VideoCaptureDeviceInfo& b) {
  EXPECT_EQ(a.descriptor, b.descriptor);
  EXPECT_EQ(a.supported_formats.size(), b.supported_formats.size());
}

void VerifyVectorsEquality(
    const std::vector<media::VideoCaptureDeviceInfo>& a,
    const std::vector<media::VideoCaptureDeviceInfo>& b) {
  ASSERT_EQ(a.size(), b.size());
  for (size_t index = 0; index < b.size(); index++) {
    VerifyEquality(a.at(index), b.at(index));
  }
}

std::vector<media::VideoCaptureDeviceInfo> MakeDeviceInfosList() {
  std::vector<media::VideoCaptureDeviceInfo> device_infos;

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

class CameraMediatorTest : public TestWithBrowserView {
 protected:
  CameraMediatorTest()
      : video_source_provider_receiver_(&mock_video_source_provider_) {}

  void SetUp() override {
    TestWithBrowserView::SetUp();
    content::OverrideVideoCaptureServiceForTesting(
        &mock_video_capture_service_);
    SetOnCallHandlingForMockVideoCaptureService();
    SetOnCallHandlingForMockVideoSourceProvider();

    // During CameraMediator constructor, GetSourceInfos() is called. So, a list
    // of device infos is expected.
    base::RunLoop wait_for_device_infos_list;
    ExpectNewDeviceList(wait_for_device_infos_list);
    mediator_ =
        std::make_unique<CameraMediator>(devices_change_callback_.Get());
    wait_for_device_infos_list.Run();
  }

  void TearDown() override {
    content::OverrideVideoCaptureServiceForTesting(nullptr);
    TestWithBrowserView::TearDown();
  }

  void SetOnCallHandlingForMockVideoCaptureService() {
    ON_CALL(mock_video_capture_service_, DoConnectToVideoSourceProvider(_))
        .WillByDefault(
            [this](
                mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider>
                    receiver) {
              video_source_provider_receiver_.reset();
              video_source_provider_receiver_.Bind(std::move(receiver));
            });
  }

  void SetOnCallHandlingForMockVideoSourceProvider() {
    ON_CALL(mock_video_source_provider_, DoGetSourceInfos(_))
        .WillByDefault([this](video_capture::mojom::VideoSourceProvider::
                                  GetSourceInfosCallback& callback) {
          std::move(callback).Run(device_infos_);
        });
  }

  void ExpectNewDeviceList(base::RunLoop& wait_for_updated_device_infos_list) {
    EXPECT_CALL(devices_change_callback_, Run(_))
        .WillOnce([this, &wait_for_updated_device_infos_list](
                      const std::vector<media::VideoCaptureDeviceInfo>&
                          device_infos) {
          VerifyVectorsEquality(device_infos_, device_infos);
          wait_for_updated_device_infos_list.Quit();
        });
  }

  base::SystemMonitor monitor_;
  video_capture::MockVideoCaptureService mock_video_capture_service_;
  video_capture::MockVideoSourceProvider mock_video_source_provider_;
  mojo::Receiver<video_capture::mojom::VideoSourceProvider>
      video_source_provider_receiver_;

  base::MockCallback<CameraMediator::DevicesChangedCallback>
      devices_change_callback_;
  std::unique_ptr<CameraMediator> mediator_;
  std::vector<media::VideoCaptureDeviceInfo> device_infos_;
};

TEST_F(CameraMediatorTest, ObserveDevicesChanges) {
  EXPECT_CALL(devices_change_callback_, Run(_)).Times(0);
  monitor_.ProcessDevicesChanged(
      base::SystemMonitor::DeviceType::DEVTYPE_AUDIO);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_video_source_provider_, DoGetSourceInfos(_));
  base::RunLoop wait_for_device_infos_list;
  ExpectNewDeviceList(wait_for_device_infos_list);
  device_infos_ = MakeDeviceInfosList();
  monitor_.ProcessDevicesChanged(
      base::SystemMonitor::DeviceType::DEVTYPE_VIDEO_CAPTURE);
  wait_for_device_infos_list.Run();
}

TEST_F(CameraMediatorTest, BindVideoSource) {
  base::RunLoop wait_for_call_to_arrive;
  mojo::Remote<video_capture::mojom::VideoSource> video_source;
  EXPECT_CALL(mock_video_source_provider_, DoGetVideoSource(kDeviceId, _))
      .WillOnce(
          [&wait_for_call_to_arrive]() { wait_for_call_to_arrive.Quit(); });
  mediator_->BindVideoSource(kDeviceId,
                             video_source.BindNewPipeAndPassReceiver());
  wait_for_call_to_arrive.Run();
}
