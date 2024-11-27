// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/overlay_agent.h"

namespace ui_devtools {

OverlayAgent::OverlayAgent(DOMAgent* dom_agent) : dom_agent_(dom_agent) {
  DCHECK(dom_agent_);
}

OverlayAgent::~OverlayAgent() = default;

protocol::Response OverlayAgent::setInspectMode(
    const protocol::String& in_mode,
    std::unique_ptr<protocol::Overlay::HighlightConfig> in_highlightConfig) {
  NOTREACHED();
}

protocol::Response OverlayAgent::highlightNode(
    std::unique_ptr<protocol::Overlay::HighlightConfig> highlight_config,
    std::optional<int> node_id) {
  NOTREACHED();
}

protocol::Response OverlayAgent::hideHighlight() {
  return protocol::Response::Success();
}

}  // namespace ui_devtools
