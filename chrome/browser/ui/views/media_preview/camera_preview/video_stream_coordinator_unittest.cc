// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/video_stream_coordinator.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "media/capture/video_capture_types.h"
#include "services/video_capture/public/cpp/mock_push_subscription.h"
#include "services/video_capture/public/cpp/mock_video_frame_handler.h"
#include "services/video_capture/public/cpp/mock_video_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

}  // namespace

class VideoStreamCoordinatorTest : public TestWithBrowserView {
 protected:
  VideoStreamCoordinatorTest()
      : video_source_receiver_(&mock_video_source_),
        subscription_(&mock_subscription_),
        video_frame_access_handler_receiver_(
            &fake_video_frame_access_handler_) {}

  void SetUp() override {
    TestWithBrowserView::SetUp();
    parent_view_ = std::make_unique<views::View>();
    coordinator_ = std::make_unique<VideoStreamCoordinator>(*parent_view_);
  }

  void TearDown() override {
    coordinator_.reset();
    parent_view_.reset();
    TestWithBrowserView::TearDown();
  }

  static std::vector<media::VideoCaptureFormat> GetFormats() {
    return {{{160, 120}, 15.0, media::PIXEL_FORMAT_I420},
            {{160, 120}, 30.0, media::PIXEL_FORMAT_NV12},
            {{640, 480}, 30.0, media::PIXEL_FORMAT_NV12},
            {{640, 480}, 30.0, media::PIXEL_FORMAT_I420},
            {{3840, 2160}, 30.0, media::PIXEL_FORMAT_Y16},
            {{844, 400}, 30.0, media::PIXEL_FORMAT_NV12},
            {{1280, 720}, 30.0, media::PIXEL_FORMAT_I420}};
  }

  void ExpectCreatePushSubscriptionCall(Sequence sequence) {
    EXPECT_CALL(mock_video_source_, DoCreatePushSubscription(_, _, _, _, _))
        .InSequence(sequence)
        .WillOnce([&](auto subscriber, const auto& requested_settings,
                      bool force_reopen_with_new_settings, auto subscription,
                      auto& callback) {
          subscriber_.reset();
          subscriber_.Bind(std::move(subscriber));
          subscription_.reset();
          subscription_.Bind(std::move(subscription));
          current_settings_ = requested_settings;

          std::move(callback).Run(
              video_capture::mojom::CreatePushSubscriptionResultCode::
                  NewSuccessCode(
                      video_capture::mojom::CreatePushSubscriptionSuccessCode::
                          kCreatedWithRequestedSettings),
              requested_settings);
        });
  }

  void ExpectActivateCall(Sequence sequence) {
    EXPECT_CALL(mock_subscription_, Activate())
        .InSequence(sequence)
        .WillOnce([&]() {
          subscriber_->OnFrameAccessHandlerReady(
              video_frame_access_handler_receiver_.BindNewPipeAndPassRemote());

          start_time_ = base::Time::Now();
          const auto frame_duration =
              base::Hertz(current_settings_.requested_format.frame_rate);
          frame_timer_.Start(FROM_HERE, frame_duration, this,
                             &VideoStreamCoordinatorTest::OnNextFrame);
        });
  }

  void OnNextFrame() {
    const int buffer_id = g_next_buffer_id++;
    subscriber_->OnNewBuffer(
        buffer_id,
        GetBufferHandler(current_settings_.requested_format.frame_size));

    media::mojom::VideoFrameInfoPtr info = media::mojom::VideoFrameInfo::New();
    info->timestamp = base::Time::Now() - start_time_;
    info->pixel_format = media::PIXEL_FORMAT_I420;
    info->coded_size = current_settings_.requested_format.frame_size;
    info->visible_rect = gfx::Rect(info->coded_size);
    info->is_premapped = false;
    subscriber_->OnFrameReadyInBuffer(
        video_capture::mojom::ReadyFrameInBuffer::New(buffer_id,
                                                      /*frame_feedback_id=*/0,
                                                      std::move(info)));
  }

  void ExpectCloseCall(Sequence sequence, base::RunLoop& run_loop) {
    EXPECT_CALL(mock_subscription_, DoClose(_))
        .InSequence(sequence)
        .WillOnce(
            [&](video_capture::MockPushSubcription::CloseCallback& callback) {
              std::move(callback).Run();
              video_frame_access_handler_receiver_.reset();
              video_source_receiver_.reset();
              run_loop.Quit();
            });
  }

  std::unique_ptr<views::View> parent_view_;
  std::unique_ptr<VideoStreamCoordinator> coordinator_;

  video_capture::MockVideoSource mock_video_source_;
  mojo::Receiver<video_capture::mojom::VideoSource> video_source_receiver_;

  media::VideoCaptureParams current_settings_;

  video_capture::MockPushSubcription mock_subscription_;
  mojo::Receiver<video_capture::mojom::PushVideoStreamSubscription>
      subscription_;

  mojo::Remote<video_capture::mojom::VideoFrameHandler> subscriber_;

  video_capture::FakeVideoFrameAccessHandler fake_video_frame_access_handler_;
  mojo::Receiver<video_capture::mojom::VideoFrameAccessHandler>
      video_frame_access_handler_receiver_;

  // The next ID to be used for a newly created buffer.
  int g_next_buffer_id = 0;

  // The time at which this device started producing video frames.
  base::Time start_time_;

  // The timer that invokes `OnNextFrame()` repeatedly depending on the frame
  // rate requested.
  base::RepeatingTimer frame_timer_;
};

TEST_F(VideoStreamCoordinatorTest, ConnectToFrameHandlerAndReceiveFrames) {
  Sequence sequence;
  ExpectCreatePushSubscriptionCall(sequence);
  ExpectActivateCall(sequence);

  base::MockCallback<base::RepeatingClosure> callback;
  EXPECT_CALL(callback, Run()).Times(9).InSequence(sequence);
  EXPECT_CALL(callback, Run()).InSequence(sequence).WillOnce([this]() {
    coordinator_->Stop();
  });

  base::RunLoop run_loop;
  ExpectCloseCall(sequence, run_loop);

  coordinator_->SetFrameReceivedCallbackForTest(callback.Get());

  mojo::Remote<video_capture::mojom::VideoSource> video_source;
  video_source_receiver_.Bind(video_source.BindNewPipeAndPassReceiver());
  coordinator_->ConnectToDevice(std::move(video_source), GetFormats());
  run_loop.Run();
}

TEST_F(VideoStreamCoordinatorTest, ChooseTheClosetFormat) {
  const auto& formats = GetFormats();
  EXPECT_EQ(formats[0],
            coordinator_->GetClosestVideoFormatForTest(
                formats, /*view_width=*/130, /*minimum_frame_rate*/ 10));
  EXPECT_EQ(formats[3],
            coordinator_->GetClosestVideoFormatForTest(
                formats, /*view_width=*/300, /*minimum_frame_rate*/ 10));

  EXPECT_EQ(formats[3],
            coordinator_->GetClosestVideoFormatForTest(
                formats, /*view_width=*/280, /*minimum_frame_rate*/ 30));
  EXPECT_EQ(formats[5],
            coordinator_->GetClosestVideoFormatForTest(
                formats, /*view_width=*/700, /*minimum_frame_rate*/ 30));

  EXPECT_EQ(formats[3], coordinator_->GetClosestVideoFormatForTest(
                            formats, /*view_width=*/280,
                            /*minimum_frame_rate*/ 40));
}
