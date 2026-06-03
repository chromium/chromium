// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/testing/toy_tab_drag_session_input_adapter.h"

#include <utility>

namespace tabs_api {

ToyTabDragSessionInputAdapter::ToyTabDragSessionInputAdapter() = default;

ToyTabDragSessionInputAdapter::~ToyTabDragSessionInputAdapter() = default;

base::expected<void, mojo_base::mojom::ErrorPtr>
ToyTabDragSessionInputAdapter::StartInputCapture(
    const std::vector<tabs_api::NodeId>& source_tab_ids,
    EventCallback callback) {
  capture_started_ = true;
  callback_ = std::move(callback);
  return base::ok();
}

void ToyTabDragSessionInputAdapter::ReleaseInputCapture() {
  capture_released_ = true;
  callback_.Reset();
}

void ToyTabDragSessionInputAdapter::SendToyEvent(
    TabDragInputEvent::Type type,
    const gfx::Point& screen_point) {
  if (callback_) {
    callback_.Run({type, screen_point});
  }
}

}  // namespace tabs_api
