// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/video_stream_coordinator.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/media_preview/camera_preview/video_stream_view.h"
#include "media/capture/video_capture_types.h"
#include "services/video_capture/public/cpp/mock_push_subscription.h"
#include "services/video_capture/public/cpp/mock_video_frame_handler.h"
#include "services/video_capture/public/cpp/mock_video_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/canvas.h"

using testing::_;
using testing::Mock;
using testing::Sequence;

namespace {

media::mojom::VideoBufferHandlePtr GetBufferHandler(
    const gfx::Size& frame_size) {
  return media::mojom::VideoBufferHandle::NewUnsafeShmemRegion(
      base::UnsafeSharedMemoryRegion::Create(media::VideoFrame::AllocationSize(
          media::PIXEL_FORMAT_I420, frame_size)));
}

class FakeVideoSource : public video_capture::mojom::VideoSource {
 public:
  FakeVideoSource()
      : push_subscription_(&mock_push_subscription_),
        video_frame_access_handler_receiver_(
            &fake_video_frame_access_handler_) {
    // Mock subscription activation.
    ON_CALL(mock_push_subscription_, Activate()).WillByDefault([this]() {
      video_frame_handler_->OnFrameAccessHandlerReady(
          video_frame_access_handler_receiver_.BindNewPipeAndPassRemote());
      push_subscription_activated_.SetValue();
    });

    // Mock subscription close.
    ON_CALL(mock_push_subscription_, DoClose(_))
        .WillByDefault(
            [this](
                video_capture::MockPushSubcription::CloseCallback& callback) {
              std::move(callback).Run();
              push_subscription_closed_.SetValue();
            });
  }

  ~FakeVideoSource() override = default;

  void CreatePushSubscription(
      mojo::PendingRemote<video_capture::mojom::VideoFrameHandler> subscriber,
      const media::VideoCaptureParams& requested_settings,
      bool force_reopen_with_new_settings,
      mojo::PendingReceiver<video_capture::mojom::PushVideoStreamSubscription>
          subscription,
      CreatePushSubscriptionCallback callback) override {
    video_frame_handler_.Bind(std::move(subscriber));
    push_subscription_.Bind(std::move(subscription));
    requested_settings_ = requested_settings;

    std::move(callback).Run(
        video_capture::mojom::CreatePushSubscriptionResultCode::NewSuccessCode(
            video_capture::mojom::CreatePushSubscriptionSuccessCode::
                kCreatedWithRequestedSettings),
        requested_settings);

    created_push_subscription_.SetValue();
  }

  void RegisterVideoEffectsProcessor(
      mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>
          processor) override {}

  [[nodiscard]] bool WaitForCreatePushSubscription() {
    return created_push_subscription_.WaitAndClear();
  }

  [[nodiscard]] bool WaitForPushSubscriptionActivated() {
    return push_subscription_activated_.WaitAndClear();
  }

  [[nodiscard]] bool WaitForPushSubscriptionClosed() {
    return push_subscription_closed_.WaitAndClear();
  }

  void SendFrame() {
    ++current_buffer_id_;
    video_frame_handler_->OnNewBuffer(
        current_buffer_id_,
        GetBufferHandler(requested_settings_.requested_format.frame_size));

    media::mojom::VideoFrameInfoPtr info = media::mojom::VideoFrameInfo::New();
    info->timestamp = base::TimeTicks::Now().since_origin();
    info->pixel_format = media::PIXEL_FORMAT_I420;
    info->coded_size = requested_settings_.requested_format.frame_size;
    info->visible_rect = gfx::Rect(info->coded_size);
    info->is_premapped = false;
    video_frame_handler_->OnFrameReadyInBuffer(
        video_capture::mojom::ReadyFrameInBuffer::New(current_buffer_id_,
                                                      /*frame_feedback_id=*/0,
                                                      std::move(info)));
  }

 private:
  base::test::TestFuture<void> created_push_subscription_;
  base::test::TestFuture<void> push_subscription_activated_;
  base::test::TestFuture<void> push_subscription_closed_;
  mojo::Remote<video_capture::mojom::VideoFrameHandler> video_frame_handler_;
  video_capture::MockPushSubcription mock_push_subscription_;
  mojo::Receiver<video_capture::mojom::PushVideoStreamSubscription>
      push_subscription_;
  media::VideoCaptureParams requested_settings_;
  int current_buffer_id_ = 0;

  video_capture::FakeVideoFrameAccessHandler fake_video_frame_access_handler_;
  mojo::Receiver<video_capture::mojom::VideoFrameAccessHandler>
      video_frame_access_handler_receiver_;
};

}  // namespace

class VideoStreamCoordinatorTest : public TestWithBrowserView {
 protected:
  VideoStreamCoordinatorTest()
      : TestWithBrowserView(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        video_source_receiver_(&fake_video_source_) {}

  void SetUp() override {
    TestWithBrowserView::SetUp();
    parent_view_ = std::make_unique<views::View>();
    coordinator_ = std::make_unique<VideoStreamCoordinator>(
        *parent_view_,
        media_preview_metrics::Context(
            media_preview_metrics::UiLocation::kPermissionPrompt));
  }

  void TearDown() override {
    coordinator_.reset();
    parent_view_.reset();
    TestWithBrowserView::TearDown();
  }

  static media::VideoCaptureDeviceInfo GetVideoCaptureDeviceInfo() {
    media::VideoCaptureDeviceDescriptor descriptor;
    descriptor.device_id = "device_id";

    media::VideoCaptureDeviceInfo device_info(descriptor);
    device_info.supported_formats = {
        {{160, 120}, 15.0, media::PIXEL_FORMAT_I420},
        {{160, 120}, 30.0, media::PIXEL_FORMAT_NV12},
        {{640, 480}, 30.0, media::PIXEL_FORMAT_NV12},
        {{640, 480}, 30.0, media::PIXEL_FORMAT_I420},
        {{3840, 2160}, 30.0, media::PIXEL_FORMAT_Y16},
        {{844, 400}, 30.0, media::PIXEL_FORMAT_NV12},
        {{1280, 720}, 30.0, media::PIXEL_FORMAT_I420}};
    return device_info;
  }

  void TriggerPaint() {
    CHECK(parent_view_->children().size() == 1);
    auto* video_stream_view = coordinator_->GetVideoStreamView();
    gfx::Canvas canvas;
    video_stream_view->OnPaint(&canvas);
  }

  std::unique_ptr<views::View> parent_view_;
  std::unique_ptr<VideoStreamCoordinator> coordinator_;

  FakeVideoSource fake_video_source_;
  mojo::Receiver<video_capture::mojom::VideoSource> video_source_receiver_;

  base::HistogramTester histogram_tester_;
};

TEST_F(VideoStreamCoordinatorTest, ConnectToFrameHandlerAndReceiveFrames) {
  mojo::Remote<video_capture::mojom::VideoSource> video_source;
  video_source_receiver_.Bind(video_source.BindNewPipeAndPassReceiver());
  coordinator_->ConnectToDevice(GetVideoCaptureDeviceInfo(),
                                std::move(video_source));
  EXPECT_TRUE(fake_video_source_.WaitForCreatePushSubscription());
  EXPECT_TRUE(fake_video_source_.WaitForPushSubscriptionActivated());

  base::test::TestFuture<void> got_frame;
  coordinator_->SetFrameReceivedCallbackForTest(
      got_frame.GetRepeatingCallback());
  // Send 18 frames over a simulated second
  for (size_t i = 0; i < 18; ++i) {
    fake_video_source_.SendFrame();
    task_environment()->AdvanceClock(base::Milliseconds(55.8));
    EXPECT_TRUE(got_frame.WaitAndClear());
    // Paint every other frame.
    if (i % 2) {
      TriggerPaint();
    }
  }

  coordinator_->Stop();
  EXPECT_TRUE(fake_video_source_.WaitForPushSubscriptionClosed());

  // The selected pixel height is 120, so it will be logged in the 1 bucket.
  histogram_tester_.ExpectUniqueSample(
      "MediaPreviews.UI.Permissions.Camera.PixelHeight",
      /*bucket_min_value=*/1, 1);
  histogram_tester_.ExpectUniqueSample(
      "MediaPreviews.UI.Preview.Permissions.Video.ExpectedFPS",
      /*bucket_min_value=*/30, 1);
  histogram_tester_.ExpectUniqueSample(
      "MediaPreviews.UI.Preview.Permissions.Video.ActualFPS",
      /*bucket_min_value=*/18, 1);
  histogram_tester_.ExpectUniqueSample(
      "MediaPreviews.UI.Preview.Permissions.Video.RenderedPercent",
      /*bucket_min_value=*/50, 1);
}
