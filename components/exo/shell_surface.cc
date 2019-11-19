// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/shell_surface.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_state_type.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/exo/shell_surface_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"

namespace exo {
namespace {

// Maximum amount of time to wait for contents after a change to maximize,
// fullscreen or pinned state.
constexpr int kMaximizedOrFullscreenOrPinnedLockTimeoutMs = 100;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ShellSurface, ScopedAnimationsDisabled:

// Helper class used to temporarily disable animations. Restores the
// animations disabled property when instance is destroyed.
class ShellSurface::ScopedAnimationsDisabled {
 public:
  explicit ScopedAnimationsDisabled(ShellSurface* shell_surface);
  ~ScopedAnimationsDisabled();

 private:
  ShellSurface* const shell_surface_;
  bool saved_animations_disabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScopedAnimationsDisabled);
};

ShellSurface::ScopedAnimationsDisabled::ScopedAnimationsDisabled(
    ShellSurface* shell_surface)
    : shell_surface_(shell_surface) {
  if (shell_surface_->widget_) {
    aura::Window* window = shell_surface_->widget_->GetNativeWindow();
    saved_animations_disabled_ =
        window->GetProperty(aura::client::kAnimationsDisabledKey);
    window->SetProperty(aura::client::kAnimationsDisabledKey, true);
  }
}

ShellSurface::ScopedAnimationsDisabled::~ScopedAnimationsDisabled() {
  if (shell_surface_->widget_) {
    aura::Window* window = shell_surface_->widget_->GetNativeWindow();
    DCHECK_EQ(window->GetProperty(aura::client::kAnimationsDisabledKey), true);
    window->SetProperty(aura::client::kAnimationsDisabledKey,
                        saved_animations_disabled_);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ShellSurface, Config:

// Surface state associated with each configure request.
struct ShellSurface::Config {
  Config(uint32_t serial,
         const gfx::Vector2d& origin_offset,
         int resize_component,
         std::unique_ptr<ui::CompositorLock> compositor_lock);
  ~Config() = default;

  uint32_t serial;
  gfx::Vector2d origin_offset;
  int resize_component;
  std::unique_ptr<ui::CompositorLock> compositor_lock;
};

ShellSurface::Config::Config(
    uint32_t serial,
    const gfx::Vector2d& origin_offset,
    int resize_component,
    std::unique_ptr<ui::CompositorLock> compositor_lock)
    : serial(serial),
      origin_offset(origin_offset),
      resize_component(resize_component),
      compositor_lock(std::move(compositor_lock)) {}

////////////////////////////////////////////////////////////////////////////////
// ShellSurface, ScopedConfigure:

ShellSurface::ScopedConfigure::ScopedConfigure(ShellSurface* shell_surface,
                                               bool force_configure)
    : shell_surface_(shell_surface), force_configure_(force_configure) {
  // ScopedConfigure instances cannot be nested.
  DCHECK(!shell_surface_->scoped_configure_);
  shell_surface_->scoped_configure_ = this;
}

ShellSurface::ScopedConfigure::~ScopedConfigure() {
  DCHECK_EQ(shell_surface_->scoped_configure_, this);
  shell_surface_->scoped_configure_ = nullptr;
  if (needs_configure_ || force_configure_)
    shell_surface_->Configure();
  // ScopedConfigure instance might have suppressed a widget bounds update.
  if (shell_surface_->widget_) {
    shell_surface_->UpdateWidgetBounds();
    shell_surface_->UpdateShadow();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ShellSurface, public:

ShellSurface::ShellSurface(Surface* surface,
                           const gfx::Point& origin,
                           bool activatable,
                           bool can_minimize,
                           int container)
    : ShellSurfaceBase(surface, origin, activatable, can_minimize, container) {}

ShellSurface::ShellSurface(Surface* surface)
    : ShellSurfaceBase(surface,
                       gfx::Point(),
                       true,
                       true,
                       ash::desks_util::GetActiveDeskContainerId()) {}

ShellSurface::~ShellSurface() {
  DCHECK(!scoped_configure_);
  if (widget_)
    ash::WindowState::Get(widget_->GetNativeWindow())->RemoveObserver(this);
}

void ShellSurface::AcknowledgeConfigure(uint32_t serial) {
  TRACE_EVENT1("exo", "ShellSurface::AcknowledgeConfigure", "serial", serial);

  // Apply all configs that are older or equal to |serial|. The result is that
  // the origin of the main surface will move and the resize direction will
  // change to reflect the acknowledgement of configure request with |serial|
  // at the next call to Commit().
  while (!pending_configs_.empty()) {
    std::unique_ptr<Config> config = std::move(pending_configs_.front());
    pending_configs_.pop_front();

    // Add the config offset to the accumulated offset that will be applied when
    // Commit() is called.
    pending_origin_offset_ += config->origin_offset;

    // Set the resize direction that will be applied when Commit() is called.
    pending_resize_component_ = config->resize_component;

    if (config->serial == serial)
      break;
  }

  if (widget_) {
    UpdateWidgetBounds();
    UpdateShadow();
  }
}

void ShellSurface::SetParent(ShellSurface* parent) {
  TRACE_EVENT1("exo", "ShellSurface::SetParent", "parent",
               parent ? base::UTF16ToASCII(parent->title_) : "null");

  SetParentWindow(parent ? parent->GetWidget()->GetNativeWindow() : nullptr);
}

void ShellSurface::Maximize() {
  TRACE_EVENT0("exo", "ShellSurface::Maximize");

  if (!widget_) {
    initial_show_state_ = ui::SHOW_STATE_MAXIMIZED;
    return;
  }

  // Note: This will ask client to configure its surface even if already
  // maximized.
  ScopedConfigure scoped_configure(this, true);
  widget_->Maximize();
}

void ShellSurface::Minimize() {
  TRACE_EVENT0("exo", "ShellSurface::Minimize");

  if (!widget_) {
    initial_show_state_ = ui::SHOW_STATE_MINIMIZED;
    return;
  }

  // Note: This will ask client to configure its surface even if already
  // minimized.
  ScopedConfigure scoped_configure(this, true);
  widget_->Minimize();
}

void ShellSurface::Restore() {
  TRACE_EVENT0("exo", "ShellSurface::Restore");

  if (!widget_) {
    initial_show_state_ = ui::SHOW_STATE_NORMAL;
    return;
  }

  // Note: This will ask client to configure its surface even if not already
  // maximized or minimized.
  ScopedConfigure scoped_configure(this, true);
  widget_->Restore();
}

void ShellSurface::SetFullscreen(bool fullscreen) {
  TRACE_EVENT1("exo", "ShellSurface::SetFullscreen", "fullscreen", fullscreen);

  if (!widget_) {
    initial_show_state_ = ui::SHOW_STATE_FULLSCREEN;
    return;
  }

  // Note: This will ask client to configure its surface even if fullscreen
  // state doesn't change.
  ScopedConfigure scoped_configure(this, true);
  widget_->SetFullscreen(fullscreen);
}

void ShellSurface::SetPopup() {
  DCHECK(!widget_);
  is_popup_ = true;
}

void ShellSurface::Grab() {
  DCHECK(is_popup_);
  DCHECK(!widget_);
  has_grab_ = true;
}

void ShellSurface::StartMove() {
  TRACE_EVENT0("exo", "ShellSurface::StartMove");

  if (!widget_)
    return;

  AttemptToStartDrag(HTCAPTION);
}

void ShellSurface::StartResize(int component) {
  TRACE_EVENT1("exo", "ShellSurface::StartResize", "component", component);

  if (!widget_)
    return;

  AttemptToStartDrag(component);
}

bool ShellSurface::ShouldAutoMaximize() {
  // Unless a child class overrides the behaviour, we will never auto-maximize.
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceDelegate overrides:

void ShellSurface::OnSetParent(Surface* parent, const gfx::Point& position) {
  views::Widget* parent_widget =
      parent ? views::Widget::GetTopLevelWidgetForNativeView(parent->window())
             : nullptr;
  if (parent_widget) {
    // Set parent window if using one of the desks container and the container
    // itself is not the parent.
    if (ash::desks_util::IsDeskContainerId(container_))
      SetParentWindow(parent_widget->GetNativeWindow());

    origin_ = position;
    views::View::ConvertPointToScreen(
        parent_widget->widget_delegate()->GetContentsView(), &origin_);

    if (!widget_)
      return;

    ash::WindowState* window_state =
        ash::WindowState::Get(widget_->GetNativeWindow());
    if (window_state->is_dragged())
      return;

    gfx::Rect widget_bounds = widget_->GetWindowBoundsInScreen();
    gfx::Rect new_widget_bounds(origin_, widget_bounds.size());
    if (new_widget_bounds != widget_bounds) {
      base::AutoReset<bool> auto_ignore_window_bounds_changes(
          &ignore_window_bounds_changes_, true);
      widget_->SetBounds(new_widget_bounds);
      UpdateSurfaceBounds();
    }
  } else {
    SetParentWindow(nullptr);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ShellSurfaceBase overrides:

void ShellSurface::InitializeWindowState(ash::WindowState* window_state) {
  window_state->AddObserver(this);
  window_state->set_allow_set_bounds_direct(movement_disabled_);
  window_state->set_ignore_keyboard_bounds_change(movement_disabled_);
  widget_->set_movement_disabled(movement_disabled_);

  // If this window is a child of some window, it should be made transient.
  MaybeMakeTransient();
}

base::Optional<gfx::Rect> ShellSurface::GetWidgetBounds() const {
  // Defer if configure requests are pending.
  if (!pending_configs_.empty() || scoped_configure_)
    return base::nullopt;

  gfx::Rect visible_bounds = GetVisibleBounds();
  gfx::Rect new_widget_bounds =
      widget_->non_client_view()
          ? widget_->non_client_view()->GetWindowBoundsForClientBounds(
                visible_bounds)
          : visible_bounds;

  if (movement_disabled_) {
    new_widget_bounds.set_origin(origin_);
  } else if (resize_component_ == HTCAPTION) {
    // Preserve widget position.
    new_widget_bounds.set_origin(widget_->GetWindowBoundsInScreen().origin());
  } else {
    // Compute widget origin using surface origin if the current location of
    // surface is being anchored to one side of the widget as a result of a
    // resize operation.
    gfx::Rect visible_bounds = GetVisibleBounds();
    gfx::Point origin = GetSurfaceOrigin() + visible_bounds.OffsetFromOrigin();
    wm::ConvertPointToScreen(widget_->GetNativeWindow(), &origin);
    new_widget_bounds.set_origin(origin);
  }
  return new_widget_bounds;
}

gfx::Point ShellSurface::GetSurfaceOrigin() const {
  DCHECK(!movement_disabled_ || resize_component_ == HTCAPTION);

  gfx::Rect visible_bounds = GetVisibleBounds();
  gfx::Rect client_bounds = GetClientViewBounds();

  switch (resize_component_) {
    case HTCAPTION:
      return gfx::Point() + origin_offset_ - visible_bounds.OffsetFromOrigin();
    case HTBOTTOM:
    case HTRIGHT:
    case HTBOTTOMRIGHT:
      return gfx::Point() - visible_bounds.OffsetFromOrigin();
    case HTTOP:
    case HTTOPRIGHT:
      return gfx::Point(0, client_bounds.height() - visible_bounds.height()) -
             visible_bounds.OffsetFromOrigin();
    case HTLEFT:
    case HTBOTTOMLEFT:
      return gfx::Point(client_bounds.width() - visible_bounds.width(), 0) -
             visible_bounds.OffsetFromOrigin();
    case HTTOPLEFT:
      return gfx::Point(client_bounds.width() - visible_bounds.width(),
                        client_bounds.height() - visible_bounds.height()) -
             visible_bounds.OffsetFromOrigin();
    default:
      NOTREACHED();
      return gfx::Point();
  }
}

////////////////////////////////////////////////////////////////////////////////
// aura::WindowObserver overrides:

void ShellSurface::OnWindowBoundsChanged(aura::Window* window,
                                         const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds,
                                         ui::PropertyChangeReason reason) {
  if (!widget_ || !root_surface() || ignore_window_bounds_changes_)
    return;

  if (window == widget_->GetNativeWindow()) {
    if (new_bounds.size() == old_bounds.size())
      return;

    // If size changed then give the client a chance to produce new contents
    // before origin on screen is changed. Retain the old origin by reverting
    // the origin delta until the next configure is acknowledged.
    gfx::Vector2d delta = new_bounds.origin() - old_bounds.origin();
    origin_offset_ -= delta;
    pending_origin_offset_accumulator_ += delta;

    UpdateSurfaceBounds();

    // The shadow size may be updated to match the widget. Change it back
    // to the shadow content size. Note that this relies on wm::ShadowController
    // being notified of the change before |this|.
    UpdateShadow();

    Configure();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ash::WindowStateObserver overrides:

void ShellSurface::OnPreWindowStateTypeChange(ash::WindowState* window_state,
                                              ash::WindowStateType old_type) {
  ash::WindowStateType new_type = window_state->GetStateType();
  if (ash::IsMinimizedWindowStateType(old_type) ||
      ash::IsMinimizedWindowStateType(new_type)) {
    return;
  }

  if (ash::IsMaximizedOrFullscreenOrPinnedWindowStateType(old_type) ||
      ash::IsMaximizedOrFullscreenOrPinnedWindowStateType(new_type)) {
    if (!widget_)
      return;
    // When transitioning in/out of maximized or fullscreen mode, we need to
    // make sure we have a configure callback before we allow the default
    // cross-fade animations. The configure callback provides a mechanism for
    // the client to inform us that a frame has taken the state change into
    // account, and without this cross-fade animations are unreliable.
    if (!configure_callback_.is_null()) {
      // Give client a chance to produce a frame that takes state change into
      // account by acquiring a compositor lock.
      ui::Compositor* compositor =
          widget_->GetNativeWindow()->layer()->GetCompositor();
      configure_compositor_lock_ = compositor->GetCompositorLock(
          nullptr, base::TimeDelta::FromMilliseconds(
                       kMaximizedOrFullscreenOrPinnedLockTimeoutMs));
    } else {
      scoped_animations_disabled_ =
          std::make_unique<ScopedAnimationsDisabled>(this);
    }
  }
}

void ShellSurface::OnPostWindowStateTypeChange(ash::WindowState* window_state,
                                               ash::WindowStateType old_type) {
  ash::WindowStateType new_type = window_state->GetStateType();
  if (ash::IsMaximizedOrFullscreenOrPinnedWindowStateType(new_type)) {
    Configure();
  }

  if (widget_) {
    UpdateWidgetBounds();
    UpdateShadow();
  }

  // Re-enable animations if they were disabled in pre state change handler.
  scoped_animations_disabled_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// wm::ActivationChangeObserver overrides:

void ShellSurface::OnWindowActivated(ActivationReason reason,
                                     aura::Window* gained_active,
                                     aura::Window* lost_active) {
  ShellSurfaceBase::OnWindowActivated(reason, gained_active, lost_active);

  if (!widget_)
    return;

  if (gained_active == widget_->GetNativeWindow() ||
      lost_active == widget_->GetNativeWindow()) {
    Configure();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ShellSurfaceBase overrides:

void ShellSurface::SetWidgetBounds(const gfx::Rect& bounds) {
  if (bounds == widget_->GetWindowBoundsInScreen())
    return;

  // Set |ignore_window_bounds_changes_| as this change to window bounds
  // should not result in a configure request.
  DCHECK(!ignore_window_bounds_changes_);
  ignore_window_bounds_changes_ = true;

  widget_->SetBounds(bounds);
  UpdateSurfaceBounds();

  ignore_window_bounds_changes_ = false;
}

bool ShellSurface::OnPreWidgetCommit() {
  if (!widget_ && GetEnabled()) {
    // Defer widget creation and commit until surface has contents.
    if (host_window()->bounds().IsEmpty() &&
        root_surface()->surface_hierarchy_content_bounds().IsEmpty()) {
      Configure();
      return false;
    }

    // Allow the window to maximize itself on launch.
    if (ShouldAutoMaximize())
      initial_show_state_ = ui::SHOW_STATE_MAXIMIZED;

    CreateShellSurfaceWidget(initial_show_state_);
  }

  // Apply the accumulated pending origin offset to reflect acknowledged
  // configure requests.
  origin_offset_ += pending_origin_offset_;
  pending_origin_offset_ = gfx::Vector2d();

  // Update resize direction to reflect acknowledged configure requests.
  resize_component_ = pending_resize_component_;

  return true;
}

void ShellSurface::OnPostWidgetCommit() {}

////////////////////////////////////////////////////////////////////////////////
// ShellSurface, private:

void ShellSurface::SetParentWindow(aura::Window* parent) {
  if (parent_) {
    parent_->RemoveObserver(this);
    if (widget_) {
      aura::Window* child_window = widget_->GetNativeWindow();
      wm::TransientWindowManager::GetOrCreate(child_window)
          ->set_parent_controls_visibility(false);
      wm::RemoveTransientChild(parent_, child_window);
    }
  }
  parent_ = parent;
  if (parent_) {
    parent_->AddObserver(this);
    MaybeMakeTransient();
  }

  // If |parent_| is set effects the ability to maximize the window.
  if (widget_)
    widget_->OnSizeConstraintsChanged();
}

void ShellSurface::MaybeMakeTransient() {
  if (!parent_ || !widget_)
    return;
  aura::Window* child_window = widget_->GetNativeWindow();
  wm::AddTransientChild(parent_, child_window);
  // In the case of activatable non-popups, we also want the parent to control
  // the child's visibility.
  if (!widget_->is_top_level() || !widget_->CanActivate())
    return;
  wm::TransientWindowManager::GetOrCreate(child_window)
      ->set_parent_controls_visibility(true);
}

void ShellSurface::Configure(bool ends_drag) {
  // Delay configure callback if |scoped_configure_| is set.
  if (scoped_configure_) {
    scoped_configure_->set_needs_configure();
    return;
  }

  gfx::Vector2d origin_offset = pending_origin_offset_accumulator_;
  pending_origin_offset_accumulator_ = gfx::Vector2d();

  auto* window_state =
      widget_ ? ash::WindowState::Get(widget_->GetNativeWindow()) : nullptr;
  int resize_component = HTCAPTION;
  // If surface is being resized, save the resize direction.
  if (window_state && window_state->is_dragged() && !ends_drag)
    resize_component = window_state->drag_details()->window_component;

  uint32_t serial = 0;
  if (!configure_callback_.is_null()) {
    if (window_state) {
      serial = configure_callback_.Run(
          GetClientViewBounds().size(), window_state->GetStateType(),
          IsResizing(), widget_->IsActive(), origin_offset);
    } else {
      serial =
          configure_callback_.Run(gfx::Size(), ash::WindowStateType::kNormal,
                                  false, false, origin_offset);
    }
  }

  if (!serial) {
    pending_origin_offset_ += origin_offset;
    pending_resize_component_ = resize_component;
    return;
  }

  // Apply origin offset and resize component at the first Commit() after this
  // configure request has been acknowledged.
  pending_configs_.push_back(
      std::make_unique<Config>(serial, origin_offset, resize_component,
                               std::move(configure_compositor_lock_)));
  LOG_IF(WARNING, pending_configs_.size() > 100)
      << "Number of pending configure acks for shell surface has reached: "
      << pending_configs_.size();
}

void ShellSurface::AttemptToStartDrag(int component) {
  ash::WindowState* window_state =
      ash::WindowState::Get(widget_->GetNativeWindow());

  // Ignore if surface is already being dragged.
  if (window_state->is_dragged())
    return;

  aura::Window* target = widget_->GetNativeWindow();
  ash::ToplevelWindowEventHandler* toplevel_handler =
      ash::Shell::Get()->toplevel_window_event_handler();
  aura::Window* mouse_pressed_handler =
      target->GetHost()->dispatcher()->mouse_pressed_handler();
  // Start dragging only if:
  // 1) touch guesture is in progress.
  // 2) mouse was pressed on the target or its subsurfaces.
  aura::Window* gesture_target = toplevel_handler->gesture_target();
  if (!gesture_target && !mouse_pressed_handler &&
      target->Contains(mouse_pressed_handler)) {
    return;
  }
  auto end_drag = [](ShellSurface* shell_surface,
                     ash::ToplevelWindowEventHandler::DragResult result) {
    shell_surface->EndDrag();
  };

  if (gesture_target) {
    gfx::Point location = toplevel_handler->event_location_in_gesture_target();
    aura::Window::ConvertPointToTarget(
        gesture_target, widget_->GetNativeWindow()->GetRootWindow(), &location);
    toplevel_handler->AttemptToStartDrag(
        target, location, component,
        base::BindOnce(end_drag, base::Unretained(this)));
  } else {
    gfx::Point location = aura::Env::GetInstance()->last_mouse_location();
    ::wm::ConvertPointFromScreen(widget_->GetNativeWindow()->GetRootWindow(),
                                 &location);
    toplevel_handler->AttemptToStartDrag(
        target, location, component,
        base::BindOnce(end_drag, base::Unretained(this)));
  }
  // Notify client that resizing state has changed.
  if (IsResizing())
    Configure();
}

void ShellSurface::EndDrag() {
  if (resize_component_ != HTCAPTION) {
    Configure(/*ends_drag=*/true);
  }
}

}  // namespace exo
