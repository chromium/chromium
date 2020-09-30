// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/fullscreen_shell_surface.h"

#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/compositor.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace exo {

class FullscreenShellSurface::FullscreenShellView : public views::View {
 public:
  FullscreenShellView() = default;
  FullscreenShellView(const FullscreenShellView&) = delete;
  FullscreenShellView& operator=(const FullscreenShellView&) = delete;
  ~FullscreenShellView() override = default;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kClient;

    if (child_ax_tree_id_ == ui::AXTreeIDUnknown())
      return;

    node_data->AddStringAttribute(ax::mojom::StringAttribute::kChildTreeId,
                                  child_ax_tree_id_.ToString());
  }

  void SetChildAxTreeId(ui::AXTreeID child_ax_tree_id) {
    child_ax_tree_id_ = child_ax_tree_id;
  }

 private:
  ui::AXTreeID child_ax_tree_id_ = ui::AXTreeIDUnknown();
};

FullscreenShellSurface::FullscreenShellSurface()
    : SurfaceTreeHost("FullscreenShellSurfaceHost") {
  CreateFullscreenShellSurfaceWidget(ui::SHOW_STATE_FULLSCREEN);
  widget_->SetFullscreen(true);
}

FullscreenShellSurface::~FullscreenShellSurface() {
  if (widget_) {
    widget_->GetNativeWindow()->RemoveObserver(this);
    // Remove transient children so they are not automatically destroyed.
    for (auto* child : wm::GetTransientChildren(widget_->GetNativeWindow()))
      wm::RemoveTransientChild(widget_->GetNativeWindow(), child);
    if (widget_->IsVisible())
      widget_->Hide();
    widget_->CloseNow();
  }
  if (parent_)
    parent_->RemoveObserver(this);
  if (root_surface())
    root_surface()->RemoveSurfaceObserver(this);
}

void FullscreenShellSurface::SetApplicationId(const char* application_id) {
  // Store the value in |application_id_| in case the window does not exist yet.
  if (application_id)
    application_id_ = std::string(application_id);
  else
    application_id_.reset();

  if (widget_ && widget_->GetNativeWindow())
    SetShellApplicationId(widget_->GetNativeWindow(), application_id_);
}

void FullscreenShellSurface::SetStartupId(const char* startup_id) {
  // Store the value in |startup_id_| in case the window does not exist yet.
  if (startup_id)
    startup_id_ = std::string(startup_id);
  else
    startup_id_.reset();

  if (widget_ && widget_->GetNativeWindow())
    SetShellStartupId(widget_->GetNativeWindow(), startup_id_);
}

void FullscreenShellSurface::SetSurface(Surface* surface) {
  if (root_surface())
    root_surface()->RemoveSurfaceObserver(this);
  SetRootSurface(surface);
  SetShellMainSurface(widget_->GetNativeWindow(), root_surface());
  if (surface) {
    surface->AddSurfaceObserver(this);
    host_window()->Show();
    widget_->Show();
  } else {
    host_window()->Hide();
    widget_->Hide();
  }
}

void FullscreenShellSurface::Maximize() {
  if (!widget_)
    return;

  widget_->Maximize();
}

void FullscreenShellSurface::Minimize() {
  if (!widget_)
    return;

  widget_->Minimize();
}

void FullscreenShellSurface::Close() {
  if (!close_callback_.is_null())
    close_callback_.Run();
}

void FullscreenShellSurface::OnSurfaceCommit() {
  SurfaceTreeHost::OnSurfaceCommit();
  if (!OnPreWidgetCommit())
    return;

  CommitWidget();
  SubmitCompositorFrame();
}

bool FullscreenShellSurface::IsInputEnabled(Surface*) const {
  return true;
}

void FullscreenShellSurface::OnSetFrame(SurfaceFrameType frame_type) {}

void FullscreenShellSurface::OnSetFrameColors(SkColor active_color,
                                              SkColor inactive_color) {}

void FullscreenShellSurface::OnSetStartupId(const char* startup_id) {
  SetStartupId(startup_id);
}

void FullscreenShellSurface::OnSetApplicationId(const char* application_id) {
  SetApplicationId(application_id);
}

void FullscreenShellSurface::OnSurfaceDestroying(Surface* surface) {
  DCHECK_EQ(root_surface(), surface);
  surface->RemoveSurfaceObserver(this);
  SetRootSurface(nullptr);

  if (widget_)
    SetShellMainSurface(widget_->GetNativeWindow(), nullptr);

  // Hide widget before surface is destroyed. This allows hide animations to
  // run using the current surface contents.
  if (widget_) {
    // Remove transient children so they are not automatically hidden.
    for (auto* child : wm::GetTransientChildren(widget_->GetNativeWindow()))
      wm::RemoveTransientChild(widget_->GetNativeWindow(), child);

    widget_->Hide();
  }

  // Note: In its use in the Wayland server implementation, the surface
  // destroyed callback may destroy the ShellSurface instance. This call needs
  // to be last so that the instance can be destroyed.
  std::move(surface_destroyed_callback_).Run();
}

bool FullscreenShellSurface::CanResize() const {
  return false;
}

bool FullscreenShellSurface::CanMaximize() const {
  return true;
}

bool FullscreenShellSurface::CanMinimize() const {
  return true;
}

bool FullscreenShellSurface::ShouldShowWindowTitle() const {
  return false;
}

void FullscreenShellSurface::WindowClosing() {
  contents_view_->SetEnabled(false);
  contents_view_ = nullptr;
  widget_ = nullptr;
}

views::Widget* FullscreenShellSurface::GetWidget() {
  return widget_;
}

const views::Widget* FullscreenShellSurface::GetWidget() const {
  return widget_;
}

views::View* FullscreenShellSurface::GetContentsView() {
  if (!contents_view_)
    contents_view_ = new FullscreenShellView();
  return contents_view_;
}

bool FullscreenShellSurface::WidgetHasHitTestMask() const {
  return true;
}

void FullscreenShellSurface::GetWidgetHitTestMask(SkPath* mask) const {
  GetHitTestMask(mask);
  gfx::Point origin = host_window()->bounds().origin();
  SkMatrix matrix;
  // TODO (sagallea) acquire scale from display
  float scale = 1.f;
  matrix.setScaleTranslate(
      SkFloatToScalar(1.0f / scale), SkFloatToScalar(1.0f / scale),
      SkIntToScalar(origin.x()), SkIntToScalar(origin.y()));
  mask->transform(matrix);
}

void FullscreenShellSurface::OnWindowDestroying(aura::Window* window) {
  if (window == parent_) {
    parent_ = nullptr;
    // |parent_| being set to null effects the ability to maximize the window.
    if (widget_)
      widget_->OnSizeConstraintsChanged();
  }

  window->RemoveObserver(this);
}

void FullscreenShellSurface::SetChildAxTreeId(ui::AXTreeID child_ax_tree_id) {
  DCHECK(contents_view_);
  contents_view_->SetChildAxTreeId(child_ax_tree_id);
}

void FullscreenShellSurface::SetEnabled(bool enabled) {
  DCHECK(contents_view_);
  contents_view_->SetEnabled(enabled);
}

void FullscreenShellSurface::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  DCHECK(contents_view_);
  contents_view_->GetAccessibleNodeData(node_data);
}

void FullscreenShellSurface::UpdateHostWindowBounds() {
  // This method applies multiple changes to the window tree. Use ScopedPause
  // to ensure that occlusion isn't recomputed before all changes have been
  // applied.
  aura::WindowOcclusionTracker::ScopedPause pause_occlusion;

  host_window()->SetBounds(
      gfx::Rect(root_surface()->window()->bounds().size()));
  host_window()->SetTransparent(!root_surface()->FillsBoundsOpaquely());
}

void FullscreenShellSurface::CreateFullscreenShellSurfaceWidget(
    ui::WindowShowState show_state) {
  DCHECK(!widget_);

  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.ownership = views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;
  params.delegate = this;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.show_state = show_state;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_YES;
  params.parent = WMHelper::GetInstance()->GetRootWindowForNewWindows();
  params.bounds = gfx::Rect(params.parent->bounds().size());

  widget_ = new views::Widget();
  widget_->Init(std::move(params));

  aura::Window* window = widget_->GetNativeWindow();
  window->SetName("FullscreenShellSurface");
  window->AddChild(host_window());

  SetShellApplicationId(window, application_id_);
  SetShellStartupId(window, startup_id_);
  SetShellMainSurface(window, root_surface());
  SetArcAppType(window);

  window->AddObserver(this);
}

void FullscreenShellSurface::CommitWidget() {
  if (!widget_)
    return;

  // Show widget if needed.
  if (!widget_->IsVisible()) {
    DCHECK(!widget_->IsClosed());
    widget_->Show();
  }
}

bool FullscreenShellSurface::OnPreWidgetCommit() {
  // If we have a |widget_|, then we must have a |contents_view_| as both are
  // created together.
  if (!widget_ && contents_view_->GetEnabled() &&
      host_window()->bounds().IsEmpty()) {
    return false;
  }

  return true;
}

}  // namespace exo
