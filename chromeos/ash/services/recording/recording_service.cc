// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/recording/recording_service.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chromeos/ash/services/recording/audio_capture_util.h"
#include "chromeos/ash/services/recording/audio_stream_mixer.h"
#include "chromeos/ash/services/recording/gif_encoder.h"
#include "chromeos/ash/services/recording/recording_encoder.h"
#include "chromeos/ash/services/recording/recording_service_constants.h"
#include "chromeos/ash/services/recording/rgb_video_frame.h"
#include "chromeos/ash/services/recording/video_capture_params.h"
#include "chromeos/ash/services/recording/webm_encoder_muxer.h"
#include "media/audio/audio_device_description.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "services/audio/public/cpp/device_factory.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace recording {

namespace {

// For a capture size of 320 by 240, we use a bitrate of 256 kbit/s. This value
// is used as the minimum bitrate that we don't go below regardless of the video
// size.
constexpr uint32_t kMinBitrateInBitsPerSecond = 256 * 1000;

// The size (in DIPs) within which we will try to fit a thumbnail image
// extracted from the first valid video frame. The value was chosen to be
// suitable with the image container in the notification UI.
constexpr gfx::Size kThumbnailSize{328, 184};

// Video frames are generated when the recorded surface has damage. If the
// contents of the surface is static, no new video frames will be generated, and
// as a result, seeking through the output video may produce garbage play-back
// video frames (see http://b/238505291).
// To avoid this issue, we use the below duration as the maximum time to wait
// since the last received video frames before we request a new refresh frame.
// This value was chosen by trial and error, and it also matches the one used by
// Chromecast.
constexpr base::TimeDelta kVideoFramesRefreshInterval = base::Milliseconds(250);

// Calculates the bitrate (in bits/seconds) used to initialize the video encoder
// based on the given |capture_size|.
uint32_t CalculateVpxEncoderBitrate(const gfx::Size& capture_size) {
  // We use the Kush Gauge formula which goes like this:
  // bitrate (bits/s) = width * height * frame rate * motion factor * 0.07.
  // Here we use a motion factor = 1, which works well for our use cases.
  // This formula gives a balance between the video quality and the file size so
  // it doesn't become too large.
  const uint32_t bitrate =
      std::ceil(capture_size.GetArea() * kMaxFrameRate * 0.07f);

  // Make sure to return a value that is divisible by 8 so that we work with
  // whole bytes.
  return std::max(kMinBitrateInBitsPerSecond, (bitrate & ~7));
}

// Given the desired |capture_size|, it creates and returns the options needed
// to configure the video encoder.
media::VideoEncoder::Options CreateVideoEncoderOptions(
    const gfx::Size& capture_size) {
  media::VideoEncoder::Options video_encoder_options;
  video_encoder_options.bitrate =
      media::Bitrate::ConstantBitrate(CalculateVpxEncoderBitrate(capture_size));
  video_encoder_options.framerate = kMaxFrameRate;
  video_encoder_options.frame_size = capture_size;
  // This value, expressed as a number of frames, forces the encoder to code
  // a keyframe if one has not been coded in the last keyframe_interval frames.
  video_encoder_options.keyframe_interval = 100;
  return video_encoder_options;
}

// Extracts a potentially scaled-down RGB image from the given video |frame|,
// which is suitable to use as a thumbnail for the video.
gfx::ImageSkia ExtractImageFromVideoFrame(const media::VideoFrame& frame) {
  const gfx::Size visible_size = frame.visible_rect().size();
  media::PaintCanvasVideoRenderer renderer;
  SkBitmap bitmap;
  bitmap.allocN32Pixels(visible_size.width(), visible_size.height());
  renderer.ConvertVideoFrameToRGBPixels(&frame, bitmap.getPixels(),
                                        bitmap.rowBytes());

  // Since this image will be used as a thumbnail, we can scale it down to save
  // on memory if needed. For example, if recording a FHD display, that will be
  // (for 12 bits/pixel):
  // 1920 * 1080 * 12 / 8, which is approx. = 3 MB, which is a lot to keep
  // around for a thumbnail.
  const gfx::ImageSkia thumbnail = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  if (visible_size.width() <= kThumbnailSize.width() &&
      visible_size.height() <= kThumbnailSize.height()) {
    return thumbnail;
  }

  const gfx::Size scaled_size =
      media::ScaleSizeToFitWithinTarget(visible_size, kThumbnailSize);
  return gfx::ImageSkiaOperations::CreateResizedImage(
      thumbnail, skia::ImageOperations::ResizeMethod::RESIZE_BETTER,
      scaled_size);
}

// Called when the channel to the client of the recording service gets
// disconnected. At that point, there's nothing useful to do here, and instead
// of wasting resources encoding/muxing remaining frames, and flushing the
// buffers, we terminate the recording service process immediately.
void TerminateServiceImmediately() {
  LOG(ERROR)
      << "The recording service client was disconnected. Exiting immediately.";
  std::exit(EXIT_FAILURE);
}

// Creates the appropriate encoder capabilities based to the type of the given
// `output_file_path`.
std::unique_ptr<RecordingEncoder::Capabilities> CreateEncoderCapabilities(
    const base::FilePath& output_file_path) {
  return output_file_path.MatchesExtension(".gif")
             ? GifEncoder::CreateCapabilities()
             : WebmEncoderMuxer::CreateCapabilities();
}

// Creates and returns the appropriate encoder based on the type of the given
// `output_file_path`.
base::SequenceBound<RecordingEncoder> CreateEncoder(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const media::VideoEncoder::Options& video_encoder_options,
    const media::AudioParameters* audio_input_params,
    mojo::PendingRemote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate,
    const base::FilePath& output_file_path,
    OnFailureCallback on_failure_callback) {
  if (output_file_path.MatchesExtension(".gif")) {
    DCHECK(!audio_input_params);

    return GifEncoder::Create(std::move(blocking_task_runner),
                              video_encoder_options,
                              std::move(drive_fs_quota_delegate),
                              output_file_path, std::move(on_failure_callback));
  }

  DCHECK(output_file_path.MatchesExtension(".webm"));
  return WebmEncoderMuxer::Create(
      std::move(blocking_task_runner), video_encoder_options,
      audio_input_params, std::move(drive_fs_quota_delegate), output_file_path,
      std::move(on_failure_callback));
}

}  // namespace

RecordingService::RecordingService(
    mojo::PendingReceiver<mojom::RecordingService> receiver)
    : audio_parameters_(audio_capture_util::GetAudioCaptureParameters()),
      receiver_(this, std::move(receiver)),
      consumer_receiver_(this),
      main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      encoding_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          // We use |USER_VISIBLE| here as opposed to |BEST_EFFORT| since the
          // latter is extremely low priority and may stall encoding for random
          // reasons.
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

RecordingService::~RecordingService() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  if (!current_video_capture_params_)
    return;

  // If the service gets destructed while recording in progress, the client must
  // be still connected (since otherwise the service process would have been
  // immediately terminated). We attempt to flush whatever we have right now
  // before exiting.
  DCHECK(client_remote_.is_bound());
  DCHECK(client_remote_.is_connected());
  StopRecording();
  video_capturer_remote_.reset();
  consumer_receiver_.reset();
  // Note that we can call FlushAndFinalize() on the |encoder_muxer_| even
  // though it will be done asynchronously on the |encoding_task_runner_| and by
  // then this |RecordingService| instance will have already been gone. This is
  // because the muxer writes directly to the file and does not rely on this
  // instance.
  encoder_muxer_.AsyncCall(&RecordingEncoder::FlushAndFinalize)
      .WithArgs(base::DoNothing());
  SignalRecordingEndedToClient(mojom::RecordingStatus::kServiceClosing);
}

void RecordingService::RecordFullscreen(
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
    float device_scale_factor) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  StartNewRecording(
      std::move(client), std::move(video_capturer),
      std::move(microphone_stream_factory),
      std::move(system_audio_stream_factory),
      std::move(drive_fs_quota_delegate), output_file_path,
      VideoCaptureParams::CreateForFullscreenCapture(
          frame_sink_id, frame_sink_size_dip, device_scale_factor));
}

void RecordingService::RecordWindow(
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
    const gfx::Size& window_size_dip) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  StartNewRecording(std::move(client), std::move(video_capturer),
                    std::move(microphone_stream_factory),
                    std::move(system_audio_stream_factory),
                    std::move(drive_fs_quota_delegate), output_file_path,
                    VideoCaptureParams::CreateForWindowCapture(
                        frame_sink_id, subtree_capture_id, frame_sink_size_dip,
                        device_scale_factor, window_size_dip));
}

void RecordingService::RecordRegion(
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
    const gfx::Rect& crop_region_dip) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  StartNewRecording(std::move(client), std::move(video_capturer),
                    std::move(microphone_stream_factory),
                    std::move(system_audio_stream_factory),
                    std::move(drive_fs_quota_delegate), output_file_path,
                    VideoCaptureParams::CreateForRegionCapture(
                        frame_sink_id, frame_sink_size_dip, device_scale_factor,
                        crop_region_dip));
}

void RecordingService::StopRecording() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  refresh_timer_.Stop();
  video_capturer_remote_->Stop();
  MaybeStopAudioRecording();
}

void RecordingService::OnRecordedWindowChangingRoot(
    const viz::FrameSinkId& new_frame_sink_id,
    const gfx::Size& new_frame_sink_size_dip,
    float new_device_scale_factor) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  if (!current_video_capture_params_) {
    // A recording might terminate before we signal the client with an
    // |OnRecordingEnded()| call.
    return;
  }

  // If there's a change in the pixel size of the recorded window as a result of
  // it moving to a different display, we must reconfigure the video encoder so
  // that output video has the correct dimensions.
  if (current_video_capture_params_->OnRecordedWindowChangingRoot(
          video_capturer_remote_, new_frame_sink_id, new_frame_sink_size_dip,
          new_device_scale_factor)) {
    ReconfigureVideoEncoder();
  }
}

void RecordingService::OnRecordedWindowSizeChanged(
    const gfx::Size& new_window_size_dip) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  if (!current_video_capture_params_) {
    // A recording might terminate before we signal the client with an
    // |OnRecordingEnded()| call.
    return;
  }

  if (current_video_capture_params_->OnRecordedWindowSizeChanged(
          video_capturer_remote_, new_window_size_dip)) {
    ReconfigureVideoEncoder();
  }
}

void RecordingService::OnFrameSinkSizeChanged(
    const gfx::Size& new_frame_sink_size_dip,
    float new_device_scale_factor) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  if (!current_video_capture_params_) {
    // A recording might terminate before we signal the client with an
    // |OnRecordingEnded()| call.
    return;
  }

  // A change in the pixel size of the frame sink may result in changing the
  // pixel size of the captured target (e.g. window or region). we must
  // reconfigure the video encoder so that output video has the correct
  // dimensions.
  if (current_video_capture_params_->OnFrameSinkSizeChanged(
          video_capturer_remote_, new_frame_sink_size_dip,
          new_device_scale_factor)) {
    ReconfigureVideoEncoder();
  }
}

void RecordingService::OnFrameCaptured(
    media::mojom::VideoBufferHandlePtr data,
    media::mojom::VideoFrameInfoPtr info,
    const gfx::Rect& content_rect,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        callbacks) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DCHECK(encoder_muxer_);

  // Once a frame is received, we reset the timer back to the full
  // `kVideoFramesRefreshInterval` so that it fires only if we don't get any
  // new video frames during that interval.
  refresh_timer_.Reset();

  CHECK(data->is_read_only_shmem_region());
  base::ReadOnlySharedMemoryRegion& shmem_region =
      data->get_read_only_shmem_region();

  // The |data| parameter is not nullable and mojo type mapping for
  // `base::ReadOnlySharedMemoryRegion` defines that nullable version of it is
  // the same type, with null check being equivalent to IsValid() check. Given
  // the above, we should never be able to receive a read only shmem region that
  // is not valid - mojo will enforce it for us.
  DCHECK(shmem_region.IsValid());

  // We ignore any subsequent frames after a failure.
  if (did_failure_occur_)
    return;

  base::ReadOnlySharedMemoryMapping mapping = shmem_region.Map();
  if (!mapping.IsValid()) {
    DLOG(ERROR) << "Mapping of video frame shared memory failed.";
    return;
  }

  if (mapping.size() <
      media::VideoFrame::AllocationSize(info->pixel_format, info->coded_size)) {
    DLOG(ERROR) << "Shared memory size was less than expected.";
    return;
  }

  DCHECK(current_video_capture_params_);
  const gfx::Rect& visible_rect =
      current_video_capture_params_->GetVideoFrameVisibleRect(
          info->visible_rect);
  scoped_refptr<media::VideoFrame> frame = media::VideoFrame::WrapExternalData(
      info->pixel_format, info->coded_size, visible_rect, visible_rect.size(),
      reinterpret_cast<const uint8_t*>(mapping.memory()), mapping.size(),
      info->timestamp);
  if (!frame) {
    DLOG(ERROR) << "Failed to create a VideoFrame.";
    return;
  }

  // Takes ownership of |mapping| and |callbacks| to keep them alive until
  // |frame| is released.
  frame->AddDestructionObserver(base::BindOnce(
      [](base::ReadOnlySharedMemoryMapping mapping,
         mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
             callbacks) {},
      std::move(mapping), std::move(callbacks)));
  frame->set_metadata(info->metadata);
  frame->set_color_space(info->color_space);

  if (video_thumbnail_.isNull())
    video_thumbnail_ = ExtractImageFromVideoFrame(*frame);

  if (on_video_frame_delivered_callback_for_testing_) {
    std::move(on_video_frame_delivered_callback_for_testing_)
        .Run(*frame, content_rect);
  }

  if (encoder_capabilities_->SupportsRgbVideoFrame()) {
    // This is the GIF encoding path.
    encoder_muxer_.AsyncCall(&RecordingEncoder::EncodeRgbVideo)
        .WithArgs(RgbVideoFrame(*frame));

    // Note that we no longer need `frame`. `RgbVideoFrame` already copied the
    // pixel colors (which is needed to be able to modify them later when we
    // dither the image). Note that the video `frame`'s memory itself cannot be
    // modified, as it is backed by a read-only shared memory region. This
    // allows us to return the frame early to Viz capturer buffer pool, which
    // has a maximum number of in-flight frames (See b/316588576).
    frame.reset();
    return;
  }

  // This is the WebM path.
  encoder_muxer_.AsyncCall(&RecordingEncoder::EncodeVideo)
      .WithArgs(std::move(frame));
}

void RecordingService::OnNewSubCaptureTargetVersion(
    uint32_t sub_capture_target_version) {}

void RecordingService::OnFrameWithEmptyRegionCapture() {}

void RecordingService::OnStopped() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  // If a failure occurred, we don't wait till the capturer sends us this
  // signal. The recording had already been terminated by now.
  if (!did_failure_occur_)
    TerminateRecording(mojom::RecordingStatus::kSuccess);
}

void RecordingService::OnLog(const std::string& message) {
  DLOG(WARNING) << message;
}

void RecordingService::StartNewRecording(
    mojo::PendingRemote<mojom::RecordingServiceClient> client,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoCapturer> video_capturer,
    mojo::PendingRemote<media::mojom::AudioStreamFactory>
        microphone_stream_factory,
    mojo::PendingRemote<media::mojom::AudioStreamFactory>
        system_audio_stream_factory,
    mojo::PendingRemote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate,
    const base::FilePath& output_file_path,
    std::unique_ptr<VideoCaptureParams> capture_params) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  if (current_video_capture_params_) {
    LOG(ERROR) << "Cannot start a new recording while another is in progress.";
    return;
  }

  client_remote_.reset();
  client_remote_.Bind(std::move(client));
  client_remote_.set_disconnect_handler(
      base::BindOnce(&TerminateServiceImmediately));

  current_video_capture_params_ = std::move(capture_params);
  const bool should_record_audio = microphone_stream_factory.is_valid() ||
                                   system_audio_stream_factory.is_valid();

  encoder_capabilities_ = CreateEncoderCapabilities(output_file_path);
  encoder_muxer_ = CreateEncoder(
      encoding_task_runner_,
      CreateVideoEncoderOptions(current_video_capture_params_->GetVideoSize()),
      should_record_audio ? &audio_parameters_ : nullptr,
      std::move(drive_fs_quota_delegate), output_file_path,
      BindOnceToMainThread(&RecordingService::OnEncodingFailure));

  ConnectAndStartVideoCapturer(std::move(video_capturer));

  if (!should_record_audio)
    return;

  audio_stream_mixer_ = AudioStreamMixer::Create(encoding_task_runner_);

  if (microphone_stream_factory) {
    // Ideally, we should be able to use echo cancellation with the microphone,
    // but due to observed distortion to the user's voice that it may cause, we
    // decided to hold off for now.
    // We use automatic gain control for the microphone capture since depending
    // on the users voice and their environment, the strength and clarity may
    // vary. System audio is just captured as is.
    audio_stream_mixer_.AsyncCall(&AudioStreamMixer::AddAudioCapturer)
        .WithArgs(media::AudioDeviceDescription::kDefaultDeviceId,
                  std::move(microphone_stream_factory),
                  /*use_automatic_gain_control=*/true,
                  /*use_echo_canceller=*/false);
  }

  if (system_audio_stream_factory) {
    audio_stream_mixer_.AsyncCall(&AudioStreamMixer::AddAudioCapturer)
        .WithArgs(media::AudioDeviceDescription::kLoopbackInputDeviceId,
                  std::move(system_audio_stream_factory),
                  /*use_automatic_gain_control=*/false,
                  /*use_echo_canceller=*/false);
  }

  encoder_muxer_.AsyncCall(&RecordingEncoder::GetEncodeAudioCallback)
      .Then(base::BindOnce(&RecordingService::OnEncodeAudioCallbackReady,
                           weak_ptr_factory_.GetWeakPtr()));
}

void RecordingService::OnEncodeAudioCallbackReady(
    EncodeAudioCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  // This can be triggered after we have stopped recording already, e.g. in
  // tests.
  if (audio_stream_mixer_) {
    audio_stream_mixer_.AsyncCall(&AudioStreamMixer::Start)
        .WithArgs(std::move(callback));
  }
}

void RecordingService::ReconfigureVideoEncoder() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DCHECK(current_video_capture_params_);
  DCHECK(encoder_capabilities_);

  if (!encoder_capabilities_->SupportsVideoFrameSizeChanges()) {
    OnEncodingFailure(
        mojom::RecordingStatus::kVideoEncoderReconfigurationFailure);
    return;
  }

  ++number_of_video_encoder_reconfigures_;
  encoder_muxer_.AsyncCall(&RecordingEncoder::InitializeVideoEncoder)
      .WithArgs(CreateVideoEncoderOptions(
          current_video_capture_params_->GetVideoSize()));
}

void RecordingService::TerminateRecording(mojom::RecordingStatus status) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DCHECK(encoder_muxer_);

  refresh_timer_.Stop();
  current_video_capture_params_.reset();
  encoder_capabilities_.reset();
  video_capturer_remote_.reset();
  consumer_receiver_.reset();

  encoder_muxer_.AsyncCall(&RecordingEncoder::FlushAndFinalize)
      .WithArgs(BindOnceToMainThread(&RecordingService::OnEncoderMuxerFlushed,
                                     status));
}

void RecordingService::ConnectAndStartVideoCapturer(
    mojo::PendingRemote<viz::mojom::FrameSinkVideoCapturer> video_capturer) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DCHECK(current_video_capture_params_);
  DCHECK(encoder_capabilities_);

  video_capturer_remote_.reset();
  video_capturer_remote_.Bind(std::move(video_capturer));
  // The GPU process could crash while recording is in progress, and the video
  // capturer will be disconnected. We need to handle this event gracefully.
  video_capturer_remote_.set_disconnect_handler(base::BindOnce(
      &RecordingService::OnVideoCapturerDisconnected, base::Unretained(this)));
  current_video_capture_params_->InitializeVideoCapturer(
      video_capturer_remote_, encoder_capabilities_->GetSupportedPixelFormat());
  video_capturer_remote_->Start(consumer_receiver_.BindNewPipeAndPassRemote(),
                                viz::mojom::BufferFormatPreference::kDefault);

  // Always request the very first frame, and don't rely on damage to produce
  // it.
  video_capturer_remote_->RequestRefreshFrame();

  refresh_timer_.Start(FROM_HERE, kVideoFramesRefreshInterval, this,
                       &RecordingService::OnRefreshTimerFired);
}

void RecordingService::OnVideoCapturerDisconnected() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  // On a crash in the GPU, the video capturer gets disconnected, so we can't
  // communicate with it any longer, but we can still communicate with the audio
  // capturer. We will stop the recording and flush whatever video chunks we
  // currently have.
  did_failure_occur_ = true;
  MaybeStopAudioRecording();
  TerminateRecording(mojom::RecordingStatus::kVizVideoCapturerDisconnected);
}

void RecordingService::OnEncodingFailure(mojom::RecordingStatus status) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  did_failure_occur_ = true;
  StopRecording();
  // We don't wait for the video capturer to send us the OnStopped() signal, we
  // terminate recording immediately. We still need to flush the encoders, and
  // muxer since they may contain valid frames from before the failure occurred,
  // that we can propagate to the client.
  TerminateRecording(status);
}

void RecordingService::OnEncoderMuxerFlushed(mojom::RecordingStatus status) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  SignalRecordingEndedToClient(status);
}

void RecordingService::SignalRecordingEndedToClient(
    mojom::RecordingStatus status) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DCHECK(encoder_muxer_);

  encoder_muxer_.Reset();
  client_remote_->OnRecordingEnded(status, video_thumbnail_);
}

void RecordingService::OnRefreshTimerFired() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  if (video_capturer_remote_)
    video_capturer_remote_->RequestRefreshFrame();
}

void RecordingService::MaybeStopAudioRecording() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  if (audio_stream_mixer_) {
    audio_stream_mixer_.AsyncCall(&AudioStreamMixer::Stop);
    audio_stream_mixer_.Reset();
  }
}

}  // namespace recording
