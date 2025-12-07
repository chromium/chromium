// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_PAGE_AGENT_H_
#define COMPONENTS_UI_DEVTOOLS_PAGE_AGENT_H_

#include "base/memory/raw_ptr.h"
#include "components/ui_devtools/dom_agent.h"
#include "components/ui_devtools/page.h"

namespace ui_devtools {

class UI_DEVTOOLS_EXPORT PageAgent
    : public UiDevToolsBaseAgent<protocol::Page::Metainfo> {
 public:
  explicit PageAgent(DOMAgent* dom_agent);

  PageAgent(const PageAgent&) = delete;
  PageAgent& operator=(const PageAgent&) = delete;

  ~PageAgent() override;

 protected:
  const raw_ptr<DOMAgent, DanglingUntriaged> dom_agent_;
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_PAGE_AGENT_H_
