// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_RECORDING_RECORDING_ENCODER_H_
#define CHROMEOS_ASH_SERVICES_RECORDING_RECORDING_ENCODER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "chromeos/ash/services/recording/recording_file_io_helper.h"
#include "chromeos/ash/services/recording/rgb_video_frame.h"
#include "media/base/encoder_status.h"
#include "media/base/video_encoder.h"
#include "media/base/video_types.h"

namespace media {
class AudioBus;
class VideoFrame;
}  // namespace media

namespace recording {

namespace mojom {
enum class RecordingStatus;
}  // namespace mojom

// Defines a callback type to notify the user of RecordingEncoder of a failure
// while encoding audio or video frames.
using OnFailureCallback =
    base::OnceCallback<void(mojom::RecordingStatus status)>;

// Defines a callback that can be retrieved from the encoder in order to be
// called repeatedly by the client to provide the audio buses along with their
// timestamps so that they can get encoded and muxed with the video frames.
using EncodeAudioCallback =
    base::RepeatingCallback<void(std::unique_ptr<media::AudioBus> audio_bus,
                                 base::TimeTicks audio_capture_time)>;

// Defines a common interface for encoding audio and video frames. The concrete
// implementation classes decides how encoding is done, and the type of the
// underlying actual encoders.
class RecordingEncoder : public RecordingFileIoHelper::Delegate {
 public:
  // Defines an interface for the actual encoders types to provide the
  // capabilities that they support.
  class Capabilities {
   public:
    Capabilities() = default;
    Capabilities(const Capabilities&) = delete;
    Capabilities& operator=(const Capabilities&) = delete;
    virtual ~Capabilities() = default;

    // Returns the pixel format that the encoder supports, which should be used
    // to initialize the frame sink video capturer in the GPU. Video frames
    // received from the capturer should be in this pixel format.
    virtual media::VideoPixelFormat GetSupportedPixelFormat() const = 0;

    // Returns whether the encoder supports size changes of the video frames
    // during recording (e.g. due to screen rotation, DSF changes, ... etc.).
    // Size changes during recording require a reconfiguration of the video
    // encoder.
    virtual bool SupportsVideoFrameSizeChanges() const = 0;

    // Returns whether the encoder supports the extracted `RgbVideoFrame` from
    // the `media::VideoFrame` directly. This allows us to discard the
    // `media::VideoFrame` very early upon reception so that it can be returned
    // immediately to viz capturer buffer pool (see b/316588576).
    virtual bool SupportsRgbVideoFrame() const = 0;
  };

  explicit RecordingEncoder(OnFailureCallback on_failure_callback);
  RecordingEncoder(const RecordingEncoder&) = delete;
  RecordingEncoder& operator=(const RecordingEncoder&) = delete;
  ~RecordingEncoder() override;

  bool did_failure_occur() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return !on_failure_callback_;
  }

  // Creates and initializes the video encoder if none exists, or recreates and
  // reinitializes it otherwise (if the underlying encoder type supports this
  // behavior, i.e. when the video frame dimensions may need to change
  // dynamically (such as when a recorded window gets moved to a display with
  // different bounds)).
  virtual void InitializeVideoEncoder(
      const media::VideoEncoder::Options& video_encoder_options) = 0;

  // Encodes and muxes the given video `frame`. Clients must check first
  // `Capabilities::SupportsRgbVideoFrame()` to determine which frame type to
  // provide (i.e. `media::VideoFrame` or `RgbVideoFrame`.
  virtual void EncodeVideo(scoped_refptr<media::VideoFrame> frame) = 0;
  virtual void EncodeRgbVideo(RgbVideoFrame rgb_video_frame) = 0;

  // Returns a callback bound to this object that can be called repeatedly by
  // the client to provide the audio buses along with their timestamps so that
  // they can get encoded and muxed by the encoder.
  // Note that the underlying encoder type may not support encoding audio (e.g.
  // the GIF encoder).
  virtual EncodeAudioCallback GetEncodeAudioCallback() = 0;

  // Audio and video encoders as well as the WebmMuxer may buffer several frames
  // before they're processed. It is important to flush all those buffers before
  // releasing this object so as not to drop the final portion of the recording.
  // `on_done` will be called when all remaining buffered frames have been
  // processed and written to the output file.
  // By default, `on_done` will be called on the same sequence on which this
  // encoder is running, unless the caller binds it to another sequence by means
  // of `base::BindPostTask()`.
  virtual void FlushAndFinalize(base::OnceClosure on_done) = 0;

  // RecordingFileIoHelper::Delegate:
  void NotifyFailure(mojom::RecordingStatus status) override;

 protected:
  // Called by both the audio and video encoders to provide the `status` of
  // encoding tasks.
  void OnEncoderStatus(bool for_video, media::EncoderStatus status);

  SEQUENCE_CHECKER(sequence_checker_);

  // A callback triggered when a failure happens during encoding. Once
  // triggered, this callback is null, and therefore indicates that a failure
  // occurred (See `did_failure_occur()` above).
  // This has to be the first thing created, so it's the last thing that gets
  // destroyed, since any failure in the encoders or muxer relies on this
  // callback to notify the service about the failure.
  // See https://crbug.com/1255090.
  OnFailureCallback on_failure_callback_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace recording

#endif  // CHROMEOS_ASH_SERVICES_RECORDING_RECORDING_ENCODER_H_
