// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_SESSION_INPUT_ADAPTER_AURA_H_
#define CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_SESSION_INPUT_ADAPTER_AURA_H_

#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_session_input_adapter.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "ui/events/event_handler.h"

namespace tabs_api {

class TabDragWindowAdapter;

class AuraInputAdapter : public TabDragSessionInputAdapter,
                         public ui::EventHandler {
 public:
  AuraInputAdapter();
  ~AuraInputAdapter() override;

  // TabDragSessionInputAdapter overrides:
  base::expected<void, mojo_base::mojom::ErrorPtr> StartInputCapture(
      EventCallback callback,
      TabDragWindowAdapter* initial_window) override;
  void ReleaseInputCapture() override;
  void SuspendInputCapture() override;
  void ResumeInputCapture() override;
  void SetActiveWindowContext(TabDragWindowAdapter* new_window) override;

  // ui::EventHandler overrides:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;

 private:
  EventCallback callback_;
  raw_ptr<TabDragWindowAdapter> active_window_ = nullptr;
  bool suspended_ = false;
  bool pre_target_handler_added_ = false;
  bool ignore_capture_events_ = false;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_SESSION_INPUT_ADAPTER_AURA_H_
