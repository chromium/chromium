// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_ADAPTERS_TAB_DRAG_SESSION_INPUT_ADAPTER_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_ADAPTERS_TAB_DRAG_SESSION_INPUT_ADAPTER_H_

#include <vector>

#include "components/browser_apis/tab_strip/types/node_id.h"

namespace tabs_api {

class TabDragSessionInputAdapter {
 public:
  virtual ~TabDragSessionInputAdapter() = default;

  // Starts capturing input on the platform.
  virtual void StartInputCapture(
      const std::vector<tabs_api::NodeId>& source_tab_ids) = 0;

  // Releases input capture.
  virtual void ReleaseInputCapture() = 0;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_ADAPTERS_TAB_DRAG_SESSION_INPUT_ADAPTER_H_
