// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_RENDERER_FRAME_DEMUXER_CONNECTOR_H_
#define COMPONENTS_CAST_STREAMING_RENDERER_FRAME_DEMUXER_CONNECTOR_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "components/cast_streaming/common/public/mojom/demuxer_connector.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace cast_streaming {

class FrameInjectingDemuxer;

// Handles initiating the streaming session between the browser-process sender
// and renderer-process receiver of the Cast Streaming Session. Specifically,
// this class manages the DemuxerConnector's lifetime in the renderer
// process. The lifetime of this object should match that of |render_frame| with
// which it is associated, and is guaranteed to outlive the
// FrameInjectingDemuxer that uses it, as the RenderFrame destruction will have
// triggered its destruction first.
class DemuxerConnector final : public mojom::DemuxerConnector {
 public:
  DemuxerConnector();
  ~DemuxerConnector() override;
  DemuxerConnector(const DemuxerConnector&) = delete;
  DemuxerConnector& operator=(const DemuxerConnector&) = delete;

  void BindReceiver(
      mojo::PendingAssociatedReceiver<mojom::DemuxerConnector> connector);

  void SetDemuxer(FrameInjectingDemuxer* demuxer);
  void OnDemuxerDestroyed();

  // Returns true if a Mojo connection is active.
  bool IsBound() const;

 private:
  void MaybeCallEnableReceiverCallback();

  void OnReceiverDisconnected();

  // mojom::DemuxerConnector implementation.
  void EnableReceiver(EnableReceiverCallback callback) override;
  void OnStreamsInitialized(
      mojom::AudioStreamInitializationInfoPtr audio_stream_info,
      mojom::VideoStreamInitializationInfoPtr video_stream_info) override;

  mojo::AssociatedReceiver<mojom::DemuxerConnector> demuxer_connector_receiver_{
      this};

  EnableReceiverCallback enable_receiver_callback_;
  raw_ptr<FrameInjectingDemuxer> demuxer_ = nullptr;
  bool is_demuxer_initialized_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_RENDERER_FRAME_DEMUXER_CONNECTOR_H_
