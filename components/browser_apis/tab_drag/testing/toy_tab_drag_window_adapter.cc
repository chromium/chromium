// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/testing/toy_tab_drag_window_adapter.h"

#include "components/browser_apis/tab_drag/sessions/tab_drag_window_registry.h"

namespace tabs_api {

ToyTabDragWindowAdapter::ToyTabDragWindowAdapter(
    const gfx::Rect& bounds,
    TabDragWindowRegistry* registry)
    : bounds_(bounds), registry_(registry) {
  if (registry_) {
    id_ = registry_->Register(this);
  }
}

ToyTabDragWindowAdapter::~ToyTabDragWindowAdapter() {
  if (registry_ && id_) {
    registry_->Unregister(id_);
  }
}

gfx::Rect ToyTabDragWindowAdapter::GetBoundsInScreen() const {
  return bounds_;
}

bool ToyTabDragWindowAdapter::ShouldDragWholeWindow(
    size_t dragged_tab_count) const {
  return dragged_tab_count == tab_count_;
}

gfx::Point ToyTabDragWindowAdapter::ConvertScreenPointToLocal(
    gfx::NativeView target_view,
    const gfx::Point& screen_point) const {
  return screen_point - bounds_.OffsetFromOrigin();
}

void ToyTabDragWindowAdapter::SetCapture() {
  has_capture_ = true;
}

void ToyTabDragWindowAdapter::ReleaseCapture() {
  has_capture_ = false;
}

bool ToyTabDragWindowAdapter::HasCapture() const {
  return has_capture_;
}

}  // namespace tabs_api
