// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_FULLSCREEN_SHELL_SURFACE_H_
#define COMPONENTS_EXO_FULLSCREEN_SHELL_SURFACE_H_

#include "components/exo/surface_observer.h"
#include "components/exo/surface_tree_host.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/aura/window_observer.h"
#include "ui/views/widget/widget_delegate.h"

namespace exo {
class Surface;

// This class implements a toplevel fullscreen surface for which position
// and state are managed by the shell.
class FullscreenShellSurface : public SurfaceTreeHost,
                               public SurfaceObserver,
                               public aura::WindowObserver,
                               public views::WidgetDelegate,
                               public views::View {
 public:
  FullscreenShellSurface();
  ~FullscreenShellSurface() override;

  // Set the callback to run when the user wants the shell surface to be closed.
  // The receiver can chose to not close the window on this signal.
  void set_close_callback(const base::RepeatingClosure& close_callback) {
    close_callback_ = close_callback;
  }

  // Set the callback to run when the surface is destroyed.
  void set_surface_destroyed_callback(
      base::OnceClosure surface_destroyed_callback) {
    surface_destroyed_callback_ = std::move(surface_destroyed_callback);
  }

  // Set the application ID for the surface
  void SetApplicationId(const char* startup_id);

  // Set the startup ID for the surface.
  void SetStartupId(const char* startup_id);

  // Set the Surface in use. Will replace root_surface_ if a surface is
  // currently set. Will remove root_surface_ if |surface| is nullptr.
  void SetSurface(Surface* surface);

  void Maximize();

  void Minimize();

  void Close();

  // Overridden from SurfaceDelegate:
  void OnSurfaceCommit() override;
  bool IsInputEnabled(Surface* surface) const override;
  void OnSetFrame(SurfaceFrameType type) override;
  void OnSetFrameColors(SkColor active_color, SkColor inactive_color) override;
  void OnSetStartupId(const char* startup_id) override;
  void OnSetApplicationId(const char* application_id) override;

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

  // Overridden from views::WidgetDelegate:
  bool CanResize() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  bool ShouldShowWindowTitle() const override;
  void WindowClosing() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  views::View* GetContentsView() override;
  bool WidgetHasHitTestMask() const override;
  void GetWidgetHitTestMask(SkPath* mask) const override;

  // Overridden from aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // Overridden from ui::View
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  void SetChildAxTreeId(ui::AXTreeID child_ax_tree_id);

 private:
  // Keep the bounds in sync with the root surface bounds.
  void UpdateHostWindowBounds() override;

  void CreateFullscreenShellSurfaceWidget(ui::WindowShowState show_state);
  void CommitWidget();
  bool OnPreWidgetCommit();

  views::Widget* widget_ = nullptr;
  aura::Window* parent_ = nullptr;
  base::Optional<std::string> application_id_;
  base::Optional<std::string> startup_id_;
  base::RepeatingClosure close_callback_;
  base::OnceClosure surface_destroyed_callback_;
  ui::AXTreeID child_ax_tree_id_ = ui::AXTreeIDUnknown();

  DISALLOW_COPY_AND_ASSIGN(FullscreenShellSurface);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_FULLSCREEN_SHELL_SURFACE_H
