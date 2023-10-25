// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_RECORDING_RECORDING_SERVICE_H_
#define CHROMEOS_ASH_SERVICES_RECORDING_RECORDING_SERVICE_H_

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "chromeos/ash/services/recording/public/mojom/recording_service.mojom.h"
#include "chromeos/ash/services/recording/recording_encoder.h"
#include "chromeos/ash/services/recording/video_capture_params.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_parameters.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom.h"
#include "ui/gfx/image/image_skia.h"

namespace recording {

class AudioStreamMixer;
class RecordingServiceTestApi;

// Implements the mojo interface of the recording service which handles
// recording audio and video of the screen or portion of it, and writes the
// encoded video chunks directly to a file at a path provided to the Record*()
// functions.
class RecordingService : public mojom::RecordingService,
                         public viz::mojom::FrameSinkVideoConsumer {
 public:
  explicit RecordingService(
      mojo::PendingReceiver<mojom::RecordingService> receiver);
  RecordingService(const RecordingService&) = delete;
  RecordingService& operator=(const RecordingService&) = delete;
  ~RecordingService() override;

  // mojom::RecordingService:
  void RecordFullscreen(
      mojo::PendingRemote<mojom::RecordingServiceClient> client,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoCapturer> video_capturer,
      mojo::PendingRemote<media::mojom::AudioStreamFactory>
          microphone_stream_factory,
      mojo::PendingRemote<media::mojom::AudioStreamFactory>
          system_audio_stream_factory,
      mojo::PendingRemote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate,
      const base::FilePath& output_file_path,
      const viz::FrameSinkId& frame_sink_id,
      const gfx::Size& frame_sink_size_dip,
      float device_scale_factor) override;
  void RecordWindow(
      mojo::PendingRemote<mojom::RecordingServiceClient> client,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoCapturer> video_capturer,
      mojo::PendingRemote<media::mojom::AudioStreamFactory>
          microphone_stream_factory,
      mojo::PendingRemote<media::mojom::AudioStreamFactory>
          system_audio_stream_factory,
      mojo::PendingRemote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate,
      const base::FilePath& output_file_path,
      const viz::FrameSinkId& frame_sink_id,
      const gfx::Size& frame_sink_size_dip,
      float device_scale_factor,
      const viz::SubtreeCaptureId& subtree_capture_id,
      const gfx::Size& window_size_dip) override;
  void RecordRegion(
      mojo::PendingRemote<mojom::RecordingServiceClient> client,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoCapturer> video_capturer,
      mojo::PendingRemote<media::mojom::AudioStreamFactory>
          microphone_stream_factory,
      mojo::PendingRemote<media::mojom::AudioStreamFactory>
          system_audio_stream_factory,
      mojo::PendingRemote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate,
      const base::FilePath& output_file_path,
      const viz::FrameSinkId& frame_sink_id,
      const gfx::Size& frame_sink_size_dip,
      float device_scale_factor,
      const gfx::Rect& crop_region_dip) override;
  void StopRecording() override;
  void OnRecordedWindowChangingRoot(const viz::FrameSinkId& new_frame_sink_id,
                                    const gfx::Size& new_frame_sink_size_dip,
                                    float new_device_scale_factor) override;
  void OnRecordedWindowSizeChanged(
      const gfx::Size& new_window_size_dip) override;
  void OnFrameSinkSizeChanged(const gfx::Size& new_frame_sink_size_dip,
                              float new_device_scale_factor) override;

  // viz::mojom::FrameSinkVideoConsumer:
  void OnFrameCaptured(
      media::mojom::VideoBufferHandlePtr data,
      media::mojom::VideoFrameInfoPtr info,
      const gfx::Rect& content_rect,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
          callbacks) override;
  void OnNewSubCaptureTargetVersion(
      uint32_t sub_capture_target_version) override;
  void OnFrameWithEmptyRegionCapture() override;
  void OnStopped() override;
  void OnLog(const std::string& message) override;

 private:
  friend class RecordingServiceTestApi;

  void StartNewRecording(
      mojo::PendingRemote<mojom::RecordingServiceClient> client,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoCapturer> video_capturer,
      mojo::PendingRemote<media::mojom::AudioStreamFactory>
          microphone_stream_factory,
      mojo::PendingRemote<media::mojom::AudioStreamFactory>
          system_audio_stream_factory,
      mojo::PendingRemote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate,
      const base::FilePath& output_file_path,
      std::unique_ptr<VideoCaptureParams> capture_params);

  // Called asynchronously by the encoder to provide the `callback` that will be
  // called repeatedly by the `audio_stream_mixer_` to provide the mixed audio
  // buses along with their timestamps to the encoder. This happens only if we
  // are recording audio.
  void OnEncodeAudioCallbackReady(EncodeAudioCallback callback);

  // Called on the main thread during an on-going recording to reconfigure an
  // existing video encoder.
  void ReconfigureVideoEncoder();

  // Called on the main thread to end the recording with the given |status|.
  void TerminateRecording(mojom::RecordingStatus status);

  // Binds the given |video_capturer| to |video_capturer_remote_| and starts
  // video according to the current |current_video_capture_params_|.
  void ConnectAndStartVideoCapturer(
      mojo::PendingRemote<viz::mojom::FrameSinkVideoCapturer> video_capturer);

  // If the video capturer gets disconnected (e.g. Viz crashes) during an
  // ongoing recording, this attempts to reconnect to a new capturer and resumes
  // capturing with the same |current_video_capture_params_|.
  void OnVideoCapturerDisconnected();

  // This is called by |encoder_muxer_| on the main thread (since we bound it as
  // a callback to be invoked on the main thread. See BindOnceToMainThread()),
  // when a failure of type |status| occurs during audio or video encoding. This
  // ends the ongoing recording and signals to the client that a failure
  // occurred.
  void OnEncodingFailure(mojom::RecordingStatus status);

  // At the end of recording we ask the |encoder_muxer_| to flush and process
  // any buffered frames. When this completes this function is called on the
  // main thread (since it's bound as a callback to be invoked on the main
  // thread. See BindOnceToMainThread()). |status| indicates the recording
  // termination status that lead to this flushing of the encoders and muxer.
  void OnEncoderMuxerFlushed(mojom::RecordingStatus status);

  // Called on the main thread to tell the client that recording ended,
  // providing it with the |status| of the recording as well as a chached
  // thumbnail of the video (if available).
  void SignalRecordingEndedToClient(mojom::RecordingStatus status);

  // Called when `refresh_timer_` fires, which means it has been more than
  // `kVideoFramesRefreshInterval` since the last video frame was delivered, at
  // which point we request a new refresh video frame.
  void OnRefreshTimerFired();

  // Stops audio recording if any is being done.
  void MaybeStopAudioRecording();

  // By default, the `encoder_muxer_` will invoke any callback we provide it
  // with to notify us of certain events (such as failure errors, or flush done)
  // on the `encoding_task_runner_`'s sequence. But since these callbacks are
  // invoked asynchronously from other threads, they may get invoked after this
  // RecordingService instance had been destroyed. Therefore, we need to bind
  // these callbacks to weak ptrs, to prevent them from invoking after this
  // object's destruction. However, this won't work, since weak ptrs cannot be
  // invalidated except on the sequence on which they were invoked on. Hence, we
  // must make sure these callbacks are invoked on the main thread.
  //
  // The below is a convenience method to bind once callbacks to weak ptrs that
  // would only be invoked on the main thread.
  template <typename Functor, typename... Args>
  auto BindOnceToMainThread(Functor&& functor, Args&&... args) {
    return base::BindPostTask(main_task_runner_,
                              base::BindOnce(std::forward<Functor>(functor),
                                             weak_ptr_factory_.GetWeakPtr(),
                                             std::forward<Args>(args)...));
  }

  THREAD_CHECKER(main_thread_checker_);

  // The audio parameters that will be used when recording audio.
  const media::AudioParameters audio_parameters_;

  // The mojo receiving end of the service.
  mojo::Receiver<mojom::RecordingService> receiver_;

  // The mojo receiving end of the service as a FrameSinkVideoConsumer.
  mojo::Receiver<viz::mojom::FrameSinkVideoConsumer> consumer_receiver_
      GUARDED_BY_CONTEXT(main_thread_checker_);

  // A task runner to post tasks on the main thread of the recording service.
  // It can be accessed from any thread.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // A sequenced blocking pool task runner used to run all encoding and muxing
  // tasks on. Can be accessed from any thread.
  scoped_refptr<base::SequencedTaskRunner> encoding_task_runner_;

  // A mojo remote end of client of this service (e.g. Ash). There can only be
  // a single client of this service.
  mojo::Remote<mojom::RecordingServiceClient> client_remote_
      GUARDED_BY_CONTEXT(main_thread_checker_);

  // A callback used for testing, which will be triggered when a video frame is
  // delivered to the service from the Viz capturer.
  using OnVideoFrameDeliveredCallback =
      base::OnceCallback<void(const media::VideoFrame& frame,
                              const gfx::Rect& content_rect)>;
  OnVideoFrameDeliveredCallback on_video_frame_delivered_callback_for_testing_
      GUARDED_BY_CONTEXT(main_thread_checker_);

  // A timer used to request refresh video frames if none was delivered within
  // a certain time interval (which can happen when the contents of the surface
  // being recorded is static, resulting in no damage).
  base::RepeatingTimer refresh_timer_ GUARDED_BY_CONTEXT(main_thread_checker_);

  // A cached scaled down rgb image of the first valid video frame which will be
  // used to provide the client with an image thumbnail representing the
  // recorded video.
  gfx::ImageSkia video_thumbnail_ GUARDED_BY_CONTEXT(main_thread_checker_);

  // True if a failure has been propagated from |encoder_muxer_| that we will
  // end recording abruptly and ignore any incoming audio/video frames.
  bool did_failure_occur_ GUARDED_BY_CONTEXT(main_thread_checker_) = false;

  // The parameters of the current ongoing video capture. This object knows how
  // to initialize the video capturer depending on which capture source
  // (fullscreen, window, or region) is currently being recorded. It is set to
  // a nullptr when there's no recording happening.
  std::unique_ptr<VideoCaptureParams> current_video_capture_params_
      GUARDED_BY_CONTEXT(main_thread_checker_);

  // The mojo remote end used to interact with a video capturer living on Viz.
  mojo::Remote<viz::mojom::FrameSinkVideoCapturer> video_capturer_remote_
      GUARDED_BY_CONTEXT(main_thread_checker_);

  // The audio stream mixer that will be created only if we are capturing any
  // audio stream. The mixer creates and owns the audio capturers that we
  // require. If the mixer exists, it must contain at least a single capturer;
  // if either microphone or system audio recording was requested, or contains
  // two capturers if both are desired to be recorded an mixed together in one
  // stream.
  // All the operations performed by the mixer (including its construction and
  // destruction) are done on `encoding_task_runner_` to avoid stalling the main
  // thread (on which the video frames are received).
  base::SequenceBound<AudioStreamMixer> audio_stream_mixer_
      GUARDED_BY_CONTEXT(main_thread_checker_);

  // Abstracts querying the supported capabilities of the currently used encoder
  // type.
  std::unique_ptr<RecordingEncoder::Capabilities> encoder_capabilities_
      GUARDED_BY_CONTEXT(main_thread_checker_);

  // Performs all encoding and muxing operations asynchronously on the
  // |encoding_task_runner_|. However, the |encoder_muxer_| object itself is
  // constructed, used, and destroyed on the main thread sequence.
  base::SequenceBound<RecordingEncoder> encoder_muxer_
      GUARDED_BY_CONTEXT(main_thread_checker_);

  // The number of times the video encoder was reconfigured as a result of a
  // change in the video size in the middle of recording (See
  // ReconfigureVideoEncoder()).
  int number_of_video_encoder_reconfigures_
      GUARDED_BY_CONTEXT(main_thread_checker_) = 0;

  base::WeakPtrFactory<RecordingService> weak_ptr_factory_{this};
};

}  // namespace recording
#endif  // CHROMEOS_ASH_SERVICES_RECORDING_RECORDING_SERVICE_H_
