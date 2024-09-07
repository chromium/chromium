// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_PAGE_AGENT_VIEWS_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_PAGE_AGENT_VIEWS_H_

#include "components/ui_devtools/page_agent.h"

namespace ui_devtools {

class PageAgentViews : public PageAgent {
 public:
  explicit PageAgentViews(DOMAgent* dom_agent);
  PageAgentViews(const PageAgentViews&) = delete;
  PageAgentViews& operator=(const PageAgentViews&) = delete;
  ~PageAgentViews() override;

  // PageAgent:
  protocol::Response disable() override;
  protocol::Response getResourceTree(
      std::unique_ptr<protocol::Page::FrameResourceTree>* frame_tree) override;
  protocol::Response getResourceContent(const protocol::String& in_frameId,
                                        const protocol::String& in_url,
                                        protocol::String* out_content,
                                        bool* out_base64Encoded) override;
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_PAGE_AGENT_VIEWS_H_
