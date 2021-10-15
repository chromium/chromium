// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/cast_streaming_demuxer.h"

#include "base/bind.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "components/cast_streaming/renderer/cast_streaming_receiver.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"

namespace cast_streaming {
namespace {

// media::DemuxerStream shared audio/video implementation for Cast Streaming.
// Receives buffer metadata over a Mojo service and reads the buffers over a
// Mojo data pipe from the browser process.
class CastStreamingDemuxerStream : public media::DemuxerStream,
                                   public mojom::CastStreamingBufferReceiver {
 public:
  CastStreamingDemuxerStream(
      mojo::PendingReceiver<mojom::CastStreamingBufferReceiver>
          pending_receiver,
      mojo::ScopedDataPipeConsumerHandle consumer)
      : receiver_(this, std::move(pending_receiver)),
        decoder_buffer_reader_(std::make_unique<media::MojoDecoderBufferReader>(
            std::move(consumer))) {
    DVLOG(1) << __func__;

    // Mojo service disconnection means the Cast Streaming Session ended and no
    // further buffer will be received. kAborted will be returned to the media
    // pipeline for every subsequent DemuxerStream::Read() attempt.
    receiver_.set_disconnect_handler(base::BindOnce(
        &CastStreamingDemuxerStream::OnMojoDisconnect, base::Unretained(this)));
  }
  ~CastStreamingDemuxerStream() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // mojom::CastStreamingBufferReceiver implementation.
  void ProvideBuffer(media::mojom::DecoderBufferPtr buffer) final {
    DVLOG(3) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    pending_buffer_metadata_.push_back(std::move(buffer));
    GetNextBuffer();
  }

  void AbortPendingRead() {
    DVLOG(3) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (pending_read_cb_)
      std::move(pending_read_cb_).Run(Status::kAborted, nullptr);
  }

 protected:
  void ChangeDataPipe(mojo::ScopedDataPipeConsumerHandle data_pipe) {
    DVLOG(1) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    pending_config_change_ = true;

    // Reset the buffer reader and the current buffer. Data from the old pipe is
    // no longer valid.
    current_buffer_.reset();
    decoder_buffer_reader_ =
        std::make_unique<media::MojoDecoderBufferReader>(std::move(data_pipe));
    CompletePendingConfigChange();
  }

  void CompletePendingConfigChange() {
    DVLOG(1) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(pending_config_change_);

    if (!pending_read_cb_)
      return;

    std::move(pending_read_cb_).Run(Status::kConfigChanged, nullptr);
    pending_config_change_ = false;
  }

  // True when this stream is undergoing a decoder configuration change.
  bool pending_config_change_ = false;

 private:
  void CompletePendingRead() {
    DVLOG(3) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (pending_config_change_ || !pending_read_cb_ || !current_buffer_)
      return;

    if (current_buffer_->end_of_stream()) {
      std::move(pending_read_cb_).Run(Status::kError, nullptr);
      return;
    }

    std::move(pending_read_cb_).Run(Status::kOk, std::move(current_buffer_));
    GetNextBuffer();
  }

  void GetNextBuffer() {
    DVLOG(3) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (current_buffer_ || pending_buffer_metadata_.empty())
      return;

    media::mojom::DecoderBufferPtr buffer =
        std::move(pending_buffer_metadata_.front());
    pending_buffer_metadata_.pop_front();
    decoder_buffer_reader_->ReadDecoderBuffer(
        std::move(buffer),
        base::BindOnce(&CastStreamingDemuxerStream::OnBufferRead,
                       base::Unretained(this)));
  }

  void OnBufferRead(scoped_refptr<media::DecoderBuffer> buffer) {
    DVLOG(3) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // The pending buffer reads are cancelled when we reset the data pipe on a
    // configuration change. Just ignore them and return early here.
    if (!buffer)
      return;

    // Stop processing the pending buffer. OnMojoDisconnect() will trigger
    // sending kAborted on subsequent Read() calls. This can happen if this
    // object was in the process of reading a buffer off the data pipe when the
    // Mojo connection ended.
    if (!receiver_.is_bound())
      return;

    DCHECK(!current_buffer_);
    current_buffer_ = buffer;
    CompletePendingRead();
  }

  void OnMojoDisconnect() {
    DVLOG(1) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    receiver_.reset();
    pending_buffer_metadata_.clear();
    current_buffer_ = media::DecoderBuffer::CreateEOSBuffer();
    CompletePendingRead();
  }

  // DemuxerStream implementation.
  void Read(ReadCB read_cb) final {
    DVLOG(3) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(pending_read_cb_.is_null());

    pending_read_cb_ = std::move(read_cb);
    if (pending_config_change_)
      CompletePendingConfigChange();
    else
      CompletePendingRead();
  }
  Liveness liveness() const final { return Liveness::LIVENESS_LIVE; }
  bool SupportsConfigChanges() final { return true; }

  mojo::Receiver<CastStreamingBufferReceiver> receiver_;
  std::unique_ptr<media::MojoDecoderBufferReader> decoder_buffer_reader_;

  ReadCB pending_read_cb_;
  base::circular_deque<media::mojom::DecoderBufferPtr> pending_buffer_metadata_;
  scoped_refptr<media::DecoderBuffer> current_buffer_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

class CastStreamingAudioDemuxerStream final
    : public CastStreamingDemuxerStream {
 public:
  explicit CastStreamingAudioDemuxerStream(
      mojom::AudioStreamInfoPtr audio_stream_info)
      : CastStreamingDemuxerStream(
            std::move(audio_stream_info->buffer_receiver),
            std::move(audio_stream_info->data_pipe)),
        config_(audio_stream_info->decoder_config) {
    DVLOG(1) << __func__
             << ": config info: " << config_.AsHumanReadableString();
  }
  ~CastStreamingAudioDemuxerStream() override = default;

 private:
  // CastStreamingBufferReceiver implementation.
  void OnNewAudioConfig(const media::AudioDecoderConfig& decoder_config,
                        mojo::ScopedDataPipeConsumerHandle data_pipe) final {
    config_ = decoder_config;
    DVLOG(1) << __func__
             << ": config info: " << config_.AsHumanReadableString();
    ChangeDataPipe(std::move(data_pipe));
  }

  void OnNewVideoConfig(const media::VideoDecoderConfig& decoder_config,
                        mojo::ScopedDataPipeConsumerHandle data_pipe) final {
    NOTREACHED();
  }

  // DemuxerStream implementation.
  media::AudioDecoderConfig audio_decoder_config() final { return config_; }
  media::VideoDecoderConfig video_decoder_config() final {
    NOTREACHED();
    return media::VideoDecoderConfig();
  }
  Type type() const final { return Type::AUDIO; }

  media::AudioDecoderConfig config_;
};

class CastStreamingVideoDemuxerStream final
    : public CastStreamingDemuxerStream {
 public:
  explicit CastStreamingVideoDemuxerStream(
      mojom::VideoStreamInfoPtr video_stream_info)
      : CastStreamingDemuxerStream(
            std::move(video_stream_info->buffer_receiver),
            std::move(video_stream_info->data_pipe)),
        config_(video_stream_info->decoder_config) {
    DVLOG(1) << __func__
             << ": config info: " << config_.AsHumanReadableString();
  }
  ~CastStreamingVideoDemuxerStream() override = default;

 private:
  // CastStreamingBufferReceiver implementation.
  void OnNewAudioConfig(const media::AudioDecoderConfig& decoder_config,
                        mojo::ScopedDataPipeConsumerHandle data_pipe) final {
    NOTREACHED();
  }

  void OnNewVideoConfig(const media::VideoDecoderConfig& decoder_config,
                        mojo::ScopedDataPipeConsumerHandle data_pipe) final {
    config_ = decoder_config;
    DVLOG(1) << __func__
             << ": config info: " << config_.AsHumanReadableString();
    ChangeDataPipe(std::move(data_pipe));
  }

  // DemuxerStream implementation.
  media::AudioDecoderConfig audio_decoder_config() final {
    NOTREACHED();
    return media::AudioDecoderConfig();
  }
  media::VideoDecoderConfig video_decoder_config() final { return config_; }
  Type type() const final { return Type::VIDEO; }

  media::VideoDecoderConfig config_;
};

CastStreamingDemuxer::CastStreamingDemuxer(
    CastStreamingReceiver* receiver,
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner)
    : media_task_runner_(media_task_runner),
      original_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      receiver_(receiver) {
  DVLOG(1) << __func__;
  DCHECK(receiver_);
}

CastStreamingDemuxer::~CastStreamingDemuxer() {
  DVLOG(1) << __func__;

  if (was_initialization_successful_) {
    original_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CastStreamingReceiver::OnDemuxerDestroyed,
                                  base::Unretained(receiver_)));
  }
}

void CastStreamingDemuxer::OnStreamsInitialized(
    mojom::AudioStreamInfoPtr audio_stream_info,
    mojom::VideoStreamInfoPtr video_stream_info) {
  DVLOG(1) << __func__;
  DCHECK(!media_task_runner_->BelongsToCurrentThread());

  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CastStreamingDemuxer::OnStreamsInitializedOnMediaThread,
                     base::Unretained(this), std::move(audio_stream_info),
                     std::move(video_stream_info)));
}

void CastStreamingDemuxer::OnStreamsInitializedOnMediaThread(
    mojom::AudioStreamInfoPtr audio_stream_info,
    mojom::VideoStreamInfoPtr video_stream_info) {
  DVLOG(1) << __func__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(initialized_cb_);

  if (!audio_stream_info && !video_stream_info) {
    std::move(initialized_cb_)
        .Run(media::PipelineStatus::DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  if (audio_stream_info) {
    audio_stream_ = std::make_unique<CastStreamingAudioDemuxerStream>(
        std::move(audio_stream_info));
  }
  if (video_stream_info) {
    video_stream_ = std::make_unique<CastStreamingVideoDemuxerStream>(
        std::move(video_stream_info));
  }
  was_initialization_successful_ = true;

  std::move(initialized_cb_).Run(media::PipelineStatus::PIPELINE_OK);
}

std::vector<media::DemuxerStream*> CastStreamingDemuxer::GetAllStreams() {
  DVLOG(1) << __func__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  std::vector<media::DemuxerStream*> streams;
  if (video_stream_)
    streams.push_back(video_stream_.get());
  if (audio_stream_)
    streams.push_back(audio_stream_.get());
  return streams;
}

std::string CastStreamingDemuxer::GetDisplayName() const {
  return "CastStreamingDemuxer";
}

void CastStreamingDemuxer::Initialize(media::DemuxerHost* host,
                                      media::PipelineStatusCallback status_cb) {
  DVLOG(1) << __func__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  host_ = host;

  // Live streams have infinite duration.
  host_->SetDuration(media::kInfiniteDuration);
  initialized_cb_ = std::move(status_cb);

  original_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CastStreamingReceiver::SetDemuxer,
                     base::Unretained(receiver_), base::Unretained(this)));
}

void CastStreamingDemuxer::AbortPendingReads() {
  DVLOG(2) << __func__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (audio_stream_)
    audio_stream_->AbortPendingRead();
  if (video_stream_)
    video_stream_->AbortPendingRead();
}

// Not supported.
void CastStreamingDemuxer::StartWaitingForSeek(base::TimeDelta seek_time) {}

// Not supported.
void CastStreamingDemuxer::CancelPendingSeek(base::TimeDelta seek_time) {}

// Not supported.
void CastStreamingDemuxer::Seek(base::TimeDelta time,
                                media::PipelineStatusCallback status_cb) {
  std::move(status_cb).Run(media::PipelineStatus::PIPELINE_OK);
}

void CastStreamingDemuxer::Stop() {
  DVLOG(1) << __func__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  if (audio_stream_)
    audio_stream_.reset();
  if (video_stream_)
    video_stream_.reset();
}

base::TimeDelta CastStreamingDemuxer::GetStartTime() const {
  return base::TimeDelta();
}

// Not supported.
base::Time CastStreamingDemuxer::GetTimelineOffset() const {
  return base::Time();
}

// Not supported.
int64_t CastStreamingDemuxer::GetMemoryUsage() const {
  return 0;
}

absl::optional<media::container_names::MediaContainerName>
CastStreamingDemuxer::GetContainerForMetrics() const {
  // Cast Streaming frames have no container.
  return absl::nullopt;
}

// Not supported.
void CastStreamingDemuxer::OnEnabledAudioTracksChanged(
    const std::vector<media::MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  DLOG(WARNING) << "Track changes are not supported.";
  std::vector<media::DemuxerStream*> streams;
  std::move(change_completed_cb).Run(media::DemuxerStream::AUDIO, streams);
}

// Not supported.
void CastStreamingDemuxer::OnSelectedVideoTrackChanged(
    const std::vector<media::MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  DLOG(WARNING) << "Track changes are not supported.";
  std::vector<media::DemuxerStream*> streams;
  std::move(change_completed_cb).Run(media::DemuxerStream::VIDEO, streams);
}

}  // namespace cast_streaming
