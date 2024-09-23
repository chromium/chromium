// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_EXTENDED_DRAG_SOURCE_H_
#define COMPONENTS_EXO_EXTENDED_DRAG_SOURCE_H_

#include <memory>
#include <optional>

#include "ash/drag_drop/toplevel_window_drag_delegate.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/exo/data_source_observer.h"
#include "ui/aura/scoped_window_event_targeting_blocker.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"

namespace aura {
class Window;
}

namespace gfx {
class Vector2d;
}

namespace ui {
class LocatedEvent;
}

namespace exo {

class DataSource;
class Surface;

class ExtendedDragSource : public DataSourceObserver,
                           public aura::WindowObserver,
                           public ash::ToplevelWindowDragDelegate {
 public:
  class Delegate {
   public:
    virtual bool ShouldAllowDropAnywhere() const = 0;
    virtual bool ShouldLockCursor() const = 0;
    virtual void OnDataSourceDestroying() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  class Observer {
   public:
    virtual void OnExtendedDragSourceDestroying(ExtendedDragSource* source) = 0;

   protected:
    virtual ~Observer() = default;
  };

  static ExtendedDragSource* Get();

  ExtendedDragSource(DataSource* source, Delegate* delegate);
  ExtendedDragSource(const ExtendedDragSource&) = delete;
  ExtendedDragSource& operator=(const ExtendedDragSource&) = delete;
  ~ExtendedDragSource() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool IsActive() const;

  void Drag(Surface* surface, const gfx::Vector2d& offset);

  // ash::ToplevelWindowDragDelegate:
  void OnToplevelWindowDragStarted(const gfx::PointF& start_location,
                                   ui::mojom::DragEventSource source,
                                   aura::Window* drag_source_window) override;
  ui::mojom::DragOperation OnToplevelWindowDragDropped() override;
  void OnToplevelWindowDragCancelled() override;
  void OnToplevelWindowDragEvent(ui::LocatedEvent* event) override;

  // DataSourceObserver:
  void OnDataSourceDestroying(DataSource* source) override;

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;

  aura::Window* GetDraggedWindowForTesting();
  std::optional<gfx::Vector2d> GetDragOffsetForTesting() const;
  aura::Window* GetDragSourceWindowForTesting();

 private:
  class DraggedWindowHolder;

  void MaybeLockCursor();
  void UnlockCursor();
  void StartDrag(aura::Window* toplevel);
  void OnDraggedWindowVisibilityChanging(bool visible);
  void OnDraggedWindowVisibilityChanged(bool visible);
  void Cleanup();

  static ExtendedDragSource* instance_;

  raw_ptr<DataSource> source_ = nullptr;

  // Created and destroyed at wayland/zcr_extended_drag.cc and its lifetime is
  // tied to the zcr_extended_drag_source_v1 object it's attached to.
  const raw_ptr<Delegate, DanglingUntriaged> delegate_;

  // The pointer location in screen coordinates.
  gfx::PointF pointer_location_;
  ui::mojom::DragEventSource drag_event_source_;
  bool cursor_locked_ = false;

  std::unique_ptr<DraggedWindowHolder> dragged_window_holder_;
  std::unique_ptr<aura::ScopedWindowEventTargetingBlocker> event_blocker_;
  raw_ptr<aura::Window> drag_source_window_ = nullptr;
  bool pending_drag_start_ = false;

  base::ObserverList<Observer>::Unchecked observers_;

  base::WeakPtrFactory<ExtendedDragSource> weak_factory_{this};
};

}  // namespace exo

#endif  // COMPONENTS_EXO_EXTENDED_DRAG_SOURCE_H_
