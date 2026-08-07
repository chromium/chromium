// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_TAB_DRAG_WINDOW_ADAPTER_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_TAB_DRAG_WINDOW_ADAPTER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace tabs_api {

class TabDragWindowRegistry;

class ToyTabDragWindowAdapter : public TabDragWindowAdapter {
 public:
  ToyTabDragWindowAdapter(const gfx::Rect& bounds,
                          TabDragWindowRegistry* registry);
  ToyTabDragWindowAdapter(const ToyTabDragWindowAdapter&) = delete;
  ToyTabDragWindowAdapter& operator=(const ToyTabDragWindowAdapter&) = delete;
  ~ToyTabDragWindowAdapter() override;

  // TabDragWindowAdapter:
  TabDragWindowId GetWindowId() const override { return id_; }
  gfx::NativeWindow GetNativeWindow() const override {
    return gfx::NativeWindow();
  }
  gfx::Rect GetBoundsInScreen() const override;
  bool IsDraggingEntireWindow(size_t dragged_tab_count) const override;
  gfx::Point ConvertScreenPointToLocal(
      gfx::NativeView target_view,
      const gfx::Point& screen_point) const override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool HasCapture() const override;
  void Activate() override { activated_ = true; }
  bool activated() const { return activated_; }

  // TabDragWindowAdapter overrides:
  base::expected<TabDragWindowId, mojo_base::mojom::ErrorPtr> DetachToNewWindow(
      const std::vector<tabs_api::NodeId>& tab_ids,
      const gfx::Point& screen_point,
      const gfx::Vector2d& drag_offset) override {
    detach_to_new_window_called_ = true;
    last_detach_tab_ids_ = tab_ids;
    last_detach_drag_offset_ = drag_offset;
    if (detach_to_new_window_result_.has_value()) {
      return detach_to_new_window_result_.value();
    }
    return base::unexpected(mojo_base::mojom::Error::New(
        detach_to_new_window_result_.error()->code,
        detach_to_new_window_result_.error()->message));
  }

  DragMoveLoopResult RunWindowMoveLoop(
      const gfx::Point& screen_point,
      const gfx::Vector2d& drag_offset,
      WindowMoveCallback move_callback) override {
    run_window_move_loop_called_ = true;
    had_capture_on_move_loop_ = has_capture_;
    last_move_loop_point_ = screen_point;
    last_move_loop_offset_ = drag_offset;

    loop_ended_ = false;
    for (const auto& move_point : simulated_moves_) {
      if (loop_ended_) {
        break;
      }
      move_callback.Run(move_point);
    }

    return run_window_move_loop_result_;
  }

  void EndWindowMoveLoop() override { loop_ended_ = true; }

  base::expected<void, mojo_base::mojom::ErrorPtr> MigrateTabs(
      TabDragWindowId target_window_id,
      const std::vector<tabs_api::NodeId>& tab_ids) override {
    return base::ok();
  }

  // Toy controls:
  void set_tab_count(size_t count) { tab_count_ = count; }
  void set_detach_to_new_window_result(
      base::expected<TabDragWindowId, mojo_base::mojom::ErrorPtr> result) {
    detach_to_new_window_result_ = std::move(result);
  }
  void set_detach_to_new_window_result(TabDragWindowId result) {
    detach_to_new_window_result_ = result;
  }
  bool detach_to_new_window_called() const {
    return detach_to_new_window_called_;
  }
  const std::vector<tabs_api::NodeId>& last_detach_tab_ids() const {
    return last_detach_tab_ids_;
  }
  const gfx::Vector2d& last_detach_drag_offset() const {
    return last_detach_drag_offset_;
  }
  bool run_window_move_loop_called() const {
    return run_window_move_loop_called_;
  }
  bool had_capture_on_move_loop() const { return had_capture_on_move_loop_; }
  const gfx::Point& last_move_loop_point() const {
    return last_move_loop_point_;
  }
  const gfx::Vector2d& last_move_loop_offset() const {
    return last_move_loop_offset_;
  }
  void set_run_window_move_loop_result(DragMoveLoopResult result) {
    run_window_move_loop_result_ = result;
  }
  void set_simulated_moves(std::vector<gfx::Point> moves) {
    simulated_moves_ = std::move(moves);
  }

 private:
  size_t tab_count_ = 2;
  gfx::Rect bounds_;
  bool has_capture_ = false;
  bool had_capture_on_move_loop_ = false;
  bool activated_ = false;
  TabDragWindowId id_;
  raw_ptr<TabDragWindowRegistry> registry_;

  base::expected<TabDragWindowId, mojo_base::mojom::ErrorPtr>
      detach_to_new_window_result_ = base::unexpected(
          mojo_base::mojom::Error::New(mojo_base::mojom::Code::kUnimplemented,
                                       "Not implemented"));
  bool detach_to_new_window_called_ = false;
  std::vector<tabs_api::NodeId> last_detach_tab_ids_;
  gfx::Vector2d last_detach_drag_offset_;
  bool run_window_move_loop_called_ = false;
  gfx::Point last_move_loop_point_;
  gfx::Vector2d last_move_loop_offset_;
  DragMoveLoopResult run_window_move_loop_result_ =
      DragMoveLoopResult::kSuccess;
  std::vector<gfx::Point> simulated_moves_;
  bool loop_ended_ = false;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_TAB_DRAG_WINDOW_ADAPTER_H_
