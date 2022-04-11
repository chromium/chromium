// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_RENDERER_RPC_CALL_TRANSLATOR_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_RENDERER_RPC_CALL_TRANSLATOR_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/cast_streaming/public/rpc_call_message_handler.h"
#include "media/base/renderer.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace openscreen::cast {
class RpcMessage;
}  // namespace openscreen::cast

namespace cast_streaming::remoting {

// This class is responsible for translating between
// openscreen::cast::RpcMessage instances (used by the remoting protocol) and
// mojo API calls (used locally within this chromium instance).
class RendererRpcCallTranslator : public media::mojom::RendererClient,
                                  public RpcRendererCallMessageHandler {
 public:
  using RpcMessageProcessor = base::RepeatingCallback<void(
      std::unique_ptr<openscreen::cast::RpcMessage>)>;

  // |remote_renderer| is the remote media::mojom::Renderer to which commands
  // translated from proto messages should be sent.
  explicit RendererRpcCallTranslator(
      mojo::Remote<media::mojom::Renderer> remote_renderer);
  ~RendererRpcCallTranslator() override;

  // |processor| is responsible for handling any proto messages ready to be sent
  // out. This callback is expected to set the handle in each incoming message.
  // This callback must be callable from any thread.
  void SetMessageProcessor(RpcMessageProcessor processor);

 private:
  // media::mojom::RendererClient overrides.
  void OnTimeUpdate(base::TimeDelta media_time,
                    base::TimeDelta max_time,
                    base::TimeTicks capture_time) override;
  void OnBufferingStateChange(
      media::BufferingState state,
      media::BufferingStateChangeReason reason) override;
  void OnEnded() override;
  void OnError(const media::PipelineStatus& status) override;
  void OnAudioConfigChange(const media::AudioDecoderConfig& config) override;
  void OnVideoConfigChange(const media::VideoDecoderConfig& config) override;
  void OnVideoNaturalSizeChange(const gfx::Size& size) override;
  void OnVideoOpacityChange(bool opaque) override;
  void OnStatisticsUpdate(const media::PipelineStatistics& stats) override;
  void OnWaiting(media::WaitingReason reason) override;

  // RpcCallMessageHandler overrides.
  void OnRpcInitialize() override;
  void OnRpcFlush(uint32_t audio_count, uint32_t video_count) override;
  void OnRpcStartPlayingFrom(base::TimeDelta time) override;
  void OnRpcSetPlaybackRate(double playback_rate) override;
  void OnRpcSetVolume(double volume) override;

  // Callbacks for mojo calls. |processor| is included as an input so that if
  // the callback changes before the response to this message is returned, it
  // will send with the old |message_processor_| value.
  void OnInitializeCompleted(RpcMessageProcessor processor, bool succeeded);
  void OnFlushCompleted(RpcMessageProcessor processor);

  RpcMessageProcessor message_processor_;

  mojo::AssociatedReceiver<media::mojom::RendererClient>
      renderer_client_receiver_;
  mojo::Remote<media::mojom::Renderer> renderer_remote_;

  base::WeakPtrFactory<RendererRpcCallTranslator> weak_factory_;
};

}  // namespace cast_streaming::remoting

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_RENDERER_RPC_CALL_TRANSLATOR_H_
