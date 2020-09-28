// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/pointer.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/exo/input_trace.h"
#include "components/exo/pointer_constraint_delegate.h"
#include "components/exo/pointer_delegate.h"
#include "components/exo/pointer_gesture_pinch_delegate.h"
#include "components/exo/pointer_stylus_delegate.h"
#include "components/exo/relative_pointer_delegate.h"
#include "components/exo/seat.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/cursor_util.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/window_util.h"
#include "chromeos/constants/chromeos_features.h"
#endif

namespace exo {

// Controls Pointer capture in exo/wayland.
const base::Feature kPointerCapture{"ExoPointerCapture",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

namespace {

// TODO(oshima): Some accessibility features, including large cursors, disable
// hardware cursors. Ash does not support compositing for custom cursors, so it
// replaces them with the default cursor. As a result, this scale has no effect
// for now. See crbug.com/708378.
const float kLargeCursorScale = 2.8f;

const double kLocatedEventEpsilonSquared = 1.0 / (2000.0 * 2000.0);

bool SameLocation(const gfx::PointF& location_in_target,
                  const gfx::PointF& location) {
  // In general, it is good practice to compare floats using an epsilon.
  // In particular, the mouse location_f() could differ between the
  // MOUSE_PRESSED and MOUSE_RELEASED events. At MOUSE_RELEASED, it will have a
  // targeter() already cached, while at MOUSE_PRESSED, it will have to
  // calculate it passing through all the hierarchy of windows, and that could
  // generate rounding error. std::numeric_limits<float>::epsilon() is not big
  // enough to catch this rounding error.
  gfx::Vector2dF offset = location_in_target - location;
  return offset.LengthSquared() < (2 * kLocatedEventEpsilonSquared);
}

// Granularity for reporting force/pressure values coming from styli or other
// devices that are normalized from 0 to 1, used to limit sending noisy values.
const float kForceGranularity = 1e-2f;

// Granularity for reporting tilt values coming from styli or other devices in
// degrees, used to limit sending noisy values.
const float kTiltGranularity = 1.f;

display::ManagedDisplayInfo GetCaptureDisplayInfo() {
  display::ManagedDisplayInfo capture_info;
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    const auto& info = WMHelper::GetInstance()->GetDisplayInfo(display.id());
    if (info.device_scale_factor() >= capture_info.device_scale_factor())
      capture_info = info;
  }
  return capture_info;
}

int GetContainerIdForMouseCursor() {
#if defined(OS_CHROMEOS)
  return ash::kShellWindowId_MouseCursorContainer;
#else
  NOTIMPLEMENTED();
  return -1;
#endif
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Pointer, public:

Pointer::Pointer(PointerDelegate* delegate, Seat* seat)
    : SurfaceTreeHost("ExoPointer"),
      delegate_(delegate),
      seat_(seat),
      cursor_(ui::mojom::CursorType::kNull),
      capture_scale_(GetCaptureDisplayInfo().device_scale_factor()),
      capture_ratio_(GetCaptureDisplayInfo().GetDensityRatio()),
      cursor_capture_source_id_(base::UnguessableToken::Create()) {
  WMHelper* helper = WMHelper::GetInstance();
  helper->AddPreTargetHandler(this);
  // TODO(sky): CursorClient does not exist in mash
  // yet. https://crbug.com/631103.
  aura::client::CursorClient* cursor_client = helper->GetCursorClient();
  if (cursor_client)
    cursor_client->AddObserver(this);
  helper->AddFocusObserver(this);
}

Pointer::~Pointer() {
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
  if (stylus_delegate_)
    stylus_delegate_->OnPointerDestroying(this);
  WMHelper* helper = WMHelper::GetInstance();
  helper->RemovePreTargetHandler(this);
  // TODO(sky): CursorClient does not exist in mash
  // yet. https://crbug.com/631103.
  aura::client::CursorClient* cursor_client = helper->GetCursorClient();
  if (cursor_client)
    cursor_client->RemoveObserver(this);
  if (root_surface())
    root_surface()->RemoveSurfaceObserver(this);
  helper->RemoveFocusObserver(this);
}

void Pointer::SetCursor(Surface* surface, const gfx::Point& hotspot) {
  // Early out if the pointer doesn't have a surface in focus.
  if (!focus_surface_)
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
#if defined(OS_CHROMEOS)
  Surface* constrained_surface = delegate->GetConstrainedSurface();
  // Pointer lock should be enabled for ARC by default. The kExoPointerLock
  // should only apply to Crostini windows.
  bool is_arc_window = ash::window_util::IsArcWindow(
      constrained_surface->window()->GetToplevelWindow());
  if (!is_arc_window &&
      !base::FeatureList::IsEnabled(chromeos::features::kExoPointerLock))
    return false;
  bool success = EnablePointerCapture(constrained_surface);
  if (success)
    pointer_constraint_delegate_ = delegate;
  return success;
#else
  NOTIMPLEMENTED();
  return false;
#endif
}

void Pointer::UnconstrainPointer() {
  if (pointer_constraint_delegate_) {
    DisablePointerCapture();
    pointer_constraint_delegate_ = nullptr;
  }
}

bool Pointer::EnablePointerCapture(Surface* capture_surface) {
  if (!base::FeatureList::IsEnabled(kPointerCapture)) {
    LOG(WARNING) << "Unable to capture the pointer, feature is disabled.";
    return false;
  }

  if (capture_surface->window() !=
      WMHelper::GetInstance()->GetFocusedWindow()) {
    LOG(ERROR)
        << "Cannot enable pointer capture on a window that is not focused.";
    return false;
  }

  if (!capture_surface->HasSurfaceObserver(this))
    capture_surface->AddSurfaceObserver(this);

  capture_window_ = capture_surface->window();

  // Add a pre-target handler that can consume all mouse events before it gets
  // sent to other targets.
  aura::Env::GetInstance()->AddPreTargetHandler(
      this, ui::EventTarget::Priority::kSystem);

  location_when_pointer_capture_enabled_ = gfx::ToRoundedPoint(location_);

  if (ShouldMoveToCenter())
    MoveCursorToCenterOfActiveDisplay();

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
  root->MoveCursorTo(p);

  capture_window_ = nullptr;
  location_when_pointer_capture_enabled_.reset();
  UpdateCursor();
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
  if (surface && pointer_constraint_delegate_ &&
      surface == pointer_constraint_delegate_->GetConstrainedSurface()) {
    surface->RemoveSurfaceObserver(this);
    VLOG(1) << "Pointer constraint broken by surface destruction";
    pointer_constraint_delegate_->OnConstraintBroken();
    UnconstrainPointer();
  }
  if (surface && surface->window() == capture_window_)
    DisablePointerCapture();
  if (surface == focus_surface_) {
    SetFocus(nullptr, gfx::PointF(), 0);
    return;
  }
  if (surface == root_surface()) {
    UpdatePointerSurface(nullptr);
    return;
  }
  NOTREACHED();
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler overrides:

void Pointer::OnMouseEvent(ui::MouseEvent* event) {
  // Nothing to report to a client nor have to update the pointer when capture
  // changes.
  if (event->type() == ui::ET_MOUSE_CAPTURE_CHANGED)
    return;

  seat_->SetLastPointerLocation(event->root_location_f());

  Surface* target = GetEffectiveTargetForEvent(event);
  gfx::PointF location_in_target = event->location_f();
  if (target) {
    aura::Window::ConvertPointToTarget(
        static_cast<aura::Window*>(event->target()), target->window(),
        &location_in_target);
  }

  // Update focus if target is different than the current pointer focus.
  if (target != focus_surface_)
    SetFocus(target, location_in_target, event->button_flags());

  gfx::PointF location_in_root = GetLocationInRoot(target, location_in_target);

  if (!focus_surface_)
    return;

  TRACE_EXO_INPUT_EVENT(event);

  const auto& details = event->pointer_details();
  if (stylus_delegate_ && last_pointer_type_ != details.pointer_type) {
    last_pointer_type_ = details.pointer_type;
    stylus_delegate_->OnPointerToolChange(details.pointer_type);
    delegate_->OnPointerFrame();
  }

  if (event->IsMouseEvent()) {
    // Generate motion event if location changed. We need to check location
    // here as mouse movement can generate both "moved" and "entered" events
    // but OnPointerMotion should only be called if location changed since
    // OnPointerEnter was called.
    // For synthesized events, they typically lack floating point precision
    // so to avoid generating mouse event jitter we consider the location of
    // these events to be the same as |location| if floored values match.
    bool same_location = !event->IsSynthesized()
                             ? SameLocation(location_in_root, location_)
                             : gfx::ToFlooredPoint(location_in_root) ==
                                   gfx::ToFlooredPoint(location_);

    // Ordinal motion is sent only on platforms that support it, which is
    // indicated by the presence of a flag.
    //
    // TODO(b/161755250): the ifdef is only necessary because of the feature
    // flag. This code should work fine on non-cros.
    base::Optional<gfx::Vector2dF> ordinal_motion = base::nullopt;
#if defined(OS_CHROMEOS)
    if (event->flags() & ui::EF_UNADJUSTED_MOUSE &&
        base::FeatureList::IsEnabled(chromeos::features::kExoOrdinalMotion)) {
      ordinal_motion = event->movement();
    }
#endif

    if (!same_location) {
      bool needs_frame = HandleRelativePointerMotion(
          event->time_stamp(), location_in_root, ordinal_motion);
      if (capture_window_) {
        if (ShouldMoveToCenter())
          MoveCursorToCenterOfActiveDisplay();
      } else if (event->type() != ui::ET_MOUSE_EXITED) {
        delegate_->OnPointerMotion(event->time_stamp(), location_in_target);
        needs_frame = true;
      }
      if (needs_frame)
        delegate_->OnPointerFrame();
      location_ = location_in_root;
    }
  }
  switch (event->type()) {
    case ui::ET_MOUSE_RELEASED:
      seat_->AbortPendingDragOperation();
      FALLTHROUGH;
    case ui::ET_MOUSE_PRESSED: {
      delegate_->OnPointerButton(event->time_stamp(),
                                 event->changed_button_flags(),
                                 event->type() == ui::ET_MOUSE_PRESSED);
      delegate_->OnPointerFrame();
      break;
    }
    case ui::ET_SCROLL: {
      ui::ScrollEvent* scroll_event = static_cast<ui::ScrollEvent*>(event);

      // Scrolling with 3+ fingers should not be handled since it will be used
      // to trigger overview mode.
      if (scroll_event->finger_count() >= 3)
        break;
      delegate_->OnPointerScroll(
          event->time_stamp(),
          gfx::Vector2dF(scroll_event->x_offset(), scroll_event->y_offset()),
          false);
      delegate_->OnPointerFrame();
      break;
    }
    case ui::ET_MOUSEWHEEL: {
      delegate_->OnPointerScroll(
          event->time_stamp(),
          static_cast<ui::MouseWheelEvent*>(event)->offset(), true);
      delegate_->OnPointerFrame();
      break;
    }
    case ui::ET_SCROLL_FLING_START: {
      // Fling start in chrome signals the lifting of fingers after scrolling.
      // In wayland terms this signals the end of a scroll sequence.
      delegate_->OnPointerScrollStop(event->time_stamp());
      delegate_->OnPointerFrame();
      break;
    }
    case ui::ET_SCROLL_FLING_CANCEL: {
      // Fling cancel is generated very generously at every touch of the
      // touchpad. Since it's not directly supported by the delegate, we do not
      // want limit this event to only right after a fling start has been
      // generated to prevent erronous behavior.
      if (last_event_type_ == ui::ET_SCROLL_FLING_START) {
        // We emulate fling cancel by starting a new scroll sequence that
        // scrolls by 0 pixels, effectively stopping any kinetic scroll motion.
        delegate_->OnPointerScroll(event->time_stamp(), gfx::Vector2dF(),
                                   false);
        delegate_->OnPointerFrame();
        delegate_->OnPointerScrollStop(event->time_stamp());
        delegate_->OnPointerFrame();
      }
      break;
    }
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
      break;
    default:
      NOTREACHED();
      break;
  }

  if (stylus_delegate_) {
    bool needs_frame = false;
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
      needs_frame = true;
    }
    if (abs(last_tilt_.x() - details.tilt_x) >= kTiltGranularity ||
        abs(last_tilt_.y() - details.tilt_y) >= kTiltGranularity) {
      last_tilt_ = gfx::Vector2dF(details.tilt_x, details.tilt_y);
      stylus_delegate_->OnPointerTilt(event->time_stamp(), last_tilt_);
      needs_frame = true;
    }
    if (needs_frame)
      delegate_->OnPointerFrame();
  }

  last_event_type_ = event->type();

  // Consume all mouse events when pointer capture is enabled.
  if (capture_window_) {
    event->SetHandled();
    event->StopPropagation();
  }
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
    case ui::ET_GESTURE_PINCH_BEGIN:
      pinch_delegate_->OnPointerPinchBegin(event->unique_touch_event_id(),
                                           event->time_stamp(), focus_surface_);
      delegate_->OnPointerFrame();
      break;
    case ui::ET_GESTURE_PINCH_UPDATE:
      pinch_delegate_->OnPointerPinchUpdate(event->time_stamp(),
                                            event->details().scale());
      delegate_->OnPointerFrame();
      break;
    case ui::ET_GESTURE_PINCH_END:
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
// aura::client::CursorClientObserver overrides:

void Pointer::OnCursorSizeChanged(ui::CursorSize cursor_size) {
  if (!focus_surface_)
    return;

  if (cursor_ != ui::mojom::CursorType::kNull)
    UpdateCursor();
}

void Pointer::OnCursorDisplayChanged(const display::Display& display) {
  UpdatePointerSurface(root_surface());
  auto info = GetCaptureDisplayInfo();
  capture_scale_ = info.device_scale_factor();
  capture_ratio_ = info.GetDensityRatio();

  auto* cursor_client = WMHelper::GetInstance()->GetCursorClient();
  // TODO(crbug.com/631103): CursorClient does not exist in mash yet.
  if (!cursor_client)
    return;
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
      pointer_constraint_delegate_->OnConstraintBroken();
      UnconstrainPointer();
    } else {
      DisablePointerCapture();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Pointer, private:

Surface* Pointer::GetEffectiveTargetForEvent(
    const ui::LocatedEvent* event) const {
  if (capture_window_)
    return Surface::AsSurface(capture_window_);

  Surface* target = GetTargetSurfaceForLocatedEvent(event);

  if (!target)
    return nullptr;

  return delegate_->CanAcceptPointerEventsForSurface(target) ? target : nullptr;
}

void Pointer::SetFocus(Surface* surface,
                       const gfx::PointF& location,
                       int button_flags) {
  // First generate a leave event if we currently have a target in focus.
  if (focus_surface_) {
    delegate_->OnPointerLeave(focus_surface_);
    focus_surface_->RemoveSurfaceObserver(this);
    // Require SetCursor() to be called and cursor to be re-defined in
    // response to each OnPointerEnter() call.
    focus_surface_ = nullptr;
    cursor_capture_weak_ptr_factory_.InvalidateWeakPtrs();
  }
  // Second generate an enter event if focus moved to a new surface.
  if (surface) {
    delegate_->OnPointerEnter(surface, location, button_flags);
    location_ = GetLocationInRoot(surface, location);
    focus_surface_ = surface;
    if (!focus_surface_->HasSurfaceObserver(this))
      focus_surface_->AddSurfaceObserver(this);
  }
  delegate_->OnPointerFrame();
  UpdateCursor();
}

void Pointer::UpdatePointerSurface(Surface* surface) {
  if (root_surface()) {
    host_window()->SetTransform(gfx::Transform());
    if (host_window()->parent())
      host_window()->parent()->RemoveChild(host_window());
    root_surface()->RemoveSurfaceObserver(this);
    SetRootSurface(nullptr);
  }

  if (surface) {
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

  // Submit compositor frame to be captured.
  SubmitCompositorFrame();

  // Surface size is in DIPs, while layer size is in pseudo-DIP units that
  // depend on the DSF of the display mode. Scale the layer to capture the
  // surface at a constant pixel size, regardless of the primary display's
  // display mode DSF.
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  float scale = capture_scale_ / display.device_scale_factor();
  host_window()->SetTransform(gfx::GetScaleTransform(gfx::Point(), scale));

  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
          base::BindOnce(&Pointer::OnCursorCaptured,
                         cursor_capture_weak_ptr_factory_.GetWeakPtr(),
                         hotspot));
  request->set_result_task_runner(base::SequencedTaskRunnerHandle::Get());

  request->set_source(cursor_capture_source_id_);
  host_window()->layer()->RequestCopyOfOutput(std::move(request));
}

void Pointer::OnCursorCaptured(const gfx::Point& hotspot,
                               std::unique_ptr<viz::CopyOutputResult> result) {
  if (!focus_surface_)
    return;

  // Only successful captures should update the cursor.
  if (result->IsEmpty())
    return;

  cursor_bitmap_ = result->AsSkBitmap();
  DCHECK(cursor_bitmap_.readyToDraw());
  cursor_hotspot_ = hotspot;
  UpdateCursor();
}

void Pointer::UpdateCursor() {
  WMHelper* helper = WMHelper::GetInstance();
  aura::client::CursorClient* cursor_client = helper->GetCursorClient();
  // TODO(crbug.com/631103): CursorClient does not exist in mash yet.
  if (!cursor_client)
    return;

  if (cursor_ == ui::mojom::CursorType::kCustom) {
    SkBitmap bitmap = cursor_bitmap_;
    gfx::Point hotspot =
        gfx::ScaleToFlooredPoint(cursor_hotspot_, capture_ratio_);

    // TODO(oshima|weidongg): Add cutsom cursor API to handle size/display
    // change without explicit management like this. https://crbug.com/721601.
    const display::Display& display = cursor_client->GetDisplay();
    float scale =
        helper->GetDisplayInfo(display.id()).GetDensityRatio() / capture_ratio_;

    if (cursor_client->GetCursorSize() == ui::CursorSize::kLarge)
      scale *= kLargeCursorScale;

    // Use panel_rotation() rather than "natural" rotation, as it actually
    // relates to the hardware you're about to draw the cursor bitmap on.
    ui::ScaleAndRotateCursorBitmapAndHotpoint(scale, display.panel_rotation(),
                                              &bitmap, &hotspot);

    ui::PlatformCursor platform_cursor;
    // TODO(reveman): Add interface for creating cursors from GpuMemoryBuffers
    // and use that here instead of the current bitmap API.
    // https://crbug.com/686600
    platform_cursor =
        ui::CursorFactory::GetInstance()->CreateImageCursor(bitmap, hotspot);
    cursor_.SetPlatformCursor(platform_cursor);
    cursor_.set_custom_bitmap(bitmap);
    cursor_.set_custom_hotspot(hotspot);
    ui::CursorFactory::GetInstance()->UnrefImageCursor(platform_cursor);
  }

  // If there is a focused surface, update its widget as the views framework
  // expect that Widget knows the current cursor. Otherwise update the
  // cursor directly on CursorClient.
  if (focus_surface_) {
    aura::Window* window = focus_surface_->window();
    do {
      views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
      if (widget) {
        widget->SetCursor(cursor_);
        return;
      }
      window = window->parent();
    } while (window);
  } else {
    cursor_client->SetCursor(cursor_);
  }
}

gfx::PointF Pointer::GetLocationInRoot(Surface* target,
                                       gfx::PointF location_in_target) {
  if (!target || !target->window())
    return location_in_target;
  aura::Window* w = target->window();
  gfx::PointF p(location_in_target.x(), location_in_target.y());
  aura::Window::ConvertPointToTarget(w, w->GetRootWindow(), &p);
  return gfx::PointF(p.x(), p.y());
}

bool Pointer::ShouldMoveToCenter() {
  if (!capture_window_)
    return false;

  gfx::Rect rect = capture_window_->GetRootWindow()->bounds();
  rect.Inset(rect.width() / 6, rect.height() / 6);
  return !rect.Contains(location_.x(), location_.y());
}

void Pointer::MoveCursorToCenterOfActiveDisplay() {
  if (!capture_window_)
    return;
  aura::Window* root = capture_window_->GetRootWindow();
  gfx::Point p = root->bounds().CenterPoint();
  location_synthetic_move_ = p;
  root->MoveCursorTo(p);
}

bool Pointer::HandleRelativePointerMotion(
    base::TimeTicks time_stamp,
    gfx::PointF location_in_root,
    const base::Optional<gfx::Vector2dF>& ordinal_motion) {
  if (!relative_pointer_delegate_)
    return false;

  if (location_synthetic_move_) {
    gfx::Point synthetic = *location_synthetic_move_;
    // Since MoveCursorTo() takes integer coordinates, the resulting move could
    // have a conversion error of up to 2 due to fractional scale factors.
    if (std::abs(location_in_root.x() - synthetic.x()) <= 2 &&
        std::abs(location_in_root.y() - synthetic.y()) <= 2) {
      // This was a synthetic move event, so do not forward it and clear the
      // synthetic move.
      location_synthetic_move_.reset();
      return false;
    }
  }

  gfx::Vector2dF delta = location_in_root - location_;
  relative_pointer_delegate_->OnPointerRelativeMotion(
      time_stamp, delta,
      ordinal_motion.has_value() ? ordinal_motion.value() : delta);
  return true;
}

}  // namespace exo
