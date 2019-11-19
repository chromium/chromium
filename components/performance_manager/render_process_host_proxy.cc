// Copyright 2019 The Chromium Authors. All rights reserved.
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
  return content::RenderProcessHost::FromID(render_process_host_id_);
}

RenderProcessHostProxy::RenderProcessHostProxy(int render_process_host_id)
    : render_process_host_id_(render_process_host_id) {
  DCHECK(render_process_host_id_ >= 0);
}

}  // namespace performance_manager
