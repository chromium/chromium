// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_CONTROL_REMOTING_RENDERER_RPC_CALL_TRANSLATOR_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_CONTROL_REMOTING_RENDERER_RPC_CALL_TRANSLATOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/renderer.h"
#include "media/cast/openscreen/rpc_call_message_handler.h"
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
class RendererRpcCallTranslator
    : public media::mojom::RendererClient,
      public media::cast::RpcRendererCallMessageHandler {
 public:
  using RpcMessageProcessor = base::RepeatingCallback<void(
      openscreen::cast::RpcMessenger::Handle handle,
      std::unique_ptr<openscreen::cast::RpcMessage>)>;
  using FlushUntilCallback = base::RepeatingCallback<void(uint32_t, uint32_t)>;

  // |renderer| is the remote media::mojom::Renderer to which commands
  // translated from proto messages should be sent.
  // |processor| is responsible for handling any proto messages ready to be sent
  // out.
  explicit RendererRpcCallTranslator(RpcMessageProcessor processor,
                                     media::mojom::Renderer* renderer,
                                     FlushUntilCallback flush_until_cb);
  ~RendererRpcCallTranslator() override;

  void SendFallbackMessage();

  // Sets the |handle| to be used for future outgoing RPC calls.
  void set_remote_handle(openscreen::cast::RpcMessenger::Handle handle) {
    remote_handle_ = handle;
  }
  openscreen::cast::RpcMessenger::Handle remote_handle() const {
    return remote_handle_;
  }

  // Sets the |handle| associated with this instance.
  void set_local_handle(openscreen::cast::RpcMessenger::Handle handle) {
    local_handle_ = handle;
  }
  openscreen::cast::RpcMessenger::Handle local_handle() const {
    return local_handle_;
  }

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

  // Callback for the Initialize() mojo call. |handle_at_time_of_sending| is
  // included as an input so that if |handle_| changes before the response to
  // this message is returned, it will send with the old |handle_| value.
  void OnInitializeCompleted(
      openscreen::cast::RpcMessenger::Handle handle_at_time_of_sending,
      bool succeeded);

  // Callback to Flush() mojo call. Sends an ack message for the completion of
  // the flush command for each handle in |flush_handles_|.
  void OnFlushCompleted();

  // Signifies whether the Initialize() command has been sent to the Renderer,
  // which will only be done once over the duration of this instance's lifetime.
  bool has_been_initialized_ = false;

  // Called as part of responding to an OnRpcFlush() call, to inform the owning
  // class that in-flight frames should be flushed.
  FlushUntilCallback flush_until_cb_;

  std::vector<openscreen::cast::RpcMessenger::Handle> flush_handles_;

  RpcMessageProcessor message_processor_;

  mojo::AssociatedReceiver<media::mojom::RendererClient>
      renderer_client_receiver_;
  raw_ptr<media::mojom::Renderer> renderer_;

  openscreen::cast::RpcMessenger::Handle remote_handle_ =
      openscreen::cast::RpcMessenger::kInvalidHandle;
  openscreen::cast::RpcMessenger::Handle local_handle_ =
      openscreen::cast::RpcMessenger::kInvalidHandle;

  base::WeakPtrFactory<RendererRpcCallTranslator> weak_factory_;
};

}  // namespace cast_streaming::remoting

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_CONTROL_REMOTING_RENDERER_RPC_CALL_TRANSLATOR_H_
