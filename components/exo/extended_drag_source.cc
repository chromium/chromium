// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/extended_drag_source.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "components/exo/data_source.h"
#include "components/exo/surface.h"
#include "components/exo/surface_observer.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"
#include "ui/events/event_target.h"
#include "ui/events/event_utils.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/window_move_client.h"

namespace exo {

using ::ui::mojom::DragOperation;

// static
ExtendedDragSource* ExtendedDragSource::instance_ = nullptr;

// Internal representation of a toplevel window, backed by an Exo shell surface,
// which is being dragged. It supports both already mapped/visible windows as
// well as newly created ones (i.e: not added to a root window yet), in which
// case OnDraggedWindowVisibilityChanged callback is called to notify when it
// has just got visible.
class ExtendedDragSource::DraggedWindowHolder : public aura::WindowObserver,
                                                public SurfaceObserver {
 public:
  DraggedWindowHolder(Surface* surface,
                      const gfx::Vector2d& drag_offset,
                      ExtendedDragSource* source)
      : surface_(surface), drag_offset_(drag_offset), source_(source) {
    DCHECK(surface_);
    DCHECK(surface_->window());
    surface_->AddSurfaceObserver(this);
    if (!FindToplevelWindow()) {
      DVLOG(1) << "Dragged window not added to root window yet.";
      surface_->window()->AddObserver(this);
    }
  }

  DraggedWindowHolder(const DraggedWindowHolder&) = delete;
  DraggedWindowHolder& operator=(const DraggedWindowHolder&) = delete;

  ~DraggedWindowHolder() override {
    if (toplevel_window_) {
      toplevel_window_->RemoveObserver(this);
      toplevel_window_ = nullptr;
    } else if (surface_) {
      surface_->window()->RemoveObserver(this);
    }

    if (surface_)
      surface_->RemoveSurfaceObserver(this);
  }

  aura::Window* toplevel_window() { return toplevel_window_; }
  const gfx::Vector2d& offset() const { return drag_offset_; }

 private:
  // aura::WindowObserver:
  void OnWindowAddedToRootWindow(aura::Window* window) override {
    if (toplevel_window_)
      return;

    DCHECK_EQ(window, surface_->window());
    FindToplevelWindow();
    DCHECK(toplevel_window_);
    surface_->window()->RemoveObserver(this);
  }

  void OnWindowVisibilityChanging(aura::Window* window, bool visible) override {
    DCHECK(window);
    if (window == toplevel_window_)
      source_->OnDraggedWindowVisibilityChanging(visible);
  }

  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    DCHECK(window);
    if (window == toplevel_window_)
      source_->OnDraggedWindowVisibilityChanged(visible);
  }

  void OnWindowDestroying(aura::Window* window) override {
    DCHECK(window);
    if (window == toplevel_window_) {
      toplevel_window_->RemoveObserver(this);
      toplevel_window_ = nullptr;
    }
  }

  // SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override {
    if (surface_ == surface) {
      surface_->RemoveSurfaceObserver(this);
      if (surface_->window()->HasObserver(this)) {
        surface_->window()->RemoveObserver(this);
      }
      surface_ = nullptr;
    }
  }

  bool FindToplevelWindow() {
    if (!surface_->window()->GetRootWindow())
      return false;

    toplevel_window_ = surface_->window()->GetToplevelWindow();
    toplevel_window_->AddObserver(this);

    // Disable visibility change animations on the dragged window.
    toplevel_window_->SetProperty(aura::client::kAnimationsDisabledKey, true);
    return true;
  }

  raw_ptr<Surface> surface_;
  gfx::Vector2d drag_offset_;
  const raw_ptr<ExtendedDragSource> source_;
  raw_ptr<aura::Window> toplevel_window_ = nullptr;
};

// static
ExtendedDragSource* ExtendedDragSource::Get() {
  return instance_;
}

ExtendedDragSource::ExtendedDragSource(DataSource* source, Delegate* delegate)
    : source_(source), delegate_(delegate) {
  DCHECK(source_);
  DCHECK(delegate_);

  source_->AddObserver(this);

  DCHECK(!instance_);
  instance_ = this;
}

ExtendedDragSource::~ExtendedDragSource() {
  delegate_->OnDataSourceDestroying();
  for (auto& observer : observers_)
    observer.OnExtendedDragSourceDestroying(this);

  if (source_)
    source_->RemoveObserver(this);

  if (drag_source_window_)
    drag_source_window_->RemoveObserver(this);

  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

void ExtendedDragSource::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void ExtendedDragSource::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void ExtendedDragSource::Drag(Surface* dragged_surface,
                              const gfx::Vector2d& drag_offset) {
  // Associated data source already destroyed.
  if (!source_)
    return;

  if (!dragged_surface) {
    DVLOG(1) << "Unsetting dragged surface.";
    dragged_window_holder_.reset();
    return;
  }

  DVLOG(1) << "Dragged surface changed:"
           << " surface=" << dragged_surface
           << " offset=" << drag_offset.ToString();

  // Ensure that the surface already has a "role" assigned.
  DCHECK(dragged_surface->HasSurfaceDelegate());
  dragged_window_holder_ =
      std::make_unique<DraggedWindowHolder>(dragged_surface, drag_offset, this);

  // Drag process will be started once OnToplevelWindowDragStarted() gets
  // called, unless it is called (raced) prior to this one. In this case, we
  // trigger the dragging here.
  //
  // TODO(https://crrev.com/1360884: Unify where tab-dragging is initiated.
  if (pending_drag_start_ &&
      dragged_window_holder_->toplevel_window() == drag_source_window_) {
    StartDrag(dragged_window_holder_->toplevel_window());
    pending_drag_start_ = false;
  }
}

bool ExtendedDragSource::IsActive() const {
  return !!source_;
}

void ExtendedDragSource::OnToplevelWindowDragStarted(
    const gfx::PointF& start_location,
    ui::mojom::DragEventSource source,
    aura::Window* drag_source_window) {
  pointer_location_ = start_location;
  drag_event_source_ = source;
  drag_source_window_ =
      drag_source_window ? drag_source_window->GetToplevelWindow() : nullptr;
  if (drag_source_window_)
    drag_source_window_->AddObserver(this);
  MaybeLockCursor();
  if (dragged_window_holder_ && dragged_window_holder_->toplevel_window() &&
      dragged_window_holder_->toplevel_window()->IsVisible()) {
    StartDrag(dragged_window_holder_->toplevel_window());
  } else {
    pending_drag_start_ = true;
  }
}

DragOperation ExtendedDragSource::OnToplevelWindowDragDropped() {
  DVLOG(1) << "OnDragDropped()";
  Cleanup();
  return delegate_->ShouldAllowDropAnywhere() ? DragOperation::kMove
                                              : DragOperation::kNone;
}

void ExtendedDragSource::OnToplevelWindowDragCancelled() {
  DVLOG(1) << "OnDragCancelled()";
  auto* handler = ash::Shell::Get()->toplevel_window_event_handler();
  handler->RevertDrag();

  Cleanup();
}

void ExtendedDragSource::OnToplevelWindowDragEvent(ui::LocatedEvent* event) {
  if (!dragged_window_holder_)
    return;

  // The pointer location must be translated into screen coordinates.
  CHECK(event);
  aura::Window* target = static_cast<aura::Window*>(event->target());
  pointer_location_ = event->root_location_f();
  wm::ConvertPointToScreen(target->GetRootWindow(), &pointer_location_);

  auto* handler = ash::Shell::Get()->toplevel_window_event_handler();
  if (event->IsMouseEvent()) {
    handler->OnMouseEvent(event->AsMouseEvent());
    return;
  }

  if (event->IsGestureEvent()) {
    handler->OnGestureEvent(event->AsGestureEvent());
    return;
  }

  NOTREACHED_IN_MIGRATION() << "Only mouse and touch events are supported.";
}

void ExtendedDragSource::OnDataSourceDestroying(DataSource* source) {
  DCHECK_EQ(source, source_);
  source_->RemoveObserver(this);
  source_ = nullptr;
}

void ExtendedDragSource::OnWindowDestroyed(aura::Window* window) {
  if (drag_source_window_ == window)
    drag_source_window_ = nullptr;
}

void ExtendedDragSource::MaybeLockCursor() {
  if (delegate_->ShouldLockCursor()) {
    ash::Shell::Get()->cursor_manager()->LockCursor();
    cursor_locked_ = true;
  }
}

void ExtendedDragSource::UnlockCursor() {
  if (cursor_locked_) {
    ash::Shell::Get()->cursor_manager()->UnlockCursor();
    cursor_locked_ = false;
  }
}

void ExtendedDragSource::StartDrag(aura::Window* toplevel) {
  DCHECK(!event_blocker_);
  // Ensure |toplevel| window does skip events while it's being dragged.
  event_blocker_ =
      std::make_unique<aura::ScopedWindowEventTargetingBlocker>(toplevel);

  DVLOG(1) << "Starting drag. pointer_loc=" << pointer_location_.ToString();
  auto* toplevel_handler = ash::Shell::Get()->toplevel_window_event_handler();
  auto move_source = drag_event_source_ == ui::mojom::DragEventSource::kTouch
                         ? ::wm::WINDOW_MOVE_SOURCE_TOUCH
                         : ::wm::WINDOW_MOVE_SOURCE_MOUSE;

  auto end_closure = base::BindOnce(
      [](base::WeakPtr<ExtendedDragSource> self,
         ash::ToplevelWindowEventHandler::DragResult result) {
        if (!self)
          return;
        if (auto* window_holder = self->dragged_window_holder_.get()) {
          if (auto* toplevel = window_holder->toplevel_window()) {
            toplevel->ClearProperty(ash::kIsDraggingTabsKey);
          }
        }
        self->dragged_window_holder_.reset();
        self->event_blocker_.reset();
      },
      weak_factory_.GetWeakPtr());

  // TODO(crbug.com/40164668): Experiment setting |update_gesture_target| back
  // to true when capture is removed from drag and drop.

  gfx::PointF pointer_location_in_parent(pointer_location_);
  wm::ConvertPointFromScreen(toplevel->parent(), &pointer_location_in_parent);

  toplevel_handler->AttemptToStartDrag(
      toplevel, pointer_location_in_parent, HTCAPTION, move_source,
      std::move(end_closure),
      /*update_gesture_target=*/false,
      /*grab_capture =*/
      drag_event_source_ != ui::mojom::DragEventSource::kTouch);
}

void ExtendedDragSource::OnDraggedWindowVisibilityChanging(bool visible) {
  DCHECK(dragged_window_holder_);
  DVLOG(1) << "Dragged window visibility changing. visible=" << visible;

  if (!visible) {
    dragged_window_holder_.reset();
    return;
  }

  aura::Window* toplevel = dragged_window_holder_->toplevel_window();
  DCHECK(toplevel);
  toplevel->SetProperty(ash::kIsDraggingTabsKey, true);
}

void ExtendedDragSource::OnDraggedWindowVisibilityChanged(bool visible) {
  DCHECK(dragged_window_holder_);
  DVLOG(1) << "Dragged window visibility changed. visible=" << visible;

  if (!visible) {
    dragged_window_holder_.reset();
    return;
  }

  aura::Window* toplevel = dragged_window_holder_->toplevel_window();
  DCHECK(toplevel);

  // The |toplevel| window for the dragged surface has just been created and
  // it's about to be mapped. Calculate and set its position based on
  // |drag_offset_| and |pointer_location_| before starting the actual drag.
  auto screen_location =
      gfx::ToFlooredPoint(pointer_location_ - dragged_window_holder_->offset());

  auto toplevel_bounds =
      gfx::Rect({screen_location, toplevel->bounds().size()});
  auto display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(toplevel);
  toplevel->SetBoundsInScreen(toplevel_bounds, display);

  DVLOG(1) << "Dragged window mapped. toplevel=" << toplevel
           << " origin=" << screen_location.ToString();

  StartDrag(toplevel);
}

void ExtendedDragSource::Cleanup() {
  if (dragged_window_holder_ && dragged_window_holder_->toplevel_window()) {
    dragged_window_holder_->toplevel_window()->ClearProperty(
        aura::client::kAnimationsDisabledKey);
  }
  event_blocker_.reset();
  if (drag_source_window_)
    drag_source_window_->RemoveObserver(this);
  drag_source_window_ = nullptr;
  pending_drag_start_ = false;
  UnlockCursor();
}

aura::Window* ExtendedDragSource::GetDraggedWindowForTesting() {
  return dragged_window_holder_ ? dragged_window_holder_->toplevel_window()
                                : nullptr;
}

std::optional<gfx::Vector2d> ExtendedDragSource::GetDragOffsetForTesting()
    const {
  return dragged_window_holder_
             ? std::optional<gfx::Vector2d>(dragged_window_holder_->offset())
             : std::nullopt;
}

aura::Window* ExtendedDragSource::GetDragSourceWindowForTesting() {
  return drag_source_window_;
}

}  // namespace exo
