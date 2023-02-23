// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/control/renderer_controller_proxy.h"

#include <utility>

#include "base/logging.h"

namespace cast_streaming {

RendererControllerProxy::RendererControllerProxy(
    MojoDisconnectCB disconnection_handler)
    : renderer_process_pending_receiver_(
          renderer_process_remote_.InitWithNewPipeAndPassReceiver()),
      browser_process_receiver_(this),
      on_mojo_disconnection_(std::move(disconnection_handler)) {}

RendererControllerProxy::~RendererControllerProxy() = default;

void RendererControllerProxy::BindRendererController(
    mojo::PendingAssociatedReceiver<mojom::RendererController>
        pending_receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(on_mojo_disconnection_);
  browser_process_receiver_.Bind(std::move(pending_receiver));
  browser_process_receiver_.set_disconnect_handler(
      std::move(on_mojo_disconnection_));
}

mojo::PendingReceiver<media::mojom::Renderer>
RendererControllerProxy::GetReceiver() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(renderer_process_pending_receiver_);

  if (set_playback_controller_cb_) {
    std::move(set_playback_controller_cb_).Run();
  }

  return std::move(renderer_process_pending_receiver_);
}

void RendererControllerProxy::SetPlaybackController(
    mojo::PendingReceiver<media::mojom::Renderer>
        browser_process_renderer_controls,
    SetPlaybackControllerCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!set_playback_controller_cb_);
  set_playback_controller_cb_ = std::move(callback);

  CHECK(mojo::FusePipes(std::move(browser_process_renderer_controls),
                        std::move(renderer_process_remote_)));
  if (!renderer_process_pending_receiver_) {
    std::move(set_playback_controller_cb_).Run();
  }
}

}  // namespace cast_streaming
