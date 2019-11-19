// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/page_agent.h"

namespace ui_devtools {

PageAgent::PageAgent(DOMAgent* dom_agent) : dom_agent_(dom_agent) {}

PageAgent::~PageAgent() {}

protocol::Response PageAgent::reload(protocol::Maybe<bool> bypass_cache) {
  NOTREACHED();
  return protocol::Response::OK();
}

}  // namespace ui_devtools
