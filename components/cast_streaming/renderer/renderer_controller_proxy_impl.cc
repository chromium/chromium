// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/renderer_controller_proxy_impl.h"

#include "base/bind.h"
#include "components/cast_streaming/renderer/public/renderer_controller_proxy_factory.h"

namespace cast_streaming {

// static
std::unique_ptr<RendererControllerProxy> CreateRendererControllerProxy() {
  return std::make_unique<RendererControllerProxyImpl>();
}

RendererControllerProxyImpl::RendererControllerProxyImpl() = default;

RendererControllerProxyImpl::~RendererControllerProxyImpl() = default;

base::RepeatingCallback<
    void(mojo::PendingAssociatedReceiver<mojom::RendererController>)>
RendererControllerProxyImpl::GetBinder(content::RenderFrame* frame) {
  // base::Unretained is safe here because this binder is used only for
  // RenderFrame / RenderFrameHost communication, and as this class is owned by
  // the ContentRendererClient which outlives any such instances.
  return base::BindRepeating(&RendererControllerProxyImpl::BindInterface,
                             base::Unretained(this), frame);
}

void RendererControllerProxyImpl::OnRenderFrameDestroyed(
    content::RenderFrame* frame) {
  DCHECK(frame);

  per_frame_proxies_.erase(frame);
}

mojo::PendingReceiver<media::mojom::Renderer>
RendererControllerProxyImpl::GetReceiver(content::RenderFrame* frame) {
  DCHECK(frame);

  auto it = GetFrameProxy(frame);
  return it->second->GetReceiver();
}

void RendererControllerProxyImpl::BindInterface(
    content::RenderFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::RendererController>
        pending_receiver) {
  DCHECK(frame);
  DCHECK(pending_receiver);

  auto it = GetFrameProxy(frame);
  it->second->BindReceiver(std::move(pending_receiver));
}

RendererControllerProxyImpl::RenderFrameMap::iterator
RendererControllerProxyImpl::GetFrameProxy(content::RenderFrame* frame) {
  auto it = per_frame_proxies_.find(frame);
  if (it != per_frame_proxies_.end()) {
    return it;
  }
  auto new_proxy = std::make_unique<FrameProxy>(
      base::BindRepeating(&RendererControllerProxyImpl::OnRenderFrameDestroyed,
                          base::Unretained(this), frame));
  return per_frame_proxies_.emplace_hint(it, frame, std::move(new_proxy));
}

RendererControllerProxyImpl::FrameProxy::FrameProxy(
    base::RepeatingCallback<void()> disconnection_handler)
    : renderer_process_pending_receiver_(
          renderer_process_remote_.InitWithNewPipeAndPassReceiver()),
      browser_process_receiver_(this),
      on_mojo_disconnection_(std::move(disconnection_handler)) {}

RendererControllerProxyImpl::FrameProxy::~FrameProxy() = default;

void RendererControllerProxyImpl::FrameProxy::BindReceiver(
    mojo::PendingAssociatedReceiver<mojom::RendererController>
        pending_receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  browser_process_receiver_.Bind(std::move(pending_receiver));
  browser_process_receiver_.set_disconnect_handler(on_mojo_disconnection_);
}

mojo::PendingReceiver<media::mojom::Renderer>
RendererControllerProxyImpl::FrameProxy::GetReceiver() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(renderer_process_pending_receiver_);

  if (set_playback_controller_cb_) {
    std::move(set_playback_controller_cb_).Run();
  }

  return std::move(renderer_process_pending_receiver_);
}

void RendererControllerProxyImpl::FrameProxy::SetPlaybackController(
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
