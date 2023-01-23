// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/render_process_host_proxy.h"

#include "content/public/browser/render_process_host.h"

namespace performance_manager {

RenderProcessHostProxy::RenderProcessHostProxy() = default;
RenderProcessHostProxy::~RenderProcessHostProxy() = default;
RenderProcessHostProxy::RenderProcessHostProxy(
    const RenderProcessHostProxy& other) = default;
RenderProcessHostProxy& RenderProcessHostProxy::operator=(
    const RenderProcessHostProxy& other) = default;

content::RenderProcessHost* RenderProcessHostProxy::Get() const {
  return content::RenderProcessHost::FromID(render_process_host_id_.value());
}

RenderProcessHostProxy::RenderProcessHostProxy(
    RenderProcessHostId render_process_host_id)
    : render_process_host_id_(render_process_host_id) {
  DCHECK_GE(render_process_host_id_.value(), 0);
}

// static
RenderProcessHostProxy RenderProcessHostProxy::CreateForTesting(
    RenderProcessHostId render_process_host_id) {
  return RenderProcessHostProxy(render_process_host_id);
}

}  // namespace performance_manager
