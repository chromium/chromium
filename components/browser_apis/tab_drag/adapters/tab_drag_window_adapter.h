// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_ADAPTERS_TAB_DRAG_WINDOW_ADAPTER_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_ADAPTERS_TAB_DRAG_WINDOW_ADAPTER_H_

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "base/types/id_type.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "mojo/public/mojom/base/error.mojom-forward.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/native_ui_types.h"

namespace tabs_api {

using TabDragWindowId = base::IdType64<class TabDragWindowTag>;

enum class DragMoveLoopResult {
  kSuccess,
  kCanceled,
};

// Represents a browser window for the TabDragAPI.
class TabDragWindowAdapter {
 public:
  using WindowMoveCallback =
      base::RepeatingCallback<void(const gfx::Point& cursor_screen_point)>;

  virtual ~TabDragWindowAdapter() = default;

  virtual TabDragWindowId GetWindowId() const = 0;

  // Returns the native window handle.
  virtual gfx::NativeWindow GetNativeWindow() const = 0;

  // Returns the window bounds in screen coordinates.
  virtual gfx::Rect GetBoundsInScreen() const = 0;

  // Returns true if the given dragged tab count represents the entire window.
  virtual bool IsDraggingEntireWindow(size_t dragged_tab_count) const = 0;

  // Converts a point in screen coordinates to local coordinates relative to the
  // given `target_view`.
  virtual gfx::Point ConvertScreenPointToLocal(
      gfx::NativeView target_view,
      const gfx::Point& screen_point) const = 0;

  // Acquires input capture for this window.
  virtual void SetCapture() = 0;

  // Releases input capture from this window.
  virtual void ReleaseCapture() = 0;

  // Returns true if this window has capture.
  virtual bool HasCapture() const = 0;

  // Activates and brings the browser window to the front.
  virtual void Activate() = 0;

  // Detaches the given tabs from this window and inserts them into a newly
  // created window. Returns the ID of the new window.
  virtual base::expected<TabDragWindowId, mojo_base::mojom::ErrorPtr>
  DetachToNewWindow(const std::vector<tabs_api::NodeId>& tab_ids,
                    const gfx::Point& screen_point,
                    const gfx::Vector2d& drag_offset) = 0;

  // Runs the native window drag-move loop for this window.
  virtual DragMoveLoopResult RunWindowMoveLoop(
      const gfx::Point& screen_point,
      const gfx::Vector2d& drag_offset,
      WindowMoveCallback move_callback) = 0;

  // Signals the native window manager to end the blocking window move loop.
  virtual void EndWindowMoveLoop() = 0;

  // Migrates the given tabs from this window to the target window.
  virtual base::expected<void, mojo_base::mojom::ErrorPtr> MigrateTabs(
      TabDragWindowId target_window_id,
      const std::vector<tabs_api::NodeId>& tab_ids) = 0;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_ADAPTERS_TAB_DRAG_WINDOW_ADAPTER_H_
