// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/cast_streaming_demuxer.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "components/cast_streaming/renderer/cast_streaming_receiver.h"
#include "components/cast_streaming/renderer/decoder_buffer_reader.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace cast_streaming {
namespace {

// media::DemuxerStream implementation used for audio and video streaming.
// Receives buffer metadata over a Mojo service through a "pull" mechanism using
// the associated mojo interface's GetBuffer() method and reads the buffer's
// contents over a Mojo data pipe from the browser process.
//
// |TMojoRemoteType| is the interface used for requesting data buffers.
// Currently expected to be either AudioBufferRequester or VideoBufferRequester.
// |TStreamInfoType| is the StreamInfo that may be returned by this call, either
// AudioStreamInfo or VideoStreamInfo.
template <typename TMojoRemoteType, typename TStreamInfoType>
class CastStreamingDemuxerStream : public media::DemuxerStream {
 public:
  CastStreamingDemuxerStream(
      mojo::PendingRemote<TMojoRemoteType> pending_remote,
      TStreamInfoType stream_initialization_info)
      : remote_(std::move(pending_remote)), weak_factory_(this) {
    // Mojo service disconnection means the Cast Streaming Session ended and no
    // further buffer will be requested. kAborted will be returned to the media
    // pipeline for every subsequent DemuxerStream::Read() attempt.
    remote_.set_disconnect_handler(
        base::BindOnce(&CastStreamingDemuxerStream::OnMojoDisconnect,
                       weak_factory_.GetWeakPtr()));

    // Set the new config, but then un-set |pending_config_change_| as the
    // initial config is already applied prior to the first Read() call.
    OnNewConfig(std::move(stream_initialization_info));
    DCHECK(pending_config_change_);
    pending_config_change_ = false;

    // Request the first buffer from the browser process, to be returned
    // asynchronously as the response to the first Read() call.
    RequestNextBuffer();
  }

  ~CastStreamingDemuxerStream() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void AbortPendingRead() {
    DVLOG(3) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (pending_read_cb_)
      std::move(pending_read_cb_).Run(Status::kAborted, nullptr);
  }

 protected:
  // Deduce the Config type associated with this Mojo API (either
  // media::AudioDecoderConfig or media::VideoDecoderConfig).
  typedef decltype(TStreamInfoType::element_type::decoder_config) ConfigType;

  const ConfigType& config() {
    DCHECK(decoder_config_);
    return decoder_config_.value();
  }

 private:
  void OnMojoDisconnect() {
    DLOG(ERROR) << __func__ << ": Mojo Pipe Disconnected";
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    remote_.reset();
    buffer_reader_.reset();
    if (pending_read_cb_) {
      std::move(pending_read_cb_)
          .Run(Status::kAborted, scoped_refptr<media::DecoderBuffer>(nullptr));
    }
  }

  void OnBufferReady(scoped_refptr<media::DecoderBuffer> buffer) {
    DVLOG(3) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(buffer);

    // Starts an asynchronous callback to the browser. Will not be handled as
    // part of the current call sequence, but instead will be used as part of
    // the next call to Read().
    RequestNextBuffer();

    // Stop processing the pending buffer. OnMojoDisconnect() will trigger
    // sending kAborted on subsequent Read() calls. This can happen if this
    // object was in the process of reading a buffer off the data pipe when the
    // Mojo connection ended.
    if (!remote_.is_bound()) {
      DVLOG(1) << "Read has been cancelled due to mojo disconnection.";
      return;
    }

    // Can only occur when a read has been aborted.
    if (!pending_read_cb_) {
      DVLOG(1) << "Read has been cancelled via Abort() call.";
      return;
    }

    if (buffer->end_of_stream()) {
      std::move(pending_read_cb_).Run(Status::kError, nullptr);
    } else {
      std::move(pending_read_cb_).Run(Status::kOk, std::move(buffer));
    }
  }

  // Asynchronously requests a new buffer be sent from the browser process. The
  // result will be processed in OnGetBufferDone().
  void RequestNextBuffer() {
    DVLOG(3) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(remote_);

    remote_->GetBuffer(
        base::BindOnce(&CastStreamingDemuxerStream::OnGetBufferDone,
                       weak_factory_.GetWeakPtr()));
  }

  // Processes a new buffer as received over mojo.
  void OnGetBufferDone(TStreamInfoType data_stream_info,
                       media::mojom::DecoderBufferPtr buffer) {
    DVLOG(3) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (data_stream_info) {
      OnNewConfig(std::move(data_stream_info));
    }

    DCHECK(buffer_reader_);

    // Eventually calls OnBufferReady().
    buffer_reader_->ProvideBuffer(std::move(buffer));
  }

  // Called when a new config is received over mojo. Sets for the next call to
  // DemuxerStream::Read() to signal for a new config, and replaces the data
  // pipe which is used to read buffers in future.
  void OnNewConfig(TStreamInfoType data_stream_info) {
    DCHECK(data_stream_info);
    DVLOG(1) << __func__ << ": config info: "
             << data_stream_info->decoder_config.AsHumanReadableString();
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    decoder_config_ = std::move(data_stream_info->decoder_config);
    buffer_reader_ = std::make_unique<DecoderBufferReader>(
        base::BindRepeating(&CastStreamingDemuxerStream::OnBufferReady,
                            weak_factory_.GetWeakPtr()),
        std::move(data_stream_info->data_pipe));

    if (pending_read_cb_) {
      // Return early if there is already an ongoing Read() call. The prior
      // |buffer_reader_| instance will no longer exist, so the associated
      // OnBufferReady() call with which this Read() is associated will never
      // arrive - so the Read() call must be responded to now or the
      // DemuxerStream will deadlock.
      std::move(pending_read_cb_).Run(Status::kConfigChanged, nullptr);
    } else {
      pending_config_change_ = true;
    }
  }

  // DemuxerStream partial implementation.
  void Read(ReadCB read_cb) final {
    DVLOG(3) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    DCHECK(pending_read_cb_.is_null());
    pending_read_cb_ = std::move(read_cb);

    // Check whether OnMojoDisconnect() has been called and abort if so.
    if (!remote_) {
      std::move(pending_read_cb_)
          .Run(Status::kAborted, scoped_refptr<media::DecoderBuffer>(nullptr));
      return;
    }

    // Handle the special case of a config change.
    if (pending_config_change_) {
      // By design, the Read() method should never be called until after the
      // |decoder_config_| has been set.
      DCHECK(decoder_config_);

      pending_config_change_ = false;
      std::move(pending_read_cb_).Run(Status::kConfigChanged, nullptr);
      return;
    }

    // Eventually this will call OnBufferReady().
    buffer_reader_->ReadBufferAsync();
  }

  media::StreamLiveness liveness() const final {
    return media::StreamLiveness::kLive;
  }

  bool SupportsConfigChanges() final { return true; }

  mojo::Remote<TMojoRemoteType> remote_;

  // Called once the response to the first GetBuffer() call has been received.
  base::OnceClosure on_initialization_complete_;

  // Responsible for reading buffers from a data pipe.
  std::unique_ptr<DecoderBufferReader> buffer_reader_;

  // The current decoder config, empty until first received.
  absl::optional<ConfigType> decoder_config_;

  // Currently processing DemuxerStream::Read call's callback, if one is in
  // process.
  ReadCB pending_read_cb_;

  // True when this stream is undergoing a decoder configuration change.
  bool pending_config_change_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<CastStreamingDemuxerStream> weak_factory_;
};

}  // namespace

class CastStreamingAudioDemuxerStream final
    : public CastStreamingDemuxerStream<mojom::AudioBufferRequester,
                                        mojom::AudioStreamInfoPtr> {
 public:
  using CastStreamingDemuxerStream<
      mojom::AudioBufferRequester,
      mojom::AudioStreamInfoPtr>::CastStreamingDemuxerStream;

  ~CastStreamingAudioDemuxerStream() override = default;

 private:
  // DemuxerStream remainder of implementation.
  media::AudioDecoderConfig audio_decoder_config() final { return config(); }
  media::VideoDecoderConfig video_decoder_config() final {
    NOTREACHED();
    return media::VideoDecoderConfig();
  }
  Type type() const final { return Type::AUDIO; }
};

class CastStreamingVideoDemuxerStream final
    : public CastStreamingDemuxerStream<mojom::VideoBufferRequester,
                                        mojom::VideoStreamInfoPtr> {
 public:
  using CastStreamingDemuxerStream<
      mojom::VideoBufferRequester,
      mojom::VideoStreamInfoPtr>::CastStreamingDemuxerStream;

  ~CastStreamingVideoDemuxerStream() override = default;

 private:
  // DemuxerStream remainder of implementation.
  media::AudioDecoderConfig audio_decoder_config() final {
    NOTREACHED();
    return media::AudioDecoderConfig();
  }
  media::VideoDecoderConfig video_decoder_config() final { return config(); }
  Type type() const final { return Type::VIDEO; }
};

CastStreamingDemuxer::CastStreamingDemuxer(
    CastStreamingReceiver* receiver,
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner)
    : media_task_runner_(media_task_runner),
      original_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      receiver_(receiver),
      weak_factory_(this) {
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
    mojom::AudioStreamInitializationInfoPtr audio_stream_info,
    mojom::VideoStreamInitializationInfoPtr video_stream_info) {
  DVLOG(1) << __func__;
  DCHECK(!media_task_runner_->BelongsToCurrentThread());

  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CastStreamingDemuxer::OnStreamsInitializedOnMediaThread,
                     weak_factory_.GetWeakPtr(), std::move(audio_stream_info),
                     std::move(video_stream_info)));
}

void CastStreamingDemuxer::OnStreamsInitializedOnMediaThread(
    mojom::AudioStreamInitializationInfoPtr audio_stream_info,
    mojom::VideoStreamInitializationInfoPtr video_stream_info) {
  DVLOG(1) << __func__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DCHECK(initialized_cb_);

  if (!audio_stream_info && !video_stream_info) {
    std::move(initialized_cb_).Run(media::DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  if (audio_stream_info) {
    audio_stream_ = std::make_unique<CastStreamingAudioDemuxerStream>(
        std::move(audio_stream_info->buffer_requester),
        std::move(audio_stream_info->stream_initialization_info));
  }
  if (video_stream_info) {
    video_stream_ = std::make_unique<CastStreamingVideoDemuxerStream>(
        std::move(video_stream_info->buffer_requester),
        std::move(video_stream_info->stream_initialization_info));
  }

  was_initialization_successful_ = true;
  std::move(initialized_cb_).Run(media::PIPELINE_OK);
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
  std::move(status_cb).Run(media::PIPELINE_OK);
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
