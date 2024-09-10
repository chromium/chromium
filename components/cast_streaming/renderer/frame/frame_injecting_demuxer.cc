// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/frame/frame_injecting_demuxer.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/cast_streaming/common/frame/demuxer_stream_traits.h"
#include "components/cast_streaming/renderer/common/buffer_requester.h"
#include "components/cast_streaming/renderer/frame/demuxer_connector.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace cast_streaming {

class StreamTimestampOffsetTracker
    : public base::RefCountedThreadSafe<StreamTimestampOffsetTracker> {
 public:
  StreamTimestampOffsetTracker() = default;

  void ProcessAudioPacket(media::DecoderBuffer& buffer) {
    if (!audio_position_.is_zero() &&
        audio_position_ + base::Milliseconds(100) < buffer.timestamp()) {
      offset_ += buffer.timestamp() - audio_position_;
    }

    audio_position_ =
        buffer.timestamp() + ((buffer.duration() == media::kNoTimestamp)
                                  ? base::Milliseconds(20)
                                  : buffer.duration());
  }

  void UpdateOffset(media::DecoderBuffer& buffer) {
    buffer.set_timestamp(buffer.timestamp() - offset_);
  }

  void ResetPosition() {
    audio_position_ = {};
    offset_ = {};
  }

 private:
  friend class base::RefCountedThreadSafe<StreamTimestampOffsetTracker>;
  ~StreamTimestampOffsetTracker() = default;

  base::TimeDelta audio_position_ = {};
  base::TimeDelta offset_ = {};
  raw_ptr<media::DemuxerHost> demuxer_host_ = nullptr;
};

namespace {

// media::DemuxerStream implementation used for audio and video streaming.
// Receives buffer metadata over a Mojo service through a "pull" mechanism using
// the associated mojo interface's GetBuffer() method and reads the buffer's
// contents over a Mojo data pipe from the browser process.
//
// |TMojoRemoteType| is the interface used for requesting data buffers.
// Currently expected to be either AudioBufferRequester or VideoBufferRequester.
template <typename TMojoRemoteType>
class FrameInjectingDemuxerStream
    : public DemuxerStreamTraits<TMojoRemoteType>,
      public BufferRequester<TMojoRemoteType>::Client,
      public media::DemuxerStream {
 public:
  // See DemuxerStreamTraits for further details on these types.
  using Traits = DemuxerStreamTraits<TMojoRemoteType>;
  using ConfigType = typename Traits::ConfigType;

  FrameInjectingDemuxerStream(
      mojo::PendingRemote<TMojoRemoteType> pending_remote,
      typename Traits::StreamInfoType stream_initialization_info,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::OnceClosure on_initialization_complete_cb,
      scoped_refptr<StreamTimestampOffsetTracker> timestamp_tracker)
      : on_initialization_complete_cb_(
            std::move(on_initialization_complete_cb)),
        decoder_config_(stream_initialization_info->decoder_config),
        timestamp_tracker_(std::move(timestamp_tracker)),
        buffer_requester_(std::make_unique<BufferRequester<TMojoRemoteType>>(
            this,
            stream_initialization_info->decoder_config,
            std::move(stream_initialization_info->data_pipe),
            std::move(pending_remote),
            task_runner)),
        weak_factory_(this) {}

  ~FrameInjectingDemuxerStream() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void AbortPendingRead() {
    DVLOG(3) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    was_read_aborted_ = true;
    if (pending_read_cb_) {
      std::move(pending_read_cb_).Run(Status::kAborted, {});
    }
  }

 protected:
  const ConfigType& config() { return decoder_config_; }

 private:
  void OnBitstreamConverterEnabled(bool success) {
    if (!success) {
      LOG(ERROR) << "Failed to enable Bitstream Converter";
      OnMojoDisconnect();
      return;
    }

    is_bitstream_enable_in_progress_ = false;
    if (pending_read_cb_) {
      Read(1, std::move(pending_read_cb_));
    }
  }

  // Called by |current_buffer_provider_->ReadBufferAsync()|.
  void OnNewBuffer(scoped_refptr<media::DecoderBuffer> buffer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!!pending_read_cb_ != was_read_aborted_);

    was_read_aborted_ = false;
    if (!pending_read_cb_) {
      return;
    }

    if (!buffer) {
      std::move(pending_read_cb_).Run(Status::kAborted, {});
    } else if (buffer->end_of_stream()) {
      std::move(pending_read_cb_).Run(Status::kError, {});
    } else {
      if (type() == Type::AUDIO) {
        timestamp_tracker_->ProcessAudioPacket(*buffer);
      }
      timestamp_tracker_->UpdateOffset(*buffer);
      std::move(pending_read_cb_).Run(Status::kOk, {std::move(buffer)});
    }
  }

  // Called by |current_buffer_provider_->GetConfigAsync()|.
  void OnNewConfig(ConfigType config) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!!pending_read_cb_ != was_read_aborted_);

    decoder_config_ = std::move(config);
    was_read_aborted_ = false;
    pending_config_change_ = true;

    if (pending_read_cb_) {
      Read(1, std::move(pending_read_cb_));
    }
  }

  // BufferRequester::Client implementation.
  void OnNewBufferProvider(
      base::WeakPtr<DecoderBufferProvider<ConfigType>> ptr) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    current_buffer_provider_ = std::move(ptr);

    if (type() == Type::AUDIO) {
      timestamp_tracker_->ResetPosition();
    }

    // During initialization, the config is set from the instance provided to
    // the ctor.
    if (on_initialization_complete_cb_) {
      std::move(on_initialization_complete_cb_).Run();
      return;
    }

    current_buffer_provider_->GetConfigAsync(base::BindOnce(
        &FrameInjectingDemuxerStream::OnNewConfig, weak_factory_.GetWeakPtr()));
  }

  void OnMojoDisconnect() override {
    DLOG(ERROR) << __func__ << ": Mojo Pipe Disconnected";
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    buffer_requester_.reset();
    if (pending_read_cb_) {
      std::move(pending_read_cb_).Run(Status::kAborted, {});
    }
  }

  // DemuxerStream partial implementation.
  void Read(uint32_t count, ReadCB read_cb) final {
    DVLOG(3) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!pending_read_cb_);
    DCHECK(!buffer_requester_ || current_buffer_provider_);
    DCHECK_EQ(count, 1u)
        << "FrameInjectingDemuxerStream only reads a single buffer.";

    pending_read_cb_ = std::move(read_cb);

    // Check whether OnMojoDisconnect() has been called and abort if so.
    if (!buffer_requester_) {
      std::move(pending_read_cb_).Run(Status::kAborted, {});
      return;
    }

    // In this case, a callback to get a Read() result is already running, so
    // just replace the |pending_read_cb_| and let the ongoing call continue.
    if (was_read_aborted_) {
      was_read_aborted_ = false;
      return;
    }

    // Handle the special case of a config change.
    if (pending_config_change_) {
      pending_config_change_ = false;
      std::move(pending_read_cb_).Run(Status::kConfigChanged, {});
      return;
    }

    // If enabling bitstream conversion is in progress, do not send a Read()
    // request until that has succeeded.
    if (is_bitstream_enable_in_progress_) {
      return;
    }

    current_buffer_provider_->ReadBufferAsync(base::BindOnce(
        &FrameInjectingDemuxerStream::OnNewBuffer, weak_factory_.GetWeakPtr()));
  }

  void EnableBitstreamConverter() final {
    DCHECK(buffer_requester_);
    is_bitstream_enable_in_progress_ = true;
    buffer_requester_->EnableBitstreamConverterAsync(base::BindOnce(
        &FrameInjectingDemuxerStream::OnBitstreamConverterEnabled,
        weak_factory_.GetWeakPtr()));
  }

  media::StreamLiveness liveness() const final {
    return media::StreamLiveness::kLive;
  }

  bool SupportsConfigChanges() final { return true; }

  // Called once the response to the first GetBuffer() call has been received.
  base::OnceClosure on_initialization_complete_cb_;

  // Current config, as last provided by |current_buffer_provider_|.
  ConfigType decoder_config_;

  // Currently processing DemuxerStream::Read call's callback, if one is in
  // process.
  ReadCB pending_read_cb_;

  scoped_refptr<StreamTimestampOffsetTracker> timestamp_tracker_;

  // True when this stream is undergoing a decoder configuration change.
  bool pending_config_change_ = false;

  bool is_bitstream_enable_in_progress_ = false;

  bool was_read_aborted_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<DecoderBufferProvider<ConfigType>> current_buffer_provider_;
  std::unique_ptr<BufferRequester<TMojoRemoteType>> buffer_requester_;

  base::WeakPtrFactory<FrameInjectingDemuxerStream> weak_factory_;
};

}  // namespace

class FrameInjectingAudioDemuxerStream final
    : public FrameInjectingDemuxerStream<mojom::AudioBufferRequester> {
 public:
  using FrameInjectingDemuxerStream<
      mojom::AudioBufferRequester>::FrameInjectingDemuxerStream;

  ~FrameInjectingAudioDemuxerStream() override = default;

 private:
  // DemuxerStream remainder of implementation.
  media::AudioDecoderConfig audio_decoder_config() final { return config(); }
  media::VideoDecoderConfig video_decoder_config() final {
    NOTREACHED_IN_MIGRATION();
    return media::VideoDecoderConfig();
  }
  Type type() const final { return Type::AUDIO; }
};

class FrameInjectingVideoDemuxerStream final
    : public FrameInjectingDemuxerStream<mojom::VideoBufferRequester> {
 public:
  using FrameInjectingDemuxerStream<
      mojom::VideoBufferRequester>::FrameInjectingDemuxerStream;

  ~FrameInjectingVideoDemuxerStream() override = default;

 private:
  // DemuxerStream remainder of implementation.
  media::AudioDecoderConfig audio_decoder_config() final {
    NOTREACHED_IN_MIGRATION();
    return media::AudioDecoderConfig();
  }
  media::VideoDecoderConfig video_decoder_config() final { return config(); }
  Type type() const final { return Type::VIDEO; }
};

FrameInjectingDemuxer::FrameInjectingDemuxer(
    DemuxerConnector* demuxer_connector,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner)
    : media_task_runner_(std::move(media_task_runner)),
      original_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      demuxer_connector_(demuxer_connector),
      weak_factory_(this) {
  DVLOG(1) << __func__;
  DCHECK(demuxer_connector_);
}

FrameInjectingDemuxer::~FrameInjectingDemuxer() {
  DVLOG(1) << __func__;

  if (was_initialization_successful_) {
    original_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DemuxerConnector::OnDemuxerDestroyed,
                                  base::Unretained(demuxer_connector_)));
  }
}

void FrameInjectingDemuxer::OnStreamsInitialized(
    mojom::AudioStreamInitializationInfoPtr audio_stream_info,
    mojom::VideoStreamInitializationInfoPtr video_stream_info) {
  DVLOG(1) << __func__;
  DCHECK(!media_task_runner_->RunsTasksInCurrentSequence());

  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FrameInjectingDemuxer::OnStreamsInitializedOnMediaThread,
                     weak_factory_.GetWeakPtr(), std::move(audio_stream_info),
                     std::move(video_stream_info)));
}

void FrameInjectingDemuxer::OnStreamsInitializedOnMediaThread(
    mojom::AudioStreamInitializationInfoPtr audio_stream_info,
    mojom::VideoStreamInitializationInfoPtr video_stream_info) {
  DVLOG(1) << __func__;
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(initialized_cb_);

  if (!audio_stream_info && !video_stream_info) {
    std::move(initialized_cb_).Run(media::DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  auto initialization_complete_cb_ = base::BindRepeating(
      &FrameInjectingDemuxer::OnStreamInitializationComplete,
      weak_factory_.GetWeakPtr());

  timestamp_tracker_ = base::MakeRefCounted<StreamTimestampOffsetTracker>();

  if (audio_stream_info) {
    pending_stream_initialization_callbacks_++;
    audio_stream_ = std::make_unique<FrameInjectingAudioDemuxerStream>(
        std::move(audio_stream_info->buffer_requester),
        std::move(audio_stream_info->stream_initialization_info),
        media_task_runner_, initialization_complete_cb_, timestamp_tracker_);
  }
  if (video_stream_info) {
    pending_stream_initialization_callbacks_++;
    video_stream_ = std::make_unique<FrameInjectingVideoDemuxerStream>(
        std::move(video_stream_info->buffer_requester),
        std::move(video_stream_info->stream_initialization_info),
        media_task_runner_, std::move(initialization_complete_cb_),
        timestamp_tracker_);
  }
}

void FrameInjectingDemuxer::OnStreamInitializationComplete() {
  DCHECK_GE(pending_stream_initialization_callbacks_, 1);
  DCHECK(initialized_cb_);

  pending_stream_initialization_callbacks_--;
  if (pending_stream_initialization_callbacks_ != 0) {
    return;
  }

  was_initialization_successful_ = true;
  std::move(initialized_cb_).Run(media::PIPELINE_OK);
}

std::vector<media::DemuxerStream*> FrameInjectingDemuxer::GetAllStreams() {
  DVLOG(1) << __func__;
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  std::vector<media::DemuxerStream*> streams;
  if (video_stream_) {
    streams.push_back(video_stream_.get());
  }
  if (audio_stream_) {
    streams.push_back(audio_stream_.get());
  }
  return streams;
}

std::string FrameInjectingDemuxer::GetDisplayName() const {
  return "FrameInjectingDemuxer";
}

media::DemuxerType FrameInjectingDemuxer::GetDemuxerType() const {
  return media::DemuxerType::kFrameInjectingDemuxer;
}

void FrameInjectingDemuxer::Initialize(
    media::DemuxerHost* host,
    media::PipelineStatusCallback status_cb) {
  DVLOG(1) << __func__;
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  host_ = host;

  // Live streams have infinite duration.
  host_->SetDuration(media::kInfiniteDuration);
  initialized_cb_ = std::move(status_cb);

  original_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DemuxerConnector::SetDemuxer,
                                base::Unretained(demuxer_connector_),
                                base::Unretained(this)));
}

void FrameInjectingDemuxer::AbortPendingReads() {
  DVLOG(2) << __func__;
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  timestamp_tracker_->ResetPosition();

  if (audio_stream_) {
    audio_stream_->AbortPendingRead();
  }
  if (video_stream_) {
    video_stream_->AbortPendingRead();
  }
}

// Not supported.
void FrameInjectingDemuxer::StartWaitingForSeek(base::TimeDelta seek_time) {}

// Not supported.
void FrameInjectingDemuxer::CancelPendingSeek(base::TimeDelta seek_time) {}

// Not supported.
void FrameInjectingDemuxer::Seek(base::TimeDelta time,
                                 media::PipelineStatusCallback status_cb) {
  timestamp_tracker_->ResetPosition();
  std::move(status_cb).Run(media::PIPELINE_OK);
}

bool FrameInjectingDemuxer::IsSeekable() const {
  return false;
}

void FrameInjectingDemuxer::Stop() {
  DVLOG(1) << __func__;
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (audio_stream_) {
    audio_stream_.reset();
  }
  if (video_stream_) {
    video_stream_.reset();
  }
}

base::TimeDelta FrameInjectingDemuxer::GetStartTime() const {
  return base::TimeDelta();
}

// Not supported.
base::Time FrameInjectingDemuxer::GetTimelineOffset() const {
  return base::Time();
}

// Not supported.
int64_t FrameInjectingDemuxer::GetMemoryUsage() const {
  return 0;
}

std::optional<media::container_names::MediaContainerName>
FrameInjectingDemuxer::GetContainerForMetrics() const {
  // Cast Streaming frames have no container.
  return std::nullopt;
}

// Not supported.
void FrameInjectingDemuxer::OnEnabledAudioTracksChanged(
    const std::vector<media::MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  DLOG(WARNING) << "Track changes are not supported.";
  std::vector<media::DemuxerStream*> streams;
  std::move(change_completed_cb).Run(streams);
}

// Not supported.
void FrameInjectingDemuxer::OnSelectedVideoTrackChanged(
    const std::vector<media::MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  DLOG(WARNING) << "Track changes are not supported.";
  std::vector<media::DemuxerStream*> streams;
  std::move(change_completed_cb).Run(streams);
}

}  // namespace cast_streaming
