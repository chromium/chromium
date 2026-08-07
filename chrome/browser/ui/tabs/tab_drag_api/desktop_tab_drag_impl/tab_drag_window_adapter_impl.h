// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_WINDOW_ADAPTER_IMPL_H_
#define CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_WINDOW_ADAPTER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "mojo/public/mojom/base/error.mojom-forward.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/widget/widget_observer.h"

class BrowserWindowInterface;

namespace tabs_api {
class TabDragWindowRegistry;
}

class TabDragWindowAdapterImpl : public tabs_api::TabDragWindowAdapter,
                                 public views::WidgetObserver {
 public:
  explicit TabDragWindowAdapterImpl(BrowserWindowInterface* browser_window);
  TabDragWindowAdapterImpl(const TabDragWindowAdapterImpl&) = delete;
  TabDragWindowAdapterImpl& operator=(const TabDragWindowAdapterImpl&) = delete;
  ~TabDragWindowAdapterImpl() override;

  // tabs_api::TabDragWindowAdapter:
  tabs_api::TabDragWindowId GetWindowId() const override;
  gfx::NativeWindow GetNativeWindow() const override;
  gfx::Rect GetBoundsInScreen() const override;
  bool IsDraggingEntireWindow(size_t dragged_tab_count) const override;
  gfx::Point ConvertScreenPointToLocal(
      gfx::NativeView target_view,
      const gfx::Point& screen_point) const override;

  void SetCapture() override;
  void ReleaseCapture() override;
  bool HasCapture() const override;
  void Activate() override;

  base::expected<tabs_api::TabDragWindowId, mojo_base::mojom::ErrorPtr>
  DetachToNewWindow(const std::vector<tabs_api::NodeId>& tab_ids,
                    const gfx::Point& screen_point,
                    const gfx::Vector2d& drag_offset) override;

  tabs_api::DragMoveLoopResult RunWindowMoveLoop(
      const gfx::Point& screen_point,
      const gfx::Vector2d& drag_offset,
      tabs_api::TabDragWindowAdapter::WindowMoveCallback move_callback)
      override;

  void EndWindowMoveLoop() override;

  base::expected<void, mojo_base::mojom::ErrorPtr> MigrateTabs(
      tabs_api::TabDragWindowId target_window_id,
      const std::vector<tabs_api::NodeId>& tab_ids) override;

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

 private:
  raw_ptr<BrowserWindowInterface> browser_window_;
  raw_ptr<tabs_api::TabDragWindowRegistry> registry_;
  tabs_api::TabDragWindowId id_;
  tabs_api::TabDragWindowAdapter::WindowMoveCallback move_callback_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_WINDOW_ADAPTER_IMPL_H_
