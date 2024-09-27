// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/recording/webm_encoder_muxer.h"

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/recording/public/mojom/recording_service.mojom.h"
#include "chromeos/ash/services/recording/recording_file_io_helper.h"
#include "chromeos/ash/services/recording/recording_service_constants.h"
#include "media/base/audio_codecs.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/muxers/file_webm_muxer_delegate.h"
#include "media/muxers/muxer.h"
#include "media/muxers/webm_muxer.h"

namespace recording {

namespace {

// The audio and video encoders are initialized asynchronously, and until that
// happens, all received audio and video frames are added to
// |pending_video_frames_| and |pending_audio_frames_|. However, in order
// to avoid an OOM situation if the encoder takes too long to initialize or it
// never does, we impose an upper-bound to the number of pending frames. The
// below value is equal to the maximum number of in-flight frames that the
// capturer uses (See |viz::FrameSinkVideoCapturerImpl::kDesignLimitMaxFrames|)
// before it stops sending frames. Once we hit that limit in
// |pending_video_frames_|, we will start dropping frames to let the capturer
// proceed, with an upper limit of how many frames we can drop that is
// equivalent to 4 seconds, after which we'll declare an encoder initialization
// failure. For convenience the same limit is used for as a cap on number of
// audio frames stored in |pending_audio_frames_|.
constexpr size_t kMaxPendingFrames = 10;
constexpr size_t kMaxDroppedFrames = 4 * kMaxFrameRate;

// -----------------------------------------------------------------------------
// WebmEncoderCapabilities:

// Implements the capabilities for WebM encoding.
class WebmEncoderCapabilities : public RecordingEncoder::Capabilities {
 public:
  WebmEncoderCapabilities() = default;
  WebmEncoderCapabilities(const WebmEncoderCapabilities&) = delete;
  WebmEncoderCapabilities& operator=(const WebmEncoderCapabilities&) = delete;
  ~WebmEncoderCapabilities() override = default;

  // RecordingEncoder::Capabilities:
  media::VideoPixelFormat GetSupportedPixelFormat() const override {
    return media::PIXEL_FORMAT_I420;
  }

  bool SupportsVideoFrameSizeChanges() const override { return true; }

  bool SupportsRgbVideoFrame() const override { return false; }
};

// -----------------------------------------------------------------------------
// RecordingMuxerDelegate:

// Defines a delegate for the WebmMuxer which extends the capability of
// `media::FileWebmMuxerDelegate` (which writes seekable webm chunks directly to
// a file), by adding recording specific behavior such as ending the recording
// when an IO file write fails, or when a critical disk space threshold is
// reached. An instance of this object is owned by the WebmMuxer, which in turn
// is owned by the WebmEncoderMuxer instance.
class RecordingMuxerDelegate : public media::FileWebmMuxerDelegate {
 public:
  RecordingMuxerDelegate(
      const base::FilePath& webm_file_path,
      mojo::PendingRemote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate,
      WebmEncoderMuxer* owner)
      : FileWebmMuxerDelegate(base::File(
            webm_file_path,
            base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE)),
        file_io_helper_(webm_file_path,
                        std::move(drive_fs_quota_delegate),
                        owner) {}

  RecordingMuxerDelegate(const RecordingMuxerDelegate&) = delete;
  RecordingMuxerDelegate& operator=(const RecordingMuxerDelegate&) = delete;

  ~RecordingMuxerDelegate() override = default;

 protected:
  // media::FileWebmMuxerDelegate:
  mkvmuxer::int32 DoWrite(const void* buf, mkvmuxer::uint32 len) override {
    const auto result = FileWebmMuxerDelegate::DoWrite(buf, len);
    if (result != 0) {
      file_io_helper_.delegate()->NotifyFailure(
          mojom::RecordingStatus::kIoError);
      return result;
    }

    file_io_helper_.OnBytesWritten(len);

    return result;
  }

 private:
  RecordingFileIoHelper file_io_helper_;
};

}  // namespace

// -----------------------------------------------------------------------------
// WebmEncoderMuxer::AudioFrame:

WebmEncoderMuxer::AudioFrame::AudioFrame(
    std::unique_ptr<media::AudioBus> audio_bus,
    base::TimeTicks time)
    : bus(std::move(audio_bus)), capture_time(time) {}
WebmEncoderMuxer::AudioFrame::AudioFrame(AudioFrame&&) = default;
WebmEncoderMuxer::AudioFrame::~AudioFrame() = default;

// -----------------------------------------------------------------------------
// WebmEncoderMuxer:

// static
base::SequenceBound<RecordingEncoder> WebmEncoderMuxer::Create(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const media::VideoEncoder::Options& video_encoder_options,
    const media::AudioParameters* audio_input_params,
    mojo::PendingRemote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate,
    const base::FilePath& webm_file_path,
    OnFailureCallback on_failure_callback) {
  return base::SequenceBound<WebmEncoderMuxer>(
      std::move(blocking_task_runner), PassKey(), video_encoder_options,
      audio_input_params, std::move(drive_fs_quota_delegate), webm_file_path,
      std::move(on_failure_callback));
}

// static
std::unique_ptr<RecordingEncoder::Capabilities>
WebmEncoderMuxer::CreateCapabilities() {
  return std::make_unique<WebmEncoderCapabilities>();
}

WebmEncoderMuxer::WebmEncoderMuxer(
    PassKey,
    const media::VideoEncoder::Options& video_encoder_options,
    const media::AudioParameters* audio_input_params,
    mojo::PendingRemote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate,
    const base::FilePath& webm_file_path,
    OnFailureCallback on_failure_callback)
    : RecordingEncoder(std::move(on_failure_callback)),
      muxer_adapter_(std::make_unique<media::WebmMuxer>(
                         media::AudioCodec::kOpus,
                         /*has_video_=*/true,
                         /*has_audio_=*/!!audio_input_params,
                         std::make_unique<RecordingMuxerDelegate>(
                             webm_file_path,
                             std::move(drive_fs_quota_delegate),
                             this),
                         /*max_data_output_interval=*/std::nullopt),
                     /*has_video=*/true,
                     /*has_audio=*/!!audio_input_params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (audio_input_params) {
    media::AudioEncoder::Options audio_encoder_options;
    audio_encoder_options.codec = media::AudioCodec::kOpus;
    audio_encoder_options.channels = audio_input_params->channels();
    audio_encoder_options.sample_rate = audio_input_params->sample_rate();
    InitializeAudioEncoder(audio_encoder_options);
  }

  InitializeVideoEncoder(video_encoder_options);
}

WebmEncoderMuxer::~WebmEncoderMuxer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WebmEncoderMuxer::InitializeVideoEncoder(
    const media::VideoEncoder::Options& video_encoder_options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Note: The VpxVideoEncoder supports changing the encoding options
  // dynamically, but it won't work for all frame size changes and may cause
  // encoding failures. Therefore, it's better to recreate and reinitialize a
  // new encoder. See media::VpxVideoEncoder::ChangeOptions() for more details.

  if (video_encoder_ && is_video_encoder_initialized_) {
    auto* encoder_ptr = video_encoder_.get();
    encoder_ptr->Flush(base::BindOnce(
        // Holds on to the old encoder until it flushes its buffers, then
        // destroys it.
        [](std::unique_ptr<media::VpxVideoEncoder> old_encoder,
           media::EncoderStatus status) {},
        std::move(video_encoder_)));
  }

  is_video_encoder_initialized_ = false;
  video_encoder_ = std::make_unique<media::VpxVideoEncoder>();
  video_encoder_->Initialize(
      media::VP8PROFILE_ANY, video_encoder_options,
      /*info_cb=*/base::DoNothing(),
      base::BindRepeating(&WebmEncoderMuxer::OnVideoEncoderOutput,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&WebmEncoderMuxer::OnVideoEncoderInitialized,
                     weak_ptr_factory_.GetWeakPtr(),
                     // TODO(crbug.com/40061562): Remove
                     // `UnsafeDanglingUntriaged`
                     base::UnsafeDanglingUntriaged(video_encoder_.get())));
}

void WebmEncoderMuxer::EncodeVideo(scoped_refptr<media::VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_video_encoder_initialized_) {
    EncodeVideoImpl(std::move(frame));
    return;
  }

  pending_video_frames_.push_back(std::move(frame));
  if (pending_video_frames_.size() == kMaxPendingFrames) {
    pending_video_frames_.pop_front();
    DCHECK_LT(pending_video_frames_.size(), kMaxPendingFrames);

    if (++num_dropped_frames_ >= kMaxDroppedFrames) {
      LOG(ERROR) << "Video encoder took too long to initialize.";
      NotifyFailure(mojom::RecordingStatus::kVideoEncoderInitializationFailure);
    }
  }
}

void WebmEncoderMuxer::EncodeRgbVideo(RgbVideoFrame rgb_video_frame) {
  NOTREACHED_IN_MIGRATION();
}

EncodeAudioCallback WebmEncoderMuxer::GetEncodeAudioCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(audio_encoder_);

  return base::BindRepeating(&WebmEncoderMuxer::EncodeAudio,
                             weak_ptr_factory_.GetWeakPtr());
}

void WebmEncoderMuxer::FlushAndFinalize(base::OnceClosure on_done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (audio_encoder_) {
    audio_encoder_->Flush(
        base::BindOnce(&WebmEncoderMuxer::OnAudioEncoderFlushed,
                       weak_ptr_factory_.GetWeakPtr(), std::move(on_done)));
  } else {
    OnAudioEncoderFlushed(std::move(on_done), media::EncoderStatus::Codes::kOk);
  }
}

void WebmEncoderMuxer::InitializeAudioEncoder(
    const media::AudioEncoder::Options& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  is_audio_encoder_initialized_ = false;
  audio_encoder_ = std::make_unique<media::AudioOpusEncoder>();
  audio_encoder_->Initialize(
      options,
      base::BindRepeating(&WebmEncoderMuxer::OnAudioEncoded,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&WebmEncoderMuxer::OnAudioEncoderInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebmEncoderMuxer::OnAudioEncoderInitialized(media::EncoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!status.is_ok()) {
    LOG(ERROR) << "Could not initialize the audio encoder: "
               << status.message();
    NotifyFailure(mojom::RecordingStatus::kAudioEncoderInitializationFailure);
    return;
  }

  is_audio_encoder_initialized_ = true;
  for (auto& frame : pending_audio_frames_) {
    EncodeAudioImpl(std::move(frame));
  }
  pending_audio_frames_.clear();
}

void WebmEncoderMuxer::OnVideoEncoderInitialized(
    media::VpxVideoEncoder* encoder,
    media::EncoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ignore initialization of encoders that were removed as part of
  // reinitialization.
  if (video_encoder_.get() != encoder) {
    return;
  }

  if (!status.is_ok()) {
    LOG(ERROR) << "Could not initialize the video encoder: "
               << status.message();
    NotifyFailure(mojom::RecordingStatus::kVideoEncoderInitializationFailure);
    return;
  }

  is_video_encoder_initialized_ = true;
  for (auto& frame : pending_video_frames_) {
    EncodeVideoImpl(std::move(frame));
  }
  pending_video_frames_.clear();
}

void WebmEncoderMuxer::EncodeAudio(std::unique_ptr<media::AudioBus> audio_bus,
                                   base::TimeTicks capture_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(audio_encoder_);

  // We ignore any subsequent frames after a failure.
  if (did_failure_occur()) {
    return;
  }

  AudioFrame frame(std::move(audio_bus), capture_time);
  if (is_audio_encoder_initialized_) {
    EncodeAudioImpl(std::move(frame));
    return;
  }

  pending_audio_frames_.push_back(std::move(frame));
  if (pending_audio_frames_.size() == kMaxPendingFrames) {
    pending_audio_frames_.pop_front();
    DCHECK_LT(pending_audio_frames_.size(), kMaxPendingFrames);
  }
}

void WebmEncoderMuxer::EncodeAudioImpl(AudioFrame frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_audio_encoder_initialized_);

  if (did_failure_occur()) {
    return;
  }

  audio_encoder_->Encode(
      std::move(frame.bus), frame.capture_time,
      base::BindOnce(&WebmEncoderMuxer::OnEncoderStatus,
                     weak_ptr_factory_.GetWeakPtr(), /*for_video=*/false));
}

void WebmEncoderMuxer::EncodeVideoImpl(scoped_refptr<media::VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_video_encoder_initialized_);

  if (did_failure_occur()) {
    return;
  }

  DCHECK(frame->metadata().reference_time);
  encoded_video_params_.push(EncodedVideoFrameParams{
      *frame->metadata().reference_time, frame->visible_rect().size()});
  video_encoder_->Encode(
      std::move(frame), media::VideoEncoder::EncodeOptions(/*key_frame=*/false),
      base::BindOnce(&WebmEncoderMuxer::OnEncoderStatus,
                     weak_ptr_factory_.GetWeakPtr(), /*for_video=*/true));
}

void WebmEncoderMuxer::OnVideoEncoderOutput(
    media::VideoEncoderOutput output,
    std::optional<media::VideoEncoder::CodecDescription> codec_description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!encoded_video_params_.empty());
  const auto& encoded_video_params = encoded_video_params_.front();
  const media::Muxer::VideoParameters muxer_params(
      encoded_video_params.visible_rect_size, kMaxFrameRate,
      media::VideoCodec::kVP8, kColorSpace);
  const base::TimeTicks timestamp = encoded_video_params.frame_reference_time;
  encoded_video_params_.pop();

  auto buffer = media::DecoderBuffer::FromArray(std::move(output.data));
  buffer->set_is_key_frame(output.key_frame);
  muxer_adapter_.OnEncodedVideo(muxer_params, std::move(buffer),
                                std::move(codec_description), timestamp);
}

void WebmEncoderMuxer::OnAudioEncoded(
    media::EncodedAudioBuffer encoded_audio,
    std::optional<media::AudioEncoder::CodecDescription> codec_description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(audio_encoder_);

  auto buffer =
      media::DecoderBuffer::FromArray(std::move(encoded_audio.encoded_data));
  muxer_adapter_.OnEncodedAudio(encoded_audio.params, std::move(buffer),
                                std::move(codec_description),
                                encoded_audio.timestamp);
}

void WebmEncoderMuxer::OnAudioEncoderFlushed(base::OnceClosure on_done,
                                             media::EncoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!status.is_ok()) {
    LOG(ERROR) << "Could not flush audio encoder: " << status.message();
  }

  DCHECK(video_encoder_);
  video_encoder_->Flush(base::BindOnce(&WebmEncoderMuxer::OnVideoEncoderFlushed,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       std::move(on_done)));
}

void WebmEncoderMuxer::OnVideoEncoderFlushed(base::OnceClosure on_done,
                                             media::EncoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!status.is_ok()) {
    LOG(ERROR) << "Could not flush remaining video frames: "
               << status.message();
  }

  muxer_adapter_.Flush();
  std::move(on_done).Run();
}

}  // namespace recording
