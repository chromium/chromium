// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/overlay_agent.h"

namespace ui_devtools {

OverlayAgent::OverlayAgent(DOMAgent* dom_agent) : dom_agent_(dom_agent) {
  DCHECK(dom_agent_);
}

OverlayAgent::~OverlayAgent() {}

protocol::Response OverlayAgent::setInspectMode(
    const protocol::String& in_mode,
    protocol::Maybe<protocol::Overlay::HighlightConfig> in_highlightConfig) {
  NOTREACHED();
  return protocol::Response::Success();
}

protocol::Response OverlayAgent::highlightNode(
    std::unique_ptr<protocol::Overlay::HighlightConfig> highlight_config,
    protocol::Maybe<int> node_id) {
  NOTREACHED();
  return protocol::Response::Success();
}

protocol::Response OverlayAgent::hideHighlight() {
  return protocol::Response::Success();
}

}  // namespace ui_devtools
