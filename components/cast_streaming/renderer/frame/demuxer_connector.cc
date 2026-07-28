// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/frame/demuxer_connector.h"

#include "components/cast_streaming/renderer/frame/frame_injecting_demuxer.h"

namespace cast_streaming {

DemuxerStreamConfigBuffer::DemuxerStreamConfigBuffer() = default;
DemuxerStreamConfigBuffer::~DemuxerStreamConfigBuffer() = default;

void DemuxerStreamConfigBuffer::SetConfigs(
    mojom::AudioStreamInitializationInfoPtr audio_stream_info,
    mojom::VideoStreamInitializationInfoPtr video_stream_info) {
  base::OnceClosure closure;
  scoped_refptr<base::SequencedTaskRunner> target_runner;
  {
    base::AutoLock lock(lock_);
    if (has_configs_) {
      return;
    }
    has_configs_ = true;
    audio_stream_info_ = std::move(audio_stream_info);
    video_stream_info_ = std::move(video_stream_info);

    if (pending_callback_ && media_task_runner_) {
      target_runner = media_task_runner_;
      closure = base::BindOnce(std::move(pending_callback_),
                               std::move(audio_stream_info_),
                               std::move(video_stream_info_));
    }
  }
  if (closure && target_runner) {
    target_runner->PostTask(FROM_HERE, std::move(closure));
  }
}

void DemuxerStreamConfigBuffer::ReadConfigs(
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    ConfigCallback callback) {
  base::OnceClosure closure;
  {
    base::AutoLock lock(lock_);
    if (has_configs_) {
      closure =
          base::BindOnce(std::move(callback), std::move(audio_stream_info_),
                         std::move(video_stream_info_));
    } else {
      media_task_runner_ = media_task_runner;
      pending_callback_ = std::move(callback);
    }
  }
  if (closure) {
    media_task_runner->PostTask(FROM_HERE, std::move(closure));
  }
}

DemuxerConnector::DemuxerConnector() = default;

DemuxerConnector::~DemuxerConnector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DemuxerConnector::BindReceiver(
    mojo::PendingAssociatedReceiver<mojom::DemuxerConnector> receiver) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!demuxer_connector_receiver_.is_bound());

  config_buffer_ = base::MakeRefCounted<DemuxerStreamConfigBuffer>();
  demuxer_connector_receiver_.Bind(std::move(receiver));

  // Mojo service disconnection means the Cast Streaming Session ended or the
  // Cast Streaming Sender disconnected.
  demuxer_connector_receiver_.set_disconnect_handler(base::BindOnce(
      &DemuxerConnector::OnReceiverDisconnected, base::Unretained(this)));
}

bool DemuxerConnector::IsBound() const {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return demuxer_connector_receiver_.is_bound();
}

void DemuxerConnector::MaybeCallEnableReceiverCallback() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (enable_receiver_callback_) {
    std::move(enable_receiver_callback_).Run();
  }
}

void DemuxerConnector::OnReceiverDisconnected() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  demuxer_connector_receiver_.reset();
  enable_receiver_callback_.Reset();

  config_buffer_->SetConfigs(mojom::AudioStreamInitializationInfoPtr(),
                             mojom::VideoStreamInitializationInfoPtr());
  config_buffer_ = base::MakeRefCounted<DemuxerStreamConfigBuffer>();
}

void DemuxerConnector::EnableReceiver(EnableReceiverCallback callback) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!enable_receiver_callback_);
  DCHECK(callback);

  enable_receiver_callback_ = std::move(callback);
  MaybeCallEnableReceiverCallback();
}

void DemuxerConnector::OnStreamsInitialized(
    mojom::AudioStreamInitializationInfoPtr audio_stream_info,
    mojom::VideoStreamInitializationInfoPtr video_stream_info) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  config_buffer_->SetConfigs(std::move(audio_stream_info),
                             std::move(video_stream_info));
}

}  // namespace cast_streaming
