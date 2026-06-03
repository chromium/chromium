// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/sessions/tab_drag_session.h"

#include "base/functional/bind.h"

namespace tabs_api {

TabDragSession::TabDragSession(
    const std::vector<tabs_api::NodeId>& source_tab_ids,
    TabDragSessionInputAdapter& input_adapter,
    base::OnceClosure end_callback)
    : dragged_tabs_(source_tab_ids),
      input_adapter_(input_adapter),
      end_callback_(std::move(end_callback)) {}

base::expected<void, mojo_base::mojom::ErrorPtr> TabDragSession::Start() {
  return input_adapter_->StartInputCapture(
      dragged_tabs_, base::BindRepeating(&TabDragSession::OnInputEvent,
                                         base::Unretained(this)));
}

TabDragSession::~TabDragSession() {
  input_adapter_->ReleaseInputCapture();
}

void TabDragSession::Cancel() {
  EndSession();
}

void TabDragSession::EndSession() {
  if (end_callback_) {
    std::move(end_callback_).Run();
  }
}

void TabDragSession::OnInputEvent(const TabDragInputEvent& event) {
  switch (event.type) {
    case TabDragInputEvent::Type::kCancelled:
      Cancel();
      break;
    case TabDragInputEvent::Type::kDropped:
      EndSession();
      break;
    case TabDragInputEvent::Type::kMoved:
      break;
  }
}

}  // namespace tabs_api
