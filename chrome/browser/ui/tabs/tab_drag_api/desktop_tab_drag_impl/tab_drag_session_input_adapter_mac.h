// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_SESSION_INPUT_ADAPTER_MAC_H_
#define CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_SESSION_INPUT_ADAPTER_MAC_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_session_input_adapter.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "ui/events/event_observer.h"

// Forward declarations for ObjC classes used in the delegate interface.
#if defined(__OBJC__)
@class NSEvent;
@class NSWindow;
#else
class NSEvent;
class NSWindow;
#endif

namespace views {
class EventMonitor;
}

namespace remote_cocoa {
class CocoaMouseCapture;
}

namespace tabs_api {

class TabDragWindowAdapter;

class MacInputAdapter : public TabDragSessionInputAdapter,
                        public ui::EventObserver {
 public:
  MacInputAdapter();
  ~MacInputAdapter() override;

  // TabDragSessionInputAdapter overrides:
  base::expected<void, mojo_base::mojom::ErrorPtr> StartInputCapture(
      EventCallback callback,
      TabDragWindowAdapter* initial_window) override;
  void ReleaseInputCapture() override;
  void SuspendInputCapture() override;
  void ResumeInputCapture() override;
  void SetActiveWindowContext(TabDragWindowAdapter* new_window) override;

  // ui::EventObserver overrides:
  void OnEvent(const ui::Event& event) override;

  // Called by CocoaMouseCaptureDelegateImpl helper:
  bool PostCapturedEvent(NSEvent* event);
  void OnMouseCaptureLost();
  NSWindow* GetWindow() const;

 private:
  class CocoaMouseCaptureDelegateImpl;

  EventCallback callback_;
  raw_ptr<TabDragWindowAdapter> active_window_ = nullptr;
  bool suspended_ = false;
  std::unique_ptr<CocoaMouseCaptureDelegateImpl> capture_delegate_;
  std::unique_ptr<remote_cocoa::CocoaMouseCapture> capture_;
  std::unique_ptr<views::EventMonitor> event_monitor_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_SESSION_INPUT_ADAPTER_MAC_H_
