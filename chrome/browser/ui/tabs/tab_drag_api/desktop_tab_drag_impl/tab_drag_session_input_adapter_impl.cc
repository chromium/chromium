// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_drag_api/desktop_tab_drag_impl/tab_drag_session_input_adapter_impl.h"

#include "mojo/public/mojom/base/error.mojom.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/event_monitor.h"

namespace tabs_api {

TabDragSessionInputAdapterImpl::TabDragSessionInputAdapterImpl() = default;
TabDragSessionInputAdapterImpl::~TabDragSessionInputAdapterImpl() = default;

base::expected<void, mojo_base::mojom::ErrorPtr>
TabDragSessionInputAdapterImpl::StartInputCapture(
    EventCallback callback) {
  callback_ = std::move(callback);
  event_monitor_ = views::EventMonitor::CreateApplicationMonitor(
      this, gfx::NativeWindow(),
      {ui::EventType::kMouseMoved, ui::EventType::kMouseDragged,
       ui::EventType::kMouseReleased, ui::EventType::kKeyPressed,
       ui::EventType::kMouseCaptureChanged});
  return base::ok();
}

void TabDragSessionInputAdapterImpl::ReleaseInputCapture() {
  event_monitor_.reset();
  callback_.Reset();
}

void TabDragSessionInputAdapterImpl::OnEvent(const ui::Event& event) {
  if (!callback_) {
    return;
  }
  gfx::Point screen_point = display::Screen::Get()->GetCursorScreenPoint();
  switch (event.type()) {
    case ui::EventType::kKeyPressed:
      if (event.AsKeyEvent()->key_code() == ui::VKEY_ESCAPE) {
        callback_.Run({TabDragInputEvent::Type::kCancelled});
      }
      break;
    case ui::EventType::kMouseReleased:
      callback_.Run({TabDragInputEvent::Type::kDropped, screen_point});
      break;
    case ui::EventType::kMouseMoved:
    case ui::EventType::kMouseDragged:
      callback_.Run({TabDragInputEvent::Type::kMoved, screen_point});
      break;
    case ui::EventType::kMouseCaptureChanged:
      callback_.Run({TabDragInputEvent::Type::kCaptureChanged});
      break;
    default:
      break;
  }
}

}  // namespace tabs_api
