// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_RENDERER_CAST_STREAMING_RECEIVER_H_
#define COMPONENTS_CAST_STREAMING_RENDERER_CAST_STREAMING_RECEIVER_H_

#include "base/callback.h"
#include "base/sequence_checker.h"
#include "components/cast_streaming/public/mojom/cast_streaming_session.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace cast_streaming {

class CastStreamingDemuxer;

// Handles initiating the streaming session between the browser-process sender
// and renderer-process receiver of the Cast Streaming Session. Specifically,
// this class manages the CastStreamingReceiver's lifetime in the renderer
// process. The lifetime of this object should match that of |render_frame| with
// which it is associated, and is guaranteed to outlive the CastStreamingDemuxer
// that uses it, as the RenderFrame destruction will have triggered its
// destruction first.
class CastStreamingReceiver final : public mojom::CastStreamingReceiver {
 public:
  explicit CastStreamingReceiver(content::RenderFrame* render_frame);
  ~CastStreamingReceiver() override;

  CastStreamingReceiver(const CastStreamingReceiver&) = delete;
  CastStreamingReceiver& operator=(const CastStreamingReceiver&) = delete;

  void SetDemuxer(CastStreamingDemuxer* demuxer);
  void OnDemuxerDestroyed();

  // Returns true if a Mojo connection is active.
  bool IsBound() const;

 private:
  void BindToReceiver(
      mojo::PendingAssociatedReceiver<mojom::CastStreamingReceiver> receiver);

  void MaybeCallEnableReceiverCallback();

  void OnReceiverDisconnected();

  // mojom::CastStreamingReceiver implementation.
  void EnableReceiver(EnableReceiverCallback callback) override;
  void OnStreamsInitialized(
      mojom::AudioStreamInitializationInfoPtr audio_stream_info,
      mojom::VideoStreamInitializationInfoPtr video_stream_info) override;

  mojo::AssociatedReceiver<mojom::CastStreamingReceiver>
      cast_streaming_receiver_receiver_{this};

  EnableReceiverCallback enable_receiver_callback_;
  CastStreamingDemuxer* demuxer_ = nullptr;
  bool is_demuxer_initialized_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_RENDERER_CAST_STREAMING_RECEIVER_H_
