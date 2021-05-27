// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/cast_streaming_render_frame_observer.h"

#include "base/bind.h"
#include "components/cast_streaming/renderer/cast_streaming_receiver.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace cast_streaming {
namespace {

void BindInterface(
    content::RenderFrame* render_frame,
    cast_streaming::CastStreamingReceiver::InterfaceRegistryBinderCallback cb) {
  DCHECK(render_frame);
  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(std::move(cb));
}

}  // namespace

CastStreamingRenderFrameObserver::CastStreamingRenderFrameObserver(
    content::RenderFrame* render_frame,
    base::OnceCallback<void(int)> on_render_frame_deleted_callback)
    : content::RenderFrameObserver(render_frame),
      cast_streaming_receiver_(
          base::BindOnce(&BindInterface, base::Unretained(render_frame))),
      on_render_frame_deleted_callback_(
          std::move(on_render_frame_deleted_callback)) {
  DCHECK(render_frame);
  DCHECK(on_render_frame_deleted_callback_);
}

CastStreamingRenderFrameObserver::~CastStreamingRenderFrameObserver() = default;

void CastStreamingRenderFrameObserver::OnDestruct() {
  std::move(on_render_frame_deleted_callback_).Run(routing_id());
}

}  // namespace cast_streaming
