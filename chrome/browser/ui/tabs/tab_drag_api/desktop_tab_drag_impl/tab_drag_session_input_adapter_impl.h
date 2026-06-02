// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_SESSION_INPUT_ADAPTER_IMPL_H_
#define CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_SESSION_INPUT_ADAPTER_IMPL_H_

#include "components/browser_apis/tab_drag/adapters/tab_drag_session_input_adapter.h"

namespace tabs_api {

class TabDragSessionInputAdapterImpl : public TabDragSessionInputAdapter {
 public:
  TabDragSessionInputAdapterImpl();
  TabDragSessionInputAdapterImpl(const TabDragSessionInputAdapterImpl&) =
      delete;
  TabDragSessionInputAdapterImpl& operator=(
      const TabDragSessionInputAdapterImpl&) = delete;
  ~TabDragSessionInputAdapterImpl() override;

  // TabDragSessionInputAdapter overrides:
  void StartInputCapture(
      const std::vector<tabs_api::NodeId>& source_tab_ids) override;
  void ReleaseInputCapture() override;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_SESSION_INPUT_ADAPTER_IMPL_H_
