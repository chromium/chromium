// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/render_frame_host_proxy.h"

#include "content/public/browser/render_frame_host.h"

namespace performance_manager {

RenderFrameHostProxy::RenderFrameHostProxy() = default;
RenderFrameHostProxy::~RenderFrameHostProxy() = default;
RenderFrameHostProxy::RenderFrameHostProxy(const RenderFrameHostProxy& other) =
    default;
RenderFrameHostProxy& RenderFrameHostProxy::operator=(
    const RenderFrameHostProxy& other) = default;

content::RenderFrameHost* RenderFrameHostProxy::Get() const {
  return content::RenderFrameHost::FromID(global_frame_routing_id_);
}

RenderFrameHostProxy::RenderFrameHostProxy(
    const content::GlobalRenderFrameHostId& global_frame_routing_id)
    : global_frame_routing_id_(global_frame_routing_id) {}

}  // namespace performance_manager
