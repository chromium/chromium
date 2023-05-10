// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_RECORDING_RECORDING_SERVICE_TEST_API_H_
#define CHROMEOS_ASH_SERVICES_RECORDING_RECORDING_SERVICE_TEST_API_H_

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "chromeos/ash/services/recording/public/mojom/recording_service.mojom.h"
#include "chromeos/ash/services/recording/recording_service.h"
#include "media/base/video_frame.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/gfx/image/image_skia.h"

namespace recording {

// Defines an API to test the internals of the recording service. The recording
// service instance is created (in-process) and owned by this class.
class RecordingServiceTestApi {
 public:
  explicit RecordingServiceTestApi(
      mojo::PendingReceiver<mojom::RecordingService> receiver);
  RecordingServiceTestApi(const RecordingServiceTestApi&) = delete;
  RecordingServiceTestApi& operator=(const RecordingServiceTestApi&) = delete;
  ~RecordingServiceTestApi() = default;

  // Gets the current frame sink id being captured by the service.
  viz::FrameSinkId GetCurrentFrameSinkId() const;

  // Gets the device scale factor value used by the recording service. This
  // value will always be 1.f when recording at the DIPs sizes, and equal to the
  // current DSF value of the display being recorded when recording at the pixel
  // sizes (see |ash::features::kRecordAtPixelSize|).
  float GetCurrentDeviceScaleFactor() const;

  // Gets the current size of the frame sink being recorded in pixels.
  gfx::Size GetCurrentFrameSinkSizeInPixels() const;

  // Gets the current video size being captured by the service. This matches the
  // the pixel size of the target being recorded.
  gfx::Size GetCurrentVideoSize() const;

  // Gets the thumbnail image that will be used by the service to provide it to
  // the client.
  gfx::ImageSkia GetVideoThumbnail() const;

  // Gets the number of times the video encoder was reconfigured (not counting
  // the first time it was configured) as a result of a change in the video
  // size.
  int GetNumberOfVideoEncoderReconfigures() const;

  // Requests a video frame from the video capturer and waits for it to be
  // delivered to the service. If the caller provided a non-null
  // |verify_frame_callback| it will be called before this function returns.
  using VerifyVideoFrameCallback =
      base::OnceCallback<void(const media::VideoFrame& frame,
                              const gfx::Rect& content_rect)>;
  void RequestAndWaitForVideoFrame(
      VerifyVideoFrameCallback verify_frame_callback = base::NullCallback());

  // Returns true if the recording service is currently recording audio.
  bool IsDoingAudioRecording() const;

  // Returns the number of audio capturers currently owned by the recording
  // service.
  int GetNumberOfAudioCapturers() const;

 private:
  // The actual recording service instance.
  RecordingService recording_service_;
};

}  // namespace recording

#endif  // CHROMEOS_ASH_SERVICES_RECORDING_RECORDING_SERVICE_TEST_API_H_
