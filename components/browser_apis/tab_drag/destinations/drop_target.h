// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_DESTINATIONS_DROP_TARGET_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_DESTINATIONS_DROP_TARGET_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_drag/destinations/drop_target_id.h"
#include "components/browser_apis/tab_drag/tab_drag_api.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"

namespace gfx {
class Point;
}

namespace tabs_api {

class DropTarget {
 public:
  DropTarget(DropTargetId id,
             TabDragWindowAdapter* window,
             gfx::NativeView native_view,
             mojo::PendingAssociatedRemote<mojom::DropTarget> remote);
  DropTarget(const DropTarget&) = delete;
  DropTarget& operator=(const DropTarget&) = delete;
  ~DropTarget();

  DropTargetId id() const { return id_; }
  TabDragWindowId window_id() const { return window_->GetWindowId(); }
  TabDragWindowAdapter* window() const { return window_.get(); }
  gfx::NativeView native_view() const { return native_view_; }

  gfx::Point ConvertScreenPointToLocal(const gfx::Point& screen_point) const;

  std::optional<gfx::Rect> cached_bounds() const { return cached_bounds_; }
  void set_cached_bounds(std::optional<gfx::Rect> bounds) {
    cached_bounds_ = bounds;
  }

  // Forward drag events to the underlying Mojo remote, performing coordinate
  // conversion internally using the window handle.
  void DragEnter(const std::vector<tabs_api::NodeId>& dragged_tabs,
                 const gfx::Point& screen_point,
                 int32_t tab_original_offset_x);
  void DragOver(const gfx::Point& screen_point);
  void DragLeave();
  void DragCancel();
  void Drop(const std::vector<tabs_api::NodeId>& dragged_tabs,
            const gfx::Point& screen_point);

 private:
  const DropTargetId id_;
  const raw_ptr<TabDragWindowAdapter> window_;
  const gfx::NativeView native_view_;
  std::optional<gfx::Rect> cached_bounds_;
  mojo::AssociatedRemote<mojom::DropTarget> remote_;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_DESTINATIONS_DROP_TARGET_H_
