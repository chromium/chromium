// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_EFFECTS_TEST_FAKE_VIDEO_SOURCE_H_
#define COMPONENTS_MEDIA_EFFECTS_TEST_FAKE_VIDEO_SOURCE_H_

#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/cpp/mock_push_subscription.h"
#include "services/video_capture/public/cpp/mock_video_frame_handler.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "services/video_capture/public/mojom/video_source.mojom.h"

class FakeVideoSource : public video_capture::mojom::VideoSource {
 public:
  FakeVideoSource();
  ~FakeVideoSource() override;

  void CreatePushSubscription(
      mojo::PendingRemote<video_capture::mojom::VideoFrameHandler> subscriber,
      const media::VideoCaptureParams& requested_settings,
      bool force_reopen_with_new_settings,
      mojo::PendingReceiver<video_capture::mojom::PushVideoStreamSubscription>
          subscription,
      CreatePushSubscriptionCallback callback) override;

  void RegisterVideoEffectsProcessor(
      mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>
          processor) override {}

  [[nodiscard]] bool WaitForCreatePushSubscription();

  [[nodiscard]] bool WaitForPushSubscriptionActivated();

  [[nodiscard]] bool WaitForPushSubscriptionClosed();

  void SendFrame();

  void SendError(media::VideoCaptureError error);

  media::VideoCaptureParams get_requested_settings() {
    return requested_settings_;
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

#endif  // COMPONENTS_MEDIA_EFFECTS_TEST_FAKE_VIDEO_SOURCE_H_
