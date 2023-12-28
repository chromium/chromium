// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_RECORDING_WEBM_ENCODER_MUXER_H_
#define CHROMEOS_ASH_SERVICES_RECORDING_WEBM_ENCODER_MUXER_H_

#include <memory>
#include <optional>

#include "base/containers/circular_deque.h"
#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "chromeos/ash/services/recording/recording_encoder.h"
#include "media/audio/audio_opus_encoder.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/muxers/muxer_timestamp_adapter.h"
#include "media/video/vpx_video_encoder.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class FilePath;
}

namespace recording {

namespace mojom {
class DriveFsQuotaDelegate;
}  // namespace mojom

// Encapsulates encoding and muxing audio and video frame. An instance of this
// object can only be interacted with via a |base::SequenceBound| wrapper, which
// guarantees all encoding and muxing operations as well as destruction of the
// instance are done on the sequenced |blocking_task_runner| given to Create().
// This prevents expensive encoding and muxing operations from blocking the main
// thread of the recording service, on which the video frames are delivered.
//
// This object performs VP8 video encoding and Opus audio encoding, and mux the
// audio and video encoded frames into a Webm container.
class WebmEncoderMuxer : public RecordingEncoder {
 private:
  using PassKey = base::PassKey<WebmEncoderMuxer>;

 public:
  // Creates an instance of this class that is bound to the given sequenced
  // |blocking_task_runner| on which all operations as well as destruction will
  // happen. |video_encoder_options| and |audio_input_params| will be used to
  // initialize the video and audio encoders respectively.
  // If |audio_input_params| is nullptr, then the service is not recording
  // audio, and the muxer will be initialized accordingly.
  // the webm muxer chunks will be written directly to a file at the given
  // |webm_file_path|.
  // |on_failure_callback| will be called to inform the owner of this object of
  // a failure, after which all subsequent calls to EncodeVideo() and
  // EncodeAudio() will be ignored.
  //
  // By default, |on_failure_callback| will be called on the same sequence of
  // |blocking_task_runner| (unless the caller binds the given callback to a
  // different sequence by means of base::BindPostTask()).
  static base::SequenceBound<RecordingEncoder> Create(
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      const media::VideoEncoder::Options& video_encoder_options,
      const media::AudioParameters* audio_input_params,
      mojo::PendingRemote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate,
      const base::FilePath& webm_file_path,
      OnFailureCallback on_failure_callback);

  // Creates and returns an object that specifies the capabilities of this
  // encoder.
  static std::unique_ptr<Capabilities> CreateCapabilities();

  WebmEncoderMuxer(
      PassKey,
      const media::VideoEncoder::Options& video_encoder_options,
      const media::AudioParameters* audio_input_params,
      mojo::PendingRemote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate,
      const base::FilePath& webm_file_path,
      OnFailureCallback on_failure_callback);
  WebmEncoderMuxer(const WebmEncoderMuxer&) = delete;
  WebmEncoderMuxer& operator=(const WebmEncoderMuxer&) = delete;
  ~WebmEncoderMuxer() override;

  // RecordingEncoder:
  void InitializeVideoEncoder(
      const media::VideoEncoder::Options& video_encoder_options) override;
  void EncodeVideo(scoped_refptr<media::VideoFrame> frame) override;
  void EncodeRgbVideo(RgbVideoFrame rgb_video_frame) override;
  EncodeAudioCallback GetEncodeAudioCallback() override;
  void FlushAndFinalize(base::OnceClosure on_done) override;

 private:
  struct AudioFrame {
    AudioFrame(std::unique_ptr<media::AudioBus>, base::TimeTicks);
    AudioFrame(AudioFrame&&);
    ~AudioFrame();

    std::unique_ptr<media::AudioBus> bus;
    base::TimeTicks capture_time;
  };

  // Gathers the video frame data needed when the frame is encoded and ready to
  // be submitted to the muxer.
  struct EncodedVideoFrameParams {
    // The received video frame's reference time. See
    // `media::VideoFrameMetadata::reference_time`.
    base::TimeTicks frame_reference_time;

    // The size of the visible region of the received video frame. Note that the
    // visible rect sizes may change from frame to frame (e.g. when recording a
    // window, and the window gets resized).
    gfx::Size visible_rect_size;
  };

  // Creates and initializes the audio encoder.
  void InitializeAudioEncoder(const media::AudioEncoder::Options& options);

  // Called when the audio encoder is initialized to provide the |status| of
  // the initialization.
  void OnAudioEncoderInitialized(media::EncoderStatus status);

  // Called when the video |encoder| is initialized to provide the |status| of
  // the initialization. If initialization failed, |on_failure_callback_| will
  // be triggered.
  void OnVideoEncoderInitialized(media::VpxVideoEncoder* encoder,
                                 media::EncoderStatus status);

  // Encodes and muxes the given audio frames in `audio_bus` captured at
  // `capture_time`. If `did_failure_occur()` is true, all `audio_bus`s will be
  // ignored.
  // It is bound to the callback returned by `GetEncodeAudioCallback()`.
  void EncodeAudio(std::unique_ptr<media::AudioBus> audio_bus,
                   base::TimeTicks capture_time);

  // Performs the actual encoding of the given audio |frame|. It should never be
  // called before the audio encoder is initialized. Audio frames received
  // before initialization should be added to |pending_audio_frames_| and
  // handled once initialization is complete.
  void EncodeAudioImpl(AudioFrame frame);

  // Performs the actual encoding of the given video |frame|. It should never be
  // called before the video encoder is initialized. Video frames received
  // before initialization should be added to |pending_video_frames_| and
  // handled once initialization is complete.
  void EncodeVideoImpl(scoped_refptr<media::VideoFrame> frame);

  // Called by the video encoder to provide the encoded video frame `output`,
  // which will then be sent to the muxer.
  void OnVideoEncoderOutput(
      media::VideoEncoderOutput output,
      std::optional<media::VideoEncoder::CodecDescription> codec_description);

  // Called by the audio encoder to provide the |encoded_audio|.
  void OnAudioEncoded(
      media::EncodedAudioBuffer encoded_audio,
      std::optional<media::AudioEncoder::CodecDescription> codec_description);

  // Called when the audio encoder flushes all its buffered frames, at which
  // point we can flush the video encoder. |on_done| will be passed to
  // OnVideoEncoderFlushed()
  void OnAudioEncoderFlushed(base::OnceClosure on_done,
                             media::EncoderStatus status);

  // Called when the video encoder flushes all its buffered frames, at which
  // point we can flush the muxer. |on_done| will be called to signal that
  // flushing is complete.
  void OnVideoEncoderFlushed(base::OnceClosure on_done,
                             media::EncoderStatus status);

  std::unique_ptr<media::VpxVideoEncoder> video_encoder_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<media::AudioOpusEncoder> audio_encoder_
      GUARDED_BY_CONTEXT(sequence_checker_);

  media::MuxerTimestampAdapter muxer_adapter_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Holds video frames that were received before the video encoder is
  // initialized, so that they can be processed once initialization is complete.
  base::circular_deque<scoped_refptr<media::VideoFrame>> pending_video_frames_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Holds audio frames that were received before the audio encoder is
  // initialized, so that they can be processed once initialization is complete.
  base::circular_deque<AudioFrame> pending_audio_frames_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The total number of frames that we dropped to keep the size of
  // |pending_video_frames_| limited to |kMaxPendingFrames| to avoid consuming
  // too much memory, or stalling the capturer since it has a maximum number of
  // in-flight frames at a time.
  size_t num_dropped_frames_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // A queue containing the parameters of the encoded video frames in the same
  // order of their encoding. These parameters are used when submitting the
  // video encoder output to the muxer. See `OnVideoEncoderOutput()`.
  base::queue<EncodedVideoFrameParams> encoded_video_params_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // True once video encoder is initialized successfully.
  bool is_video_encoder_initialized_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  // True once audio encoder is initialized successfully.
  bool is_audio_encoder_initialized_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  base::WeakPtrFactory<WebmEncoderMuxer> weak_ptr_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace recording

#endif  // CHROMEOS_ASH_SERVICES_RECORDING_WEBM_ENCODER_MUXER_H_
