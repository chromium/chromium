// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_RENDERER_CONTROL_RENDERER_CONTROLLER_PROXY_H_
#define COMPONENTS_CAST_STREAMING_RENDERER_CONTROL_RENDERER_CONTROLLER_PROXY_H_

#include "base/functional/callback.h"
#include "base/threading/thread_checker.h"
#include "components/cast_streaming/common/public/mojom/renderer_controller.mojom.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace cast_streaming {

// This class serves the purpose of allowing both the browser and renderer
// processes to connect media::mojom::Renderer receiver and remote endpoints
// without concerns of creation order or race conditions becoming concerns.
class RendererControllerProxy : public mojom::RendererController {
 public:
  using MojoDisconnectCB = base::OnceCallback<void()>;

  explicit RendererControllerProxy(MojoDisconnectCB disconnection_handler);
  ~RendererControllerProxy() override;

  // Binds the frame-specific receiver from the browser process.
  void BindRendererController(
      mojo::PendingAssociatedReceiver<mojom::RendererController>
          pending_receiver);

  // Gets the receiver associated with the RenderFrame associated with this
  // instance.
  mojo::PendingReceiver<media::mojom::Renderer> GetReceiver();

 private:
  // mojom::RendererController overrides.
  //
  // Fuses |browser_process_renderer_controls| received from the browser
  // process with  |renderer_process_remote_|, so that commands sent by the
  // browser process are passed immediately to
  // |renderer_process_pending_receiver_|.
  void SetPlaybackController(mojo::PendingReceiver<media::mojom::Renderer>
                                 browser_process_renderer_controls,
                             SetPlaybackControllerCallback callback) override;

  // Callback from the SetPlaybackController() method. Returned when the
  // receiver is taken by the Renderer which uses it.
  SetPlaybackControllerCallback set_playback_controller_cb_;

  // This handle is bound to the |renderer_process_pending_receiver_| upon
  // class construction. When SetPlaybackController() is called, ownership of
  // this instance will be transferred away from this class.
  mojo::PendingRemote<media::mojom::Renderer> renderer_process_remote_;

  // Owned by this class from time of creation until the first call to
  // GetReceiver(), at which time its ownership is passed to the caller.
  mojo::PendingReceiver<media::mojom::Renderer>
      renderer_process_pending_receiver_;

  // Receiver passed from the browser process to the renderer process.
  mojo::AssociatedReceiver<mojom::RendererController> browser_process_receiver_;

  MojoDisconnectCB on_mojo_disconnection_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_RENDERER_CONTROL_RENDERER_CONTROLLER_PROXY_H_
