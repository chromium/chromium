// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/recording/recording_service_test_api.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/threading/thread_checker.h"
#include "chromeos/ash/services/recording/audio_stream_mixer.h"

namespace recording {

RecordingServiceTestApi::RecordingServiceTestApi(
    mojo::PendingReceiver<mojom::RecordingService> receiver)
    : recording_service_(std::move(receiver)) {}

viz::FrameSinkId RecordingServiceTestApi::GetCurrentFrameSinkId() const {
  DCHECK_CALLED_ON_VALID_THREAD(recording_service_.main_thread_checker_);
  DCHECK(recording_service_.current_video_capture_params_);

  return recording_service_.current_video_capture_params_->frame_sink_id();
}

float RecordingServiceTestApi::GetCurrentDeviceScaleFactor() const {
  DCHECK_CALLED_ON_VALID_THREAD(recording_service_.main_thread_checker_);
  DCHECK(recording_service_.current_video_capture_params_);

  return recording_service_.current_video_capture_params_
      ->current_device_scale_factor();
}

gfx::Size RecordingServiceTestApi::GetCurrentFrameSinkSizeInPixels() const {
  DCHECK_CALLED_ON_VALID_THREAD(recording_service_.main_thread_checker_);
  DCHECK(recording_service_.current_video_capture_params_);

  return recording_service_.current_video_capture_params_
      ->current_frame_sink_size_pixels();
}

gfx::Size RecordingServiceTestApi::GetCurrentVideoSize() const {
  DCHECK_CALLED_ON_VALID_THREAD(recording_service_.main_thread_checker_);
  DCHECK(recording_service_.current_video_capture_params_);

  return recording_service_.current_video_capture_params_->GetVideoSize();
}

gfx::ImageSkia RecordingServiceTestApi::GetVideoThumbnail() const {
  DCHECK_CALLED_ON_VALID_THREAD(recording_service_.main_thread_checker_);

  return recording_service_.video_thumbnail_;
}

int RecordingServiceTestApi::GetNumberOfVideoEncoderReconfigures() const {
  DCHECK_CALLED_ON_VALID_THREAD(recording_service_.main_thread_checker_);

  return recording_service_.number_of_video_encoder_reconfigures_;
}

void RecordingServiceTestApi::RequestAndWaitForVideoFrame(
    VerifyVideoFrameCallback verify_frame_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(recording_service_.main_thread_checker_);
  DCHECK(recording_service_.video_capturer_remote_);
  DCHECK(!recording_service_.on_video_frame_delivered_callback_for_testing_);

  // Flush any pending calls from before.
  recording_service_.video_capturer_remote_.FlushForTesting();
  recording_service_.consumer_receiver_.FlushForTesting();

  base::RunLoop run_loop;

  recording_service_.on_video_frame_delivered_callback_for_testing_ =
      base::BindOnce(
          [](base::OnceClosure run_loop_quit_closure,
             VerifyVideoFrameCallback verify_callback,
             const media::VideoFrame& frame, const gfx::Rect& content_rect) {
            if (verify_callback)
              std::move(verify_callback).Run(frame, content_rect);

            std::move(run_loop_quit_closure).Run();
          },
          run_loop.QuitClosure(), std::move(verify_frame_callback));

  recording_service_.video_capturer_remote_->RequestRefreshFrame();
  recording_service_.video_capturer_remote_.FlushForTesting();

  run_loop.Run();
}

bool RecordingServiceTestApi::IsDoingAudioRecording() const {
  DCHECK_CALLED_ON_VALID_THREAD(recording_service_.main_thread_checker_);

  return !!recording_service_.audio_stream_mixer_;
}

int RecordingServiceTestApi::GetNumberOfAudioCapturers() const {
  DCHECK_CALLED_ON_VALID_THREAD(recording_service_.main_thread_checker_);

  if (recording_service_.audio_stream_mixer_) {
    base::RunLoop loop;
    int num_capturers = 0;
    // Since `audio_stream_mixer_` is bound to the `encoding_task_runner_`, we
    // can't call into it directly. We must flush all the posted tasks on it to
    // make sure all the capturers that it was supposed to create have already
    // been created. Then we can asynchronously get their number.
    recording_service_.audio_stream_mixer_.FlushPostedTasksForTesting();
    recording_service_.audio_stream_mixer_
        .AsyncCall(&AudioStreamMixer::GetNumberOfCapturers)
        .Then(base::BindLambdaForTesting([&loop, &num_capturers](int num) {
          num_capturers = num;
          loop.Quit();
        }));
    loop.Run();
    return num_capturers;
  }

  return 0;
}

}  // namespace recording
