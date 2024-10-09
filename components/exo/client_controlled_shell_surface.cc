// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/client_controlled_shell_surface.h"

#include <map>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/frame/wide_frame_view.h"
#include "ash/public/cpp/arc_resize_lock_type.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/rounded_corner_utils.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/client_controlled_state.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/drag_details.h"
#include "ash/wm/pip/pip_controller.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_util.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "chromeos/ui/base/window_pin_type.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/caption_buttons/caption_button_model.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "chromeos/ui/frame/header_view.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/scoped_window_event_targeting_blocker.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/class_property.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_lock.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(exo::ClientControlledShellSurface*)

namespace exo {

namespace {
using ::ash::screen_util::GetIdealBoundsForMaximizedOrFullscreenOrPinnedState;
using ::chromeos::WindowStateType;

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

  ClientControlledStateDelegate(const ClientControlledStateDelegate&) = delete;
  ClientControlledStateDelegate& operator=(
      const ClientControlledStateDelegate&) = delete;

  ~ClientControlledStateDelegate() override = default;

  // Overridden from ash::ClientControlledState::Delegate:
  void HandleWindowStateRequest(ash::WindowState* window_state,
                                chromeos::WindowStateType next_state) override {
    shell_surface_->OnWindowStateChangeEvent(window_state->GetStateType(),
                                             next_state);
  }
  void HandleBoundsRequest(ash::WindowState* window_state,
                           chromeos::WindowStateType requested_state,
                           const gfx::Rect& bounds_in_display,
                           int64_t display_id) override {
    shell_surface_->OnBoundsChangeEvent(
        window_state->GetStateType(), requested_state, display_id,
        bounds_in_display,
        window_state->drag_details() && shell_surface_->IsDragging()
            ? window_state->drag_details()->bounds_change
            : 0,
        /*is_adjusted_bounds=*/false);
  }

 private:
  raw_ptr<ClientControlledShellSurface> shell_surface_;
};

// A WindowStateDelegate that implements ToggleFullscreen behavior for
// client controlled window.
class ClientControlledWindowStateDelegate : public ash::WindowStateDelegate {
 public:
  explicit ClientControlledWindowStateDelegate(
      ClientControlledShellSurface* shell_surface,
      ash::ClientControlledState::Delegate* delegate)
      : shell_surface_(shell_surface), delegate_(delegate) {}

  ClientControlledWindowStateDelegate(
      const ClientControlledWindowStateDelegate&) = delete;
  ClientControlledWindowStateDelegate& operator=(
      const ClientControlledWindowStateDelegate&) = delete;

  ~ClientControlledWindowStateDelegate() override = default;

  // Overridden from ash::WindowStateDelegate:
  bool ToggleFullscreen(ash::WindowState* window_state) override {
    chromeos::WindowStateType next_state;
    aura::Window* window = window_state->window();
    switch (window_state->GetStateType()) {
      case chromeos::WindowStateType::kDefault:
      case chromeos::WindowStateType::kNormal:
        next_state = chromeos::WindowStateType::kFullscreen;
        break;
      case chromeos::WindowStateType::kMaximized:
        next_state = chromeos::WindowStateType::kFullscreen;
        break;
      case chromeos::WindowStateType::kFullscreen:
        switch (window->GetProperty(aura::client::kRestoreShowStateKey)) {
          case ui::mojom::WindowShowState::kDefault:
          case ui::mojom::WindowShowState::kNormal:
            next_state = chromeos::WindowStateType::kNormal;
            break;
          case ui::mojom::WindowShowState::kMaximized:
            next_state = chromeos::WindowStateType::kMaximized;
            break;
          case ui::mojom::WindowShowState::kMinimized:
            next_state = chromeos::WindowStateType::kMinimized;
            break;
          case ui::mojom::WindowShowState::kFullscreen:
          case ui::mojom::WindowShowState::kInactive:
          case ui::mojom::WindowShowState::kEnd:
            DUMP_WILL_BE_NOTREACHED()
                << " unknown state :"
                << window->GetProperty(aura::client::kRestoreShowStateKey);
            return false;
        }
        break;
      case chromeos::WindowStateType::kMinimized: {
        next_state = chromeos::WindowStateType::kFullscreen;
        break;
      }
      default:
        // TODO(oshima|xdai): Handle SNAP state.
        return false;
    }
    delegate_->HandleWindowStateRequest(window_state, next_state);
    return true;
  }

  void ToggleLockedFullscreen(ash::WindowState*) override {
    // No special handling for locked ARC windows.
    return;
  }

  std::unique_ptr<ash::PresentationTimeRecorder> OnDragStarted(
      int component) override {
    shell_surface_->OnDragStarted(component);
    return nullptr;
  }

  void OnDragFinished(bool canceled, const gfx::PointF& location) override {
    shell_surface_->OnDragFinished(canceled, location);
  }

 private:
  raw_ptr<ClientControlledShellSurface> shell_surface_;
  raw_ptr<ash::ClientControlledState::Delegate, DanglingUntriaged> delegate_;
};

bool IsPinned(const ash::WindowState* window_state) {
  return window_state->IsPinned() || window_state->IsTrustedPinned();
}

class CaptionButtonModel : public chromeos::CaptionButtonModel {
 public:
  CaptionButtonModel(uint32_t visible_button_mask, uint32_t enabled_button_mask)
      : visible_button_mask_(visible_button_mask),
        enabled_button_mask_(enabled_button_mask) {}

  CaptionButtonModel(const CaptionButtonModel&) = delete;
  CaptionButtonModel& operator=(const CaptionButtonModel&) = delete;

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
};

// EventTargetingBlocker blocks the event targeting by setting NONE targeting
// policy to the window subtrees. It resets to the original policy upon
// deletion.
class EventTargetingBlocker : aura::WindowObserver {
 public:
  EventTargetingBlocker() = default;

  EventTargetingBlocker(const EventTargetingBlocker&) = delete;
  EventTargetingBlocker& operator=(const EventTargetingBlocker&) = delete;

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
    for (aura::Window* child : window->children()) {
      Register(child);
    }
  }

  void Unregister(aura::Window* window) {
    window->RemoveObserver(this);
    event_targeting_blocker_map_.erase(window);
    for (aura::Window* child : window->children()) {
      Unregister(child);
    }
  }

  void OnWindowDestroying(aura::Window* window) override {
    Unregister(window);
    if (window_ == window)
      window_ = nullptr;
  }

  std::map<aura::Window*,
           std::unique_ptr<aura::ScopedWindowEventTargetingBlocker>>
      event_targeting_blocker_map_;
  raw_ptr<aura::Window> window_ = nullptr;
};

}  // namespace

class ClientControlledShellSurface::ScopedSetBoundsLocally {
 public:
  explicit ScopedSetBoundsLocally(ClientControlledShellSurface* shell_surface)
      : state_(shell_surface->client_controlled_state_) {
    state_->set_bounds_locally(true);
  }

  ScopedSetBoundsLocally(const ScopedSetBoundsLocally&) = delete;
  ScopedSetBoundsLocally& operator=(const ScopedSetBoundsLocally&) = delete;

  ~ScopedSetBoundsLocally() { state_->set_bounds_locally(false); }

 private:
  const raw_ptr<ash::ClientControlledState> state_;
};

class ClientControlledShellSurface::ScopedLockedToRoot {
 public:
  explicit ScopedLockedToRoot(views::Widget* widget)
      : window_(widget->GetNativeWindow()) {
    window_->SetProperty(ash::kLockedToRootKey, true);
  }

  ScopedLockedToRoot(const ScopedLockedToRoot&) = delete;
  ScopedLockedToRoot& operator=(const ScopedLockedToRoot&) = delete;

  ~ScopedLockedToRoot() { window_->ClearProperty(ash::kLockedToRootKey); }

 private:
  const raw_ptr<aura::Window> window_;
};

class ClientControlledShellSurface::ScopedDeferWindowStateUpdate {
 public:
  explicit ScopedDeferWindowStateUpdate(
      ClientControlledShellSurface* shell_surface)
      : shell_surface_(shell_surface) {
    CHECK(!shell_surface_->scoped_defer_window_state_update_);
    shell_surface_->scoped_defer_window_state_update_ = base::WrapUnique(this);
    // Do not activate if the widget is initially minimized.
    if (shell_surface->GetWidget()->IsMinimized()) {
      can_activate_ =
          shell_surface->GetWidget()->widget_delegate()->CanActivate();
      shell_surface->GetWidget()->widget_delegate()->SetCanActivate(false);
    }
  }

  ScopedDeferWindowStateUpdate(const ScopedDeferWindowStateUpdate&) = delete;
  ScopedDeferWindowStateUpdate& operator=(const ScopedDeferWindowStateUpdate&) =
      delete;

  ~ScopedDeferWindowStateUpdate() {
    auto self = shell_surface_->scoped_defer_window_state_update_.release();
    DCHECK_EQ(self, this);
    if (can_activate_.has_value()) {
      shell_surface_->GetWidget()->widget_delegate()->SetCanActivate(
          can_activate_.value());
    }
    if (next_state_) {
      shell_surface_->OnWindowStateChangeEvent(*next_state_, *next_state_);
    }
  }

  void SetNextState(chromeos::WindowStateType next_state) {
    next_state_ = next_state;
  }

 private:
  raw_ptr<ClientControlledShellSurface> shell_surface_;
  std::optional<chromeos::WindowStateType> next_state_;
  std::optional<bool> can_activate_;
};

////////////////////////////////////////////////////////////////////////////////
// ClientControlledShellSurface, public:

ClientControlledShellSurface::ClientControlledShellSurface(
    Surface* surface,
    bool can_minimize,
    int container,
    bool default_scale_cancellation,
    bool supports_floated_state)
    : ShellSurfaceBase(surface, gfx::Point(), can_minimize, container),
      use_default_scale_cancellation_(default_scale_cancellation),
      supports_floated_state_(supports_floated_state) {
  server_side_resize_ = true;
  set_client_submits_surfaces_in_pixel_coordinates(true);
}

ClientControlledShellSurface::~ClientControlledShellSurface() {
  // Reset the window delegate here so that we won't try to do any dragging
  // operation on a to-be-destroyed window. |widget_| can be nullptr in tests.
  if (GetWidget())
    GetWindowState()->SetDelegate(nullptr);
  if (client_controlled_state_)
    client_controlled_state_->ResetDelegate();
  wide_frame_.reset();
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

  const gfx::Rect bounds_dp =
      gfx::ScaleToRoundedRect(bounds, GetClientToDpPendingScale());
  SetGeometry(bounds_dp);
}

void ClientControlledShellSurface::SetBoundsOrigin(int64_t display_id,
                                                   const gfx::Point& origin) {
  TRACE_EVENT2("exo", "ClientControlledShellSurface::SetBoundsOrigin",
               "display_id", display_id, "origin", origin.ToString());
  SetDisplay(display_id);
  const gfx::Point origin_dp =
      gfx::ScaleToRoundedPoint(origin, GetClientToDpPendingScale());
  pending_geometry_.set_origin(origin_dp);
}

void ClientControlledShellSurface::SetBoundsSize(const gfx::Size& size) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetBoundsSize", "size",
               size.ToString());

  if (size.IsEmpty()) {
    DLOG(WARNING) << "Bounds size must be non-empty";
    return;
  }

  const gfx::Size size_dp =
      gfx::ScaleToRoundedSize(size, GetClientToDpPendingScale());
  pending_geometry_.set_size(size_dp);
}

void ClientControlledShellSurface::SetMaximized() {
  TRACE_EVENT0("exo", "ClientControlledShellSurface::SetMaximized");
  pending_window_state_ = chromeos::WindowStateType::kMaximized;
}

void ClientControlledShellSurface::SetMinimized() {
  TRACE_EVENT0("exo", "ClientControlledShellSurface::SetMinimized");
  pending_window_state_ = chromeos::WindowStateType::kMinimized;
}

void ClientControlledShellSurface::SetRestored() {
  TRACE_EVENT0("exo", "ClientControlledShellSurface::SetRestored");
  pending_window_state_ = chromeos::WindowStateType::kNormal;
}

void ClientControlledShellSurface::SetFullscreen(bool fullscreen,
                                                 int64_t display_id) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetFullscreen",
               "fullscreen", fullscreen);
  pending_window_state_ = fullscreen ? chromeos::WindowStateType::kFullscreen
                                     : chromeos::WindowStateType::kNormal;
  // TODO(crbug.com/40280523): `display_id` might need to be used here
  // somewhere.
}

void ClientControlledShellSurface::SetPinned(chromeos::WindowPinType type) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetPinned", "type",
               static_cast<int>(type));

  if (!widget_)
    CreateShellSurfaceWidget(ui::mojom::WindowShowState::kNormal);

  if (type == chromeos::WindowPinType::kNone) {
    // Set other window state mode will automatically cancelled pin mode.
    // TODO: Add NOTREACH() here after ARC side integration fully landed.
  } else {
    bool trusted = type == chromeos::WindowPinType::kTrustedPinned;
    pending_window_state_ = trusted ? chromeos::WindowStateType::kTrustedPinned
                                    : chromeos::WindowStateType::kPinned;
  }
}

void ClientControlledShellSurface::SetSystemUiVisibility(bool autohide) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetSystemUiVisibility",
               "autohide", autohide);

  if (!widget_)
    CreateShellSurfaceWidget(ui::mojom::WindowShowState::kNormal);

  ash::window_util::SetAutoHideShelf(widget_->GetNativeWindow(), autohide);
}

void ClientControlledShellSurface::SetAlwaysOnTop(bool always_on_top) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetAlwaysOnTop",
               "always_on_top", always_on_top);
  pending_always_on_top_ = always_on_top;
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
      bounds.IsEmpty() ? std::nullopt : std::make_optional(bounds);
  if (shadow_bounds_ != shadow_bounds) {
    shadow_bounds_ = shadow_bounds;
    shadow_bounds_changed_ = true;
  }
}

void ClientControlledShellSurface::OnWindowStateChangeEvent(
    chromeos::WindowStateType current_state,
    chromeos::WindowStateType next_state) {
  // Android already knows this state change. Don't send state change to Android
  // that it is about to do anyway.
  if (scoped_defer_window_state_update_) {
    scoped_defer_window_state_update_->SetNextState(next_state);
    return;
  }

  if (delegate_ && pending_window_state_ != next_state)
    delegate_->OnStateChanged(current_state, next_state);
}

void ClientControlledShellSurface::StartDrag(int component,
                                             const gfx::PointF& location) {
  TRACE_EVENT2("exo", "ClientControlledShellSurface::StartDrag", "component",
               component, "location", location.ToString());

  if (!widget_)
    return;
  AttemptToStartDrag(component, location);
}

void ClientControlledShellSurface::AttemptToStartDrag(
    int component,
    const gfx::PointF& location) {
  aura::Window* target = widget_->GetNativeWindow();
  ash::ToplevelWindowEventHandler* toplevel_handler =
      ash::Shell::Get()->toplevel_window_event_handler();
  aura::Window* mouse_pressed_handler =
      target->GetHost()->dispatcher()->mouse_pressed_handler();
  // Start dragging only if:
  // 1) touch guesture is in progress or
  // 2) mouse was pressed on the target or its subsurfaces.
  // If neither condition is met, we do not start the drag.
  gfx::PointF point_in_root;
  if (toplevel_handler->gesture_target()) {
    point_in_root = toplevel_handler->event_location_in_gesture_target();
    aura::Window::ConvertPointToTarget(
        toplevel_handler->gesture_target(),
        widget_->GetNativeWindow()->GetRootWindow(), &point_in_root);
  } else if (mouse_pressed_handler && target->Contains(mouse_pressed_handler)) {
    point_in_root = location;
    if (use_default_scale_cancellation_) {
      // When default scale cancellation is enabled, the client sends the
      // location in screen coordinates. Otherwise, the location should already
      // be in the display's coordinates.
      wm::ConvertPointFromScreen(target->GetRootWindow(), &point_in_root);
    }
  } else {
    return;
  }
  toplevel_handler->AttemptToStartDrag(
      target, point_in_root, component,
      ash::ToplevelWindowEventHandler::EndClosure());
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
    chromeos::ImmersiveFullscreenController::EnableForWidget(widget_, enabled);
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
    const std::u16string& extra_title) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetExtraTitle",
               "extra_title", base::UTF16ToUTF8(extra_title));

  if (!widget_) {
    initial_extra_title_ = extra_title;
    return;
  }

  GetFrameView()->GetHeaderView()->GetFrameHeader()->SetFrameTextOverride(
      extra_title);
  if (wide_frame_) {
    wide_frame_->header_view()->GetFrameHeader()->SetFrameTextOverride(
        extra_title);
  }
}

void ClientControlledShellSurface::RebindRootSurface(
    Surface* root_surface,
    bool can_minimize,
    int container,
    bool default_scale_cancellation,
    bool supports_floated_state) {
  use_default_scale_cancellation_ = default_scale_cancellation;
  supports_floated_state_ = supports_floated_state;
  auto* const window = widget_ ? widget_->GetNativeWindow() : nullptr;
  if (window) {
    window->SetProperty(chromeos::kSupportsFloatedStateKey,
                        supports_floated_state_);
  }
  ShellSurfaceBase::RebindRootSurface(root_surface, can_minimize, container);
}

void ClientControlledShellSurface::DidReceiveCompositorFrameAck() {
  orientation_ = pending_orientation_;
  // Unlock the compositor after the frame is received by viz so that
  // screenshot contain the correct frame.
  if (expected_orientation_ == orientation_)
    orientation_compositor_lock_.reset();
  SurfaceTreeHost::DidReceiveCompositorFrameAck();
}

void ClientControlledShellSurface::OnBoundsChangeEvent(
    chromeos::WindowStateType current_state,
    chromeos::WindowStateType requested_state,
    int64_t display_id,
    const gfx::Rect& window_bounds,
    int bounds_change,
    bool is_adjusted_bounds) {
  // 1) Do no update the bounds unless we have geometry from client.
  // 2) Do not update the bounds if window is minimized unless it
  // exiting the minimzied state.
  // The bounds will be provided by client when unminimized.
  if (geometry().IsEmpty() || window_bounds.IsEmpty() ||
      (widget_->IsMinimized() &&
       requested_state == chromeos::WindowStateType::kMinimized) ||
      !delegate_) {
    return;
  }

  // Sends the client bounds, which matches the geometry
  // when frame is enabled.
  const gfx::Rect client_bounds = GetClientBoundsForWindowBoundsAndWindowState(
      window_bounds, requested_state);

  gfx::Size current_size = GetFrameView()->GetBoundsForClientView().size();
  bool is_resize = client_bounds.size() != current_size &&
                   !widget_->IsMaximized() && !widget_->IsFullscreen();

  // Make sure to use the up-to-date scale factor.
  display::Display display;
  const bool display_exists =
      display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id,
                                                            &display);
  DCHECK(display_exists && display.is_valid());
  const float scale =
      use_default_scale_cancellation_ ? 1.f : display.device_scale_factor();
  const gfx::Rect scaled_client_bounds =
      gfx::ScaleToRoundedRect(client_bounds, scale);
  delegate_->OnBoundsChanged(current_state, requested_state, display_id,
                             scaled_client_bounds, is_resize, bounds_change,
                             is_adjusted_bounds);

  auto* window_state = GetWindowState();
  if (server_reparent_window_ &&
      window_state->GetDisplay().id() != display_id) {
    ScopedSetBoundsLocally scoped_set_bounds(this);
    int container_id = window_state->window()->parent()->GetId();
    aura::Window* new_parent =
        ash::Shell::GetRootWindowControllerWithDisplayId(display_id)
            ->GetContainer(container_id);
    new_parent->AddChild(window_state->window());
  }
}

void ClientControlledShellSurface::ChangeZoomLevel(ZoomChange change) {
  if (delegate_)
    delegate_->OnZoomLevelChanged(change);
}

void ClientControlledShellSurface::OnDragStarted(int component) {
  in_drag_ = true;
  if (delegate_)
    delegate_->OnDragStarted(component);
}

void ClientControlledShellSurface::OnDragFinished(bool canceled,
                                                  const gfx::PointF& location) {
  in_drag_ = false;
  if (!delegate_)
    return;

  const float scale = 1.f / GetClientToDpScale();
  const gfx::PointF scaled = gfx::ScalePoint(location, scale);
  delegate_->OnDragFinished(scaled.x(), scaled.y(), canceled);
}

float ClientControlledShellSurface::GetClientToDpScale() const {
  // If the default_device_scale_factor is used for scale cancellation,
  // we expect the client will already send bounds in DP.
  if (use_default_scale_cancellation_)
    return 1.f;
  return 1.f / GetScale();
}

void ClientControlledShellSurface::SetResizeLockType(
    ash::ArcResizeLockType resize_lock_type) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetResizeLockType",
               "resize_lock_type", resize_lock_type);
  pending_resize_lock_type_ = resize_lock_type;
}

void ClientControlledShellSurface::UpdateResizability() {
  TRACE_EVENT0("exo", "ClientControlledShellSurface::updateCanResize");
  widget_->GetNativeWindow()->SetProperty(ash::kArcResizeLockTypeKey,
                                          pending_resize_lock_type_);
  // If resize lock is enabled, the window is explicitly marded as unresizable.
  // Otherwise, the decision is deferred to the parent class.
  if (pending_resize_lock_type_ ==
           ash::ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE ||
       pending_resize_lock_type_ ==
           ash::ArcResizeLockType::RESIZE_DISABLED_NONTOGGLABLE) {
    SetCanResize(false);
    return;
  }
  ShellSurfaceBase::UpdateResizability();
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
  pending_frame_type_ = type;
}

void ClientControlledShellSurface::OnSetFrameColors(SkColor active_color,
                                                    SkColor inactive_color) {
  ShellSurfaceBase::OnSetFrameColors(active_color, inactive_color);
  if (wide_frame_) {
    aura::Window* window = wide_frame_->GetWidget()->GetNativeWindow();
    window->SetProperty(chromeos::kTrackDefaultFrameColors, false);
    window->SetProperty(chromeos::kFrameActiveColorKey, active_color);
    window->SetProperty(chromeos::kFrameInactiveColorKey, inactive_color);
  }
}

void ClientControlledShellSurface::SetSnapPrimary(float snap_ratio) {
  TRACE_EVENT0("exo", "ClientControlledShellSurface::SetSnappedToPrimary");
  pending_window_state_ = chromeos::WindowStateType::kPrimarySnapped;
}

void ClientControlledShellSurface::SetSnapSecondary(float snap_ratio) {
  TRACE_EVENT0("exo", "ClientControlledShellSurface::SetSnappedToSecondary");
  pending_window_state_ = chromeos::WindowStateType::kSecondarySnapped;
}

void ClientControlledShellSurface::SetPip() {
  TRACE_EVENT0("exo", "ClientControlledShellSurface::SetPip");
  pending_window_state_ = chromeos::WindowStateType::kPip;
}

void ClientControlledShellSurface::UnsetPip() {
  TRACE_EVENT0("exo", "ClientControlledShellSurface::UnsetPip");
  SetRestored();
}

void ClientControlledShellSurface::SetFloatToLocation(
    chromeos::FloatStartLocation float_start_location) {
  TRACE_EVENT0("exo", "ClientControlledShellSurface::SetFloatToLocation");
  pending_window_state_ = chromeos::WindowStateType::kFloated;
}

void ClientControlledShellSurface::OnDidProcessDisplayChanges(
    const DisplayConfigurationChange& configuration_change) {
  ShellSurfaceBase::OnDidProcessDisplayChanges(configuration_change);

  if (!widget_) {
    return;
  }

  // The PIP window bounds is adjusted in Ash when the screen is rotated, but
  // Android has an obsolete bounds for a while and applies it incorrectly.
  // We need to ignore those bounds change until the states are completely
  // synced on both sides.
  const bool any_displays_rotated = base::ranges::any_of(
      configuration_change.display_metrics_changes,
      [](const DisplayManagerObserver::DisplayMetricsChange& change) {
        return change.changed_metrics &
               display::DisplayObserver::DISPLAY_METRIC_ROTATION;
      });
  if (GetWindowState()->IsPip() && any_displays_rotated) {
    gfx::Rect bounds_after_rotation =
        ash::PipPositioner::GetSnapFractionAppliedBounds(GetWindowState());
    display_rotating_with_pip_ =
        bounds_after_rotation !=
        GetWindowState()->window()->GetBoundsInScreen();
  }

  // Early return if no display changes are relevant to the shell surface's host
  // display.
  const auto host_display_change = base::ranges::find(
      configuration_change.display_metrics_changes, output_display_id(),
      [](const DisplayManagerObserver::DisplayMetricsChange& change) {
        return change.display->id();
      });
  if (host_display_change ==
      configuration_change.display_metrics_changes.end()) {
    return;
  }

  uint32_t changed_metrics = host_display_change->changed_metrics;
  if (!display::Screen::GetScreen()->InTabletMode() || !widget_->IsActive() ||
      !(changed_metrics & display::DisplayObserver::DISPLAY_METRIC_ROTATION)) {
    return;
  }

  Orientation target_orientation =
      SizeToOrientation(host_display_change->display->size());
  if (orientation_ == target_orientation) {
    return;
  }
  expected_orientation_ = target_orientation;
  EnsureCompositorIsLockedForOrientationChange();
}

////////////////////////////////////////////////////////////////////////////////
// aura::WindowObserver overrides:
void ClientControlledShellSurface::OnWindowDestroying(aura::Window* window) {
  if (client_controlled_state_) {
    client_controlled_state_->ResetDelegate();
    client_controlled_state_ = nullptr;
  }
  ShellSurfaceBase::OnWindowDestroying(window);
}

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

void ClientControlledShellSurface::WindowClosing() {
  wide_frame_.reset();
  ShellSurfaceBase::WindowClosing();
}

bool ClientControlledShellSurface::CanMaximize() const {
  return can_maximize_;
}

std::unique_ptr<views::NonClientFrameView>
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
  auto frame_view = CreateNonClientFrameViewInternal(widget);
  immersive_fullscreen_controller_ =
      std::make_unique<chromeos::ImmersiveFullscreenController>();
  static_cast<ash::NonClientFrameViewAsh*>(frame_view.get())
      ->InitImmersiveFullscreenControllerForView(
          immersive_fullscreen_controller_.get());
  return frame_view;
}

bool ClientControlledShellSurface::ShouldSaveWindowPlacement() const {
  return false;
}

void ClientControlledShellSurface::SaveWindowPlacement(
    const gfx::Rect& bounds,
    ui::mojom::WindowShowState show_state) {}

bool ClientControlledShellSurface::GetSavedWindowPlacement(
    const views::Widget* widget,
    gfx::Rect* bounds,
    ui::mojom::WindowShowState* show_state) const {
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
// ui::CompositorLockClient overrides:

void ClientControlledShellSurface::CompositorLockTimedOut() {
  orientation_compositor_lock_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// ShellSurfaceBase overrides:

void ClientControlledShellSurface::SetSystemModal(bool system_modal) {
  // System modal container is used by clients to implement client side
  // managed system modal dialogs using a single ShellSurface instance.
  // Hit-test region will be non-empty when at least one dialog exists on
  // the client side. Here we detect the transition between no client side
  // dialog and at least one dialog so activatable state is properly
  // updated.
  if (container_ != ash::kShellWindowId_SystemModalContainer) {
    LOG(ERROR)
        << "Only a window in SystemModalContainer can change the modality";
    return;
  }

  ShellSurfaceBase::SetSystemModal(system_modal);
}

void ClientControlledShellSurface::SetWidgetBounds(const gfx::Rect& bounds,
                                                   bool adjusted_by_server) {
  set_bounds_is_dirty(true);
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
  // TODO(oshima): Move this to ComputeAdjustedBounds.
  gfx::Rect adjusted_bounds = bounds;
  if (!is_display_move_pending) {
    const gfx::Rect& restriction = GetWindowState()->IsFullscreen()
                                       ? target_display.bounds()
                                       : target_display.work_area();
    ash::AdjustBoundsToEnsureMinimumWindowVisibility(
        restriction, /*client_controlled=*/true, &adjusted_bounds);
    // Collision detection to the bounds set by Android should be applied only
    // to initial bounds and any client-requested bounds (I.E. Double-Tap to
    // resize). Do not adjust new bounds for fling/display rotation as it can be
    // obsolete or in transit during animation, which results in incorrect
    // resting postiion. The resting position should be fully controlled by
    // chrome afterwards because Android isn't aware of Chrome OS System UI.
    const bool is_resizing_without_rotation =
        !display_rotating_with_pip_ && !IsDragging() &&
        !ash::Shell::Get()->pip_controller()->is_tucked() &&
        GetWindowState()->GetCurrentBoundsInScreen().size() != bounds.size();
    if (GetWindowState()->IsPip() &&
        (!ash::PipPositioner::HasSnapFraction(GetWindowState()) ||
         is_resizing_without_rotation)) {
      adjusted_bounds = ash::CollisionDetectionUtils::GetRestingPosition(
          target_display, adjusted_bounds,
          ash::CollisionDetectionUtils::RelativePriority::kPictureInPicture);

      // Only if the window is resizing with a double tap, the bounds should
      // be applied via a scaling animation. Position changes will be applied
      // via kAnimate.
      if (is_resizing_without_rotation && !IsDragging()) {
        client_controlled_state_->set_next_bounds_change_animation_type(
            ash::WindowState::BoundsChangeAnimationType::kCrossFade);
      }
    }
  }

  if (adjusted_bounds == widget_->GetWindowBoundsInScreen() &&
      target_display.id() == current_display.id()) {
    return;
  }

  bool set_bounds_locally =
      display_rotating_with_pip_ ||
      (GetWindowState()->is_dragged() && !is_display_move_pending);

  if (set_bounds_locally || client_controlled_state_->set_bounds_locally()) {
    // Convert from screen to display coordinates.
    gfx::Point origin = bounds.origin();
    wm::ConvertPointFromScreen(window->parent(), &origin);

    // Move the window relative to the current display.
    {
      ScopedSetBoundsLocally scoped_set_bounds(this);
      window->SetBounds(gfx::Rect(origin, adjusted_bounds.size()));
    }
    UpdateHostWindowOrigin();
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
                        adjusted_bounds_in_display, 0,
                        /*is_adjusted_bounds=*/true);
  }

  UpdateHostWindowOrigin();
}
gfx::Rect ClientControlledShellSurface::GetVisibleBounds() const {
  const auto* screen = display::Screen::GetScreen();
  display::Display display;

  if (geometry_.IsEmpty() ||
      !screen->GetDisplayWithDisplayId(display_id_, &display)) {
    return ShellSurfaceBase::GetVisibleBounds();
  }
  // ARC sends geometry_ in screen coordinates.
  return geometry_ + display.bounds().OffsetFromOrigin();
}

gfx::Rect ClientControlledShellSurface::GetShadowBounds() const {
  gfx::Rect shadow_bounds = ShellSurfaceBase::GetShadowBounds();
  const ash::NonClientFrameViewAsh* frame_view = GetFrameView();
  if (frame_view->GetFrameEnabled() && !shadow_bounds_->IsEmpty() &&
      !geometry_.IsEmpty() && !frame_view->GetFrameOverlapped()) {
    // The client controlled geometry is only for the client
    // area. When the chrome side frame is enabled, the shadow height
    // has to include the height of the frame, and the total height is
    // equals to the window height computed by
    // |GetWindowBoundsForClientBounds|.
    // But when the frame is overlapped with the client area, shadow bounds
    // should be the same as the client area bounds.
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
  if (initial_orientation_lock_ != chromeos::OrientationType::kAny)
    SetOrientationLock(initial_orientation_lock_);
  if (initial_extra_title_ != std::u16string())
    SetExtraTitle(initial_extra_title_);

  // Register Client controlled accelerators.
  views::FocusManager* focus_manager = widget_->GetFocusManager();
  accelerator_target_ =
      std::make_unique<ClientControlledAcceleratorTarget>(this);

  // These shortcuts are same as ones used in chrome.
  // TODO: investigate if we need to reassign.
  for (const auto& entry : kAccelerators) {
    focus_manager->RegisterAccelerator(
        ui::Accelerator(entry.keycode, entry.modifiers),
        ui::AcceleratorManager::kNormalPriority, accelerator_target_.get());
    accelerator_target_->RegisterAccelerator(
        ui::Accelerator(entry.keycode, entry.modifiers), entry.action);
  }

  auto* window = widget_->GetNativeWindow();
  GrantPermissionToActivateIndefinitely(window);

  window->SetProperty(chromeos::kSupportsFloatedStateKey,
                      supports_floated_state_);
}

float ClientControlledShellSurface::GetScale() const {
  return !use_default_scale_cancellation_
             ? ShellSurfaceBase::GetScaleFactor()
             : ::exo::GetDefaultDeviceScaleFactor();
}

float ClientControlledShellSurface::GetScaleFactor() const {
  // TODO(andreaorru): consolidate Scale and ScaleFactor.
  return GetScale();
}

std::optional<gfx::Rect> ClientControlledShellSurface::GetWidgetBounds() const {
  const ash::NonClientFrameViewAsh* frame_view = GetFrameView();
  if (frame_view->GetFrameEnabled() && !frame_view->GetFrameOverlapped()) {
    gfx::Rect visible_bounds = GetVisibleBounds();
    if (widget_->IsMaximized() && frame_type_ == SurfaceFrameType::NORMAL) {
      // When the widget is maximized in clamshell mode, client sends
      // |geometry_| without taking caption height into account.
      visible_bounds.Offset(0, frame_view->NonClientTopBorderHeight());
    }
    return frame_view->GetWindowBoundsForClientBounds(visible_bounds);
  }

  // When frame is overlapped with the client window, widget bounds is the same
  // as the |geometry_| from client.
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
    CreateShellSurfaceWidget(
        chromeos::ToWindowShowState(pending_window_state_));
  }

  // Finish ignoring obsolete bounds update as the state changes caused by
  // display rotation are synced.
  // TODO(takise): This assumes no other bounds update happens during screen
  // rotation. Implement more robust logic to handle synchronization for
  // screen rotation.
  if (pending_geometry_ != geometry_)
    display_rotating_with_pip_ = false;

  ash::WindowState* window_state = GetWindowState();
  state_changed_ = window_state->GetStateType() != pending_window_state_;
  if (!state_changed_) {
    // Animate PIP window movement unless it is being dragged.
    client_controlled_state_->set_next_bounds_change_animation_type(
        window_state->IsPip() && !window_state->is_dragged()
            ? ash::WindowState::BoundsChangeAnimationType::kAnimate
            : ash::WindowState::BoundsChangeAnimationType::kNone);
    return true;
  }

  if (IsPinned(window_state) &&
      (pending_window_state_ == chromeos::WindowStateType::kPinned ||
       pending_window_state_ == chromeos::WindowStateType::kTrustedPinned)) {
    VLOG(1) << "Pinned was requested while pinned";
    return true;
  }

  auto animation_type = ash::WindowState::BoundsChangeAnimationType::kNone;
  switch (pending_window_state_) {
    case chromeos::WindowStateType::kNormal:
      if (widget_->IsMaximized() || widget_->IsFullscreen()) {
        animation_type =
            ash::WindowState::BoundsChangeAnimationType::kCrossFade;
      }
      break;
    case chromeos::WindowStateType::kFloated:
      animation_type =
          ash::WindowState::BoundsChangeAnimationType::kCrossFadeFloat;
      break;
    case chromeos::WindowStateType::kMaximized:
    case chromeos::WindowStateType::kFullscreen:
      if (!window_state->IsPip())
        animation_type =
            ash::WindowState::BoundsChangeAnimationType::kCrossFade;
      break;
    default:
      break;
  }

  if (window_state->IsFloated()) {
    animation_type =
        ash::WindowState::BoundsChangeAnimationType::kCrossFadeUnfloat;
  }

  bool wasPip = window_state->IsPip();
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
    // TODO(crbug.com/40616384): Investigate if we can activate PIP windows
    // inside commit.
    window_state->Activate();
  }

  return true;
}

void ClientControlledShellSurface::ShowWidget(bool inactive) {
  ScopedDeferWindowStateUpdate update(this);
  ShellSurfaceBase::ShowWidget(inactive);
}

void ClientControlledShellSurface::OnPostWidgetCommit() {
  DCHECK(widget_);

  UpdateFrame();
  UpdateBackdrop();

  if (delegate_) {
    // Since the visible bounds are in screen coordinates, do not scale these
    // bounds with the display's scale before sending them.
    // TODO(b/167286795): Instead of sending bounds in screen coordinates, send
    // the bounds in the display along with the display information, similar to
    // the bounds_changed_callback_.
    delegate_->OnGeometryChanged(GetVisibleBounds());
  }

  // Apply new top inset height.
  if (pending_top_inset_height_ != top_inset_height_) {
    widget_->GetNativeWindow()->SetProperty(aura::client::kTopViewInset,
                                            pending_top_inset_height_);
    top_inset_height_ = pending_top_inset_height_;
  }

  widget_->GetNativeWindow()->SetProperty(aura::client::kZOrderingKey,
                                          pending_always_on_top_
                                              ? ui::ZOrderLevel::kFloatingWindow
                                              : ui::ZOrderLevel::kNormal);

  UpdateResizability();

  ash::WindowState* window_state = GetWindowState();
  // For PIP, the snap fraction is used to specify the ideal position. Usually
  // this value is set in CompleteDrag, but for the initial position, we need
  // to set it here, when the transition is completed.
  if (window_state->IsPip() &&
      !ash::PipPositioner::HasSnapFraction(window_state)) {
    ash::PipPositioner::SaveSnapFraction(
        window_state, window_state->window()->GetBoundsInScreen());
  }

  ShellSurfaceBase::OnPostWidgetCommit();
}

void ClientControlledShellSurface::OnSurfaceDestroying(Surface* surface) {
  if (client_controlled_state_) {
    client_controlled_state_->ResetDelegate();
    client_controlled_state_ = nullptr;
  }
  ShellSurfaceBase::OnSurfaceDestroying(surface);
}

////////////////////////////////////////////////////////////////////////////////
// ClientControlledShellSurface, private:

void ClientControlledShellSurface::UpdateFrame() {
  if (!widget_)
    return;
  ash::WindowState* window_state = GetWindowState();
  bool enable_wide_frame = false;
  if (GetFrameView()->GetFrameEnabled() &&
      window_state->IsMaximizedOrFullscreenOrPinned()) {
    gfx::Rect ideal_bounds =
        GetIdealBoundsForMaximizedOrFullscreenOrPinnedState(
            widget_->GetNativeWindow());
    enable_wide_frame = ideal_bounds.width() != geometry().width();
  }
  bool update_frame = state_changed_;
  state_changed_ = false;
  if (enable_wide_frame) {
    if (!wide_frame_) {
      update_frame = true;
      wide_frame_ = std::make_unique<ash::WideFrameView>(widget_);
      chromeos::ImmersiveFullscreenController::EnableForWidget(widget_, false);
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
    DCHECK_EQ(chromeos::FrameHeader::Get(widget_),
              wide_frame_->header_view()->GetFrameHeader());
  } else {
    if (wide_frame_) {
      update_frame = true;
      chromeos::ImmersiveFullscreenController::EnableForWidget(widget_, false);
      wide_frame_.reset();
      GetFrameView()->InitImmersiveFullscreenControllerForView(
          immersive_fullscreen_controller_.get());
      // Restoring window targeter replaced by ImmersiveFullscreenController.
      InstallCustomWindowTargeter();

      UpdateCaptionButtonModel();
    }
    DCHECK_EQ(chromeos::FrameHeader::Get(widget_),
              GetFrameView()->GetHeaderView()->GetFrameHeader());
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
  bool enable_backdrop = widget_->IsFullscreen() || widget_->IsMaximized();

  ash::WindowBackdrop::BackdropMode target_backdrop_mode =
      enable_backdrop ? ash::WindowBackdrop::BackdropMode::kEnabled
                      : ash::WindowBackdrop::BackdropMode::kAuto;

  ash::WindowBackdrop* window_backdrop = ash::WindowBackdrop::Get(window);
  if (window_backdrop->mode() != target_backdrop_mode)
    window_backdrop->SetBackdropMode(target_backdrop_mode);
}

void ClientControlledShellSurface::UpdateFrameWidth() {
  int width = -1;
  if (shadow_bounds_) {
    float device_scale_factor =
        GetWidget()->GetNativeWindow()->layer()->device_scale_factor();
    float dsf_to_default_dsf = device_scale_factor / GetScale();
    width = base::ClampRound(shadow_bounds_->width() * dsf_to_default_dsf);
  }
  static_cast<chromeos::HeaderView*>(GetFrameView()->GetHeaderView())
      ->SetWidthInPixels(width);
}

void ClientControlledShellSurface::UpdateFrameType() {
  if (container_ == ash::kShellWindowId_SystemModalContainer &&
      pending_frame_type_ != SurfaceFrameType::NONE) {
    LOG(WARNING)
        << "A surface in system modal container should not have a frame:"
        << static_cast<int>(pending_frame_type_);
    return;
  }

  // TODO(oshima): We shouldn't send the synthesized motion event when just
  // changing the frame type. The better solution would be to keep the window
  // position regardless of the frame state, but that won't be available until
  // next arc version.
  // This is a stopgap solution not to generate the event until it is resolved.
  EventTargetingBlocker blocker;
  bool suppress_mouse_event = frame_type_ != pending_frame_type_ && widget_;
  if (suppress_mouse_event)
    blocker.Block(widget_->GetNativeWindow());
  ShellSurfaceBase::OnSetFrame(pending_frame_type_);
  UpdateAutoHideFrame();

  if (suppress_mouse_event)
    UpdateHostWindowOrigin();
}

bool ClientControlledShellSurface::GetCanResizeFromSizeConstraints() const {
  // Both min and max bounds of unresizable, maximized ARC windows are empty
  // because Ash requires maximizable apps have empty max bounds.
  // This assumes that ARC sets non-empty min sizes to all resizable apps.
  //
  // Example values of size constraints:
  // ----------------------------------------------------------------------
  // |           |          resizable           |      non-resizable      |
  // ----------------------------------------------------------------------
  // | freeform  | min: (400, 400), max: (0, 0) | min = max = window size |
  // ----------------------------------------------------------------------
  // | maximized | min: (400, 400), max: (0, 0) |   min = max = (0, 0)    |
  // ----------------------------------------------------------------------

  return requested_minimum_size_ != requested_maximum_size_;
}

void ClientControlledShellSurface::
    EnsureCompositorIsLockedForOrientationChange() {
  if (!orientation_compositor_lock_) {
    ui::Compositor* compositor =
        widget_->GetNativeWindow()->layer()->GetCompositor();
    orientation_compositor_lock_ = compositor->GetCompositorLock(
        this, base::Milliseconds(kOrientationLockTimeoutMs));
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

float ClientControlledShellSurface::GetClientToDpPendingScale() const {
  // When the client is scale-aware, we expect that it will resize windows when
  // reacting to scale changes. Since we do not commit the scale until the
  // buffer size changes, any bounds sent after a scale change and before the
  // scale commit will result in mismatched sizes between widget and the buffer.
  // To work around this, we use pending scale factor to calculate bounds in DP
  // instead of GetClientToDpScale().
  return use_default_scale_cancellation_ ? 1.f : 1.f / GetPendingScaleFactor();
}

gfx::Rect
ClientControlledShellSurface::GetClientBoundsForWindowBoundsAndWindowState(
    const gfx::Rect& window_bounds,
    chromeos::WindowStateType window_state) const {
  // The client's geometry uses fullscreen in client controlled,
  // (but the surface is placed under the frame), so just use
  // the window bounds instead for maximixed state.
  // Snapped window states in tablet mode do not include the caption height.
  const bool is_snapped =
      window_state == chromeos::WindowStateType::kPrimarySnapped ||
      window_state == chromeos::WindowStateType::kSecondarySnapped;
  const bool is_maximized =
      window_state == chromeos::WindowStateType::kMaximized;

  if (is_maximized ||
      (is_snapped && display::Screen::GetScreen()->InTabletMode())) {
    return window_bounds;
  }

  gfx::Rect client_bounds =
      GetFrameView()->GetFrameOverlapped()
          ? window_bounds
          : GetFrameView()->GetClientBoundsForWindowBounds(window_bounds);

  if (is_snapped && display::Screen::GetScreen()->GetTabletState() ==
                        display::TabletState::kExitingTabletMode) {
    // Until the next commit, the frame view is in immersive mode, and the above
    // GetClientBoundsForWindowBounds doesn't return bounds taking the caption
    // height into account.
    client_bounds.Inset(gfx::Insets().set_top(
        GetFrameView()->NonClientTopBorderPreferredHeight()));
  }
  return client_bounds;
}

// static
void ClientControlledShellSurface::
    SetClientControlledStateDelegateFactoryForTest(
        const DelegateFactoryCallback& callback) {
  auto& factory = GetFactoryForTesting();
  factory = callback;
}

}  // namespace exo
