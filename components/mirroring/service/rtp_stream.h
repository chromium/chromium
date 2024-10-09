// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_RTP_STREAM_H_
#define COMPONENTS_MIRRORING_SERVICE_RTP_STREAM_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/base/audio_bus.h"
#include "media/cast/cast_config.h"
#include "media/cast/constants.h"
#include "media/video/video_encode_accelerator.h"

namespace media {

class VideoFrame;

namespace cast {
class VideoSender;
class AudioSender;
}  // namespace cast

}  // namespace media

namespace mirroring {

class COMPONENT_EXPORT(MIRRORING_SERVICE) RtpStreamClient {
 public:
  virtual ~RtpStreamClient() = default;

  // Called when error happened during streaming.
  virtual void OnError(const std::string& message) = 0;

  // Request a fresh video frame from the capturer.
  virtual void RequestRefreshFrame() = 0;

  // The VEA is necessary for hardware encoding.
  virtual void CreateVideoEncodeAccelerator(
      media::cast::ReceiveVideoEncodeAcceleratorCallback callback) = 0;

};

// Receives video frames and submits the data to media::cast::VideoSender. It
// also includes a timer to request refresh frames when the source halts (e.g.,
// a screen capturer stops delivering frames because the screen is not being
// updated). When a halt is detected, refresh frames will be requested at
// intervals `refresh_interval` apart for a short period of time. This provides
// the video encoder, downstream, several copies of the last frame so that it
// may clear up lossy encoding artifacts.
//
// Note that this mostly calls through to the media::cast::VideoSender, and the
// refresh frame logic could be factored out into a separate object.
// TODO(issues.chromium.org/329781397): Remove unnecessary wrapper objects in
// Chrome's implementation of the Cast sender.
class COMPONENT_EXPORT(MIRRORING_SERVICE) VideoRtpStream final {
 public:
  VideoRtpStream(std::unique_ptr<media::cast::VideoSender> video_sender,
                 base::WeakPtr<RtpStreamClient> client,
                 base::TimeDelta refresh_interval);

  VideoRtpStream(const VideoRtpStream&) = delete;
  VideoRtpStream& operator=(const VideoRtpStream&) = delete;

  ~VideoRtpStream();

  // Called by VideoCaptureClient when a video frame is received.
  // |video_frame| is required to provide REFERENCE_TIME in the metadata.
  void InsertVideoFrame(scoped_refptr<media::VideoFrame> video_frame);

  void SetTargetPlayoutDelay(base::TimeDelta playout_delay);
  base::TimeDelta GetTargetPlayoutDelay() const;

  base::WeakPtr<VideoRtpStream> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void OnRefreshTimerFired();

  const std::unique_ptr<media::cast::VideoSender> video_sender_;
  const base::WeakPtr<RtpStreamClient> client_;

  // The time between requests for refresh frames. If zero, no refresh frames
  // will be requested.
  base::TimeDelta refresh_interval_;

  // Requests refresh frames at a constant rate while the source is paused, up
  // to a consecutive maximum.
  base::RepeatingTimer refresh_timer_;

  // Set to true when a request for a refresh frame has been made.  This is
  // cleared once the next frame is received.
  bool expecting_a_refresh_frame_{false};

  base::WeakPtrFactory<VideoRtpStream> weak_ptr_factory_{this};

  friend class RtpStreamTest;
};

// Receives audio data and submits the data to media::cast::AudioSender.
// Note that this mostly calls through to the media::cast::VideoSender, and the
// refresh frame logic could be factored out into a separate object.
//
// NOTE: This is a do-nothing wrapper over the underlying AudioSender.
// TODO(issues.chromium.org/329781397): Remove unnecessary wrapper objects in
// Chrome's implementation of the Cast sender.
class COMPONENT_EXPORT(MIRRORING_SERVICE) AudioRtpStream final {
 public:
  AudioRtpStream(std::unique_ptr<media::cast::AudioSender> audio_sender,
                 base::WeakPtr<RtpStreamClient> client);

  AudioRtpStream(const AudioRtpStream&) = delete;
  AudioRtpStream& operator=(const AudioRtpStream&) = delete;

  ~AudioRtpStream();

  // Called by AudioCaptureClient when new audio data is available.
  void InsertAudio(std::unique_ptr<media::AudioBus> audio_bus,
                   base::TimeTicks estimated_capture_time);

  void SetTargetPlayoutDelay(base::TimeDelta playout_delay);
  base::TimeDelta GetTargetPlayoutDelay() const;

  // Get the real time encoder bitrate usage. Note that not all encoders support
  // changing the bitrate in realtime.
  int GetEncoderBitrate() const;

  base::WeakPtr<AudioRtpStream> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const std::unique_ptr<media::cast::AudioSender> audio_sender_;
  const base::WeakPtr<RtpStreamClient> client_;
  base::WeakPtrFactory<AudioRtpStream> weak_ptr_factory_{this};
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_RTP_STREAM_H_
