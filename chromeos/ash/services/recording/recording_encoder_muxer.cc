// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/recording/recording_encoder_muxer.h"

#include "base/bind.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "chromeos/ash/services/recording/public/mojom/recording_service.mojom.h"
#include "chromeos/ash/services/recording/recording_service_constants.h"
#include "media/base/audio_codecs.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/muxers/file_webm_muxer_delegate.h"
#include "media/muxers/muxer.h"
#include "mojo/public/cpp/bindings/remote.h"

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

// We use a threshold of 512 MB to end the video recording due to low disk
// space, which is the same threshold as that used by the low disk space
// notification (See low_disk_notification.cc).
constexpr int64_t kLowDiskSpaceThresholdInBytes = 512 * 1024 * 1024;

// To avoid checking the remaining desk space after every write operation, we do
// it only once every 10 MB written of webm data.
constexpr int64_t kMinNumBytesBetweenDiskSpaceChecks = 10 * 1024 * 1024;

}  // namespace

// -----------------------------------------------------------------------------
// RecordingEncoderMuxer::RecordingMuxerDelegate:

// Defines a delegate for the WebmMuxer which extends the capability of
// |media::FileWebmMuxerDelegate| (which writes seekable webm chunks directly to
// a file), by adding recording specific behavior such as ending the recording
// when an IO file write fails, or when a critical disk space threshold is
// reached. An instance of this object is owned by the WebmMuxer, which in turn
// is owned by the RecordingEncoderMuxer instance.
class RecordingEncoderMuxer::RecordingMuxerDelegate
    : public media::FileWebmMuxerDelegate {
 public:
  RecordingMuxerDelegate(
      const base::FilePath& webm_file_path,
      RecordingEncoderMuxer* muxer_owner,
      mojo::PendingRemote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate)
      : FileWebmMuxerDelegate(base::File(
            webm_file_path,
            base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE)),
        muxer_owner_(muxer_owner),
        drive_fs_quota_delegate_remote_(std::move(drive_fs_quota_delegate)),
        webm_file_path_(webm_file_path) {
    DCHECK(muxer_owner_);
  }

  RecordingMuxerDelegate(const RecordingMuxerDelegate&) = delete;
  RecordingMuxerDelegate& operator=(const RecordingMuxerDelegate&) = delete;

  ~RecordingMuxerDelegate() override = default;

 protected:
  // media::FileWebmMuxerDelegate:
  mkvmuxer::int32 DoWrite(const void* buf, mkvmuxer::uint32 len) override {
    const auto result = FileWebmMuxerDelegate::DoWrite(buf, len);
    num_bytes_till_next_disk_space_check_ -= len;
    if (result != 0) {
      muxer_owner_->NotifyFailure(mojom::RecordingStatus::kIoError);
      return result;
    }

    MaybeCheckRemainingSpace();

    return result;
  }

 private:
  // Returns true if the video file is being written to a path `webm_file_path_`
  // that exists in DriveFS, false if it's a local file.
  bool IsDriveFsFile() const {
    return drive_fs_quota_delegate_remote_.is_bound();
  }

  // Checks the remaining free space (whether for a local file, or a DriveFS
  // file) once `num_bytes_till_next_disk_space_check_` goes below zero.
  void MaybeCheckRemainingSpace() {
    if (num_bytes_till_next_disk_space_check_ > 0)
      return;

    if (!IsDriveFsFile()) {
      OnGotRemainingFreeSpace(
          mojom::RecordingStatus::kLowDiskSpace,
          base::SysInfo::AmountOfFreeDiskSpace(webm_file_path_));
      return;
    }

    if (waiting_for_drive_fs_delegate_)
      return;

    DCHECK(drive_fs_quota_delegate_remote_);
    waiting_for_drive_fs_delegate_ = true;
    drive_fs_quota_delegate_remote_->GetDriveFsFreeSpaceBytes(
        base::BindOnce(&RecordingMuxerDelegate::OnGotRemainingFreeSpace,
                       weak_ptr_factory_.GetWeakPtr(),
                       mojom::RecordingStatus::kLowDriveFsQuota));
  }

  // Called to test the `remaining_free_space_bytes` against the minimum
  // threshold below which we end the recording with a failure. The failure type
  // that will be propagated to the client is the given `status`.
  void OnGotRemainingFreeSpace(mojom::RecordingStatus status,
                               int64_t remaining_free_space_bytes) {
    waiting_for_drive_fs_delegate_ = false;
    num_bytes_till_next_disk_space_check_ = kMinNumBytesBetweenDiskSpaceChecks;

    if (remaining_free_space_bytes < 0) {
      // A negative value (e.g. -1) indicates a failure in computing the free
      // space.
      return;
    }

    if (remaining_free_space_bytes < kLowDiskSpaceThresholdInBytes) {
      LOG(WARNING) << "Ending recording due to " << status
                   << ", and remaining free space of "
                   << base::FormatBytesUnlocalized(remaining_free_space_bytes);
      muxer_owner_->NotifyFailure(status);
    }
  }

  // A reference to the owner of the WebmMuxer instance that owns |this|. It is
  // used to notify with any IO or disk space errors while writing the webm
  // chunks.
  RecordingEncoderMuxer* const muxer_owner_;  // Not owned.

  // A remote end to the DriveFS delegate that can calculate the remaining free
  // space in Drive. This is bound only when the `webm_file_path_` points to a
  // file in DriveFS. Being unbound means the file is a local disk file.
  mojo::Remote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate_remote_;

  // The path of the webm file to which the muxer output will be written.
  const base::FilePath webm_file_path_;

  // Once this value becomes <= 0, we trigger a remaining disk space poll.
  // Initialized to 0, so that we poll the disk space on the very first write
  // operation.
  int64_t num_bytes_till_next_disk_space_check_ = 0;

  // True when we're waiting for a reply from the remote DriveFS quota delegate.
  bool waiting_for_drive_fs_delegate_ = false;

  base::WeakPtrFactory<RecordingMuxerDelegate> weak_ptr_factory_{this};
};

// -----------------------------------------------------------------------------
// RecordingEncoderMuxer::AudioFrame:

RecordingEncoderMuxer::AudioFrame::AudioFrame(
    std::unique_ptr<media::AudioBus> audio_bus,
    base::TimeTicks time)
    : bus(std::move(audio_bus)), capture_time(time) {}
RecordingEncoderMuxer::AudioFrame::AudioFrame(AudioFrame&&) = default;
RecordingEncoderMuxer::AudioFrame::~AudioFrame() = default;

// -----------------------------------------------------------------------------
// RecordingEncoderMuxer:

// static
base::SequenceBound<RecordingEncoderMuxer> RecordingEncoderMuxer::Create(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const media::VideoEncoder::Options& video_encoder_options,
    const media::AudioParameters* audio_input_params,
    mojo::PendingRemote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate,
    const base::FilePath& webm_file_path,
    OnFailureCallback on_failure_callback) {
  return base::SequenceBound<RecordingEncoderMuxer>(
      std::move(blocking_task_runner), PassKey(), video_encoder_options,
      audio_input_params, std::move(drive_fs_quota_delegate), webm_file_path,
      std::move(on_failure_callback));
}

RecordingEncoderMuxer::RecordingEncoderMuxer(
    PassKey,
    const media::VideoEncoder::Options& video_encoder_options,
    const media::AudioParameters* audio_input_params,
    mojo::PendingRemote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate,
    const base::FilePath& webm_file_path,
    OnFailureCallback on_failure_callback)
    : on_failure_callback_(std::move(on_failure_callback)),
      webm_muxer_(media::AudioCodec::kOpus,
                  /*has_video_=*/true,
                  /*has_audio_=*/!!audio_input_params,
                  std::make_unique<RecordingMuxerDelegate>(
                      webm_file_path,
                      this,
                      std::move(drive_fs_quota_delegate))) {
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

RecordingEncoderMuxer::~RecordingEncoderMuxer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void RecordingEncoderMuxer::InitializeVideoEncoder(
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
      base::BindRepeating(&RecordingEncoderMuxer::OnVideoEncoderOutput,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&RecordingEncoderMuxer::OnVideoEncoderInitialized,
                     weak_ptr_factory_.GetWeakPtr(), video_encoder_.get()));
}

void RecordingEncoderMuxer::EncodeVideo(
    scoped_refptr<media::VideoFrame> frame) {
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

void RecordingEncoderMuxer::EncodeAudio(
    std::unique_ptr<media::AudioBus> audio_bus,
    base::TimeTicks capture_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(audio_encoder_);

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

void RecordingEncoderMuxer::FlushAndFinalize(base::OnceClosure on_done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (audio_encoder_) {
    audio_encoder_->Flush(
        base::BindOnce(&RecordingEncoderMuxer::OnAudioEncoderFlushed,
                       weak_ptr_factory_.GetWeakPtr(), std::move(on_done)));
  } else {
    OnAudioEncoderFlushed(std::move(on_done), media::EncoderStatus::Codes::kOk);
  }
}

void RecordingEncoderMuxer::InitializeAudioEncoder(
    const media::AudioEncoder::Options& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  is_audio_encoder_initialized_ = false;
  audio_encoder_ = std::make_unique<media::AudioOpusEncoder>();
  audio_encoder_->Initialize(
      options,
      base::BindRepeating(&RecordingEncoderMuxer::OnAudioEncoded,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&RecordingEncoderMuxer::OnAudioEncoderInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RecordingEncoderMuxer::OnAudioEncoderInitialized(
    media::EncoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!status.is_ok()) {
    LOG(ERROR) << "Could not initialize the audio encoder: "
               << status.message();
    NotifyFailure(mojom::RecordingStatus::kAudioEncoderInitializationFailure);
    return;
  }

  is_audio_encoder_initialized_ = true;
  for (auto& frame : pending_audio_frames_)
    EncodeAudioImpl(std::move(frame));
  pending_audio_frames_.clear();
}

void RecordingEncoderMuxer::OnVideoEncoderInitialized(
    media::VpxVideoEncoder* encoder,
    media::EncoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ignore initialization of encoders that were removed as part of
  // reinitialization.
  if (video_encoder_.get() != encoder)
    return;

  if (!status.is_ok()) {
    LOG(ERROR) << "Could not initialize the video encoder: "
               << status.message();
    NotifyFailure(mojom::RecordingStatus::kVideoEncoderInitializationFailure);
    return;
  }

  is_video_encoder_initialized_ = true;
  for (auto& frame : pending_video_frames_)
    EncodeVideoImpl(frame);
  pending_video_frames_.clear();
}

void RecordingEncoderMuxer::EncodeAudioImpl(AudioFrame frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_audio_encoder_initialized_);

  if (did_failure_occur())
    return;

  audio_encoder_->Encode(
      std::move(frame.bus), frame.capture_time,
      base::BindOnce(&RecordingEncoderMuxer::OnEncoderStatus,
                     weak_ptr_factory_.GetWeakPtr(), /*for_video=*/false));
}

void RecordingEncoderMuxer::EncodeVideoImpl(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_video_encoder_initialized_);

  if (did_failure_occur())
    return;

  video_visible_rect_sizes_.push(frame->visible_rect().size());
  video_encoder_->Encode(
      frame, /*key_frame=*/false,
      base::BindOnce(&RecordingEncoderMuxer::OnEncoderStatus,
                     weak_ptr_factory_.GetWeakPtr(), /*for_video=*/true));
}

void RecordingEncoderMuxer::OnVideoEncoderOutput(
    media::VideoEncoderOutput output,
    absl::optional<media::VideoEncoder::CodecDescription> codec_description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  media::Muxer::VideoParameters params(video_visible_rect_sizes_.front(),
                                       kMaxFrameRate, media::VideoCodec::kVP8,
                                       kColorSpace);
  video_visible_rect_sizes_.pop();

  // TODO(crbug.com/1143798): Explore changing the WebmMuxer so it doesn't work
  // with strings, to avoid copying the encoded data.
  std::string data{reinterpret_cast<const char*>(output.data.get()),
                   output.size};
  webm_muxer_.OnEncodedVideo(params, std::move(data), std::string(),
                             base::TimeTicks() + output.timestamp,
                             output.key_frame);
}

void RecordingEncoderMuxer::OnAudioEncoded(
    media::EncodedAudioBuffer encoded_audio,
    absl::optional<media::AudioEncoder::CodecDescription> codec_description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(audio_encoder_);

  // TODO(crbug.com/1143798): Explore changing the WebmMuxer so it doesn't work
  // with strings, to avoid copying the encoded data.
  std::string encoded_data{
      reinterpret_cast<const char*>(encoded_audio.encoded_data.get()),
      encoded_audio.encoded_data_size};
  webm_muxer_.OnEncodedAudio(encoded_audio.params, std::move(encoded_data),
                             encoded_audio.timestamp);
}

void RecordingEncoderMuxer::OnAudioEncoderFlushed(base::OnceClosure on_done,
                                                  media::EncoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!status.is_ok())
    LOG(ERROR) << "Could not flush audio encoder: " << status.message();

  DCHECK(video_encoder_);
  video_encoder_->Flush(
      base::BindOnce(&RecordingEncoderMuxer::OnVideoEncoderFlushed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_done)));
}

void RecordingEncoderMuxer::OnVideoEncoderFlushed(base::OnceClosure on_done,
                                                  media::EncoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!status.is_ok()) {
    LOG(ERROR) << "Could not flush remaining video frames: "
               << status.message();
  }

  webm_muxer_.Flush();
  std::move(on_done).Run();
}

void RecordingEncoderMuxer::OnEncoderStatus(bool for_video,
                                            media::EncoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status.is_ok())
    return;

  LOG(ERROR) << "Failed to encode " << (for_video ? "video" : "audio")
             << " frame: " << status.message();
  NotifyFailure(for_video ? mojom::RecordingStatus::kVideoEncodingError
                          : mojom::RecordingStatus::kAudioEncodingError);
}

void RecordingEncoderMuxer::NotifyFailure(mojom::RecordingStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (on_failure_callback_)
    std::move(on_failure_callback_).Run(status);
}

}  // namespace recording
