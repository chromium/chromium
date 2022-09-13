// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_PAGE_AGENT_H_
#define COMPONENTS_UI_DEVTOOLS_PAGE_AGENT_H_

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

  // Called on Ctrl+R (windows, linux) or Meta+R (mac) from frontend, but used
  // in UI Devtools to toggle the bounds debug rectangles for views. If called
  // using Ctrl+Shift+R (windows, linux) or Meta+Shift+R (mac), |bypass_cache|
  // will be true and will toggle bubble lock.
  protocol::Response reload(protocol::Maybe<bool> bypass_cache) override;

 protected:
  DOMAgent* const dom_agent_;
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_PAGE_AGENT_H_
