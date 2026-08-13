// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_drag_api/desktop_tab_drag_impl/tab_drag_session_input_adapter_aura.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_session_input_adapter.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"

namespace tabs_api {

AuraInputAdapter::AuraInputAdapter() = default;

AuraInputAdapter::~AuraInputAdapter() {
  ReleaseInputCapture();
}

base::expected<void, mojo_base::mojom::ErrorPtr>
AuraInputAdapter::StartInputCapture(EventCallback callback,
                                    TabDragWindowAdapter* initial_window) {
  callback_ = std::move(callback);
  active_window_ = initial_window;
  aura::Env::GetInstance()->AddPreTargetHandler(this);
  pre_target_handler_added_ = true;
  if (active_window_ && active_window_->GetNativeWindow()) {
    active_window_->GetNativeWindow()->SetCapture();
  }
  return base::ok();
}

void AuraInputAdapter::ReleaseInputCapture() {
  if (pre_target_handler_added_) {
    aura::Env::GetInstance()->RemovePreTargetHandler(this);
    pre_target_handler_added_ = false;
  }
  if (active_window_ && active_window_->GetNativeWindow() &&
      active_window_->GetNativeWindow()->HasCapture()) {
    active_window_->GetNativeWindow()->ReleaseCapture();
  }
  active_window_ = nullptr;
  callback_.Reset();
}

void AuraInputAdapter::SuspendInputCapture() {
  suspended_ = true;
  if (pre_target_handler_added_) {
    aura::Env::GetInstance()->RemovePreTargetHandler(this);
    pre_target_handler_added_ = false;
  }
  if (active_window_ && active_window_->GetNativeWindow() &&
      active_window_->GetNativeWindow()->HasCapture()) {
    active_window_->GetNativeWindow()->ReleaseCapture();
  }
}

void AuraInputAdapter::ResumeInputCapture() {
  suspended_ = false;
  if (!pre_target_handler_added_) {
    aura::Env::GetInstance()->AddPreTargetHandler(this);
    pre_target_handler_added_ = true;
  }
  if (active_window_ && active_window_->GetNativeWindow()) {
    active_window_->GetNativeWindow()->SetCapture();
  }
}

void AuraInputAdapter::SetActiveWindowContext(
    TabDragWindowAdapter* new_window) {
  if (active_window_ == new_window) {
    return;
  }
  base::AutoReset<bool> reset(&ignore_capture_events_, true);
  if (!suspended_ && active_window_ && active_window_->GetNativeWindow() &&
      active_window_->GetNativeWindow()->HasCapture()) {
    active_window_->GetNativeWindow()->ReleaseCapture();
  }
  active_window_ = new_window;
  if (!suspended_ && active_window_ && active_window_->GetNativeWindow()) {
    active_window_->GetNativeWindow()->SetCapture();
  }
}

void AuraInputAdapter::OnMouseEvent(ui::MouseEvent* event) {
  if (!callback_ || suspended_) {
    return;
  }
  const gfx::Point screen_point =
      display::Screen::Get()->GetCursorScreenPoint();

  if (event->type() == ui::EventType::kMouseMoved ||
      event->type() == ui::EventType::kMouseDragged) {
    callback_.Run({TabDragInputEvent::Type::kMoved, screen_point});
  } else if (event->type() == ui::EventType::kMouseReleased) {
    callback_.Run({TabDragInputEvent::Type::kDropped, screen_point});
  } else if (event->type() == ui::EventType::kMouseCaptureChanged) {
    if (!ignore_capture_events_) {
      aura::Window* target = static_cast<aura::Window*>(event->target());
      if (active_window_ && target == active_window_->GetNativeWindow()) {
        callback_.Run({TabDragInputEvent::Type::kCaptureChanged});
      }
    }
  }
}

void AuraInputAdapter::OnKeyEvent(ui::KeyEvent* event) {
  if (!callback_ || suspended_) {
    return;
  }
  if (event->type() == ui::EventType::kKeyPressed &&
      event->key_code() == ui::VKEY_ESCAPE) {
    callback_.Run({TabDragInputEvent::Type::kCancelled});
  }
}

}  // namespace tabs_api
