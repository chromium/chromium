// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/element.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

DomObjectFrameStack::DomObjectFrameStack() = default;

DomObjectFrameStack::~DomObjectFrameStack() = default;

DomObjectFrameStack::DomObjectFrameStack(const DomObjectFrameStack&) = default;

GlobalBackendNodeId::GlobalBackendNodeId(
    content::RenderFrameHost* render_frame_host,
    int backend_node_id)
    : backend_node_id_(backend_node_id) {
  if (render_frame_host) {
    host_id_ = render_frame_host->GetGlobalId();
  }
}

GlobalBackendNodeId::GlobalBackendNodeId(
    content::GlobalRenderFrameHostId host_id,
    int backend_node_id)
    : host_id_(host_id), backend_node_id_(backend_node_id) {}

GlobalBackendNodeId::~GlobalBackendNodeId() = default;

GlobalBackendNodeId::GlobalBackendNodeId(const GlobalBackendNodeId&) = default;

bool GlobalBackendNodeId::operator==(const GlobalBackendNodeId& other) const {
  return host_id_ == other.host_id_ &&
         backend_node_id_ == other.backend_node_id_;
}

content::GlobalRenderFrameHostId GlobalBackendNodeId::host_id() const {
  return host_id_;
}

int GlobalBackendNodeId::backend_node_id() const {
  return backend_node_id_;
}

content::RenderFrameHost* FindCorrespondingRenderFrameHost(
    const std::string& frame_id,
    content::WebContents* web_contents) {
  if (frame_id.empty()) {
    return web_contents->GetPrimaryMainFrame();
  }
  content::RenderFrameHost* result = nullptr;
  web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHostWithAction(
      [&frame_id, &result](content::RenderFrameHost* render_frame_host) {
        if (render_frame_host->GetDevToolsFrameToken().ToString() == frame_id) {
          result = render_frame_host;
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });
  return result;
}

}  // namespace autofill_assistant
