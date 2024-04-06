// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_OVERLAY_AGENT_H_
#define COMPONENTS_UI_DEVTOOLS_OVERLAY_AGENT_H_

#include "base/memory/raw_ptr.h"
#include "components/ui_devtools/dom_agent.h"
#include "components/ui_devtools/overlay.h"

namespace ui_devtools {

class UI_DEVTOOLS_EXPORT OverlayAgent
    : public UiDevToolsBaseAgent<protocol::Overlay::Metainfo> {
 public:
  explicit OverlayAgent(DOMAgent* dom_agent);

  OverlayAgent(const OverlayAgent&) = delete;
  OverlayAgent& operator=(const OverlayAgent&) = delete;

  ~OverlayAgent() override;

  // Overlay::Backend:
  protocol::Response setInspectMode(
      const protocol::String& in_mode,
      protocol::Maybe<protocol::Overlay::HighlightConfig> in_highlightConfig)
      override;
  protocol::Response highlightNode(
      std::unique_ptr<protocol::Overlay::HighlightConfig> highlight_config,
      protocol::Maybe<int> node_id) override;
  protocol::Response hideHighlight() override;

 protected:
  DOMAgent* dom_agent() const { return dom_agent_; }

 private:
  const raw_ptr<DOMAgent> dom_agent_;
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_OVERLAY_AGENT_H_
