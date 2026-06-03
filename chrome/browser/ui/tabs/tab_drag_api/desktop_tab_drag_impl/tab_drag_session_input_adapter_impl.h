// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_SESSION_INPUT_ADAPTER_IMPL_H_
#define CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_SESSION_INPUT_ADAPTER_IMPL_H_

#include <memory>

#include "components/browser_apis/tab_drag/adapters/tab_drag_session_input_adapter.h"
#include "ui/events/event_observer.h"

namespace views {
class EventMonitor;
}

namespace tabs_api {

class TabDragSessionInputAdapterImpl : public TabDragSessionInputAdapter,
                                       public ui::EventObserver {
 public:
  TabDragSessionInputAdapterImpl();
  TabDragSessionInputAdapterImpl(const TabDragSessionInputAdapterImpl&) =
      delete;
  TabDragSessionInputAdapterImpl& operator=(
      const TabDragSessionInputAdapterImpl&) = delete;
  ~TabDragSessionInputAdapterImpl() override;

  // TabDragSessionInputAdapter overrides:
  base::expected<void, mojo_base::mojom::ErrorPtr> StartInputCapture(
      const std::vector<tabs_api::NodeId>& source_tab_ids,
      EventCallback callback) override;
  void ReleaseInputCapture() override;

  // ui::EventObserver overrides:
  void OnEvent(const ui::Event& event) override;

 private:
  std::unique_ptr<views::EventMonitor> event_monitor_;
  EventCallback callback_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_SESSION_INPUT_ADAPTER_IMPL_H_
