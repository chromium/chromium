// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/sessions/tab_drag_session.h"

#include "base/functional/bind.h"

namespace tabs_api {

TabDragSession::TabDragSession(
    const std::vector<tabs_api::NodeId>& source_tab_ids,
    const gfx::Point& start_point,
    TabDragSessionInputAdapter& input_adapter,
    TabDragSessionInputListener* listener,
    base::OnceClosure end_callback)
    : dragged_tabs_(source_tab_ids),
      input_adapter_(input_adapter),
      listener_(listener),
      end_callback_(std::move(end_callback)),
      start_point_in_screen_(start_point),
      last_mouse_screen_point_(start_point) {}

base::expected<void, mojo_base::mojom::ErrorPtr> TabDragSession::Start() {
  auto result = input_adapter_->StartInputCapture(
      dragged_tabs_, base::BindRepeating(&TabDragSession::OnInputEvent,
                                         base::Unretained(this)));
  if (result.has_value() && listener_) {
    listener_->OnSessionStarted(this);
  }
  return result;
}

TabDragSession::~TabDragSession() {
  input_adapter_->ReleaseInputCapture();
}

void TabDragSession::Cancel() {
  EndSession();
}

void TabDragSession::EndSession() {
  if (listener_) {
    listener_->OnSessionEnded();
  }
  if (end_callback_) {
    std::move(end_callback_).Run();
  }
}

void TabDragSession::OnInputEvent(const TabDragInputEvent& event) {
  TabDragSessionInputEvent::Type event_type;
  switch (event.type) {
    case TabDragInputEvent::Type::kCancelled:
      event_type = TabDragSessionInputEvent::Type::kCancelled;
      Cancel();
      break;
    case TabDragInputEvent::Type::kDropped:
      event_type = TabDragSessionInputEvent::Type::kDropped;
      last_mouse_screen_point_ = event.screen_point;
      delta_ = event.screen_point - start_point_in_screen_;
      EndSession();
      break;
    case TabDragInputEvent::Type::kMoved:
      event_type = TabDragSessionInputEvent::Type::kMoved;
      last_mouse_screen_point_ = event.screen_point;
      delta_ = event.screen_point - start_point_in_screen_;
      break;
  }

  TabDragSessionInputEvent session_event{.type = event_type,
                                         .screen_point = event.screen_point};
  if (listener_) {
    listener_->OnDragSessionEvent(session_event);
  }
}

}  // namespace tabs_api
