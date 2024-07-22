// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/pointer.h"

#include <optional>
#include <utility>

#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/window_util.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/exo/buffer.h"
#include "components/exo/input_trace.h"
#include "components/exo/pointer_constraint_delegate.h"
#include "components/exo/pointer_delegate.h"
#include "components/exo/pointer_gesture_pinch_delegate.h"
#include "components/exo/pointer_stylus_delegate.h"
#include "components/exo/relative_pointer_delegate.h"
#include "components/exo/seat.h"
#include "components/exo/security_delegate.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/cursor_util.h"

namespace exo {

namespace {

const double kLocatedEventEpsilonSquared = 1.0 / (2000.0 * 2000.0);

bool SameLocation(const gfx::PointF& location_in_target,
                  const gfx::PointF& location) {
  // TODO(crbug.com/40859165): This is no longer necessary.  Switch to
  // std::numeric_limits<float>::eplison().
  gfx::Vector2dF offset = location_in_target - location;
  return offset.LengthSquared() < (2 * kLocatedEventEpsilonSquared);
}

// Granularity for reporting force/pressure values coming from styli or other
// devices that are normalized from 0 to 1, used to limit sending noisy values.
const float kForceGranularity = 1e-2f;

// Granularity for reporting tilt values coming from styli or other devices in
// degrees, used to limit sending noisy values.
const float kTiltGranularity = 1.f;

int GetContainerIdForMouseCursor() {
  return ash::kShellWindowId_MouseCursorContainer;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Pointer, public:

Pointer::Pointer(PointerDelegate* delegate,
                 Seat* seat,
                 std::unique_ptr<aura::Window> host_window)
    : SurfaceTreeHost("ExoPointer", std::move(host_window)),
      delegate_(delegate),
      seat_(seat),
      cursor_(ui::mojom::CursorType::kNull),
      cursor_capture_source_id_(base::UnguessableToken::Create()) {
  WMHelper* helper = WMHelper::GetInstance();
  // TODO(sky): CursorClient does not exist in mash
  // yet. https://crbug.com/631103.
  aura::client::CursorClient* cursor_client = helper->GetCursorClient();
  if (cursor_client)
    cursor_client->AddObserver(this);
  helper->AddFocusObserver(this);

  auto* drag_drop_client = helper->GetDragDropClient();
  if (drag_drop_client)
    drag_drop_client->AddObserver(this);

  ash::Shell::Get()->AddShellObserver(this);
  for (aura::Window* root : ash::Shell::GetAllRootWindows()) {
    root->AddPreTargetHandler(this);
  }

  ash::DesksController::Get()->AddObserver(this);
}

Pointer::~Pointer() {
  ash::DesksController::Get()->RemoveObserver(this);

  ash::Shell::Get()->RemoveShellObserver(this);
  for (aura::Window* root : ash::Shell::GetAllRootWindows()) {
    root->RemovePreTargetHandler(this);
  }

  WMHelper* helper = WMHelper::GetInstance();
  // Remove the pretarget handler in case the pointer is deleted
  // w/o disabling pointer capture.
  aura::Env::GetInstance()->RemovePreTargetHandler(this);

  delegate_->OnPointerDestroying(this);
  if (focus_surface_)
    focus_surface_->RemoveSurfaceObserver(this);
  if (pinch_delegate_)
    pinch_delegate_->OnPointerDestroying(this);
  if (relative_pointer_delegate_)
    relative_pointer_delegate_->OnPointerDestroying(this);
  if (pointer_constraint_delegate_) {
    pointer_constraint_delegate_->GetConstrainedSurface()
        ->RemoveSurfaceObserver(this);
    VLOG(1) << "Pointer constraint broken by pointer destruction";
    pointer_constraint_delegate_->OnConstraintBroken();
  }
  for (auto it : constraints_) {
    it.first->RemoveSurfaceObserver(this);
    it.second->OnDefunct();
  }
  if (stylus_delegate_)
    stylus_delegate_->OnPointerDestroying(this);
  // TODO(sky): CursorClient does not exist in mash
  // yet. https://crbug.com/631103.
  aura::client::CursorClient* cursor_client = helper->GetCursorClient();
  if (cursor_client)
    cursor_client->RemoveObserver(this);
  if (root_surface())
    root_surface()->RemoveSurfaceObserver(this);
  helper->RemoveFocusObserver(this);

  auto* drag_drop_client = helper->GetDragDropClient();
  if (drag_drop_client)
    drag_drop_client->RemoveObserver(this);
}

void Pointer::SetCursor(Surface* surface, const gfx::Point& hotspot) {
  if (!focus_surface_ && !capture_window_)
    return;

  // This is used to avoid unnecessary cursor changes.
  bool cursor_changed = false;

  // If surface is different than the current pointer surface then remove the
  // current surface and add the new surface.
  if (surface != root_surface()) {
    if (surface && surface->HasSurfaceDelegate()) {
      DLOG(ERROR) << "Surface has already been assigned a role";
      return;
    }
    UpdatePointerSurface(surface);
    cursor_changed = true;
  } else if (!surface && cursor_ != ui::mojom::CursorType::kNone) {
    cursor_changed = true;
  }

  if (hotspot != hotspot_) {
    hotspot_ = hotspot;
    cursor_changed = true;
  }

  // Early out if cursor did not change.
  if (!cursor_changed)
    return;

  // If |SurfaceTreeHost::root_surface_| is set then asynchronously capture a
  // snapshot of cursor, otherwise cancel pending capture and immediately set
  // the cursor to "none".
  if (root_surface()) {
    cursor_ = ui::mojom::CursorType::kCustom;
    CaptureCursor(hotspot);
  } else {
    cursor_ = ui::mojom::CursorType::kNone;
    cursor_bitmap_.reset();
    cursor_capture_weak_ptr_factory_.InvalidateWeakPtrs();
    UpdateCursor();
  }
}

void Pointer::SetCursorType(ui::mojom::CursorType cursor_type) {
  // Early out if the pointer doesn't have a surface in focus.
  if (!focus_surface_)
    return;

  if (cursor_ == cursor_type)
    return;
  cursor_ = cursor_type;
  cursor_bitmap_.reset();
  UpdatePointerSurface(nullptr);
  cursor_capture_weak_ptr_factory_.InvalidateWeakPtrs();
  UpdateCursor();
}

void Pointer::SetGesturePinchDelegate(PointerGesturePinchDelegate* delegate) {
  // For the |pinch_delegate_| (and |relative_pointer_delegate_| below) it is
  // possible to bind multiple extensions to the same pointer interface (not
  // that this is a particularly reasonable thing to do). When that happens we
  // choose to only keep a single binding alive, so we simulate pointer
  // destruction for the previous binding.
  if (pinch_delegate_)
    pinch_delegate_->OnPointerDestroying(this);
  pinch_delegate_ = delegate;
}

void Pointer::RegisterRelativePointerDelegate(
    RelativePointerDelegate* delegate) {
  if (relative_pointer_delegate_)
    relative_pointer_delegate_->OnPointerDestroying(this);
  relative_pointer_delegate_ = delegate;
}

void Pointer::UnregisterRelativePointerDelegate(
    RelativePointerDelegate* delegate) {
  DCHECK(relative_pointer_delegate_ == delegate);
  relative_pointer_delegate_ = nullptr;
}

bool Pointer::ConstrainPointer(PointerConstraintDelegate* delegate) {
  // Pointer lock is a chromeos-only feature (i.e. the chromeos::features
  // namespace only exists in chromeos builds). So we do not compile pointer
  // lock support unless we are on chromeos.
  Surface* constrained_surface = delegate->GetConstrainedSurface();
  if (!constrained_surface) {
    delegate->OnDefunct();
    return false;
  }

  // Permission of Pointer lock is controlled by SecurityDelegate, created per
  // server instance. Default implementation allows this for ARC and Lacros
  // windows which have their own security mechanism and are considered trusted.
  aura::Window* toplevel = constrained_surface->window()->GetToplevelWindow();

  SecurityDelegate* security_delegate =
      constrained_surface->GetSecurityDelegate();
  // |security_delegate| could be nullptr, if:
  // - the surface hasn't been assigned a role; or
  // - a role has been assigned, but that specific role doesn't set a security
  //   delegate.
  bool permitted =
      security_delegate && security_delegate->CanLockPointer(toplevel);
  if (!permitted) {
    delegate->OnDefunct();
    return false;
  }

  // Can only have one active constraint request per surface
  auto result = constraints_.try_emplace(constrained_surface, delegate);
  if (result.first->second != delegate) {
    VLOG(1) << "Pointer constraint not granted; one already exists.";
    delegate->OnAlreadyConstrained();
    delegate->OnDefunct();
    return false;
  }

  if (!constrained_surface->HasSurfaceObserver(this))
    constrained_surface->AddSurfaceObserver(this);

  bool success = EnablePointerCapture(constrained_surface);
  if (success) {
    pointer_constraint_delegate_ = delegate;
    delegate->OnConstraintActivated();
  }
  return success;
}

bool Pointer::UnconstrainPointerByUserAction() {
  // Prevent pointer capture until the next user action that permits it,
  // even if a constraint is currently not active (to prevent an app from
  // rapidly toggling pointer capture to evade such prevention).
  capture_permitted_ = false;
  UpdateCursor();  // forces the cursor to be visible in case the app hid it

  if (pointer_constraint_delegate_ && capture_window_) {
    VLOG(1) << "Pointer constraint broken by user action";
    UnconstrainPointer();
    return true;
  } else {
    VLOG(1) << "Pointer constraint forbidden by user (though none active now)";
    return false;
  }
}

void Pointer::RemoveConstraintDelegate(PointerConstraintDelegate* delegate) {
  delegate->OnDefunct();

  Surface* surface = delegate->GetConstrainedSurface();
  auto it = constraints_.find(surface);
  if (it != constraints_.end() && it->second == delegate) {
    constraints_.erase(it);
    MaybeRemoveSurfaceObserver(surface);
  }
}

void Pointer::UnconstrainPointer() {
  if (pointer_constraint_delegate_) {
    pointer_constraint_delegate_->OnConstraintBroken();
    if (!pointer_constraint_delegate_->IsPersistent()) {
      RemoveConstraintDelegate(pointer_constraint_delegate_);
    }
    pointer_constraint_delegate_ = nullptr;
    DisablePointerCapture();
  }
}

void Pointer::MaybeReactivatePointerConstraint(Surface* surface) {
  if (!pointer_constraint_delegate_ && surface) {
    auto it = constraints_.find(surface);
    if (it != constraints_.end())
      ConstrainPointer(it->second);
  }
}

void Pointer::OnPointerConstraintDelegateDestroying(
    PointerConstraintDelegate* delegate) {
  if (pointer_constraint_delegate_ == delegate) {
    DisablePointerCapture();
    pointer_constraint_delegate_ = nullptr;
  }
  RemoveConstraintDelegate(delegate);
}

bool Pointer::EnablePointerCapture(Surface* capture_surface) {
  if (!capture_permitted_) {
    VLOG(1) << "Unable to re-capture the pointer due to previous user action.";
    return false;
  }

  aura::Window* window = capture_surface->window();
  aura::Window* active_window = WMHelper::GetInstance()->GetActiveWindow();
  if (!active_window || !active_window->Contains(window)) {
    VLOG(1) << "Cannot enable pointer capture on an inactive window.";
    return false;
  }

  capture_window_ = window;

  // Add a pre-target handler that can consume all mouse events before it gets
  // sent to other targets. If there's an ongoing animation, the pre-target
  // handler will be added once `OnDeskSwitchAnimationFinished` is triggered.
  if (!ash::DesksController::Get()->animation()) {
    aura::Env::GetInstance()->AddPreTargetHandler(
        this, ui::EventTarget::Priority::kSystem);
  }

  location_when_pointer_capture_enabled_ =
      gfx::ToRoundedPoint(location_in_root_);

  if (ShouldMoveToCenter())
    MoveCursorToCenterOfActiveDisplay();

  seat_->NotifyPointerCaptureEnabled(this, window);

  return true;
}

void Pointer::DisablePointerCapture() {
  // Early out if pointer capture is not enabled.
  if (!capture_window_)
    return;

  // Remove the pre-target handler that consumes all mouse events.
  aura::Env::GetInstance()->RemovePreTargetHandler(this);

  aura::Window* root = capture_window_->GetRootWindow();
  gfx::Point p = location_when_pointer_capture_enabled_
                     ? *location_when_pointer_capture_enabled_
                     : root->bounds().CenterPoint();
  expected_next_mouse_location_ = p;
  root->MoveCursorTo(p);

  aura::Window* window = capture_window_;
  capture_window_ = nullptr;
  location_when_pointer_capture_enabled_.reset();
  UpdateCursor();

  seat_->NotifyPointerCaptureDisabled(this, window);
}

void Pointer::SetStylusDelegate(PointerStylusDelegate* delegate) {
  stylus_delegate_ = delegate;

  // Reset last reported values to default.
  last_pointer_type_ = ui::EventPointerType::kUnknown;
  last_force_ = std::numeric_limits<float>::quiet_NaN();
  last_tilt_ = gfx::Vector2dF();
}

bool Pointer::HasStylusDelegate() const {
  return !!stylus_delegate_;
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceDelegate overrides:

void Pointer::OnSurfaceCommit() {
  SurfaceTreeHost::OnSurfaceCommit();

  // Capture new cursor to reflect result of commit.
  if (focus_surface_)
    CaptureCursor(hotspot_);
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceObserver overrides:

void Pointer::OnSurfaceDestroying(Surface* surface) {
  bool was_correctly_subscribed = false;
  if (surface && pointer_constraint_delegate_ &&
      surface == pointer_constraint_delegate_->GetConstrainedSurface()) {
    surface->RemoveSurfaceObserver(this);
    VLOG(1) << "Pointer constraint broken by surface destruction";
    UnconstrainPointer();
    was_correctly_subscribed = true;
  }
  if (surface && surface->window() == capture_window_) {
    DisablePointerCapture();
    was_correctly_subscribed = true;
  }

  auto it = constraints_.find(surface);
  if (it != constraints_.end()) {
    it->second->OnDefunct();
    constraints_.erase(it);
    surface->RemoveSurfaceObserver(this);
    was_correctly_subscribed = true;
  }

  if (surface == focus_surface_) {
    SetFocus(nullptr, gfx::PointF(), gfx::PointF(), 0);
    was_correctly_subscribed = true;
  } else if (surface == root_surface()) {
    UpdatePointerSurface(nullptr);
    was_correctly_subscribed = true;
  }
  DCHECK(was_correctly_subscribed);
  DCHECK(!surface->HasSurfaceObserver(this));
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler overrides:

void Pointer::OnMouseEvent(ui::MouseEvent* event) {
  if (seat_->was_shutdown() || event->handled())
    return;
  // Ask seat instead of ash's DragDropController because it ends
  // asynchronously.
  if (seat_->IsDragDropOperationInProgress()) {
    return;
  } else if (button_flags_on_drag_drop_start_) {
    // Send release events for buttons that are released during the drag and
    // drop operation.
    int released_button_flags =
        button_flags_on_drag_drop_start_ & ~event->button_flags();
    delegate_->OnPointerButton(event->time_stamp(), released_button_flags,
                               false);
    delegate_->OnPointerFrame();
    button_flags_on_drag_drop_start_ = 0;
  }

  // Nothing to report to a client nor have to update the pointer when capture
  // changes.
  if (event->type() == ui::EventType::kMouseCaptureChanged) {
    return;
  }

  // TODO(crbug.com/1395073, crbug.com/1395256): Currently, due to a bug in
  // multi-display implementation, mouse move event sent to hide cursor is
  // sent twice occasionally. That confuses focus tracking implemented in this
  // class.
  // For the short term workaround, we ignore such events.
  // Note that this is not a *correct* implementation, because we have to send
  // the correconding wayland event to client (such as Lacros) with carrying
  // the info that it is triggered for cursor hiding to let it take an action
  // on cursor hiding (e.g. hiding hover, too).
  // We need to fix the implementation here, though, it depends on the fix of
  // multi-display event tracking.
  if (event->flags() & ui::EF_CURSOR_HIDE) {
    return;
  }

  // Fling cancel is generated very generously at every touch of the
  // touchpad. Since it's not directly supported by the delegate, we want
  // limit this event to only right after a fling start has been generated
  // to prevent erronous behavior.
  if (event->type() == ui::EventType::kScrollFlingCancel &&
      last_event_type_ != ui::EventType::kScrollFlingStart) {
    // Should we update this for above cases?
    last_event_type_ = event->type();
    return;
  }

  gfx::PointF location_in_target;
  Surface* target = GetEffectiveTargetForEvent(event, &location_in_target);
  gfx::PointF location_in_root = event->root_location_f();

  // Update focus if target is different than the current pointer focus.
  if (target != focus_surface_) {
    SetFocus(target, location_in_root, location_in_target,
             event->button_flags());
  }

  if (!focus_surface_)
    return;

  TRACE_EXO_INPUT_EVENT(event);

  bool needs_frame = false;

  const auto& details = event->pointer_details();
  if (stylus_delegate_ && last_pointer_type_ != details.pointer_type) {
    last_pointer_type_ = details.pointer_type;
    stylus_delegate_->OnPointerToolChange(details.pointer_type);
    needs_frame |= true;
  }

  if (event->IsMouseEvent()) {
    // Ordinal motion is sent only on platforms that support it, which is
    // indicated by the presence of a flag.
    std::optional<gfx::Vector2dF> ordinal_motion = std::nullopt;
    if (event->flags() & ui::EF_UNADJUSTED_MOUSE &&
        base::FeatureList::IsEnabled(ash::features::kExoOrdinalMotion)) {
      ordinal_motion = event->movement();
    }

    // Generate motion event if location changed or the location hasn't been
    // sent yet. We need to check location here as mouse movement can generate
    // both "moved" and "entered" events but OnPointerMotion should only be
    // called if location changed since OnPointerEnter was called.
    if (!CheckIfSameLocation(event->IsSynthesized(), location_in_root,
                             location_in_target)) {
      bool ignore_motion = false;
      if (expected_next_mouse_location_) {
        const gfx::Point& expected = *expected_next_mouse_location_;
        // Since MoveCursorTo() takes integer coordinates, the resulting move
        // could have a conversion error of up to 2 due to fractional scale
        // factors.
        if (std::abs(location_in_root.x() - expected.x()) <= 2 &&
            std::abs(location_in_root.y() - expected.y()) <= 2) {
          // This was a synthetic move event, so do not forward it and clear the
          // expected location.
          expected_next_mouse_location_.reset();
          ignore_motion = true;
        }
      }
      needs_frame |= !ignore_motion &&
                     HandleRelativePointerMotion(
                         event->time_stamp(), location_in_root, ordinal_motion);
      if (capture_window_) {
        if (ShouldMoveToCenter())
          MoveCursorToCenterOfActiveDisplay();
        location_in_root_ = location_in_root;
        location_in_surface_ = location_in_target;
      } else if (event->type() != ui::EventType::kMouseExited &&
                 !ignore_motion) {
        delegate_->OnPointerMotion(event->time_stamp(), location_in_target);
        needs_frame |= true;
        location_in_root_ = location_in_root;
        location_in_surface_ = location_in_target;
      }
    }
  }
  switch (event->type()) {
    case ui::EventType::kMouseReleased:
      seat_->AbortPendingDragOperation();
      [[fallthrough]];
    case ui::EventType::kMousePressed: {
      if (!capture_permitted_) {
        // Clicking any surface with a constraint delegate permits capture
        auto it = constraints_.find(focus_surface_);
        if (it != constraints_.end()) {
          capture_permitted_ = true;
          UpdateCursor();
          ConstrainPointer(it->second);
        }
      }
      delegate_->OnPointerButton(event->time_stamp(),
                                 event->changed_button_flags(),
                                 event->type() == ui::EventType::kMousePressed);
      needs_frame |= true;
      break;
    }
    case ui::EventType::kScroll: {
      ui::ScrollEvent* scroll_event = static_cast<ui::ScrollEvent*>(event);

      // Scrolling with 3+ fingers should not be handled since it will be used
      // to trigger overview mode.
      if (scroll_event->finger_count() >= 3)
        break;
      delegate_->OnPointerScroll(
          event->time_stamp(),
          gfx::Vector2dF(scroll_event->x_offset(), scroll_event->y_offset()),
          false);
      needs_frame |= true;
      break;
    }
    case ui::EventType::kMousewheel: {
      delegate_->OnPointerScroll(
          event->time_stamp(),
          static_cast<ui::MouseWheelEvent*>(event)->offset(), true);
      needs_frame |= true;
      break;
    }
    case ui::EventType::kScrollFlingStart: {
      // Fling start in chrome signals the lifting of fingers after scrolling.
      // In wayland terms this signals the end of a scroll sequence.
      delegate_->OnFingerScrollStop(event->time_stamp());
      needs_frame |= true;
      break;
    }
    case ui::EventType::kScrollFlingCancel: {
      // We emulate fling cancel by starting a new scroll sequence that
      // scrolls by 0 pixels, effectively stopping any kinetic scroll motion.
      delegate_->OnPointerScroll(event->time_stamp(), gfx::Vector2dF(), false);
      delegate_->OnPointerFrame();
      delegate_->OnFingerScrollStop(event->time_stamp());
      delegate_->OnPointerFrame();
      break;
    }
    case ui::EventType::kMouseMoved:
    case ui::EventType::kMouseDragged:
    case ui::EventType::kMouseEntered:
    case ui::EventType::kMouseExited:
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  if (stylus_delegate_) {
    // Report the force value when either:
    // - switching from a device that supports force to one that doesn't or
    //   vice-versa (since force is NaN if the device doesn't support it), OR
    // - the force value differs from the last reported force by greater than
    //   the granularity.
    // Using std::isgreaterequal for quiet error handling for NaNs.
    if (std::isnan(last_force_) != std::isnan(details.force) ||
        std::isgreaterequal(abs(last_force_ - details.force),
                            kForceGranularity)) {
      last_force_ = details.force;
      stylus_delegate_->OnPointerForce(event->time_stamp(), details.force);
      needs_frame |= true;
    }
    if (abs(last_tilt_.x() - details.tilt_x) >= kTiltGranularity ||
        abs(last_tilt_.y() - details.tilt_y) >= kTiltGranularity) {
      last_tilt_ = gfx::Vector2dF(details.tilt_x, details.tilt_y);
      stylus_delegate_->OnPointerTilt(event->time_stamp(), last_tilt_);
      needs_frame |= true;
    }
  }

  last_event_type_ = event->type();

  // Consume all mouse events when pointer capture is enabled.
  if (capture_window_) {
    event->SetHandled();
    event->StopPropagation();
  }

  if (needs_frame)
    delegate_->OnPointerFrame();
}

void Pointer::OnScrollEvent(ui::ScrollEvent* event) {
  OnMouseEvent(event);
}

void Pointer::OnGestureEvent(ui::GestureEvent* event) {
  // We don't want to handle gestures generated from touchscreen events,
  // we handle touch events in touch.cc
  if (event->details().device_type() != ui::GestureDeviceType::DEVICE_TOUCHPAD)
    return;

  if (!focus_surface_ || !pinch_delegate_)
    return;

  TRACE_EXO_INPUT_EVENT(event);

  switch (event->type()) {
    case ui::EventType::kGesturePinchBegin:
      pinch_delegate_->OnPointerPinchBegin(event->unique_touch_event_id(),
                                           event->time_stamp(), focus_surface_);
      delegate_->OnPointerFrame();
      break;
    case ui::EventType::kGesturePinchUpdate:
      pinch_delegate_->OnPointerPinchUpdate(event->time_stamp(),
                                            event->details().scale());
      delegate_->OnPointerFrame();
      break;
    case ui::EventType::kGesturePinchEnd:
      pinch_delegate_->OnPointerPinchEnd(event->unique_touch_event_id(),
                                         event->time_stamp());
      delegate_->OnPointerFrame();
      break;
    default:
      break;
  }

  // Consume all mouse events when pointer capture is enabled.
  if (capture_window_) {
    event->SetHandled();
    event->StopPropagation();
  }
}

////////////////////////////////////////////////////////////////////////////////
// aura::client::DragDropClientObserver overrides:
void Pointer::OnDragStarted() {
  button_flags_on_drag_drop_start_ =
      aura::Env::GetInstance()->mouse_button_flags();

  // Drag 'n drop operations driven by sources different than pointer/mouse
  // should have not effect here.
  WMHelper* helper = WMHelper::GetInstance();
  if (auto* drag_drop_client = helper->GetDragDropClient()) {
    if (static_cast<ash::DragDropController*>(drag_drop_client)
            ->event_source() != ui::mojom::DragEventSource::kMouse)
      return;
  }

  SetFocus(nullptr, gfx::PointF(), gfx::PointF(), 0);
}

void Pointer::OnDragCompleted(const ui::DropTargetEvent& event) {
  // Don't update the focus here as the DragDropOperation is still processing
  // the DnD and hasn't sent drop/leave events.
  // The focus has been reset upon DnD start above, and will be updated on
  // next Mouse Event.
  // This is not ideal, but the better fix should be done as a part of
  // DnD nested loop removal. (crbug.com/1160925)
}

////////////////////////////////////////////////////////////////////////////////
// aura::client::CursorClientObserver overrides:

void Pointer::OnCursorSizeChanged(ui::CursorSize cursor_size) {
  if (!focus_surface_)
    return;

  if (cursor_ != ui::mojom::CursorType::kNull)
    UpdateCursor();
}

void Pointer::OnCursorDisplayChanged(const display::Display& display) {
  UpdatePointerSurface(root_surface());

  auto* cursor_client = WMHelper::GetInstance()->GetCursorClient();
  DCHECK(cursor_client);
  if (cursor_ == ui::mojom::CursorType::kCustom &&
      cursor_ == cursor_client->GetCursor()) {
    // If the current cursor is still the one created by us,
    // it's our responsibility to update the cursor for the new display.
    // Don't check |focus_surface_| because it can be null while
    // dragging the window due to an event capture.
    UpdateCursor();
  }
}

////////////////////////////////////////////////////////////////////////////////
// aura::client::FocusChangeObserver overrides:

void Pointer::OnWindowFocused(aura::Window* gained_focus,
                              aura::Window* lost_focus) {
  if (capture_window_ && capture_window_ != gained_focus) {
    if (pointer_constraint_delegate_) {
      VLOG(1) << "Pointer constraint broken by focus change";
      UnconstrainPointer();
    } else {
      DisablePointerCapture();
    }
  }
  if (gained_focus)
    MaybeReactivatePointerConstraint(Surface::AsSurface(gained_focus));
}

////////////////////////////////////////////////////////////////////////////////
// ash::ShellObserver:
void Pointer::OnRootWindowAdded(aura::Window* root_window) {
  root_window->AddPreTargetHandler(this);
}

void Pointer::OnRootWindowWillShutdown(aura::Window* root_window) {
  root_window->RemovePreTargetHandler(this);
}

////////////////////////////////////////////////////////////////////////////////
// ash::DesksController::Observer:
void Pointer::OnDeskSwitchAnimationFinished() {
  if (capture_window_) {
    aura::Env::GetInstance()->AddPreTargetHandler(
        this, ui::EventTarget::Priority::kSystem);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Pointer, private:

Surface* Pointer::GetEffectiveTargetForEvent(
    const ui::LocatedEvent* event,
    gfx::PointF* location_in_target) const {
  DCHECK(location_in_target);
  Surface* target = nullptr;
  if (capture_window_) {
    target = Surface::AsSurface(capture_window_);
  } else {
    target = GetTargetSurfaceForLocatedEvent(event);

    if (!target || !delegate_->CanAcceptPointerEventsForSurface(target))
      return nullptr;
  }

  if (target) {
    *location_in_target = event->location_f();
    aura::Window::ConvertPointToTarget(
        static_cast<aura::Window*>(event->target()), target->window(),
        location_in_target);
  }
  return target;
}

void Pointer::SetFocus(Surface* surface,
                       const gfx::PointF& root_location,
                       const gfx::PointF& surface_location,
                       int button_flags) {
  DCHECK(!surface || delegate_->CanAcceptPointerEventsForSurface(surface));
  // First generate a leave event if we currently have a target in focus.
  if (focus_surface_) {
    delegate_->OnPointerLeave(focus_surface_);
    delegate_->OnPointerFrame();
    // Require SetCursor() to be called and cursor to be re-defined in
    // response to each OnPointerEnter() call.
    Surface* old_surface = focus_surface_;
    focus_surface_ = nullptr;
    MaybeRemoveSurfaceObserver(old_surface);
    cursor_capture_weak_ptr_factory_.InvalidateWeakPtrs();
  }
  // Second generate an enter event if focus moved to a new surface.
  if (surface) {
    // Pointer enter should not be generated during dnd session.
#if DCHECK_IS_ON()
    auto* drag_drop_controller = static_cast<ash::DragDropController*>(
        aura::client::GetDragDropClient(surface->window()->GetRootWindow()));
    DCHECK(!drag_drop_controller->IsDragDropInProgress());
#endif
    delegate_->OnPointerEnter(surface, surface_location, button_flags);
    delegate_->OnPointerFrame();
    location_in_root_ = root_location;
    location_in_surface_ = surface_location;
    focus_surface_ = surface;
    if (!focus_surface_->HasSurfaceObserver(this))
      focus_surface_->AddSurfaceObserver(this);
  }
  UpdateCursor();
}

void Pointer::UpdatePointerSurface(Surface* surface) {
  if (root_surface()) {
    host_window()->SetTransform(gfx::Transform());
    if (host_window()->parent())
      host_window()->parent()->RemoveChild(host_window());
    Surface* old_surface = root_surface();
    SetRootSurface(nullptr);
    MaybeRemoveSurfaceObserver(old_surface);
  }

  if (surface) {
    if (!surface->HasSurfaceObserver(this))
      surface->AddSurfaceObserver(this);
    // Note: Surface window needs to be added to the tree so we can take a
    // snapshot. Where in the tree is not important but we might as well use
    // the cursor container.
    WMHelper::GetInstance()
        ->GetPrimaryDisplayContainer(GetContainerIdForMouseCursor())
        ->AddChild(host_window());
    SetRootSurface(surface);
  }
}

void Pointer::CaptureCursor(const gfx::Point& hotspot) {
  DCHECK(root_surface());
  DCHECK(focus_surface_);

  // Defer capture until surface commit.
  if (host_window()->bounds().IsEmpty())
    return;

  // Return if the surface has no committed buffer.
  Buffer* buffer = root_surface()->GetBuffer();
  if (!buffer) {
    return;
  }

  // Cancel all pending captures.
  cursor_capture_weak_ptr_factory_.InvalidateWeakPtrs();

  // If bitmap can be directly created from the buffer,
  // use the bitmap to create cursor.
  // Otherwise, send RequestCopyOfOutput request to viz
  // to capture cursor bitmap.
  if (!root_surface()->HasAcquireFence()) {
    SkBitmap bitmap = buffer->CreateBitmap();
    if (!bitmap.empty()) {
      OnCursorBitmapObtained(hotspot, bitmap, root_surface()->GetBufferScale());
      return;
    }
  }

  // Advance the surface id to ensure capturing the correct compositor frame.
  AllocateLocalSurfaceId();
  // Submit compositor frame to be captured.
  SubmitCompositorFrame();

  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA,
          viz::CopyOutputRequest::ResultDestination::kSystemMemory,
          base::BindOnce(&Pointer::OnCursorCaptured,
                         cursor_capture_weak_ptr_factory_.GetWeakPtr(),
                         hotspot));
  request->set_result_task_runner(
      base::SequencedTaskRunner::GetCurrentDefault());

  request->set_source(cursor_capture_source_id_);

  aura::Env::GetInstance()
      ->context_factory()
      ->GetHostFrameSinkManager()
      ->RequestCopyOfOutput(GetSurfaceId(), std::move(request));
}

void Pointer::OnCursorCaptured(const gfx::Point& hotspot,
                               std::unique_ptr<viz::CopyOutputResult> result) {
  // Only successful captures should update the cursor.
  if (result->IsEmpty())
    return;

  OnCursorBitmapObtained(hotspot,
                         result->ScopedAccessSkBitmap().GetOutScopedBitmap(),
                         GetScaleFactor());
}

void Pointer::OnCursorBitmapObtained(const gfx::Point& hotspot,
                                     const SkBitmap& cursor_bitmap,
                                     float cursor_scale) {
  if (!focus_surface_) {
    return;
  }

  cursor_bitmap_ = cursor_bitmap;
  cursor_scale_ = cursor_scale;
  DCHECK(cursor_bitmap_.readyToDraw());
  cursor_hotspot_ = hotspot;
  UpdateCursor();
}

void Pointer::UpdateCursor() {
  WMHelper* helper = WMHelper::GetInstance();
  aura::client::CursorClient* cursor_client = helper->GetCursorClient();
  DCHECK(cursor_client);

  if (cursor_ == ui::mojom::CursorType::kCustom) {
    SkBitmap bitmap = cursor_bitmap_;

    // TODO(oshima|weidongg): Add cutsom cursor API to handle size/display
    // change without explicit management like this. https://crbug.com/721601.

    // Scaling bitmap to match the corresponding supported scale factor of ash.
    const display::Display& display = cursor_client->GetDisplay();
    const float resource_scale_factor = ui::GetScaleForResourceScaleFactor(
        ui::GetSupportedResourceScaleFactor(display.device_scale_factor()));
    const float scale = resource_scale_factor / cursor_scale_;
    gfx::Point hotspot =
        gfx::ScaleToFlooredPoint(cursor_hotspot_, cursor_scale_);
    // Use panel_rotation() rather than "natural" rotation, as it actually
    // relates to the hardware you're about to draw the cursor bitmap on.
    wm::ScaleAndRotateCursorBitmapAndHotpoint(scale, display.panel_rotation(),
                                              &bitmap, &hotspot);

    // TODO(reveman): Add interface for creating cursors from GpuMemoryBuffers
    // and use that here instead of the current bitmap API.
    // https://crbug.com/686600
    cursor_ = ui::Cursor::NewCustom(std::move(bitmap), std::move(hotspot),
                                    resource_scale_factor);
    cursor_.SetPlatformCursor(
        ui::CursorFactory::GetInstance()->CreateImageCursor(
            cursor_.type(), cursor_.custom_bitmap(), cursor_.custom_hotspot(),
            cursor_.image_scale_factor()));
  }

  // When pointer capture is broken, use the standard system cursor instead of
  // the application-requested one. But we keep the app-requested cursor around
  // for when capture becomes permitted again.
  const ui::Cursor& cursor =
      capture_permitted_ ? cursor_ : ui::mojom::CursorType::kPointer;

  // If there is a focused surface, update its widget as the views framework
  // expect that Widget knows the current cursor. Otherwise update the
  // cursor directly on CursorClient.
  if (focus_surface_) {
    aura::Window* window = focus_surface_->window();
    do {
      views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
      if (widget) {
        widget->SetCursor(cursor);
        return;
      }
      window = window->parent();
    } while (window);
  } else {
    cursor_client->SetCursor(cursor);
  }
}

bool Pointer::ShouldMoveToCenter() {
  if (!capture_window_)
    return false;

  gfx::Rect rect = capture_window_->GetRootWindow()->bounds();
  rect.Inset(gfx::Insets::VH(rect.height() / 6, rect.width() / 6));
  return !rect.Contains(location_in_root_.x(), location_in_root_.y());
}

void Pointer::MoveCursorToCenterOfActiveDisplay() {
  if (!capture_window_)
    return;
  aura::Window* root = capture_window_->GetRootWindow();
  gfx::Point p = root->bounds().CenterPoint();
  expected_next_mouse_location_ = p;
  root->MoveCursorTo(p);
}

bool Pointer::HandleRelativePointerMotion(
    base::TimeTicks time_stamp,
    gfx::PointF location_in_root,
    const std::optional<gfx::Vector2dF>& ordinal_motion) {
  if (!relative_pointer_delegate_)
    return false;

  gfx::Vector2dF delta = location_in_root - location_in_root_;
  relative_pointer_delegate_->OnPointerRelativeMotion(
      time_stamp, delta,
      ordinal_motion.has_value() ? ordinal_motion.value() : delta);
  return true;
}

bool Pointer::ShouldObserveSurface(Surface* surface) {
  if (!surface)
    return false;

  if (surface == root_surface() || surface == focus_surface_ ||
      constraints_.find(surface) != constraints_.end()) {
    return true;
  }
  return false;
}

void Pointer::MaybeRemoveSurfaceObserver(Surface* surface) {
  if (!ShouldObserveSurface(surface)) {
    surface->RemoveSurfaceObserver(this);
  }
}

bool Pointer::CheckIfSameLocation(bool is_synthesized,
                                  const gfx::PointF& location_in_root,
                                  const gfx::PointF& location_in_target) {
  // There is a specific case that location_in_root is the same
  // but location_in_target is updated with SynthesizeMouseMove
  // without the actual mouse movement when the window bounds changes.
  // To handle this case, PointerMotion event should be delievered to
  // delegate to update the current pointer location properly.
  // Hence, check either target or root has changed.
  if (!is_synthesized) {
    return SameLocation(location_in_root, location_in_root_) &&
           SameLocation(location_in_target, location_in_surface_);
  }

  // For synthesized events, they typically lack floating point precision
  // so to avoid generating mouse event jitter we consider the location of
  // these events to be the same as |location| if floored values match.
  return (gfx::ToFlooredPoint(location_in_root) ==
          gfx::ToFlooredPoint(location_in_root_)) &&
         (gfx::ToFlooredPoint(location_in_target) ==
          gfx::ToFlooredPoint(location_in_surface_));
}

}  // namespace exo
