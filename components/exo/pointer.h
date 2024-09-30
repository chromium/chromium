// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_POINTER_H_
#define COMPONENTS_EXO_POINTER_H_

#include <memory>
#include <optional>

#include "ash/shell_observer.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "components/exo/surface_observer.h"
#include "components/exo/surface_tree_host.h"
#include "components/exo/wm_helper.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/client/drag_drop_client_observer.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-forward.h"
#include "ui/events/event_handler.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/native_widget_types.h"

namespace viz {
class CopyOutputResult;
}

namespace ui {
class LocatedEvent;
class MouseEvent;
}  // namespace ui

namespace exo {
class PointerConstraintDelegate;
class PointerDelegate;
class PointerGesturePinchDelegate;
class PointerStylusDelegate;
class RelativePointerDelegate;
class Seat;
class Surface;
class SurfaceTreeHost;

// This class implements a client pointer that represents one or more input
// devices, such as mice, which control the pointer location and pointer focus.
class Pointer : public SurfaceTreeHost,
                public SurfaceObserver,
                public ui::EventHandler,
                public aura::client::DragDropClientObserver,
                public aura::client::CursorClientObserver,
                public aura::client::FocusChangeObserver,
                public ash::ShellObserver,
                public ash::DesksController::Observer {
 public:
  Pointer(PointerDelegate* delegate,
          Seat* seat,
          std::unique_ptr<aura::Window> host_window = nullptr);

  Pointer(const Pointer&) = delete;
  Pointer& operator=(const Pointer&) = delete;

  ~Pointer() override;

  PointerDelegate* delegate() const { return delegate_; }

  // Set the pointer surface, i.e., the surface that contains the pointer image
  // (cursor). The |hotspot| argument defines the position of the pointer
  // surface relative to the pointer location. Its top-left corner is always at
  // (x, y) - (hotspot.x, hotspot.y), where (x, y) are the coordinates of the
  // pointer location, in surface local coordinates.
  void SetCursor(Surface* surface, const gfx::Point& hotspot);

  // Set the pointer cursor type. This is similar to SetCursor, but this method
  // accepts ui::mojom::CursorType instead of the surface for the pointer image.
  void SetCursorType(ui::mojom::CursorType cursor_type);

  // Set delegate for pinch events.
  void SetGesturePinchDelegate(PointerGesturePinchDelegate* delegate);

  // SurfaceDelegate:
  void OnSurfaceCommit() override;

  // SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // aura::client::DragDropClientObserver:
  void OnDragStarted() override;
  void OnDragCompleted(const ui::DropTargetEvent& event) override;

  // aura::client::CursorClientObserver:
  void OnCursorSizeChanged(ui::CursorSize cursor_size) override;
  void OnCursorDisplayChanged(const display::Display& display) override;

  // aura::client::FocusChangeObserver;
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // ash::ShellObserver:
  void OnRootWindowAdded(aura::Window* root_window) override;
  void OnRootWindowWillShutdown(aura::Window* root_window) override;

  // ash::DesksController::Observer:
  void OnDeskSwitchAnimationFinished() override;

  // Relative motion registration.
  void RegisterRelativePointerDelegate(RelativePointerDelegate* delegate);
  void UnregisterRelativePointerDelegate(RelativePointerDelegate* delegate);

  // Enable the pointer constraint on the given surface. Returns true if the
  // lock was granted, false otherwise.
  //
  // The delegate must call OnPointerConstraintDelegateDestroying() upon/before
  // being destroyed, regardless of the return value of ConstrainPointer(),
  // unless PointerConstraintDelegate::OnDefunct() is called first.
  //
  // TODO(crbug.com/957455): For legacy reasons, locking the pointer will also
  // hide the cursor.
  bool ConstrainPointer(PointerConstraintDelegate* delegate);

  // Notifies that |delegate| is being destroyed.
  void OnPointerConstraintDelegateDestroying(
      PointerConstraintDelegate* delegate);

  // Disable the pointer constraint, notify the delegate, and do not permit
  // the constraint to be re-established until the user acts on the surface
  // (by clicking on it).
  //
  // Designed to be called by client code, on behalf of a user action to break
  // the constraint.
  //
  // Returns true if an active pointer constraint was disabled.
  bool UnconstrainPointerByUserAction();

  // Set the stylus delegate for handling stylus events.
  void SetStylusDelegate(PointerStylusDelegate* delegate);
  bool HasStylusDelegate() const;

  // Pointer capture is enabled if and only if `capture_window_` is not null.
  bool GetIsPointerConstrainedForTesting() {
    return capture_window_ != nullptr;
  }

 private:
  // Remove |delegate| from |constraints_|.
  void RemoveConstraintDelegate(PointerConstraintDelegate* delegate);

  // Disable the pointer constraint and notify the delegate.
  void UnconstrainPointer();

  // Try to reactivate a pointer constraint previously requested for the given
  // surface, if any.
  void MaybeReactivatePointerConstraint(Surface* surface);

  // Capture the pointer for the given surface. Returns true iff the capture
  // succeeded.
  bool EnablePointerCapture(Surface* capture_surface);

  // Remove the currently active pointer capture (if there is one).
  void DisablePointerCapture();

  // Returns the effective target for |event| and the event's location converted
  // to the target's coordinates.
  Surface* GetEffectiveTargetForEvent(const ui::LocatedEvent* event,
                                      gfx::PointF* location_in_target) const;

  // Change pointer focus to |surface|.
  void SetFocus(Surface* surface,
                const gfx::PointF& root_location,
                const gfx::PointF& surface_location,
                int button_flags);

  // Updates the root_surface in |SurfaceTreeHost| from which the cursor
  // is captured.
  void UpdatePointerSurface(Surface* surface);

  // Asynchronously update the cursor by capturing a snapshot of
  // |SurfaceTreeHost::root_surface()|.
  void CaptureCursor(const gfx::Point& hotspot);

  // Called when cursor snapshot has been captured.
  void OnCursorCaptured(const gfx::Point& hotspot,
                        std::unique_ptr<viz::CopyOutputResult> result);

  // Called when cursor bitmap has been obtained either from viz copy output
  // results or directly from the buffer.
  void OnCursorBitmapObtained(const gfx::Point& hotspot,
                              const SkBitmap& cursor_bitmap,
                              float cursor_scale);

  // Update |cursor_| to |cursor_bitmap_| transformed with |cursor_scale_|.
  void UpdateCursor();

  // Called to check if cursor should be moved to the center of the window when
  // sending relative movements.
  bool ShouldMoveToCenter();

  // Moves the cursor to center of the active display.
  void MoveCursorToCenterOfActiveDisplay();

  // Process the delta for relative pointer motion. Returns true if relative
  // motion was sent to the delegate, false otherwise. If |ordinal_motion| is
  // supplied, it will be used for determining physical motion, otherwise
  // physical motion will be the relative delta.
  bool HandleRelativePointerMotion(
      base::TimeTicks time_stamp,
      gfx::PointF location_in_target,
      const std::optional<gfx::Vector2dF>& ordinal_motion);

  // Whether this Pointer should observe the given |surface|.
  bool ShouldObserveSurface(Surface* surface);

  // Stop observing |surface| if it's no longer relevant.
  void MaybeRemoveSurfaceObserver(Surface* surface);

  // Return true if location is same.
  bool CheckIfSameLocation(bool is_synthesized,
                           const gfx::PointF& location_in_root,
                           const gfx::PointF& location_in_target);

  // The delegate instance that all events are dispatched to.
  const raw_ptr<PointerDelegate, DanglingUntriaged> delegate_;

  const raw_ptr<Seat> seat_;

  // The delegate instance that all pinch related events are dispatched to.
  raw_ptr<PointerGesturePinchDelegate> pinch_delegate_ = nullptr;

  // The delegate instance that relative movement events are dispatched to.
  raw_ptr<RelativePointerDelegate> relative_pointer_delegate_ = nullptr;

  // Delegate that owns the currently granted pointer lock, if any.
  raw_ptr<PointerConstraintDelegate> pointer_constraint_delegate_ = nullptr;

  // All delegates currently requesting a pointer locks, whether granted or
  // not. Only one such request may exist per surface; others will be denied.
  base::flat_map<Surface*, raw_ptr<PointerConstraintDelegate, CtnExperimental>>
      constraints_;

  // The delegate instance that stylus/pen events are dispatched to.
  raw_ptr<PointerStylusDelegate> stylus_delegate_ = nullptr;

  // The current focus surface for the pointer.
  raw_ptr<Surface> focus_surface_ = nullptr;

  // The location of the pointer in the root window.
  gfx::PointF location_in_root_;

  // The location of the pointer converted to the target.
  gfx::PointF location_in_surface_;

  // The location of the pointer when pointer capture is first enabled.
  std::optional<gfx::Point> location_when_pointer_capture_enabled_;

  // If this is not nullptr, a synthetic move was sent and this points to the
  // location of a generated move that was sent which should not be forwarded.
  std::optional<gfx::Point> expected_next_mouse_location_;

  // The window with pointer capture. Pointer capture is enabled if and only if
  // this is not null.
  raw_ptr<aura::Window> capture_window_ = nullptr;

  // True if this pointer is permitted to be captured.
  //
  // Set false when a user action (except focus loss) breaks pointer capture.
  // Set true when the user clicks in any Exo window.
  bool capture_permitted_ = true;

  // The position of the pointer surface relative to the pointer location.
  gfx::Point hotspot_;

  // Latest cursor snapshot.
  SkBitmap cursor_bitmap_;

  // Latest cursor image scale;
  float cursor_scale_ = 1.0f;

  // The current cursor.
  ui::Cursor cursor_;

  // Hotspot to use with latest cursor snapshot.
  gfx::Point cursor_hotspot_;

  // Source used for cursor capture copy output requests.
  const base::UnguessableToken cursor_capture_source_id_;

  // Last received event type.
  ui::EventType last_event_type_ = ui::EventType::kUnknown;

  // Last reported stylus values.
  ui::EventPointerType last_pointer_type_ = ui::EventPointerType::kUnknown;
  float last_force_ = std::numeric_limits<float>::quiet_NaN();
  gfx::Vector2dF last_tilt_;

  // Bitmask of the button event flags that started the drag and drop operation.
  // Used to send the release events upon drop.
  int button_flags_on_drag_drop_start_ = 0;

  // Weak pointer factory used for cursor capture callbacks.
  base::WeakPtrFactory<Pointer> cursor_capture_weak_ptr_factory_{this};
};

}  // namespace exo

#endif  // COMPONENTS_EXO_POINTER_H_
