// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/frame/demuxer_connector.h"

#include "components/cast_streaming/renderer/frame/frame_injecting_demuxer.h"

namespace cast_streaming {

DemuxerConnector::DemuxerConnector() = default;

DemuxerConnector::~DemuxerConnector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DemuxerConnector::SetDemuxer(FrameInjectingDemuxer* demuxer) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(demuxer);

  if (demuxer_) {
    // We do not support more than one active FrameInjectingDemuxer in the same
    // RenderFrame. Return early here.
    demuxer->OnStreamsInitialized(mojom::AudioStreamInitializationInfoPtr(),
                                  mojom::VideoStreamInitializationInfoPtr());
    return;
  }

  DCHECK(!is_demuxer_initialized_);

  if (IsBound()) {
    demuxer_ = demuxer;
    MaybeCallEnableReceiverCallback();
  } else {
    // The Cast Streaming Sender disconnected after |demuxer| was instantiated
    // but before |demuxer| was initialized on the media thread.
    demuxer->OnStreamsInitialized(mojom::AudioStreamInitializationInfoPtr(),
                                  mojom::VideoStreamInitializationInfoPtr());
  }
}

void DemuxerConnector::OnDemuxerDestroyed() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(demuxer_);

  demuxer_ = nullptr;
  is_demuxer_initialized_ = false;
  demuxer_connector_receiver_.reset();
}

void DemuxerConnector::BindReceiver(
    mojo::PendingAssociatedReceiver<mojom::DemuxerConnector> receiver) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!demuxer_connector_receiver_.is_bound());

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

  if (enable_receiver_callback_ && demuxer_) {
    std::move(enable_receiver_callback_).Run();
  }
}

void DemuxerConnector::OnReceiverDisconnected() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  demuxer_connector_receiver_.reset();
  enable_receiver_callback_.Reset();

  if (demuxer_ && !is_demuxer_initialized_) {
    OnStreamsInitialized(mojom::AudioStreamInitializationInfoPtr(),
                         mojom::VideoStreamInitializationInfoPtr());
  }
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
  DCHECK(!is_demuxer_initialized_);
  DCHECK(demuxer_);

  is_demuxer_initialized_ = true;
  demuxer_->OnStreamsInitialized(std::move(audio_stream_info),
                                 std::move(video_stream_info));
}

}  // namespace cast_streaming
