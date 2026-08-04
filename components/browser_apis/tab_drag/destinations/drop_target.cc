// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/destinations/drop_target.h"

#include <utility>

#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "ui/gfx/geometry/point.h"

namespace tabs_api {

DropTarget::DropTarget(DropTargetId id,
                       TabDragWindowAdapter* window,
                       gfx::NativeView native_view,
                       mojo::PendingAssociatedRemote<mojom::DropTarget> remote)
    : id_(id),
      window_(window),
      native_view_(native_view),
      remote_(std::move(remote)) {
  CHECK(window_);
}

DropTarget::~DropTarget() = default;

gfx::Point DropTarget::ConvertScreenPointToLocal(
    const gfx::Point& screen_point) const {
  return window_->ConvertScreenPointToLocal(native_view_, screen_point);
}

void DropTarget::DragEnter(const std::vector<tabs_api::NodeId>& dragged_tabs,
                           const gfx::Point& screen_point,
                           int32_t tab_original_offset_x) {
  remote_->OnDragEntered(
      dragged_tabs,
      window_->ConvertScreenPointToLocal(native_view_, screen_point),
      tab_original_offset_x);
}

void DropTarget::DragOver(const gfx::Point& screen_point) {
  remote_->OnDrag(
      window_->ConvertScreenPointToLocal(native_view_, screen_point));
}

void DropTarget::DragLeave() {
  remote_->OnDragLeave();
}

void DropTarget::DragCancel() {
  remote_->OnDragCancelled();
}

void DropTarget::Drop(const std::vector<tabs_api::NodeId>& dragged_tabs,
                      const gfx::Point& screen_point) {
  remote_->OnDrop(dragged_tabs, window_->ConvertScreenPointToLocal(
                                    native_view_, screen_point));
}

}  // namespace tabs_api
