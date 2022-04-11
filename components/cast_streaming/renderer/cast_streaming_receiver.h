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

namespace cast_streaming {

class CastStreamingDemuxer;

// Handles the Cast Streaming Session lifetime in the renderer process.
// Owned by WebEngineRenderFrameObserver, this object will be destroyed on
// RenderFrame destruction. This is guaranteed to outlive the
// CastStreamingDemuxer that uses it as the RenderFrame destruction will have
// triggered the destruction of the media pipeline and the CastStreamingDemuxer
// before the call to content::RenderFrameObserver::OnDestruct(), which triggers
// this object destruction.
class CastStreamingReceiver final : public mojom::CastStreamingReceiver {
 public:
  using PendingCastStreamingReceiver =
      mojo::PendingAssociatedReceiver<mojom::CastStreamingReceiver>;
  using InterfaceRegistryBinderCallback = base::RepeatingCallback<void(
      CastStreamingReceiver::PendingCastStreamingReceiver)>;

  explicit CastStreamingReceiver(
      base::OnceCallback<void(InterfaceRegistryBinderCallback)>
          interface_binder_factory);
  ~CastStreamingReceiver() override;

  CastStreamingReceiver(const CastStreamingReceiver&) = delete;
  CastStreamingReceiver& operator=(const CastStreamingReceiver&) = delete;

  void SetDemuxer(CastStreamingDemuxer* demuxer);
  void OnDemuxerDestroyed();

  // Returns true if a Mojo connection is active.
  bool IsBound() const;

 private:
  void BindToReceiver(PendingCastStreamingReceiver receiver);

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
