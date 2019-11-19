// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/client_controlled_shell_surface.h"

#include <map>
#include <utility>

#include "ash/frame/header_view.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/frame/wide_frame_view.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/caption_buttons/caption_button_model.h"
#include "ash/public/cpp/default_frame_header.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller.h"
#include "ash/public/cpp/rounded_corner_decorator.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_pin_type.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/cpp/window_state_type.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/client_controlled_state.h"
#include "ash/wm/drag_details.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/scoped_window_event_targeting_blocker.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/class_property.h"
#include "ui/compositor/compositor_lock.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"

namespace exo {

namespace {

// Client controlled specific accelerators.
const struct {
  ui::KeyboardCode keycode;
  int modifiers;
  ClientControlledAcceleratorAction action;
} kAccelerators[] = {
    {ui::VKEY_OEM_MINUS, ui::EF_CONTROL_DOWN,
     ClientControlledAcceleratorAction::ZOOM_OUT},
    {ui::VKEY_OEM_PLUS, ui::EF_CONTROL_DOWN,
     ClientControlledAcceleratorAction::ZOOM_IN},
    {ui::VKEY_0, ui::EF_CONTROL_DOWN,
     ClientControlledAcceleratorAction::ZOOM_RESET},
};

ClientControlledShellSurface::DelegateFactoryCallback& GetFactoryForTesting() {
  using CallbackType = ClientControlledShellSurface::DelegateFactoryCallback;
  static base::NoDestructor<CallbackType> factory;
  return *factory;
}

// Maximum amount of time to wait for contents that match the display's
// orientation in tablet mode.
// TODO(oshima): Looks like android is generating unnecessary frames.
// Fix it on Android side and reduce the timeout.
constexpr int kOrientationLockTimeoutMs = 2500;

Orientation SizeToOrientation(const gfx::Size& size) {
  DCHECK_NE(size.width(), size.height());
  return size.width() > size.height() ? Orientation::LANDSCAPE
                                      : Orientation::PORTRAIT;
}

// A ClientControlledStateDelegate that sends the state/bounds
// change request to exo client.
class ClientControlledStateDelegate
    : public ash::ClientControlledState::Delegate {
 public:
  explicit ClientControlledStateDelegate(
      ClientControlledShellSurface* shell_surface)
      : shell_surface_(shell_surface) {}
  ~ClientControlledStateDelegate() override {}

  // Overridden from ash::ClientControlledState::Delegate:
  void HandleWindowStateRequest(ash::WindowState* window_state,
                                ash::WindowStateType next_state) override {
    shell_surface_->OnWindowStateChangeEvent(window_state->GetStateType(),
                                             next_state);
  }
  void HandleBoundsRequest(ash::WindowState* window_state,
                           ash::WindowStateType requested_state,
                           const gfx::Rect& bounds_in_display,
                           int64_t display_id) override {
    shell_surface_->OnBoundsChangeEvent(
        window_state->GetStateType(), requested_state, display_id,
        bounds_in_display,
        window_state->drag_details() && shell_surface_->IsDragging()
            ? window_state->drag_details()->bounds_change
            : 0);
  }

 private:
  ClientControlledShellSurface* shell_surface_;

  DISALLOW_COPY_AND_ASSIGN(ClientControlledStateDelegate);
};

// A WindowStateDelegate that implements ToggleFullscreen behavior for
// client controlled window.
class ClientControlledWindowStateDelegate : public ash::WindowStateDelegate {
 public:
  explicit ClientControlledWindowStateDelegate(
      ClientControlledShellSurface* shell_surface,
      ash::ClientControlledState::Delegate* delegate)
      : shell_surface_(shell_surface), delegate_(delegate) {}
  ~ClientControlledWindowStateDelegate() override {}

  // Overridden from ash::WindowStateDelegate:
  bool ToggleFullscreen(ash::WindowState* window_state) override {
    ash::WindowStateType next_state;
    aura::Window* window = window_state->window();
    switch (window_state->GetStateType()) {
      case ash::WindowStateType::kDefault:
      case ash::WindowStateType::kNormal:
        window->SetProperty(aura::client::kPreFullscreenShowStateKey,
                            ui::SHOW_STATE_NORMAL);
        next_state = ash::WindowStateType::kFullscreen;
        break;
      case ash::WindowStateType::kMaximized:
        window->SetProperty(aura::client::kPreFullscreenShowStateKey,
                            ui::SHOW_STATE_MAXIMIZED);
        next_state = ash::WindowStateType::kFullscreen;
        break;
      case ash::WindowStateType::kFullscreen:
        switch (window->GetProperty(aura::client::kPreFullscreenShowStateKey)) {
          case ui::SHOW_STATE_DEFAULT:
          case ui::SHOW_STATE_NORMAL:
            next_state = ash::WindowStateType::kNormal;
            break;
          case ui::SHOW_STATE_MAXIMIZED:
            next_state = ash::WindowStateType::kMaximized;
            break;
          case ui::SHOW_STATE_MINIMIZED:
            next_state = ash::WindowStateType::kMinimized;
            break;
          case ui::SHOW_STATE_FULLSCREEN:
          case ui::SHOW_STATE_INACTIVE:
          case ui::SHOW_STATE_END:
            NOTREACHED() << " unknown state :"
                         << window->GetProperty(
                                aura::client::kPreFullscreenShowStateKey);
            return false;
        }
        break;
      case ash::WindowStateType::kMinimized: {
        ui::WindowShowState pre_full_state =
            window->GetProperty(aura::client::kPreMinimizedShowStateKey);
        if (pre_full_state != ui::SHOW_STATE_FULLSCREEN) {
          window->SetProperty(aura::client::kPreFullscreenShowStateKey,
                              pre_full_state);
        }
        next_state = ash::WindowStateType::kFullscreen;
        break;
      }
      default:
        // TODO(oshima|xdai): Handle SNAP state.
        return false;
    }
    delegate_->HandleWindowStateRequest(window_state, next_state);
    return true;
  }

  void OnDragStarted(int component) override {
    shell_surface_->OnDragStarted(component);
  }

  void OnDragFinished(bool canceled, const gfx::Point& location) override {
    shell_surface_->OnDragFinished(canceled, location);
  }

 private:
  ClientControlledShellSurface* shell_surface_;
  ash::ClientControlledState::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ClientControlledWindowStateDelegate);
};

bool IsPinned(const ash::WindowState* window_state) {
  return window_state->IsPinned() || window_state->IsTrustedPinned();
}

class CaptionButtonModel : public ash::CaptionButtonModel {
 public:
  CaptionButtonModel(uint32_t visible_button_mask, uint32_t enabled_button_mask)
      : visible_button_mask_(visible_button_mask),
        enabled_button_mask_(enabled_button_mask) {}

  // Overridden from ash::CaptionButtonModel:
  bool IsVisible(views::CaptionButtonIcon icon) const override {
    return visible_button_mask_ & (1 << icon);
  }
  bool IsEnabled(views::CaptionButtonIcon icon) const override {
    return enabled_button_mask_ & (1 << icon);
  }
  bool InZoomMode() const override {
    return visible_button_mask_ & (1 << views::CAPTION_BUTTON_ICON_ZOOM);
  }

 private:
  uint32_t visible_button_mask_;
  uint32_t enabled_button_mask_;

  DISALLOW_COPY_AND_ASSIGN(CaptionButtonModel);
};

// EventTargetingBlocker blocks the event targeting by setting NONE targeting
// policy to the window subtrees. It resets to the original policy upon
// deletion.
class EventTargetingBlocker : aura::WindowObserver {
 public:
  EventTargetingBlocker() = default;

  ~EventTargetingBlocker() override {
    if (window_)
      Unregister(window_);
  }

  void Block(aura::Window* window) {
    window_ = window;
    Register(window);
  }

 private:
  void Register(aura::Window* window) {
    window->AddObserver(this);
    event_targeting_blocker_map_[window] =
        std::make_unique<aura::ScopedWindowEventTargetingBlocker>(window);
    for (auto* child : window->children())
      Register(child);
  }

  void Unregister(aura::Window* window) {
    window->RemoveObserver(this);
    event_targeting_blocker_map_.erase(window);
    for (auto* child : window->children())
      Unregister(child);
  }

  void OnWindowDestroying(aura::Window* window) override {
    Unregister(window);
    if (window_ == window)
      window_ = nullptr;
  }

  std::map<aura::Window*,
           std::unique_ptr<aura::ScopedWindowEventTargetingBlocker>>
      event_targeting_blocker_map_;
  aura::Window* window_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(EventTargetingBlocker);
};

}  // namespace

class ClientControlledShellSurface::ScopedSetBoundsLocally {
 public:
  explicit ScopedSetBoundsLocally(ClientControlledShellSurface* shell_surface)
      : state_(shell_surface->client_controlled_state_) {
    state_->set_bounds_locally(true);
  }
  ~ScopedSetBoundsLocally() { state_->set_bounds_locally(false); }

 private:
  ash::ClientControlledState* const state_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSetBoundsLocally);
};

class ClientControlledShellSurface::ScopedLockedToRoot {
 public:
  explicit ScopedLockedToRoot(views::Widget* widget)
      : window_(widget->GetNativeWindow()) {
    window_->SetProperty(ash::kLockedToRootKey, true);
  }
  ~ScopedLockedToRoot() { window_->ClearProperty(ash::kLockedToRootKey); }

 private:
  aura::Window* const window_;

  DISALLOW_COPY_AND_ASSIGN(ScopedLockedToRoot);
};

////////////////////////////////////////////////////////////////////////////////
// ClientControlledShellSurface, public:

ClientControlledShellSurface::ClientControlledShellSurface(Surface* surface,
                                                           bool can_minimize,
                                                           int container)
    : ShellSurfaceBase(surface, gfx::Point(), true, can_minimize, container),
      current_pin_(ash::WindowPinType::kNone) {
  display::Screen::GetScreen()->AddObserver(this);
}

ClientControlledShellSurface::~ClientControlledShellSurface() {
  // Reset the window delegate here so that we won't try to do any dragging
  // operation on a to-be-destroyed window. |widget_| can be nullptr in tests.
  if (GetWidget())
    GetWindowState()->SetDelegate(nullptr);
  if (client_controlled_state_)
    client_controlled_state_->ResetDelegate();
  wide_frame_.reset();
  display::Screen::GetScreen()->RemoveObserver(this);
  if (current_pin_ != ash::WindowPinType::kNone)
    SetPinned(ash::WindowPinType::kNone);
}

void ClientControlledShellSurface::SetBounds(int64_t display_id,
                                             const gfx::Rect& bounds) {
  TRACE_EVENT2("exo", "ClientControlledShellSurface::SetBounds", "display_id",
               display_id, "bounds", bounds.ToString());

  if (bounds.IsEmpty()) {
    DLOG(WARNING) << "Bounds must be non-empty";
    return;
  }

  SetDisplay(display_id);
  SetGeometry(bounds);
}

void ClientControlledShellSurface::SetMaximized() {
  TRACE_EVENT0("exo", "ClientControlledShellSurface::SetMaximized");
  pending_window_state_ = ash::WindowStateType::kMaximized;
}

void ClientControlledShellSurface::SetMinimized() {
  TRACE_EVENT0("exo", "ClientControlledShellSurface::SetMinimized");
  pending_window_state_ = ash::WindowStateType::kMinimized;
}

void ClientControlledShellSurface::SetRestored() {
  TRACE_EVENT0("exo", "ClientControlledShellSurface::SetRestored");
  pending_window_state_ = ash::WindowStateType::kNormal;
}

void ClientControlledShellSurface::SetFullscreen(bool fullscreen) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetFullscreen",
               "fullscreen", fullscreen);
  pending_window_state_ = fullscreen ? ash::WindowStateType::kFullscreen
                                     : ash::WindowStateType::kNormal;
}

void ClientControlledShellSurface::SetSnappedToLeft() {
  TRACE_EVENT0("exo", "ClientControlledShellSurface::SetSnappedToLeft");
  pending_window_state_ = ash::WindowStateType::kLeftSnapped;
}

void ClientControlledShellSurface::SetSnappedToRight() {
  TRACE_EVENT0("exo", "ClientControlledShellSurface::SetSnappedToRight");
  pending_window_state_ = ash::WindowStateType::kRightSnapped;
}

void ClientControlledShellSurface::SetPip() {
  TRACE_EVENT0("exo", "ClientControlledShellSurface::SetPip");
  pending_window_state_ = ash::WindowStateType::kPip;
}

void ClientControlledShellSurface::SetPinned(ash::WindowPinType type) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetPinned", "type",
               static_cast<int>(type));

  if (!widget_)
    CreateShellSurfaceWidget(ui::SHOW_STATE_NORMAL);

  widget_->GetNativeWindow()->SetProperty(ash::kWindowPinTypeKey, type);
  current_pin_ = type;
}

void ClientControlledShellSurface::SetSystemUiVisibility(bool autohide) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetSystemUiVisibility",
               "autohide", autohide);

  if (!widget_)
    CreateShellSurfaceWidget(ui::SHOW_STATE_NORMAL);

  ash::window_util::SetAutoHideShelf(widget_->GetNativeWindow(), autohide);
}

void ClientControlledShellSurface::SetAlwaysOnTop(bool always_on_top) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetAlwaysOnTop",
               "always_on_top", always_on_top);
  pending_always_on_top_ = always_on_top;
}

void ClientControlledShellSurface::SetImeBlocked(bool ime_blocked) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetImeBlocked",
               "ime_blocked", ime_blocked);

  if (!widget_)
    CreateShellSurfaceWidget(ui::SHOW_STATE_NORMAL);

  WMHelper::GetInstance()->SetImeBlocked(widget_->GetNativeWindow(),
                                         ime_blocked);
}

void ClientControlledShellSurface::SetOrientation(Orientation orientation) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetOrientation",
               "orientation",
               orientation == Orientation::PORTRAIT ? "portrait" : "landscape");
  pending_orientation_ = orientation;
}

void ClientControlledShellSurface::SetShadowBounds(const gfx::Rect& bounds) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetShadowBounds", "bounds",
               bounds.ToString());
  auto shadow_bounds =
      bounds.IsEmpty() ? base::nullopt : base::make_optional(bounds);
  if (shadow_bounds_ != shadow_bounds) {
    shadow_bounds_ = shadow_bounds;
    shadow_bounds_changed_ = true;
  }
}

void ClientControlledShellSurface::SetScale(double scale) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetScale", "scale", scale);

  if (scale <= 0.0) {
    DLOG(WARNING) << "Surface scale must be greater than 0";
    return;
  }

  pending_scale_ = scale;
}

void ClientControlledShellSurface::CommitPendingScale() {
  if (pending_scale_ != scale_) {
    gfx::Transform transform;
    DCHECK_NE(pending_scale_, 0.0);
    transform.Scale(1.0 / pending_scale_, 1.0 / pending_scale_);
    host_window()->SetTransform(transform);
    scale_ = pending_scale_;
  }
}

void ClientControlledShellSurface::SetTopInset(int height) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetTopInset", "height",
               height);

  pending_top_inset_height_ = height;
}

void ClientControlledShellSurface::SetResizeOutset(int outset) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetResizeOutset", "outset",
               outset);
  // Deprecated.
  NOTREACHED();
}

void ClientControlledShellSurface::OnWindowStateChangeEvent(
    ash::WindowStateType current_state,
    ash::WindowStateType next_state) {
  // Android already knows this state change. Don't send state change to Android
  // that it is about to do anyway.
  if (state_changed_callback_ && pending_window_state_ != next_state)
    state_changed_callback_.Run(current_state, next_state);
}

void ClientControlledShellSurface::StartDrag(int component,
                                             const gfx::Point& location) {
  TRACE_EVENT2("exo", "ClientControlledShellSurface::StartDrag", "component",
               component, "location", location.ToString());

  if (!widget_)
    return;
  AttemptToStartDrag(component, location);
}

void ClientControlledShellSurface::AttemptToStartDrag(
    int component,
    const gfx::Point& location) {
  aura::Window* target = widget_->GetNativeWindow();
  ash::ToplevelWindowEventHandler* toplevel_handler =
      ash::Shell::Get()->toplevel_window_event_handler();
  aura::Window* mouse_pressed_handler =
      target->GetHost()->dispatcher()->mouse_pressed_handler();
  // Start dragging only if ...
  // 1) touch guesture is in progres.
  // 2) mouse was pressed on the target or its subsurfaces.
  if (toplevel_handler->gesture_target() ||
      (mouse_pressed_handler && target->Contains(mouse_pressed_handler))) {
    gfx::Point point_in_root(location);
    wm::ConvertPointFromScreen(target->GetRootWindow(), &point_in_root);
    toplevel_handler->AttemptToStartDrag(
        target, point_in_root, component,
        ash::ToplevelWindowEventHandler::EndClosure());
  }
}

bool ClientControlledShellSurface::IsDragging() {
  return in_drag_;
}

void ClientControlledShellSurface::SetCanMaximize(bool can_maximize) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetCanMaximize",
               "can_maximzie", can_maximize);
  can_maximize_ = can_maximize;
  if (widget_)
    widget_->OnSizeConstraintsChanged();
}

void ClientControlledShellSurface::UpdateAutoHideFrame() {
  if (immersive_fullscreen_controller_) {
    bool enabled = (frame_type_ == SurfaceFrameType::AUTOHIDE &&
                    (GetWindowState()->IsMaximizedOrFullscreenOrPinned() ||
                     GetWindowState()->IsSnapped()));
    ash::ImmersiveFullscreenController::EnableForWidget(widget_, enabled);
  }
}

void ClientControlledShellSurface::SetFrameButtons(
    uint32_t visible_button_mask,
    uint32_t enabled_button_mask) {
  if (frame_visible_button_mask_ == visible_button_mask &&
      frame_enabled_button_mask_ == enabled_button_mask) {
    return;
  }
  frame_visible_button_mask_ = visible_button_mask;
  frame_enabled_button_mask_ = enabled_button_mask;

  if (widget_)
    UpdateCaptionButtonModel();
}

void ClientControlledShellSurface::SetExtraTitle(
    const base::string16& extra_title) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetExtraTitle",
               "extra_title", base::UTF16ToUTF8(extra_title));

  if (!widget_)
    return;

  GetFrameView()->GetHeaderView()->GetFrameHeader()->SetFrameTextOverride(
      extra_title);
  if (wide_frame_) {
    wide_frame_->header_view()->GetFrameHeader()->SetFrameTextOverride(
        extra_title);
  }
}

void ClientControlledShellSurface::SetOrientationLock(
    ash::OrientationLockType orientation_lock) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetOrientationLock",
               "orientation_lock", static_cast<int>(orientation_lock));

  if (!widget_) {
    initial_orientation_lock_ = orientation_lock;
    return;
  }

  ash::Shell* shell = ash::Shell::Get();
  shell->screen_orientation_controller()->LockOrientationForWindow(
      widget_->GetNativeWindow(), orientation_lock);
}

void ClientControlledShellSurface::OnBoundsChangeEvent(
    ash::WindowStateType current_state,
    ash::WindowStateType requested_state,
    int64_t display_id,
    const gfx::Rect& window_bounds,
    int bounds_change) {
  if (ignore_bounds_change_request_)
    return;
  // 1) Do no update the bounds unless we have geometry from client.
  // 2) Do not update the bounds if window is minimized unless it
  // exiting the minimzied state.
  // The bounds will be provided by client when unminimized.
  if (!geometry().IsEmpty() && !window_bounds.IsEmpty() &&
      (!widget_->IsMinimized() ||
       requested_state != ash::WindowStateType::kMinimized) &&
      bounds_changed_callback_) {
    // Sends the client bounds, which matches the geometry
    // when frame is enabled.
    ash::NonClientFrameViewAsh* frame_view = GetFrameView();

    // Snapped window states in tablet mode do not include the caption height.
    const bool becoming_snapped =
        requested_state == ash::WindowStateType::kLeftSnapped ||
        requested_state == ash::WindowStateType::kRightSnapped;
    const bool is_tablet_mode = WMHelper::GetInstance()->InTabletMode();
    gfx::Rect client_bounds =
        becoming_snapped && is_tablet_mode
            ? window_bounds
            : frame_view->GetClientBoundsForWindowBounds(window_bounds);
    gfx::Size current_size = frame_view->GetBoundsForClientView().size();
    bool is_resize = client_bounds.size() != current_size &&
                     !widget_->IsMaximized() && !widget_->IsFullscreen();

    bounds_changed_callback_.Run(current_state, requested_state, display_id,
                                 client_bounds, is_resize, bounds_change);

    auto* window_state = GetWindowState();
    if (server_reparent_window_ &&
        window_state->GetDisplay().id() != display_id) {
      ScopedSetBoundsLocally scoped_set_bounds(this);
      int container_id = window_state->window()->parent()->id();
      aura::Window* new_parent =
          ash::Shell::GetRootWindowControllerWithDisplayId(display_id)
              ->GetContainer(container_id);
      new_parent->AddChild(window_state->window());
    }
  }
}

void ClientControlledShellSurface::ChangeZoomLevel(ZoomChange change) {
  if (change_zoom_level_callback_)
    change_zoom_level_callback_.Run(change);
}

void ClientControlledShellSurface::OnDragStarted(int component) {
  in_drag_ = true;
  if (drag_started_callback_)
    drag_started_callback_.Run(component);
}

void ClientControlledShellSurface::OnDragFinished(bool canceled,
                                                  const gfx::Point& location) {
  in_drag_ = false;
  if (drag_finished_callback_)
    drag_finished_callback_.Run(location.x(), location.y(), canceled);
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceDelegate overrides:

bool ClientControlledShellSurface::IsInputEnabled(Surface* surface) const {
  // Client-driven dragging/resizing relies on implicit grab, which ensures that
  // mouse/touch events are delivered to the focused surface until release, even
  // if they fall outside surface bounds. However, if the client destroys the
  // surface with implicit grab, the drag/resize is prematurely ended. Prevent
  // this by delivering all input events to the root surface, which shares the
  // lifetime of the shell surface.
  // TODO(domlaskowski): Remove once the client is provided with an API to hook
  // into server-driven dragging/resizing.
  return surface == root_surface();
}

void ClientControlledShellSurface::OnSetFrame(SurfaceFrameType type) {
  if (container_ == ash::kShellWindowId_SystemModalContainer &&
      type != SurfaceFrameType::NONE) {
    LOG(WARNING)
        << "A surface in system modal container should not have a frame:"
        << static_cast<int>(type);
    return;
  }

  // TODO(oshima): We shouldn't send the synthesized motion event when just
  // changing the frame type. The better solution would be to keep the window
  // position regardless of the frame state, but that won't be available until
  // next arc version.
  // This is a stopgap solution not to generate the event until it is resolved.
  EventTargetingBlocker blocker;
  bool suppress_mouse_event = frame_type_ != type && widget_;
  if (suppress_mouse_event)
    blocker.Block(widget_->GetNativeWindow());
  ShellSurfaceBase::OnSetFrame(type);
  UpdateAutoHideFrame();

  if (suppress_mouse_event)
    UpdateSurfaceBounds();
}

void ClientControlledShellSurface::OnSetFrameColors(SkColor active_color,
                                                    SkColor inactive_color) {
  ShellSurfaceBase::OnSetFrameColors(active_color, inactive_color);
  if (wide_frame_) {
    aura::Window* window = wide_frame_->GetWidget()->GetNativeWindow();
    window->SetProperty(ash::kFrameActiveColorKey, active_color);
    window->SetProperty(ash::kFrameInactiveColorKey, inactive_color);
  }
}

////////////////////////////////////////////////////////////////////////////////
// aura::WindowObserver overrides:

void ClientControlledShellSurface::OnWindowAddedToRootWindow(
    aura::Window* window) {
  // Window dragging across display moves the window to target display when
  // dropped, but the actual window bounds comes later from android.  Update the
  // window bounds now so that the window stays where it is expected to be. (it
  // may still move if the android sends different bounds).
  if (client_controlled_state_->set_bounds_locally() ||
      !GetWindowState()->is_dragged()) {
    return;
  }

  ScopedLockedToRoot scoped_locked_to_root(widget_);
  UpdateWidgetBounds();
}

////////////////////////////////////////////////////////////////////////////////
// views::WidgetDelegate overrides:

bool ClientControlledShellSurface::CanMaximize() const {
  return can_maximize_;
}

views::NonClientFrameView*
ClientControlledShellSurface::CreateNonClientFrameView(views::Widget* widget) {
  ash::WindowState* window_state = GetWindowState();
  std::unique_ptr<ash::ClientControlledState::Delegate> delegate =
      GetFactoryForTesting()
          ? GetFactoryForTesting().Run()
          : std::make_unique<ClientControlledStateDelegate>(this);

  auto window_delegate = std::make_unique<ClientControlledWindowStateDelegate>(
      this, delegate.get());
  auto state =
      std::make_unique<ash::ClientControlledState>(std::move(delegate));
  client_controlled_state_ = state.get();
  window_state->SetStateObject(std::move(state));
  window_state->SetDelegate(std::move(window_delegate));
  ash::NonClientFrameViewAsh* frame_view =
      static_cast<ash::NonClientFrameViewAsh*>(
          CreateNonClientFrameViewInternal(widget, /*client_controlled=*/true));
  immersive_fullscreen_controller_ =
      std::make_unique<ash::ImmersiveFullscreenController>();
  frame_view->InitImmersiveFullscreenControllerForView(
      immersive_fullscreen_controller_.get());
  return frame_view;
}

void ClientControlledShellSurface::SaveWindowPlacement(
    const gfx::Rect& bounds,
    ui::WindowShowState show_state) {}

bool ClientControlledShellSurface::GetSavedWindowPlacement(
    const views::Widget* widget,
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// views::View overrides:

gfx::Size ClientControlledShellSurface::GetMaximumSize() const {
  if (can_maximize_) {
    // On ChromeOS, a window with non empty maximum size is non-maximizable,
    // even if CanMaximize() returns true. ClientControlledShellSurface
    // sololy depends on |can_maximize_| to determine if it is maximizable,
    // so just return empty size.
    return gfx::Size();
  } else {
    return ShellSurfaceBase::GetMaximumSize();
  }
}

void ClientControlledShellSurface::OnDeviceScaleFactorChanged(float old_dsf,
                                                              float new_dsf) {
  views::View::OnDeviceScaleFactorChanged(old_dsf, new_dsf);
  UpdateFrameWidth();
}

////////////////////////////////////////////////////////////////////////////////
// display::DisplayObserver overrides:

void ClientControlledShellSurface::OnDisplayMetricsChanged(
    const display::Display& new_display,
    uint32_t changed_metrics) {
  if (!widget_ || !widget_->IsActive() ||
      !WMHelper::GetInstance()->InTabletMode()) {
    return;
  }

  const display::Screen* screen = display::Screen::GetScreen();
  display::Display current_display =
      screen->GetDisplayNearestWindow(widget_->GetNativeWindow());
  if (current_display.id() != new_display.id() ||
      !(changed_metrics & display::DisplayObserver::DISPLAY_METRIC_ROTATION)) {
    return;
  }

  Orientation target_orientation = SizeToOrientation(new_display.size());
  if (orientation_ == target_orientation)
    return;
  expected_orientation_ = target_orientation;
  EnsureCompositorIsLockedForOrientationChange();
}

////////////////////////////////////////////////////////////////////////////////
// ui::CompositorLockClient overrides:

void ClientControlledShellSurface::CompositorLockTimedOut() {
  orientation_compositor_lock_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// ShellSurfaceBase overrides:

void ClientControlledShellSurface::SetWidgetBounds(const gfx::Rect& bounds) {
  const auto* screen = display::Screen::GetScreen();
  aura::Window* window = widget_->GetNativeWindow();
  display::Display current_display = screen->GetDisplayNearestWindow(window);

  bool is_display_move_pending = false;
  display::Display target_display = current_display;

  display::Display display;
  if (screen->GetDisplayWithDisplayId(display_id_, &display)) {
    bool is_display_stale = display_id_ != current_display.id();

    // Preserve widget bounds until client acknowledges display move.
    if (preserve_widget_bounds_ && is_display_stale)
      return;

    // True if the window has just been reparented to another root window, and
    // the move was initiated by the server.
    // TODO(oshima): Improve the window moving logic. https://crbug.com/875047
    is_display_move_pending =
        window->GetProperty(ash::kLockedToRootKey) && is_display_stale;

    if (!is_display_move_pending)
      target_display = display;

    preserve_widget_bounds_ = is_display_move_pending;
  } else {
    preserve_widget_bounds_ = false;
  }

  // Calculate a minimum window visibility required bounds.
  gfx::Rect adjusted_bounds = bounds;
  if (!is_display_move_pending) {
    ash::ClientControlledState::AdjustBoundsForMinimumWindowVisibility(
        target_display.bounds(), &adjusted_bounds);
  }

  if (adjusted_bounds == widget_->GetWindowBoundsInScreen() &&
      target_display.id() == current_display.id()) {
    return;
  }

  bool set_bounds_locally =
      GetWindowState()->is_dragged() && !is_display_move_pending;

  if (set_bounds_locally || client_controlled_state_->set_bounds_locally()) {
    // Convert from screen to display coordinates.
    gfx::Point origin = bounds.origin();
    wm::ConvertPointFromScreen(window->parent(), &origin);

    // Move the window relative to the current display.
    {
      ScopedSetBoundsLocally scoped_set_bounds(this);
      window->SetBounds(gfx::Rect(origin, adjusted_bounds.size()));
    }
    UpdateSurfaceBounds();
    return;
  }

  {
    ScopedSetBoundsLocally scoped_set_bounds(this);
    window->SetBoundsInScreen(adjusted_bounds, target_display);
  }

  if (bounds != adjusted_bounds || is_display_move_pending) {
    // Notify client that bounds were adjusted or window moved across displays.
    auto state_type = GetWindowState()->GetStateType();
    gfx::Rect adjusted_bounds_in_display(adjusted_bounds);

    adjusted_bounds_in_display.Offset(
        -target_display.bounds().OffsetFromOrigin());

    OnBoundsChangeEvent(state_type, state_type, target_display.id(),
                        adjusted_bounds_in_display, 0);
  }

  UpdateSurfaceBounds();
}

gfx::Rect ClientControlledShellSurface::GetShadowBounds() const {
  gfx::Rect shadow_bounds = ShellSurfaceBase::GetShadowBounds();
  const ash::NonClientFrameViewAsh* frame_view = GetFrameView();
  if (frame_view->GetVisible()) {
    // The client controlled geometry is only for the client
    // area. When the chrome side frame is enabled, the shadow height
    // has to include the height of the frame, and the total height is
    // equals to the window height computed by
    // |GetWindowBoundsForClientBounds|.
    shadow_bounds.set_size(
        frame_view->GetWindowBoundsForClientBounds(shadow_bounds).size());
  }

  return shadow_bounds;
}

void ClientControlledShellSurface::InitializeWindowState(
    ash::WindowState* window_state) {
  // Allow the client to request bounds that do not fill the entire work area
  // when maximized, or the entire display when fullscreen.
  window_state->set_allow_set_bounds_direct(true);
  window_state->set_ignore_keyboard_bounds_change(true);
  if (container_ == ash::kShellWindowId_SystemModalContainer ||
      container_ == ash::kShellWindowId_ArcVirtualKeyboardContainer) {
    DisableMovement();
  }
  ash::NonClientFrameViewAsh* frame_view = GetFrameView();
  frame_view->SetCaptionButtonModel(std::make_unique<CaptionButtonModel>(
      frame_visible_button_mask_, frame_enabled_button_mask_));
  UpdateAutoHideFrame();
  UpdateFrameWidth();
  if (initial_orientation_lock_ != ash::OrientationLockType::kAny)
    SetOrientationLock(initial_orientation_lock_);

  // Register Client controlled accelerators.
  views::FocusManager* focus_manager = widget_->GetFocusManager();
  accelerator_target_ =
      std::make_unique<ClientControlledAcceleratorTarget>(this);

  for (const auto& entry : kAccelerators) {
    focus_manager->RegisterAccelerator(
        ui::Accelerator(entry.keycode, entry.modifiers),
        ui::AcceleratorManager::kNormalPriority, accelerator_target_.get());
    accelerator_target_->RegisterAccelerator(
        ui::Accelerator(entry.keycode, entry.modifiers), entry.action);
  }
}

float ClientControlledShellSurface::GetScale() const {
  return scale_;
}

base::Optional<gfx::Rect> ClientControlledShellSurface::GetWidgetBounds()
    const {
  const ash::NonClientFrameViewAsh* frame_view = GetFrameView();
  if (frame_view->GetVisible()) {
    return frame_view->GetWindowBoundsForClientBounds(GetVisibleBounds());
  }

  return GetVisibleBounds();
}

gfx::Point ClientControlledShellSurface::GetSurfaceOrigin() const {
  return gfx::Point();
}

bool ClientControlledShellSurface::OnPreWidgetCommit() {
  if (!widget_) {
    // Modify the |origin_| to the |pending_geometry_| to place the window on
    // the intended display. See b/77472684 for details.
    // TODO(domlaskowski): Remove this once clients migrate to geometry API with
    // explicit target display.
    if (!pending_geometry_.IsEmpty())
      origin_ = pending_geometry_.origin();
    CreateShellSurfaceWidget(ash::ToWindowShowState(pending_window_state_));
  }

  ash::WindowState* window_state = GetWindowState();
  state_changed_ = window_state->GetStateType() != pending_window_state_;
  if (!state_changed_) {
    // Animate PIP window movement unless it is being dragged.
    client_controlled_state_->set_next_bounds_change_animation_type(
        window_state->IsPip() && !window_state->is_dragged()
            ? ash::ClientControlledState::kAnimationAnimated
            : ash::ClientControlledState::kAnimationNone);
    return true;
  }

  if (IsPinned(window_state)) {
    VLOG(1) << "State change was requested while pinned";
    return true;
  }

  auto animation_type = ash::ClientControlledState::kAnimationNone;
  switch (pending_window_state_) {
    case ash::WindowStateType::kNormal:
      if (widget_->IsMaximized() || widget_->IsFullscreen()) {
        animation_type = ash::ClientControlledState::kAnimationCrossFade;
      }
      break;

    case ash::WindowStateType::kMaximized:
    case ash::WindowStateType::kFullscreen:
      if (!window_state->IsPip())
        animation_type = ash::ClientControlledState::kAnimationCrossFade;
      break;

    default:
      break;
  }

  if (pending_window_state_ == ash::WindowStateType::kPip) {
    if (ash::features::IsPipRoundedCornersEnabled()) {
      decorator_ = std::make_unique<ash::RoundedCornerDecorator>(
          window_state->window(), host_window(), host_window()->layer(),
          ash::kPipRoundedCornerRadius);
    }
  } else {
    decorator_.reset();  // Remove rounded corners.
  }

  bool wasPip = window_state->IsPip();

  // As the bounds of the widget is updated later, ensure that no bounds change
  // happens with this state change (e.g. updatePipBounds can be triggered).
  base::AutoReset<bool> resetter(&ignore_bounds_change_request_, true);
  if (client_controlled_state_->EnterNextState(window_state,
                                               pending_window_state_)) {
    client_controlled_state_->set_next_bounds_change_animation_type(
        animation_type);
  }

  if (wasPip && !window_state->IsMinimized()) {
    // Expanding PIP should end tablet split view (see crbug.com/941788).
    // Clamshell split view does not require special handling. We activate the
    // PIP window, and so overview ends, which means clamshell split view ends.
    // TODO(edcourtney): Consider not ending tablet split view on PIP expand.
    // See crbug.com/950827.
    ash::SplitViewController* split_view_controller =
        ash::SplitViewController::Get(ash::Shell::GetPrimaryRootWindow());
    if (split_view_controller->InTabletSplitViewMode())
      split_view_controller->EndSplitView();
    // As Android doesn't activate PIP tasks after they are expanded, we need
    // to do it here explicitly.
    // TODO(937738): Investigate if we can activate PIP windows inside commit.
    window_state->Activate();
  }

  return true;
}

void ClientControlledShellSurface::OnPostWidgetCommit() {
  DCHECK(widget_);

  UpdateFrame();
  UpdateBackdrop();

  if (geometry_changed_callback_)
    geometry_changed_callback_.Run(GetVisibleBounds());

  // Apply new top inset height.
  if (pending_top_inset_height_ != top_inset_height_) {
    widget_->GetNativeWindow()->SetProperty(aura::client::kTopViewInset,
                                            pending_top_inset_height_);
    top_inset_height_ = pending_top_inset_height_;
  }

  // Update surface scale.
  CommitPendingScale();

  orientation_ = pending_orientation_;
  if (expected_orientation_ == orientation_)
    orientation_compositor_lock_.reset();

  widget_->GetNativeWindow()->SetProperty(aura::client::kZOrderingKey,
                                          pending_always_on_top_
                                          ? ui::ZOrderLevel::kFloatingWindow
                                          : ui::ZOrderLevel::kNormal);

}

void ClientControlledShellSurface::OnSurfaceDestroying(Surface* surface) {
  if (client_controlled_state_)
    client_controlled_state_->ResetDelegate();
  ShellSurfaceBase::OnSurfaceDestroying(surface);
}

////////////////////////////////////////////////////////////////////////////////
// ClientControlledShellSurface, private:

void ClientControlledShellSurface::UpdateFrame() {
  if (!widget_)
    return;
  gfx::Rect work_area =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(widget_->GetNativeWindow())
          .work_area();

  ash::WindowState* window_state = GetWindowState();
  bool enable_wide_frame = GetFrameView()->GetVisible() &&
                           window_state->IsMaximizedOrFullscreenOrPinned() &&
                           work_area.width() != geometry().width();
  bool update_frame = state_changed_;
  state_changed_ = false;
  if (enable_wide_frame) {
    if (!wide_frame_) {
      update_frame = true;
      wide_frame_ = std::make_unique<ash::WideFrameView>(widget_);
      ash::ImmersiveFullscreenController::EnableForWidget(widget_, false);
      wide_frame_->Init(immersive_fullscreen_controller_.get());
      wide_frame_->header_view()->GetFrameHeader()->SetFrameTextOverride(
          GetFrameView()
              ->GetHeaderView()
              ->GetFrameHeader()
              ->frame_text_override());
      wide_frame_->GetWidget()->Show();

      // Restoring window targeter replaced by ImmersiveFullscreenController.
      InstallCustomWindowTargeter();

      UpdateCaptionButtonModel();
    }
  } else {
    if (wide_frame_) {
      update_frame = true;
      ash::ImmersiveFullscreenController::EnableForWidget(widget_, false);
      wide_frame_.reset();
      GetFrameView()->InitImmersiveFullscreenControllerForView(
          immersive_fullscreen_controller_.get());
      // Restoring window targeter replaced by ImmersiveFullscreenController.
      InstallCustomWindowTargeter();

      UpdateCaptionButtonModel();
    }
    UpdateFrameWidth();
  }
  // The autohide should be applied when the window state is in
  // maximzied, fullscreen or pinned. Update the auto hide state
  // inside commit.
  if (update_frame)
    UpdateAutoHideFrame();
}

void ClientControlledShellSurface::UpdateCaptionButtonModel() {
  auto model = std::make_unique<CaptionButtonModel>(frame_visible_button_mask_,
                                                    frame_enabled_button_mask_);
  if (wide_frame_)
    wide_frame_->SetCaptionButtonModel(std::move(model));
  else
    GetFrameView()->SetCaptionButtonModel(std::move(model));
}

void ClientControlledShellSurface::UpdateBackdrop() {
  aura::Window* window = widget_->GetNativeWindow();

  // Always create a backdrop regardless of the geometry because
  // maximized/fullscreen widget's geometry can be cropped.
  bool enable_backdrop = (widget_->IsFullscreen() || widget_->IsMaximized());

  ash::BackdropWindowMode target_backdrop_mode =
      enable_backdrop ? ash::BackdropWindowMode::kEnabled
                      : ash::BackdropWindowMode::kAutoOpaque;

  if (window->GetProperty(ash::kBackdropWindowMode) != target_backdrop_mode)
    window->SetProperty(ash::kBackdropWindowMode, target_backdrop_mode);
}

void ClientControlledShellSurface::UpdateFrameWidth() {
  int width = -1;
  if (shadow_bounds_) {
    float device_scale_factor =
        GetWidget()->GetNativeWindow()->layer()->device_scale_factor();
    float dsf_to_default_dsf = device_scale_factor / scale_;
    width = gfx::ToRoundedInt(shadow_bounds_->width() * dsf_to_default_dsf);
  }
  static_cast<ash::HeaderView*>(GetFrameView()->GetHeaderView())
      ->SetWidthInPixels(width);
}

void ClientControlledShellSurface::
    EnsureCompositorIsLockedForOrientationChange() {
  if (!orientation_compositor_lock_) {
    ui::Compositor* compositor =
        widget_->GetNativeWindow()->layer()->GetCompositor();
    orientation_compositor_lock_ = compositor->GetCompositorLock(
        this, base::TimeDelta::FromMilliseconds(kOrientationLockTimeoutMs));
  }
}

ash::WindowState* ClientControlledShellSurface::GetWindowState() {
  return ash::WindowState::Get(widget_->GetNativeWindow());
}

ash::NonClientFrameViewAsh* ClientControlledShellSurface::GetFrameView() {
  return static_cast<ash::NonClientFrameViewAsh*>(
      widget_->non_client_view()->frame_view());
}

const ash::NonClientFrameViewAsh* ClientControlledShellSurface::GetFrameView()
    const {
  return static_cast<const ash::NonClientFrameViewAsh*>(
      widget_->non_client_view()->frame_view());
}

// static
void ClientControlledShellSurface::
    SetClientControlledStateDelegateFactoryForTest(
        const DelegateFactoryCallback& callback) {
  auto& factory = GetFactoryForTesting();
  factory = callback;
}

}  // namespace exo
