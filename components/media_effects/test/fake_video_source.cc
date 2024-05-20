// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_effects/test/fake_video_source.h"

#include "media/capture/mojom/video_capture_buffer.mojom-forward.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/video_capture/public/cpp/mock_push_subscription.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "services/video_capture/public/mojom/video_source.mojom.h"

using testing::_;

namespace {

media::mojom::VideoBufferHandlePtr GetBufferHandler(
    const gfx::Size& frame_size) {
  return media::mojom::VideoBufferHandle::NewUnsafeShmemRegion(
      base::UnsafeSharedMemoryRegion::Create(media::VideoFrame::AllocationSize(
          media::PIXEL_FORMAT_I420, frame_size)));
}

}  // namespace

FakeVideoSource::FakeVideoSource()
    : push_subscription_(&mock_push_subscription_),
      video_frame_access_handler_receiver_(&fake_video_frame_access_handler_) {
  // Mock subscription activation.
  ON_CALL(mock_push_subscription_, Activate()).WillByDefault([this]() {
    video_frame_handler_->OnFrameAccessHandlerReady(
        video_frame_access_handler_receiver_.BindNewPipeAndPassRemote());
    push_subscription_activated_.SetValue();
  });

  // Mock subscription close.
  ON_CALL(mock_push_subscription_, DoClose(_))
      .WillByDefault(
          [this](video_capture::MockPushSubcription::CloseCallback& callback) {
            std::move(callback).Run();
            push_subscription_closed_.SetValue();
          });
}

FakeVideoSource::~FakeVideoSource() = default;

void FakeVideoSource::CreatePushSubscription(
    mojo::PendingRemote<video_capture::mojom::VideoFrameHandler> subscriber,
    const media::VideoCaptureParams& requested_settings,
    bool force_reopen_with_new_settings,
    mojo::PendingReceiver<video_capture::mojom::PushVideoStreamSubscription>
        subscription,
    CreatePushSubscriptionCallback callback) {
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

bool FakeVideoSource::WaitForCreatePushSubscription() {
  return created_push_subscription_.WaitAndClear();
}

bool FakeVideoSource::WaitForPushSubscriptionActivated() {
  return push_subscription_activated_.WaitAndClear();
}

bool FakeVideoSource::WaitForPushSubscriptionClosed() {
  return push_subscription_closed_.WaitAndClear();
}

void FakeVideoSource::SendFrame() {
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

void FakeVideoSource::SendError(media::VideoCaptureError error) {
  video_frame_handler_->OnError(error);
}
