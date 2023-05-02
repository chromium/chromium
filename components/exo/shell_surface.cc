// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/shell_surface.h"

#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/exo/custom_window_state_delegate.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"

namespace exo {
namespace {

// Default maximum amount of time to wait for contents to change. For example,
// happens during a maximize, fullscreen or pinned state change, or raster scale
// change.
constexpr int kDefaultCompositorLockTimeoutMs = 100;

gfx::Rect GetClientBoundsInScreen(views::Widget* widget) {
  gfx::Rect window_bounds = widget->GetWindowBoundsInScreen();
  // Account for popup windows not having a non-client view.
  if (widget->non_client_view()) {
    return static_cast<ash::NonClientFrameViewAsh*>(
               widget->non_client_view()->frame_view())
        ->GetClientBoundsForWindowBounds(window_bounds);
  }
  return window_bounds;
}

// HTCLIENT can be used to drag the window in specific scenario.
// (e.g. Drag from shelf)
bool IsMoveComponent(int resize_component) {
  return resize_component == HTCAPTION || resize_component == HTCLIENT;
}

}  // namespace

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
                           bool can_minimize,
                           int container)
    : ShellSurfaceBase(surface, origin, can_minimize, container) {}

ShellSurface::ShellSurface(Surface* surface)
    : ShellSurfaceBase(surface,
                       gfx::Point(),
                       /*can_minimize=*/true,
                       ash::desks_util::GetActiveDeskContainerId()) {}

ShellSurface::~ShellSurface() {
  DCHECK(!scoped_configure_);
  // Client is gone by now, so don't call callback.
  configure_callback_.Reset();
  origin_change_callback_.Reset();
  ash::WindowState* window_state =
      widget_ ? ash::WindowState::Get(widget_->GetNativeWindow()) : nullptr;
  if (window_state)
    window_state->RemoveObserver(this);

  for (auto& observer : observers_)
    observer.OnShellSurfaceDestroyed();
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

    if (config->serial == serial) {
      // `config` needs to stay alive until the next Commit() call.
      config_waiting_for_commit_ = std::move(config);
      break;
    }
  }

  for (auto& observer : observers_)
    observer.OnAcknowledgeConfigure(serial);

  // Shadow bounds update should be called in the next Commit() when applying
  // config instead of updating right when the client acknowledge the config.
}

void ShellSurface::SetParent(ShellSurface* parent) {
  TRACE_EVENT1("exo", "ShellSurface::SetParent", "parent",
               parent ? base::UTF16ToASCII(parent->GetWindowTitle()) : "null");

  SetParentWindow(parent ? parent->GetWidget()->GetNativeWindow() : nullptr);
}

bool ShellSurface::CanMaximize() const {
  // Prevent non-resizable windows being resized via maximize.
  return ShellSurfaceBase::CanMaximize() && CanResize();
}

void ShellSurface::Maximize() {
  TRACE_EVENT0("exo", "ShellSurface::Maximize");

  if (!widget_) {
    if (initial_show_state_ != ui::SHOW_STATE_FULLSCREEN ||
        ShouldExitFullscreenFromRestoreOrMaximized())
      initial_show_state_ = ui::SHOW_STATE_MAXIMIZED;
    return;
  }

  if (!widget_->IsFullscreen() ||
      ShouldExitFullscreenFromRestoreOrMaximized()) {
    // Note: This will ask client to configure its surface even if already
    // maximized.
    ScopedConfigure scoped_configure(this, true);
    widget_->Maximize();
  }
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
    if (initial_show_state_ != ui::SHOW_STATE_FULLSCREEN ||
        ShouldExitFullscreenFromRestoreOrMaximized())
      initial_show_state_ = ui::SHOW_STATE_NORMAL;
    return;
  }

  if (!widget_->IsFullscreen() ||
      ShouldExitFullscreenFromRestoreOrMaximized()) {
    // Note: This will ask client to configure its surface even if already
    // maximized.
    ScopedConfigure scoped_configure(this, true);
    widget_->Restore();
  }
}

void ShellSurface::SetFullscreen(bool fullscreen) {
  TRACE_EVENT1("exo", "ShellSurface::SetFullscreen", "fullscreen", fullscreen);

  if (!widget_) {
    if (fullscreen) {
      initial_show_state_ = ui::SHOW_STATE_FULLSCREEN;
    } else if (initial_show_state_ == ui::SHOW_STATE_FULLSCREEN) {
      initial_show_state_ = ui::SHOW_STATE_DEFAULT;
    }
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

void ShellSurface::AddObserver(ShellSurfaceObserver* observer) {
  observers_.AddObserver(observer);
}

void ShellSurface::RemoveObserver(ShellSurfaceObserver* observer) {
  observers_.RemoveObserver(observer);
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceDelegate overrides:

void ShellSurface::OnSetFrame(SurfaceFrameType type) {
  ShellSurfaceBase::OnSetFrame(type);

  if (!widget_)
    return;
  widget_->GetNativeWindow()->SetProperty(
      aura::client::kUseWindowBoundsForShadow,
      frame_type_ != SurfaceFrameType::SHADOW);
}

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
      base::AutoReset<bool> notify_bounds_changes(&notify_bounds_changes_,
                                                  false);
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

absl::optional<gfx::Rect> ShellSurface::GetWidgetBounds() const {
  // Defer if configure requests are pending.
  if (!pending_configs_.empty() || scoped_configure_)
    return absl::nullopt;

  gfx::Rect new_widget_bounds = GetWidgetBoundsFromVisibleBounds();

  if (movement_disabled_) {
    new_widget_bounds.set_origin(origin_);
  } else if (IsMoveComponent(resize_component_)) {
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
  DCHECK(!movement_disabled_ || IsMoveComponent(resize_component_));
  gfx::Rect visible_bounds = GetVisibleBounds();
  gfx::Rect client_bounds = GetClientViewBounds();

  switch (resize_component_) {
    case HTCAPTION:
    case HTCLIENT:
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
      NOTREACHED() << "Unsupported component:" << resize_component_;
      return gfx::Point();
  }
}

void ShellSurface::SetUseImmersiveForFullscreen(bool value) {
  ShellSurfaceBase::SetUseImmersiveForFullscreen(value);
  // Ensure that the widget has been created before attempting to configure it.
  // Otherwise, the positioning of the window could be undefined.
  if (widget_)
    Configure();
}

////////////////////////////////////////////////////////////////////////////////
// aura::WindowObserver overrides:

void ShellSurface::OnWindowBoundsChanged(aura::Window* window,
                                         const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds,
                                         ui::PropertyChangeReason reason) {
  if (!root_surface() || !notify_bounds_changes_) {
    return;
  }
  if (IsShellSurfaceWindow(window)) {
    auto* window_state = ash::WindowState::Get(window);
    if (window_state && window_state->is_moving_to_another_display()) {
      old_screen_bounds_for_pending_move_ = old_bounds;
      wm::ConvertRectToScreen(window->parent(),
                              &old_screen_bounds_for_pending_move_);
      return;
    }

    if (new_bounds.size() == old_bounds.size()) {
      if (!origin_change_callback_.is_null())
        origin_change_callback_.Run(GetClientBoundsInScreen(widget_).origin());
      return;
    }

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

    // A window state change will send a configuration event. Avoid sending
    // two configuration events for the same change.
    if (!window_state_is_changing_)
      Configure();
  }
}

void ShellSurface::OnWindowAddedToRootWindow(aura::Window* window) {
  ShellSurfaceBase::OnWindowAddedToRootWindow(window);
  if (!IsShellSurfaceWindow(window)) {
    return;
  }
  auto* window_state = ash::WindowState::Get(window);
  if (window_state && window_state->is_moving_to_another_display() &&
      !old_screen_bounds_for_pending_move_.IsEmpty()) {
    gfx::Rect new_bounds_in_screen = window->bounds();
    wm::ConvertRectToScreen(window->parent(), &new_bounds_in_screen);

    gfx::Vector2d delta = new_bounds_in_screen.origin() -
                          old_screen_bounds_for_pending_move_.origin();
    old_screen_bounds_for_pending_move_ = gfx::Rect();
    origin_offset_ -= delta;
    pending_origin_offset_accumulator_ += delta;
    UpdateSurfaceBounds();
    UpdateShadow();

    if (!window_state_is_changing_)
      Configure();

  } else {
    if (!origin_change_callback_.is_null())
      origin_change_callback_.Run(GetClientBoundsInScreen(widget_).origin());
  }
}

void ShellSurface::OnWindowPropertyChanged(aura::Window* window,
                                           const void* key,
                                           intptr_t old_value) {
  ShellSurfaceBase::OnWindowPropertyChanged(window, key, old_value);
  if (IsShellSurfaceWindow(window)) {
    if (key == aura::client::kRasterScale) {
      float raster_scale = window->GetProperty(aura::client::kRasterScale);

      if (raster_scale == pending_raster_scale_) {
        return;
      }

      // We need to wait until raster scale changes are acked by the client. For
      // example, upon entering overview mode, updating the raster scale of
      // clients is meant to reduce buffer sizes and improve the smoothness of
      // the overview enter animation. But, if we don't wait for these updated
      // buffers, we will end up animating with unnecessarily large buffers,
      // which negates the entire point of updating the raster scale. So, lock
      // the compositor until we get an ack for updating the raster scale.
      if (!configure_callback_.is_null()) {
        ui::Compositor* compositor =
            widget_->GetNativeWindow()->layer()->GetCompositor();
        configure_compositor_lock_ = compositor->GetCompositorLock(
            nullptr, base::Milliseconds(kDefaultCompositorLockTimeoutMs));
      }
      pending_raster_scale_ = raster_scale;

      Configure();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// ash::WindowStateObserver overrides:

void ShellSurface::OnPreWindowStateTypeChange(
    ash::WindowState* window_state,
    chromeos::WindowStateType old_type) {
  window_state_is_changing_ = true;
  chromeos::WindowStateType new_type = window_state->GetStateType();
  if (chromeos::IsMinimizedWindowStateType(old_type) ||
      chromeos::IsMinimizedWindowStateType(new_type)) {
    return;
  }

  if (chromeos::IsMaximizedOrFullscreenOrPinnedWindowStateType(old_type) ||
      chromeos::IsMaximizedOrFullscreenOrPinnedWindowStateType(new_type)) {
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
          nullptr, base::Milliseconds(kDefaultCompositorLockTimeoutMs));
    } else {
      animations_disabler_ = std::make_unique<ash::ScopedAnimationDisabler>(
          widget_->GetNativeWindow());
    }
  }
}

void ShellSurface::OnPostWindowStateTypeChange(
    ash::WindowState* window_state,
    chromeos::WindowStateType old_type) {
  // Send the new state to the exo-client when the state changes. This is
  // important for client presentation. For example exo-client using client-side
  // decoration, window-state information is needed to toggle the maximize and
  // restore buttons. When the window is restored, we show a maximized button;
  // otherwise we show a restore button.
  //
  // Note that configuration events on bounds change is suppressed during state
  // change, because it is assumed that a configuration event will always be
  // sent at the end of a state change.
  Configure();

  if (widget_) {
    UpdateWidgetBounds();
    UpdateShadow();
  }

  if (root_surface() && window_state->GetStateType() != old_type &&
      (window_state->GetStateType() == chromeos::WindowStateType::kFullscreen ||
       old_type == chromeos::WindowStateType::kFullscreen)) {
    root_surface()->OnFullscreenStateChanged(window_state->IsFullscreen());
  }

  // Re-enable animations if they were disabled in pre state change handler.
  animations_disabler_.reset();
  window_state_is_changing_ = false;
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

gfx::Rect ShellSurface::ComputeAdjustedBounds(const gfx::Rect& bounds) const {
  DCHECK(widget_);
  auto min_size = widget_->GetMinimumSize();
  auto max_size = widget_->GetMaximumSize();
  gfx::Size size = bounds.size();
  // use `minimum_size_` as the GetMinimumSize always return min size
  // bigger or equal to 1x1.
  if (!minimum_size_.IsEmpty() && !min_size.IsEmpty()) {
    size.SetToMax(min_size);
  }
  if (!max_size.IsEmpty()) {
    size.SetToMin(max_size);
  }
  // Keep the origin instead of center.
  return gfx::Rect(bounds.origin(), size);
}

void ShellSurface::SetWidgetBounds(const gfx::Rect& bounds,
                                   bool adjusted_by_server) {
  if (bounds == widget_->GetWindowBoundsInScreen() && !adjusted_by_server)
    return;

  // Set |notify_bounds_changes_| as this change to window bounds
  // should not result in a configure request unless the bounds is modified by
  // the server.
  DCHECK(notify_bounds_changes_);
  notify_bounds_changes_ = adjusted_by_server;

  if (IsDragged()) {
    // Do not move the root window.
    auto* window = widget_->GetNativeWindow();
    auto* screen_position_client =
        aura::client::GetScreenPositionClient(window->GetRootWindow());
    gfx::PointF origin(bounds.origin());
    screen_position_client->ConvertPointFromScreen(window->parent(), &origin);
    widget_->GetNativeWindow()->SetBounds(
        gfx::Rect(origin.x(), origin.y(), bounds.width(), bounds.height()));
  } else {
    widget_->SetBounds(bounds);
  }
  UpdateSurfaceBounds();

  notify_bounds_changes_ = true;
}

bool ShellSurface::OnPreWidgetCommit() {
  if (!widget_ && GetEnabled()) {
    // Defer widget creation and commit until surface has contents.
    if (host_window()->bounds().IsEmpty() &&
        root_surface()->surface_hierarchy_content_bounds().IsEmpty()) {
      Configure();

      if (initial_show_state_ != ui::SHOW_STATE_MINIMIZED)
        needs_layout_on_show_ = true;
    }

    CreateShellSurfaceWidget(initial_show_state_);
  }

  // Apply the accumulated pending origin offset to reflect acknowledged
  // configure requests.
  origin_offset_ += pending_origin_offset_;
  pending_origin_offset_ = gfx::Vector2d();

  // Update resize direction to reflect acknowledged configure requests.
  resize_component_ = pending_resize_component_;

  config_waiting_for_commit_.reset();

  return true;
}

std::unique_ptr<views::NonClientFrameView>
ShellSurface::CreateNonClientFrameView(views::Widget* widget) {
  ash::WindowState* window_state =
      ash::WindowState::Get(widget->GetNativeWindow());
  window_state->SetDelegate(std::make_unique<CustomWindowStateDelegate>(this));
  return CreateNonClientFrameViewInternal(widget);
}

////////////////////////////////////////////////////////////////////////////////
// ShellSurface, private:

void ShellSurface::SetParentWindow(aura::Window* new_parent) {
  if (new_parent && GetWidget() &&
      new_parent == GetWidget()->GetNativeWindow()) {
    // Some apps e.g. crbug/1210235 try to be their own parent. Ignore them to
    // prevent chrome from locking up/crashing.
    auto* app_id = GetShellApplicationId(host_window());
    LOG(WARNING)
        << "Client attempts to add itself as a transient parent: app_id="
        << app_id;
    return;
  }
  if (parent()) {
    parent()->RemoveObserver(this);
    if (widget_) {
      aura::Window* child_window = widget_->GetNativeWindow();
      wm::TransientWindowManager::GetOrCreate(child_window)
          ->set_parent_controls_visibility(false);
      wm::RemoveTransientChild(parent(), child_window);
    }
  }
  SetParentInternal(new_parent);
  if (parent()) {
    parent()->AddObserver(this);
    MaybeMakeTransient();
  }
}

void ShellSurface::MaybeMakeTransient() {
  if (!parent() || !widget_)
    return;
  aura::Window* child_window = widget_->GetNativeWindow();
  wm::AddTransientChild(parent(), child_window);
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
      serial = configure_callback_.Run(GetClientBoundsInScreen(widget_),
                                       window_state->GetStateType(),
                                       IsResizing(), widget_->IsActive(),
                                       origin_offset, pending_raster_scale_);
    } else {
      gfx::Rect bounds;
      if (initial_bounds_)
        bounds.set_origin(initial_bounds_->origin());
      serial = configure_callback_.Run(
          bounds, chromeos::WindowStateType::kNormal, false, false,
          origin_offset, pending_raster_scale_);
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

  for (auto& observer : observers_)
    observer.OnConfigure(serial);
}

bool ShellSurface::GetCanResizeFromSizeConstraints() const {
  // Both the default min and max sizes are empty and windows must be resizable
  // in that case.
  return (minimum_size_.IsEmpty() || minimum_size_ != maximum_size_);
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

  if (gesture_target) {
    gfx::PointF location = toplevel_handler->event_location_in_gesture_target();
    aura::Window::ConvertPointToTarget(
        gesture_target, widget_->GetNativeWindow()->GetRootWindow(), &location);
    toplevel_handler->AttemptToStartDrag(target, location, component, {});
  } else {
    gfx::Point location = aura::Env::GetInstance()->last_mouse_location();
    ::wm::ConvertPointFromScreen(widget_->GetNativeWindow()->GetRootWindow(),
                                 &location);
    toplevel_handler->AttemptToStartDrag(target, gfx::PointF(location),
                                         component, {});
  }
  // Notify client that resizing state has changed.
  if (IsResizing())
    Configure();
}

void ShellSurface::EndDrag() {
  if (!IsMoveComponent(resize_component_))
    Configure(/*ends_drag=*/true);
}

}  // namespace exo
