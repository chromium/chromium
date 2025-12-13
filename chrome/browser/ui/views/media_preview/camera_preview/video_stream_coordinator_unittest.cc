// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/video_stream_coordinator.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/media_preview/camera_preview/video_stream_view.h"
#include "chrome/browser/ui/views/media_preview/media_preview_metrics.h"
#include "components/media_effects/test/fake_video_source.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_image_transport_factory.h"
#include "content/public/test/test_renderer_host.h"
#include "media/capture/video_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/canvas.h"

namespace {

constexpr char kVideoDelay[] =
    "MediaPreviews.UI.Preview.Permissions.Video.Delay";
constexpr char kPixelHeight[] =
    "MediaPreviews.UI.Permissions.Camera.PixelHeight";
constexpr char kExpectedFPS[] =
    "MediaPreviews.UI.Preview.Permissions.Video.ExpectedFPS";
constexpr char kActualFPS[] =
    "MediaPreviews.UI.Preview.Permissions.Video.ActualFPS";
constexpr char kRenderedPercent[] =
    "MediaPreviews.UI.Preview.Permissions.Video.RenderedPercent";
constexpr char kTotalVisibleDuration[] =
    "MediaPreviews.UI.Preview.Permissions.Video.TotalVisibleDuration";
constexpr char kTimeToActionWithoutPreview[] =
    "MediaPreviews.UI.Preview.Permissions.Video.TimeToActionWithoutPreview";
constexpr char kCapturedErrors[] =
    "MediaPreviews.UI.Preview.Permissions.VideoCaptureError";

}  // namespace

using testing::_;
using testing::Mock;
using testing::Sequence;

class VideoStreamCoordinatorTest : public testing::Test {
 protected:
  VideoStreamCoordinatorTest() : video_source_receiver_(&fake_video_source_) {}
  ~VideoStreamCoordinatorTest() override = default;

  void SetUp() override {
    Test::SetUp();
    layout_provider_ = ChromeLayoutProvider::CreateLayoutProvider();
    parent_view_ = std::make_unique<views::View>();
    coordinator_ = std::make_unique<VideoStreamCoordinator>(
        *parent_view_, media_preview_metrics::Context(
                           media_preview_metrics::UiLocation::kPermissionPrompt,
                           media_preview_metrics::PreviewType::kCamera));
  }

  void TearDown() override {
    coordinator_.reset();
    parent_view_.reset();
    Test::TearDown();
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

  void SendAndWaitForError(media::VideoCaptureError error) {
    base::test::TestFuture<void> got_error;
    coordinator_->SetErrorReceivedCallbackForTest(
        got_error.GetRepeatingCallback());
    fake_video_source_.SendError(error);
    EXPECT_TRUE(got_error.WaitAndClear());
  }

  std::unique_ptr<views::LayoutProvider> layout_provider_;
  std::unique_ptr<views::View> parent_view_;
  std::unique_ptr<VideoStreamCoordinator> coordinator_;

  FakeVideoSource fake_video_source_;
  mojo::Receiver<video_capture::mojom::VideoSource> video_source_receiver_;

  base::HistogramTester histogram_tester_;

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
};

TEST_F(VideoStreamCoordinatorTest, ConnectToFrameHandlerAndReceiveFrames) {
  mojo::Remote<video_capture::mojom::VideoSource> video_source;
  video_source_receiver_.Bind(video_source.BindNewPipeAndPassReceiver());
  coordinator_->ConnectToDevice(GetVideoCaptureDeviceInfo(),
                                std::move(video_source));

  coordinator_->GetVideoStreamView()->SetPreferredSize(gfx::Size{250, 180});
  coordinator_->GetVideoStreamView()->SizeToPreferredSize();

  EXPECT_TRUE(fake_video_source_.WaitForCreatePushSubscription());
  EXPECT_TRUE(fake_video_source_.WaitForPushSubscriptionActivated());

  base::test::TestFuture<void> got_frame;
  coordinator_->SetFrameReceivedCallbackForTest(
      got_frame.GetRepeatingCallback());
  // Send 18 frames over a simulated second
  for (size_t i = 0; i < 18; ++i) {
    fake_video_source_.SendFrame();
    task_environment_.AdvanceClock(base::Milliseconds(55.8));
    EXPECT_TRUE(got_frame.WaitAndClear());
    // Paint every other frame.
    if (i % 2) {
      TriggerPaint();
    }
  }

  const auto error = media::VideoCaptureError::
      kErrorFakeDeviceIntentionallyEmittingErrorEvent;  // any random error.
  SendAndWaitForError(error);
  histogram_tester_.ExpectUniqueSample(kCapturedErrors,
                                       /*sample=*/error, 1);

  coordinator_->Stop();
  EXPECT_TRUE(fake_video_source_.WaitForPushSubscriptionClosed());

  histogram_tester_.ExpectUniqueSample(kVideoDelay,
                                       /*sample=*/50, 1);

  // The selected pixel height is 720, so it will be logged in the 675 bucket.
  histogram_tester_.ExpectUniqueSample(kPixelHeight,
                                       /*sample=*/675, 1);
  histogram_tester_.ExpectUniqueSample(kExpectedFPS,
                                       /*sample=*/30, 1);
  histogram_tester_.ExpectUniqueSample(kActualFPS,
                                       /*sample=*/18, 1);
  histogram_tester_.ExpectUniqueSample(kRenderedPercent,
                                       /*sample=*/50, 1);

  coordinator_.reset();
  histogram_tester_.ExpectUniqueSample(kTotalVisibleDuration,
                                       /*sample=*/750, 1);
  histogram_tester_.ExpectTotalCount(kTimeToActionWithoutPreview, 0);
}

TEST_F(VideoStreamCoordinatorTest, ConnectToFrameHandlerAndReceiveNoFrames) {
  mojo::Remote<video_capture::mojom::VideoSource> video_source;
  video_source_receiver_.Bind(video_source.BindNewPipeAndPassReceiver());
  coordinator_->ConnectToDevice(GetVideoCaptureDeviceInfo(),
                                std::move(video_source));

  coordinator_->GetVideoStreamView()->SetPreferredSize(gfx::Size{250, 180});
  coordinator_->GetVideoStreamView()->SizeToPreferredSize();

  EXPECT_TRUE(fake_video_source_.WaitForCreatePushSubscription());
  EXPECT_TRUE(fake_video_source_.WaitForPushSubscriptionActivated());

  base::RunLoop().RunUntilIdle();
  task_environment_.AdvanceClock(base::Milliseconds(130));

  const auto error = media::VideoCaptureError::
      kVideoCaptureControllerUnsupportedPixelFormat;  // any random error.
  SendAndWaitForError(error);
  histogram_tester_.ExpectUniqueSample(kCapturedErrors,
                                       /*sample=*/error, 1);

  fake_video_source_.SendError(error);
  coordinator_->Stop();
  EXPECT_TRUE(fake_video_source_.WaitForPushSubscriptionClosed());

  // Sending errors close to stopping time is disregarded.
  histogram_tester_.ExpectTotalCount(kCapturedErrors, 1);

  histogram_tester_.ExpectTotalCount(kVideoDelay, 0);

  // The selected pixel height is 720, so it will be logged in the 675 bucket.
  histogram_tester_.ExpectUniqueSample(kPixelHeight,
                                       /*sample=*/675, 1);
  histogram_tester_.ExpectUniqueSample(kExpectedFPS,
                                       /*sample=*/30, 1);

  histogram_tester_.ExpectTotalCount(kActualFPS, 0);
  histogram_tester_.ExpectTotalCount(kRenderedPercent, 0);

  coordinator_.reset();
  histogram_tester_.ExpectUniqueSample(kTotalVisibleDuration,
                                       /*sample=*/0, 1);
  histogram_tester_.ExpectUniqueSample(kTimeToActionWithoutPreview,
                                       /*sample=*/125, 1);
}

TEST_F(VideoStreamCoordinatorTest,
       ConnectToFrameHandlerWithUnBoundVideoSource) {
  mojo::Remote<video_capture::mojom::VideoSource> video_source;
  coordinator_->ConnectToDevice(GetVideoCaptureDeviceInfo(),
                                std::move(video_source));

  coordinator_->GetVideoStreamView()->SetPreferredSize(gfx::Size{250, 180});
  coordinator_->GetVideoStreamView()->SizeToPreferredSize();

  base::RunLoop().RunUntilIdle();
  task_environment_.AdvanceClock(base::Milliseconds(130));

  coordinator_->Stop();
  histogram_tester_.ExpectTotalCount(kVideoDelay, 0);
  histogram_tester_.ExpectTotalCount(kPixelHeight, 0);
  histogram_tester_.ExpectTotalCount(kExpectedFPS, 0);
  histogram_tester_.ExpectTotalCount(kActualFPS, 0);
  histogram_tester_.ExpectTotalCount(kRenderedPercent, 0);

  coordinator_.reset();
  histogram_tester_.ExpectTotalCount(kTotalVisibleDuration, 0);
  histogram_tester_.ExpectTotalCount(kTimeToActionWithoutPreview, 0);
}
