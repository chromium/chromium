// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/cast_streaming_receiver.h"

#include "components/cast_streaming/renderer/cast_streaming_demuxer.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace cast_streaming {

CastStreamingReceiver::CastStreamingReceiver(
    content::RenderFrame* render_frame) {
  DVLOG(1) << __func__;
  DCHECK(render_frame);

  // It is fine to use an unretained pointer to |this| here as the
  // AssociatedInterfaceRegistry, owned by |render_frame| will be torn-down at
  // the same time as |this|.
  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(
      base::BindRepeating(&CastStreamingReceiver::BindToReceiver,
                          base::Unretained(this)));
}

CastStreamingReceiver::~CastStreamingReceiver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CastStreamingReceiver::SetDemuxer(CastStreamingDemuxer* demuxer) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(demuxer);

  if (demuxer_) {
    // We do not support more than one active CastStreamingDemuxer in the same
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

void CastStreamingReceiver::OnDemuxerDestroyed() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(demuxer_);

  demuxer_ = nullptr;
  is_demuxer_initialized_ = false;
  cast_streaming_receiver_receiver_.reset();
}

void CastStreamingReceiver::BindToReceiver(
    mojo::PendingAssociatedReceiver<mojom::CastStreamingReceiver> receiver) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!cast_streaming_receiver_receiver_.is_bound());

  cast_streaming_receiver_receiver_.Bind(std::move(receiver));

  // Mojo service disconnection means the Cast Streaming Session ended or the
  // Cast Streaming Sender disconnected.
  cast_streaming_receiver_receiver_.set_disconnect_handler(base::BindOnce(
      &CastStreamingReceiver::OnReceiverDisconnected, base::Unretained(this)));
}

bool CastStreamingReceiver::IsBound() const {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return cast_streaming_receiver_receiver_.is_bound();
}

void CastStreamingReceiver::MaybeCallEnableReceiverCallback() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (enable_receiver_callback_ && demuxer_)
    std::move(enable_receiver_callback_).Run();
}

void CastStreamingReceiver::OnReceiverDisconnected() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cast_streaming_receiver_receiver_.reset();
  enable_receiver_callback_.Reset();

  if (demuxer_ && !is_demuxer_initialized_) {
    OnStreamsInitialized(mojom::AudioStreamInitializationInfoPtr(),
                         mojom::VideoStreamInitializationInfoPtr());
  }
}

void CastStreamingReceiver::EnableReceiver(EnableReceiverCallback callback) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!enable_receiver_callback_);
  DCHECK(callback);

  enable_receiver_callback_ = std::move(callback);
  MaybeCallEnableReceiverCallback();
}

void CastStreamingReceiver::OnStreamsInitialized(
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
